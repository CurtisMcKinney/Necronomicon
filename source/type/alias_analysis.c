/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "symtable.h"
#include "alias_analysis.h"
#include "arena.h"
#include "base.h"
#include "infer.h"
#include "result.h"
#include "kind.h"

/*
    Rust / Futhark style Ownership/Consume/Move semantics
        * Futhark design: https://futhark-lang.org/publications/troels-henriksen-phd-thesis.pdf
        * Only restricts function calls
        * Only performs simple alias analysis

        * Rules:
            1: Functiona parameters with * are marked as unique and consume their values.
            2: Values can't be used after (in the computation sense, not syntactical) they are consumed.
            3: In function definitions, you cannot consume non-unique parameters.
            4: Uniqueness types are never inferred, they are only ever applied manually!
            5: Non-top level expression with functional types (i.e. non-zero order) cannot perform in-place updates (including the defintions of any lamdas).
            6: Partially applied functions with consumed value (as part of their closure, i.e. one of the partially applied arguments) cannot be passed as arguments.

        * Ownership based DSP
            accumulateNewAudio1 :: Default s => (Double -> *s -> *(Double, s)) -> Audio -> Audio
            updateSinOscState :: Double -> *SinOscState -> *(Double, SinOscState)
            updateSinOscState freq state =
              (value, state { .phase <- phase' })
              where
                phase' = state.phase + freq * inverseSampleRateDelta
                value  = waveTableLookup phase' sinOscWaveTable
            sinOsc :: Audio -> Audio
            sinOsc freq = accumulateNewAudio1 updateSinOscState freq

        * Ownership based World IO
            outputAudio :: Audio -> Int -> *World -> *World
            outputFrame :: Frame -> *World -> *World
            recordAudio :: Audio -> Text -> *World -> *World
            main        :: *World -> *World
            main world = world |> outAudio synths 1 |> outputFrame scene |> recordAudio synths fileName

    Values cannot be used after they are consumed (moved?).
    In general the system will not automatically do optimizations with owned values.
    But you can do in place updates with owned values that you couldn't do otherwise,
    thus putting the power in the hands of the programmer.
    Two in place updates styles:
        -- Update a field (or fields) in a variable in place, returning the updated value in its new state. Consumes the variable and everything it aliases.
        var{ .field <- expr }
        -- Update an array in place, returning the updated array in its new state. Consumes the array and everything it aliases.
        var[i] <- expr
    example:
        let sinOscState = SinOscState phase
        in  sinOscState <- SinOscState (phase * 2) -- In place update syntax, like ML, but an expression.
*/

typedef struct
{
    NecroIntern*         intern;
    NecroScopedSymTable* scoped_symtable;
    NecroBase*           base;
    NecroAstArena*       ast_arena;
    NecroPagedArena*     arena;
    NecroSnapshotArena   snapshot_arena;
} NecroAliasAnalysis;

NecroAliasAnalysis necro_alias_analysis_empty()
{
    NecroAliasAnalysis alias_analysis = (NecroAliasAnalysis)
    {
        .intern          = NULL,
        .arena           = NULL,
        .snapshot_arena  = necro_snapshot_arena_empty(),
        .scoped_symtable = NULL,
        .base            = NULL,
        .ast_arena       = NULL,
    };
    return alias_analysis;
}

NecroAliasAnalysis necro_alias_analysis_create(NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroAliasAnalysis alias_analysis = (NecroAliasAnalysis)
    {
        .intern          = intern,
        .arena           = &ast_arena->arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
    };
    return alias_analysis;
}

void necro_alias_analysis_destroy(NecroAliasAnalysis* alias_analysis)
{
    assert(alias_analysis != NULL);
    necro_snapshot_arena_destroy(&alias_analysis->snapshot_arena);
    *alias_analysis = necro_alias_analysis_empty();
}

NecroResult(NecroAliasSet) necro_alias_analysis_go(NecroAliasAnalysis* alias_analysis, NecroAst* ast);
NecroResult(void)              necro_alias_analysis(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena)
{
    UNUSED(info);
    NecroAliasAnalysis alias_analysis = necro_alias_analysis_create(intern, scoped_symtable, base, ast_arena);
    necro_alias_analysis_go(&alias_analysis, ast_arena->root);
    return ok_void();
}

///////////////////////////////////////////////////////
// NecroAliasSet
//--------------------------
// * Fixed capacity
// * Should never need to grow
// * Fast merge
///////////////////////////////////////////////////////
typedef struct
{
    NecroAstSymbol** data;
    size_t           capacity;
} NecroAliasSetTable;

typedef enum
{
    NECRO_ALIAS_SET_SINGLETON,
    NECRO_ALIAS_SET_TABLE,
} NECRO_ALIAS_SET_TYPE;

typedef struct NecroAliasSet
{
    union
    {
        NecroAstSymbol*    singleton;
        NecroAliasSetTable table;
    };
    size_t               count;
    NECRO_ALIAS_SET_TYPE type;
} NecroAliasSet;

NecroAliasSet* necro_alias_set_create_singleton(NecroPagedArena* arena, NecroAstSymbol* symbol)
{
    NecroAliasSet* set = necro_paged_arena_alloc(arena, sizeof(NecroAliasSet));
    set->type          = NECRO_ALIAS_SET_SINGLETON;
    set->singleton     = symbol;
    set->count         = 1;
    return set;
}

NecroAliasSet* necro_alias_set_create_table(NecroPagedArena* arena, size_t capacity)
{
    NecroAstSymbol** data = necro_paged_arena_alloc(arena, capacity * sizeof(NecroAstSymbol));
    for (size_t i = 0; i < capacity; ++i)
        data[i] = NULL;
    NecroAliasSet* set  = necro_paged_arena_alloc(arena, capacity * sizeof(NecroAstSymbol));
    set->type           = NECRO_ALIAS_SET_TABLE;
    set->table.data     = data;
    set->table.capacity = capacity;
    set->count          = 0;
    return set;
}

// Returns whether or not the hash set already contained the symbol
void necro_alias_set_insert(NecroAliasSet* set, NecroAstSymbol* symbol)
{
    assert(symbol != NULL);
    assert(set->type == NECRO_ALIAS_SET_TABLE);
    assert(set->count < set->table.capacity * 2);
    size_t hash = necro_hash((size_t)symbol) & set->table.capacity - 1;
    while (set->table.data[hash] != NULL)
    {
        if (set->table.data[hash] == symbol)
            return;
        hash = (hash + 1) & set->table.capacity - 1;
    }
    set->table.data[hash] = symbol;
    set->count++;
}

bool necro_alias_set_contains(NecroAliasSet* set, NecroAstSymbol* symbol)
{
    if (set == NULL)
        return false;
    if (set->type == NECRO_ALIAS_SET_SINGLETON)
        return set->singleton == symbol;
    if (set->count == 0 || set->table.capacity == 0 || symbol == NULL)
        return false;
    size_t hash = necro_hash((size_t)symbol) & set->table.capacity - 1;
    while (set->table.data[hash] != NULL)
    {
        if (set->table.data[hash] == symbol)
            return true;
        hash = (hash + 1) & set->table.capacity - 1;
    }
    return false;
}

bool necro_alias_set_is_overlapping(NecroAliasSet* set1, NecroAliasSet* set2)
{
    if (set1 == NULL || set2 == NULL)
        return false;
    if (set1->type == NECRO_ALIAS_SET_SINGLETON)
        return necro_alias_set_contains(set2, set1->singleton);
    for (size_t i = 0; i < set1->table.capacity; ++i)
    {
        if (set1->table.data[i] != NULL && necro_alias_set_contains(set2, set1->table.data[i]))
            return true;
    }
    return false;
}

NecroAliasSet* necro_alias_set_merge(NecroPagedArena* arena, NecroAliasSet* set1, NecroAliasSet* set2)
{
    if (set1 == NULL)
        return set2;
    if (set2 == NULL)
        return NULL;
    NecroAliasSet* new_set = necro_alias_set_create_table(arena, (set1->count + set2->count) * 2);
    if (set1->type == NECRO_ALIAS_SET_SINGLETON)
    {
        necro_alias_set_insert(new_set, set1->singleton);
    }
    else
    {
        for (size_t i = 0; i < set1->table.capacity; ++i)
        {
            if (set1->table.data[i] != NULL)
                necro_alias_set_insert(new_set, set1->table.data[i]);
        }
    }
    if (set2->type == NECRO_ALIAS_SET_SINGLETON)
    {
        necro_alias_set_insert(new_set, set2->singleton);
    }
    else
    {
        for (size_t i = 0; i < set2->table.capacity; ++i)
        {
            if (set2->table.data[i] != NULL)
                necro_alias_set_insert(new_set, set2->table.data[i]);
        }
    }
    return new_set;
}

NecroAliasSet* necro_alias_set_merge_many(NecroPagedArena* arena, NecroAliasSet** sets, size_t set_count)
{
    size_t total_count = 0;
    for (size_t i = 0; i < set_count; ++i)
    {
        if (sets[i] != NULL)
            total_count += sets[i]->count;
    }
    NecroAliasSet* new_set = necro_alias_set_create_table(arena, total_count * 2);
    for (size_t s = 0; s < set_count; ++s)
    {
        NecroAliasSet* set = sets[s];
        if (set == NULL)
        {
            continue;
        }
        if (set->type == NECRO_ALIAS_SET_SINGLETON)
        {
            necro_alias_set_insert(new_set, set->singleton);
        }
        else
        {
            for (size_t i = 0; i < set->table.capacity; ++i)
            {
                if (set->table.data[i] != NULL)
                    necro_alias_set_insert(new_set, set->table.data[i]);
            }
        }
    }
    return new_set;
}

///////////////////////////////////////////////////////
// Alias Analysis Go
///////////////////////////////////////////////////////
NecroResult(NecroAliasSet) necro_alias_analysis_list(NecroAliasAnalysis* alias_analysis, NecroAst* list)
{
    assert(list->type == NECRO_AST_LIST_NODE);
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&alias_analysis->snapshot_arena);
    size_t             count    = 0;
    NecroAst*          exprs    = list;
    while (exprs != NULL)
    {
        count++;
        exprs = exprs->list.next_item;
    }
    NecroAliasSet** sets = necro_snapshot_arena_alloc(&alias_analysis->snapshot_arena, count * sizeof(NecroAliasSet));
    size_t          i    = 0;
    exprs                = list;
    while (exprs != NULL)
    {
        sets[i] = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, exprs->list.item));
        exprs   = exprs->list.next_item;
        i++;
    }
    NecroAliasSet* list_set = necro_alias_set_merge_many(alias_analysis->arena, sets, count);
    necro_snapshot_arena_rewind(&alias_analysis->snapshot_arena, snapshot);
    return ok(NecroAliasSet, list_set);
}

NecroResult(NecroAliasSet) necro_alias_analysis_pat(NecroAliasAnalysis* alias_analysis, NecroAst* ast, NecroAliasSet* incoming_set)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_CONSTANT:        return ok(NecroAliasSet, NULL);
    case NECRO_AST_WILDCARD:        return ok(NecroAliasSet, NULL);
    case NECRO_AST_CONID:           return ok(NecroAliasSet, NULL);

    case NECRO_AST_TUPLE:            return necro_alias_analysis_pat(alias_analysis, ast->tuple.expressions, incoming_set);
    case NECRO_AST_EXPRESSION_LIST:  return necro_alias_analysis_pat(alias_analysis, ast->expression_list.expressions, incoming_set);
    case NECRO_AST_EXPRESSION_ARRAY: return necro_alias_analysis_pat(alias_analysis, ast->expression_array.expressions, incoming_set);
    case NECRO_AST_CONSTRUCTOR:      return necro_alias_analysis_pat(alias_analysis, ast->constructor.arg_list, incoming_set);

    case NECRO_AST_BIN_OP_SYM:
        necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, ast->bin_op_sym.left, incoming_set));
        necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, ast->bin_op_sym.right, incoming_set));
        return ok(NecroAliasSet, NULL);

    case NECRO_AST_LIST_NODE:
    {
        NecroAst* curr = ast;
        while (curr != NULL)
        {
            necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, curr->list.item, incoming_set));
            curr = curr->list.next_item;
        }
        return ok(NecroAliasSet, NULL);
    }

    case NECRO_AST_APATS:
    {
        NecroAst* curr = ast;
        while (curr != NULL)
        {
            necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, curr->apats.apat, incoming_set));
            curr = curr->apats.next_apat;
        }
        return ok(NecroAliasSet, NULL);
    }

    case NECRO_AST_VARIABLE:
    {
        NecroAliasSet* var_set              = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast));
        ast->variable.ast_symbol->alias_set = necro_alias_set_merge(alias_analysis->arena, var_set, incoming_set);
        return ok(NecroAliasSet, NULL);
    }

    default:
        necro_unreachable(NecroAliasSet);
    }
}

NecroResult(NecroAliasSet) necro_alias_analysis_case_alternatives(NecroAliasAnalysis* alias_analysis, NecroAst* list, NecroAliasSet* incoming_set)
{
    assert(list->type == NECRO_AST_LIST_NODE);
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&alias_analysis->snapshot_arena);
    size_t             count    = 0;
    NecroAst*          exprs    = list;
    while (exprs != NULL)
    {
        count++;
        exprs = exprs->list.next_item;
    }
    NecroAliasSet** sets = necro_snapshot_arena_alloc(&alias_analysis->snapshot_arena, count * sizeof(NecroAliasSet));
    size_t          i    = 0;
    exprs                = list;
    while (exprs != NULL)
    {
        assert(exprs->list.item->type == NECRO_AST_CASE_ALTERNATIVE);
        necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, exprs->list.item->case_alternative.pat, incoming_set));
        sets[i] = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, exprs->list.item->case_alternative.body));
        exprs   = exprs->list.next_item;
        i++;
    }
    NecroAliasSet* list_set = necro_alias_set_merge_many(alias_analysis->arena, sets, count);
    necro_snapshot_arena_rewind(&alias_analysis->snapshot_arena, snapshot);
    return ok(NecroAliasSet, list_set);
}

NecroResult(NecroAliasSet) necro_alias_analysis_apply(NecroAliasAnalysis* alias_analysis, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    NecroType* applied_type = necro_type_find(ast->necro_type);
    // Reference type as it is in environment, don't unify / infer uniqueness attribute...it must be manually applied!
    // Need to go as far in as we can. Type CAN be unique if most inner ast is a variable which has a function type who's result is a unique type.
    NecroAst*  fexprs    = ast;
    size_t     count     = 0;
    while (fexprs->type == NECRO_AST_FUNCTION_EXPRESSION)
    {
        if (fexprs->fexpression.next_fexpression->necro_type->uniqueness_attribute == NECRO_TYPE_NOT_UNIQUE)
            count++;
        fexprs = fexprs->fexpression.aexp;
    }
    const bool is_fn_result_unique = fexprs->type == NECRO_AST_VARIABLE && necro_type_find(fexprs->variable.ast_symbol->type)->type == NECRO_TYPE_FUN && necro_type_get_fully_applied_fun_type(necro_type_find(fexprs->variable.ast_symbol->type))->uniqueness_attribute;
    if (is_fn_result_unique)
    {
        if (applied_type->type == NECRO_TYPE_FUN)
        {
            // Check if there are any unique parameters being applied.
            // If we are applying unique parameters, yet returning a functional value, that violates rule #6.
            // return rule #6 error.
        }
    }
    else
    {
        // NOTE: Only alias non-unique parameters
        NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&alias_analysis->snapshot_arena);
        NecroAliasSet**    sets     = necro_snapshot_arena_alloc(&alias_analysis->snapshot_arena, count * sizeof(NecroAliasSet));
        size_t             i        = 0;
        fexprs                      = ast;
        while (fexprs->type == NECRO_AST_FUNCTION_EXPRESSION)
        {
            // TODO: consult type derived from ast_symbol for correct uniqueness attributes!
            // if (fexprs->fexpression.next_fexpression->necro_type->uniqueness_attribute == NECRO_TYPE_NOT_UNIQUE)
                sets[i] = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, fexprs->fexpression.next_fexpression));
            fexprs = fexprs->fexpression.aexp;
        }
        necro_snapshot_arena_rewind(&alias_analysis->snapshot_arena, snapshot);
    }
    return ok(NecroAliasSet, NULL);
}

NecroResult(NecroAliasSet) necro_alias_analysis_go(NecroAliasAnalysis* alias_analysis, NecroAst* ast)
{
    if (ast == NULL)
        return ok(NecroAliasSet, NULL);
    switch (ast->type)
    {

    //--------------------
    // Declaration type things
    //--------------------
    case NECRO_AST_DECLARATION_GROUP_LIST:
    {
        NecroAst* group_list = ast;
        while (group_list != NULL)
        {
            necro_alias_analysis_go(alias_analysis, group_list->declaration_group_list.declaration_group);
            group_list = group_list->declaration_group_list.next;
        }
        return ok(NecroAliasSet, NULL);
    }
    case NECRO_AST_DECL:
    {
        NecroAst* declaration_group = ast;
        while (declaration_group != NULL)
        {
            necro_alias_analysis_go(alias_analysis, declaration_group->declaration.declaration_impl);
            declaration_group = declaration_group->declaration.next_declaration;
        }
        return ok(NecroAliasSet, NULL);
    }
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        return necro_alias_analysis_go(alias_analysis, ast->type_class_instance.declarations);

    //--------------------
    // Assignment type things
    //--------------------
    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_DECLARATION:
            // Initializers are static time, should never alias
            if (!necro_type_is_copy_type(ast->necro_type))
                ast->variable.ast_symbol->alias_set = necro_alias_set_create_singleton(alias_analysis->arena, ast->variable.ast_symbol);
            else
                ast->variable.ast_symbol->alias_set = NULL;
            return ok(NecroAliasSet, ast->variable.ast_symbol->alias_set);
        case NECRO_VAR_VAR:
            return ok(NecroAliasSet, ast->variable.ast_symbol->alias_set);
        case NECRO_VAR_SIG:                  return ok(NecroAliasSet, NULL);
        case NECRO_VAR_TYPE_VAR_DECLARATION: return ok(NecroAliasSet, NULL);
        case NECRO_VAR_TYPE_FREE_VAR:        return ok(NecroAliasSet, NULL);
        case NECRO_VAR_CLASS_SIG:            return ok(NecroAliasSet, NULL);
        default:
            assert(false);
            return ok(NecroAliasSet, NULL);
        }
    // TODO: How to handle recursive values?
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        // necro_alias_analysis_go(alias_analysis, ast->simple_assignment.initializer); // Initializers are static time, should never alias
        if (!necro_type_is_copy_type(ast->simple_assignment.ast_symbol->type))
            ast->simple_assignment.ast_symbol->alias_set = necro_alias_set_create_singleton(alias_analysis->arena, ast->simple_assignment.ast_symbol);
        NecroAliasSet* rhs_set = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->simple_assignment.rhs));
        ast->simple_assignment.ast_symbol->alias_set = necro_alias_set_merge(alias_analysis->arena, rhs_set, ast->simple_assignment.ast_symbol->alias_set);
        return ok(NecroAliasSet, NULL);
    }
    case NECRO_AST_APATS_ASSIGNMENT:
    {
        necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, ast->apats_assignment.apats, NULL));
        NecroAliasSet* rhs_set = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->apats_assignment.rhs));
        ast->simple_assignment.ast_symbol->alias_set = rhs_set;
        return ok(NecroAliasSet, NULL);
    }
    case NECRO_AST_LAMBDA:
    {
        necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, ast->lambda.apats, NULL));
        NecroAliasSet* rhs_set = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->lambda.expression));
        return ok(NecroAliasSet, rhs_set);
    }
    // TODO: How to handle recursive values?
    case NECRO_AST_PAT_ASSIGNMENT:
    {
        NecroAliasSet* rhs_set = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->pat_assignment.rhs));
        necro_try(NecroAliasSet, necro_alias_analysis_pat(alias_analysis, ast->pat_assignment.pat, rhs_set));
        return ok(NecroAliasSet, NULL);
    }
    case NECRO_AST_CASE:
    {
        NecroAliasSet* expr_set = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->case_expression.expression));
        NecroAliasSet* alt_set  = necro_try(NecroAliasSet, necro_alias_analysis_case_alternatives(alias_analysis, ast->case_expression.alternatives, expr_set));
        return ok(NecroAliasSet, necro_alias_set_merge(alias_analysis->arena, expr_set, alt_set));
    }

    //--------------------
    // Other Stuff
    //--------------------
    case NECRO_AST_FUNCTION_EXPRESSION:
        return necro_alias_analysis_apply(alias_analysis, ast);
    case NECRO_AST_BIN_OP:
    {
        // NOTE: CURRENTLY No Bin ops conume arguments
        NecroAliasSet* set1 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->bin_op.lhs));
        NecroAliasSet* set2 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->bin_op.rhs));
        NecroAliasSet* mset = necro_alias_set_merge(alias_analysis->arena, set1, set2);
        return ok(NecroAliasSet, mset);
    }
    case NECRO_AST_OP_LEFT_SECTION:
        // NOTE: CURRENTLY No Bin ops conume arguments
        return necro_alias_analysis_go(alias_analysis, ast->op_left_section.left);
    case NECRO_AST_OP_RIGHT_SECTION:
        // NOTE: CURRENTLY No Bin ops conume arguments
        return necro_alias_analysis_go(alias_analysis, ast->op_right_section.right);
    case NECRO_AST_IF_THEN_ELSE:
    {
        necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->if_then_else.if_expr));
        NecroAliasSet* set1 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->if_then_else.then_expr));
        NecroAliasSet* set2 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->if_then_else.else_expr));
        NecroAliasSet* mset = necro_alias_set_merge(alias_analysis->arena, set1, set2);
        return ok(NecroAliasSet, mset);
    }
    case NECRO_AST_ARITHMETIC_SEQUENCE:
    {
        NecroAliasSet* set1 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->arithmetic_sequence.from));
        NecroAliasSet* set2 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->arithmetic_sequence.then));
        NecroAliasSet* set3 = necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->arithmetic_sequence.to));
        NecroAliasSet* mset = necro_alias_set_merge_many(alias_analysis->arena, (NecroAliasSet*[3]) { set1, set2, set3 }, 3);
        return ok(NecroAliasSet, mset);
    }
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->right_hand_side.declarations));
        return necro_alias_analysis_go(alias_analysis, ast->right_hand_side.expression);
    case NECRO_AST_LET_EXPRESSION:
        // ....I think this is right?
        necro_try(NecroAliasSet, necro_alias_analysis_go(alias_analysis, ast->let_expression.declarations));
        return necro_alias_analysis_go(alias_analysis, ast->let_expression.expression);

    //--------------------
    // Container type thing expressions (not used for patterns)
    //--------------------
    case NECRO_AST_LIST_NODE:
        return necro_alias_analysis_list(alias_analysis, ast);
    case NECRO_AST_EXPRESSION_LIST:
        return necro_alias_analysis_list(alias_analysis, ast->expression_list.expressions);
    case NECRO_AST_EXPRESSION_ARRAY:
        return necro_alias_analysis_list(alias_analysis, ast->expression_array.expressions);
    case NECRO_AST_PAT_EXPRESSION:
        return necro_alias_analysis_list(alias_analysis, ast->pattern_expression.expressions);
    case NECRO_AST_TUPLE:
        return necro_alias_analysis_list(alias_analysis, ast->tuple.expressions);

    //--------------------
    // Nothing to do here
    //--------------------
    case NECRO_AST_CONID:
    case NECRO_AST_WILDCARD:
    case NECRO_AST_TYPE_APP:
    case NECRO_AST_SIMPLE_TYPE:
    case NECRO_AST_TOP_DECL:
    case NECRO_AST_UNDEFINED:
    case NECRO_AST_CONSTANT:
    case NECRO_AST_UN_OP:
    case NECRO_AST_DATA_DECLARATION:
    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_FUNCTION_TYPE:
        return ok(NecroAliasSet, NULL);

    //--------------------
    // Handle in pats
    //--------------------
    case NECRO_AST_APATS:
    case NECRO_AST_CASE_ALTERNATIVE:
    case NECRO_AST_BIN_OP_SYM:
    case NECRO_AST_CONSTRUCTOR:
        assert(false && "Handle in pats");
        return ok(NecroAliasSet, NULL);

    //--------------------
    // Not Implemented
    //--------------------
    case NECRO_PAT_BIND_ASSIGNMENT:
    case NECRO_BIND_ASSIGNMENT:
    case NECRO_AST_DO:
        assert(false && "Not Implemented");
        return ok(NecroAliasSet, NULL);

    default:
        assert(false && "Unrecognized type in necro_alias_analysis");
        return ok(NecroAliasSet, NULL);
    }
}

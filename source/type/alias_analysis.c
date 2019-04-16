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

#define NECRO_ALIAS_ANALYSIS_VERBOSE 1

typedef struct
{
    NecroAstSymbol* symbol;
    NecroUsage*     usage;
} NecroAliasSetData;

typedef struct NecroAliasSet
{
    NecroPagedArena*   arena;
    NecroAliasSetData* data;
    size_t             count;
    size_t             capacity;
} NecroAliasSet;

typedef struct
{
    NecroAstArena*   ast_arena;
    NecroPagedArena* arena;
    NecroPagedArena  alias_arena;
    NecroAliasSet*   top_set;
    NecroAliasSet*   free_vars;
} NecroAliasAnalysis;

//--------------------
// Forward Declarations
//--------------------
#define NECRO_ALIAS_SET_INITIAL_CAPACITY 8
struct NecroAliasSet* necro_alias_set_create(NecroPagedArena* arena, size_t capacity);
void necro_alias_analysis_go(NecroAliasAnalysis* alias_analysis, struct NecroAliasSet* alias_set, NecroAst* ast);
void necro_alias_set_print_sharing(NecroAliasSet* alias_set);

NecroAliasAnalysis necro_alias_analysis_empty()
{
    NecroAliasAnalysis alias_analysis = (NecroAliasAnalysis)
    {
        .arena       = NULL,
        .ast_arena   = NULL,
        .alias_arena = necro_paged_arena_empty(),
        .top_set     = NULL,
        .free_vars   = NULL,
    };
    return alias_analysis;
}

NecroAliasAnalysis necro_alias_analysis_create(NecroAstArena* ast_arena)
{
    NecroPagedArena arena     = necro_paged_arena_create();
    NecroAliasSet*  alias_set = necro_alias_set_create(&arena, 512);
    // NecroAliasSet*  free_vars = necro_alias_set_create(&ast_arena->arena, 16);
    NecroAliasAnalysis alias_analysis = (NecroAliasAnalysis)
    {
        .arena       = &ast_arena->arena,
        .ast_arena   = ast_arena,
        .alias_arena = arena,
        .top_set     = alias_set,
        .free_vars   = NULL,
    };
    return alias_analysis;
}

void necro_alias_analysis_destroy(NecroAliasAnalysis* alias_analysis)
{
    assert(alias_analysis != NULL);
    necro_paged_arena_destroy(&alias_analysis->alias_arena);
    *alias_analysis = necro_alias_analysis_empty();
}

void necro_alias_analysis_impl(NecroAliasAnalysis* alias_analysis)
{
    necro_alias_analysis_go(alias_analysis, alias_analysis->top_set, alias_analysis->ast_arena->root);
    for (size_t i = 0; i < alias_analysis->top_set->capacity; ++i)
    {
        if (alias_analysis->top_set->data[i].symbol == NULL)
            continue;
        alias_analysis->top_set->data[i].symbol->usage = alias_analysis->top_set->data[i].usage;
    }
}

void necro_alias_analysis(NecroCompileInfo info, NecroAstArena* ast_arena)
{
    NecroAliasAnalysis alias_analysis = necro_alias_analysis_create(ast_arena);
    necro_alias_analysis_impl(&alias_analysis);
    if (info.verbosity > 0)
        necro_alias_set_print_sharing(alias_analysis.top_set);
    necro_alias_analysis_destroy(&alias_analysis);
}

///////////////////////////////////////////////////////
// NecroUsage
///////////////////////////////////////////////////////
NecroUsage* necro_usage_create(NecroPagedArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroUsage* usage = necro_paged_arena_alloc(arena, sizeof(NecroUsage));
    usage->source_loc = source_loc;
    usage->end_loc    = end_loc;
    usage->next       = NULL;
    return usage;
}

size_t necro_usage_count(NecroUsage* usage)
{
    size_t count = 0;
    while (usage != NULL)
    {
        count++;
        usage = usage->next;
    }
    return count;
}

bool necro_usage_is_unshared(NecroUsage* usage)
{
    return usage == NULL || usage->next == NULL;
}

///////////////////////////////////////////////////////
// NecroAliasSet
///////////////////////////////////////////////////////
static NecroAstSymbol NECRO_ALIAS_SET_TOMBSTONE = {0};
NecroAliasSet* necro_alias_set_create(NecroPagedArena* arena, size_t capacity)
{
    NecroAliasSetData* data = necro_paged_arena_alloc(arena, capacity * sizeof(NecroAliasSetData));
    for (size_t i = 0; i < capacity; ++i)
    {
        data[i].symbol = NULL;
        data[i].usage  = NULL;
    }
    NecroAliasSet* set = necro_paged_arena_alloc(arena, sizeof(NecroAliasSet));
    set->arena         = arena;
    set->data          = data;
    set->capacity      = capacity;
    set->count         = 0;
    return set;
}

void necro_free_vars_print(NecroFreeVars* free_vars)
{
    printf("free_vars, count: %zu\n", free_vars->count);
    for (size_t i = 0; i < free_vars->count; ++i)
        printf("    %s\n", (&free_vars->data)[i]->name->str);
}

NecroFreeVars* necro_alias_set_to_free_vars(NecroAliasSet* set)
{
    NecroFreeVars* free_vars = necro_paged_arena_alloc(set->arena, sizeof(NecroFreeVars*) + sizeof(size_t) + (set->count * sizeof(NecroAstSymbol)));
    free_vars->next          = NULL;
    free_vars->count         = set->count;
    if (free_vars->count == 0)
        return free_vars;
    NecroAstSymbol** data      = &free_vars->data;
    size_t           symbol_i  = 0;
    for (size_t i = 0; i < set->capacity; ++i)
    {
        NecroAstSymbol* symbol = set->data[i].symbol;
        if (symbol == NULL || symbol == &NECRO_ALIAS_SET_TOMBSTONE)
            continue;
        data[symbol_i] = symbol;
        symbol_i++;
    }
    assert(symbol_i == set->count);
    // if (free_vars->count > 0)
// #if NECRO_ALIAS_ANALYSIS_VERBOSE
//         necro_free_vars_print(free_vars);
// #endif
    return free_vars;
}

void necro_alias_set_grow(NecroAliasSet* set)
{
    NecroAliasSetData* old_data     = set->data;
    size_t             old_capacity = set->capacity;
    size_t             old_count    = set->count;
    set->capacity                  *= 2;
    set->data                       = necro_paged_arena_alloc(set->arena, set->capacity * sizeof(NecroAliasSetData));
    set->count                      = 0;
    for (size_t i = 0; i < set->capacity; ++i)
    {
        set->data[i].symbol = NULL;
        set->data[i].usage  = NULL;
    }
    for (size_t i = 0; i < old_capacity; ++i)
    {
        NecroAstSymbol* symbol = old_data[i].symbol;
        NecroUsage*     usage  = old_data[i].usage;
        if (symbol == NULL || symbol == &NECRO_ALIAS_SET_TOMBSTONE)
            continue;
        size_t hash = necro_hash((size_t)symbol) & (set->capacity - 1);
        while (set->data[hash].symbol != NULL)
        {
            hash = (hash + 1) & (set->capacity - 1);
        }
        set->data[hash].symbol = symbol;
        set->data[hash].usage  = usage;
        set->count++;
    }
    assert(set->count == old_count);
}

void necro_alias_set_empty(NecroAliasSet* set)
{
    set->count = 0;
    for (size_t i = 0; i < set->capacity; ++i)
    {
        set->data[i].symbol = NULL;
        set->data[i].usage  = NULL;
    }
}

void necro_alias_set_delete_symbol(NecroAliasSet* set, NecroAstSymbol* symbol)
{
    assert(symbol != NULL);
    size_t hash = necro_hash((size_t)symbol) & (set->capacity - 1);
    while (set->data[hash].symbol != NULL)
    {
        if (set->data[hash].symbol == symbol)
        {
            set->data[hash].symbol = &NECRO_ALIAS_SET_TOMBSTONE;
            set->data[hash].usage  = NULL;
            set->count--;
            return;
        }
        hash = (hash + 1) & (set->capacity - 1);
    }
}

void necro_alias_set_insert_symbol_without_usage(NecroAliasSet* set, NecroAstSymbol* symbol)
{
    assert(symbol != NULL);
    if (set->count * 2 >= set->capacity)
        necro_alias_set_grow(set);
    size_t hash = necro_hash((size_t)symbol) & (set->capacity - 1);
    while (set->data[hash].symbol != NULL && set->data[hash].symbol != &NECRO_ALIAS_SET_TOMBSTONE)
    {
        if (set->data[hash].symbol == symbol)
            return;
        hash = (hash + 1) & (set->capacity - 1);
    }
    set->data[hash].symbol = symbol;
    set->data[hash].usage  = NULL;
    set->count++;
}

NecroAliasSet* necro_alias_set_merge_without_usage(NecroAliasSet* set1, NecroAliasSet* set2)
{
    if (set1 == NULL)
        return NULL;
    for (size_t i = 0; i < set2->capacity; ++i)
    {
        NecroAstSymbol* symbol = set2->data[i].symbol;
        if (symbol == NULL || symbol == &NECRO_ALIAS_SET_TOMBSTONE)
            continue;
        necro_alias_set_insert_symbol_without_usage(set1, symbol);
    }
    return set1;
}

void necro_alias_set_add_usage_go(NecroAliasSet* set, NecroAstSymbol* symbol, NecroUsage* usage)
{
    assert(symbol != NULL);

    if (set->count * 2 >= set->capacity)
        necro_alias_set_grow(set);

    size_t hash = necro_hash((size_t)symbol) & (set->capacity - 1);
    while (set->data[hash].symbol != NULL && set->data[hash].symbol != &NECRO_ALIAS_SET_TOMBSTONE)
    {
        if (set->data[hash].symbol == symbol)
        {
            NecroUsage* curr_usage = usage;
            while (curr_usage->next != NULL)
                curr_usage = curr_usage->next;
            curr_usage->next      = set->data[hash].usage;
            set->data[hash].usage = usage;
            return;
        }
        hash = (hash + 1) & (set->capacity - 1);
    }
    set->data[hash].symbol = symbol;
    set->data[hash].usage  = usage;
    set->count++;
}

void necro_alias_set_add_usage(NecroPagedArena* ast_arena, NecroAliasSet* set, NecroAstSymbol* symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_alias_set_add_usage_go(set, symbol, necro_usage_create(ast_arena, source_loc, end_loc));
}

void necro_alias_set_union_children_then_merge_into_parent(NecroAliasSet* parent, NecroAliasSet** children, size_t children_count)
{
    if (children_count == 0)
        return;
    NecroAliasSet* set = NULL;
    if (children_count == 1)
    {
        set = children[0];
    }
    else
    {
        // Alloc
        size_t capacity = 0;
        for (size_t i = 0; i < children_count; ++i)
        {
            if (children[i]->capacity > capacity)
                capacity = children[i]->capacity;
        }
        assert(capacity > 0);
        set = necro_alias_set_create(children[0]->arena, capacity * 2);
        // Merge
        for (size_t set_i = 0; set_i < children_count; ++set_i)
        {
            NecroAliasSet* child = children[set_i];
            for (size_t i = 0; i < child->capacity; ++i)
            {
                NecroAstSymbol* symbol = child->data[i].symbol;
                NecroUsage*     usage  = child->data[i].usage;
                if (symbol == NULL || symbol == &NECRO_ALIAS_SET_TOMBSTONE)
                    continue;

                if (set->count * 2 >= set->capacity)
                    necro_alias_set_grow(set);

                size_t hash          = necro_hash((size_t)symbol) & (set->capacity - 1);
                bool   should_insert = true;
                while (set->data[hash].symbol != NULL && set->data[hash].symbol != &NECRO_ALIAS_SET_TOMBSTONE)
                {
                    if (set->data[hash].symbol == symbol)
                    {
                        // Replace if prev is unshared, and curr is shared
                        should_insert = necro_usage_is_unshared(set->data[hash].usage) && !necro_usage_is_unshared(usage);
                        break;
                    }
                    hash = (hash + 1) & (set->capacity - 1);
                }
                if (should_insert)
                {
                    set->data[hash].symbol = symbol;
                    set->data[hash].usage  = usage;
                    set->count++;
                }
            }
        }
    }
    // Merge into parent
    for (size_t i = 0; i < set->capacity; ++i)
    {
        NecroAstSymbol* symbol = set->data[i].symbol;
        NecroUsage*     usage  = set->data[i].usage;
        if (symbol == NULL || symbol == &NECRO_ALIAS_SET_TOMBSTONE)
            continue;
        necro_alias_set_add_usage_go(parent, symbol, usage);
    }
}

///////////////////////////////////////////////////////
// Analysis Go
///////////////////////////////////////////////////////
NecroFreeVars* necro_alias_analysis_apats_free_var_delete(NecroAliasAnalysis* alias_analysis, NecroAliasSet* alias_set, NecroAst* ast)
{
    if (ast == NULL)
        return NULL;
    assert(ast->type == NECRO_AST_APATS);
    NecroFreeVars* next_vars = necro_alias_analysis_apats_free_var_delete(alias_analysis, alias_set, ast->apats.next_apat);
    necro_alias_analysis_go(alias_analysis, alias_set, ast->apats.apat);
    NecroFreeVars* free_vars = necro_alias_set_to_free_vars(alias_analysis->free_vars);
    free_vars->next          = next_vars;
    return free_vars;
}

// Force usage for unused
void necro_alias_analysis_add_apats_to_free_vars(NecroAliasAnalysis* alias_analysis, NecroAliasSet* alias_set, NecroAst* ast)
{
    if (ast == NULL)
        return;
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_DECLARATION:
            necro_alias_set_insert_symbol_without_usage(alias_analysis->free_vars, ast->variable.ast_symbol);
            return;
        case NECRO_VAR_VAR:                  return;
        case NECRO_VAR_SIG:                  return;
        case NECRO_VAR_TYPE_VAR_DECLARATION: return;
        case NECRO_VAR_TYPE_FREE_VAR:        return;
        case NECRO_VAR_CLASS_SIG:            return;
        default:
            assert(false);
            return;
        }
    case NECRO_AST_WILDCARD:
        // TODO: How to handle this!? This can cause issues with partial application of unique types!
        // Need to force free_var usage somehow? But we're using NecroAstSymbols, and this has none!
        return;
    case NECRO_AST_APATS:
        while (ast != NULL)
        {
            necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->apats.apat);
            ast = ast->apats.next_apat;
        }
        return;
    case NECRO_AST_LIST_NODE:
        while (ast != NULL)
        {
            necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->list.item);
            ast = ast->list.next_item;
        }
        return;
    case NECRO_AST_TUPLE:
        necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->tuple.expressions);
        return;
    case NECRO_AST_EXPRESSION_LIST:
        necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->expression_list.expressions);
        return;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->expression_array.expressions);
        return;
    case NECRO_AST_CONSTRUCTOR:
        necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->constructor.arg_list);
        return;
    case NECRO_AST_CONID:
    case NECRO_AST_CONSTANT:
    case NECRO_AST_BIN_OP_SYM:
        return;
    default:
        assert(false);
        return;
    }
}

void necro_alias_analysis_go(NecroAliasAnalysis* alias_analysis, NecroAliasSet* alias_set, NecroAst* ast)
{
    if (ast == NULL)
        return;
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
            necro_alias_analysis_go(alias_analysis, alias_set, group_list->declaration_group_list.declaration_group);
            group_list = group_list->declaration_group_list.next;
            // necro_alias_set_empty(alias_analysis->free_vars);
        }
        return;
    }
    case NECRO_AST_DECL:
    {
        NecroAliasSet* prev_free_vars = alias_analysis->free_vars;
        alias_analysis->free_vars     = necro_alias_set_create(&alias_analysis->ast_arena->arena, 8);
        NecroAst* declaration_group = ast;
        while (declaration_group != NULL)
        {
            necro_alias_analysis_go(alias_analysis, alias_set, declaration_group->declaration.declaration_impl);
            declaration_group = declaration_group->declaration.next_declaration;
        }
        alias_analysis->free_vars = necro_alias_set_merge_without_usage(prev_free_vars, alias_analysis->free_vars);
        return;
    }
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->type_class_instance.declarations);
        return;

    //--------------------
    // Assignment type things
    //--------------------
    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_DECLARATION:
            necro_alias_set_delete_symbol(alias_analysis->free_vars, ast->variable.ast_symbol);
            return;
        case NECRO_VAR_VAR:
            necro_alias_set_add_usage(alias_analysis->arena, alias_set, ast->variable.ast_symbol, ast->source_loc, ast->end_loc);
            if (!necro_scope_contains(ast->scope, ast->variable.ast_symbol->source_name))
                necro_alias_set_insert_symbol_without_usage(alias_analysis->free_vars, ast->variable.ast_symbol);
            return;
        case NECRO_VAR_SIG:                  return;
        case NECRO_VAR_TYPE_VAR_DECLARATION: return;
        case NECRO_VAR_TYPE_FREE_VAR:        return;
        case NECRO_VAR_CLASS_SIG:            return;
        default:
            assert(false);
            return;
        }

    case NECRO_AST_LAMBDA:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->lambda.expression);
        necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->lambda.apats);
        ast->lambda.free_vars = necro_alias_analysis_apats_free_var_delete(alias_analysis, alias_set, ast->lambda.apats);
        return;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        // necro_alias_analysis_go(alias_analysis, ast->simple_assignment.initializer); // Initializers are static time, should never alias
        necro_alias_analysis_go(alias_analysis, alias_set, ast->simple_assignment.rhs);
        necro_alias_set_delete_symbol(alias_analysis->free_vars, ast->simple_assignment.ast_symbol);
        return;
    case NECRO_AST_APATS_ASSIGNMENT:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->apats_assignment.rhs);
        necro_alias_set_delete_symbol(alias_analysis->free_vars, ast->apats_assignment.ast_symbol);
        necro_alias_analysis_add_apats_to_free_vars(alias_analysis, alias_set, ast->apats_assignment.apats);
        ast->apats_assignment.free_vars = necro_alias_analysis_apats_free_var_delete(alias_analysis, alias_set, ast->apats_assignment.apats);
        return;
    case NECRO_AST_FOR_LOOP:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->for_loop.range_init);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->for_loop.value_init);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->for_loop.expression);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->for_loop.index_apat);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->for_loop.value_apat);
        return;
    case NECRO_AST_PAT_ASSIGNMENT:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->pat_assignment.rhs);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->pat_assignment.pat);
        return;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->case_alternative.body);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->pat_assignment.pat);
        return;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->pat_bind_assignment.expression);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->pat_bind_assignment.pat);
        return;
    case NECRO_BIND_ASSIGNMENT:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->bind_assignment.expression);
        necro_alias_set_delete_symbol(alias_analysis->free_vars, ast->bind_assignment.ast_symbol);
        return;

    //--------------------
    // Branching
    //--------------------
    // Note: We're creating separate sets for then/else branches, unioning them, then merge the result of that into the parent set.
    // We do this because it allows us to use an identifier on each side of a branch without counting as aliasing.
    case NECRO_AST_IF_THEN_ELSE:
    {

        necro_alias_analysis_go(alias_analysis, alias_set, ast->if_then_else.if_expr);
        NecroAliasSet* then_set = necro_alias_set_create(&alias_analysis->alias_arena, NECRO_ALIAS_SET_INITIAL_CAPACITY);
        NecroAliasSet* else_set = necro_alias_set_create(&alias_analysis->alias_arena, NECRO_ALIAS_SET_INITIAL_CAPACITY);
        necro_alias_analysis_go(alias_analysis, then_set, ast->if_then_else.then_expr);
        necro_alias_analysis_go(alias_analysis, else_set, ast->if_then_else.else_expr);
        necro_alias_set_union_children_then_merge_into_parent(alias_set, (NecroAliasSet*[2]) { then_set, else_set }, 2);
        return;
    }
    // NOTE: Case works the same as if/then/else, but with a variable number of branches
    case NECRO_AST_CASE:
    {
        necro_alias_analysis_go(alias_analysis, alias_set, ast->case_expression.expression);
        size_t    alt_count = 0;
        NecroAst* curr_alt  = ast->case_expression.alternatives;
        while (curr_alt != NULL)
        {
            alt_count++;
            curr_alt = curr_alt->list.next_item;
        }
        NecroAliasSet** alt_sets = necro_paged_arena_alloc(&alias_analysis->alias_arena, alt_count * sizeof(NecroAliasSet*));
        for (size_t i = 0; i < alt_count; ++i)
            alt_sets[i] = necro_alias_set_create(&alias_analysis->alias_arena, NECRO_ALIAS_SET_INITIAL_CAPACITY);
        curr_alt     = ast->case_expression.alternatives;
        size_t alt_i = 0;
        while (curr_alt != NULL)
        {
            necro_alias_analysis_go(alias_analysis, alt_sets[alt_i], curr_alt->list.item);
            alt_i++;
            curr_alt = curr_alt->list.next_item;
        }
        necro_alias_set_union_children_then_merge_into_parent(alias_set, alt_sets, alt_count);
        return;
    }

    //--------------------
    // Other Stuff
    //--------------------
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->fexpression.aexp);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->fexpression.next_fexpression);
        return;
    case NECRO_AST_BIN_OP:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->bin_op.lhs);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->bin_op.rhs);
        return;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->op_left_section.left);
        return;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->op_right_section.right);
        return;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->arithmetic_sequence.from);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->arithmetic_sequence.then);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->arithmetic_sequence.to);
        return;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->right_hand_side.expression);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->right_hand_side.declarations);
        return;
    case NECRO_AST_LET_EXPRESSION:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->let_expression.expression);
        necro_alias_analysis_go(alias_analysis, alias_set, ast->let_expression.declarations);
        return;

    //--------------------
    // Container type thing expressions (not used for branching!)
    //--------------------
    case NECRO_AST_LIST_NODE:
    {
        NecroAst* list = ast;
        while (list != NULL)
        {
            necro_alias_analysis_go(alias_analysis, alias_set, list->list.item);
            list = list->list.next_item;
        }
        return;
    }
    case NECRO_AST_EXPRESSION_LIST:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->expression_list.expressions);
        return;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->expression_array.expressions);
        return;
    case NECRO_AST_PAT_EXPRESSION:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->pattern_expression.expressions);
        return;
    case NECRO_AST_TUPLE:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->tuple.expressions);
        return;
    case NECRO_AST_DO:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->do_statement.statement_list);
        return;

    //--------------------
    // APATS
    //--------------------
    case NECRO_AST_APATS:
    {
        NecroAst* apat = ast;
        while (apat != NULL)
        {
            necro_alias_analysis_go(alias_analysis, alias_set, apat->apats.apat);
            apat = apat->apats.next_apat;
        }
        return;
    }
    case NECRO_AST_CONSTRUCTOR:
        necro_alias_analysis_go(alias_analysis, alias_set, ast->constructor.arg_list);
        return;

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
    case NECRO_AST_TYPE_ATTRIBUTE:
    case NECRO_AST_BIN_OP_SYM:
        return;

    default:
        assert(false && "Unrecognized type in necro_alias_analysis");
        return;
    }
}

void necro_alias_set_print_sharing(NecroAliasSet* alias_set)
{
    printf("-----------------\n");
    for (size_t i = 0; i < alias_set->capacity; ++i)
    {
        NecroAstSymbol* symbol = alias_set->data[i].symbol;
        NecroUsage*     usage  = alias_set->data[i].usage;
        if (symbol == NULL)
            continue;
        if (necro_usage_is_unshared(usage))
            printf("| %s: Unshared\n", symbol->name->str);
        else
            printf("| %s: Shared\n", symbol->name->str);
    }
    printf("-----------------\n\n");
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_alias_analysis_test_case(const char* test_name, const char* str, const char* name, const bool is_shared)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();
    NecroAliasAnalysis  alias_analysis  = necro_alias_analysis_create(&ast);

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    necro_alias_analysis_impl(&alias_analysis);

    // Test
    NecroSymbol     name_symbol     = necro_intern_string(&intern, name);
    NecroAstSymbol* name_ast_symbol = necro_symtable_get_top_level_ast_symbol(&scoped_symtable, name_symbol);
    bool            test_success    = !necro_usage_is_unshared(name_ast_symbol->usage) == is_shared;

    if (test_success)
        printf("%s: Passed\n", test_name);
    else
        printf("%s: FAILED\n", test_name);

#if NECRO_ALIAS_ANALYSIS_VERBOSE
    necro_alias_set_print_sharing(alias_analysis.top_set);
#endif

    // Cleanup
    necro_alias_analysis_destroy(&alias_analysis);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_ownership_test(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    info.verbosity = NECRO_ALIAS_ANALYSIS_VERBOSE;
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    info.verbosity = 0;
    NecroResult(void) result = necro_infer(info, &intern, &scoped_symtable, &base, &ast);

    // Assert
    if (result.type != expected_result)
    {
        necro_ast_arena_print(&ast);
        necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    }
#if NECRO_ALIAS_ANALYSIS_VERBOSE
    necro_scoped_symtable_print_top_scopes(&scoped_symtable);
#endif

    assert(result.type == expected_result);
    bool passed = result.type == expected_result;
    if (expected_result == NECRO_RESULT_ERROR)
    {
        assert(error_type != NULL);
        if (result.error != NULL && error_type != NULL)
        {
            assert(result.error->type == *error_type);
            passed &= result.error->type == *error_type;
        }
        else
        {
            passed = false;
        }
    }

    const char* result_string = passed ? "Passed" : "Failed";
    printf("Ownership %s test: %s\n", test_name, result_string);
    fflush(stdout);

    // Clean up
#if NECRO_ALIAS_ANALYSIS_VERBOSE
    if (result.type == NECRO_RESULT_ERROR)
        necro_result_error_print(result.error, str, "Test");
    else if (result.error)
        necro_result_error_destroy(result.type, result.error);
#else
    necro_result_error_destroy(result.type, result.error);
#endif

    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_alias_analysis_test()
{
    necro_announce_phase("Alias Analysis");
    printf("\n");

    {
        const char* test_name   = "Data Types 1";
        const char* test_source = ""
            "data Apply a b = Apply (a -> b) a\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Data Types 2";
        const char* test_source = ""
            "data LotsOfShit a b = LotsOfShit (Maybe a) (Maybe (b, b))\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Data Types 3";
        const char* test_source = ""
            "data EitherNor a b = ELeft a | ERight b | Nor\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Please Dont Crash";
        const char* test_source = ""
            "x = 0\n"
            "y = 1\n";
        const char* name        = "x";
        const bool  shared_flag = false;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Please Dont Crash 2";
        const char* test_source = ""
            "x = y\n"
            "y = x + x\n";
        const char* name        = "x";
        const bool  shared_flag = true;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "If Test";
        const char* test_source = ""
            "x = True\n"
            "y = if True then x else x\n";
        const char* name        = "x";
        const bool  shared_flag = false;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Case Test 1";
        const char* test_source = ""
            "x = True\n"
            "y = case Nothing of\n"
            "  Just _ -> x\n"
            "  _      -> x + y\n";
        const char* name        = "x";
        const bool  shared_flag = false;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Case Test 2";
        const char* test_source = ""
            "x = True\n"
            "y = case Nothing of\n"
            "  Just x -> x + x\n"
            "  _      -> x + y\n";
        const char* name        = "x";
        const bool  shared_flag = false;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Case Test 3";
        const char* test_source = ""
            "x = True\n"
            "y = case True of\n"
            "  True  -> x\n"
            "  False -> case False of\n"
            "    True  -> x\n"
            "    False -> x + y\n";
        const char* name        = "x";
        const bool  shared_flag = false;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Case Test 4";
        const char* test_source = ""
            "x = True\n"
            "y = case True of\n"
            "  True  -> x\n"
            "  False -> case False of\n"
            "    True  -> x * y\n"
            "    False -> x + y\n"
            "z = if True then x else x\n";
        const char* name        = "x";
        const bool  shared_flag = true;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Apats Test 1";
        const char* test_source = ""
            "x = True\n"
            "y (Just w) = case True of\n"
            "  True  -> x\n"
            "  False -> case False of\n"
            "    True  -> x * y Nothing\n"
            "    False -> x + y Nothing\n"
            "z = if True then (if False then x else x) else x\n";
        const char* name        = "x";
        const bool  shared_flag = true;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Apats Test 2";
        const char* test_source = ""
            "x = True\n"
            "y (Just w) = case True of\n"
            "  True  -> x\n"
            "  False -> case False of\n"
            "    True  -> x * y Nothing\n"
            "    False -> x + y Nothing\n"
            "z = if True then (if False then 0 else 1) else 2\n";
        const char* name        = "x";
        const bool  shared_flag = false;
        necro_alias_analysis_test_case(test_name, test_source, name, shared_flag);
    }

    {
        const char* test_name   = "Basic Test 0";
        const char* test_source = ""
            "dup x = let x' = x in x + x\n";
            // "dup x = x + x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 1";
        const char* test_source = ""
            "id' x = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 2";
        const char* test_source = ""
            "just' x = Just x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 3";
        const char* test_source = ""
            "dupJust' x = let y = x in Just x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 4";
        const char* test_source = ""
            "id' x = x\n"
            "nodup y = id' (Just y)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 5";
        const char* test_source = ""
            "compose f g x = f (g x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 6";
        const char* test_source = ""
            "just' = Just\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 7";
        const char* test_source = ""
            "dup x = let y = x in x\n"
            "double z = dup z\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Basic Test 8";
        const char* test_source = ""
            "data AHigherKind c a = AHigherKind (c a)\n"
            "higherMaybe x = AHigherKind (Just x)\n"
            "lowerMaybe (AHigherKind (Just x)) = let x' = x in x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unique Test 1";
        const char* test_source = ""
            "pwned :: *Int -> *Int\n"
            "pwned x = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unique Test 2";
        const char* test_source = ""
            "pwned :: *Int -> *Int\n"
            "pwned x = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unique Test 3";
        const char* test_source = ""
            "dup :: *Int -> *Int\n"
            "dup x = let y = x in x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Unique Test 4";
        const char* test_source = ""
            "id' :: a -> a\n"
            "id' x = x\n"
            "dontShare :: *Int -> *Int\n"
            "dontShare x = id' x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "maybe Test";
        const char* test_source = ""
            "maybe' y f mx =\n"
            "  case mx of\n"
            "    Just x  -> f x\n"
            "    Nothing -> y";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unique Test 5";
        const char* test_source = ""
            "pleaseShare :: Int -> Int\n"
            "pleaseShare x = x\n"
            "dontShare :: *Int -> *Int\n"
            "dontShare x = x\n"
            "pwned x = dontShare (pleaseShare x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Unique Test 6";
        const char* test_source = ""
            "dontShare :: *Maybe Int -> *Maybe Int\n"
            "dontShare x = x\n"
            "pwned x y = dontShare (dontShare x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Var test";
        const char* test_source = ""
            "dontShare :: *a -> *a\n"
            "dontShare x = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Mismatched Sig 1";
        const char* test_source = ""
            "dontShare :: a -> *a\n"
            "dontShare x = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Sig 2";
        const char* test_source = ""
            "data Pair a b = Pair a b\n"
            "dontShare :: *a -> a -> b -> *Pair a b\n"
            "dontShare x y z = Pair x z\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Sig 3";
        const char* test_source = ""
            "dontShare :: *a -> b -> (a, b)\n"
            "dontShare x y = (x, y)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Tuple";
        const char* test_source = ""
            "uid :: *a -> *a\n"
            "uid x = x\n"
            "nid :: a -> a\n"
            "nid x = x\n"
            "dup x y = (uid x, nid y)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Tuple 2";
        const char* test_source = ""
            "dup :: *a -> (a, b) -> *a\n"
            "dup x _ = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Dup 1";
        const char* test_source = ""
            "dup :: a -> (a, a)\n"
            "dup x = (x, x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Dup 2";
        const char* test_source = ""
            "dup :: *a -> *(a, a)\n"
            "dup x = (x, x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Dup 3";
        const char* test_source = ""
            "dup x = (x, x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Dup 4";
        const char* test_source = ""
            "dup x y = (x, y)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Free Var 1";
        const char* test_source = ""
            "freeVarTest :: *a -> *b -> *c -> *d -> *(a, b, c, d)\n"
            "freeVarTest w x y z = (w, x, y, z)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Free Var Error 1";
        const char* test_source = ""
            "freeVarTest :: *a -> *b -> *c -> *d -> *(a, b, c, d)\n"
            "freeVarTest w x y z = (w, x, y, z)\n"
            "partialApp = freeVarTest () () ()\n"
            "val1 = partialApp ()\n"
            "val2 = partialApp ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Free Var 2";
        const char* test_source = ""
            "freeVarTest w x y z = sub w x y z where sub w' x' y' z' = (w', x', y', z')\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Free Var 3";
        const char* test_source = ""
            "freeVarTest1 w x y z = sub w x y z where sub w' x' y' z' = (w', x', y', z')\n"
            "freeVarTest2 a b c d = sub a b c d where sub a' b' c' d' = (a', b', c', d')\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Free Var 4";
        const char* test_source = ""
            "freeVarTest2 x y z = (sub1 x y z, sub2 x y z) where\n"
            "  sub1 a b c = (a, b, c)\n"
            "  sub2 d e f = (d, e, f)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Free Var 5";
        const char* test_source = ""
            "top1 = 0\n"
            "top2 x y = sub x y where sub x' y' = x' + y' + top1\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Free Var 6";
        const char* test_source = ""
            "freeVarTest = \\w x y z -> (w, x, y, z)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Function Test 1";
        const char* test_source = ""
            "frugal :: *a -> *b -> *c -> *d -> ()\n"
            "frugal x y z w = ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Function Test 2";
        const char* test_source = ""
            "frugal x y z w = ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Function Test 3";
        const char* test_source = ""
            "frugal (u, v, x, y) z w = ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Function Test 2";
        const char* test_source = ""
            "frugal x y z w = ()\n"
            "neverShare :: *()\n"
            "neverShare = frugal 0 1 2 3\n"
            "alwaysShare :: ()\n"
            "alwaysShare = frugal Nothing () True False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unique FreeVar Error";
        const char* test_source = ""
            "neverShare :: *()\n"
            "neverShare = ()\n"
            "coughItUp _ = neverShare\n"
            "one = coughItUp True\n"
            "two = coughItUp False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "HKT 1";
        const char* test_source = ""
            "data AppMaybe m a = AppMaybe (m a)\n"
            "appMaybe :: *AppMaybe m a -> m a\n"
            "appMaybe (AppMaybe m) = m\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "HKT 2";
        const char* test_source = ""
            "data AppMaybe m a = AppMaybe (m a)\n"
            "appMaybe :: *AppMaybe m a -> *m a\n"
            "appMaybe (AppMaybe m) = m\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Constraint Test 1";
        const char* test_source = ""
            "neverShare :: *Bool\n"
            "neverShare   = True\n"
            "frugal x y z = ()\n"
            "notFrugal    = frugal () neverShare\n"
            "bang         = notFrugal ()\n"
            "pop          = notFrugal ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Constraint Test 2";
        const char* test_source = ""
            "neverShare :: *Bool\n"
            "neverShare   = True\n"
            "alwaysShare :: ()\n"
            "alwaysShare = ()\n"
            "frugal x y z = ()\n"
            "notFrugal    = frugal alwaysShare neverShare\n"
            "bang         = notFrugal ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Class Test 1";
        const char* test_source = ""
            "class UClass a where\n"
            "  fu :: *a -> *Maybe a\n"
            "instance UClass Bool where\n"
            "  fu b = Just b\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Class Test 2";
        const char* test_source = ""
            "class UClass a where\n"
            "  fu :: *a -> *Maybe a\n"
            "instance UClass Bool where\n"
            "  fu b = Just b\n"
            "ezGame :: (UClass a, UClass b) => *a -> *b -> *(Maybe a, Maybe b)\n"
            "ezGame x y = (fu x, fu y)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Class Test 3";
        const char* test_source = ""
            "class UClass a where\n"
            "  fu :: *a -> Bool\n"
            "instance UClass Bool where\n"
            "  fu b = b\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    // Seems like sharing isn't propagating correctly here!
    {
        const char* test_name   = "App";
        const char* test_source = ""
            "frugal x y z  = ()\n"
            "true          = True\n"
            "false         = False\n"
            "partial       = frugal true false\n"
            "appFrugal :: (a -> ()) -> a -> ()\n"
            "appFrugal f x = f x\n"
            "go1           = appFrugal partial ()\n"
            "go2           = appFrugal partial ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Class Test 3";
        const char* test_source = ""
            "id' x = x\n"
            "t = True\n"
            "u = id' t\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Warp Pipe 1";
        const char* test_source = ""
            "w :: *()\n"
            "w = ()\n"
            "id' :: *a -> *a\n"
            "id' x = x\n"
            "w' = w |> id' |> id' |> id'\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Warp Pipe 2";
        const char* test_source = ""
            "w :: *()\n"
            "w = ()\n"
            "ignore :: *b -> *a -> *a\n"
            "ignore y x = x\n"
            "w' = w |> ignore () |> ignore True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Bad Pipe";
        const char* test_source = ""
            "w :: *()\n"
            "w = ()\n"
            "badPipe :: *a -> (*a -> *b) -> *b\n"
            "badPipe x f = f x\n"
            "ignore :: *b -> *a -> *a\n"
            "ignore y x = x\n"
            "w' = badPipe w (ignore ())\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_ownership_test(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "maybe Test Poly";
        const char* test_source = ""
            "maybe' :: .b -> .(.a -> .b) -> .Maybe a -> .b\n"
            "maybe' y f mx =\n"
            "  case mx of\n"
            "    Just x  -> f x\n"
            "    Nothing -> y";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "tuple Test Poly";
        const char* test_source = ""
            "fst' :: .(a, b) -> .a\n"
            "fst' (x, _) = x\n"
            "snd' :: .(a, b) -> .b\n"
            "snd' (_, y) = y\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    }

    // TODO: Anon Dot support. Needs more testing.
    // TODO: Reduce the amount of uvars/constraints floating around from apats.
    // TODO: where / let declaration testing with simple assignment and apats assignment!
    // TODO: Uniqueness Typed Base functions (Or prehaps move stuff like that into a prelude library).

    // // TODO: fromInt is shared currently...
    // {
    //     const char* test_name   = "Attribute Test 1";
    //     const char* test_source = ""
    //         "pwned :: *Int\n"
    //         "pwned = 0\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
    //     necro_ownership_test(test_name, test_source, expect_error_result, NULL);
    // }

}

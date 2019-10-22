/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "state_analysis.h"
#include <stdio.h>
#include "type.h"
#include "kind.h"
#include "core_infer.h"
#include "monomorphize.h"
#include "lambda_lift.h"
#include "alias_analysis.h"
#include "core_simplify.h"
#include "defunctionalization.h"

typedef struct NecroCoreAstSymbolBucket
{
    size_t              hash;
    NecroCoreAstSymbol* core_symbol;
    NecroType*          type;
    NecroCoreAstSymbol* specialized_core_symbol;
    NecroCoreAstSymbol* deep_copy_fn;
} NecroCoreAstSymbolBucket;

typedef struct NecroCoreAstSymbolTable
{
    NecroCoreAstSymbolBucket* buckets;
    size_t                    count;
    size_t                    capacity;
} NecroCoreAstSymbolTable;

typedef struct NecroStateAnalysis
{
    NecroIntern*            intern;
    NecroBase*              base;
    NecroCoreAstSymbolTable symtable;
    NecroPagedArena*        arena;
    NecroSnapshotArena      snapshot_arena;
    NecroCoreAst*           new_lets_head;
    NecroCoreAst*           new_lets_tail;
} NecroStateAnalysis;

//--------------------
// Forward declarations
//--------------------
NECRO_STATE_TYPE        necro_state_analysis_go(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer);
NECRO_STATE_TYPE        necro_state_analysis_pat_go(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer);
NecroCoreAstSymbolTable necro_core_ast_symbol_table_create();
void                    necro_core_ast_symbol_table_destroy(NecroCoreAstSymbolTable* symtable);
NecroCoreAstSymbol*     necro_core_ast_symbol_get_specialized(NecroCoreAstSymbolTable* symtable, NecroCoreAstSymbol* core_symbol, NecroType* type);
NecroType*              necro_core_ast_type_specialize(NecroStateAnalysis* sa, NecroType* type);
NecroCoreAstSymbol*     necro_core_ast_symbol_get_or_insert_specialized(NecroStateAnalysis* sa, NecroCoreAstSymbol* core_symbol, NecroType* type, NecroType** out_type_to_set);
NecroCoreAst*           necro_core_ast_maybe_deep_copy(NecroStateAnalysis* context, NecroCoreAst* ast_to_deep_copy);
// void                    necro_core_ast_create_deep_copy_fns(NecroStateAnalysis* context, NecroCoreAst* top);

//--------------------
// Analyze
//--------------------
void necro_core_state_analysis(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    UNUSED(info);
    NecroStateAnalysis sa = (NecroStateAnalysis)
    {
        .intern         = intern,
        .base           = base,
        .arena          = &core_ast_arena->arena,
        .new_lets_head  = NULL,
        .new_lets_tail  = NULL,
        .snapshot_arena = necro_snapshot_arena_create(),
        .symtable       = necro_core_ast_symbol_table_create(),
    };

    // TODO: Where is the best place for this!?
    base->print_int->core_ast_symbol->state_type = NECRO_STATE_POINTWISE;
    base->mouse_x_fn->core_ast_symbol->state_type = NECRO_STATE_POINTWISE;
    base->mouse_y_fn->core_ast_symbol->state_type = NECRO_STATE_POINTWISE;
    base->world_type->core_ast_symbol->state_type = NECRO_STATE_POINTWISE;

    // Go
    necro_state_analysis_go(&sa, core_ast_arena->root, NULL);

    // Append new top level lets
    if (sa.new_lets_head != NULL)
    {
        NecroCoreAst* top = core_ast_arena->root;
        assert(top != NULL);
        assert(top->ast_type == NECRO_CORE_AST_LET);
        assert(sa.new_lets_tail->let.expr == NULL);
        while (top->let.expr != NULL && top->let.expr->ast_type == NECRO_CORE_AST_LET && top->let.expr->let.bind->ast_type == NECRO_CORE_AST_DATA_DECL)
        {
            top = top->let.expr;
        }
        NecroCoreAst* next         = top->let.expr;
        top->let.expr              = sa.new_lets_head;
        sa.new_lets_tail->let.expr = next;
    }

    // Finish
    unwrap(void, necro_core_infer(intern, base, core_ast_arena)); // TODO: Remove after some testing

    if ((info.compilation_phase == NECRO_PHASE_STATE_ANALYSIS && info.verbosity > 0) || info.verbosity > 1)
        necro_core_ast_pretty_print(core_ast_arena->root);

    // Cleanup
    necro_snapshot_arena_destroy(&sa.snapshot_arena);
    necro_core_ast_symbol_table_destroy(&sa.symtable);
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_merge_state_types(NECRO_STATE_TYPE state_type1, NECRO_STATE_TYPE state_type2)
{
    if (state_type1 > state_type2)
        return state_type1;
    else
        return state_type2;
}

void necro_set_outer_rec_stateful(NecroCoreAstSymbol* outer)
{
    if (outer->outer == NULL)
    {
        // Top level bindings store state into global variables, thus are actually pointwise
        outer->state_type = NECRO_STATE_POINTWISE;
        return;
    }
    while (outer != NULL)
    {
        outer->state_type = NECRO_STATE_STATEFUL;
        outer = outer->outer;
    }
}

///////////////////////////////////////////////////////
// State Analysis
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_var(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    UNUSED(outer);
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    ast->var.ast_symbol = necro_core_ast_symbol_get_or_insert_specialized(sa, ast->var.ast_symbol, ast->necro_type, &ast->necro_type);
    if (ast->var.ast_symbol->ast != NULL && ast->var.ast_symbol->ast->ast_type == NECRO_CORE_AST_BIND && ast->var.ast_symbol->ast->bind.initializer != NULL)
        ast->var.ast_symbol->state_type = NECRO_STATE_STATEFUL;
    if (ast->var.ast_symbol->is_constructor)
        ast->var.ast_symbol->state_type = NECRO_STATE_POLY;
    if (ast->var.ast_symbol->state_type == NECRO_STATE_STATEFUL && ast->var.ast_symbol->arity == 0) // NOTE: This assumes arity has been filled in by previous pass
        return NECRO_STATE_POINTWISE;
    return ast->var.ast_symbol->state_type;
}

NECRO_STATE_TYPE necro_state_analysis_var_pat(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    // TODO / NOTE: Should pat state types be inherited from the expression they are destructuring!?!?!?!
    UNUSED(outer);
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    ast->var.ast_symbol = necro_core_ast_symbol_get_or_insert_specialized(sa, ast->var.ast_symbol, ast->necro_type, &ast->necro_type);
    ast->var.ast_symbol->state_type = NECRO_STATE_POLY;
    return ast->var.ast_symbol->state_type;
}

NECRO_STATE_TYPE necro_state_analysis_lam(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    NecroCoreAst* arg                           = ast->lambda.arg;
    arg->var.ast_symbol                         = necro_core_ast_symbol_get_or_insert_specialized(sa, arg->var.ast_symbol, arg->necro_type, &ast->necro_type);
    ast->lambda.arg->var.ast_symbol->state_type = NECRO_STATE_POLY;
    ast->necro_type                             = necro_core_ast_type_specialize(sa, ast->necro_type);
    return necro_state_analysis_go(sa, ast->lambda.expr, outer);
}

NECRO_STATE_TYPE necro_state_analysis_let(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LET);
    ast->necro_type = necro_core_ast_type_specialize(sa, ast->necro_type);
    // TODO: We're going to blow the stack like this, transform from recursive to iterative!
    necro_state_analysis_go(sa, ast->let.bind, outer);
    if (ast->let.expr == NULL)
        return NECRO_STATE_CONSTANT;
    else
        return necro_state_analysis_go(sa, ast->let.expr, outer);
}

NECRO_STATE_TYPE necro_state_analysis_case(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_CASE);

    ast->necro_type = necro_core_ast_type_specialize(sa, ast->necro_type);

    // Expr
    NECRO_STATE_TYPE state_type = necro_state_analysis_go(sa, ast->case_expr.expr, outer);

    // Alts
    NecroCoreAstList* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_state_analysis_pat_go(sa, alts->data->case_alt.pat, outer);
        state_type = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, alts->data->case_alt.expr, outer));
        alts       = alts->next;
    }

    return state_type;
}

NECRO_STATE_TYPE necro_state_analysis_bind(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    ast->necro_type             = necro_core_ast_type_specialize(sa, ast->necro_type);
    NecroCoreAstSymbol* symbol  = ast->bind.ast_symbol;
    symbol->type                = necro_core_ast_type_specialize(sa, symbol->type);
    symbol->outer               = outer;
    if (ast->bind.initializer != NULL)
    {
        symbol->state_type = NECRO_STATE_STATEFUL;
        necro_state_analysis_go(sa, ast->bind.initializer, symbol);
        necro_set_outer_rec_stateful(symbol);
        symbol->state_type = necro_state_analysis_merge_state_types(symbol->state_type, necro_state_analysis_go(sa, ast->bind.expr, symbol));
        ast->bind.expr     = necro_core_ast_maybe_deep_copy(sa, ast->bind.expr);
    }
    else
    {
        symbol->state_type = necro_state_analysis_merge_state_types(symbol->state_type, necro_state_analysis_go(sa, ast->bind.expr, symbol));
    }
    return symbol->state_type;
}

NECRO_STATE_TYPE necro_state_analysis_bind_rec(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_BIND_REC);

    assert(false && "TODO");

    // TODO / NOTE: This is probably wrong, as we'll need to propagate statefulness changes and handle outers amongst all of these...

    // Binds
    NECRO_STATE_TYPE  state_type = NECRO_STATE_CONSTANT;
    NecroCoreAstList* binds      = ast->bind_rec.binds;
    while (binds != NULL)
    {
        state_type = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, binds->data->bind.expr, outer));
        binds      = binds->next;
    }

    return state_type;

}

NECRO_STATE_TYPE necro_state_analysis_loop(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LOOP);
    ast->necro_type             = necro_core_ast_type_specialize(sa, ast->necro_type);
    NECRO_STATE_TYPE state_type = necro_state_analysis_pat_go(sa, ast->loop.value_pat, outer);
    state_type                  = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->loop.value_init, outer));
    if (ast->loop.loop_type == NECRO_LOOP_FOR)
    {
        state_type = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_pat_go(sa, ast->loop.for_loop.index_pat, outer));
        state_type = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->loop.for_loop.range_init, outer));
        state_type = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->loop.do_expression, outer));
    }
    else
    {
        state_type              = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->loop.while_loop.while_expression, outer));
        state_type              = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->loop.do_expression, outer));
        ast->loop.do_expression = necro_core_ast_maybe_deep_copy(sa, ast->loop.do_expression);
    }
    return state_type;
}

NECRO_STATE_TYPE necro_state_analysis_app(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    NecroCoreAst*    app             = ast;
    NECRO_STATE_TYPE args_state_type = NECRO_STATE_CONSTANT;
    while (app->ast_type == NECRO_CORE_AST_APP)
    {
        app->necro_type = necro_core_ast_type_specialize(sa, app->necro_type);
        args_state_type = necro_state_analysis_merge_state_types(args_state_type, necro_state_analysis_go(sa, app->app.expr2, outer));
        app             = app->app.expr1;
    }
    NECRO_STATE_TYPE fn_state_type = necro_state_analysis_go(sa, app, outer);
    if (fn_state_type == NECRO_STATE_POLY)
        return args_state_type;
    else
        return necro_state_analysis_merge_state_types(fn_state_type, args_state_type);
}

NECRO_STATE_TYPE necro_state_analysis_app_pat(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    NecroCoreAst*    app             = ast;
    NECRO_STATE_TYPE args_state_type = NECRO_STATE_POLY;
    while (app->ast_type == NECRO_CORE_AST_APP)
    {
        app->necro_type = necro_core_ast_type_specialize(sa, app->necro_type);
        args_state_type = necro_state_analysis_merge_state_types(args_state_type, necro_state_analysis_pat_go(sa, app->app.expr2, outer));
        app             = app->app.expr1;
    }
    NECRO_STATE_TYPE fn_state_type = necro_state_analysis_pat_go(sa, app, outer);
    if (fn_state_type == NECRO_STATE_POLY)
        return args_state_type;
    else
        return necro_state_analysis_merge_state_types(fn_state_type, args_state_type);
}

NECRO_STATE_TYPE necro_state_analysis_lit(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LIT);
    if (ast->lit.type != NECRO_AST_CONSTANT_ARRAY)
        return NECRO_STATE_CONSTANT;
    NECRO_STATE_TYPE  state_type = NECRO_STATE_CONSTANT;
    ast->necro_type              = necro_core_ast_type_specialize(sa, ast->necro_type);
    NecroCoreAstList* elements   = ast->lit.array_literal_elements;
    while (elements != NULL)
    {
        state_type = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_pat_go(sa, elements->data, outer));
        elements   = elements->next;
    }
    return state_type;
}

///////////////////////////////////////////////////////
// Go
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_go(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:       return necro_state_analysis_var(sa, ast, outer);
    case NECRO_CORE_AST_APP:       return necro_state_analysis_app(sa, ast, outer);
    case NECRO_CORE_AST_LAM:       return necro_state_analysis_lam(sa, ast, outer);
    case NECRO_CORE_AST_LET:       return necro_state_analysis_let(sa, ast, outer);
    case NECRO_CORE_AST_CASE:      return necro_state_analysis_case(sa, ast, outer);
    case NECRO_CORE_AST_BIND:      return necro_state_analysis_bind(sa, ast, outer);
    case NECRO_CORE_AST_BIND_REC:  return necro_state_analysis_bind_rec(sa, ast, outer);
    case NECRO_CORE_AST_LOOP:      return necro_state_analysis_loop(sa, ast, outer);
    case NECRO_CORE_AST_LIT:       return necro_state_analysis_lit(sa, ast, outer);
    case NECRO_CORE_AST_DATA_DECL: return NECRO_STATE_CONSTANT;
    case NECRO_CORE_AST_DATA_CON:  return NECRO_STATE_CONSTANT;
    default:                       assert(false && "Unimplemented AST type in necro_state_analysis_go"); return NECRO_STATE_CONSTANT;
    }
}

NECRO_STATE_TYPE necro_state_analysis_pat_go(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_VAR: return necro_state_analysis_var_pat(sa, ast, outer);
    case NECRO_CORE_AST_APP: return necro_state_analysis_app_pat(sa, ast, outer);
    case NECRO_CORE_AST_LIT: return NECRO_STATE_POLY;
    default:
        assert(false && "necro_state_analysis_pat_go");
        return NECRO_STATE_POLY;
    }
}

///////////////////////////////////////////////////////
// NecroCoreAstSymbolTable
///////////////////////////////////////////////////////
NecroCoreAstSymbolTable necro_core_ast_symbol_table_empty()
{
    return (NecroCoreAstSymbolTable)
    {
        .buckets  = NULL,
        .count    = 0,
        .capacity = 0,
    };
}

NecroCoreAstSymbolTable necro_core_ast_symbol_table_create()
{
    const size_t initial_capacity = 512;
    NecroCoreAstSymbolTable symtable =
    {
        .buckets  = emalloc(initial_capacity * sizeof(NecroCoreAstSymbolBucket)),
        .count    = 0,
        .capacity = initial_capacity,
    };
    for (size_t i = 0; i < symtable.capacity; ++i)
        symtable.buckets[i] = (NecroCoreAstSymbolBucket){ .hash = 0, .core_symbol = NULL, .type = NULL, .specialized_core_symbol = NULL, .deep_copy_fn = NULL };
    return symtable;
}

void necro_core_ast_symbol_table_destroy(NecroCoreAstSymbolTable* symtable)
{
    assert(symtable != NULL);
    free(symtable->buckets);
    *symtable = necro_core_ast_symbol_table_empty();
}

void necro_core_ast_symbol_table_grow(NecroCoreAstSymbolTable* symtable)
{
    assert(symtable != NULL);
    assert(symtable->count > 0);
    assert(symtable->capacity >= symtable->count);
    assert(symtable->buckets != NULL);
    const size_t              old_count    = symtable->count;
    const size_t              old_capacity = symtable->capacity;
    NecroCoreAstSymbolBucket* old_buckets  = symtable->buckets;
    symtable->count                        = 0;
    symtable->capacity                     = old_capacity * 2;
    symtable->buckets                      = emalloc(symtable->capacity * sizeof(NecroCoreAstSymbolBucket));
    for (size_t i = 0; i < symtable->capacity; ++i)
        symtable->buckets[i] = (NecroCoreAstSymbolBucket){ .hash = 0, .core_symbol = NULL, .type = NULL, .specialized_core_symbol = NULL, .deep_copy_fn = NULL };
    for (size_t i = 0; i < old_capacity; ++i)
    {
        const NecroCoreAstSymbolBucket* bucket = old_buckets + i;
        if (bucket->core_symbol == NULL)
            continue;
        size_t bucket_index = bucket->hash & (symtable->capacity - 1);
        while (true)
        {
            if (symtable->buckets[bucket_index].core_symbol == NULL)
            {
                symtable->buckets[bucket_index] = *bucket;
                symtable->count++;
                break;
            }
            bucket_index = (bucket_index + 1) & (symtable->capacity - 1);
        }
    }
    assert(symtable->count == old_count);
    assert(symtable->count > 0);
    assert(symtable->capacity >= symtable->count);
    free(old_buckets);
}


void necro_core_ast_symbol_insert_specialized(NecroCoreAstSymbolTable* symtable, NecroCoreAstSymbol* core_symbol, NecroType* type, NecroCoreAstSymbol* specialized_core_symbol)
{
    assert(core_symbol != NULL);
    assert(type != NULL);
    assert(specialized_core_symbol != NULL);
    // Grow
    if ((symtable->count * 2) >= symtable->capacity)
        necro_core_ast_symbol_table_grow(symtable);
    // Hash
    type                = necro_type_strip_for_all(necro_type_find(type));
    size_t hash         = core_symbol->name->hash ^ necro_type_hash(type);
    size_t bucket_index = hash & (symtable->capacity - 1);
    // Find
    while (true)
    {
        NecroCoreAstSymbolBucket* bucket = symtable->buckets + bucket_index;
        if (bucket->hash == hash && bucket->specialized_core_symbol != NULL && bucket->core_symbol == core_symbol && necro_type_exact_unify(type, bucket->type))
        {
            // Found
            return;
        }
        else if (bucket->specialized_core_symbol == NULL)
        {
            // Insert
            bucket->hash                    = hash;
            bucket->core_symbol             = core_symbol;
            bucket->type                    = type;
            bucket->specialized_core_symbol = specialized_core_symbol;
            symtable->count++;
            return;
        }
        bucket_index = (bucket_index + 1) & (symtable->capacity - 1);
    }
    assert(false);
}

NecroCoreAstSymbol* necro_core_ast_symbol_get_specialized(NecroCoreAstSymbolTable* symtable, NecroCoreAstSymbol* core_symbol, NecroType* type)
{
    assert(core_symbol != NULL);
    assert(type != NULL);
    // Hash
    type                = necro_type_strip_for_all(necro_type_find(type));
    size_t hash         = core_symbol->name->hash ^ necro_type_hash(type);
    size_t bucket_index = hash & (symtable->capacity - 1);
    // Find
    while (true)
    {
        NecroCoreAstSymbolBucket* bucket = symtable->buckets + bucket_index;
        if (bucket->hash == hash && bucket->specialized_core_symbol != NULL && bucket->core_symbol == core_symbol && necro_type_exact_unify(type, bucket->type))
        {
            // Found
            return bucket->specialized_core_symbol;
        }
        else if (bucket->specialized_core_symbol == NULL)
        {
            // Not found
            return NULL;
        }
        bucket_index = (bucket_index + 1) & (symtable->capacity - 1);
    }
    assert(false);
}

NecroCoreAstSymbol* necro_core_ast_symbol_get_or_insert_specialized(NecroStateAnalysis* sa, NecroCoreAstSymbol* core_symbol, NecroType* type, NecroType** out_type_to_set)
{
    if (core_symbol == sa->base->prim_undefined->core_ast_symbol)
    {
        *out_type_to_set = necro_core_ast_type_specialize(sa, type);
        return core_symbol;
    }
    NecroType* specialized_type = necro_core_ast_type_specialize(sa, type);
    if (type == specialized_type)
        return core_symbol;
    NecroCoreAstSymbol* specialized_symbol = necro_core_ast_symbol_get_specialized(&sa->symtable, core_symbol, type);
    if (specialized_symbol != NULL)
    {
        *out_type_to_set = specialized_symbol->type;
        return specialized_symbol;
    }
    else
    {
        core_symbol->type = necro_core_ast_type_specialize(sa, core_symbol->type);
        necro_core_ast_symbol_insert_specialized(&sa->symtable, core_symbol, type, core_symbol);
        necro_core_ast_symbol_insert_specialized(&sa->symtable, core_symbol, core_symbol->type, core_symbol);
        *out_type_to_set = core_symbol->type;
        return core_symbol;
    }
}

///////////////////////////////////////////////////////
// Specialize types
///////////////////////////////////////////////////////
NecroCoreAst* necro_core_ast_add_new_top_let(NecroStateAnalysis* sa, NecroCoreAst* data_decl_ast)
{
    if (sa->new_lets_head == NULL)
    {
        sa->new_lets_head = necro_core_ast_create_let(sa->arena, data_decl_ast, NULL);
        sa->new_lets_tail = sa->new_lets_head;
    }
    else
    {
        sa->new_lets_tail->let.expr = necro_core_ast_create_let(sa->arena, data_decl_ast, NULL);
        sa->new_lets_tail = sa->new_lets_tail->let.expr;
    }
    return sa->new_lets_tail;
}

NecroType* necro_core_ast_type_specialize(NecroStateAnalysis* sa, NecroType* type)
{
    if (type == NULL)
        return type;
    type = necro_type_find(type);
    switch (type->type)
    {

    case NECRO_TYPE_CON:
    {
        //--------------------
        // Monotype early exit
        //--------------------
        if (type->con.args == NULL)
            return type;
        NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&sa->snapshot_arena);

        //--------------------
        // Primitively polymoprhic types Early exit
        if (type->con.con_symbol == sa->base->array_type)
        {
            NecroType* con_args = necro_core_ast_type_specialize(sa, type->con.args);
            if (con_args == type->con.args)
                return type;
            NecroType* new_type = necro_type_con_create(sa->arena, type->con.con_symbol, con_args);
            new_type->kind      = type->kind;
            new_type->ownership = type->ownership;
            // TODO: Insert as specialized version into type hash map?
            return new_type;
        }
        else if (type->con.con_symbol == sa->base->index_type)
        {
            return sa->base->uint_type->type;
        }

        //--------------------
        // Check to see if we've specialized this type befor
        NecroCoreAstSymbol* maybe_specialized_symbol = necro_core_ast_symbol_get_specialized(&sa->symtable, type->con.con_symbol->core_ast_symbol, type);
        if (maybe_specialized_symbol != NULL)
        {
            NecroType* new_type = necro_type_deep_copy(sa->arena, maybe_specialized_symbol->type);
            new_type->ownership = type->ownership;
            return new_type;
        }

        //--------------------
        // Create Specialized Name
        const size_t      specialized_type_name_length = necro_type_mangled_string_length(type);
        char*             specialized_type_name_buffer = necro_snapshot_arena_alloc(&sa->snapshot_arena, specialized_type_name_length * sizeof(char));
        necro_type_mangled_sprintf(specialized_type_name_buffer, 0, type);
        const NecroSymbol specialized_type_source_name = necro_intern_string(sa->intern, specialized_type_name_buffer);

        //--------------------
        // Create specialized type suffix
        char* specialized_type_suffix_buffer = specialized_type_name_buffer;
        while (*specialized_type_suffix_buffer != '<')
            specialized_type_suffix_buffer++;
        NecroSymbol specialized_type_suffix_symbol = necro_intern_string(sa->intern, specialized_type_suffix_buffer);

        //--------------------
        // Create Specialized Symbol and Type
        NecroAstSymbol* ast_symbol           = necro_ast_symbol_create(sa->arena, specialized_type_source_name, specialized_type_source_name, type->con.con_symbol->module_name, NULL);
        ast_symbol->is_constructor           = type->con.con_symbol->is_constructor;
        ast_symbol->is_enum                  = type->con.con_symbol->is_enum;
        ast_symbol->is_primitive             = type->con.con_symbol->is_primitive;
        ast_symbol->is_recursive             = type->con.con_symbol->is_recursive;
        ast_symbol->con_num                  = type->con.con_symbol->con_num;
        ast_symbol->is_unboxed               = type->con.con_symbol->is_unboxed;
        ast_symbol->primop_type              = type->con.con_symbol->primop_type;
        ast_symbol->type                     = necro_type_con_create(sa->arena, ast_symbol, NULL);
        ast_symbol->type->kind               = sa->base->star_kind->type;
        NecroCoreAstSymbol* core_symbol      = necro_core_ast_symbol_create_from_ast_symbol(sa->arena, ast_symbol);
        NecroType*          specialized_type = ast_symbol->type;
        necro_core_ast_symbol_insert_specialized(&sa->symtable, type->con.con_symbol->core_ast_symbol, type, core_symbol);

        //--------------------
        // Specialize args
        NecroType* con_args = type->con.args;
        while (con_args != NULL)
        {
            necro_core_ast_type_specialize(sa, con_args->list.item);
            con_args = con_args->list.next;
        }

        //--------------------
        // Specialize Data Declaration and Data Cons
        NecroCoreAst*       poly_decl   = type->con.con_symbol->core_ast_symbol->ast;
        NecroCoreAstList*   poly_cons   = poly_decl->data_decl.con_list;
        NecroCoreAstList*   data_cons   = NULL;
        while (poly_cons != NULL)
        {
            //--------------------
            // Poly data_con data
            NecroCoreAst*       poly_con        = poly_cons->data;
            NecroCoreAstSymbol* poly_con_symbol = poly_con->data_con.ast_symbol;
            NecroType*          poly_con_type   = poly_con->data_con.type;

            //--------------------
            // Specialize data_con name and symbol
            NecroSymbol         new_con_name    = necro_intern_concat_symbols(sa->intern, poly_con->data_con.ast_symbol->name, specialized_type_suffix_symbol);
            NecroAstSymbol*     con_ast_symbol  = necro_ast_symbol_create(sa->arena, new_con_name, new_con_name, ast_symbol->module_name, NULL);
            con_ast_symbol->is_constructor      = poly_con_symbol->is_constructor;
            con_ast_symbol->is_enum             = poly_con_symbol->is_enum;
            con_ast_symbol->is_primitive        = poly_con_symbol->is_primitive;
            con_ast_symbol->is_recursive        = poly_con_symbol->is_recursive;
            con_ast_symbol->con_num             = poly_con_symbol->con_num;
            con_ast_symbol->is_unboxed          = poly_con_symbol->is_unboxed;
            con_ast_symbol->primop_type         = poly_con_symbol->primop_type;
            NecroCoreAstSymbol* con_core_symbol = necro_core_ast_symbol_create_from_ast_symbol(sa->arena, con_ast_symbol);

            //--------------------
            // Specialize data_con type
            NecroType* data_con_type   = unwrap_result(NecroType, necro_type_instantiate(sa->arena, NULL, sa->base, poly_con_type, NULL));
            NecroType* data_con_result = data_con_type;
            while (data_con_result->type == NECRO_TYPE_FUN)
                data_con_result = data_con_result->fun.type2;
            unwrap(NecroType, necro_type_unify(sa->arena, NULL, sa->base, data_con_result, type, NULL));
            // TODO: Is this necessary?
            // unwrap(void, necro_kind_infer_default_unify_with_star(monomorphize->arena, monomorphize->base, specialized_data_con_type, NULL, NULL_LOC, NULL_LOC));
            NecroType* specialized_data_con_type = necro_core_ast_type_specialize(sa, data_con_type); // Specialize and cache data_con_type
            unwrap(void, necro_kind_infer_default_unify_with_star(sa->arena, sa->base, specialized_data_con_type, NULL, NULL_LOC, NULL_LOC));
            con_ast_symbol->type  = specialized_data_con_type;
            con_core_symbol->type = specialized_data_con_type;

            //--------------------
            // Append, the move to next data_con
            necro_core_ast_symbol_insert_specialized(&sa->symtable, poly_con_symbol, data_con_type, con_core_symbol);
            NecroCoreAst* data_con = necro_core_ast_create_data_con(sa->arena, con_core_symbol, specialized_data_con_type, specialized_type);
            data_cons              = necro_append_core_ast_list(sa->arena, data_con, data_cons);
            poly_cons              = poly_cons->next;
        }

        //--------------------
        // Create data declaration
        NecroCoreAst* data_decl = necro_core_ast_create_data_decl(sa->arena, core_symbol, data_cons);
        necro_core_ast_add_new_top_let(sa, data_decl);

        //--------------------
        // Clean up and return
        necro_snapshot_arena_rewind(&sa->snapshot_arena, snapshot);
        NecroType* new_type = necro_type_deep_copy(sa->arena, specialized_type);
        new_type->ownership = type->ownership;
        return new_type;
        // return specialized_type;
    }

    case NECRO_TYPE_FUN:
    {
        NecroType* type1 = necro_core_ast_type_specialize(sa, type->fun.type1);
        NecroType* type2 = necro_core_ast_type_specialize(sa, type->fun.type2);
        if (type1 == type->fun.type1 && type2 == type->fun.type2)
        {
            return type;
        }
        else
        {
            NecroType* new_type = necro_type_fn_create(sa->arena, type1, type2);
            new_type->kind      = type->kind;
            new_type->ownership = type->ownership;
            return new_type;
        }
    }

    case NECRO_TYPE_APP:
        return necro_core_ast_type_specialize(sa, necro_type_uncurry_app(sa->arena, sa->base, type));

    case NECRO_TYPE_LIST:
    {
        NecroType* item = necro_core_ast_type_specialize(sa, type->list.item);
        NecroType* next = necro_core_ast_type_specialize(sa, type->list.next);
        if (item == type->list.item && next == type->list.next)
        {
            return type;
        }
        else
        {
            NecroType* new_type = necro_type_list_create(sa->arena, item, next);
            new_type->kind      = type->kind;
            new_type->ownership = type->ownership;
            return new_type;
        }
    }

    // Ignore
    case NECRO_TYPE_VAR:
        return type;
    case NECRO_TYPE_FOR:
        return type;
    case NECRO_TYPE_NAT:
        return type;
    case NECRO_TYPE_SYM:
        return type;

    default:
        assert(false);
        return NULL;
    }
}


///////////////////////////////////////////////////////
// Deep copy
///////////////////////////////////////////////////////
// TODO: Deep copy individual variables with initializers for pattern assignment with recursion
// TODO: Handle Arrays
// TODO: Handle Ping-ponging
// TODO: Don't deep copy Unique/Owned types, only shared types
// NOTE: For arrays simply inline directly the array copying logic, that way we don't have to worry about how to "cache" the array copy functions"

NecroCoreAst* necro_core_ast_create_deep_copy_array(NecroStateAnalysis* context, NecroCoreAst* ast_to_deep_copy);
NecroCoreAst* necro_core_ast_create_deep_copy(NecroStateAnalysis* context, NecroCoreAst* ast);
NecroCoreAst* necro_core_ast_maybe_deep_copy(NecroStateAnalysis* context, NecroCoreAst* ast_to_deep_copy)
{
    assert(ast_to_deep_copy != NULL);
    NecroType* type = necro_type_find(ast_to_deep_copy->necro_type);
    assert(type->type == NECRO_TYPE_CON);
    if (type->con.con_symbol == context->base->array_type)
    {
        return necro_core_ast_create_deep_copy_array(context, ast_to_deep_copy);
    }
    NecroCoreAstSymbol* deep_copy_fn_symbol = type->con.con_symbol->core_ast_symbol->deep_copy_fn;
    if (deep_copy_fn_symbol == NULL)
    {
        NecroCoreAst* type_data_decl = type->con.con_symbol->core_ast_symbol->ast;
        NecroCoreAst* deep_copy_ast  = necro_core_ast_create_deep_copy(context, type_data_decl);
        if (deep_copy_ast != NULL)
        {
            deep_copy_fn_symbol                                 = deep_copy_ast->bind.ast_symbol;
            type->con.con_symbol->core_ast_symbol->deep_copy_fn = deep_copy_fn_symbol;
            necro_core_ast_add_new_top_let(context, deep_copy_ast);
        }
    }
    if (deep_copy_fn_symbol == NULL)
    {
        // Unboxed, primitive, or enum
        return ast_to_deep_copy;
    }
    else
    {
        // Normal type
        NecroCoreAst* deep_copy_fn_var = necro_core_ast_create_var(context->arena, deep_copy_fn_symbol, deep_copy_fn_symbol->type);
        return necro_core_ast_create_app(context->arena, deep_copy_fn_var, ast_to_deep_copy);
    }
}

NecroCoreAst* necro_core_ast_create_deep_copy_array(NecroStateAnalysis* context, NecroCoreAst* ast_to_deep_copy)
{
    UNUSED(context);
    // TODO: Finish!
    return ast_to_deep_copy;
/*
    assert(ast_to_deep_copy != NULL);
    NecroType* type    = necro_type_find(ast_to_deep_copy->necro_type);
    assert(type->type == NECRO_TYPE_CON);
    assert(type->con.con_symbol == context->base->array_type);
    assert(type->con.args != NULL);
    NecroType* array_n = type->con.args->list.item;
    size_t              max_loops  = necro_nat_to_size_t(context->base, array_n);
    NecroCoreAst*       zero_ast   = necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .uint_literal = 0, .type = NECRO_AST_CONSTANT_UNSIGNED_INTEGER });
    NecroCoreAst*       range_ast  = necro_core_ast_create_app(context->arena, necro_core_ast_create_app(context->arena, necro_core_ast_create_app(context->arena, necro_core_ast_create_var(context->arena, context->base->range_con->core_ast_symbol, necro_type_con1_create(context->arena, context->base->range_type, array_n)), zero_ast), zero_ast), zero_ast);
    NecroCoreAst*       emptyArray = necro_core_ast_create_app(context->arena, necro_core_ast_create_var(context->arena, context->base->unsafe_empty_array->core_ast_symbol, type), necro_core_ast_create_var(context->arena, context->base->unit_con->core_ast_symbol , context->base->unit_type->type));
    NecroType*          i_type     = context->base->uint_type->type;
    // NecroType*          i_type     = necro_type_con1_create(context->arena, context->base->index_type, array_n);
    NecroCoreAstSymbol* i_symbol   = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "i"), i_type);
    NecroCoreAstSymbol* a_symbol   = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "a"), type);
    NecroCoreAst*       index_pat  = necro_core_ast_create_var(context->arena, i_symbol, i_type);
    NecroCoreAst*       value_pat  = necro_core_ast_create_var(context->arena, a_symbol, type);
    NecroCoreAst*       read_ast   =
        necro_core_ast_create_app(context->arena,
            necro_core_ast_create_app(context->arena,
                necro_core_ast_create_var(context->arena, context->base->read_array->core_ast_symbol, necro_type_fresh_var(context->arena, NULL)),
                necro_core_ast_create_var(context->arena, i_symbol, i_type)),
            ast_to_deep_copy);
    read_ast->necro_type    = type->con.args->list.next->list.item;
    NecroCoreAst* deep_copy = necro_core_ast_maybe_deep_copy(context, read_ast);
    NecroCoreAst* write_ast =
        necro_core_ast_create_app(context->arena,
            necro_core_ast_create_app(context->arena,
                necro_core_ast_create_app(context->arena,
                    necro_core_ast_create_var(context->arena, context->base->write_array->core_ast_symbol, necro_type_fresh_var(context->arena, NULL)),
                    necro_core_ast_create_var(context->arena, i_symbol, context->base->unit_type->type)),
                deep_copy),
            necro_core_ast_create_var(context->arena, a_symbol, context->base->unit_type->type));
    NecroCoreAst* copy_loop = necro_core_ast_create_for_loop(context->arena, max_loops, range_ast, emptyArray, index_pat, value_pat, write_ast);
    return copy_loop;
*/
}

NecroCoreAst* necro_core_ast_create_deep_copy(NecroStateAnalysis* context, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_DATA_DECL);

    //--------------------
    // Ignore primitives and polymorphic types
    // if (ast->data_decl.ast_symbol->is_primitive || necro_type_is_polymorphic_ignoring_ownership(context->base, ast->data_decl.ast_symbol->type))
    if (ast->data_decl.ast_symbol->is_primitive)
        return NULL;

    // TODO: How to handle arrays!?!?!?!?!
    if (ast->data_decl.ast_symbol == context->base->array_type->core_ast_symbol)
        return necro_core_ast_create_deep_copy_array(context, ast);

    //--------------------
    // is_enum (accurate calculation. coming into this pass is_enum seems spotty. perhaps move this earlier on?)
    bool              is_enum   = true;
    NecroCoreAstList* data_cons = ast->data_decl.con_list;
    while (data_cons && is_enum)
    {
        NecroType* con_type  = necro_type_strip_for_all(necro_type_find(data_cons->data->data_con.type));
        if (necro_type_is_polymorphic_ignoring_ownership(context->base, con_type))
            return NULL;
        is_enum              = con_type->type == NECRO_TYPE_CON;
        data_cons            = data_cons->next;
    }
    ast->data_decl.ast_symbol->is_enum = is_enum;
    if (is_enum)
        return NULL;

    //--------------------
    // Alts
    NecroCoreAstList* alts      = NULL;
    data_cons                   = ast->data_decl.con_list;
    while (data_cons != NULL)
    {
        //--------------------
        // DataCon
        NecroType*    con_type  = necro_type_strip_for_all(necro_type_find(data_cons->data->data_con.type));
        NecroCoreAst* pat       = necro_core_ast_create_var(context->arena, data_cons->data->data_con.ast_symbol, con_type);
        NecroCoreAst* expr      = necro_core_ast_create_var(context->arena, data_cons->data->data_con.ast_symbol, con_type);
        NecroType*    con_args  = con_type;
        while (con_args->type == NECRO_TYPE_FUN)
        {
            //--------------------
            // Args
            NecroType*          con_arg_type        = necro_type_find(con_args->fun.type1);
            NecroSymbol         con_arg_name        = necro_intern_unique_string(context->intern, "p");
            NecroCoreAstSymbol* con_arg_pat_symbol  = necro_core_ast_symbol_create(context->arena, con_arg_name, con_arg_type);
            NecroCoreAst*       con_arg_pat_ast     = necro_core_ast_create_var(context->arena, con_arg_pat_symbol, con_arg_type);
            pat                                     = necro_core_ast_create_app(context->arena, pat, con_arg_pat_ast);
            NecroCoreAst*       con_arg_expr_ast    = necro_core_ast_create_var(context->arena, con_arg_pat_symbol, con_arg_type);
            NecroCoreAst*       deep_copied_con_arg = necro_core_ast_maybe_deep_copy(context, con_arg_expr_ast);
            expr                                    = necro_core_ast_create_app(context->arena, expr, deep_copied_con_arg);
            con_args                                = necro_type_find(con_args->fun.type2);
        }
        //--------------------
        // Alts
        NecroCoreAst* alt = necro_core_ast_create_case_alt(context->arena, pat, expr);
        alts              = necro_cons_core_ast_list(context->arena, alt, alts);
        data_cons         = data_cons->next;
    }

    //--------------------
    // Arg
    NecroSymbol         arg_name   = necro_intern_unique_string(context->intern, "x");
    NecroCoreAstSymbol* arg_symbol = necro_core_ast_symbol_create(context->arena, arg_name, ast->data_decl.ast_symbol->type);
    NecroCoreAst*       arg_ast    = necro_core_ast_create_var(context->arena, arg_symbol, ast->data_decl.ast_symbol->type);

    //--------------------
    // Case
    NecroCoreAst* case_ast = necro_core_ast_create_case(context->arena, arg_ast, alts);

    //--------------------
    // Lambda
    NecroCoreAst* lambda_ast = necro_core_ast_create_lam(context->arena, arg_ast, case_ast);

    //--------------------
    // Deep Copy fn
    NecroType*          deep_copy_type      = necro_type_fn_create(context->arena, ast->data_decl.ast_symbol->type, ast->data_decl.ast_symbol->type);
    unwrap(NecroType, necro_kind_infer(context->arena, context->base, deep_copy_type, zero_loc, zero_loc));
    NecroSymbol         deep_copy_name      = necro_intern_unique_string(context->intern, "deepCopy");
    NecroCoreAstSymbol* deep_copy_symbol    = necro_core_ast_symbol_create(context->arena, deep_copy_name, deep_copy_type);
    NecroCoreAst*       deep_copy_ast       = necro_core_ast_create_bind(context->arena, deep_copy_symbol, lambda_ast, NULL);
    deep_copy_symbol->state_type            = NECRO_STATE_POINTWISE;
    deep_copy_ast->necro_type               = deep_copy_type;
    deep_copy_symbol->ast                   = deep_copy_ast;
    deep_copy_symbol->type                  = deep_copy_type;
    deep_copy_symbol->is_deep_copy_fn       = true;
    ast->data_decl.ast_symbol->deep_copy_fn = deep_copy_symbol;
    return deep_copy_ast;
}


///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_state_analysis_test_string(const char* test_name, const char* str)
{

    //--------------------
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCoreAstArena   core_ast        = necro_core_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();
    info.verbosity                      = 1;
    info.compilation_phase              = NECRO_PHASE_STATE_ANALYSIS;

    //--------------------
    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast));
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));
    necro_core_ast_pre_simplify(info, &intern, &base, &core_ast);
    necro_core_lambda_lift(info, &intern, &base, &core_ast);
    necro_core_defunctionalize(info, &intern, &base, &core_ast);
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));
    necro_core_state_analysis(info, &intern, &base, &core_ast);

    //--------------------
    // Print
    printf("State Analysis %s test: Passed\n", test_name);

    //--------------------
    // Clean up
    necro_core_ast_arena_destroy(&core_ast);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}


void necro_state_analysis_test()
{
    necro_announce_phase("State Analysis");

/*

    {
        const char* test_name   = "Rec 1";
        const char* test_source = ""
            "counter :: Int\n"
            "counter = let x ~ 0 = add x 1 in x\n"
            "main :: *World -> *World\n"
            "main w = printInt counter w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 2";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing = x\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1) =\n"
            "    case x of\n"
            "      (l, r) -> (r, l)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3.1";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter = x where\n"
            "  x ~ Nothing =\n"
            "    case x of\n"
            "      Nothing -> Just 0\n"
            "      Just 0  -> Nothing\n"
            "      Just i  -> Just (add i 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3.2";
        const char* test_source = ""
            "counter :: Maybe (Maybe Float, Int)\n"
            "counter = x where\n"
            "  x ~ Nothing =\n"
            "    case x of\n"
            "      Nothing     -> Just (Nothing, 0)\n"
            "      Just (_, 0) -> Nothing\n"
            "      Just (f, i) -> Just (f, add i 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1.5";
        const char* test_source = ""
            "counter :: Int\n"
            "counter =\n"
            "  case (gt mouseX 50) of\n"
            "    True  -> let x ~ 0 = (add x 1) in x\n"
            "    False -> let y ~ 0 = (sub y 1) in y\n"
            "main :: *World -> *World\n"
            "main w = printInt counter w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 1.6";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter =\n"
            "  case (gt mouseX 50) of\n"
            "    True  -> let x ~ 0 = add x 1 in (x, x)\n"
            "    False -> let y ~ 0 = sub y 1 in (y, y)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 4";
        const char* test_source = ""
            "counter :: Int\n"
            "counter = add x y where\n"
            "  x ~ -10 = add x 1\n"
            "  y ~ 666 = add y 1\n"
            "main :: *World -> *World\n"
            "main w = printInt counter w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 5";
        const char* test_source = ""
            "counter :: (Int, Int, Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1, 2, 3) = x\n"
            "  y ~ (3, 2, 1, 0) = y\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 5.5";
        const char* test_source = ""
            "counter :: (Int, Int)\n"
            "counter = x where\n"
            "  x ~ (0, 1) =\n"
            "    case x of\n"
            "      (xl, xr) -> (xr, xl)\n"
            "  y ~ (2, 3) =\n"
            "    case y of\n"
            "      (yl, yr) -> (yr, yl)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 6";
        const char* test_source = ""
            "counter :: Int\n"
            "counter ~ 0 = add counter 1\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 7";
        const char* test_source = ""
            "counter :: Maybe Int\n"
            "counter ~ Nothing = \n"
            "  case counter of\n"
            "    Nothing -> Just 0\n"
            "    Just i  -> Just (add i 1)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Double Up";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Type Function Parameter";
        const char* test_source = ""
            "dropIt :: (#Int, Bool, Float#) -> Int\n"
            "dropIt x = 0\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Emptiness";
        const char* test_source = ""
            "nothingInThere :: *Array 4 (Share Int)\n"
            "nothingInThere = unsafeEmptyArray ()\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Mono?";
        const char* test_source = ""
            "ambigAudio = saw 440\n"
            "main :: *World -> *World\n"
            "main w = outAudio 0 ambigAudio w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 1";
        const char* test_source = ""
            "seqTest1 :: Seq Bool\n"
            "seqTest1 = pure True\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 2";
        const char* test_source = ""
            "seqTest1 :: Seq Bool\n"
            "seqTest1 = pure True\n"
            "seqGo :: SeqValue Bool\n"
            "seqGo = runSeq seqTest1 0\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 3";
        const char* test_source = ""
            "seqTest :: Seq Int\n"
            "seqTest = pure 0\n"
            "coolSeq :: Seq Int\n"
            "coolSeq = map (add 666) seqTest\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq seqTest 0\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "HOF Fun 1";
        const char* test_source = ""
            "doubleUpInt :: (Int -> Int) -> Int -> Int\n"
            "doubleUpInt f x = f x + f x\n"
            "doubleDownInt :: (Int -> Int -> Int) -> Int -> Int\n"
            "doubleDownInt f x = f x x + f x x\n"
            "integrity :: Int -> Int\n"
            "integrity i = doubleUpInt (doubleDownInt add) i\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

    {
        const char* test_name   = "HOF Fun 2";
        const char* test_source = ""
            "doubleUpInt :: (Int -> Int) -> Int -> Int\n"
            "doubleUpInt f x = f x + f x\n"
            "doubleDownInt :: (Int -> Int) -> Int -> Int\n"
            "doubleDownInt f x = f x + f x\n"
            "integrity :: Int -> Int\n"
            "integrity i = doubleUpInt (doubleDownInt (add mouseX)) i\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

*/

    {
        const char* test_name   = "Seq 7";
        const char* test_source = ""
            "coolSeq :: Seq Int\n"
            "coolSeq = 666 * 22 + 3 * 4 - 256 * 10\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq coolSeq 0\n";
        necro_state_analysis_test_string(test_name, test_source);
    }

/*

*/

}


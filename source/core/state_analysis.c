/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "state_analysis.h"
#include <stdio.h>
#include "type.h"
#include "kind.h"
#include "core_infer.h"

/*
///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
typedef struct NecroStateAnalysis
{
    NecroPagedArena       arena;
    NecroIntern*          intern;
    NecroSymTable*        symtable;
    NecroScopedSymTable*  scoped_symtable;
    NecroPrimTypes*       prim_types;
    NecroVar              closure_con;
    NecroVar              null_con;
    NecroVar              dyn_state_con;
} NecroStateAnalysis;

typedef struct NecroOuter
{
    struct NecroOuter* outer;
    NecroSymbolInfo*  info;
} NecroOuter;

NecroOuter* necro_create_state_outer(NecroPagedArena* arena, NecroSymbolInfo* info, NecroOuter* a_outer)
{
    NecroOuter* outer = necro_paged_arena_alloc(arena, sizeof(NecroOuter));
    outer->outer      = a_outer;
    outer->info       = info;
    return outer;
}

void necro_set_outer_rec_stateful(NecroOuter* outer)
{
    if (outer->outer == NULL)
    {
        // Top level bindings store state into global variables, thus are actually pointwise
        outer->info->state_type = NECRO_STATE_POINTWISE;
        return;
    }
    while (outer != NULL)
    {
        outer->info->state_type = NECRO_STATE_STATEFUL;
        outer = outer->outer;
    }
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_go(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer);

///////////////////////////////////////////////////////
// State Analysis
///////////////////////////////////////////////////////
// void necro_state_analysis(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types)
// {
//     NecroStateAnalysis sa = (NecroStateAnalysis)
//     {
//         .arena           = necro_paged_arena_create(),
//         .intern          = intern,
//         .symtable        = symtable,
//         .scoped_symtable = scoped_symtable,
//         .prim_types      = prim_types,
//         .closure_con     = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_Closure"))),
//         .null_con        = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_NullPoly"))),
//         .dyn_state_con   = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_DynState"))),
//     };
//     // Primitively stateful functions, etc
//     necro_symtable_get(symtable, prim_types->apply_fn.id)->state_type   = NECRO_STATE_STATEFUL;
//     necro_symtable_get(symtable, prim_types->mouse_x_fn.id)->state_type = NECRO_STATE_POINTWISE;
//     necro_symtable_get(symtable, prim_types->mouse_y_fn.id)->state_type = NECRO_STATE_POINTWISE;
//     // Go
//     necro_state_analysis_go(&sa, in_ast->root, NULL);
//     // Cleanup
//     necro_paged_arena_destroy(&sa.arena);
// }

NECRO_STATE_TYPE necro_merge_state_types(NECRO_STATE_TYPE state_type1, NECRO_STATE_TYPE state_type2)
{
    if (state_type1 > state_type2)
        return state_type1;
    else
        return state_type2;
}

NECRO_STATE_TYPE necro_state_analysis_list(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LIST);
    NECRO_STATE_TYPE state_type = NECRO_STATE_CONSTANT;
    while (ast != NULL)
    {
        if (ast->list.expr == NULL)
        {
            ast = ast->list.next;
            continue;
        }
        state_type = necro_merge_state_types(state_type, necro_state_analysis_go(sa, ast->list.expr, outer));
        ast = ast->list.next;
    }
    return state_type;
}

NECRO_STATE_TYPE necro_state_analysis_bind(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    NecroSymbolInfo* info       = necro_symtable_get(sa->symtable, ast->bind.var.id);
    outer                       = necro_create_state_outer(&sa->arena, info, outer);
    NECRO_STATE_TYPE state_type = necro_state_analysis_go(sa, ast->bind.expr, outer);
    if (info->is_recursive)
        necro_set_outer_rec_stateful(outer);
    info->state_type = necro_merge_state_types(info->state_type, state_type);
    return info->state_type;
}

NECRO_STATE_TYPE necro_state_analysis_let(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LET);
    necro_state_analysis_bind(sa, ast->let.bind, outer);
    return necro_state_analysis_go(sa, ast->let.expr, outer);
}

NECRO_STATE_TYPE necro_state_analysis_lam(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LAM);
    necro_symtable_get(sa->symtable, ast->lambda.arg->var.id)->state_type = NECRO_STATE_POLY;
    return necro_state_analysis_go(sa, ast->lambda.expr, outer);
}

NECRO_STATE_TYPE necro_state_analysis_case(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);

    // Expr
    NECRO_STATE_TYPE state_type = necro_state_analysis_go(sa, ast->case_expr.expr, outer);

    // Alts
    NecroCoreAST_CaseAlt* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        state_type = necro_merge_state_types(state_type, necro_state_analysis_go(sa, alts->expr, outer));
        alts = alts->next;
    }
    return state_type;
}

NECRO_STATE_TYPE necro_state_analysis_var(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    UNUSED(outer);
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    NecroSymbolInfo* info = necro_symtable_get(sa->symtable, ast->var.id);
    if (info->is_constructor)
        info->state_type = NECRO_STATE_POLY;
    if (info->state_type == NECRO_STATE_STATEFUL && info->arity == 0)
        return NECRO_STATE_POINTWISE;
    return info->state_type;
}

NECRO_STATE_TYPE necro_state_analysis_app(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);

    NecroCoreAST_Expression* app       = ast;
    size_t                   arg_count = 0;
    while (app->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        app = app->app.exprA;
    }
    if (app->expr_type == NECRO_CORE_EXPR_VAR && app->var.id.id == sa->closure_con.id.id)
    {
        // Closure application
        NECRO_STATE_TYPE args_state_type = NECRO_STATE_CONSTANT;
        app                              = ast;
        size_t arg_count_2               = 0;
        while (app->expr_type == NECRO_CORE_EXPR_APP && arg_count_2 < arg_count - 4) // Ignore first 4 arguments
        {
            args_state_type = necro_merge_state_types(args_state_type, necro_state_analysis_go(sa, app->app.exprB, outer));
            arg_count_2++;
            app = app->app.exprA;
        }
        // Analyze state application
        necro_state_analysis_app(sa, app->app.exprA->app.exprB, outer);
        return args_state_type;
    }
    else if (app->expr_type == NECRO_CORE_EXPR_VAR && app->var.id.id == sa->dyn_state_con.id.id)
    {
        // _State application
        NECRO_STATE_TYPE args_state_type = NECRO_STATE_CONSTANT;
        UNUSED(args_state_type);
        app                              = ast;
        NECRO_STATE_TYPE fn_state_type   = necro_symtable_get(sa->symtable, app->app.exprA->app.exprB->var.id)->state_type;
        if (fn_state_type != NECRO_STATE_STATEFUL)
        {
            *ast = *necro_create_core_var(&sa->arena, sa->null_con);
        }
        return NECRO_STATE_CONSTANT;
    }
    else
    {
        // Normal Application
        NECRO_STATE_TYPE args_state_type = NECRO_STATE_CONSTANT;
        app                              = ast;
        while (app->expr_type == NECRO_CORE_EXPR_APP)
        {
            args_state_type = necro_merge_state_types(args_state_type, necro_state_analysis_go(sa, app->app.exprB, outer));
            app = app->app.exprA;
        }
        NECRO_STATE_TYPE fn_state_type = necro_state_analysis_go(sa, app, outer);
        if (fn_state_type == NECRO_STATE_POLY)
            return args_state_type;
        else
            return necro_merge_state_types(fn_state_type, args_state_type);
    }
}

///////////////////////////////////////////////////////
// State Analysis Go
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_go(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroOuter* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    // if (ast == NULL)
    //     return NECRO_STATE_CONSTANT;
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:       return necro_state_analysis_var(sa, ast, outer);
    case NECRO_CORE_EXPR_APP:       return necro_state_analysis_app(sa, ast, outer);
    case NECRO_CORE_EXPR_LAM:       return necro_state_analysis_lam(sa, ast, outer);
    case NECRO_CORE_EXPR_LET:       return necro_state_analysis_let(sa, ast, outer);
    case NECRO_CORE_EXPR_BIND:      return necro_state_analysis_bind(sa, ast, outer);
    case NECRO_CORE_EXPR_CASE:      return necro_state_analysis_case(sa, ast, outer);
    case NECRO_CORE_EXPR_LIST:      return necro_state_analysis_list(sa, ast, outer);
    case NECRO_CORE_EXPR_LIT:       return NECRO_STATE_CONSTANT;
    case NECRO_CORE_EXPR_DATA_DECL: return NECRO_STATE_CONSTANT;
    case NECRO_CORE_EXPR_DATA_CON:  return NECRO_STATE_CONSTANT;
    case NECRO_CORE_EXPR_TYPE:      return NECRO_STATE_CONSTANT;
    default:                        assert(false && "Unimplemented AST type in necro_closure_conversion_go"); return NECRO_STATE_CONSTANT;
    }
}
*/

///////////////////////////////////////////////////////
// New API
///////////////////////////////////////////////////////

typedef struct NecroCoreAstSymbolBucket
{
    size_t              hash;
    NecroCoreAstSymbol* core_symbol;
    NecroType*          type;
    NecroCoreAstSymbol* specialized_core_symbol;
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
void                    necro_core_ast_create_deep_copy_fns(NecroStateAnalysis* context, NecroCoreAst* top);

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
    // TODO: Put back!
    base->mouse_x_fn->core_ast_symbol->state_type = NECRO_STATE_POINTWISE;
    base->mouse_y_fn->core_ast_symbol->state_type = NECRO_STATE_POINTWISE;

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

    // Create deep_copy fns
    necro_core_ast_create_deep_copy_fns(&sa, core_ast_arena->root);

    // Finish
    unwrap(void, necro_core_infer(intern, base, core_ast_arena)); // TODO: Remove after some testing
    // necro_core_ast_pretty_print(core_ast_arena->root);

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
    }
    symbol->state_type = necro_state_analysis_merge_state_types(symbol->state_type, necro_state_analysis_go(sa, ast->bind.expr, symbol));
    // if (symbol->is_recursive)
    //     necro_set_outer_rec_stateful(symbol);
    // symbol->state_type = necro_state_analysis_merge_state_types(symbol->state_type, state_type);
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

NECRO_STATE_TYPE necro_state_analysis_for(NecroStateAnalysis* sa, NecroCoreAst* ast, NecroCoreAstSymbol* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_FOR);
    ast->necro_type             = necro_core_ast_type_specialize(sa, ast->necro_type);
    NECRO_STATE_TYPE state_type = necro_state_analysis_go(sa, ast->for_loop.range_init, outer);
    state_type                  = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->for_loop.value_init, outer));
    state_type                  = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_pat_go(sa, ast->for_loop.index_arg, outer));
    state_type                  = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_pat_go(sa, ast->for_loop.value_arg, outer));
    state_type                  = necro_state_analysis_merge_state_types(state_type, necro_state_analysis_go(sa, ast->for_loop.expression, outer));
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
    case NECRO_CORE_AST_FOR:       return necro_state_analysis_for(sa, ast, outer);
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
        symtable.buckets[i] = (NecroCoreAstSymbolBucket){ .hash = 0, .core_symbol = NULL, .type = NULL, .specialized_core_symbol = NULL };
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
        symtable->buckets[i] = (NecroCoreAstSymbolBucket){ .hash = 0, .core_symbol = NULL, .type = NULL, .specialized_core_symbol = NULL };
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
            return new_type;
        }
        else if (type->con.con_symbol == sa->base->index_type)
        {
            return sa->base->uint_type->type;
        }
        // else if (type->con.con_symbol == sa->base->range_type)
        // {
        //     return type;
        // }

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
        return NULL; // TODO: Finish!

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
            assert(con_arg_type->type == NECRO_TYPE_CON);
            NecroCoreAstSymbol* con_arg_symbol      = con_arg_type->con.con_symbol->core_ast_symbol;
            NecroSymbol         con_arg_name        = necro_intern_unique_string(context->intern, "p");
            NecroCoreAstSymbol* con_arg_pat_symbol  = necro_core_ast_symbol_create(context->arena, con_arg_name, con_arg_type);
            NecroCoreAst*       con_arg_pat_ast     = necro_core_ast_create_var(context->arena, con_arg_pat_symbol, con_arg_type);
            pat                                     = necro_core_ast_create_app(context->arena, pat, con_arg_pat_ast);
            if (con_arg_symbol->is_primitive || con_arg_symbol->is_enum)
            {
                expr = necro_core_ast_create_app(context->arena, expr, con_arg_pat_ast);
            }
            else
            {
                // assert(con_arg_symbol->deep_copy_fn != NULL);
                if (con_arg_symbol->deep_copy_fn == NULL)
                    return NULL; // NOTE: This is likely NULL because we're skipping out on implementing array deep copying for now. Replace with assert when we've finish that.
                NecroCoreAst* con_arg_copy_fn_var = necro_core_ast_create_var(context->arena, con_arg_symbol->deep_copy_fn, con_arg_symbol->deep_copy_fn->type);
                NecroCoreAst* copy_con_arg        = necro_core_ast_create_app(context->arena, con_arg_copy_fn_var, con_arg_pat_ast);
                expr                              = necro_core_ast_create_app(context->arena, expr, copy_con_arg);
            }
            con_args = necro_type_find(con_args->fun.type2);
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
    deep_copy_ast->necro_type               = deep_copy_type;
    deep_copy_symbol->ast                   = deep_copy_ast;
    deep_copy_symbol->type                  = deep_copy_type;
    ast->data_decl.ast_symbol->deep_copy_fn = deep_copy_symbol;
    return deep_copy_ast;
}

void necro_core_ast_create_deep_copy_fns(NecroStateAnalysis* context, NecroCoreAst* top)
{
    assert(top != NULL);
    assert(top->ast_type == NECRO_CORE_AST_LET);
    NecroCoreAst* copy_head = NULL;
    NecroCoreAst* copy_tail = NULL;
    NecroCoreAst* curr      = top;
    while (curr->let.expr != NULL && curr->let.expr->ast_type == NECRO_CORE_AST_LET && curr->let.expr->let.bind->ast_type == NECRO_CORE_AST_DATA_DECL)
    {
        NecroCoreAst* deep_copy_fn = necro_core_ast_create_deep_copy(context, curr->let.expr->let.bind);
        if (deep_copy_fn != NULL)
        {
            if (copy_head == NULL)
            {
                copy_head = necro_core_ast_create_let(context->arena, deep_copy_fn, NULL);
                copy_tail = copy_head;
            }
            else
            {
                copy_tail->let.expr = necro_core_ast_create_let(context->arena, deep_copy_fn, NULL);
                copy_tail           = copy_tail->let.expr;
            }
        }
        curr = curr->let.expr;
    }
    if (copy_tail != NULL)
    {
        copy_tail->let.expr = curr->let.expr;
        curr->let.expr      = copy_head;
    }
}


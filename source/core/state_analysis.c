/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "closure_conversion.h"
#include <stdio.h>
#include "type.h"
#include "prim.h"
#include "core_create.h"
#include "machine/machine_type.h"

/*
    Closure layout: { farity, num_pargs, fn_ptr, pargs... }
    * farity:    function arity
    * num_pargs: number of partially applied args
    * fn_ptr:    function pointer

    (implied)
    * carity:    closure arity
*/

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
    NecroInfer*           infer;
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
void necro_state_analysis(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer)
{
    NecroStateAnalysis sa = (NecroStateAnalysis)
    {
        .arena           = necro_paged_arena_create(),
        .intern          = intern,
        .symtable        = symtable,
        .scoped_symtable = scoped_symtable,
        .prim_types      = prim_types,
        .infer           = infer,
        .closure_con     = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_Closure"))),
        .null_con        = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_NullPoly"))),
        .dyn_state_con   = necro_con_to_var(necro_prim_types_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_DynState"))),
    };
    // Primitively stateful functions, etc
    necro_symtable_get(symtable, prim_types->apply_fn.id)->state_type   = NECRO_STATE_STATEFUL;
    necro_symtable_get(symtable, prim_types->mouse_x_fn.id)->state_type = NECRO_STATE_POINTWISE;
    necro_symtable_get(symtable, prim_types->mouse_y_fn.id)->state_type = NECRO_STATE_POINTWISE;
    // Go
    necro_state_analysis_go(&sa, in_ast->root, NULL);
    // Cleanup
    necro_paged_arena_destroy(&sa.arena);
}

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

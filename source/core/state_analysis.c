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
    // NecroPagedArena       arena;
    // NecroSnapshotArena    snapshot_arena;
    NecroIntern*          intern;
    NecroSymTable*        symtable;
    NecroScopedSymTable*  scoped_symtable;
    NecroPrimTypes*       prim_types;
    NecroInfer*           infer;
} NecroStateAnalysis;

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_go(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer);

///////////////////////////////////////////////////////
// State Analysis
///////////////////////////////////////////////////////
void necro_state_analysis(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer)
{
    NecroStateAnalysis sa = (NecroStateAnalysis)
    {
        // .arena            = necro_create_paged_arena(),
        // .snapshot_arena   = necro_create_snapshot_arena(0),
        .intern           = intern,
        .symtable         = symtable,
        .scoped_symtable  = scoped_symtable,
        .prim_types       = prim_types,
        .infer            = infer,
    };
    // Primitively stateful functions, etc
    necro_symtable_get(symtable, prim_types->apply_fn.id)->state_type   = NECRO_STATE_STATEFUL;
    necro_symtable_get(symtable, prim_types->mouse_x_fn.id)->state_type = NECRO_STATE_POINTWISE;
    necro_symtable_get(symtable, prim_types->mouse_y_fn.id)->state_type = NECRO_STATE_POINTWISE;
    // Go
    necro_state_analysis_go(&sa, in_ast->root, NULL);
    // Cleanup
    // necro_destroy_snapshot_arena(&sa.snapshot_arena);
}

NECRO_STATE_TYPE necro_merge_state_types(NECRO_STATE_TYPE state_type1, NECRO_STATE_TYPE state_type2)
{
    if (state_type1 > state_type2)
        return state_type1;
    else
        return state_type2;
}

NECRO_STATE_TYPE necro_state_analysis_list(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
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

// TODO: Do we need an outer?
NECRO_STATE_TYPE necro_state_analysis_bind(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer == NULL)
        outer = &ast->bind.var;
    NECRO_STATE_TYPE state_type = necro_state_analysis_go(sa, ast->bind.expr, outer);
    NecroSymbolInfo* info       = necro_symtable_get(sa->symtable, ast->bind.var.id);
    if (info->is_recursive)
        state_type = NECRO_STATE_STATEFUL;
    info->state_type = state_type;
    return state_type;
}

NECRO_STATE_TYPE necro_state_analysis_let(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer == NULL)
        outer = &ast->let.bind->var;
    necro_state_analysis_bind(sa, ast->let.bind, outer);
    return necro_state_analysis_go(sa, ast->let.expr, outer);
}

NECRO_STATE_TYPE necro_state_analysis_lam(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LAM);
    // TODO: WTF is this about?
    // necro_state_analysis_bind(sa, ast->let.bind, outer);
    necro_symtable_get(sa->symtable, ast->lambda.arg->var.id)->state_type = NECRO_STATE_POINTWISE;
    return necro_state_analysis_go(sa, ast->lambda.expr, outer);
}

NECRO_STATE_TYPE necro_state_analysis_case(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
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
        // TODO: Set alt state types!
        state_type = necro_merge_state_types(state_type, necro_state_analysis_go(sa, alts->expr, outer));
        alts = alts->next;
    }
    return state_type;
}

NECRO_STATE_TYPE necro_state_analysis_var(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    NecroSymbolInfo* info = necro_symtable_get(sa->symtable, ast->var.id);
    if (info->is_constructor)
        info->state_type = NECRO_STATE_CONSTANT;
    // else if (info->is_recursive)
    //     info->state_type = NECRO_STATE_STATEFUL;
    // TODO: Fix this! Set outer to stateful!
    // else if (info->state_type == NECRO_STATE_STATEFUL && ast->var.id.id != outer->id.id)
    //     return NECRO_STATE_POINTWISE;
    // else if (info->core_ast == NULL)
    // TODO: Probably need more work in here
    return info->state_type;
}

NECRO_STATE_TYPE necro_state_analysis_app(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    NECRO_STATE_TYPE         args_state_type = NECRO_STATE_CONSTANT;
    NecroCoreAST_Expression* app = ast;
    while (app->expr_type == NECRO_CORE_EXPR_APP)
    {
        args_state_type = necro_merge_state_types(args_state_type, necro_state_analysis_go(sa, app->app.exprB, outer));
        app = app->app.exprA;
    }
    NECRO_STATE_TYPE fn_state_type = necro_state_analysis_go(sa, app, outer);
    // if (fn_state_type == NECRO_STATE_STATEFUL)
    //     return fn_state_type;
    // else if (fn_state_type == NECRO_STATE_CONSTANT && args_state_type == NECRO_STATE_CONSTANT)
    //     return NECRO_STATE_CONSTANT;
    // else if ((fn_state_type == NECRO_STATE_CONSTANT || fn_state_type == NECRO_STATE_POINTWISE) && args_state_type == NECRO_STATE_STATEFUL)
    //     return NECRO_STATE_POINTWISE;
    // else
    return necro_merge_state_types(fn_state_type, args_state_type);
}

///////////////////////////////////////////////////////
// State Analysis Go
///////////////////////////////////////////////////////
NECRO_STATE_TYPE necro_state_analysis_go(NecroStateAnalysis* sa, NecroCoreAST_Expression* ast, NecroVar* outer)
{
    assert(sa != NULL);
    assert(ast != NULL);
    // if (ast == NULL)
    //     return NECRO_STATE_CONSTANT;
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:       return necro_state_analysis_var(sa, ast, NULL);
    case NECRO_CORE_EXPR_APP:       return necro_state_analysis_app(sa, ast, NULL);
    case NECRO_CORE_EXPR_LAM:       return necro_state_analysis_lam(sa, ast, NULL);
    case NECRO_CORE_EXPR_LET:       return necro_state_analysis_let(sa, ast, NULL);
    case NECRO_CORE_EXPR_BIND:      return necro_state_analysis_bind(sa, ast, NULL);
    case NECRO_CORE_EXPR_CASE:      return necro_state_analysis_case(sa, ast, NULL);
    case NECRO_CORE_EXPR_LIST:      return necro_state_analysis_list(sa, ast, NULL);
    case NECRO_CORE_EXPR_LIT:       return NECRO_STATE_CONSTANT;
    case NECRO_CORE_EXPR_DATA_DECL: return NECRO_STATE_CONSTANT;
    case NECRO_CORE_EXPR_DATA_CON:  return NECRO_STATE_CONSTANT;
    case NECRO_CORE_EXPR_TYPE:      return NECRO_STATE_CONSTANT;
    default:                        assert(false && "Unimplemented AST type in necro_closure_conversion_go"); return NECRO_STATE_CONSTANT;
    }
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "lambda_lift.h"
#include <stdio.h>
#include "type.h"
#include "prim.h"
#include "core_create.h"

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
NECRO_DECLARE_ARENA_LIST(NecroVar, Var, var);

typedef struct NecroLambdaLiftSymbolInfo
{
    NecroVar                          var;
    NecroVarList*                     free_vars;
    struct NecroLambdaLiftSymbolInfo* parent;
    NecroVar                          renamed_var;
} NecroLambdaLiftSymbolInfo;
NECRO_DECLARE_VECTOR(NecroLambdaLiftSymbolInfo, NecroLambdaLiftSymbol, lambda_lift_symbol);
NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroVar, Var, var);

typedef struct NecroLambdaLift
{
    NecroPagedArena             arena;
    NecroSnapshotArena          snapshot_arena;
    NecroLambdaLiftSymbolVector ll_symtable;
    NecroIntern*                intern;
    NecroSymTable*              symtable;
    NecroScopedSymTable*        scoped_symtable;
    NecroPrimTypes*             prim_types;
    NecroInfer*                 infer;
    NecroCoreAST_Expression*    lift_point;
    size_t                      num_anon_functions;
    NecroVarTable               lifted_env;
} NecroLambdaLift;

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_lambda_lift_go(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer);
NecroCoreAST_Expression* necro_lambda_lift_pat_go(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer);

///////////////////////////////////////////////////////
// Lambda Lifting
///////////////////////////////////////////////////////
NecroCoreAST necro_lambda_lift(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer)
{
    // Init
    NecroLambdaLift ll = (NecroLambdaLift)
    {
        .arena              = necro_create_paged_arena(),
        .snapshot_arena     = necro_create_snapshot_arena(),
        .ll_symtable        = necro_create_lambda_lift_symbol_vector(),
        .intern             = intern,
        .symtable           = symtable,
        .scoped_symtable    = scoped_symtable,
        .prim_types         = prim_types,
        .infer              = infer,
        .lift_point         = NULL,
        .num_anon_functions = 0,
        .lifted_env         = necro_create_var_table(),
    };
    // Lambda Lift
    NecroCoreAST_Expression* out_ast = necro_lambda_lift_go(&ll, in_ast->root, NULL, NULL);
    // Clean up
    necro_destroy_snapshot_arena(&ll.snapshot_arena);
    necro_destroy_lambda_lift_symbol_vector(&ll.ll_symtable);
    necro_destroy_var_table(&ll.lifted_env);
    // Return
    return (NecroCoreAST) { .arena = ll.arena, .root = out_ast };
}

bool necro_bind_is_fn(NecroCoreAST_Expression* bind_ast)
{
    assert(bind_ast->expr_type == NECRO_CORE_EXPR_BIND);
    NecroCoreAST_Expression* bind_expr = bind_ast->bind.expr;
    return bind_expr->expr_type == NECRO_CORE_EXPR_LAM;
}

bool necro_is_in_env(NecroVarList* env, NecroVar var)
{
    // while (env != NULL && env->data.id.id != var.id.id)
    while (env != NULL && env->data.symbol.id != var.symbol.id)
    {
        env = env->next;
    }
    return env != NULL;
}

bool necro_is_free_var(NecroLambdaLift* ll, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer, NecroVar var)
{
    if (necro_is_in_env(env, var))
        return false;
    NecroVar* lifted_env_result = necro_var_table_get(&ll->lifted_env, var.id.id);
    if (lifted_env_result != NULL && lifted_env_result->id.id != 0)
        return false;
    if (outer != NULL && necro_is_in_env(outer->free_vars, var))
        return false;
    NecroID found_id = necro_scope_find(ll->scoped_symtable->top_scope, var.symbol);
    return found_id.id != var.id.id;
}

NecroVar necro_find_in_env(NecroVarList* env, NecroVar var)
{
    // while (env != NULL && env->data.id.id != var.id.id)
    while (env != NULL)
    {
        if (env->data.symbol.id == var.symbol.id)
            return env->data;
        env = env->next;
    }
    return (NecroVar) { 0, 0 };
}

NecroVar necro_find_free_var(NecroLambdaLift*ll, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer, NecroVar var)
{
    NecroVar result = necro_find_in_env(env, var);
    if (result.id.id != 0)
        return result;
    NecroVar* lifted_env_result = necro_var_table_get(&ll->lifted_env, var.id.id);
    if (lifted_env_result != NULL && lifted_env_result->id.id == var.id.id)
        return *lifted_env_result;
    result = (outer != NULL) ? necro_find_in_env(outer->free_vars, var) : (NecroVar) { 0, 0 };
    if (result.id.id != 0)
        return result;
    NecroID found_id = necro_scope_find(ll->scoped_symtable->top_scope, var.symbol);
    return (NecroVar) { .id = found_id, .symbol = var.symbol };
}

NecroVar necro_copy_var(NecroLambdaLift* ll, NecroVar old_var)
{
    // char itoabuf[10];
    NecroSymbolInfo*   old_info = necro_symtable_get(ll->symtable, old_var.id);
    NecroVar           new_var  = (NecroVar) { .id = necro_symtable_manual_new_symbol(ll->symtable, old_var.symbol), .symbol = old_var.symbol };
    NecroSymbolInfo*   new_info = necro_symtable_get(ll->symtable, new_var.id);
    new_info->arity                   = 0; // old_info->arity; // should arity always be 0?
    new_info->name                    = old_info->name;
    // new_info->string_name             = necro_intern_get_string(intern, symbol),
    new_info->id                      = new_var.id;
    new_info->con_num                 = old_info->con_num;
    new_info->is_enum                 = old_info->is_enum;
    new_info->source_loc              = old_info->source_loc;
    new_info->scope                   = NULL;
    new_info->ast                     = NULL;
    new_info->core_ast                = NULL;
    new_info->declaration_group       = NULL;
    new_info->optional_type_signature = NULL;
    new_info->type                    = old_info->type;
    new_info->closure_type            = old_info->closure_type;
    new_info->type_status             = old_info->type_status;
    new_info->method_type_class       = NULL;
    new_info->type_class              = NULL;
    new_info->type_class_instance     = NULL;
    new_info->persistent_slot         = old_info->persistent_slot;
    new_info->is_constructor          = old_info->is_constructor;
    new_info->is_recursive            = old_info->is_recursive;
    new_info->arity                   = old_info->arity;
    new_info->necro_machine_ast       = NULL;
    new_info->state_type              = NECRO_STATE_CONSTANT;
    return new_var;
}

NecroLambdaLiftSymbolInfo* necro_lambda_lift_symtable_get(NecroLambdaLift* ll, NecroVar var)
{
    assert(ll != NULL);
    assert(var.id.id != 0);
    while (var.id.id >= ll->ll_symtable.length)
    {
        NecroLambdaLiftSymbolInfo info = { var, NULL, NULL, (NecroVar) { 0, 0 } };
        necro_push_lambda_lift_symbol_vector(&ll->ll_symtable, &info);
    }
    return ll->ll_symtable.data + var.id.id;
}

NecroCoreAST_CaseAlt* necro_lambda_lift_case_alt(NecroLambdaLift* ll, NecroCoreAST_CaseAlt* alts, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    if (alts == NULL)
        return NULL;

    // Push Env
    NecroVarList*            new_env = env;
    NecroCoreAST_Expression* alt_con = necro_lambda_lift_pat_go(ll, alts->altCon, &new_env, outer);
    NecroCoreAST_Expression* expr    = necro_lambda_lift_go(ll, alts->expr, new_env, outer);
    // Pop Env

    NecroCoreAST_CaseAlt*    next    = necro_lambda_lift_case_alt(ll, alts->next, env, outer);
    return necro_create_core_case_alt(&ll->arena, expr, alt_con, next);
}

NecroCoreAST_Expression* necro_lambda_lift_case(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
    NecroCoreAST_Expression* expr    = necro_lambda_lift_go(ll, in_ast->case_expr.expr, env, outer);
    NecroCoreAST_CaseAlt*    alts    = necro_lambda_lift_case_alt(ll, in_ast->case_expr.alts, env, outer);
    NecroCoreAST_Expression* out_ast = necro_create_core_case(&ll->arena, expr, alts);
    out_ast->necro_type              = in_ast->necro_type;
    out_ast->case_expr.type          = in_ast->case_expr.type;
    return out_ast;
}

NecroCoreAST_Expression* necro_lambda_lift_app(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
    NecroCoreAST_Expression* out_ast = necro_create_core_app(&ll->arena, necro_lambda_lift_go(ll, in_ast->app.exprA, env, outer), necro_lambda_lift_go(ll, in_ast->app.exprB, env, outer));
    out_ast->necro_type              = in_ast->necro_type;
    out_ast->app.persistent_slot     = in_ast->app.persistent_slot;
    return out_ast;
}

NecroCoreAST_Expression* necro_lambda_lift_list(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);

    // if (in_ast->list.expr == NULL)
    //     return necro_lambda_lift_go(ll, in_ast->list.next, env, outer);

    NecroCoreAST_Expression* initial_lift_point = ll->lift_point;

    // NecroCoreAST_Expression* item_lift = NULL;
    NecroCoreAST_Expression* item = necro_lambda_lift_go(ll, in_ast->list.expr, env, outer);
    NecroCoreAST_Expression* next = necro_lambda_lift_go(ll, in_ast->list.next, env, outer);

    // item_lift = ll->lift_point;
    // while (ll->lift_point != NULL)
    // {
    //     item_lift = necro_create_core_list(&ll->arena, ll->lift_point->list.expr, item_lift);
    //     ll->lift_point = ll->lift_point->list.next;
    // }
    // ll->lift_point = NULL;

    NecroCoreAST_Expression* out_ast = necro_create_core_list(&ll->arena, item, next);

    if (ll->lift_point != NULL && initial_lift_point == NULL && outer == NULL)
    {
        assert(ll->lift_point->expr_type == NECRO_CORE_EXPR_LIST);
        NecroCoreAST_Expression* tmp = ll->lift_point;
        while (tmp->list.next != NULL)
            tmp = tmp->list.next;
        tmp->list.next = out_ast;
        tmp = NULL;
        out_ast = ll->lift_point;
        // ll->lift_point = initial_lift_point;
        ll->lift_point = NULL;
    }

    // NecroCoreAST_Expression* head = NULL;
    // if (in_ast->list.expr == NULL)
    //     head = necro_create_core_list(&ll->arena, NULL, NULL);
    // else
    //     head = necro_create_core_list(&ll->arena, necro_lambda_lift_go(ll, in_ast->list.expr, env, outer), NULL);
    // head->necro_type = in_ast->necro_type;
    // NecroCoreAST_Expression* curr = head;
    // in_ast = in_ast->list.next;
    // while (in_ast != NULL)
    // {
    //     if (in_ast->list.expr == NULL)
    //         curr->list.next = necro_create_core_list(&ll->arena, NULL, NULL);
    //     else
    //         curr->list.next = necro_create_core_list(&ll->arena, necro_lambda_lift_go(ll, in_ast->list.expr, env, outer), NULL);
    //     if (ll->lift_point != NULL)
    //     {
    //         assert(ll->lift_point->expr_type == NECRO_CORE_EXPR_LIST);
    //         NecroCoreAST_Expression* tmp = ll->lift_point;
    //         while (tmp->list.next != NULL)
    //             tmp = tmp->list.next;
    //         tmp->list.next  = curr->list.next;
    //         curr->list.next = ll->lift_point;
    //         curr = tmp->list.next;
    //         ll->lift_point = NULL;
    //     }
    //     else
    //     {
    //         curr = curr->list.next;
    //     }
    //     in_ast = in_ast->list.next;
    // }

    return out_ast;
}

NecroCoreAST_Expression* necro_lambda_lift_smash_lambda(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
    NecroCoreAST_Expression* arg_ast  = necro_deep_copy_core_ast(&ll->arena, in_ast->lambda.arg);
    env                               = necro_cons_var_list(&ll->arena, arg_ast->var, env);
    NecroCoreAST_Expression* expr_ast =
        (in_ast->lambda.expr->expr_type == NECRO_CORE_EXPR_LAM) ?
        necro_lambda_lift_smash_lambda(ll, in_ast->lambda.expr, env, outer) :
        necro_lambda_lift_go(ll, in_ast->lambda.expr, env, outer);
    NecroCoreAST_Expression* out_ast  = necro_create_core_lam(&ll->arena, arg_ast, expr_ast);
    out_ast->necro_type               = in_ast->necro_type;
    return out_ast;
}

NecroCoreAST_Expression* necro_lift_lambda_and_insert_into_env(NecroLambdaLift* ll, NecroCoreAST_Expression* ast_to_lift, NecroVar var)
{
    if (ll->lift_point == NULL)
    {
        ll->lift_point = necro_create_core_list(&ll->arena, ast_to_lift, ll->lift_point);
        necro_var_table_insert(&ll->lifted_env, var.id.id, &var);
        return ll->lift_point;
    }
    else
    {
        NecroCoreAST_Expression* tmp = ll->lift_point;
        while (tmp->list.next != NULL)
            tmp = tmp->list.next;
        tmp->list.next = necro_create_core_list(&ll->arena, ast_to_lift, NULL);
        necro_var_table_insert(&ll->lifted_env, var.id.id, &var);
        return tmp->list.next;
    }
}

NecroCoreAST_Expression* necro_lambda_lift_lambda(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);

    // Create anonymous function name
    ll->num_anon_functions++;
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&ll->snapshot_arena);

    char num_anon_func_buf[20] = { 0 };
    snprintf(num_anon_func_buf, 20, "%zu", ll->num_anon_functions);
    const char* num_anon_func_buf_ptr = (const char*) num_anon_func_buf;

    const char*        fn_name  = necro_concat_strings(&ll->snapshot_arena, 2, (const char*[]) { "_anon_fn_", num_anon_func_buf_ptr });
    NecroSymbol        var_sym  = necro_intern_string(ll->intern, fn_name);
    NecroID            var_id   = necro_scoped_symtable_new_symbol_info(ll->scoped_symtable, ll->scoped_symtable->top_scope, necro_create_initial_symbol_info(var_sym, (NecroSourceLoc) { 0 }, NULL));
    NecroVar           fn_var   = (NecroVar) { .id = var_id, .symbol = var_sym };
    NecroSymbolInfo*   s_info   = necro_symtable_get(ll->symtable, var_id);
    s_info->type                = in_ast->necro_type;

    // TODO: Function which does this AND inserts it into top level scope!!!
    NecroCoreAST_Expression* lambda_lift_point = necro_lift_lambda_and_insert_into_env(ll, NULL, fn_var);
    // NecroCoreAST_Expression* lambda_lift_point = ll->lift_point;
    // if (ll->lift_point == NULL)
    // {
    //     ll->lift_point = necro_create_core_list(&ll->arena, NULL, ll->lift_point);
    //     lambda_lift_point = ll->lift_point;
    // }
    // else
    // {
    //     NecroCoreAST_Expression* tmp = ll->lift_point;
    //     while (tmp->list.next != NULL)
    //         tmp = tmp->list.next;
    //     tmp->list.next = necro_create_core_list(&ll->arena, NULL, NULL);
    //     lambda_lift_point = tmp->list.next;
    // }

    // Env and Outer
    NecroVarList*              prev_env   = env;
    NecroLambdaLiftSymbolInfo* prev_outer = outer;
    env                                   = NULL;
    outer                                 = necro_lambda_lift_symtable_get(ll, fn_var);
    outer->parent                         = prev_outer;
    ll->lift_point                        = necro_create_core_list(&ll->arena, NULL, ll->lift_point);

    // Go deeper
    NecroCoreAST_Expression* arg_ast  = necro_deep_copy_core_ast(&ll->arena, in_ast->lambda.arg);
    env                               = necro_cons_var_list(&ll->arena, arg_ast->var, env);
    NecroCoreAST_Expression* expr_ast =
        (in_ast->lambda.expr->expr_type == NECRO_CORE_EXPR_LAM) ?
        necro_lambda_lift_smash_lambda(ll, in_ast->lambda.expr, env, outer) :
        necro_lambda_lift_go(ll, in_ast->lambda.expr, env, outer);
    expr_ast                          = necro_create_core_lam(&ll->arena, arg_ast, expr_ast);
    expr_ast->necro_type              = in_ast->necro_type;

    // Lift
    NecroCoreAST_Expression*   bind_ast  = necro_create_core_bind(&ll->arena, expr_ast, fn_var);
    NecroLambdaLiftSymbolInfo* info      = outer;
    NecroVarList*              free_vars = necro_reverse_var_list(&ll->arena, info->free_vars);
    // TODO: Look at Types and arities to figure out what's going wrong.
    while (free_vars != NULL)
    // while (false)
    {
        NecroVar env_var = necro_find_free_var(ll, env, outer, free_vars->data);
        expr_ast     = necro_create_core_lam(&ll->arena, necro_create_core_var(&ll->arena, env_var), expr_ast);
        s_info->type = necro_create_type_fun(ll->infer, necro_symtable_get(ll->symtable, free_vars->data.id)->type, s_info->type);
        // if (prev_outer != NULL && necro_is_free_var(ll, prev_env, prev_outer, free_vars->data))
        // {
        //     // prev_outer->free_vars = necro_cons_var_list(&ll->arena, free_vars->data, prev_outer->free_vars);
        //     prev_outer->free_vars = necro_cons_var_list(&ll->arena, necro_copy_var(ll, free_vars->data), prev_outer->free_vars);
        // }
        free_vars = free_vars->next;
    }
    bind_ast->bind.expr = expr_ast;

    lambda_lift_point->list.expr = bind_ast;

    // Apply free vars
    necro_rewind_arena(&ll->snapshot_arena, snapshot);
    return necro_lambda_lift_go(ll, necro_create_core_var(&ll->arena, fn_var), prev_env, prev_outer);
}

NecroCoreAST_Expression* necro_lambda_lift_bind(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);

    // TODO: Nested lambdas messes up ordering at lift point!
    // TODO: Rename lifted functions to avoid name clashes!

    // NecroVarList*              prev_env   = env;
    NecroLambdaLiftSymbolInfo* prev_outer = outer;

    bool is_fn = necro_bind_is_fn(in_ast);
    if (is_fn)
    {
        env           = necro_cons_var_list(&ll->arena, in_ast->bind.var, NULL);
        outer         = necro_lambda_lift_symtable_get(ll, in_ast->bind.var);
        outer->parent = prev_outer;
    }

    NecroCoreAST_Expression* expr_ast = is_fn ?
        necro_lambda_lift_smash_lambda(ll, in_ast->bind.expr, env, outer) :
        necro_lambda_lift_go(ll, in_ast->bind.expr, env, outer);
    NecroCoreAST_Expression* bind_ast = necro_create_core_bind(&ll->arena, expr_ast, in_ast->var);
    bind_ast->necro_type              = in_ast->necro_type;
    bind_ast->bind.is_recursive       = in_ast->bind.is_recursive;

    if (is_fn)
    // if (false)
    {
        NecroLambdaLiftSymbolInfo* info      = outer;
        NecroSymbolInfo*           s_info    = necro_symtable_get(ll->symtable, info->var.id);
        NecroVarList*              free_vars = necro_reverse_var_list(&ll->arena, info->free_vars);
        while (free_vars != NULL)
        {
            NecroVar env_var = necro_find_free_var(ll, env, outer, free_vars->data);
            expr_ast     = necro_create_core_lam(&ll->arena, necro_create_core_var(&ll->arena, env_var), expr_ast);
            s_info->type = necro_create_type_fun(ll->infer, necro_symtable_get(ll->symtable, free_vars->data.id)->type, s_info->type);
            // if (prev_outer != NULL && necro_is_free_var(ll, prev_env, prev_outer, free_vars->data))
            // {
            //     // prev_outer->free_vars = necro_cons_var_list(&ll->arena, free_vars->data, prev_outer->free_vars);
            //     prev_outer->free_vars = necro_cons_var_list(&ll->arena, necro_copy_var(ll, free_vars->data), prev_outer->free_vars);
            // }
            free_vars = free_vars->next;
        }
        bind_ast->bind.expr = expr_ast;

    }

    return bind_ast;
}

NecroCoreAST_Expression* necro_lambda_lift_let(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);

    env                               = necro_cons_var_list(&ll->arena, in_ast->let.bind->bind.var, env);
    NecroCoreAST_Expression* bind_ast = necro_lambda_lift_go(ll, in_ast->let.bind, env, outer);

    // NecroCoreAST_Expression* let_lift_point = NULL;
    if (necro_bind_is_fn(in_ast->let.bind))
        necro_lift_lambda_and_insert_into_env(ll, bind_ast, in_ast->let.bind->bind.var);
        // let_lift_point = necro_lift_lambda_and_insert_into_env(ll, bind_ast);

    NecroCoreAST_Expression* expr_ast = necro_lambda_lift_go(ll, in_ast->let.expr, env, outer);

    if (necro_bind_is_fn(in_ast->let.bind))
    {
        // Lift to top level
        // ll->lift_point = necro_create_core_list(&ll->arena, bind_ast, ll->lift_point);
        // let_lift_point->list.expr = bind_ast;
        return expr_ast;
    }
    else
    {
        NecroCoreAST_Expression* out_ast  = necro_create_core_let(&ll->arena, bind_ast, expr_ast);
        out_ast->necro_type               = in_ast->necro_type;
        return out_ast;
    }
}

NecroCoreAST_Expression* necro_lambda_lift_var(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);

    NecroVar var = necro_find_free_var(ll, env, outer, in_ast->var);

    NecroCoreAST_Expression* out_ast = necro_create_core_var(&ll->arena, var);
    // Set var as free var if not bound
    out_ast->necro_type              = in_ast->necro_type;
    // if (necro_is_free_var(ll, env, outer, in_ast->var))
    if (var.id.id != in_ast->var.id.id)
    {
        // Logic for binding....
        // needs new ID!!!!
        assert(outer != NULL);
        out_ast->var = necro_copy_var(ll, in_ast->var);
        var          = out_ast->var;
        outer->free_vars = necro_cons_var_list(&ll->arena, out_ast->var, outer->free_vars);
    }

    // Apply free vars to variable
    NecroLambdaLiftSymbolInfo* info = necro_lambda_lift_symtable_get(ll, in_ast->var);
    assert(info != NULL);
    NecroVarList* free_vars = info->free_vars;
    while (free_vars != NULL)
    {
        NecroVar env_var = necro_find_free_var(ll, env, outer, free_vars->data);// necro_create_core_var(&ll->arena, free_vars->data)
        if (env_var.id.id == 0)
        {
            // IT'S MISSING, DO SOMETHING!
            env_var = necro_copy_var(ll, free_vars->data);
            outer->free_vars = necro_cons_var_list(&ll->arena, env_var, outer->free_vars);
        }
        // out_ast                        = necro_create_core_app(&ll->arena, out_ast, necro_create_core_var(&ll->arena, free_vars->data));
        out_ast                        = necro_create_core_app(&ll->arena, out_ast, necro_create_core_var(&ll->arena, env_var));
        out_ast->app.exprB->necro_type = necro_symtable_get(ll->symtable, free_vars->data.id)->type;
        free_vars                      = free_vars->next;
    }
    return out_ast;
}

///////////////////////////////////////////////////////
// Lambda Lift Go
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_lambda_lift_go(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    if (in_ast == NULL)
        return NULL;
    switch (in_ast->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:       return necro_lambda_lift_var(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_LAM:       return necro_lambda_lift_lambda(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_LET:       return necro_lambda_lift_let(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_BIND:      return necro_lambda_lift_bind(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_LIST:      return necro_lambda_lift_list(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_CASE:      return necro_lambda_lift_case(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_APP:       return necro_lambda_lift_app(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_DATA_DECL: return necro_deep_copy_core_ast(&ll->arena, in_ast); // TODO: This is only correct if in default state, not in PAT state!
    case NECRO_CORE_EXPR_DATA_CON:  return necro_deep_copy_core_ast(&ll->arena, in_ast); // TODO: This is only correct if in default state, not in PAT state!
    case NECRO_CORE_EXPR_LIT:       return necro_deep_copy_core_ast(&ll->arena, in_ast);
    case NECRO_CORE_EXPR_TYPE:      return necro_deep_copy_core_ast(&ll->arena, in_ast);
    default:                        assert(false && "Unimplemented AST type in necro_lambda_lift_go"); return NULL;
    }
}

///////////////////////////////////////////////////////
// Lambda Lift Pat
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_lambda_lift_app_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
    NecroCoreAST_Expression* expr_a  = necro_lambda_lift_pat_go(ll, in_ast->app.exprA, env, outer);
    NecroCoreAST_Expression* expr_b  = necro_lambda_lift_pat_go(ll, in_ast->app.exprB, env, outer);
    NecroCoreAST_Expression* out_ast = necro_create_core_app(&ll->arena, expr_a, expr_b);
    out_ast->necro_type              = in_ast->necro_type;
    out_ast->app.persistent_slot     = in_ast->app.persistent_slot;
    return out_ast;
}

NecroCoreAST_Expression* necro_lambda_lift_data_con_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
    // Go deeper
    if (in_ast->data_con.next != NULL)
    {
        NecroCoreAST_Expression con;
        con.expr_type                    = NECRO_CORE_EXPR_DATA_CON;
        con.data_con                     = *in_ast->data_con.next;
        NecroCoreAST_Expression* args    = necro_lambda_lift_app_pat(ll, in_ast->data_con.arg_list, env, outer);
        NecroCoreAST_DataCon*    next    = &necro_lambda_lift_app_pat(ll, &con, env, outer)->data_con;
        NecroCoreAST_Expression* out_ast = necro_create_core_data_con(&ll->arena, in_ast->data_con.condid, args, next);
        out_ast->necro_type              = in_ast->necro_type;
        return out_ast;
    }
    else
    {
        NecroCoreAST_Expression* out_ast = necro_create_core_data_con(&ll->arena, in_ast->data_con.condid, necro_lambda_lift_app_pat(ll, in_ast->data_con.arg_list, env, outer), NULL);
        out_ast->necro_type              = in_ast->necro_type;
        return out_ast;
    }
}

NecroCoreAST_Expression* necro_lambda_lift_list_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
    NecroCoreAST_Expression* head = NULL;
    if (in_ast->list.expr == NULL)
        head = necro_create_core_list(&ll->arena, NULL, NULL);
    else
        head = necro_create_core_list(&ll->arena, necro_lambda_lift_pat_go(ll, in_ast->list.expr, env, outer), NULL);
    head->necro_type = in_ast->necro_type;
    NecroCoreAST_Expression* curr = head;
    in_ast = in_ast->list.next;
    while (in_ast != NULL)
    {
        if (in_ast->list.expr == NULL)
            curr->list.next = necro_create_core_list(&ll->arena, NULL, NULL);
        else
            curr->list.next = necro_create_core_list(&ll->arena, necro_lambda_lift_pat_go(ll, in_ast->list.expr, env, outer), NULL);
        curr   = curr->list.next;
        in_ast = in_ast->list.next;
    }
    return head;
}

NecroCoreAST_Expression* necro_lambda_lift_var_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env)
{
    assert(ll != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
    NecroCoreAST_Expression* out_ast = necro_create_core_var(&ll->arena, in_ast->var);
    out_ast->necro_type              = in_ast->necro_type;
    *env                             = necro_cons_var_list(&ll->arena, in_ast->var, *env);
    return out_ast;
}

NecroCoreAST_Expression* necro_lambda_lift_pat_go(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
{
    assert(ll != NULL);
    if (in_ast == NULL)
        return NULL;
    switch (in_ast->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:       return necro_lambda_lift_var_pat(ll, in_ast, env);
    case NECRO_CORE_EXPR_LIST:      return necro_lambda_lift_list_pat(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_DATA_CON:  return necro_lambda_lift_data_con_pat(ll, in_ast, env, outer);
    case NECRO_CORE_EXPR_APP:       return necro_lambda_lift_app_pat(ll, in_ast, env, outer);

    case NECRO_CORE_EXPR_BIND:      assert(false); return NULL;
    case NECRO_CORE_EXPR_LAM:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;

    case NECRO_CORE_EXPR_LIT:       return necro_deep_copy_core_ast(&ll->arena, in_ast);
    case NECRO_CORE_EXPR_TYPE:      return necro_deep_copy_core_ast(&ll->arena, in_ast);
    default:                        assert(false && "Unimplemented AST type in necro_lambda_lift_go"); return NULL;
    }
}


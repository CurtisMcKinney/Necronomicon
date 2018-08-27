/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core_create.h"

///////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_create_core_bind(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroVar var)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_BIND;
    ast->bind.expr               = expr;
    ast->bind.var                = var;
    ast->bind.is_recursive       = false;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_let(NecroPagedArena* arena, NecroCoreAST_Expression* bind, NecroCoreAST_Expression* in)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LET;
    ast->let.bind                = bind;
    ast->let.expr                = in;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_lam(NecroPagedArena* arena, NecroCoreAST_Expression* arg, NecroCoreAST_Expression* expr)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LAM;
    ast->lambda.arg              = arg;
    ast->lambda.expr             = expr;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_app(NecroPagedArena* arena, NecroCoreAST_Expression* expr1, NecroCoreAST_Expression* expr2)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_APP;
    ast->app.exprA               = expr1;
    ast->app.exprB               = expr2;
    ast->necro_type              = NULL;
    ast->app.persistent_slot     = 0;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_var(NecroPagedArena* arena, NecroVar var)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_VAR;
    ast->var                     = var;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_lit(NecroPagedArena* arena, NecroAST_Constant_Reified lit)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LIT;
    ast->lit                     = lit;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_list(NecroPagedArena* arena, NecroCoreAST_Expression* item, NecroCoreAST_Expression* next)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LIST;
    ast->list.expr               = item;
    ast->list.next               = next;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_data_con(NecroPagedArena* arena, NecroVar conid, NecroCoreAST_Expression* arg_list, NecroCoreAST_DataCon* next)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_DATA_CON;
    ast->data_con.condid         = conid;
    ast->data_con.arg_list       = arg_list;
    ast->data_con.next           = next;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_data_decl(NecroPagedArena* arena, NecroVar data_id, NecroCoreAST_DataCon* con_list)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_DATA_DECL;
    ast->data_decl.data_id       = data_id;
    ast->data_decl.con_list      = con_list;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_case(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroCoreAST_CaseAlt* alts)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_CASE;
    ast->case_expr.expr          = expr;
    ast->case_expr.alts          = alts;
    ast->case_expr.type          = NULL;
    ast->necro_type              = NULL;
    return ast;
}

NecroCoreAST_CaseAlt* necro_create_core_case_alt(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroCoreAST_Expression* alt_con, NecroCoreAST_CaseAlt* next)
{
    NecroCoreAST_CaseAlt* alt = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_CaseAlt));
    alt->expr                 = expr;
    alt->altCon               = alt_con;
    alt->next                 = next;
    return alt;
}

NecroCoreAST_Expression* necro_create_core_necro_type(NecroPagedArena* arena, NecroType* necro_type)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_TYPE;
    ast->type.type               = necro_type;
    return ast;
}

///////////////////////////////////////////////////////
// Deep copy
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_deep_copy_core_list(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
    NecroCoreAST_Expression* head = NULL;
    if (in_ast->list.expr == NULL)
        head = necro_create_core_list(arena, NULL, NULL);
    else
        head = necro_create_core_list(arena, necro_deep_copy_core_ast(arena, in_ast->list.expr), NULL);
    head->necro_type = in_ast->necro_type;
    NecroCoreAST_Expression* curr = head;
    in_ast = in_ast->list.next;
    while (in_ast != NULL)
    {
        if (in_ast->list.expr == NULL)
            curr->list.next = necro_create_core_list(arena, NULL, NULL);
        else
            curr->list.next = necro_create_core_list(arena, necro_deep_copy_core_ast(arena, in_ast->list.expr), NULL);
        curr   = curr->list.next;
        in_ast = in_ast->list.next;
    }
    return head;
}

NecroCoreAST_Expression* necro_deep_copy_core_lit(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIT);
    NecroCoreAST_Expression* out_ast = necro_create_core_lit(arena, in_ast->lit);
    out_ast->necro_type = in_ast->necro_type;
    return out_ast;
}

NecroCoreAST_CaseAlt* necro_deep_copy_core_case_alt(NecroPagedArena* arena, NecroCoreAST_CaseAlt* alts)
{
    assert(arena != NULL);
    if (alts == NULL)
        return NULL;
    NecroCoreAST_Expression* alt_con = necro_deep_copy_core_ast(arena, alts->altCon);
    NecroCoreAST_Expression* expr    = necro_deep_copy_core_ast(arena, alts->expr);
    NecroCoreAST_CaseAlt*    next    = necro_deep_copy_core_case_alt(arena, alts->next);
    return necro_create_core_case_alt(arena, expr, alt_con, next);
}

NecroCoreAST_Expression* necro_deep_copy_core_case(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
    NecroCoreAST_Expression* expr    = necro_deep_copy_core_ast(arena, in_ast->case_expr.expr);
    NecroCoreAST_CaseAlt*    alts    = necro_deep_copy_core_case_alt(arena, in_ast->case_expr.alts);
    NecroCoreAST_Expression* out_ast = necro_create_core_case(arena, expr, alts);
    out_ast->necro_type              = in_ast->necro_type;
    out_ast->case_expr.type          = in_ast->case_expr.type;
    return out_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_lam(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
    NecroCoreAST_Expression* arg_ast  = necro_deep_copy_core_ast(arena, in_ast->lambda.arg);
    NecroCoreAST_Expression* expr_ast = necro_deep_copy_core_ast(arena, in_ast->lambda.expr);
    NecroCoreAST_Expression* out_ast  = necro_create_core_lam(arena, arg_ast, expr_ast);
    out_ast->necro_type               = in_ast->necro_type;
    return out_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_let(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
    NecroCoreAST_Expression* bind_ast = necro_deep_copy_core_ast(arena, in_ast->let.bind);
    NecroCoreAST_Expression* expr_ast = necro_deep_copy_core_ast(arena, in_ast->let.expr);
    NecroCoreAST_Expression* out_ast  = necro_create_core_let(arena, bind_ast, expr_ast);
    out_ast->necro_type               = in_ast->necro_type;
    return out_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_var(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
    NecroCoreAST_Expression* out_ast = necro_create_core_var(arena, in_ast->var);
    out_ast->necro_type              = in_ast->necro_type;
    return out_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_bind(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);
    NecroCoreAST_Expression* expr_ast = necro_deep_copy_core_ast(arena, in_ast->bind.expr);
    NecroCoreAST_Expression* bind_ast = necro_create_core_bind(arena, expr_ast, in_ast->var);
    bind_ast->necro_type              = in_ast->necro_type;
    bind_ast->bind.is_recursive       = in_ast->bind.is_recursive;
    return bind_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_data_decl(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);
    assert(in_ast->data_decl.con_list != NULL);
    NecroCoreAST_Expression con;
    con.expr_type = NECRO_CORE_EXPR_DATA_CON;
    con.data_con  = *in_ast->data_decl.con_list;
    NecroCoreAST_Expression* out_ast = necro_create_core_data_decl(arena, in_ast->var, &necro_deep_copy_core_ast(arena, &con)->data_con);
    out_ast->necro_type              = in_ast->necro_type;
    return out_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_data_con(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
    // Go deeper
    if (in_ast->data_con.next != NULL)
    {
        NecroCoreAST_Expression con;
        con.expr_type                    = NECRO_CORE_EXPR_DATA_CON;
        con.data_con                     = *in_ast->data_con.next;
        NecroCoreAST_Expression* out_ast = necro_create_core_data_con(arena, in_ast->data_con.condid, necro_deep_copy_core_ast(arena, in_ast->data_con.arg_list), &necro_deep_copy_core_ast(arena, &con)->data_con);
        out_ast->necro_type              = in_ast->necro_type;
        return out_ast;
    }
    else
    {
        NecroCoreAST_Expression* out_ast = necro_create_core_data_con(arena, in_ast->data_con.condid, necro_deep_copy_core_ast(arena, in_ast->data_con.arg_list), NULL);
        out_ast->necro_type              = in_ast->necro_type;
        return out_ast;
    }
}

NecroCoreAST_Expression* necro_deep_copy_core_app(NecroPagedArena* arena, NecroCoreAST_Expression* in_ast)
{
    assert(arena != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
    NecroCoreAST_Expression* out_ast = necro_create_core_app(arena, necro_deep_copy_core_ast(arena, in_ast->app.exprA), necro_deep_copy_core_ast(arena, in_ast->app.exprB));
    out_ast->necro_type              = in_ast->necro_type;
    out_ast->app.persistent_slot     = in_ast->app.persistent_slot;
    return out_ast;
}

NecroCoreAST_Expression* necro_deep_copy_core_ast(NecroPagedArena* arena, NecroCoreAST_Expression* ast)
{
    assert(arena != NULL);
    if (ast == NULL)
        return NULL;
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_APP:       return necro_deep_copy_core_app(arena, ast);
    case NECRO_CORE_EXPR_BIND:      return necro_deep_copy_core_bind(arena, ast);
    case NECRO_CORE_EXPR_LAM:       return necro_deep_copy_core_lam(arena, ast);
    case NECRO_CORE_EXPR_CASE:      return necro_deep_copy_core_case(arena, ast);
    case NECRO_CORE_EXPR_LET:       return necro_deep_copy_core_let(arena, ast);
    case NECRO_CORE_EXPR_VAR:       return necro_deep_copy_core_var(arena, ast);
    case NECRO_CORE_EXPR_LIT:       return necro_deep_copy_core_lit(arena, ast);
    case NECRO_CORE_EXPR_DATA_DECL: return necro_deep_copy_core_data_decl(arena, ast);
    case NECRO_CORE_EXPR_DATA_CON:  return necro_deep_copy_core_data_con(arena, ast);
    case NECRO_CORE_EXPR_LIST:      return necro_deep_copy_core_list(arena, ast);
    case NECRO_CORE_EXPR_TYPE:      return necro_create_core_necro_type(arena, ast->type.type);
    default:                        assert(false && "Unimplemented AST type in necro_deep_copy_core_ast"); return NULL;
    }
}


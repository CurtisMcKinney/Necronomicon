/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core_create.h"

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


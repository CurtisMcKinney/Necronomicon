/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "closure_conversion.h"
#include <stdio.h>
#include "type.h"

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
typedef struct NecroClosureConversion
{
    NecroPagedArena       arena;
    NecroSnapshotArena    snapshot_arena;
    NecroIntern*          intern;
    NecroSymTable*        symtable;
    NecroScopedSymTable*  scoped_symtable;
    NecroPrimTypes*       prim_types;
} NecroClosureConversion;

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_closure_conversion_go(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast);

///////////////////////////////////////////////////////
// Closure Conversion
///////////////////////////////////////////////////////
NecroCoreAST necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types)
{
    NecroClosureConversion cc = (NecroClosureConversion)
    {
        .arena           = necro_create_paged_arena(),
        .snapshot_arena  = necro_create_snapshot_arena(0),
        .intern          = intern,
        .symtable        = symtable,
        .scoped_symtable = scoped_symtable,
        .prim_types      = prim_types,
    };
    NecroCoreAST_Expression* out_ast = necro_closure_conversion_go(&cc, in_ast->root);
    necro_destroy_snapshot_arena(&cc.snapshot_arena);
    return (NecroCoreAST) { .arena = cc.arena, .root = out_ast };
}

///////////////////////////////////////////////////////
// NecroCore create (Canonical constructors should be in core.h ....)
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_create_core_bind(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroVar var)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_BIND;
    ast->bind.expr               = ast;
    ast->bind.var                = var;
    ast->bind.is_recursive       = false;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_let(NecroPagedArena* arena, NecroCoreAST_Expression* bind, NecroCoreAST_Expression* in)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LET;
    ast->let.bind                = bind;
    ast->let.expr                = in;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_lam(NecroPagedArena* arena, NecroCoreAST_Expression* arg, NecroCoreAST_Expression* expr)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LAM;
    ast->lambda.arg              = arg;
    ast->lambda.expr             = expr;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_app(NecroPagedArena* arena, NecroCoreAST_Expression* expr1, NecroCoreAST_Expression* expr2)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_APP;
    ast->app.exprA               = expr1;
    ast->app.exprB               = expr2;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_var(NecroPagedArena* arena, NecroVar var)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_VAR;
    ast->var                     = var;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_lit(NecroPagedArena* arena, NecroAST_Constant_Reified lit)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LIT;
    ast->lit                     = lit;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_list(NecroPagedArena* arena, NecroCoreAST_Expression* item, NecroCoreAST_Expression* next)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_LIST;
    ast->list.expr               = item;
    ast->list.next               = next;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_data_con(NecroPagedArena* arena, NecroVar conid, NecroCoreAST_Expression* arg_list, NecroCoreAST_DataCon* next)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_DATA_CON;
    ast->data_con.condid         = conid;
    ast->data_con.arg_list       = arg_list;
    ast->data_con.next           = next;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_data_decl(NecroPagedArena* arena, NecroVar data_id, NecroCoreAST_DataCon* con_list)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_DATA_DECL;
    ast->data_decl.data_id       = data_id;
    ast->data_decl.con_list      = con_list;
    return ast;
}

NecroCoreAST_Expression* necro_create_core_case(NecroPagedArena* arena, NecroCoreAST_Expression* expr, NecroCoreAST_CaseAlt* alts)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_CASE;
    ast->case_expr.expr          = expr;
    ast->case_expr.alts          = alts;
    ast->case_expr.type          = NULL;
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

///////////////////////////////////////////////////////
// Closure conversion
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_closure_conversion_list(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
    NecroCoreAST_Expression* head = necro_create_core_list(&cc->arena, necro_closure_conversion_go(cc, in_ast->list.expr), NULL);
    NecroCoreAST_Expression* curr = head;
    in_ast = in_ast->list.next;
    while (in_ast != NULL)
    {
        curr->list.next = necro_create_core_list(&cc->arena, necro_closure_conversion_go(cc, in_ast->list.expr), NULL);
        curr            = curr->list.next;
        in_ast = in_ast->list.next;
    }
    return head;
}

NecroCoreAST_Expression* necro_closure_conversion_data_decl(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);
    assert(in_ast->data_decl.con_list != NULL);
    NecroCoreAST_Expression con;
    con.expr_type = NECRO_CORE_EXPR_DATA_CON;
    con.data_con  = *in_ast->data_decl.con_list;
    return necro_create_core_data_decl(&cc->arena, in_ast->var, &necro_closure_conversion_go(cc, &con)->data_con);
}

NecroCoreAST_Expression* necro_closure_conversion_data_con(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
    if (in_ast->data_con.next == NULL)
    {
        return necro_create_core_data_con(&cc->arena, in_ast->data_con.condid, necro_closure_conversion_go(cc, in_ast->data_con.arg_list), NULL);
    }
    else
    {
        NecroCoreAST_Expression con;
        con.expr_type = NECRO_CORE_EXPR_DATA_CON;
        con.data_con  = *in_ast->data_con.next;
        return necro_create_core_data_con(&cc->arena, in_ast->data_con.condid, necro_closure_conversion_go(cc, in_ast->data_con.arg_list), &necro_closure_conversion_go(cc, in_ast->data_con.arg_list)->data_con);
    }
}

NecroCoreAST_Expression* necro_closure_conversion_lit(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIT);
    return necro_create_core_lit(&cc->arena, in_ast->lit);
}

NecroCoreAST_Expression* necro_closure_conversion_var(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
    return necro_create_core_var(&cc->arena, in_ast->var);
}

NecroCoreAST_CaseAlt* necro_closure_conversion_alts(NecroClosureConversion* cc, NecroCoreAST_CaseAlt* alts)
{
    assert(cc != NULL);
    if (alts == NULL)
        return NULL;
    NecroCoreAST_Expression* alt_con = necro_closure_conversion_go(cc, alts->expr);
    NecroCoreAST_Expression* expr    = necro_closure_conversion_go(cc, alts->altCon);
    NecroCoreAST_CaseAlt*    next    = necro_closure_conversion_alts(cc, alts->next);
    return necro_create_core_case_alt(&cc->arena, expr, alt_con, next);
}

NecroCoreAST_Expression* necro_closure_conversion_case(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
    NecroCoreAST_Expression* expr = necro_closure_conversion_go(cc, in_ast->case_expr.expr);
    NecroCoreAST_CaseAlt*    alts = necro_closure_conversion_alts(cc, in_ast->case_expr.alts);
    return necro_create_core_case(&cc->arena, expr, alts);
}

NecroCoreAST_Expression* necro_closure_conversion_let(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
    NecroCoreAST_Expression* bind_ast = necro_closure_conversion_go(cc, in_ast->let.bind);
    NecroCoreAST_Expression* expr_ast = necro_closure_conversion_go(cc, in_ast->let.expr);
    return necro_create_core_let(&cc->arena, bind_ast, expr_ast);
}

///////////////////////////////////////////////////////
// Closure Conversion Go
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_closure_conversion_go(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    switch (in_ast->expr_type)
    {
    case NECRO_CORE_EXPR_LAM:       return NULL;
    case NECRO_CORE_EXPR_APP:       return NULL;
    case NECRO_CORE_EXPR_BIND:      return NULL;
    case NECRO_CORE_EXPR_CASE:      return necro_closure_conversion_case(cc, in_ast);
    case NECRO_CORE_EXPR_LET:       return necro_closure_conversion_let(cc, in_ast);
    case NECRO_CORE_EXPR_VAR:       return necro_closure_conversion_var(cc, in_ast);
    case NECRO_CORE_EXPR_LIT:       return necro_closure_conversion_lit(cc, in_ast);
    case NECRO_CORE_EXPR_DATA_DECL: return necro_closure_conversion_data_decl(cc, in_ast);
    case NECRO_CORE_EXPR_DATA_CON:  return necro_closure_conversion_data_con(cc, in_ast);
    case NECRO_CORE_EXPR_LIST:      return necro_closure_conversion_list(cc, in_ast);
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    default:                        assert(false && "Unimplemented AST type in necro_closure_conversion_go"); return NULL;
    }
}

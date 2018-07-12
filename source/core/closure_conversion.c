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

// TODO: Finish
// NecroCoreAST_Expression* necro_create_core_data_con(NecroPagedArena* arena, NecroAST_Constant_Reified lit)
// {
//     NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
//     ast->expr_type               = NECRO_CORE_EXPR_DATA_CON;
//     ast->data_con.
//     return ast;
// }

///////////////////////////////////////////////////////
// Closure Conversion Go
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_closure_conversion_go(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    switch (in_ast->expr_type)
    {
    case NECRO_CORE_EXPR_BIND:      return NULL; // necro_core_to_machine_3_bind(program, core_ast, outer);
    case NECRO_CORE_EXPR_LET:       return NULL; // necro_core_to_machine_3_let(program, core_ast, outer);
    case NECRO_CORE_EXPR_LAM:       return NULL; // necro_core_to_machine_3_lambda(program, core_ast, outer);
    case NECRO_CORE_EXPR_APP:       return NULL; // necro_core_to_machine_3_app(program, core_ast, outer);
    case NECRO_CORE_EXPR_CASE:      return NULL; // necro_core_to_machine_3_case(program, core_ast, outer);
    case NECRO_CORE_EXPR_VAR:       return NULL; // necro_core_to_machine_3_var(program, core_ast, outer);
    case NECRO_CORE_EXPR_LIT:       return NULL; // necro_core_to_machine_3_lit(program, core_ast, outer);
    case NECRO_CORE_EXPR_DATA_DECL: return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    default:                        assert(false && "Unimplemented AST type in necro_closure_conversion_go"); return NULL;
    }
}

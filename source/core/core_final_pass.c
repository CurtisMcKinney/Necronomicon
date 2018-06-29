/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core_final_pass.h"
#include "type.h"

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
typedef struct
{
    NecroCoreAST    core_ast;
    NecroIntern*    intern;
    NecroSymTable*  symtable;
    NecroError      error;
} NecroCoreFinalPass;

NecroCoreFinalPass necro_create_final_pass(NecroIntern* intern, NecroSymTable* symtable)
{
    return (NecroCoreFinalPass)
    {
        .core_ast = (NecroCoreAST) { .arena = necro_create_paged_arena(), .root = NULL },
        .intern   = intern,
        .symtable = symtable,
        .error    = necro_create_error(),
    };
}

void necro_destroy_final_pass(NecroCoreFinalPass* final_pass)
{
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
inline bool necro_is_final_pass_error(NecroCoreFinalPass* final_pass)
{
    return final_pass->error.return_code == NECRO_ERROR;
}

inline void necro_throw_final_pass_error(NecroCoreFinalPass* final_pass, struct NecroCoreAST_Expression* ast, const char* error_message)
{
    necro_error(&final_pass->error, (NecroSourceLoc) {0}, error_message);
}

///////////////////////////////////////////////////////
// Stuff which really should be in core.h/core.c
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_create_core_var(NecroPagedArena* arena, NecroID id, NecroSymbol symbol)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_VAR;
    ast->var.id                  = id;
    ast->var.symbol              = symbol;
    return ast;
}

NecroCoreAST_Expression* necro_create_data_con(NecroPagedArena* arena, NecroCoreAST_Expression* arg_list, NecroVar conid, NecroCoreAST_DataCon* next)
{
    NecroCoreAST_Expression* ast = necro_paged_arena_alloc(arena, sizeof(NecroCoreAST_Expression));
    ast->expr_type               = NECRO_CORE_EXPR_DATA_CON;
    ast->data_con.arg_list       = arg_list;
    ast->data_con.condid         = conid;
    ast->data_con.next           = next;
    return ast;
}

///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_core_final_pass_go(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast);

///////////////////////////////////////////////////////
// Final Pass
///////////////////////////////////////////////////////
NecroCoreAST necro_core_final_pass(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable)
{
    NecroCoreFinalPass final_pass = necro_create_final_pass(intern, symtable);
    necro_core_final_pass_go(&final_pass, in_ast->root);
    return final_pass.core_ast;
}

NecroCoreAST_Expression* necro_core_final_pass_var_go(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
{
    assert(final_pass != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (necro_is_final_pass_error(final_pass)) return NULL;
    NecroCoreAST_Expression* out_ast = necro_create_core_var(&final_pass->core_ast.arena, in_ast->var.id, in_ast->var.symbol);
    return out_ast;
}

NecroCoreAST_Expression* necro_core_final_pass_go(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
{
    assert(final_pass != NULL);
    assert(in_ast != NULL);
    if (necro_is_final_pass_error(final_pass)) return NULL;
    switch (in_ast->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:       return necro_core_final_pass_var_go(final_pass, in_ast);
    case NECRO_CORE_EXPR_DATA_DECL: return NULL;
    case NECRO_CORE_EXPR_BIND:      return NULL;
    case NECRO_CORE_EXPR_APP:       return NULL;
    case NECRO_CORE_EXPR_LAM:       return NULL;
    case NECRO_CORE_EXPR_LET:       return NULL;
    case NECRO_CORE_EXPR_CASE:      return NULL;
    case NECRO_CORE_EXPR_LIT:       return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
    case NECRO_CORE_EXPR_LIST:      return NULL; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    default:                        assert(false && "Unimplemented AST type in necro_core_final_pass_go"); return NULL;
    }
    return NULL;
}
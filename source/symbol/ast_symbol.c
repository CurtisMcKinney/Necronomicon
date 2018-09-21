/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "ast_symbol.h"

NecroAstSymbolData* necro_ast_symbol_data_create(NecroPagedArena* arena, struct NecroAst* ast, struct NecroAst* optional_type_signature, struct NecroDeclarationGroup* declaration_group)
{
    NecroAstSymbolData* data      = necro_paged_arena_alloc(arena, sizeof(NecroAstSymbolData));
    data->ast                     = ast;
    data->optional_type_signature = optional_type_signature;
    data->declaration_group       = declaration_group;
    return data;
}

const char* necro_ast_symbol_most_qualified_name(NecroAstSymbol ast_symbol)
{
    if (ast_symbol.name != NULL && ast_symbol.name->str != NULL)
        return ast_symbol.name->str;
    else if (ast_symbol.source_name != NULL && ast_symbol.source_name->str != NULL)
        return ast_symbol.source_name->str;
    else
        return "NULL AST_SYMBOL";
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "ast_symbol.h"

NecroAstSymbol* necro_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, struct NecroAst* ast)
{
    NecroAstSymbol* ast_symbol = necro_paged_arena_alloc(arena, sizeof(NecroAstSymbol));
    *ast_symbol = (NecroAstSymbol)
    {
        .name                    = name,
        .source_name             = source_name,
        .module_name             = module_name,
        .ast                     = ast,
        .optional_type_signature = NULL,
        .declaration_group       = NULL,
        .type                    = NULL,
        .con_num                 = 0,
        .is_constructor          = false,
        .type_status             = NECRO_TYPE_UNCHECKED,
        .is_recursive            = false,
        .method_type_class       = NULL,
        .type_class              = NULL,
        .type_class_instance     = NULL,
        .necro_machine_ast       = NULL,
    };
    return ast_symbol;
}

const char* necro_ast_symbol_most_qualified_name(NecroAstSymbol* ast_symbol)
{
    if (ast_symbol->name != NULL && ast_symbol->name->str != NULL)
        return ast_symbol->name->str;
    else if (ast_symbol->source_name != NULL && ast_symbol->source_name->str != NULL)
        return ast_symbol->source_name->str;
    else
        return "NULL AST_SYMBOL";
}

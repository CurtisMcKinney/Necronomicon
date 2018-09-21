/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_AST_SYMBOL_H
#define NECRO_AST_SYMBOL_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "intern.h"

struct NecroScope;
struct NecroDeclarationGroup;

typedef struct
{
    struct NecroAst*                ast;
    struct NecroAst*                optional_type_signature;
    struct NecroDeclarationGroup*   declaration_group;
} NecroAstSymbolData;

typedef struct
{
    NecroSymbol         name;
    NecroSymbol         source_name;
    NecroSymbol         module_name;
    NecroAstSymbolData* data;
} NecroAstSymbol;

static const NecroAstSymbol necro_ast_symbol_empty = { .name = NULL, .source_name = NULL, .module_name = NULL, .data = NULL };

const char*         necro_ast_symbol_most_qualified_name(NecroAstSymbol ast_symbol);
NecroAstSymbolData* necro_ast_symbol_data_create(NecroPagedArena* arena, struct NecroAst* ast, struct NecroAst* optional_type_signature, struct NecroDeclarationGroup* declaration_group);

#endif // NECRO_AST_SYMBOL_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_RENAMER_H
#define NECRO_RENAMER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "symtable.h"
#include "ast.h"

typedef enum
{
    NECRO_VALUE_NAMESPACE,
    NECRO_TYPE_NAMESPACE,
} NECRO_NAMESPACE_TYPE;

NecroResult(void) necro_rename(NecroCompileInfo info, NecroScopedSymTable* scoped_symtable, NecroIntern* intern, NecroAstArena* ast_arena);
void              necro_rename_internal_scope_and_rename(NecroAstArena* ast_arena, NecroScopedSymTable* scoped_symtable, NecroIntern* intern, NecroAst* ast);
NecroAstSymbol*   necro_get_unique_name(NecroAstArena* ast_arena, NecroIntern* intern, NECRO_NAMESPACE_TYPE namespace_type, NECRO_MANGLE_TYPE mangle_type, NecroAstSymbol* ast_symbol);
NecroSymbol       necro_append_clash_suffix_to_name(NecroAstArena* ast_arena, NecroIntern* intern, const char* name);
void              necro_rename_test();

#endif // NECRO_RENAMER_H

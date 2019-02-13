/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef D_ANALYZER_H
#define D_ANALYZER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "symtable.h"
#include "ast.h"

// Dependency analysis
// typedef struct NecroDependencyList
// {
//     struct NecroDeclarationGroup* dependency_group;
//     struct NecroDependencyList*   next;
// } NecroDependencyList;

// typedef struct NecroDeclarationGroup
// {
//     NecroAst*                     declaration_ast;
//     struct NecroDeclarationGroup* next;
//     NecroDependencyList*          dependency_list;
//     struct NecroDeclarationsInfo* info;
//     int32_t                       index;
//     int32_t                       low_link;
//     bool                          on_stack;
//     bool                          type_checked;
// } NecroDeclarationGroup;

// typedef struct NecroDeclarationGroupList
// {
//     NecroDeclarationGroup*            declaration_group;
//     struct NecroDeclarationGroupList* next;
// } NecroDeclarationGroupList;

// NecroDeclarationGroup* necro_declaration_group_append(NecroPagedArena* arena, NecroAst* declaration_ast, NecroDeclarationGroup* head);
void necro_dependency_analyze(NecroCompileInfo info, NecroIntern* intern, NecroAstArena* ast_arena);

#endif // D_ANALYZER_H

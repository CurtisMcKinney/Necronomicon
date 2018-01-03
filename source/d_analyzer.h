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

typedef struct
{
    NecroSymTable*         symtable;
    NecroError             error;
    NecroDeclarationGroup* current_declaration_group;
    NecroPagedArena*       arena;
} NecroDependencyAnalyzer;

NecroDependencyAnalyzer necro_create_dependency_analyzer(NecroSymTable* symtable);
void                    necro_destroy_dependency_analyzer(NecroDependencyAnalyzer* d_analyzer);
NECRO_RETURN_CODE       necro_dependency_analyze_ast(NecroDependencyAnalyzer* d_analyzer, NecroPagedArena* ast_arena, NecroASTNode* ast);

#endif // D_ANALYZER_H

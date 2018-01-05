/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RENAMER_H
#define RENAMER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "symtable.h"
#include "ast.h"

typedef struct
{
    NecroIntern*           intern;
    NecroScopedSymTable*   scoped_symtable;
    NecroError             error;
    NecroSymbol            current_class_instance_symbol;
    NecroSymbol            prev_class_instance_symbol;
    bool                   should_free_type_declare;
    NecroDeclarationGroup* current_declaration_group;
    NecroPagedArena*       arena;
} NecroRenamer;

NecroRenamer      necro_create_renamer(NecroScopedSymTable* scoped_symtable, NecroIntern*);
void              necro_destroy_renamer(NecroRenamer* renamer);
NECRO_RETURN_CODE necro_rename_declare_pass(NecroRenamer* renamer, NecroPagedArena* ast_arena, NecroAST_Node_Reified* input_ast);
NECRO_RETURN_CODE necro_rename_var_pass(NecroRenamer* renamer, NecroPagedArena* ast_arena, NecroAST_Node_Reified* input_ast);

#endif // RENAMER_H

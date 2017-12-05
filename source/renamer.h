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

typedef enum
{
    NECRO_NO_DECLARE,
    NECRO_DECLARE_PATTERN,
    NECRO_DECLARE_TYPE,
    NECRO_DECLARE_DATA
} NECRO_DECLARE_MODE;

typedef struct
{
    NecroScopedSymTable* scoped_symtable;
    NECRO_DECLARE_MODE   declare_mode;
    NecroError           error;
} NecroRenamer;

NecroRenamer      necro_create_renamer(NecroScopedSymTable* scoped_symtable);
void              necro_destroy_renamer(NecroRenamer* renamer);
NECRO_RETURN_CODE necro_rename_declare_pass(NecroRenamer* renamer, NecroAST_Reified* input_ast);
NECRO_RETURN_CODE necro_rename_var_pass(NecroRenamer* renamer, NecroAST_Reified* input_ast);

#endif // RENAMER_H

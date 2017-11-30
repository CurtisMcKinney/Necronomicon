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
    NECRO_RENAME_NORMAL,
    NECRO_RENAME_PATTERN
} NECRO_RENAMER_MODE;

typedef struct
{
    NecroAST_Reified    ast;
    NecroScopedSymTable scoped_symtable;
    NECRO_RENAMER_MODE  mode;
    NecroError          error;
} NecroRenamer;

NecroRenamer      necro_create_renamer(NecroSymTable* symtable);
void              necro_destroy_renamer(NecroRenamer* renamer);
NECRO_RETURN_CODE necro_rename_ast(NecroRenamer* renamer, NecroAST_Reified* input_ast);

#endif // RENAMER_H

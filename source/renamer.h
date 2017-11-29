/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RENAMER_H
#define RENAMER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "parser.h"
#include "symtable.h"
#include "ast.h"

typedef enum
{
    NECRO_RENAME_SUCCESSFUL,
    NECRO_RENAME_ERROR
} NECRO_RENAME_RESULT_TYPE;

typedef struct
{
    union
    {
        NecroError       error;
        NecroAST_Reified ast;
    };
    NECRO_RENAME_RESULT_TYPE type;
} NecroRenameResult;

NecroRenameResult necro_rename_ast(NecroAST_Reified* input_ast, NecroSymTable* symtable);
void necro_test_rename(const char* input_string);

#endif // RENAMER_H

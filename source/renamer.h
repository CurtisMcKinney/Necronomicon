/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RENAMER_H
#define RENAMER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

typedef enum
{
    NECRO_RENAME_SUCCESSFUL,
    NECRO_RENAME_ERROR
} NECRO_RENAME_RESULT;

typedef struct
{
    NecroSymbol* symbols;
} NecroSymbolTable;

typedef struct
{
    char*     error_message;
    NecroAST* ast;
} NecroRenamer;

NECRO_RENAME_RESULT rename_ast(NecroRenamer* renamer, NecroAST_LocalPtr* out_root_node_ptr);

#endif // RENAMER_H

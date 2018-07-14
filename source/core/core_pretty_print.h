/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_PRETTY_PRINT_H
#define CORE_PRETTY_PRINT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core.h"
#include "symtable.h"

void necro_core_pretty_print(NecroCoreAST* ast, NecroSymTable* symtable);

#endif // CORE_PRETTY_PRINT_H

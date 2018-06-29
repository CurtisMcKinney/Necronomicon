/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_FINAL_PASS_H
#define CORE_FINAL_PASS_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core.h"

NecroCoreAST necro_core_final_pass(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable);

#endif // CORE_FINAL_PASS
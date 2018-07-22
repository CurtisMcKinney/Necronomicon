/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_CLOSURE_CONVERSION_H
#define CORE_CLOSURE_CONVERSION_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core.h"

NecroCoreAST necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer);

#endif // CORE_CLOSURE_CONVERSION_H

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

typedef struct NecroApplyDef
{
    size_t apply_arity;
    size_t fn_arity;
    size_t num_pargs;
    bool   is_stateful;
    size_t uid;
    size_t label;
} NecroApplyDef;
NECRO_DECLARE_VECTOR(NecroApplyDef, NecroApplyDef, apply_def);

NecroCoreAST necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer, NecroApplyDefVector* out_apply_defs);

#endif // CORE_CLOSURE_CONVERSION_H

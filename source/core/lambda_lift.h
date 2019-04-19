/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_LAMBDA_LIFT_H
#define CORE_LAMBDA_LIFT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core_ast.h"

void necro_core_lambda_lift(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena);
void necro_core_lambda_lift_test();

#endif // CORE_LAMBDA_LIFT_H

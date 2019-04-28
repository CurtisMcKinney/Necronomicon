/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_CORE_DEFUNCTIONALIZATION_H
#define NECRO_CORE_DEFUNCTIONALIZATION_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core_ast.h"
#include "base.h"
#include "driver.h"

void necro_defunctionalize(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena);
void necro_defunctionalize_test();

#endif // NECRO_CORE_DEFUNCTIONALIZATION_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_STATE_ANALYSIS_H
#define CORE_STATE_ANALYSIS_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core_ast.h"

void necro_core_state_analysis(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena);
void necro_state_analysis_test();

#endif // CORE_STATE_ANALYSIS_H

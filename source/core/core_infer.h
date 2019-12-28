/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_INFER
#define CORE_INFER 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core_ast.h"
#include "result.h"

NecroResult(void) necro_core_infer(NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena);
size_t            necro_core_ast_hash(NecroCoreAst* ast);

#endif // CORE_INFER

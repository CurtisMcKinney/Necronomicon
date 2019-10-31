/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef D_ANALYZER_H
#define D_ANALYZER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "symtable.h"
#include "ast.h"

struct NecroBase;

void necro_dependency_analyze(NecroCompileInfo info, NecroIntern* intern, struct NecroBase* base, NecroAstArena* ast_arena);

#endif // D_ANALYZER_H

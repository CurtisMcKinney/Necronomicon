/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_TYPE_ALIAS_ANALYSIS_H
#define NECRO_TYPE_ALIAS_ANALYSIS_H 1


#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "utility.h"
#include "type.h"
#include "necro/driver.h"

NecroResult(void) necro_alias_analysis(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);

#endif // NECRO_TYPE_ALIAS_ANALYSIS_H
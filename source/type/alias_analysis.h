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

//--------------------
// Sharing Analysis
//--------------------
typedef struct NecroUsage
{
    struct NecroUsage* next;
    NecroSourceLoc     source_loc;
    NecroSourceLoc     end_loc;
} NecroUsage;

typedef struct NecroFreeVars
{
    struct NecroFreeVars* next;
    size_t                count;
    NecroAstSymbol*       data;
} NecroFreeVars;

void necro_alias_analysis(NecroCompileInfo info, NecroAstArena* ast_arena);
void necro_alias_analysis_test();
bool necro_usage_is_unshared(NecroUsage* usage);
void necro_free_vars_print(NecroFreeVars* free_vars);

#endif // NECRO_TYPE_ALIAS_ANALYSIS_H
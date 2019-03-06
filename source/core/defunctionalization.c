/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "closure_conversion.h"
#include <stdio.h>
#include "type.h"
#include "prim.h"
#include "core_create.h"

/*
    * NOTES:
        - Could use a "merge" function for case statements to merge different possible lambda values.

*/

struct NecroStaticValue;
struct NecroStaticValueList;

typedef enum
{
    NECRO_STATIC_VALUE_DYNAMIC,
    NECRO_STATIC_VALUE_CONSTRUCTOR,
    NECRO_STATIC_VALUE_LAMBDA,
} NECRO_STATIC_VALUE_TYPE;

typedef struct
{
    NecroType* type;
} NecroStaticValueDynamic;

typedef struct
{
    NecroType*  type;
    NecroSymbol function_symbol;
} NecroStaticValueLambda;

typedef struct
{
    NecroType*                   type;
    struct NecroStaticValueList* args;
} NecroStaticValueConstructor;

typedef struct NecroStaticValue
{
    union
    {
        NecroStaticValueDynamic     dyn;
        NecroStaticValueConstructor con;
        NecroStaticValueLambda      lam;
    };
    NECRO_STATIC_VALUE_TYPE type;
} NecroStaticValue;

NECRO_DECLARE_ARENA_LIST(struct NecroStaticValue, StaticValue, static_value);

typedef struct
{
    NecroPagedArena* arena;
} NecroDefunctionalizer;

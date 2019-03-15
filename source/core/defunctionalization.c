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
    - Largely based on "High Performance Defunctionalisation in Futhark": https://futhark-lang.org/publications/tfp18.pdf
    - Some obvious differences are that we support the branching extension
    - Perhaps also Arrays (since all containers have a fixed bounded size in Necro).
    - If we could obviate the need for using lifted type in recursive value check we could nix lifted types entirely.
*/

struct NecroStaticValue;
struct NecroStaticValueList;

typedef enum
{
    NECRO_STATIC_VALUE_DYNAMIC,
    NECRO_STATIC_VALUE_LAMBDA,
    NECRO_STATIC_VALUE_CONSTRUCTOR,
    NECRO_STATIC_VALUE_SUM,
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
    NecroType*                   type; // The Type of the constructor.
    NecroAstSymbol*              con;  // The Specific constructor of the provided type. Replace with NecroCoreAstSymbol or whatever when that's done.
    struct NecroStaticValueList* args;
} NecroStaticValueConstructor;

typedef struct
{
    NecroType*                   type;
    struct NecroStaticValueList* options; // Constructors containing further types.
} NecroStaticValueSum;

typedef struct NecroStaticValue
{
    union
    {
        NecroStaticValueDynamic     dyn;
        NecroStaticValueLambda      lam;
        NecroStaticValueConstructor con;
    };
    NECRO_STATIC_VALUE_TYPE type;
} NecroStaticValue;

NECRO_DECLARE_ARENA_LIST(struct NecroStaticValue, StaticValue, static_value);

typedef struct
{
    NecroPagedArena* arena;
} NecroDefunctionalizer;


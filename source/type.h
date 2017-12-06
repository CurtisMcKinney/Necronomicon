/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_H
#define TYPE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "intern.h"
#include "ast.h"

// TODO: Predicates?

// Var / Con
typedef struct
{
    NecroSymbol symbol;
    NecroID     id;
} NecroVar;

typedef struct
{
    NecroSymbol symbol;
    NecroID     id;
} NecroCon;

// NecroType
struct NecroType;
typedef enum
{
    NECRO_TYPE_VAR,
    NECRO_TYPE_APP,
    NECRO_TYPE_CON,
    NECRO_TYPE_FUN,
    NECRO_TYPE_LIST,
    NECRO_TYPE_LIT,
    NECRO_TYPE_FOR,
    // End
    NECRO_TYPE_END,
} NECRO_TYPE;

typedef struct
{
    NecroVar var;
} NecroTypeVar;

typedef struct
{
    struct NecroType* type1;
    struct NecroType* type2;
} NecroTypeApp;

typedef struct
{
    NecroCon          con;
    struct NecroType* args;
} NecroTypeCon;

typedef struct
{
    struct NecroType* type1;
    struct NecroType* type2;
} NecroTypeFun;

typedef struct
{
    NecroVar          var;
    struct NecroType* type;
} NecroTypeFor;

typedef struct
{
    NecroSymbol symbol;
    NecroID     id;
} NecroTypeLit;

typedef struct
{
    struct NecroType* item;
    struct NecroType* next;
} NecroTypeList;

typedef struct NecroType
{
    union
    {
        NecroTypeVar  var;
        NecroTypeApp  app;
        NecroTypeCon  con;
        NecroTypeFun  fun;
        NecroTypeFor  for_all;
        NecroTypeLit  lit;
        NecroTypeList list;
    };
    NECRO_TYPE     type;
    NecroSourceLoc source_loc;
} NecroType;

typedef struct
{
    NecroPagedArena arena;
    NecroIntern*    intern;
    NecroError      error;
} NecroInfer;

NecroInfer necro_create_infer(NecroIntern* intern);
void       necro_destroy_infer(NecroInfer* infer);

// API
void       necro_test_infer();

#endif // TYPE_H
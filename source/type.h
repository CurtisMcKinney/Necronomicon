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

//=====================================================
//  Var / Con
//=====================================================
typedef struct
{
    // NecroSymbol symbol;
    NecroID     id;
} NecroVar;

typedef struct
{
    NecroSymbol symbol;
    NecroID     id;
} NecroCon;


//=====================================================
// NecorType
//=====================================================
struct NecroType;
typedef enum
{
    NECRO_TYPE_VAR,
    NECRO_TYPE_APP,
    NECRO_TYPE_CON,
    NECRO_TYPE_FUN,
    NECRO_TYPE_LIST,
    NECRO_TYPE_LIT,
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
    size_t            arity;
} NecroTypeCon;

typedef struct
{
    struct NecroType* type1;
    struct NecroType* type2;
} NecroTypeFun;

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
        NecroTypeList list;
    };
    NECRO_TYPE     type;
    NecroSourceLoc source_loc;
} NecroType;

//=====================================================
// NecroTypeScheme - i.e. forall
//=====================================================
struct NecroTypeScheme;
typedef struct
{
    NecroVar                var;
    struct NecroTypeScheme* scheme;
} NecroForAll;

typedef enum
{
    NECRO_TYPE_SCHEME_FOR_ALL,
    NECRO_TYPE_SCHEME_TERM,
} NECRO_TYPE_SCHEME_TYPE;

typedef struct NecroTypeScheme
{
    union
    {
        NecroForAll for_all;
        NecroType   term;
    };
    NECRO_TYPE_SCHEME_TYPE type;
} NecroTypeScheme;

//=====================================================
// Gamma - i.e. Assumptions...Trying this the slow way first, for correctness sake
// TODO: Make faster
//=====================================================
typedef struct NecroGamma
{
    struct NecroGamma* next;
    NecroVar           key;
    NecroTypeScheme*   scheme;
} NecroGamma;

//=====================================================
// Infer
//=====================================================
typedef struct
{
    NecroPagedArena arena;
    NecroIntern*    intern;
    NecroError      error;
    size_t          highest_id;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer necro_create_infer(NecroIntern* intern);
void       necro_destroy_infer(NecroInfer* infer);

// API
void       necro_test_infer();

#endif // TYPE_H
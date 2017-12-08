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
    NECRO_TYPE_FOR,
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

typedef struct
{
    NecroVar          var;
    struct NecroType* type;
} NecroTypeForAll;

typedef struct NecroType
{
    union
    {
        NecroTypeVar    var;
        NecroTypeApp    app;
        NecroTypeCon    con;
        NecroTypeFun    fun;
        NecroTypeList   list;
        NecroTypeForAll for_all;
    };
    NECRO_TYPE     type;
    NecroSourceLoc source_loc;
} NecroType;

// //=====================================================
// // NecroTypeScheme - i.e. forall
// //=====================================================
// struct NecroTypeScheme;
// typedef struct
// {
//     NecroVar                var;
//     struct NecroTypeScheme* scheme;
// } NecroForAll;

// typedef enum
// {
//     NECRO_TYPE_SCHEME_FOR_ALL,
//     NECRO_TYPE_SCHEME_TERM,
// } NECRO_TYPE_SCHEME_TYPE;

// typedef struct NecroTypeScheme
// {
//     union
//     {
//         NecroForAll for_all;
//         NecroType   term;
//     };
//     NECRO_TYPE_SCHEME_TYPE type;
// } NecroTypeScheme;

// //=====================================================
// // NecroSub
// //=====================================================
// typedef struct NecroSub
// {
//     NecroType*        type;
//     NecroID           name;
//     struct NecroSub*  next;
// } NecroSub;

// //=====================================================
// // Gamma - Assumptions
// //=====================================================
// typedef struct NecroGamma
// {
//     struct NecroGamma* next;
//     NecroVar           var;
//     NecroTypeScheme*   scheme;
// } NecroGamma;

//=====================================================
// NecroTypeEnv
//=====================================================
typedef struct
{
    NecroType** data;
    size_t      capacity;
    size_t      count;
} NecroTypeEnv;

//=====================================================
// Infer
//=====================================================
typedef struct
{
    NecroTypeEnv    env;
    NecroPagedArena arena;
    NecroIntern*    intern;
    NecroError      error;
    size_t          highest_id;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer       necro_create_infer(NecroIntern* intern);
void             necro_destroy_infer(NecroInfer* infer);

void             necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*       necro_inst(NecroInfer* infer, NecroType* poly_type);
NecroType*       necro_gen(NecroInfer* infer, NecroType* type);

NecroType*       necro_env_get(NecroInfer* infer, NecroVar var);
void             necro_env_set(NecroInfer* infer, NecroVar var, NecroType* type);

char*            necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length);
const char*      necro_id_as_character_string(NecroInfer* infer, NecroID id);
bool             necro_check_and_print_type_error(NecroInfer* infer);
void             necro_test_type();

#endif // TYPE_H
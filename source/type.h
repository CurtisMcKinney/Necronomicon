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

typedef struct
{
    NecroSymbol int_symbol;
    NecroSymbol add_int_symbol;
    NecroSymbol sub_int_symbol;
    NecroSymbol mul_int_symbol;
    NecroSymbol div_int_symbol;
    NecroSymbol float_symbol;
    NecroSymbol audio_symbol;
    // NecroSymbol string_symbol;
    NecroSymbol char_symbol;
    NecroSymbol bool_symbol;
} NecroPrimSymbols;

//=====================================================
// NecroTypeEnv
//=====================================================
typedef struct
{
    NecroType** data;
    size_t      capacity;
} NecroTypeEnv;

//=====================================================
// Infer
//=====================================================
typedef struct
{
    NecroPrimSymbols prim_symbols;
    NecroTypeEnv     env;
    NecroPagedArena  arena;
    NecroIntern*     intern;
    NecroError       error;
    size_t           highest_id;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer       necro_create_infer(NecroIntern* intern);
void             necro_destroy_infer(NecroInfer* infer);
void             necro_reset_infer(NecroInfer* infer);

void             necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*       necro_inst(NecroInfer* infer, NecroType* poly_type);
NecroType*       necro_gen(NecroInfer* infer, NecroType* type);
NecroType*       necro_new_name(NecroInfer* infer);
// NecroType*       necro_abs(NecroInfer* infer, NecroType* arg_type, NecroType* result_type);

NecroType*       necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args, size_t arity);
NecroType*       necro_create_type_fun(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*       necro_make_bin_op_type(NecroInfer* infer, NecroAST_BinOpType bin_op_type);
NecroType*       necro_make_float_type(NecroInfer* infer);
NecroType*       necro_make_bool_type(NecroInfer* infer);
NecroType*       necro_make_char_type(NecroInfer* infer);

NecroType*       necro_make_int_type(NecroInfer* infer);
NecroType*       necro_make_float_type(NecroInfer* infer);

NecroType*       necro_env_get(NecroInfer* infer, NecroVar var);
void             necro_env_set(NecroInfer* infer, NecroVar var, NecroType* type);

char*            necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length);
const char*      necro_id_as_character_string(NecroInfer* infer, NecroID id);
bool             necro_check_and_print_type_error(NecroInfer* infer);
void             necro_check_type_sanity(NecroInfer* infer, NecroType* type);
void             necro_print_type_test_result(const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern);
void             necro_print_env(NecroInfer* infer);
void             necro_test_type();

#endif // TYPE_H
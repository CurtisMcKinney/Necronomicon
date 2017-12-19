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

// Forward Declarations
struct NecroType;
struct NecroTypeClassEnv;
struct NecroSymTable;
struct NecroTypeClassContext;

//=====================================================
//  Var / Con
//=====================================================
typedef struct
{
    NecroID            id;
    struct NecroScope* scope;
} NecroVar;

typedef struct
{
    NecroSymbol symbol;
    NecroID     id;
} NecroCon;

//=====================================================
// NecorType
//=====================================================
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
    NecroVar                      var;
    bool                          is_rigid;
    bool                          is_type_class_var;
    struct NecroTypeClassContext* context;
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
    bool              is_class;
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
    NecroVar                      var;
    struct NecroTypeClassContext* context;
    struct NecroType*             type;
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
    bool           pre_supplied;
} NecroType;

//=====================================================
// NecroTypeEnv
//=====================================================
typedef struct
{
    NecroType** data;
    size_t      capacity;
} NecroTypeEnv;

//=====================================================
// PrimTypes
//=====================================================
typedef struct
{
    NecroCon two;
    NecroCon three;
    NecroCon four;
    NecroCon five;
    NecroCon six;
    NecroCon seven;
    NecroCon eight;
    NecroCon nine;
    NecroCon ten;
} NecroTupleTypes;

typedef struct
{
    NecroCon add_type;
    NecroCon sub_type;
    NecroCon mul_type;
    NecroCon div_type;
    NecroCon mod_type;
    NecroCon eq_type;
    NecroCon not_eq_type;
    NecroCon gt_type;
    NecroCon lt_type;
    NecroCon gte_type;
    NecroCon lte_type;
    NecroCon and_type;
    NecroCon or_type;
    NecroCon double_colon_type;
    NecroCon left_shift_type;
    NecroCon right_shift_type;
    NecroCon pipe_type;
    NecroCon forward_pipe_type;
    NecroCon back_pipe_type;
    NecroCon dot_type;
    NecroCon bind_right_type;
    NecroCon bind_left_type;
    NecroCon double_exclamation_type;
    NecroCon append_type;
} NecroBinOpTypes;

typedef struct
{
    NecroTupleTypes tuple_types;
    NecroBinOpTypes bin_op_types;
    NecroCon        io_type;
    NecroCon        list_type;
    NecroCon        unit_type;
    NecroCon        int_type;
    NecroCon        float_type;
    NecroCon        audio_type;
    NecroCon        char_type;
    NecroCon        bool_type;
} NecroPrimTypes;

//=====================================================
// Infer
//=====================================================
typedef struct
{
    struct NecroSymTable*     symtable;
    NecroPrimTypes            prim_types;
    struct NecroTypeClassEnv* type_class_env;
    NecroTypeEnv              env;
    NecroPagedArena           arena;
    NecroIntern*              intern;
    NecroError                error;
    size_t                    highest_id;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer  necro_create_infer(NecroIntern* intern, struct NecroSymTable* symtable, NecroPrimTypes prim_types, struct NecroTypeClassEnv* type_class_env);
void        necro_destroy_infer(NecroInfer* infer);
void        necro_reset_infer(NecroInfer* infer);
bool        necro_is_infer_error(NecroInfer* infer);

void        necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, struct NecroScope* scope);
NecroType*  necro_inst(NecroInfer* infer, NecroType* poly_type, struct NecroScope* scope);
NecroType*  necro_gen(NecroInfer* infer, NecroType* type, struct NecroScope* scope);
NecroType*  necro_new_name(NecroInfer* infer);
NecroType*  necro_find(NecroInfer* infer, NecroType* type);

NecroType*  necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args, size_t arity);
NecroType*  necro_create_type_fun(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*  necro_create_type_var(NecroInfer* infer, NecroVar var);
NecroType*  necro_create_type_app(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*  necro_create_type_list(NecroInfer* infer, NecroType* item, NecroType* next);
// NecroType*  necro_create_for_all(NecroInfer* infer, NecroVar var, NecroType* type);
NecroType*  necro_create_for_all(NecroInfer* infer, NecroVar var, struct NecroTypeClassContext* context, NecroType* type);

NecroType*  necro_get_bin_op_type(NecroInfer* infer, NecroAST_BinOpType bin_op_type);
NecroType*  necro_make_con_1(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1);
NecroType*  necro_make_con_2(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2);
NecroType*  necro_make_con_3(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3);
NecroType*  necro_make_con_4(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4);
NecroType*  necro_make_con_5(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5);
NecroType*  necro_make_con_6(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6);
NecroType*  necro_make_con_7(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7);
NecroType*  necro_make_con_8(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8);
NecroType*  necro_make_con_9(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9);
NecroType*  necro_make_con_10(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10);
NecroType*  necro_make_tuple_con(NecroInfer* infer, NecroType* types_list);

NecroType*  necro_env_get(NecroInfer* infer, NecroVar var);
void        necro_env_set(NecroInfer* infer, NecroVar var, NecroType* type);

void        necro_print_type_sig(NecroType* type, NecroIntern* intern);
char*       necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length);
const char* necro_id_as_character_string(NecroInfer* infer, NecroID id);
bool        necro_check_and_print_type_error(NecroInfer* infer);
void        necro_check_type_sanity(NecroInfer* infer, NecroType* type);
void        necro_print_type_test_result(const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern);
void        necro_print_env(NecroInfer* infer);
void        necro_test_type();

#endif // TYPE_H
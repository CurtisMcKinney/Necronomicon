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
    NecroSymbol symbol;
    NecroID     id;
} NecroVar;

typedef struct
{
    NecroSymbol symbol;
    NecroID     id;
} NecroCon;

//=====================================================
// Kind
//=====================================================
struct NecroKind;
typedef enum
{
    NECRO_KIND_STAR,
    NECRO_KIND_APP,
    NECRO_KIND_INIT,
} NECRO_KIND;

typedef struct
{
    struct NecroKind* kind1;
    struct NecroKind* kind2;
} NecroKindApp;

typedef struct NecroKind
{
    union
    {
        NecroKindApp app;
    };
    NECRO_KIND kind;
} NecroKind;

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

typedef enum
{
    NECRO_WEIGHT_W,
    NECRO_WEIGHT_1,
    NECRO_WEIGHT_PI
} NECRO_WEIGHT;

typedef struct
{
    NecroVar                      var;
    int32_t                       arity;
    bool                          is_rigid;
    bool                          is_type_class_var;
    struct NecroTypeClassContext* context;
    struct NecroType*             bound;
    struct NecroScope*            scope;
    NECRO_WEIGHT                  weight; // Future linear types usage???
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
    bool              is_linear; // Future linear types usage?
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
    NecroKind*     kind;
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
// Infer
//=====================================================
typedef struct
{
    struct NecroSymTable*     symtable;
    struct NecroPrimTypes*    prim_types;
    struct NecroTypeClassEnv* type_class_env;
    NecroTypeEnv              env;
    NecroPagedArena           arena;
    NecroIntern*              intern;
    NecroError                error;
    size_t                    highest_id;
    NecroKind*                star_kind;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer  necro_create_infer(NecroIntern* intern, struct NecroSymTable* symtable, struct NecroPrimTypes* prim_types, struct NecroTypeClassEnv* type_class_env);
void        necro_destroy_infer(NecroInfer* infer);
void        necro_reset_infer(NecroInfer* infer);
bool        necro_is_infer_error(NecroInfer* infer);

void        necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroType* macro_type, const char* error_preamble);

NecroType*  necro_inst(NecroInfer* infer, NecroType* poly_type, struct NecroScope* scope);
NecroType*  necro_gen(NecroInfer* infer, NecroType* type, struct NecroScope* scope);
NecroType*  necro_new_name(NecroInfer* infer, NecroSourceLoc source_loc);
NecroType*  necro_find(NecroInfer* infer, NecroType* type);
void        necr_bind_type_var(NecroInfer* infer, NecroVar var, NecroType* type);

NecroType*  necro_declare_type(NecroInfer* infer, NecroCon con, size_t arity);
NecroType*  necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args, size_t arity);
NecroType*  necro_create_type_fun(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*  necro_create_type_var(NecroInfer* infer, NecroVar var);
NecroType*  necro_create_type_app(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*  necro_create_type_list(NecroInfer* infer, NecroType* item, NecroType* next);
NecroType*  necro_create_for_all(NecroInfer* infer, NecroVar var, struct NecroTypeClassContext* context, NecroType* type);

NecroType*  necro_get_bin_op_type(NecroInfer* infer, NecroAST_BinOpType bin_op_type);
NecroType*  necro_make_con_1(NecroInfer* infer,  NecroCon con, NecroType* arg1);
NecroType*  necro_make_con_2(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2);
NecroType*  necro_make_con_3(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3);
NecroType*  necro_make_con_4(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4);
NecroType*  necro_make_con_5(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5);
NecroType*  necro_make_con_6(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6);
NecroType*  necro_make_con_7(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7);
NecroType*  necro_make_con_8(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8);
NecroType*  necro_make_con_9(NecroInfer* infer,  NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9);
NecroType*  necro_make_con_10(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10);
NecroType*  necro_make_tuple_con(NecroInfer* infer, NecroType* types_list);

size_t      necro_type_list_count(NecroType* list);

NecroKind*  necro_create_kind_app(NecroInfer* infer, NecroKind* kind1, NecroKind* kind2);
NecroKind*  necro_infer_kind(NecroInfer* infer, NecroType* type, NecroKind* kind_to_match, NecroType* macro_type, const char* error_preamble);
void        necro_unify_kinds(NecroInfer* infer, NecroType* type1, NecroKind** kind1, NecroKind** kind2, NecroType* macro_type, const char* error_preamble);
void        necro_print_kind(NecroKind* kind);
char*       necro_kind_string(NecroInfer* infer, NecroKind* kind);
NecroKind*  necro_create_kind_init(NecroInfer* infer);

char*       necro_kind_string(NecroInfer* infer, NecroKind* kind);
void        necro_print_type_sig(NecroType* type, NecroIntern* intern);
void        necro_print_type_sig_go(NecroType* type, NecroIntern* intern);
char*       necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length);
const char* necro_id_as_character_string(NecroInfer* infer, NecroVar var);
bool        necro_check_and_print_type_error(NecroInfer* infer);
void        necro_print_type_test_result(const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern);
void        necro_print_env(NecroInfer* infer);
void        necro_test_type();

#endif // TYPE_H
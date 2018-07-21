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
struct NecroSymTable;
struct NecroTypeClassContext;
struct NecroLifetime;
struct NecroScopedSymTable;
struct NecroRenamer;

typedef enum
{
    NECRO_TYPE_UNCHECKED,
    NECRO_TYPE_CHECKING,
    NECRO_TYPE_DONE
} NECRO_TYPE_STATUS;

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

inline NecroCon necro_var_to_con(NecroVar var)
{
    return (NecroCon) { .symbol = var.symbol, .id = var.id };
}

inline NecroVar necro_con_to_var(NecroCon con)
{
    return (NecroVar) { .symbol = con.symbol, .id = con.id };
}

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
    // NECRO_TYPE_LIT,
    NECRO_TYPE_FOR,
} NECRO_TYPE;

typedef struct
{
    NecroVar                      var;
    int32_t                       arity;
    bool                          is_rigid;
    bool                          is_type_class_var;
    struct NecroTypeClassContext* context;
    struct NecroType*             bound;
    struct NecroScope*            scope;
    struct NecroType*             gen_bound;
} NecroTypeVar;

typedef struct
{
    struct NecroType* type1;
    struct NecroType* type2;
} NecroTypeApp;

typedef struct
{
    NecroCon              con;
    struct NecroType*     args;
    size_t                arity;
    bool                  is_class;
    struct NecroLifetime* lifetime;
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
    NECRO_TYPE        type;
    NecroSourceLoc    source_loc;
    bool              pre_supplied;
    struct NecroType* type_kind;
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
typedef struct NecroInfer
{
    struct NecroSymTable*       symtable;
    struct NecroScopedSymTable* scoped_symtable;
    struct NecroRenamer*        renamer;
    struct NecroPrimTypes*      prim_types;
    NecroTypeEnv                env;
    NecroSnapshotArena          snapshot_arena;
    NecroPagedArena             arena;
    NecroIntern*                intern;
    NecroError                  error;
    size_t                      highest_id;
    NecroType*                  star_type_kind;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer  necro_create_infer(NecroIntern* intern, struct NecroSymTable* symtable, struct NecroScopedSymTable* scoped_symtable, struct NecroRenamer* renamer, struct NecroPrimTypes* prim_types);
void        necro_destroy_infer(NecroInfer* infer);
void        necro_reset_infer(NecroInfer* infer);
bool        necro_is_infer_error(NecroInfer* infer);
NecroType*  necro_alloc_type(NecroInfer* infer);
void*       necro_infer_error(NecroInfer* infer, const char* error_preamble, NecroType* type, const char* error_message, ...);

void        necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroType* macro_type, const char* error_preamble);
bool        necro_exact_unify(NecroType* type1, NecroType* type2);

NecroType*  necro_inst(NecroInfer* infer, NecroType* poly_type, struct NecroScope* scope);
NecroType*  necro_inst_with_context(NecroInfer* infer, NecroType* type, struct NecroScope* scope, struct NecroTypeClassContext** inst_context);
NecroType*  necro_gen(NecroInfer* infer, NecroType* type, struct NecroScope* scope);
NecroType*  necro_new_name(NecroInfer* infer, NecroSourceLoc source_loc);
NecroType*  necro_find(NecroType* type);
void        necr_bind_type_var(NecroInfer* infer, NecroVar var, NecroType* type);
bool        necro_is_bound_in_scope(NecroInfer* infer, NecroType* type, struct NecroScope* scope);
bool        necro_occurs(NecroInfer* infer, NecroType* type_var, NecroType* type, NecroType* macro_type, const char* error_preamble);
NecroType*  necro_curry_con(NecroInfer* infer, NecroType* con);
size_t      necro_type_arity(NecroType* type);

NecroType*  necro_declare_type(NecroInfer* infer, NecroCon con, size_t arity);
NecroType*  necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args, size_t arity);
NecroType*  necro_create_type_fun(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*  necro_create_type_var(NecroInfer* infer, NecroVar var);
NecroType*  necro_create_type_app(NecroInfer* infer, NecroType* type1, NecroType* type2);
NecroType*  necro_create_type_list(NecroInfer* infer, NecroType* item, NecroType* next);
NecroType*  necro_create_for_all(NecroInfer* infer, NecroVar var, struct NecroTypeClassContext* context, NecroType* type);
NecroType*  necro_duplicate_type(NecroInfer* infer, NecroType* type);
size_t      necro_type_hash(NecroType* type);

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

char*       necro_type_string(NecroInfer* infer, NecroType* type);
void        necro_print_type_sig(NecroType* type, NecroIntern* intern);
void        necro_print_type_sig_go(NecroType* type, NecroIntern* intern);
char*       necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length);
const char* necro_id_as_character_string(NecroIntern* infer, NecroVar var);
NecroSymbol necro_id_as_symbol(NecroIntern* intern, NecroVar var);
bool        necro_check_and_print_type_error(NecroInfer* infer);
void        necro_print_type_test_result(const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern);
void        necro_print_env(NecroInfer* infer);
void        necro_test_type();

#endif // TYPE_H
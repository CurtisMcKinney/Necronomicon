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
struct NecroTypeClassContext;
struct NecroLifetime;
struct NecroScopedSymTable;
struct NecroRenamer;
struct NecroBase;

typedef enum
{
    NECRO_STATE_CONSTANT  = 0,
    NECRO_STATE_POLY      = 1,
    NECRO_STATE_POINTWISE = 2,
    NECRO_STATE_STATEFUL  = 3,
} NECRO_STATE_TYPE; // Used for state analysis and in necromachine

// TODO: Remove!
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
    NECRO_TYPE_FOR,
} NECRO_TYPE;

typedef struct
{
    // NecroVar                      var;
    NecroAstSymbol                var_symbol;
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
    // NecroCon              con;
    NecroAstSymbol     con_symbol;
    struct NecroType*  args;
    size_t             arity;
    bool               is_class;
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
    // NecroVar                      var;
    NecroAstSymbol                var_symbol;
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
    struct NecroType* kind;
} NecroType;

//=====================================================
// Infer
//=====================================================
typedef struct NecroInfer
{
    struct NecroScopedSymTable* scoped_symtable;
    struct NecroBase*           base;
    NecroPagedArena*            arena;
    NecroError                  error;     // TODO: Nix
    NecroSnapshotArena          snapshot_arena;
    NecroIntern*                intern;
} NecroInfer;

//=====================================================
// API
//=====================================================
NecroInfer  necro_infer_empty();
NecroInfer  necro_infer_create(NecroPagedArena* arena, NecroIntern* intern, struct NecroScopedSymTable* scoped_symtable, struct NecroBase* base);
void        necro_infer_destroy(NecroInfer* infer);
bool        necro_is_infer_error(NecroInfer* infer);
void*       necro_infer_error(NecroInfer* infer, const char* error_preamble, NecroType* type, const char* error_message, ...);

void        necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroType* macro_type, const char* error_preamble);
bool        necro_exact_unify(NecroType* type1, NecroType* type2);

NecroType*  necro_inst(NecroInfer* infer, NecroType* poly_type, struct NecroScope* scope);
NecroType*  necro_inst_with_context(NecroInfer* infer, NecroType* type, struct NecroScope* scope, struct NecroTypeClassContext** inst_context);
NecroType*  necro_gen(NecroInfer* infer, NecroType* type, struct NecroScope* scope);
NecroType*  necro_new_name(NecroInfer* infer);
NecroType*  necro_find(NecroType* type);
bool        necro_is_bound_in_scope(NecroType* type, struct NecroScope* scope);
bool        necro_occurs(NecroAstSymbol var_symbol, NecroType* type, NecroType* macro_type, const char* error_preamble);
NecroType*  necro_curry_con(NecroPagedArena* arena, NecroType* con);
size_t      necro_type_arity(NecroType* type);

NecroType*  necro_type_duplicate(NecroPagedArena* arena, NecroType* type);
size_t      necro_type_hash(NecroType* type);

NecroType*  necro_type_alloc(NecroPagedArena* arena);
NecroType*  necro_type_var_create(NecroPagedArena* arena, NecroAstSymbol var_symbol);
NecroType*  necro_type_declare(NecroPagedArena* arena, NecroAstSymbol con_symbol, size_t arity);
NecroType*  necro_type_con_create(NecroPagedArena* arena, NecroAstSymbol con_symbol, NecroType* args, size_t arity);
NecroType*  necro_type_fn_create(NecroPagedArena* arena, NecroType* type1, NecroType* type2);
NecroType*  necro_type_app_create(NecroPagedArena* arena, NecroType* type1, NecroType* type2);
NecroType*  necro_type_list_create(NecroPagedArena* arena, NecroType* item, NecroType* next);
NecroType*  necro_type_for_all_create(NecroPagedArena* arena, NecroAstSymbol var_symbol, struct NecroTypeClassContext* context, NecroType* type);

NecroType*  necro_type_con1_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1);
NecroType*  necro_type_con2_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2);
NecroType*  necro_type_con3_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3);
NecroType*  necro_type_con4_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4);
NecroType*  necro_type_con5_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5);
NecroType*  necro_type_con6_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6);
NecroType*  necro_type_con7_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7);
NecroType*  necro_type_con8_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8);
NecroType*  necro_type_con9_create(NecroPagedArena* arena,  NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9);
NecroType*  necro_type_con10_create(NecroPagedArena* arena, NecroAstSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10);
NecroType*  necro_type_tuple_con_create(NecroInfer* infer, NecroType* types_list);

size_t      necro_type_list_count(NecroType* list);

// char*       necro_type_string(NecroInfer* infer, NecroType* type);
void        necro_type_sig_print(NecroType* type);
void        necro_type_sig_print_go(NecroType* type);
// char*       necro_type_sig_snprintf(NecroType* type, char* buffer, const size_t buffer_length);
// bool        necro_check_and_print_type_error(NecroInfer* infer);
void        necro_type_test();

#endif // TYPE_H
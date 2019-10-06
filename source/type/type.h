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
struct NecroScopedSymTable;
struct NecroRenamer;
struct NecroBase;
struct NecroConstraint;
struct NecroFreeVars;
struct NecroConstraintEnv;

///////////////////////////////////////////////////////
// NecroType
///////////////////////////////////////////////////////
typedef enum
{
    // Main
    NECRO_TYPE_VAR,
    NECRO_TYPE_APP,
    NECRO_TYPE_CON,
    NECRO_TYPE_FUN,
    NECRO_TYPE_LIST,
    NECRO_TYPE_FOR,

    // Other
    NECRO_TYPE_NAT,
    NECRO_TYPE_SYM,
} NECRO_TYPE;

typedef struct
{
    NecroAstSymbol*               var_symbol;
    struct NecroTypeClassContext* context;
    struct NecroType*             bound;
    struct NecroScope*            scope;
    int32_t                       arity;
    bool                          is_rigid;
    NECRO_TYPE_ORDER              order;
} NecroTypeVar;

typedef struct
{
    struct NecroType*          type1;
    struct NecroType*          type2;
} NecroTypeApp;

typedef struct
{
    NecroAstSymbol*   con_symbol;
    struct NecroType* args;
} NecroTypeCon;

typedef struct
{
    struct NecroType*     type1;
    struct NecroType*     type2;
    struct NecroFreeVars* free_vars;
} NecroTypeFun;

typedef struct
{
    struct NecroType* item;
    struct NecroType* next;
} NecroTypeList;

typedef struct
{
    NecroAstSymbol*   var_symbol;
    struct NecroType* type;
} NecroTypeForAll;

typedef struct
{
    size_t value;
} NecroTypeNat;

typedef struct
{
    NecroSymbol value;
} NecroTypeSym;

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
        NecroTypeNat    nat;
        NecroTypeSym    sym;
    };
    NECRO_TYPE                  type;
    bool                        pre_supplied;
    struct NecroType*           kind;
    struct NecroType*           ownership;
    struct NecroConstraintList* constraints;
    size_t                      hash;
} NecroType;

typedef struct NecroInstSub
{
    NecroAstSymbol*      var_to_replace;
    NecroType*           new_name;
    struct NecroInstSub* next;
} NecroInstSub;

//=====================================================
// API
//=====================================================
NecroResult(NecroType) necro_type_unify_with_info(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType) necro_type_unify_with_full_info(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroType* macro_type1, NecroType* macro_type2);
NecroResult(NecroType) necro_type_unify(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope);
NecroResult(NecroType) necro_type_occurs(NecroAstSymbol* var_symbol, NecroType* type);
NecroResult(NecroType) necro_type_instantiate(NecroPagedArena* arenas, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, struct NecroScope* scope);
NecroResult(NecroType) necro_type_replace_with_subs_deep_copy(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, NecroInstSub* subs);
NecroInstSub*          necro_type_union_subs(NecroInstSub* subs1, NecroInstSub* subs2);
NecroInstSub*          necro_type_deep_copy_subs(NecroPagedArena* arena, NecroInstSub* subs);
NecroInstSub*          necro_type_filter_and_deep_copy_subs(NecroPagedArena* arena, NecroInstSub* subs, NecroAstSymbol* var_to_replace, NecroType* new_name);
NecroResult(NecroType) necro_type_instantiate_with_subs(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, struct NecroScope* scope, NecroInstSub** subs);
NecroResult(NecroType) necro_type_generalize(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, struct NecroScope* scope);
NecroResult(NecroType) necro_type_set_zero_order(NecroType* type, const NecroSourceLoc* source_loc, const NecroSourceLoc* end_loc);
bool                   necro_type_is_higher_order_function(const NecroType* type, size_t arity);

bool                   necro_type_exact_unify(NecroType* type1, NecroType* type2);
NecroType*             necro_type_find(NecroType* type);
const NecroType*       necro_type_find_const(const NecroType* type);
bool                   necro_type_is_bound_in_scope(NecroType* type, struct NecroScope* scope);
NecroResult(bool)      necro_type_is_unambiguous_polymorphic(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, const NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)      necro_type_ambiguous_type_variable_check(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, const NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
bool                   necro_type_is_polymorphic(const NecroType* type);
bool                   necro_type_is_polymorphic_ignoring_ownership(const struct NecroBase* base, const NecroType* type);
bool                   necro_type_is_copy_type(const NecroType* type);
size_t                 necro_type_arity(NecroType* type);
size_t                 necro_type_hash(NecroType* type);
size_t                 necro_type_list_count(NecroType* list);
NecroType*             necro_type_strip_for_all(NecroType* type);
NecroType*             necro_type_get_fully_applied_fun_type(NecroType* type);
const NecroType*       necro_type_get_fully_applied_fun_type_const(const NecroType* type);
NecroType*             necro_type_uncurry_app(NecroPagedArena* arena, struct NecroBase* base, NecroType* app);
bool                   necro_type_is_inhabited(struct NecroBase* base, const NecroType* type);
void                   necro_type_assert_no_rigid_variables(const NecroType* type);

NecroType*             necro_type_alloc(NecroPagedArena* arena);
NecroType*             necro_type_fresh_var(NecroPagedArena* arena, struct NecroScope* scope);
NecroType*             necro_type_var_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol, struct NecroScope* scope);
NecroType*             necro_type_con_create(NecroPagedArena* arena, NecroAstSymbol* con_symbol, NecroType* args);
NecroType*             necro_type_fn_create(NecroPagedArena* arena, NecroType* type1, NecroType* type2);
NecroType*             necro_type_app_create(NecroPagedArena* arena, NecroType* type1, NecroType* type2);
NecroType*             necro_type_list_create(NecroPagedArena* arena, NecroType* item, NecroType* next);
NecroType*             necro_type_for_all_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol, NecroType* type);
NecroType*             necro_type_nat_create(NecroPagedArena* arena, size_t value);
NecroType*             necro_type_sym_create(NecroPagedArena* arena, NecroSymbol value);

NecroType*             necro_type_con1_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1);
NecroType*             necro_type_con2_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2);
NecroType*             necro_type_con3_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3);
NecroType*             necro_type_con4_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4);
NecroType*             necro_type_con5_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5);
NecroType*             necro_type_con6_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6);
NecroType*             necro_type_con7_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7);
NecroType*             necro_type_con8_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8);
NecroType*             necro_type_con9_create(NecroPagedArena* arena,  NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9);
NecroType*             necro_type_con10_create(NecroPagedArena* arena, NecroAstSymbol* con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10);
NecroType*             necro_type_tuple_con_create(NecroPagedArena* arena, struct NecroBase* base, NecroType* types_list);
NecroType*             necro_type_unboxed_tuple_con_create(NecroPagedArena* arena, struct NecroBase* base, NecroType* types_list);

NecroType*             necro_type_ownership_fresh_var(NecroPagedArena* arena, struct NecroBase* base, struct NecroScope* scope);
NecroResult(NecroType) necro_type_infer_and_unify_ownership_for_two_types(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope);
NecroResult(NecroType) necro_type_ownership_infer_from_type(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, struct NecroScope* scope);
NecroResult(NecroType) necro_type_ownership_infer_from_sig(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, struct NecroScope* scope);
NecroResult(NecroType) necro_type_ownership_unify(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, NecroType* ownership1, NecroType* ownership2, struct NecroScope* scope);
NecroResult(NecroType) necro_type_ownership_unify_with_info(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* ownership1, NecroType* ownership2, struct NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
void                   necro_type_ownership_bind_uvar_to_with_queue_push_front(NecroPagedArena* arena, struct NecroConstraintEnv* con_env, NecroType* uvar_to_bind, NecroType* utype_to_bind_to);

void                   necro_type_fprint(FILE* stream, const NecroType* type);
void                   necro_type_print(const NecroType* type);
size_t                 necro_type_mangled_string_length(const NecroType* type);
size_t                 necro_type_mangled_sprintf(char* buffer, size_t offset, const NecroType* type);

NecroInstSub*          necro_create_inst_sub_manual(NecroPagedArena* arena, NecroAstSymbol* var_to_replace, NecroType* new_type, NecroInstSub* next);
NecroType*             necro_type_deep_copy(NecroPagedArena* arena, NecroType* type);

#endif // TYPE_H
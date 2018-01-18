/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "lifetime.h"

NecroLifetime* necro_create_constant_lifetime(NecroInfer* infer)
{
    NecroSymbol     constant_symbol = necro_intern_string(infer->intern, "\'constant");
    NecroSymbolInfo constant_info   = necro_create_initial_symbol_info(constant_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         constant_id     = necro_symtable_insert(infer->symtable, constant_info);
    NecroCon        constant_con    = (NecroCon) { .id = constant_id, .symbol = constant_symbol };
    NecroType*      constant_type   = necro_alloc_type(infer);
    constant_type->type             = NECRO_TYPE_CON;
    constant_type->con              = (NecroTypeCon)
    {
        .con      = constant_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    constant_type->type_kind    = NULL;
    constant_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, constant_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, constant_con.id)->type = constant_type;
    return constant_type;
}

NecroLifetime* necro_create_static_lifetime(NecroInfer* infer)
{
    NecroSymbol     static_symbol = necro_intern_string(infer->intern, "\'static");
    NecroSymbolInfo static_info   = necro_create_initial_symbol_info(static_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         static_id     = necro_symtable_insert(infer->symtable, static_info);
    NecroCon        static_con    = (NecroCon) { .id = static_id, .symbol = static_symbol };
    NecroType*      static_type   = necro_alloc_type(infer);
    static_type->type             = NECRO_TYPE_CON;
    static_type->con              = (NecroTypeCon)
    {
        .con      = static_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    static_type->type_kind    = NULL;
    static_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, static_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, static_con.id)->type = static_type;
    return static_type;
}

NecroLifetime* necro_create_never_lifetime(NecroInfer* infer)
{
    NecroSymbol     never_symbol = necro_intern_string(infer->intern, "\'never");
    NecroSymbolInfo never_info   = necro_create_initial_symbol_info(never_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         never_id     = necro_symtable_insert(infer->symtable, never_info);
    NecroCon        never_con    = (NecroCon) { .id = never_id, .symbol = never_symbol };
    NecroType*      never_type   = necro_alloc_type(infer);
    never_type->type             = NECRO_TYPE_CON;
    never_type->con              = (NecroTypeCon)
    {
        .con      = never_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    never_type->type_kind    = NULL;
    never_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, never_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, never_con.id)->type = never_type;
    return never_type;
}

void necro_infer_lifetime(NecroInfer* infer, NecroASTNode* ast)
{
}

void necro_rigid_lifetime_variable_error(NecroInfer* infer, NecroVar type_var, NecroLifetime* type, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    const char* type_name = NULL;
    if (type->type == NECRO_TYPE_CON)
        type_name = necro_intern_get_string(infer->intern, type->con.con.symbol);
    else if (type->type == NECRO_TYPE_APP)
        type_name = "TypeApp";
    else if (type->type == NECRO_TYPE_FUN)
        type_name = "(->)";
    else if (type->type == NECRO_TYPE_VAR)
        type_name = necro_id_as_character_string(infer, type->var.var);
    else
        assert(false);
    const char* var_name = necro_id_as_character_string(infer, type_var);
    necro_infer_error(infer, error_preamble, macro_type, "Couldn't match lifetime \'%s\' with lifetime \'%s\'.\n\'%s\' is a lifetime kind variable bound by a type signature.", var_name, type_name, var_name);
}

inline void necro_instantiate_lifetime_var(NecroInfer* infer, NecroTypeVar* lifetime_var, NecroLifetime* lifetime, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    necr_bind_type_var(infer, lifetime_var->var, lifetime);
}

void necro_unify_var_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(lifetime1 != NULL);
    assert(lifetime1->type == NECRO_TYPE_VAR);
    if (lifetime2 == NULL)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Mismatched arities");
        return;
    }
    if (lifetime1 == lifetime2)
        return;
    switch (lifetime2->type)
    {
    case NECRO_TYPE_VAR:
        // necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        // necro_kind_unify(infer, type1->type_kind, type2->type_kind, scope, macro_type, error_preamble);
        if (lifetime1->var.var.id.id == lifetime2->var.var.id.id)
            return;
        else if (lifetime1->var.is_rigid && lifetime2->var.is_rigid)
            necro_rigid_lifetime_variable_error(infer, lifetime1->var.var, lifetime2, macro_type, error_preamble);
        else if (necro_occurs(infer, lifetime1, lifetime2, macro_type, error_preamble))
            return;
        else if (lifetime1->var.is_rigid)
            necro_instantiate_lifetime_var(infer, &lifetime2->var, lifetime1, macro_type, error_preamble);
        else if (lifetime2->var.is_rigid)
            necro_instantiate_lifetime_var(infer, &lifetime1->var, lifetime2, macro_type, error_preamble);
        else if (necro_is_bound_in_scope(infer, lifetime1, scope))
            necro_instantiate_lifetime_var(infer, &lifetime2->var, lifetime1, macro_type, error_preamble);
        else
            necro_instantiate_lifetime_var(infer, &lifetime1->var, lifetime2, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (lifetime1->var.is_rigid)
        {
            necro_rigid_lifetime_variable_error(infer, lifetime1->var.var, lifetime2, macro_type, error_preamble);
            return;
        }
        if (necro_occurs(infer, lifetime1, lifetime2, macro_type, error_preamble))
        {
            return;
        }
        necro_instantiate_lifetime_var(infer, &lifetime1->var, lifetime2, macro_type, error_preamble);
        // necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        // necro_kind_unify(infer, type1->type_kind, type2->type_kind, scope, macro_type, error_preamble);
        return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to lifetime unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to lifetime unify LifetimeVar with type args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent lifetime type (lifetime1: %d, lifetime2: %s) type found in necro_unify_var_lifetimes.", lifetime1->type, lifetime2->type); return;
    }
}

void necro_unify_con_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    // if (necro_is_infer_error(infer))
    //     return;
    // assert(type1 != NULL);
    // assert(type1->type == NECRO_TYPE_CON);
    // assert(type2 != NULL);
    // switch (type2->type)
    // {
    // case NECRO_TYPE_VAR:
    //     if (type2->var.is_rigid)
    //         necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
    //     else if (necro_occurs(infer, type2, type1, macro_type, error_preamble))
    //         return;
    //     else
    //         necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
    //     return;
    // case NECRO_TYPE_CON:
    //     if (type1->con.con.symbol.id != type2->con.con.symbol.id)
    //     {
    //         necro_infer_error(infer, error_preamble, type1, "Attempting to unify two different types, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
    //     }
    //     else
    //     {
    //         type1 = type1->con.args;
    //         type2 = type2->con.args;
    //         while (type1 != NULL && type2 != NULL)
    //         {
    //             if (type1 == NULL || type2 == NULL)
    //             {
    //                 necro_infer_error(infer, error_preamble, type1, "Mismatched arities, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
    //                 return;
    //             }
    //             assert(type1->type == NECRO_TYPE_LIST);
    //             assert(type2->type == NECRO_TYPE_LIST);
    //             necro_unify(infer, type1->list.item, type2->list.item, scope, macro_type, error_preamble);
    //             type1 = type1->list.next;
    //             type2 = type2->list.next;
    //         }
    //     }
    //     return;
    // case NECRO_TYPE_APP:
    // {
    //     NecroType* uncurried_con = necro_curry_con(infer, type1);
    //     if (uncurried_con == NULL)
    //     {
    //         necro_infer_error(infer, error_preamble, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol));
    //     }
    //     else
    //     {
    //         necro_unify(infer, uncurried_con, type2, scope, macro_type, error_preamble);
    //     }
    //     // necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
    //     // necro_kind_unify(infer, type1->type_kind, type2->type_kind, scope, macro_type, error_preamble);
    //     return;
    // }
    // case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    // case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    // case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify polytype."); return;
    // default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    // }
}

// Unifies towards longer lifetime
// Will need a second version that unifies towards minimum lifetime for scope introducing combinators (or a special rule enforced somehow...)
void necro_unify_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(lifetime1 != NULL);
    assert(lifetime2 != NULL);
    lifetime1 = necro_find(infer, lifetime1);
    lifetime2 = necro_find(infer, lifetime2);
    if (lifetime1 == lifetime2)
        return;
    switch (lifetime1->type)
    {
    case NECRO_TYPE_VAR:  necro_unify_var_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:  necro_unify_con_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;

    case NECRO_TYPE_FOR:
    {
        while (lifetime1->type == NECRO_TYPE_FOR)
            lifetime1 = lifetime1->for_all.type;
        necro_unify_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble);
        return;
    }

    // Do we need this?
    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime app in necro_unify_lifetimes"); return;
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime function in necro_unify_lifetimes"); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime args list in necro_unify_lifetimes"); return;
    default: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent lifetime node type (lifetime1: %d, lifetime2: %s) found in necro_unify_lifetimes.", lifetime1->type, lifetime2->type); return;
    }
}
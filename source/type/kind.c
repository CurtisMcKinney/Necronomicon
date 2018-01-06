/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "prim.h"
#include "type_class.h"
#include "kind.h"

NecroTypeKind* necro_create_star_kind(NecroInfer* infer)
{
    NecroSymbol     star_symbol = necro_intern_string(infer->intern, "*");
    NecroSymbolInfo star_info   = necro_create_initial_symbol_info(star_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         star_id     = necro_symtable_insert(infer->symtable, star_info);
    NecroCon        star_con    = (NecroCon) { .id = star_id, .symbol = star_symbol };
    NecroType*      star_type   = necro_alloc_type(infer);
    star_type->type             = NECRO_TYPE_CON;
    star_type->con              = (NecroTypeCon)
    {
        .con      = star_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    star_type->kind         = NULL;
    star_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, star_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, star_con.id)->type = star_type;
    return star_type;
}

void necro_infer_kinds_for_data_declaration(NecroInfer* infer, NecroASTNode* ast)
{
    assert(infer != NULL);
}


inline void necro_kind_unify_var(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(false);
}

inline void necro_kind_unify_app(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(false);
}

inline void necro_kind_unify_fun(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    // assert(type1 != NULL);
    // assert(type1->type == NECRO_TYPE_FUN);
    // assert(type2 != NULL);
    // switch (type2->type)
    // {
    // case NECRO_TYPE_VAR:
    //     necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
    //     if (type2->var.is_rigid)
    //         necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
    //     else if (necro_occurs(infer, type2, type1, macro_type, error_preamble))
    //         return;
    //     else
    //         necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
    //     return;
    // case NECRO_TYPE_FUN:  necro_unify(infer, type1->fun.type1, type2->fun.type1, scope, macro_type, error_preamble); necro_unify(infer, type1->fun.type2, type2->fun.type2, scope, macro_type, error_preamble); return;
    // case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify (->) with TypeApp."); return;
    // case NECRO_TYPE_CON:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify (->) with TypeCon (%s)", necro_intern_get_string(infer->intern, type2->con.con.symbol)); return;
    // case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify (->) with type args list."); return;
    // default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    // }
}

inline void necro_kind_unify_con(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    // assert(type1 != NULL);
    // assert(type1->type == NECRO_TYPE_CON);
    // assert(type2 != NULL);
    // switch (type2->type)
    // {
    // case NECRO_TYPE_VAR:
    //     necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
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
    //     // else if (type1->con.arity != type2->con.arity)
    //     // {
    //     //     necro_infer_error(infer, error_preamble, type1, "Mismatched arities, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
    //     // }
    //     else
    //     {
    //         necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
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
    //             necro_unify_kinds(infer, type2, &type2->list.item->kind, &type1->list.item->kind, macro_type, error_preamble);
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
    //     necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
    //     return;
    // }
    // case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    // case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    // case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify polytype."); return;
    // default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    // }
}

void necro_kind_unify(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    // assert(infer != NULL);
    // assert(type1 != NULL);
    // assert(type2 != NULL);
    // necro_infer_kind(infer, type1, NULL, macro_type, error_preamble);
    // necro_infer_kind(infer, type2, NULL, macro_type, error_preamble);
    // type1 = necro_find(infer, type1);
    // type2 = necro_find(infer, type2);
    // if (type1 == type2)
    //     return;
    // necro_infer_kind(infer, type1, NULL, macro_type, error_preamble);
    // necro_infer_kind(infer, type2, NULL, macro_type, error_preamble);
    // switch (type1->type)
    // {
    // case NECRO_TYPE_VAR:  necro_unify_var(infer, type1, type2, scope, macro_type, error_preamble); return;
    // case NECRO_TYPE_APP:  necro_unify_app(infer, type1, type2, scope, macro_type, error_preamble); return;
    // case NECRO_TYPE_FUN:  necro_unify_fun(infer, type1, type2, scope, macro_type, error_preamble); return;
    // case NECRO_TYPE_CON:  necro_unify_con(infer, type1, type2, scope, macro_type, error_preamble); return;
    // case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, type1, "Compiler bug: Found Type args list in necro_unify"); return;
    // case NECRO_TYPE_FOR:
    // {
    //     while (type1->type == NECRO_TYPE_FOR)
    //         type1 = type1->for_all.type;
    //     necro_unify(infer, type1, type2, scope, macro_type, error_preamble);
    //     return;
    // }
    // default: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    // }
}
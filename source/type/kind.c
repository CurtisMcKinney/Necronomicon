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

void necro_rigid_kind_variable_error(NecroInfer* infer, NecroVar type_var, NecroTypeKind* type, NecroType* macro_type, const char* error_preamble)
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
    necro_infer_error(infer, error_preamble, macro_type, "Couldn't match kind \'%s\' with kind \'%s\'.\n\'%s\' is a rigid kind variable bound by a type signature.", var_name, type_name, var_name);
}

inline void necro_instantiate_kind_var(NecroInfer* infer, NecroTypeVar* kind_var, NecroTypeKind* kind, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    necr_bind_type_var(infer, kind_var->var, kind);
}

NecroTypeKind* necro_create_star_kind(NecroInfer* infer)
{
    NecroSymbol     star_symbol = necro_intern_string(infer->intern, "Type");
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
    // star_type->kind         = NULL;
    star_type->type_kind    = NULL;
    star_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, star_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, star_con.id)->type = star_type;
    return star_type;
}

NecroTypeKind* necro_create_question_kind(NecroInfer* infer)
{
    NecroSymbol     question_symbol = necro_intern_string(infer->intern, "?");
    NecroSymbolInfo question_info   = necro_create_initial_symbol_info(question_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         question_id     = necro_symtable_insert(infer->symtable, question_info);
    NecroCon        question_con    = (NecroCon) { .id = question_id, .symbol = question_symbol };
    NecroType*      question_type   = necro_alloc_type(infer);
    question_type->type             = NECRO_TYPE_CON;
    question_type->con              = (NecroTypeCon)
    {
        .con      = question_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    // question_type->kind         = NULL;
    question_type->type_kind    = NULL;
    question_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, question_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, question_con.id)->type = question_type;
    return question_type;
}

void necro_infer_kinds_for_data_declaration(NecroInfer* infer, NecroASTNode* ast)
{
    assert(infer != NULL);
}


inline void necro_kind_unify_var(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_VAR);
    if (kind2 == NULL)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Kind error: Mismatched kind arities");
        return;
    }
    if (kind1 == kind2)
        return;
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind1->var.var.id.id == kind2->var.var.id.id)
            return;
        else if (kind1->var.is_rigid && kind2->var.is_rigid)
            necro_rigid_kind_variable_error(infer, kind1->var.var, kind2, macro_type, error_preamble);
        else if (necro_occurs(infer, kind1, kind2, macro_type, error_preamble))
            return;
        else if (kind1->var.is_rigid)
            necro_instantiate_kind_var(infer, &kind2->var, kind1, macro_type, error_preamble);
        else if (kind2->var.is_rigid)
            necro_instantiate_kind_var(infer, &kind1->var, kind2, macro_type, error_preamble);
        else if (necro_is_bound_in_scope(infer, kind1, scope))
            necro_instantiate_kind_var(infer, &kind2->var, kind1, macro_type, error_preamble);
        else
            necro_instantiate_kind_var(infer, &kind1->var, kind2, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (kind1->var.is_rigid)
        {
            necro_rigid_kind_variable_error(infer, kind1->var.var, kind2, macro_type, error_preamble);
            return;
        }
        if (necro_occurs(infer, kind1, kind2, macro_type, error_preamble))
        {
            return;
        }
        necro_instantiate_kind_var(infer, &kind1->var, kind2, macro_type, error_preamble);
        return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to kind unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to kind unify KindVar with kind args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent kind (kind1: %d, kind2: %s) found in necro_kind_unify_var.", kind1->type, kind2->type); return;
    }
}

inline void necro_kind_unify_app(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(false);
}

inline void necro_kind_unify_fun(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_FUN);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind2->var.is_rigid)
            necro_rigid_kind_variable_error(infer, kind2->var.var, kind1, macro_type, error_preamble);
        else if (necro_occurs(infer, kind2, kind1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_kind_var(infer, &kind2->var, kind1, macro_type, error_preamble);
        return;
    case NECRO_TYPE_FUN:  necro_kind_unify(infer, kind1->fun.type1, kind2->fun.type1, scope, macro_type, error_preamble); necro_kind_unify(infer, kind1->fun.type2, kind2->fun.type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:
    {
        necro_infer_error(infer, error_preamble, macro_type, "Kind error: Attempting to kind unify (->) with KindCon (%s).\n  Kind1: %s\n  Kind2: %s",
            necro_intern_get_string(infer->intern, kind2->con.con.symbol),
            necro_type_string(infer, kind1),
            necro_type_string(infer, kind2)
            );
        return;
    }
    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Kind error: Attempting to kind unify (->) with KindApp."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to kind unify (->) with type args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", kind1->type, kind2->type); return;
    }
}

inline void necro_kind_unify_con(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_CON);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind2->var.is_rigid)
            necro_rigid_kind_variable_error(infer, kind2->var.var, kind1, macro_type, error_preamble);
        else if (necro_occurs(infer, kind2, kind1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_kind_var(infer, &kind2->var, kind1, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
        if (kind1->con.con.symbol.id != kind2->con.con.symbol.id)
        {
            necro_infer_error(infer, error_preamble, kind2, "Kind error: Attempting to unify two different kinds, Kind1: %s, Kind2: %s", necro_intern_get_string(infer->intern, kind1->con.con.symbol), necro_intern_get_string(infer->intern, kind2->con.con.symbol));
            return;
        }
        else
        {
            NecroTypeKind* original_kind1 = kind1;
            NecroTypeKind* original_kind2 = kind2;
            kind1 = kind1->con.args;
            kind2 = kind2->con.args;
            while (kind1 != NULL && kind2 != NULL)
            {
                if (kind1 == NULL || kind2 == NULL)
                {
                    necro_infer_error(infer, error_preamble, kind1, "Kind error: Mismatched arities, Kind1: %s, Kind2: %s", necro_intern_get_string(infer->intern, original_kind1->con.con.symbol), necro_intern_get_string(infer->intern, original_kind2->con.con.symbol));
                    return;
                }
                assert(kind1->type == NECRO_TYPE_LIST);
                assert(kind2->type == NECRO_TYPE_LIST);
                necro_kind_unify(infer, kind1->list.item, kind2->list.item, scope, macro_type, error_preamble);
                kind1 = kind1->list.next;
                kind2 = kind2->list.next;
            }
            return;
        }
    case NECRO_TYPE_APP:
    {
        assert(false);
        return;
    }
    case NECRO_TYPE_FUN:
    {
        necro_infer_error(infer, error_preamble, macro_type, "Kind error: Attempting to kind unify KindCon (%s) with (->).\n  Kind1: %s\n  Kind2: %s",
            necro_intern_get_string(infer->intern, kind1->con.con.symbol),
            necro_type_string(infer, kind1),
            necro_type_string(infer, kind2)
            );
        return;
    }
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to kind unify KindCon (%s) with type args list.", necro_intern_get_string(infer->intern, kind1->con.con.symbol)); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to kind unify polytype."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent kind (kind1: %d, kind2: %s) found in necro_unify.", kind1->type, kind2->type); return;
    }
}

void necro_kind_unify(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(kind1 != NULL);
    assert(kind2 != NULL);
    kind1 = necro_find(infer, kind1);
    kind2 = necro_find(infer, kind2);
    if (kind1 == kind2)
        return;
    switch (kind1->type)
    {
    case NECRO_TYPE_VAR:  necro_kind_unify_var(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_kind_unify_app(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_FUN:  necro_kind_unify_fun(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:  necro_kind_unify_con(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, kind1, "Compiler bug: Found Type args list in necro_unify"); return;
    case NECRO_TYPE_FOR:
    {
        while (kind1->type == NECRO_TYPE_FOR)
            kind1 = kind1->for_all.type;
        necro_kind_unify(infer, kind1, kind2, scope, macro_type, error_preamble);
        return;
    }
    default: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent kind (kind1: %d, kind2: %s) found in necro_unify.", kind1->type, kind2->type); return;
    }
}

NecroTypeKind* necro_kind_inst(NecroInfer* infer, NecroTypeKind* kind, NecroScope* scope)
{
    assert(infer != NULL);
    assert(kind != NULL);
    return kind;
}

NecroTypeKind* necro_kind_infer(NecroInfer* infer, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    assert(infer != NULL);
    assert(type != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    // if (type->type_kind != NULL) return type->type_kind;
    switch (type->type)
    {

    case NECRO_TYPE_VAR:
    {
        NecroSymbolInfo* symbol_info = necro_symtable_get(infer->symtable, type->var.var.id);
        if (symbol_info != NULL)
        {
            // assert(symbol_info != NULL);
            // assert(symbol_info->type != NULL);
            if (symbol_info->type == NULL)
            {
                symbol_info->type = necro_new_name(infer, type->source_loc);
            }
            // assert(symbol_info->type->type_kind != NULL);
            if (symbol_info->type->type_kind == NULL)
            {
                symbol_info->type->type_kind = necro_new_name(infer, type->source_loc);
            }
            type->type_kind = symbol_info->type->type_kind;
        }
        else
        {
            // NecroType* var_type = necro_find(infer, type);
            if (type->type_kind == NULL)
            {
                type->type_kind = necro_new_name(infer, type->source_loc);
            }
        }
        return type->type_kind;
    }

    case NECRO_TYPE_FUN:
    {
        NecroTypeKind* type1_kind = necro_kind_infer(infer, type->fun.type1, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        NecroTypeKind* type2_kind = necro_kind_infer(infer, type->fun.type2, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        necro_kind_unify(infer, type1_kind, infer->star_type_kind, NULL, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        necro_kind_unify(infer, type2_kind, infer->star_type_kind, NULL, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        type->type_kind = infer->star_type_kind;
        return type->type_kind;
    }

    case NECRO_TYPE_APP:
    {
        NecroTypeKind* type1_kind = necro_kind_infer(infer, type->app.type1, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        NecroTypeKind* type2_kind = necro_kind_infer(infer, type->app.type2, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        NecroType* result_kind = necro_new_name(infer, type->source_loc);
        NecroType* f_kind      = necro_create_type_fun(infer, type2_kind, result_kind);
        necro_kind_unify(infer, type1_kind, f_kind, NULL, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        type->type_kind = result_kind;
        return type->type_kind;
    }

    case NECRO_TYPE_CON:
    {
        NecroSymbolInfo* symbol_info = necro_symtable_get(infer->symtable, type->con.con.id);
        assert(symbol_info != NULL);
        assert(symbol_info->type != NULL);
        // assert(symbol_info->type->type_kind != NULL);
        if (symbol_info->type->type_kind == NULL)
        {
            symbol_info->type->type_kind = necro_new_name(infer, type->source_loc);
            type->type_kind              = symbol_info->type->type_kind;
        }
        NecroType*     args       = type->con.args;
        NecroTypeKind* args_kinds = NULL;
        NecroTypeKind* args_head  = NULL;
        while (args != NULL)
        {
            NecroTypeKind* arg_kind = necro_kind_infer(infer, args->list.item, macro_type, error_preamble);
            if (necro_is_infer_error(infer)) return NULL;
            if (args_head == NULL)
            {
                args_kinds = necro_create_type_fun(infer, arg_kind, NULL);
                args_head  = args_kinds;
            }
            else
            {
                args_kinds->fun.type2 = necro_create_type_fun(infer, arg_kind, NULL);
                args_kinds = args_kinds->fun.type2;
            }
            args = args->list.next;
        }
        NecroTypeKind* result_type = necro_new_name(infer, type->source_loc);
        if (args_kinds != NULL)
            args_kinds->fun.type2 = result_type;
        else
            args_head = result_type;
        necro_kind_unify(infer, symbol_info->type->type_kind, args_head, NULL, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        if (type->type_kind == NULL)
            type->type_kind = result_type;
        return result_type;
    }

    case NECRO_TYPE_FOR:
    {
        NecroTypeKind* for_all_type_kind = necro_kind_infer(infer, type->for_all.type, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        type->type_kind = for_all_type_kind;
        return type->type_kind;
    }

    case NECRO_TYPE_LIST: assert(false); return NULL;
    default:              return necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Unimplemented Type in necro_kind_infer: %d", type->type);
    }
}

NecroTypeKind* necro_kind_gen(NecroInfer* infer, NecroTypeKind* kind)
{
    assert(infer != NULL);
    assert(kind != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    kind = necro_find(infer, kind);
    switch (kind->type)
    {

    // Default free kind vars to *
    case NECRO_TYPE_VAR:
    {
        necro_instantiate_kind_var(infer, &kind->var, infer->star_type_kind, kind, "In necro_kind_gen: ");
        if (necro_is_infer_error(infer)) return NULL;
        return infer->star_type_kind;
    }

    case NECRO_TYPE_FUN:
    {
        NecroTypeKind* kind1 = necro_kind_gen(infer, kind->fun.type1);
        if (necro_is_infer_error(infer)) return NULL;
        NecroTypeKind* kind2 = necro_kind_gen(infer, kind->fun.type2);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_create_type_fun(infer, kind1, kind2);
    }

    case NECRO_TYPE_APP:
    {
        assert(false);
        return NULL;
    }

    case NECRO_TYPE_CON:
    {
        NecroType*     args       = kind->con.args;
        NecroTypeKind* args_kinds = NULL;
        NecroTypeKind* args_head  = NULL;
        while (args != NULL)
        {
            NecroTypeKind* arg_kind = necro_kind_gen(infer, args->list.item);
            if (necro_is_infer_error(infer)) return NULL;
            if (args_head == NULL)
            {
                args_kinds = necro_create_type_fun(infer, arg_kind, NULL);
                args_head  = args_kinds;
            }
            else
            {
                args_kinds->fun.type2 = necro_create_type_fun(infer, args_kinds, NULL);
                args_kinds = args_kinds->fun.type2;
            }
            args = args->list.next;
        }
        if (args_kinds != NULL)
            args_kinds->fun.type2 = infer->star_type_kind;
        else
            args_head = kind;
            // args_head = infer->star_type_kind;
        return args_head;
    }

    case NECRO_TYPE_FOR:
    {
        assert(false);
    }

    case NECRO_TYPE_LIST: assert(false); return NULL;
    default: return necro_infer_error(infer, NULL, kind, "Compiler bug: Unimplemented Kind in necro_kind_gen: %d", kind->type);
    }
}
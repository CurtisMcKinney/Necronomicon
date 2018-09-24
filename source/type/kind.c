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
#include "base.h"

void necro_rigid_kind_variable_error(NecroInfer* infer, NecroAstSymbol type_var, NecroTypeKind* type, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    const char* type_name = NULL;
    if (type->type == NECRO_TYPE_CON)
        type_name = type->con.con_symbol.source_name->str;
    else if (type->type == NECRO_TYPE_APP)
        type_name = "TypeApp";
    else if (type->type == NECRO_TYPE_FUN)
        type_name = "(->)";
    else if (type->type == NECRO_TYPE_VAR)
        type_name = type->var.var_symbol.source_name->str;
    else
        assert(false);
    const char* var_name = type_var.source_name->str;
    necro_infer_error(infer, error_preamble, macro_type, "Couldn't match kind \'%s\' with kind \'%s\'.\n\'%s\' is a rigid kind variable bound by a type signature.", var_name, type_name, var_name);
}

NecroAstSymbol necro_create_star_kind(NecroPagedArena* arena, NecroIntern* intern)
{
    // Add to Base?!?!?! Add to Type namespace!?!?
    NecroSymbol         star_symbol = necro_intern_string(intern, "Type");
    NecroAstSymbolData* ast_data    = necro_paged_arena_alloc(arena, sizeof(NecroAstSymbolData));
    NecroAstSymbol      ast_symbol  = { .name = star_symbol, .source_name = star_symbol, .module_name = necro_intern_string(intern, "Base"), .ast_data = ast_data };
    NecroType*          star_type   = necro_type_alloc(arena);
    star_type->type                 = NECRO_TYPE_CON;
    star_type->con                  = (NecroTypeCon)
    {
        .con_symbol = ast_symbol,
        .args       = NULL,
        .arity      = 0,
        .is_class   = false,
    };
    star_type->kind           = NULL;
    star_type->pre_supplied   = true;
    ast_symbol.ast_data->type = star_type;
    ast_symbol.ast_data->ast  = NULL;
    return ast_symbol;
}

inline void necro_kind_unify_var(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_VAR);
    if (kind2 == NULL)
    {
        // TODO: NecroResultError
        // necro_infer_error(infer, error_preamble, macro_type, "Kind error: Mismatched kind arities");
        return;
    }
    if (kind1 == kind2)
        return;
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind1->var.var_symbol.name == kind2->var.var_symbol.name)
            return;
        else if (kind1->var.is_rigid && kind2->var.is_rigid)
            necro_rigid_kind_variable_error(infer, kind1->var.var_symbol, kind2, macro_type, error_preamble);
        else if (necro_occurs(kind1->var.var_symbol, kind2, macro_type, error_preamble))
            return;
        else if (kind1->var.is_rigid)
            kind2->var.bound = kind1;
        else if (kind2->var.is_rigid)
            kind1->var.bound = kind2;
        else if (necro_is_bound_in_scope(kind1, scope))
            kind2->var.bound = kind1;
        else
            kind1->var.bound = kind2;
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (kind1->var.is_rigid)
        {
            // TODO: NecroResultError
            // necro_rigid_kind_variable_error(infer, kind1->var.var, kind2, macro_type, error_preamble);
            return;
        }
        if (necro_occurs(kind1->var.var_symbol, kind2, macro_type, error_preamble))
        {
            return;
        }
        kind1->var.bound = kind2;
        return;
    case NECRO_TYPE_FOR:  assert(false); break;
    case NECRO_TYPE_LIST: assert(false); break;
    default:              assert(false); break;
    }
}

inline void necro_kind_unify_app(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    UNUSED(infer);
    UNUSED(kind1);
    UNUSED(kind2);
    UNUSED(scope);
    UNUSED(macro_type);
    UNUSED(error_preamble);
    assert(false);
}

inline void necro_kind_unify_fun(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_FUN);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind2->var.is_rigid)
            necro_rigid_kind_variable_error(infer, kind2->var.var_symbol, kind1, macro_type, error_preamble);
        else if (necro_occurs(kind2->var.var_symbol, kind1, macro_type, error_preamble))
            return;
        else
            kind2->var.bound = kind1;
        return;
    case NECRO_TYPE_FUN:
        necro_kind_unify(infer, kind1->fun.type1, kind2->fun.type1, scope, macro_type, error_preamble);
        necro_kind_unify(infer, kind1->fun.type2, kind2->fun.type2, scope, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    {
        // TODO: NecroResultError
        // necro_infer_error(infer, error_preamble, macro_type, "Kind error: Attempting to kind unify (->) with KindCon (%s).\n  Kind1: %s\n  Kind2: %s",
        //     kind2->con.con_symbol->str,
        //     necro_type_string(infer, kind1),
        //     necro_type_string(infer, kind2)
        //     );
        return;
    }
    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Kind error: Attempting to kind unify (->) with KindApp."); return;
    case NECRO_TYPE_LIST: assert(false); return;
    default:              assert(false); return;
    }
}

inline void necro_kind_unify_con(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_CON);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind2->var.is_rigid)
            necro_rigid_kind_variable_error(infer, kind2->var.var_symbol, kind1, macro_type, error_preamble);
        else if (necro_occurs(kind2->var.var_symbol, kind1, macro_type, error_preamble))
            return;
        else
            kind2->var.bound = kind1;
        return;
    case NECRO_TYPE_CON:
        if (kind1->con.con_symbol.name != kind2->con.con_symbol.name)
        {
            // TODO: NecroResultError
            // necro_infer_error(infer, error_preamble, kind2, "Kind error: Attempting to unify two different kinds, Kind1: %s, Kind2: %s", kind1->con.con.symbol->str, kind2->con.con.symbol->str);
            return;
        }
        else
        {
            // NecroTypeKind* original_kind1 = kind1;
            // NecroTypeKind* original_kind2 = kind2;
            kind1 = kind1->con.args;
            kind2 = kind2->con.args;
            while (kind1 != NULL && kind2 != NULL)
            {
                if (kind1 == NULL || kind2 == NULL)
                {
                    // TODO: NecroResultError
                    // necro_infer_error(infer, error_preamble, kind1, "Kind error: Mismatched arities, Kind1: %s, Kind2: %s", original_kind1->con.con.symbol->str, original_kind2->con.con.symbol->str);
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
        // TODO: NecroResultError
        // necro_infer_error(infer, error_preamble, macro_type, "Kind error: Attempting to kind unify KindCon (%s) with (->).\n  Kind1: %s\n  Kind2: %s",
        //     kind1->con.con.symbol->str,
        //     necro_type_string(infer, kind1),
        //     necro_type_string(infer, kind2)
        //     );
        return;
    }
    case NECRO_TYPE_LIST: assert(false); return;
    case NECRO_TYPE_FOR:  assert(false); return;
    default:              assert(false); return;
    }
}

void necro_kind_unify(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(infer != NULL);
    assert(kind1 != NULL);
    assert(kind2 != NULL);
    kind1 = necro_find(kind1);
    kind2 = necro_find(kind2);
    if (kind1 == kind2)
        return;
    switch (kind1->type)
    {
    case NECRO_TYPE_VAR:  necro_kind_unify_var(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_kind_unify_app(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_FUN:  necro_kind_unify_fun(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:  necro_kind_unify_con(infer, kind1, kind2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_LIST: assert(false); return;
    case NECRO_TYPE_FOR:
    {
        while (kind1->type == NECRO_TYPE_FOR)
            kind1 = kind1->for_all.type;
        necro_kind_unify(infer, kind1, kind2, scope, macro_type, error_preamble);
        return;
    }
    default: assert(false); return;
    }
}

NecroTypeKind* necro_kind_inst(NecroInfer* infer, NecroTypeKind* kind, NecroScope* scope)
{
    UNUSED(scope);
    assert(infer != NULL);
    assert(kind != NULL);
    return kind;
}

NecroTypeKind* necro_kind_infer(NecroInfer* infer, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    assert(infer != NULL);
    assert(type != NULL);
    switch (type->type)
    {

    case NECRO_TYPE_VAR:
    {
        // TODO: Look at cleaning this up
        NecroAstSymbolData* data = type->var.var_symbol.ast_data;
        if (data != NULL)
        {
            // assert(symbol_info != NULL);
            // assert(symbol_info->type != NULL);
            if (data->type == NULL)
            {
                data->type = necro_new_name(infer);
            }
            // assert(symbol_info->type->type_kind != NULL);
            if (data->type->kind == NULL)
            {
                data->type->kind = necro_new_name(infer);
            }
            type->kind = data->type->kind;
        }
        else
        {
            // NecroType* var_type = necro_find(infer, type);
            if (type->kind == NULL)
            {
                type->kind = necro_new_name(infer);
            }
        }
        return type->kind;
    }

    case NECRO_TYPE_FUN:
    {
        NecroTypeKind* type1_kind = necro_kind_infer(infer, type->fun.type1, macro_type, error_preamble);
        NecroTypeKind* type2_kind = necro_kind_infer(infer, type->fun.type2, macro_type, error_preamble);
        necro_kind_unify(infer, type1_kind, infer->base->star_kind.ast_data->type, NULL, macro_type, error_preamble);
        necro_kind_unify(infer, type2_kind, infer->base->star_kind.ast_data->type, NULL, macro_type, error_preamble);
        type->kind                = infer->base->star_kind.ast_data->type;
        return type->kind;
    }

    case NECRO_TYPE_APP:
    {
        NecroTypeKind* type1_kind = necro_kind_infer(infer, type->app.type1, macro_type, error_preamble);
        NecroTypeKind* type2_kind = necro_kind_infer(infer, type->app.type2, macro_type, error_preamble);
        NecroType* result_kind    = necro_new_name(infer);
        NecroType* f_kind         = necro_type_fn_create(infer->arena, type2_kind, result_kind);
        necro_kind_unify(infer, type1_kind, f_kind, NULL, macro_type, error_preamble);
        type->kind                = result_kind;
        return type->kind;
    }

    case NECRO_TYPE_CON:
    {
        NecroAstSymbolData* data = type->con.con_symbol.ast_data;
        assert(data != NULL);
        assert(data->type != NULL);
        if (data->type->kind == NULL)
        {
            data->type->kind = necro_new_name(infer);
            type->kind       = data->type->kind;
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
                args_kinds = necro_type_fn_create(infer->arena, arg_kind, NULL);
                args_head  = args_kinds;
            }
            else
            {
                args_kinds->fun.type2 = necro_type_fn_create(infer->arena, arg_kind, NULL);
                args_kinds = args_kinds->fun.type2;
            }
            args = args->list.next;
        }
        NecroTypeKind* result_type = necro_new_name(infer);
        if (args_kinds != NULL)
            args_kinds->fun.type2 = result_type;
        else
            args_head = result_type;
        necro_kind_unify(infer, data->type->kind, args_head, NULL, macro_type, error_preamble);
        if (type->kind == NULL)
            type->kind = result_type;
        return result_type;
    }

    case NECRO_TYPE_FOR:
        type->kind = necro_kind_infer(infer, type->for_all.type, macro_type, error_preamble);
        return type->kind;

    case NECRO_TYPE_LIST: assert(false); return NULL;
    default:              assert(false); return NULL;
    }
}

NecroTypeKind* necro_kind_gen(NecroInfer* infer, NecroTypeKind* kind)
{
    assert(infer != NULL);
    assert(kind != NULL);
    kind = necro_find(kind);
    switch (kind->type)
    {

    // Default free kind vars to *
    case NECRO_TYPE_VAR:
        kind->var.bound = infer->base->star_kind.ast_data->type;
        return infer->base->star_kind.ast_data->type;

    case NECRO_TYPE_FUN:
        return necro_type_fn_create(infer->arena, necro_kind_gen(infer, kind->fun.type1), necro_kind_gen(infer, kind->fun.type2));

    case NECRO_TYPE_APP:
        assert(false);
        return NULL;

    case NECRO_TYPE_CON:
    {
        NecroType*     args       = kind->con.args;
        NecroTypeKind* args_kinds = NULL;
        NecroTypeKind* args_head  = NULL;
        while (args != NULL)
        {
            NecroTypeKind* arg_kind = necro_kind_gen(infer, args->list.item);
            if (args_head == NULL)
            {
                args_kinds = necro_type_fn_create(infer->arena, arg_kind, NULL);
                args_head  = args_kinds;
            }
            else
            {
                args_kinds->fun.type2 = necro_type_fn_create(infer->arena, args_kinds, NULL);
                args_kinds = args_kinds->fun.type2;
            }
            args = args->list.next;
        }
        if (args_kinds != NULL)
            args_kinds->fun.type2 = infer->base->star_kind.ast_data->type;
        else
            args_head = kind;
            // args_head = infer->star_type_kind;
        return args_head;
    }

    case NECRO_TYPE_FOR:
        assert(false);
        return NULL;

    case NECRO_TYPE_LIST:
        assert(false);
        return NULL;

    default:
        assert(false);
        return NULL;
    }
}
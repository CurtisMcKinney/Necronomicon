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

NecroAstSymbol* necro_kind_create_star(NecroPagedArena* arena, NecroIntern* intern)
{
    NecroAstSymbol* ast_symbol  = necro_ast_symbol_create(arena, necro_intern_string(intern, "Necro.Base.Type"), necro_intern_string(intern, "Type"), necro_intern_string(intern, "Necro.Base"), NULL);
    NecroType*      star_type   = necro_type_alloc(arena);
    star_type->type             = NECRO_TYPE_CON;
    star_type->con              = (NecroTypeCon)
    {
        .con_symbol = ast_symbol,
        .args       = NULL,
        .is_class   = false,
    };
    star_type->kind          = NULL;
    star_type->pre_supplied  = true;
    ast_symbol->type         = star_type;
    ast_symbol->ast          = necro_ast_create_var(arena, intern, "Type", NECRO_VAR_DECLARATION);
    return ast_symbol;
}

NecroResult(NecroType) necro_kind_unify_var(NecroType* kind1, NecroType* kind2, NecroScope* scope)
{
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_VAR);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind1->var.var_symbol == kind2->var.var_symbol)
            return ok(NecroType, NULL);
        if (kind1->var.is_rigid && kind2->var.is_rigid)
            return necro_kind_rigid_kind_variable_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(kind1->var.var_symbol, kind2));
        if (kind1->var.is_rigid)
            kind2->var.bound = kind1;
        else if (kind2->var.is_rigid)
            kind1->var.bound = kind2;
        else if (necro_type_is_bound_in_scope(kind1, scope))
            kind2->var.bound = kind1;
        else
            kind1->var.bound = kind2;
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (kind1->var.is_rigid)
            return necro_kind_rigid_kind_variable_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(kind1->var.var_symbol, kind2));
        kind1->var.bound = kind2;
        return ok(NecroType, NULL);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_kind_unify_fun(NecroType* kind1, NecroType* kind2, NecroScope* scope)
{
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_FUN);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind2->var.is_rigid)
            return necro_kind_rigid_kind_variable_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(kind2->var.var_symbol, kind1));
        kind2->var.bound = kind1;
        return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
        necro_try(NecroType, necro_kind_unify(kind1->fun.type1, kind2->fun.type1, scope));
        return necro_kind_unify(kind1->fun.type2, kind2->fun.type2, scope);
    case NECRO_TYPE_CON:  return necro_kind_mismatched_kind_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
    case NECRO_TYPE_APP:  return necro_kind_mismatched_kind_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_kind_unify_con(NecroType* kind1, NecroType* kind2, NecroScope* scope)
{
    assert(kind1 != NULL);
    assert(kind1->type == NECRO_TYPE_CON);
    assert(kind2 != NULL);
    switch (kind2->type)
    {
    case NECRO_TYPE_VAR:
        if (kind2->var.is_rigid)
            return necro_kind_rigid_kind_variable_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(kind2->var.var_symbol, kind1));
        kind2->var.bound = kind1;
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
        if (kind1->con.con_symbol != kind2->con.con_symbol)
        {
            return necro_kind_mismatched_kind_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
        }
        else
        {
            NecroType* original_kind1 = kind1;
            NecroType* original_kind2 = kind2;
            kind1 = kind1->con.args;
            kind2 = kind2->con.args;
            while (kind1 != NULL && kind2 != NULL)
            {
                if (kind1 == NULL || kind2 == NULL)
                {
                    return necro_kind_mismatched_arity_error(original_kind1, original_kind2, NULL, NULL, NULL_LOC, NULL_LOC);
                }
                assert(kind1->type == NECRO_TYPE_LIST);
                assert(kind2->type == NECRO_TYPE_LIST);
                necro_try(NecroType, necro_kind_unify(kind1->list.item, kind2->list.item, scope));
                kind1 = kind1->list.next;
                kind2 = kind2->list.next;
            }
            return ok(NecroType, NULL);
        }
    case NECRO_TYPE_FUN:  return necro_kind_mismatched_kind_error(kind1, kind2, NULL, NULL, NULL_LOC, NULL_LOC);
    case NECRO_TYPE_APP:  necro_unreachable(NecroType);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_kind_unify(NecroType* kind1, NecroType* kind2, NecroScope* scope)
{
    assert(kind1 != NULL);
    assert(kind2 != NULL);
    kind1 = necro_type_find(kind1);
    kind2 = necro_type_find(kind2);
    if (kind1 == kind2)
        return ok(NecroType, NULL);
    switch (kind1->type)
    {
    case NECRO_TYPE_VAR:  return necro_kind_unify_var(kind1, kind2, scope);
    case NECRO_TYPE_FUN:  return necro_kind_unify_fun(kind1, kind2, scope);
    case NECRO_TYPE_CON:  return necro_kind_unify_con(kind1, kind2, scope);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    case NECRO_TYPE_APP:  necro_unreachable(NecroType);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_kind_unify_with_info(NecroType* kind1, NecroType* kind2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroResult(NecroType) result = necro_kind_unify(kind1, kind2, scope);
    if (result.type == NECRO_RESULT_OK)
    {
        return result;
    }
    assert(result.error != NULL);
    switch (result.error->type)
    {
    case NECRO_KIND_MISMATCHED_KIND:
        result.error->default_type_error_data2.macro_type1 = kind1;
        result.error->default_type_error_data2.macro_type2 = kind2;
        result.error->default_type_error_data2.source_loc  = source_loc;
        result.error->default_type_error_data2.end_loc     = end_loc;
        break;

    case NECRO_KIND_MISMATCHED_ARITY:
    case NECRO_KIND_RIGID_KIND_VARIABLE:
        assert(false);
        break;

    default:
        assert(false);
        break;
    }
    return result;
}

NecroResult(NecroType) necro_kind_infer(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    // TODO (Curtis, 2-11-19): Memoizing kind isn't working!?!?
    if (type->kind != NULL)
        return ok(NecroType, type->kind);
    switch (type->type)
    {

    case NECRO_TYPE_VAR:
    {
        NecroAstSymbol* ast_symbol = type->var.var_symbol;
        if (ast_symbol != NULL)
        {
            if (ast_symbol->type == NULL)
            {
                ast_symbol->type = necro_type_fresh_var(arena);
            }
            if (ast_symbol->type->kind == NULL)
            {
                ast_symbol->type->kind = necro_type_fresh_var(arena);
            }
            type->kind = ast_symbol->type->kind;
        }
        else
        {
            if (type->kind == NULL)
            {
                type->kind = necro_type_fresh_var(arena);
            }
        }
        return ok(NecroType, type->kind);
    }

    case NECRO_TYPE_FUN:
    {
        NecroType* type1_kind = necro_try(NecroType, necro_kind_infer(arena, base, type->fun.type1, source_loc, end_loc));
        necro_try(NecroType, necro_kind_unify_with_info(base->star_kind->type, type1_kind, NULL, source_loc, end_loc));
        NecroType* type2_kind = necro_try(NecroType, necro_kind_infer(arena, base, type->fun.type2, source_loc, end_loc));
        necro_try(NecroType, necro_kind_unify_with_info(base->star_kind->type, type2_kind, NULL, source_loc, end_loc));
        type->kind            = base->star_kind->type;
        return ok(NecroType, type->kind);
    }

    case NECRO_TYPE_APP:
    {
        NecroType* type1_kind  = necro_try(NecroType, necro_kind_infer(arena, base, type->app.type1, source_loc, end_loc));
        NecroType* type2_kind  = necro_try(NecroType, necro_kind_infer(arena, base, type->app.type2, source_loc, end_loc));
        NecroType* result_kind = necro_type_fresh_var(arena);
        NecroType* f_kind      = necro_type_fn_create(arena, type2_kind, result_kind);
        necro_try(NecroType, necro_kind_unify_with_info(type1_kind, f_kind, NULL, source_loc, end_loc));
        type->kind             = result_kind;
        return ok(NecroType, type->kind);
    }

    case NECRO_TYPE_CON:
    {
        NecroAstSymbol* con_symbol = type->con.con_symbol;
        assert(con_symbol != NULL);
        assert(con_symbol->type != NULL);
        assert(con_symbol->type->kind != NULL);
        NecroType* args       = type->con.args;
        NecroType* args_kinds = NULL;
        NecroType* args_head  = NULL;
        while (args != NULL)
        {
            NecroType* arg_kind = necro_try(NecroType, necro_kind_infer(arena, base, args->list.item, source_loc, end_loc));
            if (args_head == NULL)
            {
                args_kinds = necro_type_fn_create(arena, arg_kind, NULL);
                args_head  = args_kinds;
            }
            else
            {
                args_kinds->fun.type2 = necro_type_fn_create(arena, arg_kind, NULL);
                args_kinds = args_kinds->fun.type2;
            }
            args = args->list.next;
        }
        NecroType* result_kind = necro_type_fresh_var(arena);
        if (args_kinds != NULL)
            args_kinds->fun.type2 = result_kind;
        else
            args_head = result_kind;
        necro_try(NecroType, necro_kind_unify_with_info(con_symbol->type->kind, args_head, NULL, source_loc, end_loc));
        type->kind = necro_type_find(result_kind);
        return ok(NecroType, necro_type_find(result_kind));
    }

    case NECRO_TYPE_FOR:
        type->kind = necro_try(NecroType, necro_kind_infer(arena, base, type->for_all.type, source_loc, end_loc));
        return ok(NecroType, type->kind);

    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroType* necro_kind_gen(NecroPagedArena* arena, struct NecroBase* base, NecroType* kind)
{
    assert(kind != NULL);
    kind = necro_type_find(kind);
    switch (kind->type)
    {

    // Default free kind vars to *
    case NECRO_TYPE_VAR:
        kind->var.bound = base->star_kind->type;
        return base->star_kind->type;

    case NECRO_TYPE_FUN:
        return necro_type_fn_create(arena, necro_kind_gen(arena, base, kind->fun.type1), necro_kind_gen(arena, base, kind->fun.type2));

    case NECRO_TYPE_CON:
    {
        NecroType* args       = kind->con.args;
        NecroType* args_kinds = NULL;
        NecroType* args_head  = NULL;
        while (args != NULL)
        {
            NecroType* arg_kind = necro_kind_gen(arena, base, args->list.item);
            if (args_head == NULL)
            {
                args_kinds = necro_type_fn_create(arena, arg_kind, NULL);
                args_head  = args_kinds;
            }
            else
            {
                args_kinds->fun.type2 = necro_type_fn_create(arena, args_kinds, NULL);
                args_kinds = args_kinds->fun.type2;
            }
            args = args->list.next;
        }
        if (args_kinds != NULL)
            args_kinds->fun.type2 = base->star_kind->type;
        else
            args_head = kind;
        return args_head;
    }

    case NECRO_TYPE_APP:
        assert(false);
        return NULL;

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

NecroResult(void) necro_kind_infer_gen_unify_with_star(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (type->kind != NULL)
        return ok_void();
    necro_try_map(NecroType, void, necro_kind_infer(arena, base, type, source_loc, end_loc));
    type->kind = necro_kind_gen(arena, base, type->kind);
    necro_try_map(NecroType, void, necro_kind_unify_with_info(base->star_kind->type, type->kind, scope, source_loc, end_loc));
    return ok_void();
}

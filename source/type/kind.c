/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "type_class.h"
#include "kind.h"
#include "base.h"

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroAstSymbol* necro_kind_create_kind_ast_symbol(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, NecroType* kind_kind)
{
    NecroAstSymbol* ast_symbol = necro_ast_symbol_create(arena, name, source_name, module_name, NULL);
    NecroType*      kind_type  = necro_type_alloc(arena);
    kind_type->type            = NECRO_TYPE_CON;
    kind_type->con             = (NecroTypeCon)
    {
        .con_symbol = ast_symbol,
        .args       = NULL,
    };
    kind_type->kind         = kind_kind;
    kind_type->pre_supplied = true;
    ast_symbol->type        = kind_type;
    ast_symbol->ast         = NULL;
    return ast_symbol;
}

NecroAstSymbol* necro_kind_create_type_con(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, NecroType* con_kind)
{
    NecroAstSymbol* ast_symbol = necro_ast_symbol_create(arena, name, source_name, module_name, NULL);
    NecroType*      type_con   = necro_type_alloc(arena);
    type_con->type   = NECRO_TYPE_CON;
    type_con->con    = (NecroTypeCon)
    {
        .con_symbol = ast_symbol,
        .args       = NULL,
    };
    type_con->kind         = con_kind;
    type_con->pre_supplied = true;
    ast_symbol->type       = type_con;
    ast_symbol->ast        = NULL;
    return ast_symbol;
}

NecroType* necro_kind_fresh_kind_var(NecroPagedArena* arena, NecroBase* base, NecroScope* scope)
{
    NecroType* kind_var   = necro_type_fresh_var(arena, scope);
    kind_var->kind        = base->kind_kind->type;
    return kind_var;
}

///////////////////////////////////////////////////////
// Base Init
///////////////////////////////////////////////////////
void necro_kind_init_kinds(NecroBase* base, NecroScopedSymTable* scoped_symtable, NecroIntern* intern)
{
    NecroSymbol base_name = necro_intern_string(intern, "Necro.Base");

    base->higher_kind      = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.TheoreticalHigherKindedKind"), necro_intern_string(intern, "TheoreticalHigherKindedKind"), base_name, NULL);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->higher_kind);

    base->kind_kind        = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Kind"), necro_intern_string(intern, "Kind"), base_name, base->higher_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->kind_kind);

    base->star_kind        = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Type"), necro_intern_string(intern, "Type"), base_name, base->kind_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->star_kind);

    base->attribute_kind = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Attribute"), necro_intern_string(intern, "Attribute"), base_name, base->kind_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->attribute_kind);

    base->nat_kind         = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Nat"), necro_intern_string(intern, "Nat"), base_name, base->kind_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->nat_kind);

    base->sym_kind         = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Sym"), necro_intern_string(intern, "Sym"), base_name, base->kind_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->sym_kind);

    // Ownership
    base->ownership_kind = necro_kind_create_kind_ast_symbol(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Uniqueness"), necro_intern_string(intern, "Uniqueness"), base_name, base->kind_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->ownership_kind);

    base->ownership_share = necro_kind_create_type_con(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Shared"), necro_intern_string(intern, "Shared"), base_name, base->ownership_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->ownership_share);

    base->ownership_steal = necro_kind_create_type_con(&base->ast.arena, necro_intern_string(intern, "Necro.Base.Unique"), necro_intern_string(intern, "Unique"), base_name, base->ownership_kind->type);
    necro_scope_insert_ast_symbol(&base->ast.arena, scoped_symtable->top_type_scope, base->ownership_steal);

}

///////////////////////////////////////////////////////
// Unify
///////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////
// Infer / Default
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_kind_infer(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    type = necro_type_find(type);
    assert(type != NULL && "NULL type found in necro_kind_infer!");
    switch (type->type)
    {

    case NECRO_TYPE_VAR:
    {
        if (type->kind != NULL)
            return ok(NecroType, type->kind);
        NecroAstSymbol* ast_symbol = type->var.var_symbol;
        if (ast_symbol != NULL)
        {
            if (ast_symbol->type == NULL)
            {
                ast_symbol->type = necro_type_fresh_var(arena, NULL);
            }
            if (ast_symbol->type->kind == NULL)
            {
                ast_symbol->type->kind = necro_type_fresh_var(arena, NULL);
            }
            type->kind = ast_symbol->type->kind;
        }
        else
        {
            if (type->kind == NULL)
            {
                type->kind = necro_type_fresh_var(arena, NULL);
            }
        }
        return ok(NecroType, type->kind);
    }

    case NECRO_TYPE_FUN:
    {
        NecroType* type1_kind = necro_try_result(NecroType, necro_kind_infer(arena, base, type->fun.type1, source_loc, end_loc));
        necro_try(NecroType, necro_kind_unify_with_info(base->star_kind->type, type1_kind, NULL, source_loc, end_loc));
        NecroType* type2_kind = necro_try_result(NecroType, necro_kind_infer(arena, base, type->fun.type2, source_loc, end_loc));
        necro_try(NecroType, necro_kind_unify_with_info(base->star_kind->type, type2_kind, NULL, source_loc, end_loc));
        type->kind            = base->star_kind->type;
        return ok(NecroType, type->kind);
    }

    case NECRO_TYPE_APP:
    {
        NecroType* type1_kind  = necro_try_result(NecroType, necro_kind_infer(arena, base, type->app.type1, source_loc, end_loc));
        NecroType* type2_kind  = necro_try_result(NecroType, necro_kind_infer(arena, base, type->app.type2, source_loc, end_loc));
        if (type->kind != NULL)
            return ok(NecroType, type->kind);
        NecroType* result_kind = necro_type_fresh_var(arena, NULL);
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
            NecroType* arg_kind = necro_try_result(NecroType, necro_kind_infer(arena, base, args->list.item, source_loc, end_loc));
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
        if (type->kind != NULL)
            return ok(NecroType, type->kind);
        NecroType* result_kind = necro_type_fresh_var(arena, NULL);
        if (args_kinds != NULL)
            args_kinds->fun.type2 = result_kind;
        else
            args_head = result_kind;
        necro_try(NecroType, necro_kind_unify_with_info(con_symbol->type->kind, args_head, NULL, source_loc, end_loc));
        type->kind = necro_type_find(result_kind);
        return ok(NecroType, necro_type_find(result_kind));
    }

    case NECRO_TYPE_FOR:
        type->kind = necro_try_result(NecroType, necro_kind_infer(arena, base, type->for_all.type, source_loc, end_loc));
        return ok(NecroType, type->kind);

    case NECRO_TYPE_NAT:
        if (type->kind != NULL)
            return ok(NecroType, type->kind);
        type->kind = necro_type_con_create(arena, base->nat_kind, NULL);
        return ok(NecroType, type->kind);

    case NECRO_TYPE_SYM:
        if (type->kind != NULL)
            return ok(NecroType, type->kind);
        type->kind = necro_type_con_create(arena, base->sym_kind, NULL);
        return ok(NecroType, type->kind);

    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroType* necro_kind_default(NecroPagedArena* arena, struct NecroBase* base, NecroType* kind)
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
    {
        NecroType* kind1 = necro_kind_default(arena, base, kind->fun.type1);
        NecroType* kind2 = necro_kind_default(arena, base, kind->fun.type2);
        if (kind1 != kind->fun.type1 || kind2 != kind->fun.type2)
            return necro_type_fn_create(arena, kind1, kind2);
        else
            return kind;
    }

    case NECRO_TYPE_CON:
        if (kind->con.args != NULL)
        {
            NecroType* args = necro_kind_default(arena, base, kind->con.args);
            if (args != kind->con.args)
                return necro_type_con_create(arena, kind->con.con_symbol, args);
            else
                return kind;
        }
        return kind;

    case NECRO_TYPE_LIST:
    {
        NecroType* item = necro_kind_default(arena, base, kind->list.item);
        NecroType* next = necro_kind_default(arena, base, kind->list.next);
        if (item != kind->list.item || next != kind->list.next)
            return necro_type_list_create(arena, item, next);
        else
            return kind;
    }

    case NECRO_TYPE_APP:
        assert(false);
        return NULL;

    case NECRO_TYPE_FOR:
        assert(false);
        return NULL;

    default:
        assert(false);
        return NULL;
    }
}

void necro_kind_default_type_kinds(NecroPagedArena* arena, struct NecroBase* base, NecroType* type)
{
    type = necro_type_find(type);
    if (type->kind == NULL)
    {
        unwrap(NecroType, necro_kind_infer(arena, base, type, NULL_LOC, NULL_LOC));
    }
    switch (type->type)
    {
    case NECRO_TYPE_CON:
    {
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            necro_kind_default_type_kinds(arena, base, args->list.item);
            args = args->list.next;
        }
        break;
    }
    case NECRO_TYPE_FUN:
        necro_kind_default_type_kinds(arena, base, type->fun.type1);
        necro_kind_default_type_kinds(arena, base, type->fun.type2);
        break;
    case NECRO_TYPE_APP:
        necro_kind_default_type_kinds(arena, base, type->app.type1);
        necro_kind_default_type_kinds(arena, base, type->app.type2);
        break;
    case NECRO_TYPE_FOR:
        necro_kind_default_type_kinds(arena, base, type->for_all.type);
        break;
    case NECRO_TYPE_VAR:
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
    case NECRO_TYPE_LIST:
        break;
    default:
        assert(false);
        break;
    }
    type->kind = necro_kind_default(arena, base, type->kind);
}

NecroResult(void) necro_kind_infer_default_unify_with_star(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (type->kind == NULL)
    {
        necro_try_map(NecroType, void, necro_kind_infer(arena, base, type, source_loc, end_loc));
    }
    necro_kind_default_type_kinds(arena, base, type);
    necro_try_map(NecroType, void, necro_kind_unify_with_info(base->star_kind->type, type->kind, scope, source_loc, end_loc));
    return ok_void();
}

NecroResult(void) necro_kind_infer_default_unify_with_kind(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroType* kind, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (type->kind == NULL)
    {
        necro_try_map(NecroType, void, necro_kind_infer(arena, base, type, source_loc, end_loc));
    }
    necro_kind_default_type_kinds(arena, base, type);
    necro_try_map(NecroType, void, necro_kind_unify_with_info(kind, type->kind, NULL, source_loc, end_loc));
    return ok_void();
}

NecroResult(void) necro_kind_infer_default(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (type->kind == NULL)
    {
        necro_try_map(NecroType, void, necro_kind_infer(arena, base, type, source_loc, end_loc));
    }
    necro_kind_default_type_kinds(arena, base, type);
    return ok_void();
}

bool necro_kind_is_type(const struct NecroBase* base, const NecroType* type)
{
    assert(type != NULL);
    type                  = necro_type_find_const(type);
    const NecroType* kind = necro_type_find_const(type->kind);
    return kind->type == NECRO_TYPE_CON && kind->con.con_symbol == base->star_kind;
}

bool necro_kind_is_ownership(const struct NecroBase* base, const NecroType* type)
{
    assert(type != NULL);
    type                  = necro_type_find_const(type);
    const NecroType* kind = necro_type_find_const(type->kind);
    return kind->type == NECRO_TYPE_CON && kind->con.con_symbol == base->ownership_kind;
}

bool necro_kind_is_nat(const struct NecroBase* base, const NecroType* type)
{
    assert(type != NULL);
    type                  = necro_type_find_const(type);
    const NecroType* kind = necro_type_find_const(type->kind);
    return kind->type == NECRO_TYPE_CON && kind->con.con_symbol == base->nat_kind;
}

bool necro_kind_is_kind(const struct NecroBase* base, const NecroType* type)
{
    assert(type != NULL);
    type                  = necro_type_find_const(type);
    const NecroType* kind = necro_type_find_const(type->kind);
    return kind->type == NECRO_TYPE_CON && kind->con.con_symbol == base->kind_kind;
}


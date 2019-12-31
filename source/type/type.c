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
#include "type.h"
#include "renamer.h"
#include "base.h"
#include "alias_analysis.h"
#include "infer.h"
#include "itoa.h"

#define NECRO_TYPE_DEBUG 0
#if NECRO_TYPE_DEBUG
#define TRACE_TYPE(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE(...)
#endif

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_type_bind_var(NecroType* var_to_bind, NecroType* type_to_bind_to);

NecroType* necro_type_alloc(NecroPagedArena* arena)
{
    NecroType* type      = necro_paged_arena_alloc(arena, sizeof(NecroType));
    type->pre_supplied   = false;
    type->kind           = NULL;
    type->ownership      = NULL;
    type->hash           = 0;
    type->has_propagated = false;
    return type;
}

NecroType* necro_type_var_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol, NecroScope* scope)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_VAR;
    type->kind      = NULL;
    type->var       = (NecroTypeVar)
    {
        .var_symbol  = var_symbol,
        .is_rigid    = false,
        .bound       = NULL,
        .scope       = scope,
    };
    return type;
}

NecroType* necro_type_app_create(NecroPagedArena* arena, NecroType* type1, NecroType* type2)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_APP;
    type->app       = (NecroTypeApp)
    {
        .type1 = type1,
        .type2 = type2,
    };
    return type;
}

NecroType* necro_type_fn_create(NecroPagedArena* arena, NecroType* type1, NecroType* type2)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_FUN;
    type->fun       = (NecroTypeFun)
    {
        .type1     = type1,
        .type2     = type2,
        // .free_vars = NULL,
    };
    return type;
}

NecroType* necro_type_con_create(NecroPagedArena* arena, NecroAstSymbol* con_symbol, NecroType* args)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con_symbol = con_symbol,
        .args       = args,
    };
    return type;
}

NecroType* necro_type_list_create(NecroPagedArena* arena, NecroType* item, NecroType* next)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_LIST;
    type->list      = (NecroTypeList)
    {
        .item = item,
        .next = next,
    };
    return type;
}

NecroType* necro_type_for_all_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol, NecroType* type)
{
    NecroType* for_all  = necro_type_alloc(arena);
    for_all->type       = NECRO_TYPE_FOR;
    for_all->for_all    = (NecroTypeForAll)
    {
        .var_symbol    = var_symbol,
        .type          = type,
        .is_normalized = false,
    };
    return for_all;
}

NecroType* necro_type_nat_create(NecroPagedArena* arena, size_t value)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_NAT;
    type->nat       = (NecroTypeNat)
    {
        .value      = value
    };
    return type;
}

NecroType* necro_type_sym_create(NecroPagedArena* arena, NecroSymbol value)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_SYM;
    type->sym       = (NecroTypeSym)
    {
        .value      = value
    };
    return type;
}

NecroType* necro_type_deep_copy_go(NecroPagedArena* arena, NecroType* type)
{
    if (type == NULL)
        return NULL;
    type                = necro_type_find(type);
    NecroType* new_type = NULL;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        new_type                              = necro_type_var_create(arena, type->var.var_symbol, type->var.scope);
        new_type->var.is_rigid                = type->var.is_rigid;
        new_type->var.var_symbol->constraints = type->var.var_symbol->constraints;
        // new_type->var.constraints = necro_constraint_list_deep_copy(arena, type->var.constraints);
        break;
    }
    case NECRO_TYPE_APP:  new_type = necro_type_app_create(arena, necro_type_deep_copy_go(arena, type->app.type1), necro_type_deep_copy_go(arena, type->app.type2)); break;
    case NECRO_TYPE_FUN:  new_type = necro_type_fn_create(arena, necro_type_deep_copy_go(arena, type->fun.type1), necro_type_deep_copy_go(arena, type->fun.type2)); break;
    case NECRO_TYPE_CON:  new_type = necro_type_con_create(arena, type->con.con_symbol, necro_type_deep_copy_go(arena, type->con.args)); break;
    case NECRO_TYPE_FOR:  new_type = necro_type_for_all_create(arena, type->for_all.var_symbol, necro_type_deep_copy_go(arena, type->for_all.type)); break;
    case NECRO_TYPE_LIST: new_type = necro_type_list_create(arena, necro_type_deep_copy_go(arena, type->list.item), necro_type_deep_copy_go(arena, type->list.next)); break;
    case NECRO_TYPE_NAT:  new_type = necro_type_nat_create(arena, type->nat.value); break;
    case NECRO_TYPE_SYM:  new_type = necro_type_sym_create(arena, type->sym.value); break;
    default:              assert(false);
    }
    new_type->kind        = necro_type_find(type->kind);
    new_type->ownership   = necro_type_deep_copy(arena, type->ownership);
    return new_type;
}

NecroType* necro_type_deep_copy(NecroPagedArena* arena,  NecroType* type)
{
    NecroType* new_type = necro_type_deep_copy_go(arena, type);
    return new_type;
}

size_t necro_type_list_count(NecroType* list)
{
    size_t     count   = 0;
    NecroType* current = list;
    while (current != NULL)
    {
        count++;
        current = current->list.next;
    }
    return count;
}

NecroType* necro_type_curry_con(NecroPagedArena* arena, NecroBase* base, NecroType* con)
{
    assert(arena != NULL);
    assert(con != NULL);
    assert(con->type == NECRO_TYPE_CON);
    if (con->con.args == NULL)
        return NULL;
    assert(con->con.args->type == NECRO_TYPE_LIST);
    NecroType* uniqueness = con->ownership;
    NecroType* curr       = con->con.args;
    NecroType* tail       = NULL;
    NecroType* head       = NULL;
    while (curr->list.next != NULL)
    {
        curr = necro_type_list_create(arena, necro_type_find(curr->list.item), curr->list.next);
        if (tail != NULL)
            tail->list.next = curr;
        if (head == NULL)
            head = curr;
        tail = curr;
        curr = curr->list.next;
    }
    if (tail != NULL)
        tail->list.next = NULL;
    NecroType* new_con = necro_type_con_create(arena, con->con.con_symbol, head);
    NecroType* ap      = necro_type_app_create(arena, new_con, curr->list.item);
    unwrap(NecroType, necro_kind_infer(arena, base, ap, NULL_LOC, NULL_LOC));
    ap->ownership      = uniqueness;
    return ap;
}

bool necro_type_is_type_app_type_con(const NecroType* app)
{
    app = necro_type_find_const(app);
    while (app->type == NECRO_TYPE_APP)
    {
        app = necro_type_find_const(app->app.type1);
    }
    return app->type == NECRO_TYPE_CON;
}

NecroType* necro_type_uncurry_app_if_type_con(NecroPagedArena* arena, NecroBase* base, NecroType* app)
{
    app = necro_type_find(app);
    if (app->type != NECRO_TYPE_APP)
        return app;
    if (necro_type_is_type_app_type_con(app))
        return necro_type_uncurry_app(arena, base, app);
    else
        return app;
}

// NOTE: Expects that the type function being applied is TypeCon and NOT a TypeVar. If it is, it asserts(false);
NecroType* necro_type_uncurry_app(NecroPagedArena* arena, NecroBase* base, NecroType* app)
{
    assert(arena != NULL);
    if (app == NULL)
        return NULL;
    app = necro_type_find(app);
    if (app->type != NECRO_TYPE_APP)
        return app;
    assert(app->type == NECRO_TYPE_APP);

    NecroType* curr_app = app;
    while (curr_app->type == NECRO_TYPE_APP)
        curr_app = necro_type_find(curr_app->app.type1);
    if (curr_app->type != NECRO_TYPE_CON)
        return app;

    NecroType* app_ownership = necro_type_find(app->ownership);

    // Get Args from TypeApp applications
    NecroType* args = NULL;
    while (app->type == NECRO_TYPE_APP)
    {
        args = necro_type_list_create(arena, necro_type_uncurry_app(arena, base, app->app.type2), args);
        app  = app->app.type1;
        app  = necro_type_find(app);
    }
    assert(app->type == NECRO_TYPE_CON);

    // Combine TypeApp args with TypeCon args
    if (app->con.args != NULL)
    {
        NecroType* prev_args     = app->con.args;
        NecroType* new_args_curr = necro_type_list_create(arena, prev_args->list.item, NULL);
        NecroType* new_args_head = new_args_curr;
        prev_args                = prev_args->list.next;
        while (prev_args != NULL)
        {
            new_args_curr->list.next = necro_type_list_create(arena, prev_args->list.item, NULL);
            new_args_curr            = new_args_curr->list.next;
            prev_args                = prev_args->list.next;
        }
        new_args_curr->list.next = args;
        args                     = new_args_head;
    }

    // Create TypeCon
    NecroType* con = necro_type_con_create(arena, app->con.con_symbol, args);
    unwrap(void, necro_kind_infer_default(arena, base, con, NULL_LOC, NULL_LOC));
    con->ownership = necro_type_find(app_ownership);
    return con;
}

NecroType* necro_type_fresh_var(NecroPagedArena* arena, NecroScope* scope)
{
    NecroAstSymbol* ast_symbol = necro_ast_symbol_create(arena, NULL, NULL, NULL, NULL);
    NecroType*      type_var   = necro_type_var_create(arena, ast_symbol, scope);
    ast_symbol->type           = type_var;
    return type_var;
}

NecroType* necro_type_strip_for_all(NecroType* type)
{
    type = necro_type_find(type);
    while (type != NULL && type->type == NECRO_TYPE_FOR)
    {
        type = type->for_all.type;
    }
    return type;
}

NecroType* necro_type_find(NecroType* type)
{
    NecroType* prev = type;
    while (type != NULL && type->type == NECRO_TYPE_VAR)
    {
        prev = type;
        type = type->var.bound;
    }
    if (type == NULL)
        return prev;
    else
        return type;
}

const NecroType* necro_type_find_const(const NecroType* type)
{
    const NecroType* prev = type;
    while (type != NULL && type->type == NECRO_TYPE_VAR)
    {
        prev = type;
        type = type->var.bound;
    }
    if (type == NULL)
        return prev;
    else
        return type;
}

bool necro_type_is_bound_in_scope(NecroType* type, NecroScope* scope)
{
    if (type->type != NECRO_TYPE_VAR)
        return false;
    if (type->var.scope == &necro_global_scope)
        return true;
    if (type->var.scope == NULL && scope == NULL)
        return false;
    if (type->var.scope == NULL)
        return false;
    NecroScope* current_scope = scope;
    while (current_scope != NULL)
    {
        if (current_scope == type->var.scope)
            return true;
        else
            current_scope = current_scope->parent;
    }
    return false;
}

size_t necro_type_bound_in_scope_distance(NecroType* type, NecroScope* scope)
{
    assert(type->type == NECRO_TYPE_VAR);
    if (type->var.scope == NULL)
        return 0;
    if (type->var.scope == &necro_global_scope)
        return SIZE_MAX;
    size_t      distance      = 0;
    NecroScope* current_scope = scope;
    while (current_scope != NULL)
    {
        if (current_scope == type->var.scope)
            return distance;
        distance++;
        current_scope = current_scope->parent;
    }
    return distance;
}

NecroType* necro_type_get_fully_applied_fun_type(NecroType* type)
{
    type = necro_type_find(type);
    while (type->type == NECRO_TYPE_FUN)
    {
        type = necro_type_find(type->fun.type2);
    }
    return type;
}

const NecroType* necro_type_get_fully_applied_fun_type_const(const NecroType* type)
{
    type = necro_type_find_const(type);
    while (type->type == NECRO_TYPE_FUN)
    {
        type = necro_type_find(type->fun.type2);
    }
    return type;
}

bool necro_type_is_polymorphic(const NecroType* type)
{
    type = necro_type_find_const(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        return true;
    case NECRO_TYPE_APP:
        return necro_type_is_polymorphic(type->app.type1) || necro_type_is_polymorphic(type->app.type2);
    case NECRO_TYPE_FUN:
        return necro_type_is_polymorphic(type->fun.type1) || necro_type_is_polymorphic(type->fun.type2);
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type->con.args;
        while (args != NULL)
        {
            if (necro_type_is_polymorphic(args->list.item))
                return true;
            args = args->list.next;
        }
        return false;
    }
    case NECRO_TYPE_LIST:
        assert(false);
        return false;
    case NECRO_TYPE_FOR:
        return true;
    case NECRO_TYPE_NAT:
        return false;
    case NECRO_TYPE_SYM:
        return false;
    default:
        assert(false);
        return false;
    }
}

bool necro_type_is_polymorphic_ignoring_ownership(const NecroBase* base, const NecroType* type)
{
    if (type->type == NECRO_TYPE_FOR)
    {
        if (!necro_kind_is_ownership(base, type->for_all.var_symbol->type))
            return true;
        else
            return necro_type_is_polymorphic_ignoring_ownership(base, type->for_all.type);
    }
    else
    {
        return necro_type_is_polymorphic(type);
    }
}

bool necro_type_is_copy_type(const NecroType* type)
{
    type = necro_type_find_const(type);
    if (type->type != NECRO_TYPE_CON)
        return false;
    else
        return type->con.con_symbol->is_enum;
}

bool necro_type_bind_var_to_type_if_instance_of_constraints(NecroPagedArena* arena, NecroType* type, NecroAstSymbol* type_symbol, NecroConstraintList* constraints)
{
    assert(type->type == NECRO_TYPE_VAR);
    assert(!type->var.is_rigid);
    while (constraints != NULL)
    {
        if (constraints->data->type != NECRO_CONSTRAINT_CLASS)
        {
            constraints = constraints->next;
            continue;
        }
        NecroInstanceList* instance_list = type_symbol->instance_list;
        while (instance_list != NULL)
        {
            if (instance_list->data->type_class_name == constraints->data->cls.type_class->type_class_name)
                break;
            instance_list = instance_list->next;
        }
        if (instance_list != NULL)
            constraints = constraints->next;
        else
            return false;
    }
    type->var.bound       = necro_type_con_create(arena, type_symbol, NULL);
    type->var.bound->kind = type_symbol->type->kind;
    return true;
}

bool necro_type_default_uvar(NecroBase* base, NecroType* type)
{
    if (type->kind->type != NECRO_TYPE_CON || type->kind->con.con_symbol != base->ownership_kind)
        return false;
    type->var.bound = base->ownership_share->type;
    return true;
}

bool necro_type_contains_constraint_with_symbol(NecroType* type, NecroAstSymbol* constraint_symbol)
{
    NecroConstraint constraint;
    constraint.type           = NECRO_CONSTRAINT_CLASS;
    constraint.cls.type1      = type;
    constraint.cls.type_class = constraint_symbol->type_class;
    return necro_constraint_find_with_super_classes(type->var.var_symbol->constraints, NULL, &constraint);
}

bool necro_type_default(NecroPagedArena* arena, NecroBase* base, NecroType* type)
{
    assert(type->type == NECRO_TYPE_VAR);
    if (necro_type_default_uvar(base, type))
        return true;
    const bool contains_semi_ring      = necro_type_contains_constraint_with_symbol(type, base->semi_ring_class);
    const bool contains_ring           = necro_type_contains_constraint_with_symbol(type, base->ring_class);
    const bool contains_division_ring  = necro_type_contains_constraint_with_symbol(type, base->division_ring_class);
    const bool contains_euclidean_ring = necro_type_contains_constraint_with_symbol(type, base->euclidean_ring_class);
    const bool contains_field          = necro_type_contains_constraint_with_symbol(type, base->field_class);
    const bool contains_num            = necro_type_contains_constraint_with_symbol(type, base->num_type_class);
    const bool contains_integral       = necro_type_contains_constraint_with_symbol(type, base->integral_class);
    const bool contains_floating       = necro_type_contains_constraint_with_symbol(type, base->floating_class);
    const bool contains_eq             = necro_type_contains_constraint_with_symbol(type, base->eq_type_class);
    const bool contains_ord            = necro_type_contains_constraint_with_symbol(type, base->ord_type_class);
    const bool contains_audio          = necro_type_contains_constraint_with_symbol(type, base->audio_type_class);
    if (contains_audio)
    {
        if (necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->mono_type, type->var.var_symbol->constraints))
            return true;
    }
    else if (contains_floating || contains_field || contains_division_ring)
    {
        if (necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->float_type, type->var.var_symbol->constraints))
            return true;
    }
    else if (contains_integral || contains_euclidean_ring)
    {
        if (necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->int_type, type->var.var_symbol->constraints))
            return true;
    }
    else if (contains_num || contains_ring)
    {
        if (necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->int_type, type->var.var_symbol->constraints) ||
            necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->float_type, type->var.var_symbol->constraints))
            return true;
    }
    else if (contains_semi_ring)
    {
        if (necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->bool_type, type->var.var_symbol->constraints) ||
            necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->int_type, type->var.var_symbol->constraints))
            return true;
    }
    else if (contains_eq || contains_ord)
    {
        if (necro_type_bind_var_to_type_if_instance_of_constraints(arena, type, base->unit_type, type->var.var_symbol->constraints))
            return true;
    }
    return false;
}

NecroResult(bool) necro_type_is_unambiguous_polymorphic(NecroPagedArena* arena, NecroBase* base, NecroType* type, const NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
            return ok(bool, true);
        else if (necro_type_default(arena, base, type))
            return ok(bool, false);
        else
            return necro_type_ambiguous_type_var_error(NULL, type, macro_type, source_loc, end_loc);
    }
    case NECRO_TYPE_APP:
    {
        bool is_type1_poly = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->app.type1, macro_type, source_loc, end_loc));
        bool is_type2_poly = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->app.type2, macro_type, source_loc, end_loc));
        return ok(bool, is_type1_poly || is_type2_poly);
    }
    case NECRO_TYPE_FUN:
    {
        bool is_type1_poly = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->fun.type1, macro_type, source_loc, end_loc));
        bool is_type2_poly = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->fun.type2, macro_type, source_loc, end_loc));
        return ok(bool, is_type1_poly || is_type2_poly);
    }
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type->con.args;
        bool is_poly          = false;
        while (args != NULL)
        {
            const bool is_poly_item = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, args->list.item, macro_type, source_loc, end_loc));
            is_poly                 = is_poly || is_poly_item;
            args                    = args->list.next;
        }
        return ok(bool, is_poly);
    }
    case NECRO_TYPE_LIST:
        assert(false);
        return ok(bool, false);
    case NECRO_TYPE_FOR:
        necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->for_all.type, macro_type, source_loc, end_loc));
        return ok(bool, true);
    case NECRO_TYPE_NAT:
        return ok(bool, false);
    case NECRO_TYPE_SYM:
        return ok(bool, false);
    default:
        assert(false);
        return ok(bool, false);
    }
}

NecroResult(void) necro_type_ambiguous_type_variable_check(NecroPagedArena* arena, NecroBase* base, NecroType* type, const NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (type == NULL)
        return ok_void();
    type = necro_type_find(type);
    // Ambig check utypes
    necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, type->ownership, macro_type, source_loc, end_loc));
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
            return ok_void();
        else if (necro_type_default(arena, base, type))
            return ok_void();
        else
            return necro_error_map(bool, void, necro_type_ambiguous_type_var_error(NULL, type, macro_type, source_loc, end_loc));
    }
    case NECRO_TYPE_APP:
        necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, type->app.type1, macro_type, source_loc, end_loc));
        necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, type->app.type2, macro_type, source_loc, end_loc));
        return ok_void();
    case NECRO_TYPE_FUN:
        necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, type->fun.type1, macro_type, source_loc, end_loc));
        necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, type->fun.type2, macro_type, source_loc, end_loc));
        return ok_void();
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type->con.args;
        while (args != NULL)
        {
            necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, args->list.item, macro_type, source_loc, end_loc));
            args = args->list.next;
        }
        return ok_void();
    }
    case NECRO_TYPE_LIST:
        assert(false);
        return ok_void();
    case NECRO_TYPE_FOR:
        necro_try(void, necro_type_ambiguous_type_variable_check(arena, base, type->for_all.type, macro_type, source_loc, end_loc));
        return ok_void();
    case NECRO_TYPE_NAT:
        return ok_void();
    case NECRO_TYPE_SYM:
        return ok_void();
    default:
        assert(false);
        return ok_void();
    }
}

NecroInstSub* necro_type_filter_and_deep_copy_subs(NecroPagedArena* arena, NecroInstSub* subs, NecroAstSymbol* var_to_replace, NecroType* new_name)
{
    if (subs == NULL)
        return NULL;
    if ((subs->var_to_replace == var_to_replace ||
        (subs->var_to_replace->name != NULL && subs->var_to_replace->name == var_to_replace->name))
        &&
        necro_type_exact_unify(subs->new_name, new_name))
    {
        return necro_type_filter_and_deep_copy_subs(arena, subs->next, var_to_replace, new_name);
    }
    else
    {
        NecroInstSub* new_sub = necro_paged_arena_alloc(arena, sizeof(NecroInstSub));
        *new_sub              = *subs;
        new_sub->next         = necro_type_filter_and_deep_copy_subs(arena, subs->next, var_to_replace, new_name);
        return new_sub;
    }
}

// NecroType* necro_type_replace_with_subs_go(NecroPagedArena* arena, NecroType* type, NecroInstSub* subs);
NecroType* necro_type_replace_with_subs_deep_copy_go(NecroPagedArena* arena, NecroBase* base, NecroConstraintEnv* con_env, NecroType* type, NecroInstSub* subs);

NecroInstSub* necro_type_union_subs(NecroPagedArena* arena, NecroBase* base, NecroInstSub* subs1, NecroInstSub* subs2)
{
    if (subs1 == NULL)
        return NULL;
    if (subs2 == NULL)
        return subs1;
    NecroType*    subs1_new_name = necro_type_find(subs1->new_name);
    NecroInstSub* curr_sub2      = subs2;
    while (curr_sub2 != NULL)
    {
        if (subs1->var_to_replace == curr_sub2->var_to_replace ||
           (subs1->var_to_replace->name != NULL &&  subs1->var_to_replace->name == curr_sub2->var_to_replace->name))
        {
            subs1->new_name = necro_type_find(curr_sub2->new_name);
            break;
            // subs1->next = necro_type_union_subs(subs1->next, subs2);
            // return subs1;
        }
        else if (subs1_new_name->type == NECRO_TYPE_VAR &&
                (subs1_new_name->var.var_symbol == curr_sub2->var_to_replace ||
                (subs1_new_name->var.var_symbol->name != NULL && subs1_new_name->var.var_symbol->name == curr_sub2->var_to_replace->name)))
        {
            subs1->new_name = necro_type_find(curr_sub2->new_name);
            break;
            // subs1->next = necro_type_union_subs(subs1->next, subs2);
            // return subs1;
        }
        else
        {
            subs1->new_name = necro_type_replace_with_subs_deep_copy(arena, NULL, base, subs1->new_name, subs2);
        }
        curr_sub2 = curr_sub2->next;
    }
    subs1->next = necro_type_union_subs(arena, base, subs1->next, subs2);
    return subs1;
    // return necro_type_union_subs(subs1->next, subs2);
}

NecroInstSub* necro_type_deep_copy_subs(NecroPagedArena* arena, NecroInstSub* subs)
{
    if (subs == NULL)
        return NULL;
    NecroInstSub* new_sub = necro_paged_arena_alloc(arena, sizeof(NecroInstSub));
    *new_sub = *subs;
    new_sub->next = necro_type_deep_copy_subs(arena, subs->next);
    return new_sub;
}

//=====================================================
// Unify
//=====================================================
NecroResult(NecroType) necro_type_occurs(NecroAstSymbol* type_var_symbol, NecroType* type)
{
    if (type == NULL)
        return ok(NecroType, NULL);
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (type_var_symbol == type->var.var_symbol)
            return necro_type_occurs_error_partial(type_var_symbol->type, type);
        else
            return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->app.type1));
        return necro_type_occurs(type_var_symbol, type->app.type2);
    case NECRO_TYPE_FUN:
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->fun.type1));
        return necro_type_occurs(type_var_symbol, type->fun.type2);
    case NECRO_TYPE_CON:
        return necro_type_occurs(type_var_symbol, type->con.args);
    case NECRO_TYPE_LIST:
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->list.item));
        return necro_type_occurs(type_var_symbol, type->list.next);
    case NECRO_TYPE_NAT:
        return ok(NecroType, NULL);
    case NECRO_TYPE_SYM:
        return ok(NecroType, NULL);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_type_occurs_flipped(NecroAstSymbol* type_var_symbol, NecroType* type)
{
    NecroResult(NecroType) result = necro_type_occurs(type_var_symbol, type);
    if (result.type == NECRO_RESULT_OK)
        return result;
    NecroType* type1 = result.error->default_type_error_data2.type1;
    NecroType* type2 = result.error->default_type_error_data2.type2;
    result.error->default_type_error_data2.type1 = type2;
    result.error->default_type_error_data2.type2 = type1;
    return result;
}

NecroResult(NecroType) necro_type_bind_var(NecroType* var_to_bind, NecroType* type_to_bind_to)
{
    // Bind
    type_to_bind_to = necro_type_find(type_to_bind_to);
    var_to_bind     = necro_type_find(var_to_bind);
    assert(var_to_bind->type == NECRO_TYPE_VAR);
    var_to_bind->var.bound = type_to_bind_to;
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_unify_var(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    UNUSED(arena);
    UNUSED(con_env);
    UNUSED(base);
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_VAR);
    assert(type2 != NULL);
    if (type1 == type2)
        return ok(NecroType, NULL);
    necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type1->var.var_symbol == type2->var.var_symbol)  return necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope);
        // if (type1->var.var_symbol == type2->var.var_symbol)  return ok(NecroType, NULL);
        else if (type1->var.is_rigid && type2->var.is_rigid) return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol, type2));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        if (type1->var.is_rigid)                             return necro_type_bind_var(type2, type1);
        else if (type2->var.is_rigid)                        return necro_type_bind_var(type1, type2);
        const bool is_type1_bound_in_scope = necro_type_is_bound_in_scope(type1, scope);
        const bool is_type2_bound_in_scope = necro_type_is_bound_in_scope(type2, scope);
        if (is_type1_bound_in_scope && is_type2_bound_in_scope)
        {
            const size_t type1_distance = necro_type_bound_in_scope_distance(type1, scope);
            const size_t type2_distance = necro_type_bound_in_scope_distance(type2, scope);
            if (type1_distance >= type2_distance)
                return necro_type_bind_var(type2, type1);
            else
                return necro_type_bind_var(type1, type2);
        }
        else if (is_type1_bound_in_scope)                                                                  return necro_type_bind_var(type2, type1);
        else if (type1->var.var_symbol->constraints != NULL && type2->var.var_symbol->constraints == NULL) return necro_type_bind_var(type2, type1);
        else                                                                                               return necro_type_bind_var(type1, type2);
    case NECRO_TYPE_FUN:
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        if (type1->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol , type2));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        necro_try(NecroType, necro_type_bind_var(type1, type2));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_app(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_APP);
    assert(type2 != NULL);
    necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs_flipped(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        necro_try(NecroType, necro_type_bind_var(type2, type1));
        return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->app.type2, type2->app.type2, scope));
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->app.type1, type2->app.type1, scope));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
    {
        int32_t    arg_count    = (int32_t) necro_type_list_count(type2->con.args);
        NecroType* curr_app_arg = type1;
        int32_t    app_count    = 0;
        while (curr_app_arg->type == NECRO_TYPE_APP)
        {
            app_count++;
            curr_app_arg = necro_type_find(curr_app_arg->app.type1);
        }
        int32_t app_diff = arg_count - app_count;
        if (app_diff != 0)
        {
            if (app_diff < 0)
                assert(false && "TODO: kind error?");
            NecroType* con_kind = necro_type_find(type2->con.con_symbol->type->kind);
            for (int32_t i = 0; i < app_diff; ++i)
                con_kind = necro_type_find(con_kind->fun.type2);
            necro_try(NecroType, necro_kind_unify(curr_app_arg->kind, con_kind, scope));
        }
        else
        {
            necro_try(NecroType, necro_kind_unify(curr_app_arg->kind, type2->con.con_symbol->type->kind, scope));
        }
        curr_app_arg = type1;
        while (curr_app_arg->type == NECRO_TYPE_APP)
        {
            NecroType* curr_con_arg = type2->con.args;
            for (int32_t i = 1; i < arg_count; ++i)
            {
                assert(curr_con_arg->type == NECRO_TYPE_LIST);
                curr_con_arg = curr_con_arg->list.next;
            }
            curr_con_arg = necro_type_find(curr_con_arg->list.item);
            necro_try(NecroType, necro_type_unify(arena, con_env, base, curr_con_arg, necro_type_find(curr_app_arg->app.type2), scope));
            arg_count--;
            curr_app_arg = necro_type_find(curr_app_arg->app.type1);
        }
        if (curr_app_arg->type == NECRO_TYPE_VAR)
        {
            NecroType* curr_args     = type2->con.args;
            NecroType* new_args_head = NULL;
            NecroType* new_args_curr = NULL;
            for (int32_t i = 0; i < app_diff; ++i)
            {
                if (new_args_head == NULL)
                {
                    new_args_head = necro_type_list_create(arena, curr_args->list.item, NULL);
                    new_args_curr = new_args_head;
                }
                else
                {
                    new_args_curr->list.next = necro_type_list_create(arena, curr_args->list.item, NULL);
                    new_args_curr            = new_args_curr->list.next;
                }
            }
            curr_app_arg->var.bound = necro_type_con_create(arena, type2->con.con_symbol, new_args_head);
            unwrap(NecroType, necro_kind_infer(arena, base, curr_app_arg->var.bound, NULL_LOC, NULL_LOC));
        }
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        *type1 = *type2;
        return ok(NecroType, NULL);
    }
    case NECRO_TYPE_FUN:
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        return necro_type_mismatched_type_error_partial(type1, type2);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_fun(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_FUN);
    assert(type2 != NULL);
    necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs_flipped(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        necro_try(NecroType, necro_type_bind_var(type2, type1));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->fun.type2, type2->fun.type2, scope));
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->fun.type1, type2->fun.type1, scope));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
    case NECRO_TYPE_CON:
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        return necro_type_mismatched_type_error_partial(type1, type2);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_con(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_CON);
    assert(type2 != NULL);
    necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs_flipped(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        necro_try(NecroType, necro_type_bind_var(type2, type1));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
        if (type1->con.con_symbol != type2->con.con_symbol)
        {
            return necro_type_mismatched_type_error_partial(type1, type2);
        }
        else
        {
            NecroType* original_type1 = type1;
            NecroType* original_type2 = type2;
            type1 = type1->con.args;
            type2 = type2->con.args;
            while (type1 != NULL && type2 != NULL)
            {
                assert(type1->type == NECRO_TYPE_LIST);
                assert(type2->type == NECRO_TYPE_LIST);
                necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->list.item, type2->list.item, scope));
                // necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1->list.item, type2->list.item, scope));
                type1 = type1->list.next;
                type2 = type2->list.next;
            }
            necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, original_type1, original_type2, scope));
            return ok(NecroType, NULL);
        }
    case NECRO_TYPE_APP:
    {
        int32_t    app_count    = 0;
        int32_t    arg_count    = (int32_t) necro_type_list_count(type1->con.args);
        NecroType* curr_app_arg = type2;
        while (curr_app_arg->type == NECRO_TYPE_APP)
        {
            app_count++;
            curr_app_arg = necro_type_find(curr_app_arg->app.type1);
        }

        int32_t app_diff = arg_count - app_count;
        if (app_diff != 0)
        {
            if (app_diff < 0)
                assert(false && "TODO: kind error?");
            NecroType* con_kind = necro_type_find(type1->con.con_symbol->type->kind);
            for (int32_t i = 0; i < app_diff; ++i)
                con_kind = necro_type_find(con_kind->fun.type1);
            necro_try(NecroType, necro_kind_unify(curr_app_arg->kind, con_kind, scope));
        }
        else
        {
            necro_try(NecroType, necro_kind_unify(curr_app_arg->kind, type1->con.con_symbol->type->kind, scope));
        }

        curr_app_arg = type2;
        while (curr_app_arg->type == NECRO_TYPE_APP)
        {
            NecroType* curr_con_arg = type1->con.args;
            for (int32_t i = 1; i < arg_count; ++i)
            {
                assert(curr_con_arg->type == NECRO_TYPE_LIST);
                curr_con_arg = curr_con_arg->list.next;
            }
            curr_con_arg = necro_type_find(curr_con_arg->list.item);
            necro_try(NecroType, necro_type_unify(arena, con_env, base, curr_con_arg, necro_type_find(curr_app_arg->app.type2), scope));
            arg_count--;
            curr_app_arg = necro_type_find(curr_app_arg->app.type1);
        }
        if (curr_app_arg->type == NECRO_TYPE_VAR)
        {
            NecroType* curr_args     = type1->con.args;
            NecroType* new_args_head = NULL;
            NecroType* new_args_curr = NULL;
            for (int32_t i = 0; i < app_diff; ++i)
            {
                if (new_args_head == NULL)
                {
                    new_args_head = necro_type_list_create(arena, curr_args->list.item, NULL);
                    new_args_curr = new_args_head;
                }
                else
                {
                    new_args_curr->list.next = necro_type_list_create(arena, curr_args->list.item, NULL);
                    new_args_curr            = new_args_curr->list.next;
                }
            }
            curr_app_arg->var.bound = necro_type_con_create(arena, type1->con.con_symbol, new_args_head);
            unwrap(NecroType, necro_kind_infer(arena, base, curr_app_arg->var.bound, NULL_LOC, NULL_LOC));
        }
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        *type2 = *type1;
        return ok(NecroType, NULL);
    }
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
    case NECRO_TYPE_FUN:
        return necro_type_mismatched_type_error_partial(type1, type2);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_nat(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    UNUSED(arena);
    UNUSED(con_env);
    UNUSED(base);
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_NAT);
    assert(type2 != NULL);
    necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs_flipped(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_type_bind_var(type2, type1));
        return ok(NecroType, NULL);
    case NECRO_TYPE_NAT:
        if (type1->nat.value != type2->nat.value)
            return necro_type_mismatched_type_error_partial(type1, type2);
        else
            return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_CON:
    case NECRO_TYPE_SYM:
        return necro_type_mismatched_type_error_partial(type1, type2);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_sym(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    UNUSED(arena);
    UNUSED(con_env);
    UNUSED(base);
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_SYM);
    assert(type2 != NULL);
    necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs_flipped(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_type_bind_var(type2, type1));
        return ok(NecroType, NULL);
    case NECRO_TYPE_SYM:
        if (type1->sym.value != type2->sym.value)
            return necro_type_mismatched_type_error_partial(type1, type2);
        else
            return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_CON:
    case NECRO_TYPE_NAT:
        return necro_type_mismatched_type_error_partial(type1, type2);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

// NOTE: Can only unify with a polymorphic type on the left side
NecroResult(NecroType) necro_type_unify(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    type1 = necro_type_find(type1);
    type2 = necro_type_find(type2);
    if (type1 == type2)
        return ok(NecroType, NULL);

    if (type1->type == NECRO_TYPE_APP && necro_type_is_type_app_type_con(type1))
        *type1 = *necro_type_uncurry_app(arena, base, type1);
    if (type2->type == NECRO_TYPE_APP && necro_type_is_type_app_type_con(type2))
        *type2 = *necro_type_uncurry_app(arena, base, type2);

    // necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));

    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  return necro_unify_var(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_APP:  return necro_unify_app(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_FUN:  return necro_unify_fun(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_CON:  return necro_unify_con(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_NAT:  return necro_unify_nat(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_SYM:  return necro_unify_sym(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:
        while (type1->type == NECRO_TYPE_FOR)
            type1 = type1->for_all.type;
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1, type2, scope));
        return ok(NecroType, NULL);
    default:
        necro_unreachable(NecroType);
    }
}

// TODO: Extend to cover more types
void necro_type_unify_con_uninhabited_args(NecroPagedArena* arena, struct NecroBase* base, NecroType* type1, NecroType* type2)
{
    if (type1 == NULL || type2 == NULL)
        return;
    type1 = necro_type_find(type1);
    type2 = necro_type_find(type2);
    if (type1->type != NECRO_TYPE_CON || type2->type != NECRO_TYPE_CON)
        return;
    if (type1->con.con_symbol != type2->con.con_symbol)
        return;
    NecroType* con_args1 = type1->con.args;
    NecroType* con_args2 = type2->con.args;
    while (con_args1 != NULL)
    {
        assert(con_args1->type == NECRO_TYPE_LIST);
        assert(con_args2->type == NECRO_TYPE_LIST);
        NecroType* con_arg1 = necro_type_find(con_args1->list.item);
        NecroType* con_arg2 = necro_type_find(con_args2->list.item);
        if (con_arg1->kind->type != NECRO_TYPE_CON || con_arg1->kind->con.con_symbol != base->star_kind)
        {
            unwrap(NecroType, necro_type_unify(arena, NULL, base, con_arg1, con_arg2, NULL));
        }
        con_args1 = con_args1->list.next;
        con_args2 = con_args2->list.next;
    }
}

NecroResult(NecroType) necro_type_unify_with_info(NecroPagedArena* arena, NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_type_unify_with_full_info(arena, con_env, base, type1, type2, scope, source_loc, end_loc, type1, type2);
}

// TODO: Refactor! Replace with context struct which holds useful info for error messages!
NecroResult(NecroType) necro_type_unify_with_full_info(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroType* macro_type1, NecroType* macro_type2)
{
    if (con_env != NULL)
    {
        con_env->source_loc = source_loc;
        con_env->end_loc    = end_loc;
    }
    NecroResult(NecroType) result = necro_type_unify(arena, con_env, base, type1, type2, scope);
    if (result.type == NECRO_RESULT_OK)
    {
        return result;
    }
    assert(result.error != NULL);
    switch (result.error->type)
    {
    case NECRO_TYPE_RIGID_TYPE_VARIABLE:
    case NECRO_TYPE_OCCURS:
    case NECRO_TYPE_MISMATCHED_TYPE:
        result.error->default_type_error_data2.macro_type1 = macro_type1;
        result.error->default_type_error_data2.macro_type2 = macro_type2;
        result.error->default_type_error_data2.source_loc  = source_loc;
        result.error->default_type_error_data2.end_loc     = end_loc;
        break;

    case NECRO_KIND_MISMATCHED_KIND:
        result.error->default_type_error_data2.macro_type1 = macro_type1;
        result.error->default_type_error_data2.macro_type2 = macro_type2;
        result.error->default_type_error_data2.source_loc  = source_loc;
        result.error->default_type_error_data2.end_loc     = end_loc;
        break;

    case NECRO_TYPE_NOT_AN_INSTANCE_OF:
        result.error->default_type_class_error_data.source_loc  = source_loc;
        result.error->default_type_class_error_data.end_loc     = end_loc;
        result.error->default_type_class_error_data.macro_type1 = macro_type1;
        result.error->default_type_class_error_data.macro_type2 = macro_type2;
        break;

    case NECRO_TYPE_MISMATCHED_ARITY:
    case NECRO_TYPE_POLYMORPHIC_PAT_BIND:
    case NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE:
    case NECRO_TYPE_FINAL_DO_STATEMENT:
    case NECRO_TYPE_AMBIGUOUS_CLASS:
    case NECRO_TYPE_CONSTRAINS_ONLY_CLASS_VAR:
    case NECRO_TYPE_AMBIGUOUS_TYPE_VAR:
    case NECRO_TYPE_NON_CONCRETE_INITIALIZED_VALUE:
    case NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE:
    case NECRO_TYPE_NOT_A_CLASS:
    case NECRO_TYPE_NOT_A_VISIBLE_METHOD:
    case NECRO_TYPE_NO_EXPLICIT_IMPLEMENTATION:
    case NECRO_TYPE_DOES_NOT_IMPLEMENT_SUPER_CLASS:
    case NECRO_TYPE_MULTIPLE_CLASS_DECLARATIONS:
    case NECRO_TYPE_MULTIPLE_INSTANCE_DECLARATIONS:
        assert(false);
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

//=====================================================
// Inst
//=====================================================
NecroInstSub* necro_create_inst_sub(NecroPagedArena* arena, NecroAstSymbol* var_to_replace, NecroScope* scope, NecroInstSub* next)
{
    NecroType* type_to_replace            = necro_type_find(var_to_replace->type);
    NecroType* type_var                   = necro_type_fresh_var(arena, scope);
    type_var->var.is_rigid                = false;
    assert(var_to_replace->name != NULL);
    type_var->var.var_symbol->name        = var_to_replace->name; // Experiment...attempting to keep the same "name" after sub
    type_var->var.var_symbol->source_name = var_to_replace->source_name;
    // type_var->var.var_symbol->source_name = type_var->var.var_symbol->source_name;
    // type_var->var.var_symbol->source_name = type_var->var.var_symbol->name;
    type_var->var.var_symbol->module_name = var_to_replace->module_name;
    type_var->kind                        = type_to_replace->kind;
    type_var->ownership                   = type_to_replace->ownership;
    type_var->var.scope                   = NULL;
    // type_var->var.scope                   = type_to_replace->var.scope;
    NecroInstSub* sub = necro_paged_arena_alloc(arena, sizeof(NecroInstSub));
    *sub              = (NecroInstSub)
    {
        .var_to_replace = var_to_replace,
        .new_name       = type_var,
        .next           = next,
    };
    type_var->var.var_symbol->constraints = type_to_replace->var.var_symbol->constraints; // NOTE: This will later be instantiated in the second step!
    return sub;
}

NecroType* necro_type_maybe_sub_var(NecroType* type_to_maybe_sub, NecroInstSub* subs)
{
    // assert(type_to_maybe_sub->type == NECRO_TYPE_VAR);
    if (type_to_maybe_sub->type != NECRO_TYPE_VAR)
        return type_to_maybe_sub;
    // TODO: Figure this out!
    // assert(necro_type_find(type_to_maybe_sub->var.var_symbol->type) == type_to_maybe_sub);
    NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        // TODO: Bang around source_name comparison here...is that too lax?
        //       Figure out a more reliable solution for this!
        if (type_to_maybe_sub->var.var_symbol == curr_sub->var_to_replace || type_to_maybe_sub == subs->new_name ||
           (type_to_maybe_sub->var.var_symbol->name != NULL && type_to_maybe_sub->var.var_symbol->name == curr_sub->var_to_replace->name)
           // || (type_to_maybe_sub->var.var_symbol->source_name != NULL && type_to_maybe_sub->var.var_symbol->source_name == curr_sub->var_to_replace->source_name)
            )
            return curr_sub->new_name;
        curr_sub = curr_sub->next;
    }
    return type_to_maybe_sub;
}

NecroConstraintList* necro_type_constraint_replace_with_subs_deep_copy(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroConstraintList* constraints, NecroInstSub* subs, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroConstraintList* new_list = NULL;
    while (constraints != NULL)
    {
        switch (constraints->data->type)
        {
            case NECRO_CONSTRAINT_UCONSTRAINT:
            {
                NecroType* u1 = necro_type_replace_with_subs_deep_copy(arena, con_env, base, constraints->data->uconstraint.u1, subs);
                NecroType* u2 = necro_type_replace_with_subs_deep_copy(arena, con_env, base, constraints->data->uconstraint.u2, subs);
                new_list      = necro_constraint_append_uniqueness_constraint_and_queue_push_back(arena, con_env, u1, u2, source_loc, end_loc, new_list);
                break;
            }
            case NECRO_CONSTRAINT_CLASS:
            {
                NecroType* type1 = necro_type_replace_with_subs_deep_copy(arena, con_env, base, constraints->data->cls.type1, subs);
                new_list         = necro_constraint_append_class_constraint_and_queue_push_back(arena, con_env, constraints->data->cls.type_class, type1, source_loc, end_loc, new_list);
                break;
            }
            default: assert(false); break;
        }
        constraints = constraints->next;
    }
    return new_list;
}

void necro_type_collapse_app_cons(NecroPagedArena* arena, NecroBase* base, NecroType* type)
{
    if (type == NULL)
        return;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_APP:
        necro_type_collapse_app_cons(arena, base, type->app.type1);
        necro_type_collapse_app_cons(arena, base, type->app.type2);
        if (necro_type_is_type_app_type_con(type))
            *type = *necro_type_uncurry_app(arena, base, type);
        break;
    case NECRO_TYPE_FOR:  necro_type_collapse_app_cons(arena, base, type->for_all.type); break;
    case NECRO_TYPE_FUN:  necro_type_collapse_app_cons(arena, base, type->fun.type1); necro_type_collapse_app_cons(arena, base, type->fun.type2); break;
    case NECRO_TYPE_CON:  necro_type_collapse_app_cons(arena, base, type->con.args); break;
    case NECRO_TYPE_LIST: necro_type_collapse_app_cons(arena, base, type->list.item); necro_type_collapse_app_cons(arena, base, type->list.next); break;
    case NECRO_TYPE_VAR:  break;
    case NECRO_TYPE_NAT:  break;
    case NECRO_TYPE_SYM:  break;
    default:
        assert(false);
        break;
    }
}

// TODO: infer kinds!?
NecroType* necro_type_replace_with_subs_deep_copy_go(NecroPagedArena* arena, NecroBase* base, NecroConstraintEnv* con_env, NecroType* type, NecroInstSub* subs)
{
    if (type == NULL)
        return NULL;
    type                = necro_type_find(type);
    NecroType* new_type = NULL;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        new_type = necro_type_maybe_sub_var(type, subs);
        if (new_type == type)
            return type;
        break;
    case NECRO_TYPE_FOR:
    {
        NecroType* forall_var = necro_type_maybe_sub_var(type->for_all.var_symbol->type, subs);
        if (forall_var != type->for_all.var_symbol->type)
            new_type = necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->for_all.type, subs);
        else
            new_type = necro_type_for_all_create(arena, type->for_all.var_symbol, necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->for_all.type, subs));
        break;
    }
    case NECRO_TYPE_APP:
    {
        NecroType* app          = type;
        NecroType* new_app_head = NULL;
        NecroType* new_app_curr = NULL;
        while (app->type == NECRO_TYPE_APP)
        {
            NecroType* arg = necro_type_find(necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, app->app.type2, subs));
            if (new_app_head == NULL)
            {
                new_app_head = necro_type_app_create(arena, NULL, arg);
                new_app_curr = new_app_head;
            }
            else
            {
                new_app_curr->app.type1 = necro_type_app_create(arena, NULL, arg);
                new_app_curr            = new_app_curr->app.type1;
            }
            app = necro_type_find(app->app.type1);
        }
        new_app_curr->app.type1 = necro_type_find(necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, app, subs));
        new_app_curr            = new_app_curr->app.type1;
        assert(new_app_head != NULL);
        if (new_app_curr->type == NECRO_TYPE_CON)
        {
            new_type = necro_type_uncurry_app(arena, base, new_app_head);
        }
        else
        {
            new_type = new_app_head;
        }
        break;
    }
    case NECRO_TYPE_FUN:
    {
        new_type = necro_type_fn_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->fun.type1, subs), necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->fun.type2, subs));
        break;
    }
    case NECRO_TYPE_CON:  new_type = necro_type_con_create(arena, type->con.con_symbol, necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->con.args, subs)); break;
    case NECRO_TYPE_LIST: new_type = necro_type_list_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->list.item, subs), necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->list.next, subs)); break;
    case NECRO_TYPE_NAT:  new_type = necro_type_nat_create(arena, type->nat.value); break;
    case NECRO_TYPE_SYM:  new_type = necro_type_sym_create(arena, type->sym.value); break;
    default: assert(false); return NULL;
    }
    if (type == base->ownership_share->type || type == base->ownership_steal->type || (type->kind != NULL && necro_kind_is_ownership(base, type)))
    {
        type->ownership = NULL;
    }
    else if (type->kind == NULL || !necro_type_is_inhabited(base, type))
    {
        type->ownership = base->ownership_share->type;
    }
    else if (type->ownership != NULL)
    {
        new_type->ownership       = necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type->ownership, subs);
        new_type->ownership->kind = type->ownership->kind;
    }
    new_type->kind = type->kind;
    // new_type->kind = NULL;
    return new_type;
}

NecroType* necro_type_replace_with_subs_deep_copy(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroInstSub* subs)
{
    NecroType* new_type = necro_type_replace_with_subs_deep_copy_go(arena, base, con_env, type, subs);
    if (new_type == NULL)
        return NULL;
    // necro_type_collapse_app_cons(arena, base, new_type);
    unwrap(NecroType, necro_kind_infer(arena, base, new_type, NULL_LOC, NULL_LOC));
    return new_type;
}

// TODO: Remove NecroResult and unwrap at kinds check during necro_type_replace_with_subs_deep_copy?
NecroType* necro_type_instantiate(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    assert(type != NULL);
    type                       = necro_type_find(type);
    // Create Subs
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs         = necro_create_inst_sub(arena, current_type->for_all.var_symbol, scope, subs);
        current_type = current_type->for_all.type;
    }
    // Instantiate Constraints
    if (con_env != NULL)
    {
        NecroInstSub* curr_sub = subs;
        while (curr_sub)
        {
            assert(curr_sub->new_name->type == NECRO_TYPE_VAR);
            if (curr_sub->new_name->var.var_symbol->type->var.var_symbol->constraints != NULL)
                curr_sub->new_name->var.var_symbol->type->var.var_symbol->constraints = necro_type_constraint_replace_with_subs_deep_copy(arena, con_env, base, curr_sub->new_name->var.var_symbol->type->var.var_symbol->constraints, subs, source_loc, end_loc);
            curr_sub = curr_sub->next;
        }
    }
    // Instantiate Type
    NecroType* result = necro_type_replace_with_subs_deep_copy(arena, con_env, base, current_type, subs);
    return result;
}

NecroType* necro_type_instantiate_with_subs(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope, NecroInstSub** subs, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    assert(type != NULL);
    type                       = necro_type_find(type);
    // Create Subs
    NecroType*    current_type = type;
    *subs                      = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        *subs        = necro_create_inst_sub(arena, current_type->for_all.var_symbol, scope, *subs);
        current_type = current_type->for_all.type;
    }
    // Instantiate Constraints
    if (con_env != NULL)
    {
        NecroInstSub* curr_sub = *subs;
        while (curr_sub)
        {
            assert(curr_sub->new_name->type == NECRO_TYPE_VAR);
            if (curr_sub->new_name->var.var_symbol->type->var.var_symbol->constraints != NULL)
                curr_sub->new_name->var.var_symbol->type->var.var_symbol->constraints = necro_type_constraint_replace_with_subs_deep_copy(arena, con_env, base, curr_sub->new_name->var.var_symbol->type->var.var_symbol->constraints, *subs, source_loc, end_loc);
            curr_sub = curr_sub->next;
        }
    }
    // Instantiate Type
    NecroType* result = necro_type_replace_with_subs_deep_copy(arena, con_env, base, current_type, *subs);
    return result;
}

//=====================================================
// Gen
//=====================================================
void necro_type_name_if_anon_type(NecroBase* base, NecroIntern* intern, NecroType* type)
{
    assert(type != NULL);
    assert(type->kind != NULL);
    type = necro_type_find(type);
    if (type->type == NECRO_TYPE_VAR && (type->var.var_symbol->name == NULL || type->var.var_symbol->source_name == NULL))
    {
        if (necro_kind_is_ownership(base, type))
        {
            type->var.var_symbol->name = necro_intern_unique_string(intern, "u");
            // type->var.var_symbol->source_name = type->var.var_symbol->name;
        }
        else
        {
            type->var.var_symbol->name = necro_intern_unique_string(intern, "t");
            // type->var.var_symbol->source_name = type->var.var_symbol->name;
        }
    }
}

void necro_type_append_to_foralls(NecroPagedArena* arena, NecroBase* base, NecroIntern* intern, NecroType** for_alls, NecroType* type)
{
    NecroType* curr = *for_alls;
    while (curr != NULL)
    {
        if (curr->for_all.var_symbol == type->var.var_symbol)
            return;
        curr = curr->for_all.type;
    }
    necro_type_name_if_anon_type(base, intern, type);
    NecroType* for_all    = necro_type_for_all_create(arena, type->var.var_symbol, NULL);
    for_all->kind         = NULL;
    for_all->ownership    = NULL;
    for_all->pre_supplied = type->pre_supplied;
    if (*for_alls != NULL)
    {
        curr = *for_alls;
        while (curr->for_all.type != NULL)
            curr = curr->for_all.type;
        curr->for_all.type = for_all;
    }
    else
    {
        *for_alls = for_all;
    }
}

NecroType* necro_type_gen_go(NecroPagedArena* arena, NecroBase* base, NecroIntern* intern, NecroType* type, NecroScope* scope, NecroType** for_alls)
{
    if (type == NULL)
        return NULL;
    type = necro_type_find(type);
    if (type->type == NECRO_TYPE_APP && necro_type_is_type_app_type_con(type))
        *type = *necro_type_uncurry_app(arena, base, type);

    assert(type->type == NECRO_TYPE_LIST || type->kind != NULL);

    NecroType* new_ownership = base->ownership_share->type;
    if (type->type != NECRO_TYPE_LIST && necro_kind_is_type(base, type))
    {
        NecroType* ownership = necro_type_find(type->ownership);
        if (ownership->kind == NULL)
            ownership->kind = base->ownership_kind->type;
        new_ownership = necro_type_gen_go(arena, base, intern, ownership, scope, for_alls);
    }

    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
        {
            necro_type_append_to_foralls(arena, base, intern, for_alls, type);
            return type;
        }
        else if (necro_type_is_bound_in_scope(type, scope))
        {
            return type;
        }
        type->var.is_rigid         = true;
        type->ownership            = new_ownership;
        type->var.var_symbol->type = type; // IS THIS NECESSARY!?
        necro_type_append_to_foralls(arena, base, intern, for_alls, type);
        return type;
    }

    case NECRO_TYPE_APP:
    {
        NecroType* type1       = necro_type_gen_go(arena, base, intern, type->app.type1, scope, for_alls);
        NecroType* type2       = necro_type_gen_go(arena, base, intern, type->app.type2, scope, for_alls);
        NecroType* new_type    = necro_type_app_create(arena, type1, type2);
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* type1       = necro_type_gen_go(arena, base, intern, type->fun.type1, scope, for_alls);
        NecroType* type2       = necro_type_gen_go(arena, base, intern, type->fun.type2, scope, for_alls);
        NecroType* new_type    = necro_type_fn_create(arena, type1, type2);
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* item_type   = necro_type_gen_go(arena, base, intern, type->list.item, scope, for_alls);
        NecroType* next_type   = necro_type_gen_go(arena, base, intern, type->list.next, scope, for_alls);
        NecroType* new_type    = necro_type_list_create(arena, item_type, next_type);
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_CON:
    {
        NecroType* new_type    = necro_type_con_create(arena, type->con.con_symbol, necro_type_gen_go(arena, base, intern, type->con.args, scope, for_alls));
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_NAT:
    {
        return type;
    }
    case NECRO_TYPE_SYM:
    {
        return type;
    }
    case NECRO_TYPE_FOR: assert(false); return NULL;
    default:             assert(false); return NULL;
    }
}

// TODO / NOTE: This only handles rigid type vars currently...
// TODO: Normalize flag for foralls
void necro_type_normalize_type_var_names(NecroIntern* intern, NecroBase* base, NecroType* foralls, NecroType* type)
{
    // LOOP through all type vars in type
    type    = necro_type_find(type);
    foralls = necro_type_find(foralls);
    while (foralls != NULL && foralls->type == NECRO_TYPE_FOR && !foralls->for_all.is_normalized && type != NULL && type->type == NECRO_TYPE_FOR)
    {
        // LOOP until unique name found
        const bool is_uvar = necro_kind_is_ownership(base, type->var.var_symbol->type);
        const bool is_kvar = necro_kind_is_kind(base, type->var.var_symbol->type);
        const bool is_hvar = !necro_kind_is_type(base, type->var.var_symbol->type);
        char curr_name[3];
        if (is_uvar)
            curr_name[0] = 'u';
        else if (is_kvar)
            curr_name[0] = 'k';
        else if (is_hvar)
            curr_name[0] = 'f';
        else
            curr_name[0] = 'a';
        curr_name[1] = '\0';
        curr_name[2] = '\0';
        size_t clash_suffix   = 0;
        bool   maybe_clashing = true;
        while (maybe_clashing)
        {
            // LOOP through all type variables in type
            maybe_clashing              = false;
            NecroAstSymbol* var_symbol  = type->for_all.var_symbol;
            NecroType*      curr_forall = foralls;
            while (curr_forall != NULL && curr_forall->type == NECRO_TYPE_FOR)
            {
                NecroAstSymbol* forall_symbol = curr_forall->for_all.var_symbol;
                const bool      is_same_type  = var_symbol == forall_symbol;
                const bool      is_name_null  = var_symbol->source_name == NULL;
                if (!is_same_type || is_name_null)
                {
                    if (var_symbol->source_name == NULL || (!is_same_type && (var_symbol->source_name == forall_symbol->source_name && !type->pre_supplied)))
                    {
                        // Handle anon name
                        maybe_clashing          = true;
                        var_symbol->source_name = necro_intern_string(intern, curr_name);
                        curr_name[0]           += 1;
                        if (curr_name[0] > 'z')
                        {
                            if (curr_name[1] == '\0')
                                curr_name[1] = '0';
                            else
                                curr_name[1] += 1;
                            if (is_uvar)
                                curr_name[0] = 'u';
                            else if (is_kvar)
                                curr_name[0] = 'k';
                            else if (is_hvar)
                                curr_name[0] = 'f';
                            else
                                curr_name[0] = 'a';
                        }
                    }
                    else if (!is_same_type && var_symbol->source_name == forall_symbol->source_name)
                    {
                        // Handle clashing name
                        maybe_clashing                 = true;
                        char itoa_buf[NECRO_ITOA_BUF_LENGTH];
                        NecroArenaSnapshot snapshot    = necro_snapshot_arena_get(&intern->snapshot_arena);
                        const size_t       str_len     = strlen(var_symbol->source_name->str);
                        const size_t       buf_size    = str_len + 32;
                        char*              unique_str  = necro_snapshot_arena_alloc(&intern->snapshot_arena, buf_size);
                        char*              itoa_result = necro_itoa((uint32_t)intern->clash_suffix, itoa_buf, NECRO_ITOA_BUF_LENGTH, 10);
                        assert(itoa_result != NULL);
                        snprintf(unique_str, buf_size, "%s%s", var_symbol->source_name->str, itoa_buf);
                        var_symbol->source_name        = necro_intern_string(intern, unique_str);
                        necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
                        clash_suffix++;
                    }
                }
                curr_forall = necro_type_find(curr_forall->for_all.type);
            }
        }
        type = necro_type_find(type->for_all.type);
    }
    if (foralls != NULL && foralls->type == NECRO_TYPE_FOR)
        foralls->for_all.is_normalized = true;
}

NecroResult(NecroType) necro_type_generalize(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroIntern* intern, NecroType* type, NecroScope* scope)
{
    UNUSED(con_env);
    assert(type != NULL);
    assert(type->type != NECRO_TYPE_FOR);
    necro_try_map(void, NecroType, necro_kind_infer_default(arena, base, type, NULL_LOC, NULL_LOC));

    // TODO: Normal and post inference constraint solving queues?
    necro_constraint_simplify(arena, con_env, base, intern);

    NecroType* for_alls   = NULL;
    NecroType* gen_type   = necro_type_gen_go(arena, base, intern, type, scope, &for_alls);
    NecroType* uniqueness = gen_type->ownership;
    assert(uniqueness != NULL);
    if (for_alls != NULL)
    {
        NecroType* curr = for_alls;
        while (curr->for_all.type != NULL)
        {
            curr->ownership = uniqueness;
            curr            = curr->for_all.type;
        }
        curr->for_all.type = gen_type;
        curr->ownership    = uniqueness;
        necro_type_normalize_type_var_names(intern, base, for_alls, for_alls);
        return ok(NecroType, for_alls);
    }
    else
    {
        necro_type_normalize_type_var_names(intern, base, gen_type, gen_type);
        return ok(NecroType, gen_type);
    }
}

//=====================================================
// Printing
//=====================================================
#define NECRO_PRINT_VERBOSE_UTYPES 0
bool necro_type_lame_hack_is_shared(const NecroType* type)
{
    static const char* share_name = "Shared";
    const NecroType* utype = necro_type_find_const(necro_type_find_const(type)->ownership);
    return utype == NULL || (utype->type == NECRO_TYPE_CON && strcmp(utype->con.con_symbol->source_name->str, share_name) == 0);
}

bool necro_type_should_print_utype(const NecroType* type)
{
    static const char* unique_name = "Unique";
    const NecroType*   utype       = necro_type_find_const(type->ownership);
    if (necro_type_lame_hack_is_shared(type))
        return false;
    switch (type->type)
    {
    case NECRO_TYPE_VAR: return true;
    case NECRO_TYPE_FUN: return utype->type == NECRO_TYPE_CON && strcmp(utype->con.con_symbol->source_name->str, unique_name) == 0;
    case NECRO_TYPE_APP:
    {
        bool should_print_utype = true;
        while (type->type == NECRO_TYPE_APP)
        {
            should_print_utype = should_print_utype && necro_type_lame_hack_is_shared(type->app.type2);
            type = necro_type_find_const(type->app.type1);
        }
        return should_print_utype;
    }
    case NECRO_TYPE_CON:
    {
        bool             should_print = true;
        const NecroType* args         = necro_type_find_const(type->con.args);
        while (args != NULL)
        {
            should_print = should_print && necro_type_lame_hack_is_shared(args->list.item);
            args         = necro_type_find_const(args->list.next);
        }
        return should_print;
    }
    case NECRO_TYPE_FOR:  return false;
    case NECRO_TYPE_NAT:  return false;
    case NECRO_TYPE_SYM:  return false;
    case NECRO_TYPE_LIST:
    default:
        assert(false);
        return false;
    }
}

void necro_type_print(const NecroType* type)
{
    necro_type_fprint(stdout, type);
}

bool necro_is_simple_type(const NecroType* type)
{
    assert(type != NULL);
    // const bool should_print_utype = !necro_type_should_print_utype(type);
    // return (type->type == NECRO_TYPE_VAR || type->type == NECRO_TYPE_NAT || type->type == NECRO_TYPE_SYM || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0)) && (type->ownership == NULL || type->ownership->type != NECRO_TYPE_VAR);
    // return (type->type == NECRO_TYPE_VAR || type->type == NECRO_TYPE_NAT || type->type == NECRO_TYPE_SYM || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0)) && should_print_utype;
    return (type->type == NECRO_TYPE_VAR || type->type == NECRO_TYPE_NAT || type->type == NECRO_TYPE_SYM || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0));
}

void necro_print_type_sig_go_maybe_with_parens(FILE* stream, const NecroType* type)
{
    if (!necro_is_simple_type(type))
        fprintf(stream, "(");
    necro_type_fprint(stream, type);
    if (!necro_is_simple_type(type))
        fprintf(stream, ")");
}

bool necro_print_tuple_sig(FILE* stream, const NecroType* type)
{
    NecroSymbol con_symbol = type->con.con_symbol->source_name;
    const char* con_string = type->con.con_symbol->source_name->str;

    const bool is_unboxed_tuple = strncmp(con_string, "(#,#)", 5) == 0;

    if (con_string[0] != '(' && con_string[0] != '[' && !is_unboxed_tuple)
        return false;

    const NecroType* current_element = type->con.args;

    // Unit
    if (strcmp(con_symbol->str, "()") == 0)
    {
        fprintf(stream, "()");
        return true;
    }

    // List
    if (strcmp(con_symbol->str, "[]") == 0)
    {
        fprintf(stream, "[");
        if (current_element != NULL && current_element->list.item != NULL)
            necro_type_fprint(stream, current_element->list.item);
        fprintf(stream, "]");
        return true;
    }

    if (type->con.args == NULL)
        return false;

    if (is_unboxed_tuple)
        fprintf(stream, "(#");
    else
        fprintf(stream, "(");
    while (current_element != NULL)
    {
        necro_type_fprint(stream, current_element->list.item);
        if (current_element->list.next != NULL)
            fprintf(stream, ", ");
        current_element = current_element->list.next;
    }
    if (is_unboxed_tuple)
        fprintf(stream, "#)");
    else
        fprintf(stream, ")");
    return true;
}

void necro_type_fprint_type_var(FILE* stream, const NecroAstSymbol* var_symbol)
{
    // NecroType* type = var_symbol->type;
    if (var_symbol->source_name != NULL && var_symbol->source_name->str != NULL)
    {
        fprintf(stream, "%s", var_symbol->source_name->str);
    }
    else
    {
        size_t truncated_symbol_pointer = (size_t)var_symbol;
        truncated_symbol_pointer        = truncated_symbol_pointer & 8191;
        const NecroType* type_var       = necro_type_find(var_symbol->type);
        if (type_var != NULL && type_var->kind != NULL && type_var->kind->type == NECRO_TYPE_CON && (strcmp(type_var->kind->con.con_symbol->source_name->str, "Uniqueness") == 0))
            fprintf(stream, "u%zu", truncated_symbol_pointer);
        else
            fprintf(stream, "t%zu", truncated_symbol_pointer);
    }
}

void necro_type_fprint_ownership(FILE* stream, const NecroType* type)
{
    if (!necro_type_should_print_utype(type))
        return;
    const NecroType* ownership = necro_type_find_const(type->ownership);
    if (ownership->type == NECRO_TYPE_VAR)
    {
        // necro_type_fprint_type_var(stream, ownership->var.var_symbol);
        // fprintf(stream, ":");
        fprintf(stream, ".");
    }
    else if (ownership->type == NECRO_TYPE_CON)
    {
        bool is_shared = strcmp(ownership->con.con_symbol->source_name->str, "Shared") == 0;
        if (!is_shared)
            fprintf(stream, "*");
    }
    else
    {
        assert(false);
    }
}

void necro_constraint_fprint(FILE* stream, const NecroConstraint* constraint)
{
    switch (constraint->type)
    {
    // case NECRO_CONSTRAINT_AND: return;
    // case NECRO_CONSTRAINT_FOR: return;
    case NECRO_CONSTRAINT_UCONSTRAINT:
        // necro_type_fprint_type_var(stream, constraint->uconstraint.u1->var.var_symbol);
        // fprintf(stream, "<=");
        // necro_type_fprint_type_var(stream, constraint->uconstraint.u2->var.var_symbol);
        return;
    case NECRO_CONSTRAINT_CLASS:
        fprintf(stream, "%s", constraint->cls.type_class->type_class_name->source_name->str);
        fprintf(stream, " ");
        necro_print_type_sig_go_maybe_with_parens(stream, constraint->cls.type1);
        return;
    case NECRO_CONSTRAINT_EQUAL: return;
    default:
        assert(false);
        return;
    }
}

void necro_type_fprint(FILE* stream, const NecroType* type)
{
    if (type == NULL)
        return;
    type = necro_type_find_const(type);
    // if (type->ownership != NULL && type->type != NECRO_TYPE_FUN)
    if (type->ownership != NULL)
        necro_type_fprint_ownership(stream, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        necro_type_fprint_type_var(stream, type->var.var_symbol);
        break;

    case NECRO_TYPE_APP:
        necro_print_type_sig_go_maybe_with_parens(stream, type->app.type1);
        fprintf(stream, " ");
        necro_print_type_sig_go_maybe_with_parens(stream, type->app.type2);
        break;

    case NECRO_TYPE_FUN:
    {
        const bool should_print_utype = necro_type_should_print_utype(type);
        // const bool is_not_shared = type->ownership != NULL && (type->ownership->type == NECRO_TYPE_VAR || (type->ownership->type == NECRO_TYPE_CON && (strcmp(type->ownership->con.con_symbol->source_name->str, "Shared") != 0)));
        if (should_print_utype)
            fprintf(stream, "(");
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            fprintf(stream, "(");
        necro_type_fprint(stream, type->fun.type1);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            fprintf(stream, ")");
        fprintf(stream, " -> ");
        necro_type_fprint(stream, type->fun.type2);
        if (should_print_utype)
            fprintf(stream, ")");
        break;
    }

    case NECRO_TYPE_CON:
    {
        if (necro_print_tuple_sig(stream, type))
            break;
        bool has_args = necro_type_list_count(type->con.args) > 0;
        fprintf(stream, "%s", type->con.con_symbol->source_name->str);
        if (has_args)
        {
            fprintf(stream, " ");
            necro_type_fprint(stream, type->con.args);
        }
        break;
    }

    case NECRO_TYPE_LIST:
        necro_print_type_sig_go_maybe_with_parens(stream, type->list.item);
        if (type->list.next != NULL)
        {
            fprintf(stream, " ");
            necro_type_fprint(stream, type->list.next);
        }
        break;

    case NECRO_TYPE_FOR:
        {
            // fprintf(stream, "forall ");

            // // Print quantified type vars
            // static const char* ukind = "Uniqueness";
            const NecroType* for_all_head = type;
            // while (type->for_all.type->type == NECRO_TYPE_FOR)
            // {
            //     if (!(necro_type_find_const(necro_type_find_const(type->for_all.var_symbol->type)->kind)->type == NECRO_TYPE_CON && strcmp(type->for_all.var_symbol->type->kind->con.con_symbol->source_name->str, ukind) == 0))
            //     {
            //         necro_type_fprint_type_var(stream, type->for_all.var_symbol);
            //         fprintf(stream, " ");
            //     }
            //     type = type->for_all.type;
            // }
            // if (!(necro_type_find_const(necro_type_find_const(type->for_all.var_symbol->type)->kind)->type == NECRO_TYPE_CON && strcmp(type->for_all.var_symbol->type->kind->con.con_symbol->source_name->str, ukind) == 0))
            //     necro_type_fprint_type_var(stream, type->for_all.var_symbol);
            // fprintf(stream, ". ");
            // type = type->for_all.type;

            // Print context
            // type             = for_all_head;
            bool has_context = false;
            while (type->type == NECRO_TYPE_FOR)
            {
                NecroConstraintList* constraints = type->for_all.var_symbol->type->var.var_symbol->constraints;
                while(constraints != NULL)
                {
                    if (constraints->data->type == NECRO_CONSTRAINT_CLASS)
                    {
                        has_context = true;
                        break;
                    }
                    constraints = constraints->next;
                }
                type = type->for_all.type;
            }
            if (has_context)
            {
                fprintf(stream, "(");
                size_t count = 0;
                type = for_all_head;
                while (type->type == NECRO_TYPE_FOR)
                {
                    NecroConstraintList* constraints = type->for_all.var_symbol->type->var.var_symbol->constraints;
                    while (constraints != NULL)
                    {
                        if (constraints->data->type == NECRO_CONSTRAINT_CLASS)
                        {
                            if (count > 0)
                                fprintf(stream, ", ");
                            necro_constraint_fprint(stream, constraints->data);
                            count++;
                        }
                        constraints = constraints->next;
                    }
                    type = type->for_all.type;
                }
                fprintf(stream, ") => ");
            }

            // Print rest of type
            necro_type_fprint(stream, type);
        }
        break;

    case NECRO_TYPE_NAT:
        fprintf(stream, "%zu", type->nat.value);
        break;

    case NECRO_TYPE_SYM:
        fprintf(stream, "\"%s\"", type->sym.value->str);
        break;

    default:
        assert(false);
        break;
    }
}

size_t necro_type_mangled_string_length(const NecroType* type)
{
    if (type == NULL)
        return 0;
    type = necro_type_find_const(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        assert(false &&  "Mangled types must be fully concrete");
        return 0;

    case NECRO_TYPE_APP:
    {
        assert(false &&  "Mangled types must be fully concrete");
        return 0;
    }

    case NECRO_TYPE_FUN:
        return necro_type_mangled_string_length(type->fun.type1) + necro_type_mangled_string_length(type->fun.type2) + 4;

    case NECRO_TYPE_CON:
        return strlen(type->con.con_symbol->source_name->str) + necro_type_mangled_string_length(type->con.args);

    case NECRO_TYPE_LIST:
    {
        size_t length = 2;
        while (type != NULL)
        {
            length += necro_type_mangled_string_length(type->list.item);
            if (type->list.next != NULL)
                length += 1;
            type = type->list.next;
        }
        return length;
    }

    case NECRO_TYPE_FOR:
        assert(false && "Mangled types must be fully concrete");
        return 0;

    case NECRO_TYPE_NAT:
    {
        char buffer[NECRO_ITOA_BUF_LENGTH];
        assert(type->nat.value < INT32_MAX);
        char* itoa_result = necro_itoa((int) type->nat.value, buffer, NECRO_ITOA_BUF_LENGTH, 10);
        assert(itoa_result != NULL);
        return strlen(buffer);
    }

    case NECRO_TYPE_SYM:
        return strlen(type->sym.value->str);

    default:
        assert(false);
        return 0;
    }
}

size_t necro_type_mangled_sprintf(char* buffer, size_t offset, const NecroType* type)
{
    if (type == NULL)
        return offset;
    type = necro_type_find_const(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        assert(false &&  "Mangled types must be fully concrete");
        return offset;

    case NECRO_TYPE_APP:
        assert(false &&  "Mangled types must be fully concrete");
        return offset;

    case NECRO_TYPE_FUN:
        offset += sprintf(buffer + offset, "(");
        offset  = necro_type_mangled_sprintf(buffer, offset, type->fun.type1);
        offset += sprintf(buffer + offset, "->");
        offset  = necro_type_mangled_sprintf(buffer, offset, type->fun.type2);
        offset += sprintf(buffer + offset, ")");
        return offset;

    case NECRO_TYPE_CON:
        offset += sprintf(buffer + offset, "%s", type->con.con_symbol->source_name->str);
        return necro_type_mangled_sprintf(buffer, offset, type->con.args);

    case NECRO_TYPE_LIST:
    {
        offset += sprintf(buffer + offset, "<");
        while (type != NULL)
        {
            offset = necro_type_mangled_sprintf(buffer, offset, type->list.item);
            if (type->list.next != NULL)
                offset += sprintf(buffer + offset, ",");
            type = type->list.next;
        }
        offset += sprintf(buffer + offset, ">");
        return offset;
    }

    case NECRO_TYPE_FOR:
        assert(false && "Mangled types must be fully concrete");
        return offset;

    case NECRO_TYPE_NAT:
        offset += sprintf(buffer + offset, "%zu", type->nat.value);
        return offset;

    case NECRO_TYPE_SYM:
        offset += sprintf(buffer + offset, "%s", type->sym.value->str);
        return offset;

    default:
        assert(false);
        return offset;
    }
}


//=====================================================
// Prim Types
//=====================================================
NecroType* necro_type_tuple_con_create(NecroPagedArena* arena, NecroBase* base, NecroType* types_list)
{
    size_t     tuple_count  = 0;
    NecroType* current_type = types_list;
    while (current_type != NULL)
    {
        assert(current_type->type == NECRO_TYPE_LIST);
        tuple_count++;
        current_type = current_type->list.next;
    }
    NecroAstSymbol* con_symbol = NULL;
    switch (tuple_count)
    {
    case 2:  con_symbol = base->tuple2_type;  break;
    case 3:  con_symbol = base->tuple3_type;  break;
    case 4:  con_symbol = base->tuple4_type;  break;
    case 5:  con_symbol = base->tuple5_type;  break;
    case 6:  con_symbol = base->tuple6_type;  break;
    case 7:  con_symbol = base->tuple7_type;  break;
    case 8:  con_symbol = base->tuple8_type;  break;
    case 9:  con_symbol = base->tuple9_type;  break;
    case 10: con_symbol = base->tuple10_type; break;
    default:
        assert(false && "Tuple size too large");
        break;
    }
    return necro_type_con_create(arena, con_symbol, types_list);
}

NecroType* necro_type_unboxed_tuple_con_create(NecroPagedArena* arena, NecroBase* base, NecroType* types_list)
{
    size_t     tuple_count  = 0;
    NecroType* current_type = types_list;
    while (current_type != NULL)
    {
        assert(current_type->type == NECRO_TYPE_LIST);
        tuple_count++;
        current_type = current_type->list.next;
    }
    NecroAstSymbol* con_symbol = necro_base_get_unboxed_tuple_type(base, tuple_count);
    return necro_type_con_create(arena, con_symbol, types_list);
}

NecroType* necro_type_con1_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1)
{
    NecroType* lst1 = necro_type_list_create(arena, arg1, NULL);
    return necro_type_con_create(arena, con, lst1 );
}

NecroType* necro_type_con2_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2)
{
    NecroType* lst2 = necro_type_list_create(arena, arg2, NULL);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con3_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3)
{
    NecroType* lst3 = necro_type_list_create(arena, arg3, NULL);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con4_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4)
{
    NecroType* lst4 = necro_type_list_create(arena, arg4, NULL);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con5_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5)
{
    NecroType* lst5 = necro_type_list_create(arena, arg5, NULL);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con6_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6)
{
    NecroType* lst6 = necro_type_list_create(arena, arg6, NULL);
    NecroType* lst5 = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con7_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7)
{
    NecroType* lst7 = necro_type_list_create(arena, arg7, NULL);
    NecroType* lst6 = necro_type_list_create(arena, arg6, lst7);
    NecroType* lst5 = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con8_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8)
{
    NecroType* lst8 = necro_type_list_create(arena, arg8, NULL);
    NecroType* lst7 = necro_type_list_create(arena, arg7, lst8);
    NecroType* lst6 = necro_type_list_create(arena, arg6, lst7);
    NecroType* lst5 = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con9_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9)
{
    NecroType* lst9 = necro_type_list_create(arena, arg9, NULL);
    NecroType* lst8 = necro_type_list_create(arena, arg8, lst9);
    NecroType* lst7 = necro_type_list_create(arena, arg7, lst8);
    NecroType* lst6 = necro_type_list_create(arena, arg6, lst7);
    NecroType* lst5 = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

NecroType* necro_type_con10_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10)
{
    NecroType* lst10 = necro_type_list_create(arena, arg10, NULL);
    NecroType* lst9  = necro_type_list_create(arena, arg9, lst10);
    NecroType* lst8  = necro_type_list_create(arena, arg8, lst9);
    NecroType* lst7  = necro_type_list_create(arena, arg7, lst8);
    NecroType* lst6  = necro_type_list_create(arena, arg6, lst7);
    NecroType* lst5  = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4  = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3  = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2  = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1  = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1);
}

size_t necro_type_arity(NecroType* type)
{
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return 0;
    case NECRO_TYPE_APP:  return 0;
    case NECRO_TYPE_CON:  return 0;
    case NECRO_TYPE_FUN:  return 1 + necro_type_arity(type->fun.type2);
    case NECRO_TYPE_FOR:  return necro_type_arity(type->for_all.type);
    case NECRO_TYPE_LIST: assert(false); return 0;
    case NECRO_TYPE_NAT:  assert(false); return 0;
    case NECRO_TYPE_SYM:  assert(false); return 0;
    default:              assert(false); return 0;
    }
}

void necro_type_assert_no_rigid_variables(const NecroType* type)
{
    if (type == NULL)
        return;
    type = necro_type_find_const(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        assert(!type->var.is_rigid);
        return;
    case NECRO_TYPE_APP:
        necro_type_assert_no_rigid_variables(type->app.type1);
        necro_type_assert_no_rigid_variables(type->app.type2);
        return;
    case NECRO_TYPE_FUN:
        necro_type_assert_no_rigid_variables(type->fun.type1) ;
        necro_type_assert_no_rigid_variables(type->fun.type2);
        return;
    case NECRO_TYPE_CON:
    {
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            necro_type_assert_no_rigid_variables(args->list.item);
            args = args->list.next;
        }
        return;
    }
    case NECRO_TYPE_FOR:
    {
        assert(false);
        return;
    }
    case NECRO_TYPE_NAT:
        return;
    case NECRO_TYPE_SYM:
        return;
    case NECRO_TYPE_LIST:
        assert(false && "Only used in TYPE_CON case");
        return;
    default:
        assert(false && "Unrecognized type in necro_type_assert_no_rigid_variables");
        return;
    }
}

size_t necro_constraint_list_hash(NecroConstraintList* constraints)
{
    size_t h = 0;
    while (constraints != NULL)
    {
        switch (constraints->data->type)
        {
        // case NECRO_CONSTRAINT_AND: assert(false); break;
        // case NECRO_CONSTRAINT_FOR: assert(false); break;
        case NECRO_CONSTRAINT_UCONSTRAINT:
            h = 7789 ^ necro_type_hash(constraints->data->uconstraint.u1) ^ necro_type_hash(constraints->data->uconstraint.u2);
            break;
        case NECRO_CONSTRAINT_CLASS:
            h = 7759 ^ constraints->data->cls.type_class->type_class_name->name->hash ^ necro_type_hash(constraints->data->cls.type1);
            break;
        case NECRO_CONSTRAINT_EQUAL:
            h = 7207 ^ necro_type_hash(constraints->data->equal.type1) ^ necro_type_hash(constraints->data->equal.type1);
            break;
        default:
            assert(false);
            break;
        }
        constraints = constraints->next;
    }
    return h;
}

size_t necro_type_hash(NecroType* type)
{
    if (type == NULL)
        return 0;
    type = necro_type_find(type);
    if (type->hash != 0)
        return type->hash;
    size_t h = 0;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        h = (size_t) type->var.var_symbol ^ necro_constraint_list_hash(type->var.var_symbol->constraints);
        break;
    case NECRO_TYPE_APP:
        h = 1361 ^ necro_type_hash(type->app.type1) ^ necro_type_hash(type->app.type2);
        break;
    case NECRO_TYPE_FUN:
        h = 8191 ^ necro_type_hash(type->fun.type1) ^ necro_type_hash(type->fun.type2);
        break;
    case NECRO_TYPE_CON:
    {
        h = 52103 ^ (size_t) type->con.con_symbol;
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            h = h ^ necro_type_hash(args->list.item);
            args = args->list.next;
        }
        break;
    }
    case NECRO_TYPE_FOR:
    {
        h = 4111  ^ necro_type_hash(type->for_all.var_symbol->type) ^ necro_type_hash(type->for_all.type);
        break;
    }
    case NECRO_TYPE_NAT:
        h = type->nat.value ^ 37;
        break;
    case NECRO_TYPE_SYM:
        h = type->sym.value->hash;
        break;
    case NECRO_TYPE_LIST:
        assert(false && "Only used in TYPE_CON case");
        break;
    default:
        assert(false && "Unrecognized tree type in necro_type_hash");
        break;
    }
    type->hash = h;
    return h;
}

bool necro_type_exact_unify(NecroType* type1, NecroType* type2)
{
    type1 = necro_type_find(type1);
    type2 = necro_type_find(type2);
    if (type1 == type2)
        return true;
    if (type1 == NULL || type2 == NULL || (type1->type != type2->type))
        return false;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR: return type1->var.var_symbol == type2->var.var_symbol;
    case NECRO_TYPE_APP: return necro_type_exact_unify(type1->app.type1, type2->app.type1) && necro_type_exact_unify(type1->app.type2, type2->app.type2);
    case NECRO_TYPE_FUN: return necro_type_exact_unify(type1->fun.type1, type2->fun.type1) && necro_type_exact_unify(type1->fun.type2, type2->fun.type2);
    case NECRO_TYPE_CON: return type1->con.con_symbol == type2->con.con_symbol && necro_type_exact_unify(type1->con.args, type2->con.args);
    case NECRO_TYPE_LIST:
    {
        while (type1 != NULL || type2 != NULL)
        {
            if (type1 == NULL || type2 == NULL)
                return false;
            if (!necro_type_exact_unify(type1->list.item, type2->list.item))
                return false;
            type1 = type1->list.next;
            type2 = type2->list.next;
        }
        return true;
    }
    case NECRO_TYPE_FOR:
    {
        if (type1->for_all.var_symbol != type2->for_all.var_symbol)
            return false;
        NecroConstraintList* constraints1 = type1->for_all.var_symbol->type->var.var_symbol->constraints;
        NecroConstraintList* constraints2 = type2->for_all.var_symbol->type->var.var_symbol->constraints;
        while (constraints1 != NULL || constraints2 != NULL)
        {
            if (constraints1 == NULL || constraints2 == NULL)
                return false;
            if (!necro_constraint_is_equal(constraints1->data, constraints2->data))
                return false;
            constraints1 = constraints1->next;
            constraints2 = constraints2->next;
        }
        return necro_type_exact_unify(type1->for_all.type, type2->for_all.type);
    }
    case NECRO_TYPE_NAT:
        return type1->nat.value == type2->nat.value;
    case NECRO_TYPE_SYM:
        return type1->sym.value == type2->sym.value;
    default:
        assert(false);
        return false;
    }
}

bool necro_type_is_higher_order_function_go(const NecroType* type)
{
    type = necro_type_find_const(type);
    switch (type->type)
    {
    case NECRO_TYPE_FUN: return true;
    case NECRO_TYPE_VAR: return false;
    case NECRO_TYPE_FOR: return necro_type_is_higher_order_function_go(type->for_all.type);
    case NECRO_TYPE_APP: return necro_type_is_higher_order_function_go(type->app.type1) || necro_type_is_higher_order_function_go(type->app.type2);
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type->con.args;
        while (args != NULL)
        {
            if (necro_type_is_higher_order_function_go(args->list.item))
                return true;
            args = args->list.next;
        }
        return false;
    }
    case NECRO_TYPE_NAT: return false;
    case NECRO_TYPE_SYM: return false;
    case NECRO_TYPE_LIST:
    default:
        assert(false);
        return false;
    }
}

bool necro_type_is_higher_order_function(const NecroType* type, size_t arity)
{
    while (type->type == NECRO_TYPE_FUN && arity != SIZE_MAX)
    {
        if (necro_type_is_higher_order_function_go(type->fun.type1))
            return true;
        arity--;
        type = type->fun.type2;
    }
    // return necro_type_is_higher_order_function_go(type);
    return false;
}

///////////////////////////////////////////////////////
// Ownership
///////////////////////////////////////////////////////
bool necro_type_is_ownership_share(const struct NecroBase* base, const NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find_const(type);
    return type->type == NECRO_TYPE_CON && type->con.con_symbol == base->ownership_share;
}

bool necro_type_is_ownership_owned(const struct NecroBase* base, const NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find_const(type);
    return type->type == NECRO_TYPE_CON && type->con.con_symbol == base->ownership_steal;
}

NecroType* necro_type_ownership_fresh_var(NecroPagedArena* arena, NecroBase* base, NecroScope* scope)
{
    NecroType* type = necro_type_fresh_var(arena, scope);
    type->kind      = base->ownership_kind->type;
    return type;
}

bool necro_type_is_inhabited(NecroBase* base, const NecroType* type)
{
    type = necro_type_find_const(type);
    // const NecroType* kind = necro_type_find_const(type->kind);
    // assert(kind != NULL);
    // assert(kind->type != NECRO_TYPE_VAR);
    // return necro_type_get_fully_applied_fun_type_const(kind) == base->star_kind->type;
    return necro_kind_is_type(base, type);
}

// Returns NULL
NecroResult(NecroType) necro_type_ownership_unify_with_info(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* ownership1, NecroType* ownership2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    UNUSED(arena);
    UNUSED(base);
    NecroResult(NecroType) result = necro_type_ownership_unify(arena, con_env, ownership1, ownership2, scope);
    if (result.type == NECRO_RESULT_OK)
    {
        return result;
    }
    assert(result.error != NULL);
    switch (result.error->type)
    {
    case NECRO_TYPE_RIGID_TYPE_VARIABLE:
    case NECRO_TYPE_OCCURS:
    case NECRO_TYPE_MISMATCHED_TYPE:
        result.error->default_type_error_data2.macro_type1 = ownership1;
        result.error->default_type_error_data2.macro_type2 = ownership2;
        result.error->default_type_error_data2.source_loc  = source_loc;
        result.error->default_type_error_data2.end_loc     = end_loc;
        break;

    case NECRO_KIND_MISMATCHED_KIND:
        result.error->default_type_error_data2.macro_type1 = ownership1->kind;
        result.error->default_type_error_data2.macro_type2 = ownership2->kind;
        result.error->default_type_error_data2.source_loc  = source_loc;
        result.error->default_type_error_data2.end_loc     = end_loc;
        break;

    default:
        assert(false);
        break;
    }
    return result;
}

void necro_type_ownership_bind_uvar_to(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroType* uvar_to_bind, NecroType* utype_to_bind_to)
{
    UNUSED(arena);
    UNUSED(con_env);
    assert(uvar_to_bind->type == NECRO_TYPE_VAR);
    uvar_to_bind->var.bound = utype_to_bind_to;
    return;
}


NecroResult(void) necro_type_ownership_bind_uvar(NecroType* uvar_to_bind, NecroType* utype_to_bind_to, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    uvar_to_bind     = necro_type_find(uvar_to_bind);
    utype_to_bind_to = necro_type_find(utype_to_bind_to);
    assert(uvar_to_bind->type == NECRO_TYPE_VAR);
    if (uvar_to_bind->var.is_rigid)
    {
        return necro_error_map(NecroType, void, necro_type_rigid_type_variable_error(uvar_to_bind, utype_to_bind_to, uvar_to_bind, utype_to_bind_to, source_loc, end_loc));
    }
    uvar_to_bind->var.bound = utype_to_bind_to;
    return ok_void();
}

// Returns NULL
NecroResult(NecroType) necro_type_ownership_unify(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroType* ownership1, NecroType* ownership2, NecroScope* scope)
{
    ownership1 = necro_type_find(ownership1);
    ownership2 = necro_type_find(ownership2);
    if (ownership1 == ownership2)
        return ok(NecroType, NULL);
    switch (ownership1->type)
    {
    case NECRO_TYPE_VAR:
    {
        switch (ownership2->type)
        {
        case NECRO_TYPE_VAR:
            if (ownership1->var.var_symbol == ownership2->var.var_symbol)  return ok(NecroType, NULL);
            else if (ownership1->var.is_rigid && ownership2->var.is_rigid) return necro_type_rigid_type_variable_error(ownership1, ownership2, NULL, NULL, NULL_LOC, NULL_LOC); // TODO: Custom Ownership error messages!
            else if (ownership1->var.is_rigid)                             { necro_type_ownership_bind_uvar_to(arena, con_env, ownership2, ownership1); return ok(NecroType, NULL); }
            else if (ownership2->var.is_rigid)                             { necro_type_ownership_bind_uvar_to(arena, con_env, ownership1, ownership2); return ok(NecroType, NULL); }
            const bool is_type1_bound_in_scope = necro_type_is_bound_in_scope(ownership1, scope);
            const bool is_type2_bound_in_scope = necro_type_is_bound_in_scope(ownership2, scope);
            if (is_type1_bound_in_scope && is_type2_bound_in_scope)
            {
                const size_t type1_distance = necro_type_bound_in_scope_distance(ownership1, scope);
                const size_t type2_distance = necro_type_bound_in_scope_distance(ownership2, scope);
                if (type1_distance >= type2_distance)
                    necro_type_ownership_bind_uvar_to(arena, con_env, ownership2, ownership1);
                else
                    necro_type_ownership_bind_uvar_to(arena, con_env, ownership1, ownership2);
            }
            else if (is_type1_bound_in_scope) necro_type_ownership_bind_uvar_to(arena, con_env, ownership2, ownership1);
            else                              necro_type_ownership_bind_uvar_to(arena, con_env, ownership1, ownership2);
            return ok(NecroType, NULL);
        case NECRO_TYPE_CON:
            if (ownership1->var.is_rigid)
                return necro_type_rigid_type_variable_error(ownership1, ownership2, NULL, NULL, NULL_LOC, NULL_LOC); // TODO: Custom Ownership error messages!
            ownership1->var.bound = ownership2;
            return ok(NecroType, NULL);
        default:
            necro_unreachable(NecroType);
        }
    }
    case NECRO_TYPE_CON:
    {
        assert(ownership1->con.args == NULL);
        switch (ownership2->type)
        {
        case NECRO_TYPE_VAR:
            if (ownership2->var.is_rigid)
                return necro_type_rigid_type_variable_error(ownership1, ownership2, NULL, NULL, NULL_LOC, NULL_LOC); // TODO: Custom Ownership error messages!
            necro_type_ownership_bind_uvar_to(arena, con_env, ownership2, ownership1);
            return ok(NecroType, NULL);
        case NECRO_TYPE_CON:
            assert(ownership2->con.args == NULL);
            if (ownership1->con.con_symbol != ownership2->con.con_symbol)
                return necro_type_mismatched_type_error(ownership1, ownership2, NULL, NULL, NULL_LOC, NULL_LOC);
            return ok(NecroType, NULL);
        default:
            necro_unreachable(NecroType);
        }
    }
    default: necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_type_infer_and_unify_ownership_for_two_types(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    if (con_env == NULL)
    {
        // HACK / NOTE / TODO: No con_env, likely inferring types during necro_core_infer. We should probably check ownership types to make sure that we don't break things, however for now we're sweeping things under the rug...
        return ok(NecroType, base->ownership_share->type);
    }
    if (necro_kind_is_ownership(base, type1))
    {
        type1->ownership = NULL;
        type2->ownership = NULL;
        return ok(NecroType, NULL);
    }
    else if (!necro_type_is_inhabited(base, type1))
    {
        type1->ownership = base->ownership_share->type;
        type2->ownership = base->ownership_share->type;
        return ok(NecroType, NULL);
    }
    else
    {
        NecroType* ownership1 = necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, NULL, type1, scope, NULL, false, con_env->source_loc, con_env->end_loc, NECRO_CONSTRAINT_UCONSTRAINT));
        NecroType* ownership2 = necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, NULL, type2, scope, NULL, false, con_env->source_loc, con_env->end_loc, NECRO_CONSTRAINT_UCONSTRAINT));
        return necro_type_ownership_unify(arena, con_env, ownership1, ownership2, scope);
    }
}

// EQ constraint?
NecroResult(void) necro_constraint_push_back_uniqueness_constraint_or_coercion(NecroPagedArena* arena, NecroConstraintEnv* env, struct NecroBase* base, struct NecroIntern* intern, NecroType* type1, NecroType* type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NECRO_CONSTRAINT_TYPE constraint_type)
{
    if (constraint_type == NECRO_CONSTRAINT_UCONSTRAINT)
    {
        necro_constraint_push_back_uniqueness_constraint(arena, env, base, intern, type1->ownership, type2->ownership, source_loc, end_loc);
        return ok_void();
    }
    else
    {
        necro_constraint_push_back_uniqueness_coercion(arena, env, base, intern, type1, type2, source_loc, end_loc);
        return ok_void();
    }
}

NecroResult(NecroType) necro_uniqueness_propagate(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroIntern* intern, NecroType* type, NecroScope* scope, NecroFreeVarList* free_vars, bool force_propagation, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NECRO_CONSTRAINT_TYPE uniqueness_coercion_type)
{
    type = necro_type_find(type);
    if (necro_kind_is_ownership(base, type))
    {
        type->ownership = NULL;
        return ok(NecroType, type->ownership);
    }
    else if (!necro_type_is_inhabited(base, type))
    {
        type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    }

    // Uncurry TypeApps
    if (type->type == NECRO_TYPE_APP && necro_type_is_type_app_type_con(type))
    {
        *type = *necro_type_uncurry_app(arena, base, type);
        // force_propagation = true;
    }

    if (type->has_propagated && !force_propagation)
    {
        return ok(NecroType, type->ownership);
    }

    type->has_propagated = true;
    type->ownership      = necro_type_find(type->ownership);
    switch (type->type)
    {

    case NECRO_TYPE_VAR:
        if (type->ownership == NULL)
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        while (type->var.var_symbol->sig_type_var_ulist != NULL)
        {
            necro_try(NecroType, necro_type_unify_with_full_info(arena, con_env, base, type->ownership, type->var.var_symbol->sig_type_var_ulist->list.item, scope, source_loc, end_loc, type->ownership, type->var.var_symbol->sig_type_var_ulist->list.item));
            type->var.var_symbol->sig_type_var_ulist = type->var.var_symbol->sig_type_var_ulist->list.next;
        }
        return ok(NecroType, type->ownership);

    case NECRO_TYPE_FUN:
    {
        if (type->ownership == NULL)
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        bool              all_share     = true;
        NecroFreeVarList* curr_free_var = free_vars;
        while (curr_free_var != NULL)
        {
            NecroType* free_var_type  = necro_type_find(curr_free_var->data.type);
            NecroType* free_var_utype = necro_type_find(curr_free_var->data.ownership_type);
            if (free_var_utype->type == NECRO_TYPE_VAR || necro_type_is_ownership_owned(base, free_var_utype))
            {
                all_share = false;
            }
            unwrap(void, necro_constraint_push_back_uniqueness_constraint_or_coercion(arena, con_env, base, intern, type, free_var_type, curr_free_var->data.source_loc, curr_free_var->data.end_loc, uniqueness_coercion_type));
            curr_free_var = curr_free_var->next;
        }
        NecroType* domain_utype = necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, intern, type->fun.type1, scope, NULL, force_propagation, source_loc, end_loc, uniqueness_coercion_type));
        // Set free vars to arg + arrow uvars
        free_vars               = necro_cons_free_var_list(arena, (NecroFreeVar){ .symbol = NULL,.type = type->fun.type1,.ownership_type = domain_utype,.source_loc = source_loc,.end_loc = end_loc }, NULL);
        free_vars               = necro_cons_free_var_list(arena, (NecroFreeVar){ .symbol = NULL, .type = type, .ownership_type = type->ownership, .source_loc = source_loc, .end_loc = end_loc }, free_vars);
        necro_try(NecroType, necro_uniqueness_propagate(arena, con_env, base, intern, type->fun.type2, scope, free_vars, force_propagation, source_loc, end_loc, uniqueness_coercion_type));
        return ok(NecroType, type->ownership);
    }

    case NECRO_TYPE_CON:
    {
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        else if (type->ownership == NULL)
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, intern, args->list.item, scope, NULL, force_propagation, source_loc, end_loc, uniqueness_coercion_type));
            if (necro_type_is_inhabited(base, args->list.item))
            {
                unwrap(void, necro_constraint_push_back_uniqueness_constraint_or_coercion(arena, con_env, base, intern, type, args->list.item, source_loc, end_loc, uniqueness_coercion_type));
            }
            args = args->list.next;
        }
        return ok(NecroType, type->ownership);
    }

    case NECRO_TYPE_APP:
    {
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        else if (type->ownership == NULL)
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        NecroType* app = type;
        while (app->type == NECRO_TYPE_APP)
            app = necro_type_find(app->app.type1);
        necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, intern, app, scope, NULL, force_propagation, source_loc, end_loc, uniqueness_coercion_type));
        app = type;
        while (app->type == NECRO_TYPE_APP)
        {
            necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, intern, app->app.type2, scope, NULL, force_propagation, source_loc, end_loc, uniqueness_coercion_type));
            if (necro_type_is_inhabited(base, app->app.type2))
            {
                unwrap(void, necro_constraint_push_back_uniqueness_constraint_or_coercion(arena, con_env, base, intern, type, app->app.type2, source_loc, end_loc, uniqueness_coercion_type));
            }
            app = necro_type_find(app->app.type1);
        }
        return ok(NecroType, type->ownership);
    }

    case NECRO_TYPE_FOR:
        type->ownership = necro_try_result(NecroType, necro_uniqueness_propagate(arena, con_env, base, intern, type->for_all.type, scope, free_vars, force_propagation, source_loc, end_loc, uniqueness_coercion_type));
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

/*
    Data constructors follow the clean attribute propagation methodology (mostly...), with the following exceptions:
        1. A type argument's position is considered propagating if has kind Type
        2. The one exception to this is for the arrow type which does not propagate the uniqueness of its type args.
    Note that this slightly less expressive than Clean's propagation rules since some type arguments will propagate uniqueness even if they are only used for embedded function types.
    However this is considered a reasonable trade off as this change greatly simplifies both the implmentation and the understanding of the type system.
*/
NecroResult(NecroType) necro_uniqueness_propagate_data_con(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroIntern* intern, NecroType* type, NecroScope* scope, NecroFreeVarList* free_vars, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAstSymbol* data_type_symbol, NecroType* data_type_uvar)
{
    type = necro_type_find(type);
    if (necro_kind_is_ownership(base, type))
    {
        type->ownership = NULL;
        return ok(NecroType, type->ownership);
    }
    type->ownership = necro_type_find(type->ownership);
    switch (type->type)
    {

    case NECRO_TYPE_VAR:
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        else if (type->ownership == NULL)
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        while (type->var.var_symbol->sig_type_var_ulist != NULL)
        {
            necro_try(NecroType, necro_type_unify_with_full_info(arena, con_env, base, type->ownership, type->var.var_symbol->sig_type_var_ulist->list.item, scope, source_loc, end_loc, type->ownership, type->var.var_symbol->sig_type_var_ulist->list.item));
            type->var.var_symbol->sig_type_var_ulist = type->var.var_symbol->sig_type_var_ulist->list.next;
        }
        type->ownership = necro_type_find(type->ownership);
        return ok(NecroType, type->ownership);

    case NECRO_TYPE_FUN:
    {
        if (type->ownership == NULL)
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        bool              all_share     = true;
        NecroFreeVarList* curr_free_var = free_vars;
        while (curr_free_var != NULL)
        {
            NecroType* free_var_type  = necro_type_find(curr_free_var->data.type);
            NecroType* free_var_utype = necro_type_find(curr_free_var->data.ownership_type);
            if (free_var_utype->type == NECRO_TYPE_VAR || necro_type_is_ownership_owned(base, free_var_utype))
            {
                all_share = false;
            }
            necro_constraint_push_back_uniqueness_coercion(arena, con_env, base, intern, type, free_var_type, curr_free_var->data.source_loc, curr_free_var->data.end_loc);
            curr_free_var = curr_free_var->next;
        }
        NecroType* domain_utype = necro_try_result(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, type->fun.type1, scope, NULL, source_loc, end_loc, data_type_symbol, data_type_uvar));
        // Set free vars to arg + arrow uvars
        free_vars               = necro_cons_free_var_list(arena, (NecroFreeVar){ .symbol = NULL,.type = type->fun.type1,.ownership_type = domain_utype,.source_loc = source_loc,.end_loc = end_loc }, NULL);
        free_vars               = necro_cons_free_var_list(arena, (NecroFreeVar){ .symbol = NULL, .type = type, .ownership_type = type->ownership, .source_loc = source_loc, .end_loc = end_loc }, free_vars);
        necro_try(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, type->fun.type2, scope, free_vars, source_loc, end_loc, data_type_symbol, data_type_uvar));
        return ok(NecroType, type->ownership);
    }

    case NECRO_TYPE_CON:
        if (type->con.con_symbol == data_type_symbol)
        {
            assert(type->ownership != NULL);
            NecroType* args = type->con.args;
            while (args != NULL)
            {
                necro_try_result(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, args->list.item, scope, NULL, source_loc, end_loc, data_type_symbol, data_type_uvar));
                if (necro_type_is_inhabited(base, args->list.item))
                    necro_constraint_push_back_uniqueness_constraint(arena, con_env, base, intern, type->ownership, args->list.item->ownership, source_loc, end_loc);
                args = args->list.next;
            }
            return ok(NecroType, type->ownership);
        }
        else
        {
            if (!necro_type_is_inhabited(base, type))
                type->ownership = base->ownership_share->type;
            else
                type->ownership = NULL;
            type->ownership = necro_type_find(type->ownership);
            NecroType* args = type->con.args;
            while (args != NULL)
            {
                NecroType* arg_ownership = necro_try_result(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, args->list.item, scope, NULL, source_loc, end_loc, data_type_symbol, data_type_uvar));
                arg_ownership            = necro_type_find(arg_ownership);
                if (necro_type_is_inhabited(base, args->list.item) && !necro_type_is_ownership_share(base, arg_ownership))
                {
                    if (type->ownership == NULL)
                        type->ownership = arg_ownership;
                    else if (!(type->ownership->type == NECRO_TYPE_VAR && arg_ownership->type == NECRO_TYPE_VAR && type->ownership->var.var_symbol == arg_ownership->var.var_symbol))
                        type->ownership = data_type_uvar;
                }
                args = args->list.next;
            }
            if (type->ownership == NULL)
                type->ownership = base->ownership_share->type;
            return ok(NecroType, type->ownership);
        }

    case NECRO_TYPE_APP:
    {
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        else
            type->ownership = NULL;
        NecroType* app = type;
        while (app->type == NECRO_TYPE_APP)
        {
            NecroType* arg_ownership = necro_try_result(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, app->app.type2, scope, NULL, source_loc, end_loc, data_type_symbol, data_type_uvar));
            arg_ownership            = necro_type_find(arg_ownership);
            type->ownership          = necro_type_find(type->ownership);
            if (necro_type_is_inhabited(base, app->app.type2) && !necro_type_is_ownership_share(base, arg_ownership))
            {
                if (type->ownership == NULL)
                    type->ownership = arg_ownership;
                else if (!(type->ownership->type == NECRO_TYPE_VAR && arg_ownership->type == NECRO_TYPE_VAR && type->ownership->var.var_symbol == arg_ownership->var.var_symbol))
                    type->ownership = data_type_uvar;
            }
            app = necro_type_find(app->app.type1);
        }
        NecroType* app_var_ownership = necro_try_result(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, app, scope, NULL, source_loc, end_loc, data_type_symbol, data_type_uvar));
        assert(necro_type_is_ownership_share(base, app_var_ownership));
        if (type->ownership == NULL)
            type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    }

    case NECRO_TYPE_FOR:
        type->ownership = necro_try_result(NecroType, necro_uniqueness_propagate_data_con(arena, con_env, base, intern, type->for_all.type, scope, free_vars, source_loc, end_loc, data_type_symbol, data_type_uvar));
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

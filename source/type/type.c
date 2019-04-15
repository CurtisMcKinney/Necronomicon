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
#include "type.h"
#include "renamer.h"
#include "base.h"
#include "alias_analysis.h"
#include "infer.h"

#define NECRO_TYPE_DEBUG 0
#if NECRO_TYPE_DEBUG
#define TRACE_TYPE(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE(...)
#endif

// TODO: "Normalize" type variables in printing so that you never print the same variable name twice with two different meanings!
///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_type_unify_order(NecroType* type1, NecroType* type2);

NecroType* necro_type_fresh_uniqueness_var(NecroPagedArena* arena)
{
    NecroAstSymbol* var_symbol = necro_ast_symbol_create(arena, NULL, NULL, NULL, NULL);
    NecroType*      uvar       = necro_paged_arena_alloc(arena, sizeof(NecroType));
    uvar->pre_supplied         = false;
    uvar->type                 = NECRO_TYPE_VAR;
    uvar->kind                 = NULL;
    uvar->constraints          = NULL;
    uvar->var                  = (NecroTypeVar)
    {
        .var_symbol  = var_symbol,
        .arity       = -1,
        .is_rigid    = false,
        .context     = NULL,
        .bound       = NULL,
        .scope       = NULL,
        .order       = NECRO_TYPE_ZERO_ORDER,
    };
    var_symbol->type = uvar;
    return uvar;
}

NecroType* necro_type_alloc(NecroPagedArena* arena)
{
    NecroType* type    = necro_paged_arena_alloc(arena, sizeof(NecroType));
    type->pre_supplied = false;
    type->kind         = NULL;
    type->constraints  = NULL,
    type->ownership    = NULL;
    return type;
}

NecroType* necro_type_var_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol, NecroScope* scope)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_VAR;
    type->kind      = NULL;
    type->var       = (NecroTypeVar)
    {
        .var_symbol = var_symbol,
        .arity      = -1,
        .is_rigid   = false,
        .context    = NULL,
        .bound      = NULL,
        .scope      = scope,
        .order      = NECRO_TYPE_POLY_ORDER,
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
        .free_vars = NULL,
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
        .var_symbol = var_symbol,
        .type       = type,
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

NecroTypeClassContext* necro_context_deep_copy(NecroPagedArena* arena, NecroTypeClassContext* context)
{
    if (context == NULL)
        return NULL;
    NecroTypeClassContext* new_context = necro_paged_arena_alloc(arena, sizeof(NecroTypeClassContext));
    new_context->class_symbol          = context->class_symbol;
    new_context->type_class            = context->type_class;
    new_context->var_symbol            = context->var_symbol;
    new_context->next                  = necro_context_deep_copy(arena, context->next);
    return new_context;
}

NecroType* necro_type_deep_copy_go(NecroPagedArena* arena, NecroType* type)
{
    if (type == NULL)
        return NULL;
    type = necro_type_find(type);
    NecroType* new_type = NULL;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        new_type               = necro_type_var_create(arena, type->var.var_symbol, type->var.scope);
        new_type->var.is_rigid = type->var.is_rigid;
        new_type->var.arity    = type->var.arity;
        new_type->var.order    = type->var.order;
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
    new_type->kind = necro_type_find(type->kind);
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
    NecroType* curr = con->con.args;
    NecroType* tail = NULL;
    NecroType* head = NULL;
    while (curr->list.next != NULL)
    {
        curr = necro_type_list_create(arena, curr->list.item, curr->list.next);
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
    return ap;
}

// NOTE: Expects that the type function being applied is TypeCon and NOT a TypeVar. If it is, it asserts(false);
NecroType* necro_type_uncurry_app(NecroPagedArena* arena, NecroBase* base, NecroType* app)
{
    assert(arena != NULL);
    assert(app != NULL);
    app = necro_type_find(app);
    assert(app->type == NECRO_TYPE_APP);

    // Get Args from TypeApp applications
    NecroType* args = NULL;
    while (app->type == NECRO_TYPE_APP)
    {
        args = necro_type_list_create(arena, app->app.type2, args);
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
    unwrap(void, necro_kind_infer_default_unify_with_star(arena, base, con, NULL, NULL_LOC, NULL_LOC));
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

bool necro_type_is_copy_type(const NecroType* type)
{
    type = necro_type_find_const(type);
    if (type->type != NECRO_TYPE_CON)
        return false;
    else
        return type->con.con_symbol->is_enum;
}

bool necro_type_bind_var_to_type_if_instance_of_context(NecroPagedArena* arena, NecroType* type, NecroAstSymbol* type_symbol, const NecroTypeClassContext* context)
{
    assert(type->type == NECRO_TYPE_VAR);
    assert(!type->var.is_rigid);
    while (context != NULL)
    {
        NecroInstanceList* instance_list = type_symbol->instance_list;
        while (instance_list != NULL)
        {
            if (instance_list->data->type_class_name == context->class_symbol)
                break;
            instance_list = instance_list->next;
        }
        if (instance_list != NULL)
            context = context->next;
        else
            return false;
    }
    type->var.bound       = necro_type_con_create(arena, type_symbol, NULL);
    type->var.bound->kind = type_symbol->type->kind;
    return true;
}

bool necro_type_default(NecroPagedArena* arena, NecroBase* base, NecroType* type)
{
    assert(type->type == NECRO_TYPE_VAR);
    bool                   contains_fractional = false;
    bool                   contains_num        = false;
    bool                   contains_eq         = false;
    bool                   contains_ord        = false;
    // bool                   contains_show       = false;
    NecroTypeClassContext* context             = type->var.context;
    while (context != NULL)
    {
        contains_num        = contains_num || context->class_symbol == base->num_type_class;
        contains_fractional = contains_fractional || context->class_symbol == base->fractional_type_class;
        contains_eq         = contains_eq || context->class_symbol == base->eq_type_class;
        contains_ord        = contains_ord || context->class_symbol == base->ord_type_class;
        // contains_show       = contains_show || context->class_symbol == base->s;
        context             = context->next;
    }
    if (contains_fractional)
    {
        if (necro_type_bind_var_to_type_if_instance_of_context(arena, type, base->float_type, type->var.context))
            return true;
    }
    else if (contains_num)
    {
        if (necro_type_bind_var_to_type_if_instance_of_context(arena, type, base->int_type, type->var.context) ||
            necro_type_bind_var_to_type_if_instance_of_context(arena, type, base->float_type, type->var.context))
            return true;
    }
    else if (contains_eq || contains_ord)
    {
        if (necro_type_bind_var_to_type_if_instance_of_context(arena, type, base->unit_type, type->var.context))
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
        bool is_type1_poly = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->app.type1, macro_type, source_loc, end_loc));
        bool is_type2_poly = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->app.type2, macro_type, source_loc, end_loc));
        return ok(bool, is_type1_poly || is_type2_poly);
    }
    case NECRO_TYPE_FUN:
    {
        bool is_type1_poly = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->fun.type1, macro_type, source_loc, end_loc));
        bool is_type2_poly = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, type->fun.type2, macro_type, source_loc, end_loc));
        return ok(bool, is_type1_poly || is_type2_poly);
    }
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type->con.args;
        bool is_poly          = false;
        while (args != NULL)
        {
            const bool is_poly_item = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, args->list.item, macro_type, source_loc, end_loc));
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

NecroInstSub* necro_type_union_subs(NecroInstSub* subs1, NecroInstSub* subs2)
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
        curr_sub2 = curr_sub2->next;
    }
    subs1->next = necro_type_union_subs(subs1->next, subs2);
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

NecroResult(NecroType) necro_type_bind_var(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type_var_type, NecroType* type, NecroScope* scope)
{
    assert(type_var_type->type == NECRO_TYPE_VAR);
    type_var_type          = necro_type_find(type_var_type);
    NecroTypeVar* type_var = &type_var_type->var;
    necro_try(NecroType, necro_type_unify_order(type_var_type, type));
    necro_try(NecroType, necro_propagate_type_classes(arena, con_env, base, type_var->context, type, scope));
    type_var->bound = necro_type_find(type);
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_unify_var(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
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
        else if (type1->var.is_rigid && type2->var.is_rigid) return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol, type2));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        if (type1->var.is_rigid)                             return necro_type_bind_var(arena, con_env, base, type2, type1, scope);
        else if (type2->var.is_rigid)                        return necro_type_bind_var(arena, con_env, base, type1, type2, scope);
        const bool is_type1_bound_in_scope = necro_type_is_bound_in_scope(type1, scope);
        const bool is_type2_bound_in_scope = necro_type_is_bound_in_scope(type2, scope);
        if (is_type1_bound_in_scope && is_type2_bound_in_scope)
        {
            const size_t type1_distance = necro_type_bound_in_scope_distance(type1, scope);
            const size_t type2_distance = necro_type_bound_in_scope_distance(type2, scope);
            if (type1_distance >= type2_distance)
                return necro_type_bind_var(arena, con_env, base, type2, type1, scope);
            else
                return necro_type_bind_var(arena, con_env, base, type1, type2, scope);
        }
        else if (is_type1_bound_in_scope)                    return necro_type_bind_var(arena, con_env, base, type2, type1, scope);
        else                                                 return necro_type_bind_var(arena, con_env, base, type1, type2, scope);
    case NECRO_TYPE_FUN:
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        if (type1->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol , type2));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        necro_try(NecroType, necro_type_bind_var(arena, con_env, base, type1, type2, scope));
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
        necro_try(NecroType, necro_type_bind_var(arena, con_env, base, type2, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->app.type2, type2->app.type2, scope));
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->app.type1, type2->app.type1, scope));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_type_curry_con(arena, base, type2);
        assert(uncurried_con != NULL);
        necro_try(NecroType, necro_type_unify(arena, con_env, base, type1, uncurried_con, scope));
        necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1, type2, scope));
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
        necro_try(NecroType, necro_type_bind_var(arena, con_env, base, type2, type1, scope));
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
        necro_try(NecroType, necro_type_bind_var(arena, con_env, base, type2, type1, scope));
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
            // TODO (Curtis, 2-10-19): Kind unify here!
            // necro_kind_unify(type1->kind, type2->)
            // necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
            type1 = type1->con.args;
            type2 = type2->con.args;
            while (type1 != NULL && type2 != NULL)
            {
                // TODO: (Curtis, 2-8-18): Replace with kinds check!?!?!?
                // if (type1 == NULL || type2 == NULL)
                //     return necro_type_mismatched_arity_error(original_type1, original_type2, NULL, NULL, NULL_LOC, NULL_LOC);
                assert(type1->type == NECRO_TYPE_LIST);
                assert(type2->type == NECRO_TYPE_LIST);
                necro_try(NecroType, necro_type_unify(arena, con_env, base, type1->list.item, type2->list.item, scope));
                necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, type1->list.item, type2->list.item, scope));
                type1 = type1->list.next;
                type2 = type2->list.next;
            }
            necro_try(NecroType, necro_type_infer_and_unify_ownership_for_two_types(arena, con_env, base, original_type1, original_type2, scope));
            return ok(NecroType, NULL);
        }
    case NECRO_TYPE_APP:
    {
        // necro_try(NecroType, necro_kind_unify(type1->kind, type2->kind, scope));
        NecroType* uncurried_con = necro_type_curry_con(arena, base, type1);
        assert(uncurried_con != NULL);
        // if (uncurried_con == NULL)
        //     return necro_type_mismatched_arity_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        // else
        return necro_type_unify(arena, con_env, base, uncurried_con, type2, scope);
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
        necro_try(NecroType, necro_type_bind_var(arena, con_env, base, type2, type1, scope));
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
        necro_try(NecroType, necro_type_bind_var(arena, con_env, base, type2, type1, scope));
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

NecroResult(NecroType) necro_type_unify_with_info(NecroPagedArena* arena, NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_type_unify_with_full_info(arena, con_env, base, type1, type2, scope, source_loc, end_loc, type1, type2);
}

NecroResult(NecroType) necro_type_unify_with_full_info(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroType* macro_type1, NecroType* macro_type2)
{
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
        result.error->default_type_error_data2.macro_type1 = macro_type1->kind;
        result.error->default_type_error_data2.macro_type2 = macro_type2->kind;
        result.error->default_type_error_data2.source_loc  = source_loc;
        result.error->default_type_error_data2.end_loc     = end_loc;
        break;

    case NECRO_TYPE_NOT_AN_INSTANCE_OF:
        result.error->default_type_class_error_data.source_loc  = source_loc;
        result.error->default_type_class_error_data.end_loc     = end_loc;
        result.error->default_type_class_error_data.macro_type1 = macro_type1;
        result.error->default_type_class_error_data.macro_type2 = macro_type2;
        break;

    case NECRO_TYPE_MISMATCHED_ORDER:
        result.error->mismatched_order_error_data.source_loc = source_loc;
        result.error->mismatched_order_error_data.end_loc    = end_loc;
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
NecroInstSub* necro_create_inst_sub(NecroPagedArena* arena, NecroAstSymbol* var_to_replace, NecroScope* scope, NecroTypeClassContext* context, NecroInstSub* next)
{
    NecroType* type_to_replace            = necro_type_find(var_to_replace->type);
    NecroType* type_var                   = necro_type_fresh_var(arena, scope);
    type_var->var.is_rigid                = false;
    type_var->var.var_symbol->name        = NULL;
    type_var->var.var_symbol->source_name = var_to_replace->source_name;
    type_var->kind                        = type_to_replace->kind;
    type_var->ownership                   = type_to_replace->ownership;
    type_var->var.order                   = type_to_replace->var.order;
    type_var->var.context                 = context;
    // type_var->var.scope                   = type_to_replace->var.scope;
    if (type_var->var.order == NECRO_TYPE_HIGHER_ORDER)
        type_var->var.order = NECRO_TYPE_POLY_ORDER;
    NecroInstSub* sub = necro_paged_arena_alloc(arena, sizeof(NecroInstSub));
    *sub              = (NecroInstSub)
    {
        .var_to_replace = var_to_replace,
        .new_name       = type_var,
        .next           = next,
    };
    return sub;
}

NecroInstSub* necro_create_inst_sub_manual(NecroPagedArena* arena, NecroAstSymbol* var_to_replace, NecroType* new_type, NecroInstSub* next)
{
    NecroInstSub* sub   = necro_paged_arena_alloc(arena, sizeof(NecroInstSub));
    sub->var_to_replace = var_to_replace;
    sub->new_name       = new_type;
    sub->next           = next;
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
        if (type_to_maybe_sub->var.var_symbol == curr_sub->var_to_replace || type_to_maybe_sub == subs->new_name ||
           (type_to_maybe_sub->var.var_symbol->name != NULL && type_to_maybe_sub->var.var_symbol->name == curr_sub->var_to_replace->name))
            return curr_sub->new_name;
        curr_sub = curr_sub->next;
    }
    return type_to_maybe_sub;
}

NecroConstraintList* necro_type_constraint_replace_with_subs_deep_copy(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroConstraintList* constraints, NecroInstSub* subs)
{
    NecroConstraintList* new_list = NULL;
    while (constraints != NULL)
    {
        switch (constraints->data->type)
        {
            case NECRO_CONSTRAINT_AND: assert(false); break;
            case NECRO_CONSTRAINT_EQL: assert(false); break;
            case NECRO_CONSTRAINT_FOR: assert(false); break;
            case NECRO_CONSTRAINT_UNI: new_list = necro_constraint_append_uniqueness_coercion_and_queue_push_back(arena, con_env, necro_type_maybe_sub_var(constraints->data->uni.u1, subs), necro_type_maybe_sub_var(constraints->data->uni.u2, subs), new_list); break;
            default:                   assert(false); break;
        }
        constraints = constraints->next;
    }
    return new_list;
}

NecroType* necro_type_replace_with_subs_deep_copy_go(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroType* type, NecroInstSub* subs)
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
            new_type = necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->for_all.type, subs);
        else
            new_type = necro_type_for_all_create(arena, type->for_all.var_symbol, necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->for_all.type, subs));
        break;
    }
    case NECRO_TYPE_APP:  new_type = necro_type_app_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->app.type1, subs), necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->app.type2, subs)); break;
    case NECRO_TYPE_FUN:  new_type = necro_type_fn_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->fun.type1, subs), necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->fun.type2, subs)); break;
    case NECRO_TYPE_CON:  new_type = necro_type_con_create(arena, type->con.con_symbol, necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->con.args, subs)); break;
    case NECRO_TYPE_LIST: new_type = necro_type_list_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->list.item, subs), necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->list.next, subs)); break;
    case NECRO_TYPE_NAT:  new_type = necro_type_nat_create(arena, type->nat.value); break;
    case NECRO_TYPE_SYM:  new_type = necro_type_sym_create(arena, type->sym.value); break;
    default: assert(false); return NULL;
    }
    if (type->ownership != NULL)
    {
        if (type->ownership->type == NECRO_TYPE_VAR)
        {
            new_type->ownership       = necro_type_replace_with_subs_deep_copy_go(arena, con_env, type->ownership, subs);
            new_type->ownership->kind = type->ownership->kind;
        }
        else
            new_type->ownership = type->ownership;
    }
    // new_type->kind = type->kind;
    new_type->kind        = NULL;
    new_type->constraints = necro_type_constraint_replace_with_subs_deep_copy(arena, con_env, type->constraints, subs);
    return new_type;
}

NecroResult(NecroType) necro_type_replace_with_subs_deep_copy(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroInstSub* subs)
{
    NecroType* new_type = necro_type_replace_with_subs_deep_copy_go(arena, con_env, type, subs);
    if (new_type == NULL)
        return ok(NecroType, NULL);
    necro_try(NecroType, necro_kind_infer(arena, base, new_type, NULL_LOC, NULL_LOC));
    return ok(NecroType, new_type);
}

// TODO: Do we need the scope argument?
NecroType* necro_type_replace_with_subs_go(NecroPagedArena* arena, NecroType* type, NecroInstSub* subs)
{
    if (type == NULL || subs == NULL)
        return type;
    type = necro_type_find(type);
    if (type->ownership != NULL && type->ownership->type == NECRO_TYPE_VAR)
        type->ownership = necro_type_replace_with_subs_go(arena, type->ownership, subs);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        while (subs != NULL)
        {
            if (type->for_all.var_symbol == subs->var_to_replace ||
               (type->for_all.var_symbol->name != NULL && type->for_all.var_symbol->name == subs->var_to_replace->name))
            {
                subs->new_name->ownership = necro_type_replace_with_subs_go(arena, subs->new_name->ownership, subs);
                return subs->new_name;
            }
            subs = subs->next;
        }
        return type;
    case NECRO_TYPE_FOR:
    {
        NecroInstSub* curr_sub = subs;
        while (curr_sub != NULL)
        {
            if (type->for_all.var_symbol == curr_sub->var_to_replace ||
               (type->for_all.var_symbol->name != NULL && type->for_all.var_symbol->name == curr_sub->var_to_replace->name))
            {
                return necro_type_replace_with_subs_go(arena, type->for_all.type, subs);
            }
            curr_sub = curr_sub->next;
        }
        NecroType* next_type = necro_type_replace_with_subs_go(arena, type->for_all.type, subs);
        if (next_type == type->for_all.type)
            return type;
        else
            return necro_type_for_all_create(arena, type->for_all.var_symbol, next_type);
    }
    case NECRO_TYPE_APP:
    {
        NecroType* type1 = necro_type_replace_with_subs_go(arena, type->app.type1, subs);
        NecroType* type2 = necro_type_replace_with_subs_go(arena, type->app.type2, subs);
        if (type1 == type->app.type1 && type2 == type->app.type2)
            return type;
        else
            return necro_type_app_create(arena, type1, type2);
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* type1   = necro_type_replace_with_subs_go(arena, type->fun.type1, subs);
        NecroType* type2   = necro_type_replace_with_subs_go(arena, type->fun.type2, subs);
        if (type1 == type->fun.type1 && type2 == type->fun.type2)
            return type;
        else
            return necro_type_fn_create(arena, type1, type2);
    }
    case NECRO_TYPE_CON:
    {
        NecroType* args = necro_type_replace_with_subs_go(arena, type->con.args, subs);
        if (args == type->con.args)
            return type;
        else
            return necro_type_con_create(arena, type->con.con_symbol, args);
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* item = necro_type_replace_with_subs_go(arena, type->list.item, subs);
        NecroType* next = necro_type_replace_with_subs_go(arena, type->list.next, subs);
        if (item == type->list.item && next == type->list.next)
            return type;
        else
            return necro_type_list_create(arena, item, next);
    }
    case NECRO_TYPE_NAT:
        return type;
    case NECRO_TYPE_SYM:
        return type;
    default:
        assert(false);
        return NULL;
    }
}

NecroResult(NecroType) necro_type_replace_with_subs(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroInstSub* subs)
{
    NecroType* new_type = necro_type_replace_with_subs_go(arena, type, subs);
    if (new_type == NULL)
        return ok(NecroType, NULL);
    // necro_try_map(void, NecroType, necro_kind_infer_gen_unify_with_star(arena, base, new_type, NULL, NULL_LOC, NULL_LOC));
    necro_try(NecroType, necro_kind_infer(arena, base, new_type, NULL_LOC, NULL_LOC));
    return ok(NecroType, new_type);
}

NecroResult(NecroType) necro_type_instantiate(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope)
{
    UNUSED(scope);
    assert(type != NULL);
    type = necro_type_find(type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs         = necro_create_inst_sub(arena, current_type->for_all.var_symbol, scope, current_type->for_all.var_symbol->type->var.context, subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_try(NecroType, necro_type_replace_with_subs_deep_copy(arena, con_env, base, current_type, subs));
    return ok(NecroType, result);
}

NecroResult(NecroType) necro_type_instantiate_with_subs(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope, NecroInstSub** subs)
{
    UNUSED(scope);
    assert(type != NULL);
    type = necro_type_find(type);
    NecroType*    current_type = type;
    *subs                      = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        *subs        = necro_create_inst_sub(arena, current_type->for_all.var_symbol, scope, current_type->for_all.var_symbol->type->var.context, *subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_try(NecroType, necro_type_replace_with_subs_deep_copy(arena, con_env, base, current_type, *subs));
    return ok(NecroType, result);
}

//=====================================================
// Gen
//=====================================================

void necro_type_append_to_foralls(NecroPagedArena* arena, NecroType** for_alls, NecroType* type)
{
    NecroType* curr = *for_alls;
    while (curr != NULL)
    {
        if (curr->for_all.var_symbol == type->var.var_symbol)
            return;
        curr = curr->for_all.type;
    }
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

NecroType* necro_type_gen_go(NecroPagedArena* arena, NecroType* type, NecroScope* scope, NecroType** for_alls)
{
    if (type == NULL)
        return NULL;

    type                     = necro_type_find(type);
    NecroType* new_ownership = necro_type_gen_go(arena, type->ownership, scope, for_alls);

    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
        {
            necro_type_append_to_foralls(arena, for_alls, type);
            return type;
        }
        else if (necro_type_is_bound_in_scope(type, scope))
        {
            return type;
        }
        type->var.is_rigid         = true;
        type->ownership            = new_ownership;
        type->var.var_symbol->type = type; // IS THIS NECESSARY!?
        necro_type_append_to_foralls(arena, for_alls, type);
        return type;
    }

    case NECRO_TYPE_APP:
    {
        NecroType* type1       = necro_type_gen_go(arena, type->app.type1, scope, for_alls);
        NecroType* type2       = necro_type_gen_go(arena, type->app.type2, scope, for_alls);
        NecroType* new_type    = necro_type_app_create(arena, type1, type2);
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* type1       = necro_type_gen_go(arena, type->fun.type1, scope, for_alls);
        NecroType* type2       = necro_type_gen_go(arena, type->fun.type2, scope, for_alls);
        NecroType* new_type    = necro_type_fn_create(arena, type1, type2);
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* item_type   = necro_type_gen_go(arena, type->list.item, scope, for_alls);
        NecroType* next_type   = necro_type_gen_go(arena, type->list.next, scope, for_alls);
        NecroType* new_type    = necro_type_list_create(arena, item_type, next_type);
        new_type->kind         = type->kind;
        new_type->ownership    = new_ownership;
        new_type->pre_supplied = type->pre_supplied;
        return new_type;
    }
    case NECRO_TYPE_CON:
    {
        NecroType* new_type    = necro_type_con_create(arena, type->con.con_symbol, necro_type_gen_go(arena, type->con.args, scope, for_alls));
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

NecroResult(NecroType) necro_type_generalize(NecroPagedArena* arena, NecroConstraintEnv* con_env, struct NecroBase* base, NecroType* type, struct NecroScope* scope)
{
    UNUSED(con_env);
    assert(type != NULL);
    assert(type->type != NECRO_TYPE_FOR);
    necro_try_map(void, NecroType, necro_kind_infer_default(arena, base, type, NULL_LOC, NULL_LOC));
    NecroType* for_alls   = NULL;
    NecroType* gen_type   = necro_type_gen_go(arena, type, scope, &for_alls);
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
        return ok(NecroType, for_alls);
    }
    else
    {
        return ok(NecroType, gen_type);
    }
}

//=====================================================
// Printing
//=====================================================
void necro_type_print(const NecroType* type)
{
    necro_type_fprint(stdout, type);
}

bool necro_is_simple_type(const NecroType* type)
{
    assert(type != NULL);
    return (type->type == NECRO_TYPE_VAR || type->type == NECRO_TYPE_NAT || type->type == NECRO_TYPE_SYM || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0)) && (type->ownership == NULL || type->ownership->type != NECRO_TYPE_VAR);
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

    if (con_string[0] != '(' && con_string[0] != '[')
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

    fprintf(stream, "(");
    while (current_element != NULL)
    {
        necro_type_fprint(stream, current_element->list.item);
        if (current_element->list.next != NULL)
            fprintf(stream, ",");
        current_element = current_element->list.next;
    }
    fprintf(stream, ")");
    return true;
}

void necro_type_fprint_type_var(FILE* stream, const NecroAstSymbol* var_symbol)
{
    // NecroType* type = var_symbol->type;
    // NOTE: removing this for now in the hopes that we can side step this whole order business
    // if (type->var.order != NECRO_TYPE_ZERO_ORDER)
    //     fprintf(stream, "^");
    if (var_symbol->source_name != NULL && var_symbol->source_name->str != NULL)
    {
        fprintf(stream, "%s", var_symbol->source_name->str);
    }
    else
    {
        size_t truncated_symbol_pointer = (size_t)var_symbol;
        truncated_symbol_pointer        = truncated_symbol_pointer & 8191;
        const NecroType* type_var       = necro_type_find(var_symbol->type);
        if (type_var != NULL && type_var->kind != NULL && type_var->kind->type == NECRO_TYPE_CON && (strcmp(type_var->kind->con.con_symbol->source_name->str, "Ownership") == 0))
            fprintf(stream, "u%x", truncated_symbol_pointer);
        else
            fprintf(stream, "a%x", truncated_symbol_pointer);
    }
}

void necro_type_fprint_ownership(FILE* stream, const NecroType* ownership)
{
    ownership = necro_type_find_const(ownership);
    if (ownership->type == NECRO_TYPE_VAR)
    {
        necro_type_fprint_type_var(stream, ownership->var.var_symbol);
        fprintf(stream, ":");
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
    case NECRO_CONSTRAINT_AND: return;
    case NECRO_CONSTRAINT_EQL: return;
    case NECRO_CONSTRAINT_FOR: return;
    case NECRO_CONSTRAINT_UNI:
        necro_type_fprint_type_var(stream, constraint->uni.u1->var.var_symbol);
        fprintf(stream, "<=");
        necro_type_fprint_type_var(stream, constraint->uni.u2->var.var_symbol);
        return;
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
        necro_type_fprint_ownership(stream, type->ownership);
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
        const bool is_not_shared = type->ownership != NULL && (type->ownership->type == NECRO_TYPE_VAR || (type->ownership->type == NECRO_TYPE_CON && (strcmp(type->ownership->con.con_symbol->source_name->str, "Shared") != 0)));
        if (is_not_shared)
            fprintf(stream, "(");
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            fprintf(stream, "(");
        necro_type_fprint(stream, type->fun.type1);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            fprintf(stream, ")");
        fprintf(stream, " -> ");
        necro_type_fprint(stream, type->fun.type2);
        if (is_not_shared)
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
        fprintf(stream, "forall ");

        // Print quantified type vars
        const NecroType* for_all_head = type;
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            necro_type_fprint_type_var(stream, type->for_all.var_symbol);
            fprintf(stream, " ");
            type = type->for_all.type;
        }
        necro_type_fprint_type_var(stream, type->for_all.var_symbol);
        fprintf(stream, ". ");
        type = type->for_all.type;

        // Print context
        type             = for_all_head;
        bool has_context = false;
        while (type->type == NECRO_TYPE_FOR)
        {
            if (type->for_all.var_symbol->type->var.context != NULL || type->for_all.var_symbol->type->constraints != NULL)
            {
                has_context = true;
                break;
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
                NecroTypeClassContext* context = type->for_all.var_symbol->type->var.context;
                while (context != NULL)
                {
                    if (count > 0)
                        fprintf(stream, ", ");
                    fprintf(stream, "%s ", context->class_symbol->source_name->str);
                    necro_type_fprint_type_var(stream, type->for_all.var_symbol);
                    // if (type->for_all.var_symbol->source_name != NULL)
                    //     fprintf(stream, "%s", type->for_all.var_symbol->source_name->str);
                    // else
                    //     fprintf(stream, "t%p", type->for_all.var_symbol);
                    context = context->next;
                    count++;
                }
                NecroConstraintList* constraints = type->for_all.var_symbol->type->constraints;
                while (constraints != NULL)
                {
                    if (count > 0)
                        fprintf(stream, ", ");
                    necro_constraint_fprint(stream, constraints->data);
                    constraints = constraints->next;
                    count++;
                }
                type = type->for_all.type;
            }
            fprintf(stream, ") => ");
        }

        // Print rest of type
        necro_type_fprint(stream, type);
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
        assert(false &&  "Mangled types must be fully concrete");
        return 0;

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
        char buffer[16];
        assert(type->nat.value < INT32_MAX);
        itoa((int) type->nat.value, buffer, 10);
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
    case 8:  con_symbol = base->tuple7_type;  break;
    case 9:  con_symbol = base->tuple9_type;  break;
    case 10: con_symbol = base->tuple10_type; break;
    default:
        assert(false && "Tuple size too large");
        break;
    }
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

size_t necro_type_hash(NecroType* type)
{
    if (type == NULL)
        return 0;
    type = necro_type_find(type);
    size_t h = 0;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        h = (size_t) type->var.var_symbol;
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
        h = 4111;
        NecroTypeClassContext* context = type->for_all.var_symbol->type->var.context;
        while (context != NULL)
        {
            h = h ^ (size_t) context->var_symbol ^ (size_t) context->class_symbol;
            context = context->next;
        }
        h = h ^ necro_type_hash(type->for_all.type);
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
    return h;
}

bool necro_type_exact_unify(NecroType* type1, NecroType* type2)
{
    type1 = necro_type_find(type1);
    type2 = necro_type_find(type2);
    if (type1 == NULL && type2 == NULL)
        return true;
    if (type1 == NULL || type2 == NULL)
        return false;
    if (type1->type != type2->type)
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
        NecroTypeClassContext* context1 = type1->for_all.var_symbol->type->var.context;
        NecroTypeClassContext* context2 = type2->for_all.var_symbol->type->var.context;
        while (context1 != NULL || context2 != NULL)
        {
            if (context1 == NULL || context2 == NULL)
                return false;
            if (context1->var_symbol != context2->var_symbol || context1->class_symbol != context2->class_symbol)
                return false;
            context1 = context1->next;
            context2 = context2->next;
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

NecroResult(NecroType) necro_type_set_zero_order(NecroType* type, const NecroSourceLoc* source_loc, const NecroSourceLoc* end_loc)
{
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        switch (type->var.order)
        {
        case NECRO_TYPE_ZERO_ORDER:   return ok(NecroType, NULL);
        case NECRO_TYPE_HIGHER_ORDER: return necro_type_lifted_type_restriction_error(NULL, type, *source_loc, *end_loc);
        case NECRO_TYPE_POLY_ORDER:
            if (!type->var.is_rigid)
                type->var.order = NECRO_TYPE_ZERO_ORDER;
            return ok(NecroType, NULL);
        }
    case NECRO_TYPE_FOR:
        switch (type->for_all.var_symbol->type->var.order)
        {
        case NECRO_TYPE_ZERO_ORDER:   break;
        case NECRO_TYPE_POLY_ORDER:   break;
        case NECRO_TYPE_HIGHER_ORDER: return necro_type_lifted_type_restriction_error(NULL, type, *source_loc, *end_loc);
        }
        return necro_type_set_zero_order(type->for_all.type, source_loc, end_loc);
    case NECRO_TYPE_FUN:
        return necro_type_lifted_type_restriction_error(NULL, type, *source_loc, *end_loc);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_set_zero_order(type->app.type1, source_loc, end_loc));
        return necro_type_set_zero_order(type->app.type2, source_loc, end_loc);
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type->con.args;
        while (args != NULL)
        {
            necro_try(NecroType, necro_type_set_zero_order(args->list.item, source_loc, end_loc));
            args = args->list.next;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_TYPE_LIST:
        assert(false);
        return ok(NecroType, NULL);
    case NECRO_TYPE_NAT:
        return ok(NecroType, NULL);
    case NECRO_TYPE_SYM:
        return ok(NecroType, NULL);
    default:
        assert(false);
        return ok(NecroType, NULL);
    }
}

NecroResult(NecroType) necro_type_unify_order(NecroType* type1, NecroType* type2)
{
    assert(type1->type == NECRO_TYPE_VAR);
    type2 = necro_type_find(type2);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type1->var.order == type2->var.order)
        {
            return ok(NecroType, NULL);
        }
        else if (type2->var.order == NECRO_TYPE_POLY_ORDER)
        {
            if (!type2->var.is_rigid)
                type2->var.order = type1->var.order;
            return ok(NecroType, NULL);
        }
        else if (type1->var.order == NECRO_TYPE_POLY_ORDER)
        {
            if (!type1->var.is_rigid)
                type1->var.order = type2->var.order;
            return ok(NecroType, NULL);
        }
        else
        {
            return necro_type_mismatched_order_error(type1->var.order, type2->var.order, type2, NULL_LOC, NULL_LOC);
        }
    case NECRO_TYPE_FUN:
        if (type1->var.order == NECRO_TYPE_ZERO_ORDER)
        {
            return necro_type_mismatched_order_error(type1->var.order, NECRO_TYPE_HIGHER_ORDER, type2, NULL_LOC, NULL_LOC);
        }
        else
        {
            // if (!type1->var.is_rigid)
            //     type1->var.order = NECRO_TYPE_HIGHER_ORDER;
            // necro_try(NecroType, necro_type_unify_order(type1, type2->fun.type1));
            // return necro_type_unify_order(type1, type2->fun.type2);
            return ok(NecroType, NULL);
        }
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_unify_order(type1, type2->app.type1));
        return necro_type_unify_order(type1, type2->app.type2);
    case NECRO_TYPE_CON:
    {
        const NecroType* args = type2->con.args;
        while (args != NULL)
        {
            necro_try(NecroType, necro_type_unify_order(type1, args->list.item));
            args = args->list.next;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_TYPE_LIST:
        assert(false);
        return ok(NecroType, NULL);
    case NECRO_TYPE_NAT:
        return ok(NecroType, NULL);
    case NECRO_TYPE_SYM:
        return ok(NecroType, NULL);
    case NECRO_TYPE_FOR:
    default:
        assert(false);
        return ok(NecroType, NULL);
    }
}

///////////////////////////////////////////////////////
// Uniqueness Attributes
///////////////////////////////////////////////////////
// NecroType* necro_type_uniqueness_attribute_alloc(NecroPagedArena* arena, NecroBase* base)
// {
//     NecroType* type            = necro_paged_arena_alloc(arena, sizeof(NecroType));
//     type->pre_supplied         = false;
//     type->kind                 = base->unique_type_attribute->type;
//     type->uniqueness_attribute = NULL;
//     return type;
// }
//
// NecroType* necro_type_uniqueness_attribute_alloc_snapshot(NecroSnapshotArena* arena, NecroBase* base)
// {
//     NecroType* type            = necro_snapshot_arena_alloc(arena, sizeof(NecroType));
//     type->pre_supplied         = false;
//     type->kind                 = base->unique_type_attribute->type;
//     type->uniqueness_attribute = NULL;
//     return type;
// }
//
// NecroType* necro_type_uniqueness_attribute_create_con(NecroPagedArena* arena, NecroBase* base, NecroAstSymbol* con_symbol, NecroType* args)
// {
//     NecroType* type = necro_type_uniqueness_attribute_alloc(arena, base);
//     type->type      = NECRO_TYPE_CON;
//     type->con       = (NecroTypeCon)
//     {
//         .con_symbol = con_symbol,
//         .args       = args,
//     };
//     return type;
// }
//
// NecroType* necro_type_uniqueness_attribute_create_con_snapshot(NecroSnapshotArena* arena, NecroBase* base, NecroAstSymbol* con_symbol, NecroType* args)
// {
//     NecroType* type = necro_type_uniqueness_attribute_alloc_snapshot(arena, base);
//     type->type      = NECRO_TYPE_CON;
//     type->con       = (NecroTypeCon)
//     {
//         .con_symbol = con_symbol,
//         .args       = args,
//     };
//     return type;
// }
//
// NecroType* necro_type_uniqueness_attribute_create_list(NecroPagedArena* arena, NecroBase* base, NecroType* item, NecroType* next)
// {
//     NecroType* type = necro_type_uniqueness_attribute_alloc(arena, base);
//     type->type      = NECRO_TYPE_LIST;
//     type->list      = (NecroTypeList)
//     {
//         .item = item,
//         .next = next,
//     };
//     return type;
// }
//
// NecroType* necro_type_uniqueness_attribute_create_list_snapshot(NecroSnapshotArena* arena, NecroBase* base, NecroType* item, NecroType* next)
// {
//     NecroType* type = necro_type_uniqueness_attribute_alloc_snapshot(arena, base);
//     type->type      = NECRO_TYPE_LIST;
//     type->list      = (NecroTypeList)
//     {
//         .item = item,
//         .next = next,
//     };
//     return type;
// }
//
// NecroType* necro_type_uniqueness_attribute_create_or(NecroPagedArena* arena, NecroBase* base, NecroType* left, NecroType* right)
// {
//     return necro_type_uniqueness_attribute_create_con(arena, base, base->disjunction_type_attribute, necro_type_uniqueness_attribute_create_list(arena, base, left, necro_type_uniqueness_attribute_create_list(arena, base, right, NULL)));
// }
//
// NecroType* necro_type_uniqueness_attribute_create_or_snapshot(NecroSnapshotArena* arena, NecroBase* base, NecroType* left, NecroType* right)
// {
//     return necro_type_uniqueness_attribute_create_con_snapshot(arena, base, base->disjunction_type_attribute, necro_type_uniqueness_attribute_create_list_snapshot(arena, base, left, necro_type_uniqueness_attribute_create_list_snapshot(arena, base, right, NULL)));
// }
//
// NecroType* necro_type_uniqueness_attribute_create_and(NecroPagedArena* arena, NecroBase* base, NecroType* left, NecroType* right)
// {
//     return necro_type_uniqueness_attribute_create_con(arena, base, base->conjunction_type_attribute, necro_type_uniqueness_attribute_create_list(arena, base, left, necro_type_uniqueness_attribute_create_list(arena, base, right, NULL)));
// }
//
// NecroType* necro_type_uniqueness_attribute_create_and_snapshot(NecroSnapshotArena* arena, NecroBase* base, NecroType* left, NecroType* right)
// {
//     return necro_type_uniqueness_attribute_create_con_snapshot(arena, base, base->conjunction_type_attribute, necro_type_uniqueness_attribute_create_list_snapshot(arena, base, left, necro_type_uniqueness_attribute_create_list_snapshot(arena, base, right, NULL)));
// }
//
// NecroType* necro_type_uniqueness_attribute_create_not(NecroPagedArena* arena, NecroBase* base, NecroType* attribute)
// {
//     return necro_type_uniqueness_attribute_create_con(arena, base, base->negation_type_attribute, necro_type_uniqueness_attribute_create_list(arena, base, attribute, NULL));
// }
//
// NecroType* necro_type_uniqueness_attribute_create_not_snapshot(NecroSnapshotArena* arena, NecroBase* base, NecroType* attribute)
// {
//     return necro_type_uniqueness_attribute_create_con_snapshot(arena, base, base->negation_type_attribute, necro_type_uniqueness_attribute_create_list_snapshot(arena, base, attribute, NULL));
// }
//
// NecroInstSub* necro_type_sub_create_manual_snapshot(NecroSnapshotArena* arena, NecroAstSymbol* var_to_replace, NecroType* new_type, NecroInstSub* next)
// {
//     NecroInstSub* sub   = necro_snapshot_arena_alloc(arena, sizeof(NecroInstSub));
//     sub->var_to_replace = var_to_replace;
//     sub->new_name       = new_type;
//     sub->next           = next;
//     return sub;
// }
//
// NecroInstSub* necro_type_sub_append_manual_snapshot(NecroSnapshotArena* arena, NecroInstSub* prev, NecroAstSymbol* var_to_replace, NecroType* new_type)
// {
//     NecroInstSub* sub   = necro_snapshot_arena_alloc(arena, sizeof(NecroInstSub));
//     sub->var_to_replace = var_to_replace;
//     sub->new_name       = new_type;
//     sub->next           = NULL;
//     if (prev == NULL)
//         return sub;
//     NecroInstSub* last = prev;
//     while (last->next != NULL)
//         last = last->next;
//     last->next = sub;
//     return prev;
// }
//
// NecroType* necro_type_uniqueness_attribute_sub_snapshot(NecroSnapshotArena* arena, NecroBase* base, NecroType* type, NecroInstSub* subs)
// {
//     if (type == NULL)
//         return NULL;
//     type = necro_type_find(type);
//     if (subs == NULL)
//         return type;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//         while (subs != NULL)
//         {
//             if (type->for_all.var_symbol == subs->var_to_replace ||
//                (type->for_all.var_symbol->name != NULL && type->for_all.var_symbol->name == subs->var_to_replace->name))
//             {
//                 return subs->new_name;
//             }
//             subs = subs->next;
//         }
//         return type;
//     case NECRO_TYPE_CON:
//         return necro_type_uniqueness_attribute_create_con_snapshot(arena, base, type->con.con_symbol, necro_type_uniqueness_attribute_sub_snapshot(arena, base, type->con.args, subs));
//     case NECRO_TYPE_LIST:
//         return necro_type_uniqueness_attribute_create_list_snapshot(arena, base, necro_type_uniqueness_attribute_sub_snapshot(arena, base, type->list.item, subs), necro_type_uniqueness_attribute_sub_snapshot(arena, base, type->list.next, subs));
//     default: assert(false);
//         return NULL;
//     }
// }
//
// inline bool necro_type_uniqueness_attribute_is_unique(NecroBase* base, const NecroType* t)
// {
//     if (t->type != NECRO_TYPE_CON)
//         return false;
//     else
//         return t->con.con_symbol == base->unique_type_attribute;
// }
//
// inline bool necro_type_uniqueness_attribute_is_zero(NecroBase* base, const NecroType* t)
// {
//     return necro_type_uniqueness_attribute_is_unique(base, t);
// }
//
//
// inline bool necro_type_uniqueness_attribute_is_non_unique(NecroBase* base, const NecroType* t)
// {
//     if (t->type != NECRO_TYPE_CON)
//         return false;
//     else
//         return t->con.con_symbol == base->non_unique_type_attribute;
// }
//
// inline bool necro_type_uniqueness_attribute_is_one(NecroBase* base, const NecroType* t)
// {
//     return necro_type_uniqueness_attribute_is_non_unique(base, t);
// }
//
// inline bool necro_type_uniqueness_attribute_is_negation(NecroBase* base, const NecroType* t)
// {
//     if (t->type != NECRO_TYPE_CON)
//         return false;
//     else
//         return t->con.con_symbol == base->negation_type_attribute;
// }
//
// inline bool necro_type_uniqueness_attribute_is_disjunction(NecroBase* base, const NecroType* t)
// {
//     if (t->type != NECRO_TYPE_CON)
//         return false;
//     else
//         return t->con.con_symbol == base->disjunction_type_attribute;
// }
//
// inline bool necro_type_uniqueness_attribute_is_conjunction(NecroBase* base, const NecroType* t)
// {
//     if (t->type != NECRO_TYPE_CON)
//         return false;
//     else
//         return t->con.con_symbol == base->conjunction_type_attribute;
// }
//
// inline NecroType* necro_type_uniqueness_attribute_one(NecroBase* base)
// {
//     return base->non_unique_type_attribute->type;
// }
//
// inline NecroType* necro_type_uniqueness_attribute_zero(NecroBase* base)
// {
//     return base->unique_type_attribute->type;
// }
//
// bool necro_type_uniqueness_attribute_equals(NecroBase* base, NecroType* left, NecroType* right)
// {
//     if (left == right)
//         return true;
//     else if (left->type != right->type)
//         return false;
//     else if (left->type == NECRO_TYPE_VAR)
//         return left->var.var_symbol == right->var.var_symbol;
//     assert(left->type == NECRO_TYPE_CON);
//     if (left->con.con_symbol != right->con.con_symbol)
//     {
//         return false;
//     }
//     else if (necro_type_uniqueness_attribute_is_unique(base, left) || necro_type_uniqueness_attribute_is_non_unique(base, left))
//     {
//         return true;
//     }
//     else if (necro_type_uniqueness_attribute_is_negation(base, left))
//     {
//         return necro_type_uniqueness_attribute_equals(base, left->con.args->list.item, right->con.args->list.item);
//     }
//     else if (necro_type_uniqueness_attribute_is_disjunction(base, left))
//     {
//         return necro_type_uniqueness_attribute_equals(base, left->con.args->list.item, right->con.args->list.item) &&
//                necro_type_uniqueness_attribute_equals(base, left->con.args->list.next->list.item, right->con.args->list.next->list.item);
//     }
//     else if (necro_type_uniqueness_attribute_is_conjunction(base, left))
//     {
//         return necro_type_uniqueness_attribute_equals(base, left->con.args->list.item, right->con.args->list.item) &&
//                necro_type_uniqueness_attribute_equals(base, left->con.args->list.next->list.item, right->con.args->list.next->list.item);
//     }
//     else
//     {
//         assert(false);
//         return false;
//     }
// }
//
// // TODO: Factoring?
// // TODO: Loop detection?
// // Judicious use of simplify?
// // Simplificaiton based on boolean algebra laws
// NecroType* necro_type_uniqueness_attribute_simplify(NecroSnapshotArena* arena, NecroBase* base, NecroType* t)
// {
//     while (true)
//     {
//         // Values
//         if (t->type == NECRO_TYPE_VAR || necro_type_uniqueness_attribute_is_zero(base, t) || necro_type_uniqueness_attribute_is_one(base, t))
//         {
//             return t;
//         }
//         // Negation
//         else if (necro_type_uniqueness_attribute_is_negation(base, t))
//         {
//             NecroType* inner = necro_type_uniqueness_attribute_simplify(arena, base, t->con.args->list.item);
//             // Involution Law
//             if (necro_type_uniqueness_attribute_is_negation(base, inner)) { t = inner->con.args->list.item; }
//             // DeMorgan's Law
//             else if (necro_type_uniqueness_attribute_is_disjunction(base, inner))
//             {
//                 t = necro_type_uniqueness_attribute_create_and_snapshot(arena, base, necro_type_uniqueness_attribute_create_not_snapshot(arena, base, inner->con.args->list.item), necro_type_uniqueness_attribute_create_not_snapshot(arena, base, inner->con.args->list.next->list.item));
//             }
//             else if (necro_type_uniqueness_attribute_is_conjunction(base, inner))
//             {
//                 t = necro_type_uniqueness_attribute_create_or_snapshot(arena, base, necro_type_uniqueness_attribute_create_not_snapshot(arena, base, inner->con.args->list.item), necro_type_uniqueness_attribute_create_not_snapshot(arena, base, inner->con.args->list.next->list.item));
//             }
//             else return t;
//         }
//         // Disjunction
//         else if (necro_type_uniqueness_attribute_is_disjunction(base, t))
//         {
//             NecroType* left  = necro_type_uniqueness_attribute_simplify(arena, base, t->con.args->list.item);
//             NecroType* right = necro_type_uniqueness_attribute_simplify(arena, base, t->con.args->list.next->list.item);
//             // Idempotent Law
//             if (necro_type_uniqueness_attribute_equals(base, left, right)) { t = left; }
//             // Identity Law
//             else if (necro_type_uniqueness_attribute_is_zero(base, left))  { t = right; }
//             else if (necro_type_uniqueness_attribute_is_one(base, right))  { t = necro_type_uniqueness_attribute_one(base); }
//             else if (necro_type_uniqueness_attribute_is_zero(base, right)) { t = left; }
//             else if (necro_type_uniqueness_attribute_is_one(base, left))   { t = necro_type_uniqueness_attribute_one(base); }
//             // Complement Law
//             else if (necro_type_uniqueness_attribute_is_negation(base, right) && necro_type_uniqueness_attribute_equals(base, left, right->con.args->list.item)) { t = necro_type_uniqueness_attribute_one(base); }
//             else if (necro_type_uniqueness_attribute_is_negation(base, left)  && necro_type_uniqueness_attribute_equals(base, right, left->con.args->list.item)) { t = necro_type_uniqueness_attribute_one(base); }
//             else return t;
//         }
//         // Conjunction
//         else if (necro_type_uniqueness_attribute_is_conjunction(base, t))
//         {
//             NecroType* left  = necro_type_uniqueness_attribute_simplify(arena, base, t->con.args->list.item);
//             NecroType* right = necro_type_uniqueness_attribute_simplify(arena, base, t->con.args->list.next->list.item);
//             // Idempotent Law
//             if (necro_type_uniqueness_attribute_equals(base, left, right)) { t = left; }
//             // Identity Law
//             else if (necro_type_uniqueness_attribute_is_zero(base, left))  { t = necro_type_uniqueness_attribute_zero(base); }
//             else if (necro_type_uniqueness_attribute_is_one(base, right))  { t = left; }
//             else if (necro_type_uniqueness_attribute_is_zero(base, right)) { t = necro_type_uniqueness_attribute_zero(base); }
//             else if (necro_type_uniqueness_attribute_is_one(base, left))   { t = right; }
//             // Complement Law
//             else if (necro_type_uniqueness_attribute_is_negation(base, right) && necro_type_uniqueness_attribute_equals(base, left, right->con.args->list.item)) { t = necro_type_uniqueness_attribute_zero(base); }
//             else if (necro_type_uniqueness_attribute_is_negation(base, left)  && necro_type_uniqueness_attribute_equals(base, right, left->con.args->list.item)) { t = necro_type_uniqueness_attribute_zero(base); }
//             else return t;
//         }
//         else
//         {
//             assert(false);
//         }
//     }
// }
//
// // Note: Unique = 0, NonUnique = 1
// NecroResult(NecroInstSub) necro_type_uniqueness_attribute_unify_0(NecroPagedArena* arena, NecroSnapshotArena* snapshot_arena, NecroBase* base, NecroType* t, NecroType* var_list)
// {
//     assert(t != NULL);
//     if (var_list == NULL)
//     {
//         NecroType* simplified_t = necro_type_uniqueness_attribute_simplify(snapshot_arena, base, t);
//         assert(simplified_t->type == NECRO_TYPE_CON);
//         assert(simplified_t->con.con_symbol == base->unique_type_attribute || simplified_t->con.con_symbol == base->non_unique_type_attribute);
//         if (necro_type_uniqueness_attribute_is_one(base, simplified_t))
//             return ok(NecroInstSub, NULL); // uniquenessAttributeUnification Error
//         else
//             return ok(NecroInstSub, NULL);
//     }
//     else
//     {
//         assert(var_list->type == NECRO_TYPE_LIST);
//         assert(var_list->list.item->type == NECRO_TYPE_VAR);
//         NecroInstSub* sub0      = necro_create_inst_sub_manual(arena, var_list->list.item->var.var_symbol, base->unique_type_attribute->type, NULL);
//         NecroType*    t0        = necro_type_uniqueness_attribute_simplify(snapshot_arena, base, necro_type_uniqueness_attribute_sub_snapshot(snapshot_arena, base, t, sub0));
//         NecroInstSub* sub1      = necro_create_inst_sub_manual(arena, var_list->list.item->var.var_symbol, base->non_unique_type_attribute->type, NULL);
//         NecroType*    t1        = necro_type_uniqueness_attribute_simplify(snapshot_arena, base, necro_type_uniqueness_attribute_sub_snapshot(snapshot_arena, base, t, sub1));
//         NecroType*    x         = var_list->list.item;
//
//         // Go deeper
//         NecroInstSub* result_sub = necro_try(NecroInstSub, necro_type_uniqueness_attribute_unify_0(arena, snapshot_arena, base, necro_type_uniqueness_attribute_create_and_snapshot(snapshot_arena, base, t0, t1), var_list->list.next));
//         NecroType*    t0_prime   = necro_type_uniqueness_attribute_simplify(snapshot_arena, base, necro_type_uniqueness_attribute_sub_snapshot(snapshot_arena, base, t0, result_sub));
//         NecroType*    t1_prime   = necro_type_uniqueness_attribute_simplify(snapshot_arena, base, necro_type_uniqueness_attribute_sub_snapshot(snapshot_arena, base, t1, result_sub));
//
//         // Look into Redundancy Laws to help simplify the form.
//         // t0 || (x && !t1)
//         NecroType* xsub_type =
//             necro_type_uniqueness_attribute_simplify(snapshot_arena, base,
//                 necro_type_uniqueness_attribute_create_or_snapshot(snapshot_arena, base,
//                     t0_prime,
//                     necro_type_uniqueness_attribute_create_and_snapshot(snapshot_arena, base,
//                         x,
//                         necro_type_uniqueness_attribute_create_not_snapshot(snapshot_arena, base, t1_prime))));
//
//         return ok(NecroInstSub, necro_create_inst_sub_manual(arena, var_list->list.item->var.var_symbol, xsub_type, result_sub));
//     }
// }
//
// NecroResult(NecroType) necro_type_uniqueness_attribute_bind_var(NecroType* var, NecroType* type)
// {
//     var  = necro_type_find(var);
//     type = necro_type_find(type);
//     assert(var->type == NECRO_TYPE_VAR);
//     var->var.bound = necro_type_find(type);
//     return ok(NecroType, type);
// }
//
// // TODO: Alloc everything out of NecroSnapshotArena until the end
// // TODO: Simplify function
// // TODO: Collect variables!
// // TODO: Simple early exits!
// // TODO: Need to handle rigid type variables and all that...
//
// // Use subs to do a more traditional unify
//
// NecroResult(NecroType) necro_type_uniqueness_attribute_sub_var(NecroType* var, NecroType* type, NecroScope* scope)
// {
//     assert(var->type == NECRO_TYPE_VAR);
//     if (type->type == NECRO_TYPE_VAR)
//     {
//         if (var->var.var_symbol == type->var.var_symbol)   return ok(NecroType, var);
//         else if (var->var.is_rigid && type->var.is_rigid)  return necro_type_rigid_type_variable_error(var, type, NULL, NULL, NULL_LOC, NULL_LOC);
//         else if (var->var.is_rigid)                        return necro_type_uniqueness_attribute_bind_var(type, var);
//         else if (type->var.is_rigid)                       return necro_type_uniqueness_attribute_bind_var(var, type);
//         else if (necro_type_is_bound_in_scope(var, scope)) return necro_type_uniqueness_attribute_bind_var(type, var);
//         else                                               return necro_type_uniqueness_attribute_bind_var(var, type);
//     }
//     else
//     {
//         if (var->var.is_rigid)
//             return necro_type_rigid_type_variable_error(var, type, NULL, NULL, NULL_LOC, NULL_LOC);
//         return necro_type_uniqueness_attribute_bind_var(var, type);
//     }
// }
//
// // TODO / NOTE: Remove conjunction?
// // Boolean algebra unification based on successive variable elimination
// NecroResult(NecroType) necro_type_uniqueness_attribute_unify(NecroPagedArena* arena, NecroSnapshotArena* snapshot_arena, NecroBase* base, NecroScope* scope, NecroType* p, NecroType* q)
// {
//     p = necro_type_find(p);
//     q = necro_type_find(q);
//
//     // Most common early exits
//     if (p->type == NECRO_TYPE_VAR &&
//        (q->type == NECRO_TYPE_VAR || necro_type_uniqueness_attribute_is_unique(base, q) || necro_type_uniqueness_attribute_is_non_unique(base, q)))
//     {
//         return necro_type_uniqueness_attribute_sub_var(p, q, scope);
//     }
//     else if (q->type == NECRO_TYPE_VAR &&
//             (p->type == NECRO_TYPE_VAR || necro_type_uniqueness_attribute_is_unique(base, p) || necro_type_uniqueness_attribute_is_non_unique(base, p)))
//     {
//         return necro_type_uniqueness_attribute_sub_var(q, p, scope);
//     }
//     else if ((necro_type_uniqueness_attribute_is_unique(base, p) || necro_type_uniqueness_attribute_is_non_unique(base, p)) &&
//              (necro_type_uniqueness_attribute_is_unique(base, q) || necro_type_uniqueness_attribute_is_non_unique(base, q)))
//     {
//         if (p->con.con_symbol == q->con.con_symbol)
//             return ok(NecroType, p);
//         else
//             return ok(NecroType, p); // return ERROR!
//     }
//
//     // Otherwise we've got something more complicated on our hands!
//
//     // unify t = (p && !q) || (!p && q) = 0
//     NecroArenaSnapshot snapshot   = necro_snapshot_arena_get(snapshot_arena);
//     NecroType*         temp_not_p = necro_type_uniqueness_attribute_create_not_snapshot(snapshot_arena, base, p);
//     NecroType*         temp_not_q = necro_type_uniqueness_attribute_create_not_snapshot(snapshot_arena, base, q);
//     NecroType*         temp_left  = necro_type_uniqueness_attribute_create_and_snapshot(snapshot_arena, base, p, temp_not_q);
//     NecroType*         temp_right = necro_type_uniqueness_attribute_create_and_snapshot(snapshot_arena, base, temp_not_p, q);
//     NecroType*         temp_t     = necro_type_uniqueness_attribute_create_or_snapshot(snapshot_arena, base, temp_left, temp_right);
//     NecroType*         temp_vars  = NULL;
//     // Unify0
//     NecroInstSub*      subs       = necro_try_map(NecroInstSub, NecroType, necro_type_uniqueness_attribute_unify_0(arena, snapshot_arena, base, temp_t, temp_vars));
//     // Apply Subs, deep copy into NecroPagedArena heap
//
//     // Deep copy sub types out, simplify, sub_var
//     NecroType*         p_prime    = necro_type_deep_copy(arena, necro_type_uniqueness_attribute_simplify(snapshot_arena, base, necro_type_uniqueness_attribute_sub_snapshot(snapshot_arena, base, p, subs)));
//
//     necro_snapshot_arena_rewind(snapshot_arena, snapshot);
//     return ok(NecroType, p_prime);
// }


///////////////////////////////////////////////////////
// Uniqueness
///////////////////////////////////////////////////////
NecroType* necro_type_ownership_fresh_var(NecroPagedArena* arena, NecroBase* base, NecroScope* scope)
{
    NecroType* type = necro_type_fresh_var(arena, scope);
    type->kind      = base->ownership_kind->type;
    return type;
}

NecroType* necro_type_uniqueness_list_to_ownership_type(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* uniqueness_list, NecroScope* scope)
{
    NecroConstraintList* constraints = NULL;
    while (uniqueness_list != NULL)
    {
        assert(uniqueness_list->list.item != NULL);
        NecroType* ownership_type = necro_type_find(uniqueness_list->list.item);
        assert(ownership_type != NULL);
        if (ownership_type->type == NECRO_TYPE_CON)
        {
            if (ownership_type->con.con_symbol == base->ownership_share)
            {
                uniqueness_list = uniqueness_list->list.next;
                continue;
            }
            if (ownership_type->con.con_symbol == base->ownership_steal)
                return base->ownership_steal->type;
        }
        constraints = necro_constraint_append_uniqueness_coercion_without_queue_push(arena, con_env, NULL, ownership_type, constraints);
        uniqueness_list = uniqueness_list->list.next;
    }
    if (constraints == NULL)
        return base->ownership_share->type;
    if (constraints->next == NULL)
        return constraints->data->uni.u2;
    NecroType*           uvar     = necro_type_ownership_fresh_var(arena, base, scope);
    NecroConstraintList* curr_con = constraints;
    while (curr_con != NULL)
    {
        curr_con->data->uni.u1 = uvar;
        necro_constraint_dequeue_push_back(&con_env->constraints, curr_con->data);
        curr_con               = curr_con->next;
    }
    uvar->constraints = constraints;
    return uvar;
}

NecroType* necro_type_add_uniqueness_type_to_uniqueness_list(NecroPagedArena* arena, NecroBase* base, NecroType* uniqueness_type, NecroType* uniqueness_list)
{
    uniqueness_type = necro_type_find(uniqueness_type);
    if (uniqueness_type->type == NECRO_TYPE_CON)
    {
        if (uniqueness_type->con.con_symbol == base->ownership_share)
            return uniqueness_list;
        else if (uniqueness_type->con.con_symbol == base->ownership_steal)
            return necro_type_list_create(arena, uniqueness_type, NULL);
    }
    NecroType* curr_uniqueness_list = uniqueness_list;
    while (curr_uniqueness_list != NULL)
    {
        NecroType* uniqueness_type2 = necro_type_find(curr_uniqueness_list->list.item);
        if (uniqueness_type2 == uniqueness_type)
            return uniqueness_list;
        else if (uniqueness_type2 == base->ownership_steal->type)
            return uniqueness_list;
        else if (uniqueness_type2->type == NECRO_TYPE_VAR && uniqueness_type->type == NECRO_TYPE_VAR && uniqueness_type2->var.var_symbol == uniqueness_type->var.var_symbol)
            return uniqueness_list;
        else if (uniqueness_type2->type == NECRO_TYPE_CON && uniqueness_type->type == NECRO_TYPE_CON && uniqueness_type2->con.con_symbol == uniqueness_type->con.con_symbol)
            return uniqueness_list;
        curr_uniqueness_list = curr_uniqueness_list->list.next;
    }
    return necro_type_list_create(arena, uniqueness_type, uniqueness_list);
}

NecroType* necro_type_free_vars_to_ownership_type(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroFreeVars* free_vars, NecroScope* scope)
{
    if (free_vars == NULL)
        return necro_type_ownership_fresh_var(arena, base, scope);
    NecroAstSymbol** data   = &free_vars->data;
    NecroType*       u_list = NULL;
    for (size_t i = 0; i < free_vars->count; ++i)
    {
        assert(data[i] != NULL);
        NecroType* ownership_type = necro_type_find(data[i]->type->ownership);
        u_list                    = necro_type_add_uniqueness_type_to_uniqueness_list(arena, base, ownership_type, u_list);
    }
    return necro_type_uniqueness_list_to_ownership_type(arena, con_env, base, u_list, scope);
}

bool necro_type_is_inhabited(NecroBase* base, const NecroType* type)
{
    type = necro_type_find_const(type);
    const NecroType* kind = necro_type_find_const(type->kind);
    assert(kind != NULL);
    assert(kind->type != NECRO_TYPE_VAR);
    return necro_type_get_fully_applied_fun_type_const(kind) == base->star_kind->type;
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
    assert(uvar_to_bind->type == NECRO_TYPE_VAR);
    // Bind
    uvar_to_bind->var.bound = utype_to_bind_to;
    // Propagate
    NecroConstraintList* constraints1    = uvar_to_bind->constraints;
    NecroConstraintList* constraints2    = utype_to_bind_to->constraints;
    NecroConstraintList* new_constraints = constraints2;
    while (constraints1 != NULL)
    {
        bool new_constraint = true;
        while (constraints2 != NULL)
        {
            if (constraints1->data->type == constraints2->data->type)
            {
                if (constraints1->data->type == NECRO_CONSTRAINT_UNI && (necro_type_find(constraints1->data->uni.u2) == necro_type_find(constraints2->data->uni.u2)))
                {
                    new_constraint = false;
                    break;
                }
            }
            constraints2 = constraints2->next;
        }
        if (new_constraint && constraints1->data->type == NECRO_CONSTRAINT_UNI)
            new_constraints = necro_constraint_append_uniqueness_coercion_and_queue_push_back(arena, con_env, utype_to_bind_to, necro_type_find(constraints1->data->uni.u2), new_constraints);
        constraints2 = utype_to_bind_to->constraints;
        constraints1 = constraints1->next;
    }
    // At least currently, only uvars actually maintain constraints
    if (utype_to_bind_to->type == NECRO_TYPE_VAR)
        utype_to_bind_to->constraints = new_constraints;
}

void necro_type_ownership_bind_uvar_to_with_queue_push_front(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroType* uvar_to_bind, NecroType* utype_to_bind_to)
{
    assert(uvar_to_bind->type == NECRO_TYPE_VAR);
    // Bind
    uvar_to_bind->var.bound = utype_to_bind_to;
    // Propagate
    NecroConstraintList* constraints1    = uvar_to_bind->constraints;
    NecroConstraintList* constraints2    = utype_to_bind_to->constraints;
    NecroConstraintList* new_constraints = constraints2;
    while (constraints1 != NULL)
    {
        bool new_constraint = true;
        while (constraints2 != NULL)
        {
            if (constraints1->data->type == constraints2->data->type)
            {
                if (constraints1->data->type == NECRO_CONSTRAINT_UNI && (necro_type_find(constraints1->data->uni.u2) == necro_type_find(constraints2->data->uni.u2)))
                {
                    new_constraint = false;
                    break;
                }
            }
            constraints2 = constraints2->next;
        }
        if (new_constraint && constraints1->data->type == NECRO_CONSTRAINT_UNI)
            new_constraints = necro_constraint_append_uniqueness_coercion_and_queue_push_front(arena, con_env, utype_to_bind_to, necro_type_find(constraints1->data->uni.u2), new_constraints);
        constraints2 = utype_to_bind_to->constraints;
        constraints1 = constraints1->next;
    }
    // At least currently, only uvars actually maintain constraints
    if (utype_to_bind_to->type == NECRO_TYPE_VAR)
        utype_to_bind_to->constraints = new_constraints;
}

// Returns NULL
NecroResult(NecroType) necro_type_ownership_unify(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroType* ownership1, NecroType* ownership2, NecroScope* scope)
{
    UNUSED(con_env);
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
            else if (is_type1_bound_in_scope || ownership2->constraints == NULL) necro_type_ownership_bind_uvar_to(arena, con_env, ownership2, ownership1);
            else                                                                 necro_type_ownership_bind_uvar_to(arena, con_env, ownership1, ownership2);
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

// Returns inferred ownership
NecroResult(NecroType) necro_type_ownership_infer_from_type(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope)
{
    type = necro_type_find(type);
    if (type->ownership != NULL)
        return ok(NecroType, type->ownership);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        else
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_FUN:
    {
        necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type->fun.type1, scope));
        necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type->fun.type2, scope));
        type->ownership = necro_type_free_vars_to_ownership_type(arena, con_env, base, type->fun.free_vars, scope);
        return ok(NecroType, type->ownership);
    }
    case NECRO_TYPE_CON:
    {
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        else
            type->ownership = necro_type_ownership_fresh_var(arena, base, scope);
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            NecroType* arg_ownership = necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, args->list.item, scope));
            if (necro_type_is_inhabited(base, args->list.item))
                necro_try(NecroType, necro_type_ownership_unify(arena, con_env, type->ownership, arg_ownership, scope));
            args = args->list.next;
        }
        return ok(NecroType, type->ownership);
    }
    case NECRO_TYPE_APP:
    {
        NecroType* ownership1 = necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type->app.type1, scope));
        NecroType* ownership2 = necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type->app.type2, scope));
        if (!necro_type_is_inhabited(base, type))
        {
            type->ownership = base->ownership_share->type;
        }
        else
        {
            type->ownership = ownership1;
            if (necro_type_is_inhabited(base, type->app.type2))
                necro_try(NecroType, necro_type_ownership_unify(arena, con_env, type->ownership, ownership2, scope));
        }
        return ok(NecroType, type->ownership);
    }
    case NECRO_TYPE_FOR:
        type->ownership = necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type->for_all.type, scope));
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

// Returns inferred ownership
NecroResult(NecroType) necro_type_ownership_infer_from_sig_go(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope, NecroType* uniqueness_list)
{
    scope = NULL;
    type = necro_type_find(type);
    assert(type->ownership != NULL);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_FUN:
    {
        NecroType* domain_ownership    = necro_try(NecroType, necro_type_ownership_infer_from_sig_go(arena, con_env, base, type->fun.type1, scope, NULL));
        NecroType* new_uniqueness_list = necro_type_add_uniqueness_type_to_uniqueness_list(arena, base, domain_ownership, uniqueness_list);
        necro_try(NecroType, necro_type_ownership_infer_from_sig_go(arena, con_env, base, type->fun.type2, scope, new_uniqueness_list));
        if (type->ownership->type == NECRO_TYPE_CON && type->ownership->con.con_symbol == base->ownership_steal)
        {
            type->ownership = base->ownership_steal->type;
        }
        else if (type->ownership->type == NECRO_TYPE_CON && type->ownership->con.con_symbol == base->ownership_share)
        {
            type->ownership = necro_type_uniqueness_list_to_ownership_type(arena, con_env, base, uniqueness_list, scope);
        }
        else if (type->ownership->type == NECRO_TYPE_VAR)
        {
            NecroType* uniqueness = necro_type_uniqueness_list_to_ownership_type(arena, con_env, base, uniqueness_list, scope);
            if (uniqueness->type != NECRO_TYPE_CON || uniqueness->con.con_symbol != base->ownership_share)
                type->ownership = uniqueness;
            // uniqueness_list = necro_type_add_uniqueness_type_to_uniqueness_list(arena, base, type->ownership, uniqueness_list);
        }
        else
        {
            assert(false);
        }
        return ok(NecroType, type->ownership);
    }
    case NECRO_TYPE_CON:
    {
        if (!necro_type_is_inhabited(base, type))
            type->ownership = base->ownership_share->type;
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            NecroType* arg_ownership = necro_try(NecroType, necro_type_ownership_infer_from_sig_go(arena, con_env, base, args->list.item, scope, NULL));
            if (necro_type_is_inhabited(base, args->list.item))
                necro_try(NecroType, necro_type_ownership_unify(arena, con_env, type->ownership, arg_ownership, scope));
            args = args->list.next;
        }
        return ok(NecroType, type->ownership);
    }
    case NECRO_TYPE_APP:
    {
        NecroType* ownership1 = necro_try(NecroType, necro_type_ownership_infer_from_sig_go(arena, con_env, base, type->app.type1, scope, NULL));
        NecroType* ownership2 = necro_try(NecroType, necro_type_ownership_infer_from_sig_go(arena, con_env, base, type->app.type2, scope, NULL));
        if (!necro_type_is_inhabited(base, type))
        {
            type->ownership = base->ownership_share->type;
        }
        else
        {
            type->ownership = ownership1;
            if (necro_type_is_inhabited(base, type->app.type2))
                necro_try(NecroType, necro_type_ownership_unify(arena, con_env, type->ownership, ownership2, scope));
        }
        return ok(NecroType, type->ownership);
    }
    case NECRO_TYPE_FOR:
        type->ownership = necro_try(NecroType, necro_type_ownership_infer_from_sig_go(arena, con_env, base, type->for_all.type, scope, NULL));
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        type->ownership = base->ownership_share->type;
        return ok(NecroType, type->ownership);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_type_ownership_infer_from_sig(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type, NecroScope* scope)
{
    return necro_type_ownership_infer_from_sig_go(arena, con_env, base, type, scope, NULL);
}

// TODO: Shared implementation...How?
// TODO: Check Higher order function uniqueness inference in signatures, and that it matches inference from terms.
NecroResult(NecroType) necro_type_infer_and_unify_ownership_for_two_types(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    NecroType* ownership1 = necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type1, scope));
    NecroType* ownership2 = necro_try(NecroType, necro_type_ownership_infer_from_type(arena, con_env, base, type2, scope));
    return necro_type_ownership_unify(arena, con_env, ownership1, ownership2, scope);
}

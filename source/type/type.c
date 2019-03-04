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

#define NECRO_TYPE_DEBUG 0
#if NECRO_TYPE_DEBUG
#define TRACE_TYPE(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE(...)
#endif

// TODO: "Normalize" type variables in printing so that you never print the same variable name twice with two different meanings!

///////////////////////////////////////////////////////
// Create / Destroy
///////////////////////////////////////////////////////
NecroInfer necro_infer_empty()
{
    return (NecroInfer)
    {
        .scoped_symtable = NULL,
        .snapshot_arena  = necro_snapshot_arena_empty(),
        .arena           = NULL,
        .intern          = NULL,
        .base            = NULL,
    };
}

NecroInfer necro_infer_create(NecroPagedArena* arena, NecroIntern* intern, struct NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena)
{
    NecroInfer infer = (NecroInfer)
    {
        .intern          = intern,
        .arena           = arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
    };
    return infer;
}

void necro_infer_destroy(NecroInfer* infer)
{
    if (infer == NULL)
        return;
    necro_snapshot_arena_destroy(&infer->snapshot_arena);
    *infer = necro_infer_empty();
}

//=====================================================
// Utility
//=====================================================
NecroResult(NecroType) necro_type_unify_order(NecroType* type1, NecroType* type2);

NecroType* necro_type_alloc(NecroPagedArena* arena)
{
    NecroType* type    = necro_paged_arena_alloc(arena, sizeof(NecroType));
    type->pre_supplied = false;
    type->kind         = NULL;
    return type;
}

NecroType* necro_type_var_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol)
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
        .scope      = NULL,
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
    };
    return type;
}

NecroType* necro_type_declare(NecroPagedArena* arena, NecroAstSymbol* con_symbol)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con_symbol = con_symbol,
        .args       = NULL,
    };
    type->pre_supplied = true;
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

NecroType* necro_type_for_all_create(NecroPagedArena* arena, NecroAstSymbol* var_symbol, NecroTypeClassContext* context, NecroType* type)
{
    NecroType* for_all  = necro_type_alloc(arena);
    for_all->type       = NECRO_TYPE_FOR;
    for_all->for_all    = (NecroTypeForAll)
    {
        .var_symbol = var_symbol,
        .context    = context,
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
        new_type               = necro_type_var_create(arena, type->var.var_symbol);
        new_type->var.is_rigid = type->var.is_rigid;
        new_type->var.arity    = type->var.arity;
        new_type->var.order    = type->var.order;
        break;
    }
    case NECRO_TYPE_APP:  new_type = necro_type_app_create(arena, necro_type_deep_copy_go(arena, type->app.type1), necro_type_deep_copy_go(arena, type->app.type2)); break;
    case NECRO_TYPE_FUN:  new_type = necro_type_fn_create(arena, necro_type_deep_copy_go(arena, type->fun.type1), necro_type_deep_copy_go(arena, type->fun.type2)); break;
    case NECRO_TYPE_CON:  new_type = necro_type_con_create(arena, type->con.con_symbol, necro_type_deep_copy_go(arena, type->con.args)); break;
    case NECRO_TYPE_FOR:  new_type = necro_type_for_all_create(arena, type->for_all.var_symbol, necro_context_deep_copy(arena, type->for_all.context), necro_type_deep_copy_go(arena, type->for_all.type)); break;
    case NECRO_TYPE_LIST: new_type = necro_type_list_create(arena, necro_type_deep_copy_go(arena, type->list.item), necro_type_deep_copy_go(arena, type->list.next)); break;
    case NECRO_TYPE_NAT:  new_type = necro_type_nat_create(arena, type->nat.value); break;
    case NECRO_TYPE_SYM:  new_type = necro_type_sym_create(arena, type->sym.value); break;
    default:              assert(false);
    }
    new_type->kind = type->kind;
    return new_type;
}

NecroType* necro_type_deep_copy(NecroPagedArena* arena, NecroType* type)
{
    NecroType* new_type = necro_type_deep_copy_go(arena, type);
    // TODO: Kind infer deep copies!!!
    // unwrap(NecroType, necro_kind_infer_gen_unify_with_star(arena, base, new_type));
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

NecroType* necro_type_fresh_var(NecroPagedArena* arena)
{
    NecroAstSymbol* ast_symbol = necro_ast_symbol_create(arena, NULL, NULL, NULL, NULL);
    NecroType*      type_var   = necro_type_var_create(arena, ast_symbol);
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

NecroType* necro_type_get_fully_applied_fun_type(NecroType* type)
{
    while (type->type == NECRO_TYPE_FUN)
    {
        type = type->fun.type2;
    }
    return type;
}

bool necro_type_is_bounded_polymorphic(const NecroType* type)
{
    while (type->type == NECRO_TYPE_FOR)
    {
        if (type->for_all.context != NULL)
            return true;
        type = type->for_all.type;
    }
    return false;
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
    type->var.bound = necro_type_con_create(arena, type_symbol, NULL);
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

NecroResult(NecroType) necro_propogate_type_classes(NecroPagedArena* arena, NecroBase* base, NecroTypeClassContext* classes, NecroType* type, NecroScope* scope)
{
    if (classes == NULL)
        return ok(NecroType, NULL);
    if (type == NULL)
        return ok(NecroType, NULL);
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (type->var.is_rigid)
        {
            // If it's a rigid variable, make sure it has all of the necessary classes in its context already
            while (classes != NULL)
            {
                if (!necro_context_and_super_classes_contain_class(type->var.context, classes))
                {
                    return necro_type_not_an_instance_of_error_partial(classes->class_symbol, type);
                }
                classes = classes->next;
            }
        }
        else
        {
            // TODO: Optimally would want to unify kinds here, but we need a better kinds story to make sure we don't break things
            // NecroTypeClassContext* curr = classes;
            // while (curr != NULL)
            // {
            //     necro_unify_kinds(infer, type, &type->kind, necro_symtable_get(infer->symtable, curr->type_class_name.id)->type->kind, macro_type, error_preamble);
            //     curr = curr->next;
            // }
            // type->var.context = necro_union_contexts(infer, type->var.context, classes);
            type->var.context = necro_union_contexts_to_same_var(arena, type->var.context, classes, type->var.var_symbol);
        }
        return ok(NecroType, NULL);

    case NECRO_TYPE_CON:
        while (classes != NULL)
        {
            NecroTypeClassInstance* instance = necro_get_type_class_instance(type->con.con_symbol, classes->class_symbol);
            if (instance == NULL)
            {
                return necro_type_not_an_instance_of_error_partial(classes->class_symbol, type);
            }
            // Would this method require a proper scope!?!?!
            NecroType* instance_data_inst = necro_try(NecroType, necro_type_instantiate(arena, base, instance->data_type, instance->ast->scope));
            necro_try(NecroType, necro_type_unify(arena, base, instance_data_inst, type, scope));
            // Propogating type classes
            NecroType* current_arg = type->con.args;
            while (current_arg != NULL)
            {
                necro_try(NecroType, necro_propogate_type_classes(arena, base, instance->context, current_arg->list.item, instance->ast->scope));
                current_arg = current_arg->list.next;
            }
            classes = classes->next;
        }
        return ok(NecroType, NULL);

    case NECRO_TYPE_FUN:
        // TODO: Type classes for functions!!!
        necro_try(NecroType, necro_propogate_type_classes(arena, base, classes, type->fun.type1, scope));
        return necro_propogate_type_classes(arena, base, classes, type->fun.type2, scope);

    case NECRO_TYPE_APP:
        // necro_propogate_type_classes(infer, classes, type->app.type1, macro_type, error_preamble);
        // necro_propogate_type_classes(infer, classes, type->app.type2, macro_type, error_preamble);
        assert(false && "Compiler bug: TypeApp not implemented in necro_propogate_type_classes! (i.e. constraints of the form: Num (f a), or (c a), are not currently supported)");
        return ok(NecroType, NULL);

    case NECRO_TYPE_NAT:
        return ok(NecroType, NULL);
    case NECRO_TYPE_SYM:
        return ok(NecroType, NULL);

    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

// TODO: rename -> necro_bind_type_var
NecroResult(NecroType) necro_instantiate_type_var(NecroPagedArena* arena, NecroBase* base, NecroType* type_var_type, NecroType* type, NecroScope* scope)
{
    assert(type_var_type->type == NECRO_TYPE_VAR);
    type_var_type          = necro_type_find(type_var_type);
    NecroTypeVar* type_var = &type_var_type->var;
    necro_try(NecroType, necro_type_unify_order(type_var_type, type));
    necro_try(NecroType, necro_propogate_type_classes(arena, base, type_var->context, type, scope));
    type_var->bound = necro_type_find(type);
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_unify_var(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
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
        if (type1->var.var_symbol == type2->var.var_symbol)  return ok(NecroType, NULL);
        else if (type1->var.is_rigid && type2->var.is_rigid) return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol, type2));
        if (type1->var.is_rigid)                             return necro_instantiate_type_var(arena, base, type2, type1, scope);
        else if (type2->var.is_rigid)                        return necro_instantiate_type_var(arena, base, type1, type2, scope);
        else if (necro_type_is_bound_in_scope(type1, scope)) return necro_instantiate_type_var(arena, base, type2, type1, scope);
        else                                                 return necro_instantiate_type_var(arena, base, type1, type2, scope);
    case NECRO_TYPE_FUN:
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        if (type1->var.is_rigid)
            return necro_type_rigid_type_variable_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol , type2));
        necro_try(NecroType, necro_instantiate_type_var(arena, base, type1, type2, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_app(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
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
        necro_try(NecroType, necro_instantiate_type_var(arena, base, type2, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_unify(arena, base, type1->app.type2, type2->app.type2, scope));
        necro_try(NecroType, necro_type_unify(arena, base, type1->app.type1, type2->app.type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_type_curry_con(arena, base, type2);
        assert(uncurried_con != NULL);
        // if (uncurried_con == NULL)
        //     return necro_type_mismatched_arity_error(type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
        // else
        return necro_type_unify(arena, base, type1, uncurried_con, scope);
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

NecroResult(NecroType) necro_unify_fun(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
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
        necro_try(NecroType, necro_instantiate_type_var(arena, base, type2, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
        necro_try(NecroType, necro_type_unify(arena, base, type1->fun.type2, type2->fun.type2, scope));
        necro_try(NecroType, necro_type_unify(arena, base, type1->fun.type1, type2->fun.type1, scope));
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

NecroResult(NecroType) necro_unify_con(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
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
        necro_try(NecroType, necro_instantiate_type_var(arena, base, type2, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
        if (type1->con.con_symbol != type2->con.con_symbol)
        {
            return necro_type_mismatched_type_error_partial(type1, type2);
        }
        else
        {
            // NecroType* original_type1 = type1;
            // NecroType* original_type2 = type2;
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
                necro_try(NecroType, necro_type_unify(arena, base, type1->list.item, type2->list.item, scope));
                type1 = type1->list.next;
                type2 = type2->list.next;
            }
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
        return necro_type_unify(arena, base, uncurried_con, type2, scope);
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

NecroResult(NecroType) necro_unify_nat(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
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
        necro_try(NecroType, necro_instantiate_type_var(arena, base, type2, type1, scope));
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

NecroResult(NecroType) necro_unify_sym(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
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
        necro_try(NecroType, necro_instantiate_type_var(arena, base, type2, type1, scope));
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
NecroResult(NecroType) necro_type_unify(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    type1 = necro_type_find(type1);
    type2 = necro_type_find(type2);
    if (type1 == type2)
        return ok(NecroType, NULL);
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  return necro_unify_var(arena, base, type1, type2, scope);
    case NECRO_TYPE_APP:  return necro_unify_app(arena, base, type1, type2, scope);
    case NECRO_TYPE_FUN:  return necro_unify_fun(arena, base, type1, type2, scope);
    case NECRO_TYPE_CON:  return necro_unify_con(arena, base, type1, type2, scope);
    case NECRO_TYPE_NAT:  return necro_unify_nat(arena, base, type1, type2, scope);
    case NECRO_TYPE_SYM:  return necro_unify_sym(arena, base, type1, type2, scope);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:
        while (type1->type == NECRO_TYPE_FOR)
            type1 = type1->for_all.type;
        necro_try(NecroType, necro_type_unify(arena, base, type1, type2, scope));
        return ok(NecroType, NULL);
    default:
        necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_type_unify_with_info(NecroPagedArena* arena, struct NecroBase* base, NecroType* type1, NecroType* type2, struct NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_type_unify_with_full_info(arena, base, type1, type2, scope, source_loc, end_loc, type1, type2);
}

NecroResult(NecroType) necro_type_unify_with_full_info(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroType* macro_type1, NecroType* macro_type2)
{
    NecroResult(NecroType) result = necro_type_unify(arena, base, type1, type2, scope);
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
NecroInstSub* necro_create_inst_sub(NecroPagedArena* arena, NecroAstSymbol* var_to_replace, NecroTypeClassContext* context, NecroInstSub* next)
{
    NecroType* type_to_replace            = necro_type_find(var_to_replace->type);
    NecroType* type_var                   = necro_type_fresh_var(arena);
    type_var->var.var_symbol->name        = var_to_replace->name;
    type_var->var.var_symbol->source_name = var_to_replace->source_name;
    type_var->kind                        = type_to_replace->kind;
    type_var->var.order                   = type_to_replace->var.order;
    if (type_var->var.order == NECRO_TYPE_HIGHER_ORDER)
        type_var->var.order = NECRO_TYPE_POLY_ORDER;
    NecroInstSub* sub = necro_paged_arena_alloc(arena, sizeof(NecroInstSub));
    *sub              = (NecroInstSub)
    {
        .var_to_replace = var_to_replace,
        .new_name       = type_var,
        .next           = next,
    };
    sub->new_name->var.context = context;
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

NecroType* necro_type_replace_with_subs_deep_copy_go(NecroPagedArena* arena, NecroType* type, NecroInstSub* subs)
{
    if (type == NULL)
        return NULL;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        while (subs != NULL)
        {
            if (type->for_all.var_symbol == subs->var_to_replace ||
               (type->for_all.var_symbol->name != NULL && type->for_all.var_symbol->name == subs->var_to_replace->name))
            {
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
                return necro_type_replace_with_subs_deep_copy_go(arena, type->for_all.type, subs);
            }
            curr_sub = curr_sub->next;
        }
        return necro_type_for_all_create(arena, type->for_all.var_symbol, type->for_all.context, necro_type_replace_with_subs_deep_copy_go(arena, type->for_all.type, subs));
    }
    case NECRO_TYPE_APP:  return necro_type_app_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, type->app.type1, subs), necro_type_replace_with_subs_deep_copy_go(arena, type->app.type2, subs));
    case NECRO_TYPE_FUN:  return necro_type_fn_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, type->fun.type1, subs), necro_type_replace_with_subs_deep_copy_go(arena, type->fun.type2, subs));
    case NECRO_TYPE_CON:  return necro_type_con_create(arena, type->con.con_symbol, necro_type_replace_with_subs_deep_copy_go(arena, type->con.args, subs));
    case NECRO_TYPE_LIST: return necro_type_list_create(arena, necro_type_replace_with_subs_deep_copy_go(arena, type->list.item, subs), necro_type_replace_with_subs_deep_copy_go(arena, type->list.next, subs));
    case NECRO_TYPE_NAT:  return necro_type_nat_create(arena, type->nat.value);
    case NECRO_TYPE_SYM:  return necro_type_sym_create(arena, type->sym.value);

    default:              assert(false); return NULL;
    }
}

NecroResult(NecroType) necro_type_replace_with_subs_deep_copy(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroInstSub* subs)
{
    NecroType* new_type = necro_type_replace_with_subs_deep_copy_go(arena, type, subs);
    if (new_type == NULL)
        return ok(NecroType, NULL);
    // necro_try_map(void, NecroType, necro_kind_infer_gen_unify_with_star(arena, base, new_type, NULL, NULL_LOC, NULL_LOC));
    necro_try(NecroType, necro_kind_infer(arena, base, new_type, NULL_LOC, NULL_LOC));
    return ok(NecroType, new_type);
}

// TODO: Do we need the scope argument?
NecroType* necro_type_replace_with_subs_go(NecroPagedArena* arena, NecroType* type, NecroInstSub* subs)
{
    if (type == NULL || subs == NULL)
        return type;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        while (subs != NULL)
        {
            if (type->for_all.var_symbol == subs->var_to_replace ||
               (type->for_all.var_symbol->name != NULL && type->for_all.var_symbol->name == subs->var_to_replace->name))
            {
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
            return necro_type_for_all_create(arena, type->for_all.var_symbol, type->for_all.context, next_type);
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
        NecroType* type1 = necro_type_replace_with_subs_go(arena, type->fun.type1, subs);
        NecroType* type2 = necro_type_replace_with_subs_go(arena, type->fun.type2, subs);
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

NecroResult(NecroType) necro_type_instantiate(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroScope* scope)
{
    UNUSED(scope);
    assert(type != NULL);
    type = necro_type_find(type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs         = necro_create_inst_sub(arena, current_type->for_all.var_symbol, current_type->for_all.context, subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_try(NecroType, necro_type_replace_with_subs_deep_copy(arena, base, current_type, subs));
    return ok(NecroType, result);
}

NecroResult(NecroType) necro_type_instantiate_with_context(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroScope* scope, NecroTypeClassContext** context)
{
    UNUSED(scope);
    assert(type != NULL);
    type = necro_type_find(type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    *context                   = NULL;
    NecroTypeClassContext* curr_context = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs = necro_create_inst_sub(arena, current_type->for_all.var_symbol, current_type->for_all.context, subs);
        NecroTypeClassContext* for_all_context = current_type->for_all.context;
        while (for_all_context != NULL)
        {
            if (*context == NULL)
            {
                curr_context = necro_create_type_class_context(arena, for_all_context->type_class, for_all_context->class_symbol, subs->new_name->var.var_symbol, NULL);
                *context     = curr_context;
            }
            else
            {
                curr_context->next = necro_create_type_class_context(arena, for_all_context->type_class, for_all_context->class_symbol, subs->new_name->var.var_symbol, NULL);
                curr_context       = curr_context->next;
            }
            for_all_context = for_all_context->next;
        }
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_try(NecroType, necro_type_replace_with_subs_deep_copy(arena, base, current_type, subs));
    assert(result != NULL);
    return ok(NecroType, result);
}

NecroResult(NecroType) necro_type_instantiate_with_subs(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroScope* scope, NecroInstSub** subs)
{
    UNUSED(scope);
    assert(type != NULL);
    type = necro_type_find(type);
    NecroType*    current_type = type;
    *subs                      = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        *subs        = necro_create_inst_sub(arena, current_type->for_all.var_symbol, current_type->for_all.context, *subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_try(NecroType, necro_type_replace_with_subs_deep_copy(arena, base, current_type, *subs));
    return ok(NecroType, result);
}

//=====================================================
// Gen
//=====================================================
typedef struct NecroGenSub
{
    struct NecroGenSub* next;
    NecroAstSymbol*     var_to_replace;
    NecroType*          type_var;
    NecroType*          for_all;
} NecroGenSub;

typedef struct
{
    NecroType*   type;
    NecroGenSub* subs;
    NecroGenSub* sub_tail;
} NecroGenResult;

// TODO: necro_normalize_type_var_names
NecroGenResult necro_gen_go(NecroPagedArena* arena, NecroType* type, NecroGenResult prev_result, NecroScope* scope)
{
    if (type == NULL)
    {
        prev_result.type = NULL;
        return prev_result;
    }

    NecroType* top = necro_type_find(type);
    if (top->type == NECRO_TYPE_VAR && top->var.is_rigid)
    {
        prev_result.type = top;
        return prev_result;
    }

    if (necro_type_is_bound_in_scope(type, scope))
    {
        prev_result.type = type;
        return prev_result;
    }

    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
        {
            prev_result.type = type;
            return prev_result;
        }
        NecroType* bound_type = type->var.bound;
        if (bound_type != NULL)
        {
            return necro_gen_go(arena, bound_type, prev_result, scope);
        }
        else
        {
            NecroGenSub* subs = prev_result.subs;
            while (subs != NULL)
            {
                if (subs->var_to_replace == type->var.var_symbol)
                {
                    prev_result.type    = subs->type_var;
                    return prev_result;
                }
                subs = subs->next;
            }

            // NOTE:  Test, Generalize type variable in place?
            NecroGenSub* sub               = necro_paged_arena_alloc(arena, sizeof(NecroGenSub));
            NecroType*   type_var          = type;
            type_var->var.is_rigid         = true;
            type_var->var.var_symbol->type = type_var; // TODO/HACK: Why is this necessary!?!?!?
            // NOTE: We can generalize to poly since the constraints have done their jobs.
            // if (type_var->var.order == NECRO_TYPE_HIGHER_ORDER)
            //     type_var->var.order = NECRO_TYPE_POLY_ORDER;

            NecroType* for_all = necro_type_for_all_create(arena, type_var->var.var_symbol, type->var.context, NULL);
            *sub               = (NecroGenSub)
            {
                .next           = NULL,
                .var_to_replace = type->var.var_symbol,
                .type_var       = type_var,
                .for_all        = for_all,
            };

            prev_result.type = type_var;
            if (prev_result.sub_tail == NULL)
            {
                prev_result.subs     = sub;
                prev_result.sub_tail = sub;
            }
            else
            {
                prev_result.sub_tail->next = sub;
                prev_result.sub_tail       = sub;
            }
            return prev_result;
        }
    }
    case NECRO_TYPE_APP:
    {
        NecroGenResult result1 = necro_gen_go(arena, type->app.type1, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(arena, type->app.type2, result1, scope);
        result2.type           = necro_type_app_create(arena, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_FUN:
    {
        NecroGenResult result1 = necro_gen_go(arena, type->fun.type1, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(arena, type->fun.type2, result1, scope);
        result2.type           = necro_type_fn_create(arena, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_LIST:
    {
        NecroGenResult result1 = necro_gen_go(arena, type->list.item, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(arena, type->list.next, result1, scope);
        result2.type           = necro_type_list_create(arena, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_CON:
    {
        NecroGenResult result = necro_gen_go(arena, type->con.args, prev_result, scope);
        result.type           = necro_type_con_create(arena, type->con.con_symbol, result.type);
        return result;
    }
    case NECRO_TYPE_NAT:
    {
        prev_result.type = type;
        return prev_result;
    }
    case NECRO_TYPE_SYM:
    {
        prev_result.type = type;
        return prev_result;
    }
    case NECRO_TYPE_FOR: assert(false); return (NecroGenResult) { NULL, NULL, NULL };
    default:             assert(false); return (NecroGenResult) { NULL, NULL, NULL };
    }
}

NecroResult(NecroType) necro_type_generalize(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, struct NecroScope* scope)
{
    assert(type != NULL);
    assert(type->type != NECRO_TYPE_FOR);
    // Switch positions!?!?!?
    necro_try(NecroType, necro_kind_infer(arena, base, type, NULL_LOC, NULL_LOC));
    NecroGenResult result = necro_gen_go(arena, type, (NecroGenResult) { NULL, NULL, NULL }, scope);
    if (result.subs != NULL)
    {
        NecroGenSub* current_sub = result.subs;
        NecroType*   head        = result.subs->for_all;
        NecroType*   tail        = head;
        assert(head != NULL);
        assert(tail != NULL);
        assert(result.type != NULL);
        while (true)
        {
            assert(current_sub->for_all != NULL);
            assert(current_sub->for_all->type == NECRO_TYPE_FOR);
            current_sub = current_sub->next;
            if (current_sub != NULL)
            {
                assert(tail->for_all.type == NULL);
                tail->for_all.type = current_sub->for_all;
                tail               = tail->for_all.type;
            }
            else
            {
                assert(tail->for_all.type == NULL);
                tail->for_all.type = result.type;
                necro_try(NecroType, necro_kind_infer(arena, base, head, NULL_LOC, NULL_LOC));
                return ok_NecroType(head);
            }
        }
    }
    else
    {
        necro_try(NecroType, necro_kind_infer(arena, base, result.type, NULL_LOC, NULL_LOC));
        return ok_NecroType(result.type);
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
    return type->type == NECRO_TYPE_VAR || type->type == NECRO_TYPE_NAT || type->type == NECRO_TYPE_SYM || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0);
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
    NecroType* type = var_symbol->type;
    if (type->var.order != NECRO_TYPE_ZERO_ORDER)
        fprintf(stream, "^");
    if (var_symbol->source_name != NULL && var_symbol->source_name->str != NULL)
    {
        fprintf(stream, "%s", var_symbol->source_name->str);
    }
    else
    {
        fprintf(stream, "t%p", var_symbol);
    }
}

void necro_type_fprint(FILE* stream, const NecroType* type)
{
    if (type == NULL)
        return;
    type = necro_type_find_const(type);
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
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            fprintf(stream, "(");
        necro_type_fprint(stream, type->fun.type1);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            fprintf(stream, ")");
        fprintf(stream, " -> ");
        necro_type_fprint(stream, type->fun.type2);
        break;

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
            if (type->for_all.context != NULL)
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
                NecroTypeClassContext* context = type->for_all.context;
                while (context != NULL)
                {
                    if (count > 0)
                        fprintf(stream, ", ");
                    fprintf(stream, "%s ", context->class_symbol->source_name->str);
                    if (type->for_all.var_symbol->source_name != NULL)
                        fprintf(stream, "%s", type->for_all.var_symbol->source_name->str);
                    else
                        fprintf(stream, "t%p", type->for_all.var_symbol);
                    context = context->next;
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
        NecroTypeClassContext* context = type->for_all.context;
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
        NecroTypeClassContext* context1 = type1->for_all.context;
        NecroTypeClassContext* context2 = type2->for_all.context;
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

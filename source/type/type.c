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

NecroInfer necro_infer_create(NecroPagedArena* arena, NecroIntern* intern, struct NecroScopedSymTable* scoped_symtable, struct NecroBase* base)
{

    NecroInfer infer = (NecroInfer)
    {
        .intern          = intern,
        .arena           = arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
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
NecroType* necro_type_alloc(NecroPagedArena* arena)
{
    NecroType* type    = necro_paged_arena_alloc(arena, sizeof(NecroType));
    type->pre_supplied = false;
    type->source_loc   = (NecroSourceLoc) { 0, 0, 0 };
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
        .var_symbol        = var_symbol,
        .arity             = -1,
        .is_rigid          = false,
        .is_type_class_var = false,
        .context           = NULL,
        .bound             = NULL,
        .scope             = NULL,
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

NecroType* necro_type_declare(NecroPagedArena* arena, NecroAstSymbol* con_symbol, size_t arity)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con_symbol = con_symbol,
        .args       = NULL,
        .arity      = arity,
        .is_class   = false,
    };
    type->pre_supplied = true;
    type->con.arity    = arity;
    return type;
}

NecroType* necro_type_con_create(NecroPagedArena* arena, NecroAstSymbol* con_symbol, NecroType* args, size_t arity)
{
    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con_symbol = con_symbol,
        .args       = args,
        .arity      = arity,
        .is_class   = false,
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

NecroType* necro_type_duplicate(NecroPagedArena* arena, NecroType* type)
{
    if (type == NULL)
        return NULL;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return necro_type_var_create(arena, type->var.var_symbol);
    case NECRO_TYPE_APP:  return necro_type_app_create(arena, necro_type_duplicate(arena, type->app.type1), necro_type_duplicate(arena, type->app.type2));
    case NECRO_TYPE_FUN:  return necro_type_fn_create(arena, necro_type_duplicate(arena, type->fun.type1), necro_type_duplicate(arena, type->fun.type2));
    case NECRO_TYPE_CON:  return necro_type_con_create(arena, type->con.con_symbol, necro_type_duplicate(arena, type->con.args), type->con.arity);
    case NECRO_TYPE_FOR:  return necro_type_for_all_create(arena, type->for_all.var_symbol, type->for_all.context, necro_type_duplicate(arena, type->for_all.type));
    case NECRO_TYPE_LIST: return necro_type_list_create(arena, necro_type_duplicate(arena, type->list.item), necro_type_duplicate(arena, type->list.next));
    default:              assert(false); return NULL;
    }
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

NecroType* necro_type_curry_con(NecroPagedArena* arena, NecroType* con)
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
    NecroType* new_con = necro_type_con_create(arena, con->con.con_symbol, head, con->con.arity);
    NecroType* ap      = necro_type_app_create(arena, new_con, curr->list.item);
    return ap;
}

NecroType* necro_type_fresh_var(NecroPagedArena* arena)
{
    NecroAstSymbol* ast_symbol = necro_ast_symbol_create(arena, NULL, NULL, NULL, NULL);
    NecroType*      type_var   = necro_type_var_create(arena, ast_symbol);
    ast_symbol->type           = type_var;
    return type_var;
}

//=====================================================
// Find
//=====================================================
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
            return necro_type_occurs_error(type_var_symbol, type_var_symbol->type, NULL_LOC, NULL_LOC, NULL, type, NULL_LOC, NULL_LOC);
        else
            return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->app.type1));
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->app.type2));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->fun.type1));
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->fun.type2));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
        return necro_type_occurs(type_var_symbol, type->con.args);
    case NECRO_TYPE_LIST:
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->list.item));
        necro_try(NecroType, necro_type_occurs(type_var_symbol, type->list.next));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
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
                    // necro_infer_error(infer, error_preamble, macro_type, "No instance for \'%s %s\'", classes->type_class_name.symbol->str, necro_id_as_character_string(infer->intern, type->var.var));
                    return necro_type_not_an_instance_of_error(NULL, type, NULL_LOC, NULL_LOC, NULL, classes->class_symbol->type, NULL_LOC, NULL_LOC);
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
                return necro_type_not_an_instance_of_error(NULL, type, NULL_LOC, NULL_LOC, NULL, classes->class_symbol->type, NULL_LOC, NULL_LOC);
            }
            // Would this method require a proper scope!?!?!
            NecroType* instance_data_inst = necro_try(NecroType, necro_type_instantiate(arena, base, instance->data_type, instance->ast->scope));
            necro_try(NecroType, necro_type_unify(arena, base, instance_data_inst, type, scope));
            // Propogating classes wrong here!
            // NecroType* current_arg = type->con.args;
            // while (current_arg != NULL)
            // {
            //     necro_propogate_type_classes(infer, instance->context, current_arg->list.item, macro_type, error_preamble);
            //     if (necro_is_infer_error(infer)) return;
            //     current_arg = current_arg->list.next;
            // }
            // if (necro_is_infer_error(infer)) return;
            classes = classes->next;
        }
        return ok(NecroType, NULL);

    case NECRO_TYPE_FUN:
        // TODO: Type classes for functions!!!
        necro_try(NecroType, necro_propogate_type_classes(arena, base, classes, type->fun.type1, scope));
        necro_try(NecroType, necro_propogate_type_classes(arena, base, classes, type->fun.type2, scope));
        return ok(NecroType, NULL);

    case NECRO_TYPE_APP:
        // necro_propogate_type_classes(infer, classes, type->app.type1, macro_type, error_preamble);
        // necro_propogate_type_classes(infer, classes, type->app.type2, macro_type, error_preamble);
        assert(false && "Compiler bug: TypeApp not implemented in necro_propogate_type_classes! (i.e. constraints of the form: Num (f a), or (c a), are not currently supported)");
        return ok(NecroType, NULL);

    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

// TODO: rename -> necro_bind_type_var
NecroResult(NecroType) necro_instantiate_type_var(NecroPagedArena* arena, NecroBase* base, NecroTypeVar* type_var, NecroType* type, NecroScope* scope)
{
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
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type1->var.var_symbol == type2->var.var_symbol)  return ok(NecroType, NULL);
        else if (type1->var.is_rigid && type2->var.is_rigid) return necro_type_rigid_type_variable_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol, type2));
        if (type1->var.is_rigid)                             return necro_instantiate_type_var(arena, base, &type2->var, type1, scope);
        else if (type2->var.is_rigid)                        return necro_instantiate_type_var(arena, base, &type1->var, type2, scope);
        else if (necro_type_is_bound_in_scope(type1, scope)) return necro_instantiate_type_var(arena, base, &type2->var, type1, scope);
        else                                                 return necro_instantiate_type_var(arena, base, &type1->var, type2, scope);
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (type1->var.is_rigid)
            return necro_type_rigid_type_variable_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type1->var.var_symbol , type2));
        necro_try(NecroType, necro_instantiate_type_var(arena, base, &type1->var, type2, scope));
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
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_instantiate_type_var(arena, base, &type2->var, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_APP:
        necro_try(NecroType, necro_type_unify(arena, base, type1->app.type1, type2->app.type1, scope));
        necro_try(NecroType, necro_type_unify(arena, base, type1->app.type2, type2->app.type2, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_type_curry_con(arena, type2);
        if (uncurried_con == NULL)
            return necro_type_mismatched_arity_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        else
            return necro_type_unify(arena, base, type1, uncurried_con, scope);
    }
    case NECRO_TYPE_FUN:  return necro_type_mismatched_type_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
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
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_instantiate_type_var(arena, base, &type2->var, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_FUN:
        necro_try(NecroType, necro_type_unify(arena, base, type1->fun.type1, type2->fun.type1, scope));
        necro_try(NecroType, necro_type_unify(arena, base, type1->fun.type2, type2->fun.type2, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_APP:  return necro_type_mismatched_type_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
    case NECRO_TYPE_CON:  return necro_type_mismatched_type_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    default:              necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_unify_con(NecroPagedArena* arena, NecroBase* base, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_CON);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            return necro_type_rigid_type_variable_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        necro_try(NecroType, necro_type_occurs(type2->var.var_symbol, type1));
        necro_try(NecroType, necro_instantiate_type_var(arena, base, &type2->var, type1, scope));
        return ok(NecroType, NULL);
    case NECRO_TYPE_CON:
        if (type1->con.con_symbol == type2->con.con_symbol)
        {
            return necro_type_mismatched_type_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        }
        else
        {
            NecroType* original_type1 = type1;
            NecroType* original_type2 = type2;
            type1 = type1->con.args;
            type2 = type2->con.args;
            while (type1 != NULL && type2 != NULL)
            {
                if (type1 == NULL || type2 == NULL)
                    return necro_type_mismatched_arity_error(NULL, original_type1, NULL_LOC, NULL_LOC, NULL, original_type2, NULL_LOC, NULL_LOC);
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
        NecroType* uncurried_con = necro_type_curry_con(arena, type1);
        if (uncurried_con == NULL)
            return necro_type_mismatched_arity_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
        else
            return necro_type_unify(arena, base, uncurried_con, type2, scope);
    }
    case NECRO_TYPE_FUN:  return necro_type_mismatched_type_error(NULL, type1, NULL_LOC, NULL_LOC, NULL, type2, NULL_LOC, NULL_LOC);
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:  necro_unreachable(NecroType);
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
    case NECRO_TYPE_LIST: necro_unreachable(NecroType);
    case NECRO_TYPE_FOR:
        while (type1->type == NECRO_TYPE_FOR)
            type1 = type1->for_all.type;
        necro_type_unify(arena, base, type1, type2, scope);
        return ok(NecroType, NULL);
    default:
        necro_unreachable(NecroType);
    }
}

//=====================================================
// Inst
//=====================================================
typedef struct NecroInstSub
{
    NecroAstSymbol*      var_to_replace;
    NecroType*           new_name;
    struct NecroInstSub* next;
} NecroInstSub;

NecroInstSub* necro_create_inst_sub(NecroPagedArena* arena, NecroAstSymbol* var_to_replace, NecroTypeClassContext* context, NecroInstSub* next)
{
    NecroType* type_var                   = necro_type_fresh_var(arena);
    type_var->var.var_symbol->source_name = var_to_replace->source_name;
    type_var->kind                        = necro_type_find(var_to_replace->type)->kind;
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

NecroType* necro_inst_go(NecroPagedArena* arena, NecroType* type, NecroInstSub* subs, NecroScope* scope)
{
    if (type == NULL)
        return NULL;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        while (subs != NULL)
        {
            if (type->var.var_symbol == subs->var_to_replace)
            {
                return subs->new_name;
            }
            subs = subs->next;
        }
        return type;
    }
    case NECRO_TYPE_APP:  return necro_type_app_create(arena, necro_inst_go(arena, type->app.type1, subs, scope), necro_inst_go(arena, type->app.type2, subs, scope));
    case NECRO_TYPE_FUN:  return necro_type_fn_create(arena, necro_inst_go(arena, type->fun.type1, subs, scope), necro_inst_go(arena, type->fun.type2, subs, scope));
    case NECRO_TYPE_CON:  return necro_type_con_create(arena, type->con.con_symbol, necro_inst_go(arena, type->con.args, subs, scope), type->con.arity);
    case NECRO_TYPE_LIST: return necro_type_list_create(arena, necro_inst_go(arena, type->list.item, subs, scope), necro_inst_go(arena, type->list.next, subs, scope));
    case NECRO_TYPE_FOR:  assert(false); return NULL;
    default:              assert(false); return NULL;
    }
}

NecroResult(NecroType) necro_type_instantiate(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroScope* scope)
{
    assert(type != NULL);
    type = necro_type_find(type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs         = necro_create_inst_sub(arena, current_type->for_all.var_symbol, current_type->for_all.context, subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_inst_go(arena, current_type, subs, scope);
    necro_try(NecroType, necro_kind_infer(arena, base, result));
    return ok(NecroType, result);
}

NecroResult(NecroType) necro_type_instantiate_with_context(NecroPagedArena* arena, NecroBase* base, NecroType* type, NecroScope* scope, NecroTypeClassContext** context)
{
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
    NecroType* result = necro_inst_go(arena, current_type, subs, scope);
    necro_try(NecroType, necro_kind_infer(arena, base, result));
    assert(result != NULL);
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
    if (top->type == NECRO_TYPE_VAR && top->var.is_rigid && !top->var.is_type_class_var)
    {
        prev_result.type = top;
        return prev_result;
    }

    if (necro_type_is_bound_in_scope(type, scope) && !type->var.is_type_class_var)
    {
        prev_result.type = type;
        return prev_result;
    }

    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid && !type->var.is_type_class_var)
        {
            prev_result.type = type;
            return prev_result;
        }
        NecroType* bound_type = type->var.bound;
        if (bound_type != NULL && !type->var.is_type_class_var)
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
                    type->var.gen_bound = subs->type_var;
                    return prev_result;
                }
                subs = subs->next;
            }
            NecroGenSub* sub = necro_paged_arena_alloc(arena, sizeof(NecroGenSub));
            NecroType*   type_var;
            if (!type->var.is_type_class_var)
            {
                type_var                              = necro_type_fresh_var(arena);
                type_var->var.var_symbol->source_name = type->var.var_symbol->source_name;
                type_var->var.is_rigid                = true;
                type_var->var.context                 = type->var.context;
                // Set context vars to new rigid var?
                NecroTypeClassContext* context = type_var->var.context;
                while (context != NULL)
                {
                    context->var_symbol = type_var->var.var_symbol;
                    context             = context->next;
                }
                type_var->kind      = type->kind;
                type->var.gen_bound = type_var;
            }
            else
            {
                type_var = type;
            }
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
        result.type           = necro_type_con_create(arena, type->con.con_symbol, result.type, type->con.arity);
        return result;
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
    necro_try(NecroType, necro_kind_infer(arena, base, type));
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
                necro_try(NecroType, necro_kind_infer(arena, base, head));
                return ok_NecroType(head);
            }
        }
    }
    else
    {
        necro_try(NecroType, necro_kind_infer(arena, base, result.type));
        return ok_NecroType(result.type);
    }
}

//=====================================================
// Printing
//=====================================================
bool necro_is_simple_type(NecroType* type)
{
    assert(type != NULL);
    return type->type == NECRO_TYPE_VAR || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0);
}

void necro_type_print(NecroType* type);

void necro_print_type_sig_go_maybe_with_parens(NecroType* type)
{
    if (!necro_is_simple_type(type))
        printf("(");
    necro_type_print(type);
    if (!necro_is_simple_type(type))
        printf(")");
}

bool necro_print_tuple_sig(NecroType* type)
{
    NecroSymbol con_symbol = type->con.con_symbol->source_name;
    const char* con_string = type->con.con_symbol->source_name->str;

    if (con_string[0] != '(' && con_string[0] != '[')
        return false;
    NecroType* current_element = type->con.args;

    // Unit
    if (strcmp(con_symbol->str, "()") == 0)
    {
        printf("()");
        return true;
    }

    // List
    if (strcmp(con_symbol->str, "[]") == 0)
    {
        printf("[");
        if (current_element != NULL && current_element->list.item != NULL)
            necro_type_print(current_element->list.item);
        printf("]");
        return true;
    }

    printf("(");
    while (current_element != NULL)
    {
        necro_type_print(current_element->list.item);
        if (current_element->list.next != NULL)
            printf(",");
    }
    printf(")");
    return true;
}

void necro_type_print(NecroType* type)
{
    if (type == NULL)
        return;
    while (type->type == NECRO_TYPE_VAR && type->var.bound != NULL)
        type = type->var.bound;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (type->var.var_symbol->source_name != NULL && type->var.var_symbol->source_name->str != NULL)
        {
            printf("%s", type->var.var_symbol->source_name->str);
        }
        else
        {
            printf("tyvar_%d", (size_t) type->var.var_symbol);
        }
        break;

    case NECRO_TYPE_APP:
        necro_print_type_sig_go_maybe_with_parens(type->app.type1);
        printf(" ");
        necro_print_type_sig_go_maybe_with_parens(type->app.type2);
        break;

    case NECRO_TYPE_FUN:
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            printf("(");
        necro_type_print(type->fun.type1);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            printf(")");
        printf(" -> ");
        necro_type_print(type->fun.type2);
        break;

    case NECRO_TYPE_CON:
    {
        if (necro_print_tuple_sig(type))
            break;
        bool has_args = necro_type_list_count(type->con.args) > 0;
        printf("%s", type->con.con_symbol->source_name->str);
        if (has_args)
        {
            printf(" ");
            necro_type_print(type->con.args);
        }
        break;
    }

    case NECRO_TYPE_LIST:
        necro_print_type_sig_go_maybe_with_parens(type->list.item);
        if (type->list.next != NULL)
        {
            printf(" ");
            necro_type_print(type->list.next);
        }
        break;

    case NECRO_TYPE_FOR:
        printf("forall ");

        // Print quantified type vars
        NecroType* for_all_head = type;
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            printf("%s", type->for_all.var_symbol->source_name->str);
            printf(" ");
            type = type->for_all.type;
        }
        printf("%s", type->for_all.var_symbol->source_name->str);
        printf(". ");
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
            printf("(");
            size_t count = 0;
            type = for_all_head;
            while (type->type == NECRO_TYPE_FOR)
            {
                NecroTypeClassContext* context = type->for_all.context;
                while (context != NULL)
                {
                    if (count > 0)
                        printf(", ");
                    printf("%s ", context->class_symbol->source_name->str);
                    printf("%s", type->for_all.var_symbol->source_name->str);
                    context = context->next;
                    count++;
                }
                type = type->for_all.type;
            }
            printf(") => ");
        }

        // Print rest of type
        necro_type_print(type);
        break;

    default:
        assert(false);
        break;
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
    case 2:  con_symbol = base->tuple2_con;  break;
    case 3:  con_symbol = base->tuple3_con;  break;
    case 4:  con_symbol = base->tuple4_con;  break;
    case 5:  con_symbol = base->tuple5_con;  break;
    case 6:  con_symbol = base->tuple6_con;  break;
    case 7:  con_symbol = base->tuple7_con;  break;
    case 8:  con_symbol = base->tuple7_con;  break;
    case 9:  con_symbol = base->tuple9_con;  break;
    case 10: con_symbol = base->tuple10_con; break;
    default:
        assert(false && "Tuple size too large");
        break;
    }
    return necro_type_con_create(arena, con_symbol, types_list, tuple_count);
}

NecroType* necro_type_con1_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1)
{
    NecroType* lst1 = necro_type_list_create(arena, arg1, NULL);
    return necro_type_con_create(arena, con, lst1 , 1);
}

NecroType* necro_type_con2_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2)
{
    NecroType* lst2 = necro_type_list_create(arena, arg2, NULL);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 2);
}

NecroType* necro_type_con3_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3)
{
    NecroType* lst3 = necro_type_list_create(arena, arg3, NULL);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 3);
}

NecroType* necro_type_con4_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4)
{
    NecroType* lst4 = necro_type_list_create(arena, arg4, NULL);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 4);
}

NecroType* necro_type_con5_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5)
{
    NecroType* lst5 = necro_type_list_create(arena, arg5, NULL);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 5);
}

NecroType* necro_type_con6_create(NecroPagedArena* arena, NecroAstSymbol* con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6)
{
    NecroType* lst6 = necro_type_list_create(arena, arg6, NULL);
    NecroType* lst5 = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 6);
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
    return necro_type_con_create(arena, con, lst1, 7);
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
    return necro_type_con_create(arena, con, lst1, 8);
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
    return necro_type_con_create(arena, con, lst1, 9);
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
    return necro_type_con_create(arena, con, lst1, 10);
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
    default:
        assert(false);
        return false;
    }
}

//=====================================================
// Testing
//=====================================================
void necro_type_test()
{
    necro_announce_phase("NecroType");
}
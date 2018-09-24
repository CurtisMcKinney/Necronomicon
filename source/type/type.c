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
        .error           = necro_create_error(),
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

NecroType*  necro_type_alloc(NecroPagedArena* arena);

//=====================================================
// Utility
//=====================================================
bool necro_is_infer_error(NecroInfer* infer)
{
    return infer->error.return_code != NECRO_SUCCESS;
}

void* necro_infer_error(NecroInfer* infer, const char* error_preamble, NecroType* type, const char* error_message, ...)
{
    UNUSED(infer);
    UNUSED(error_preamble);
    UNUSED(error_message);
    UNUSED(type);
    // if (infer->error.return_code != NECRO_SUCCESS)
    //     return type;
    // va_list args;
	// va_start(args, error_message);
    // NecroSourceLoc source_loc = (type != NULL) ? type->source_loc : infer->error.source_loc;
    // size_t count = necro_verror(&infer->error, source_loc, error_message, args);
    // if (error_preamble != NULL)
    //     count += snprintf(infer->error.error_message + count, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n %s", error_preamble);
    // count += snprintf(infer->error.error_message + count, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n Found in type: ");
    // if (type != NULL)
    //     necro_type_sig_snprintf(type, infer->intern, infer->error.error_message + count, NECRO_MAX_ERROR_MESSAGE_LENGTH);

    // va_end(args);

    return NULL;
}

// bool necro_check_and_print_type_error(NecroInfer* infer)
// {
//     if (infer->error.return_code != NECRO_SUCCESS)
//     {
//         printf("Type error:\n    %s", infer->error.error_message);
//         return true;
//     }
//     else
//     {
//         return false;
//     }
// }

NecroType* necro_type_alloc(NecroPagedArena* arena)
{
    NecroType* type    = necro_paged_arena_alloc(arena, sizeof(NecroType));
    type->pre_supplied = false;
    type->source_loc   = (NecroSourceLoc) { 0, 0, 0 };
    type->kind         = NULL;
    return type;
}

NecroType* necro_type_var_create(NecroPagedArena* arena, NecroAstSymbol var_symbol)
{
    // if (var.id.id > infer->highest_id)
    //     infer->highest_id = var.id.id + 1;

    // if (var.id.id >= infer->env.capacity)
    // {
    //     size_t new_size = next_highest_pow_of_2(var.id.id) * 2;
    //     if (new_size < infer->env.capacity)
    //         new_size = infer->env.capacity * 2;
    //     // printf("Realloc env, size: %d, new_size: %d, id: %d\n", infer->env.capacity, new_size, var.id.id);
    //     NecroType** new_data = realloc(infer->env.data, new_size * sizeof(NecroType*));
    //     if (new_data == NULL)
    //     {
    //         if (infer->env.data != NULL)
    //             free(infer->env.data);
    //         fprintf(stderr, "Malloc returned NULL in infer env reallocation!\n");
    //         exit(1);
    //     }
    //     for (size_t i = infer->env.capacity; i < new_size; ++i)
    //         new_data[i] = NULL;
    //     infer->env.capacity = new_size;
    //     infer->env.data     = new_data;
    // }

    // if (infer->env.data[var.id.id] != NULL)
    //     return infer->env.data[var.id.id];

    NecroType* type = necro_type_alloc(arena);
    type->type      = NECRO_TYPE_VAR;
    // type->kind      = NULL;
    type->kind = NULL;
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

    // infer->env.data[type->var.var.id.id] = type;
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
    // type->kind = necro_apply_kinds(infer, type1, type2, type1, NULL);
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

NecroType* necro_type_declare(NecroPagedArena* arena, NecroAstSymbol con_symbol, size_t arity)
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
    // type->kind         = infer->star_kind;
    type->pre_supplied = true;
    type->con.arity    = arity;
    return type;
}

NecroType* necro_type_con_create(NecroPagedArena* arena, NecroAstSymbol con_symbol, NecroType* args, size_t arity)
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
    // type->kind = NULL;
    return type;
}

NecroType* necro_type_for_all_create(NecroPagedArena* arena, NecroAstSymbol var_symbol, NecroTypeClassContext* context, NecroType* type)
{
    // if (var.id.id > infer->highest_id)
    //     infer->highest_id = var.id.id + 1;
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
    type = necro_find(type);
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

NecroType* necro_curry_con(NecroPagedArena* arena, NecroType* con)
{
    assert(arena != NULL);
    assert(con != NULL);
    assert(con->type == NECRO_TYPE_CON);
    // if (necro_is_infer_error(infer))
    //     return NULL;
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

// TODO: Rename to something like necro_new_type_var
NecroType* necro_new_name(NecroInfer* infer)
{
    UNUSED(infer);
    // TODO: Reimplement with something like renamer mangling system
    // infer->highest_id++;
    // assert(infer->highest_id <= UINT32_MAX);
    // NecroVar   var       = (NecroVar) { .id = { (uint32_t) infer->highest_id }, .symbol = NULL };
    // NecroType* type_var  = necro_type_var_create(infer->arena, var_symbol);
    // // type_var->kind       = NULL;
    // type_var->source_loc = source_loc;
    // type_var->kind  = NULL;
    // return type_var;
    return NULL;
}

//=====================================================
// NecroTypeEnv
//=====================================================
// TODO: Reimplement directly with NecroAstSymbol
// void necr_bind_type_var(NecroAstSymbol var_symbol, NecroType* type)
// {
//     necro_find(var_symbol.data->type)->var.bound = type;
//     // if (var.id.id >= infer->env.capacity)
//     // {
//     //     necro_infer_error(infer, NULL, NULL, "Cannot find variable %s", necro_id_as_character_string(infer->intern, var));
//     // }
//     // assert(infer->env.data != NULL);
//     // infer->env.data[var.id.id]->var.bound = type;
// }

//=====================================================
// Find
//=====================================================
NecroType* necro_find(NecroType* type)
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

bool necro_is_bound_in_scope(NecroType* type, NecroScope* scope)
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
// void necro_rigid_type_variable_error(NecroInfer* infer, NecroVar type_var, NecroType* type, NecroType* macro_type, const char* error_preamble)
// {
//     // TODO: Reimplement with NecroResult system
//     return;
//     // if (necro_is_infer_error(infer))
//     //     return;
//     // const char* type_name = NULL;
//     // if (type->type == NECRO_TYPE_CON)
//     //     type_name = type->con.con.symbol->str;
//     // else if (type->type == NECRO_TYPE_APP)
//     //     type_name = "TypeApp";
//     // else if (type->type == NECRO_TYPE_FUN)
//     //     type_name = "(->)";
//     // else if (type->type == NECRO_TYPE_VAR)
//     //     type_name = necro_id_as_character_string(infer->intern, type->var.var);
//     // else
//     //     assert(false);
//     // const char* var_name = necro_id_as_character_string(infer->intern, type_var);
//     // necro_infer_error(infer, error_preamble, macro_type, "Couldn't match type \'%s\' with type \'%s\'.\n\'%s\' is a rigid type variable bound by a type signature.", var_name, type_name, var_name);
// }

// void necro_type_is_not_instance_error(NecroInfer* infer, NecroType* type, NecroTypeClassContext* type_class, NecroType* macro_type, const char* error_preamble)
// {
//     // TODO: Reimplement with NecroResult system
//     // if (necro_is_infer_error(infer))
//     //     return;
//     // necro_infer_error(infer, error_preamble, macro_type, "\'%s\' is not an instance of class \'%s\'", type->con.con.symbol->str, type_class->type_class_name.symbol->str);
// }

// void necro_occurs_error(NecroInfer* infer, NecroVar type_var, NecroType* type, NecroType* macro_type, const char* error_preamble)
// {
//     // TODO: Reimplement with NecroResult system
//     UNUSED(type);
//     // if (necro_is_infer_error(infer))
//     //     return;
//     // necro_infer_error(infer, error_preamble, macro_type, "Occurs check error, cannot construct the infinite type: \n %s ~ ", necro_id_as_character_string(infer->intern, type_var));
//     // necro_type_sig_snprintf(macro_type, infer->intern, infer->error.error_message + 64, NECRO_MAX_ERROR_MESSAGE_LENGTH);
// }

bool necro_occurs(NecroAstSymbol type_var_symbol, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    if (type == NULL)
        return false;
    type             = necro_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (type_var_symbol.name == type->var.var_symbol.name)
        {
            // TODO: Reimplement with NecroResult system
            // necro_occurs_error(infer, type_var->var.var, type, macro_type, error_preamble);
            return true;
        }
        else
        {
            return false;
        }
    case NECRO_TYPE_APP:  return necro_occurs(type_var_symbol, type->app.type1, macro_type, error_preamble) || necro_occurs(type_var_symbol, type->app.type2, macro_type, error_preamble);
    case NECRO_TYPE_FUN:  return necro_occurs(type_var_symbol, type->fun.type1, macro_type, error_preamble) || necro_occurs(type_var_symbol, type->fun.type2, macro_type, error_preamble);
    case NECRO_TYPE_CON:  return necro_occurs(type_var_symbol, type->con.args , macro_type, error_preamble);
    case NECRO_TYPE_LIST: return necro_occurs(type_var_symbol, type->list.item, macro_type, error_preamble) || necro_occurs(type_var_symbol, type->list.next, macro_type, error_preamble);
    case NECRO_TYPE_FOR:  assert(false); return false;
    default:              assert(false); return false;
    }
}

void necro_propogate_type_classes(NecroInfer* infer, NecroTypeClassContext* classes, NecroType* type, NecroType* macro_type, const char* error_preamble, NecroScope* scope)
{
    // if (necro_is_infer_error(infer))
    //     return;
    if (classes == NULL)
        return;
    if (type == NULL)
        return;
    type = necro_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
        {
            // If it's a rigid variable, make sure it has all of the necessary classes in its context already
            while (classes != NULL)
            {
                // if (!necro_context_contains_class(infer->type_class_env, type->var.context, classes))
                if (!necro_context_and_super_classes_contain_class(type->var.context, classes))
                {
                    // TODO: NecroResultError
                    // necro_infer_error(infer, error_preamble, macro_type, "No instance for \'%s %s\'", classes->type_class_name.symbol->str, necro_id_as_character_string(infer->intern, type->var.var));
                    return;
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
            type->var.context = necro_union_contexts_to_same_var(infer->arena, type->var.context, classes, type->var.var_symbol);
        }
        return;
    }
    case NECRO_TYPE_CON:
    {
        while (classes != NULL)
        {
            NecroTypeClassInstance* instance = necro_get_type_class_instance(infer, type->con.con_symbol, classes->class_symbol);
            if (instance == NULL)
            {
                // TODO: NecroResultError
                // necro_type_is_not_instance_error(infer, type, classes, macro_type, error_preamble);
                // return;
            }
            // Propogating classes wrong here!
            NecroType* instance_data_inst = necro_inst(infer, instance->data_type, instance->ast->scope);
            // Would this method require a proper scope!?!?!
            necro_unify(infer, instance_data_inst, type, scope, type, "While inferring the instance of a type class");
            // if (necro_is_infer_error(infer)) return;
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
        return;
    }
    case NECRO_TYPE_FUN:
        // TODO: Type classes for functions!!!
        necro_propogate_type_classes(infer, classes, type->fun.type1, macro_type, error_preamble, scope);
        necro_propogate_type_classes(infer, classes, type->fun.type2, macro_type, error_preamble, scope);
        // necro_infer_error(infer, error_preamble, macro_type, "(->) Not implemented for type classes!", type->type);
        return;
    // case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: TypeApp not implemented in necro_propogate_type_classes!"); return;
    case NECRO_TYPE_APP:
        // TODO: This is wrong!!!
        // Need proper constructor constraints!!!!
        // necro_propogate_type_classes(infer, classes, type->app.type1, macro_type, error_preamble);
        // necro_propogate_type_classes(infer, classes, type->app.type2, macro_type, error_preamble);
        // return;
        // necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: TypeApp not implemented in necro_propogate_type_classes! (i.e. constraints of the form: Num (f a), are not currently supported)"); return;
        assert(false && "Compiler bug: TypeApp not implemented in necro_propogate_type_classes! (i.e. constraints of the form: Num (f a), are not currently supported)");
        return;
    case NECRO_TYPE_LIST: assert(false && "Compiler bug: Found ConTypeList in necro_propogate_type_classes!"); return;
    case NECRO_TYPE_FOR:  assert(false && "Compiler bug: Found polytype in necro_propogate_type_classes!"); return;
    default:              assert(false && "Compiler bug: Unrecognized type: %d."); return;
    }
}

inline void necro_instantiate_type_var(NecroInfer* infer, NecroTypeVar* type_var, NecroType* type, NecroType* macro_type, const char* error_preamble, NecroScope* scope)
{
    necro_propogate_type_classes(infer, type_var->context, type, macro_type, error_preamble, scope);
    type_var->bound = necro_find(type);
}

inline void necro_unify_var(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_VAR);
    if (type2 == NULL)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Mismatched arities");
        return;
    }
    if (type1 == type2)
        return;
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type1->var.var_symbol.name == type2->var.var_symbol.name) // TODO: Make NecroAstSymbol ptr based, compare them directly instead of names...
            return;
        else if (type1->var.is_rigid && type2->var.is_rigid)
            ; // TODO: NecroResultError
            // necro_rigid_type_variable_error(infer, type1->var.var, type2, macro_type, error_preamble);
        else if (necro_occurs(type1->var.var_symbol, type2, macro_type, error_preamble))
            return;
        else if (type1->var.is_rigid)
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble, scope);
        else if (type2->var.is_rigid)
            necro_instantiate_type_var(infer, &type1->var, type2, macro_type, error_preamble, scope);
        else if (necro_is_bound_in_scope(type1, scope))
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble, scope);
        else
            necro_instantiate_type_var(infer, &type1->var, type2, macro_type, error_preamble, scope);
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (type1->var.is_rigid)
        {
            // NecroResultError
            // necro_rigid_type_variable_error(infer, type1->var.var, type2, macro_type, error_preamble);
            return;
        }
        if (necro_occurs(type1->var.var_symbol , type2, macro_type, error_preamble))
        {
            return;
        }
        necro_instantiate_type_var(infer, &type1->var, type2, macro_type, error_preamble, scope);
        return;
    case NECRO_TYPE_FOR:  assert(false && "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: assert(false && "Compiler bug: Attempted to unify TypeVar with type args list."); return;
    default:              assert(false && "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify."); return;
    }
}

inline void necro_unify_app(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_APP);
    if (type2 == NULL)
    {
        // TODO: NecroResultError
        // necro_infer_error(infer, error_preamble, macro_type, "Arity mismatch during unification for type: %s", type1->con.con.symbol->str);
        // return;
    }
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            // TODO: NecroResultError
            ; // necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
        else if (necro_occurs(type2->var.var_symbol, type1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble, scope);
        return;
    case NECRO_TYPE_APP:
        necro_unify(infer, type1->app.type1, type2->app.type1, scope, macro_type, error_preamble);
        necro_unify(infer, type1->app.type2, type2->app.type2, scope, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_curry_con(infer->arena, type2);
        if (uncurried_con == NULL)
        {
            // TODO: NecroResultError
            // necro_infer_error(infer, error_preamble, macro_type, "Arity mismatch during unification for type: %s", type2->con.con.symbol->str);
        }
        else
        {
            necro_unify(infer, type1, uncurried_con, scope, macro_type, error_preamble);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify TypeApp with (->)."); return;
    case NECRO_TYPE_FOR:  assert(false); return;
    case NECRO_TYPE_LIST: assert(false); return;
    default:              assert(false); return;
    }
}

inline void necro_unify_fun(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_FUN);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            ; // TODO: NecroResultError: necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
        else if (necro_occurs(type2->var.var_symbol, type1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble, scope);
        return;
    case NECRO_TYPE_FUN:  necro_unify(infer, type1->fun.type1, type2->fun.type1, scope, macro_type, error_preamble); necro_unify(infer, type1->fun.type2, type2->fun.type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify (->) with TypeApp."); return;
    case NECRO_TYPE_CON:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify (->) with TypeCon (%s)", type2->con.con_symbol.source_name->str); return;
    case NECRO_TYPE_LIST: assert(false); return;
    default:              assert(false); return;
    }
}

inline void necro_unify_con(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_CON);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            ; // TODO: NecroResultError: necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
        else if (necro_occurs(type2->var.var_symbol, type1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble, scope);
        return;
    case NECRO_TYPE_CON:
        if (type1->con.con_symbol.name == type2->con.con_symbol.name)
        {
            // TODO: NecroResultError
            // necro_infer_error(infer, error_preamble, type1, "Attempting to unify two different types, Type1: %s Type2: %s", type1->con.con.symbol->str, type2->con.con.symbol->str);
            return;
        }
        else
        {
            // NecroType* original_type1 = type1;
            // NecroType* original_type2 = type2;
            type1 = type1->con.args;
            type2 = type2->con.args;
            while (type1 != NULL && type2 != NULL)
            {
                if (type1 == NULL || type2 == NULL)
                {
                    // TODO; NecroResultError
                    // necro_infer_error(infer, error_preamble, type1, "Mismatched arities, Type1: %s Type2: %s", original_type1->con.con.symbol->str, original_type2->con.con.symbol->str);
                    return;
                }
                assert(type1->type == NECRO_TYPE_LIST);
                assert(type2->type == NECRO_TYPE_LIST);
                necro_unify(infer, type1->list.item, type2->list.item, scope, macro_type, error_preamble);
                type1 = type1->list.next;
                type2 = type2->list.next;
            }
        }
        return;
    case NECRO_TYPE_APP:
    {
        NecroType* uncurried_con = necro_curry_con(infer->arena, type1);
        if (uncurried_con == NULL)
        {
            // TODO: NecroResultError
            // necro_infer_error(infer, error_preamble, type1, "Arity mismatch during unification for type: %s", type1->con.con.symbol->str);
            return;
        }
        else
        {
            necro_unify(infer, uncurried_con, type2, scope, macro_type, error_preamble);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify TypeCon (%s) with (->).", type1->con.con_symbol.name->str); return;
    case NECRO_TYPE_LIST: assert(false); return;
    case NECRO_TYPE_FOR:  assert(false); return;
    default:              assert(false); return;
    }
}

// NOTE: Can only unify with a polymorphic type on the left side
void necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    assert(infer != NULL);
    assert(type1 != NULL);
    assert(type2 != NULL);
    type1 = necro_find(type1);
    type2 = necro_find(type2);
    if (type1 == type2)
        return;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  necro_unify_var(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_unify_app(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_FUN:  necro_unify_fun(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:  necro_unify_con(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_LIST: assert(false && "Compiler bug: Found Type args list in necro_unify"); return;
    case NECRO_TYPE_FOR:
    {
        while (type1->type == NECRO_TYPE_FOR)
            type1 = type1->for_all.type;
        necro_unify(infer, type1, type2, scope, macro_type, error_preamble);
        return;
    }
    default:
        assert(false);
        return;
    }
}

//=====================================================
// Inst
//=====================================================
typedef struct NecroInstSub
{
    // NecroVar             var_to_replace;
    NecroAstSymbol       var_to_replace;
    NecroType*           new_name;
    struct NecroInstSub* next;
} NecroInstSub;

NecroInstSub* necro_create_inst_sub(NecroInfer* infer, NecroAstSymbol var_to_replace, NecroTypeClassContext* context, NecroInstSub* next)
{
    NecroType* type_var                  = necro_new_name(infer);
    type_var->var.var_symbol.source_name = var_to_replace.source_name;
    type_var->kind                       = necro_find(var_to_replace.ast_data->type)->kind;
    // if (necro_symtable_get(infer->symtable, var_to_replace.id) != NULL && necro_symtable_get(infer->symtable, var_to_replace.id)->type != NULL)// && necro_symtable_get(infer->symtable, var_to_replace.id)->type->type_kind != NULL)
    // {
    //     NecroType* find_type = necro_find(necro_symtable_get(infer->symtable, var_to_replace.id)->type);
    //     if (find_type != NULL && find_type->kind != NULL)
    //         type_var->kind = find_type->kind;
    // }
    // // TODO: How does this work with NecroAstSymbol
    // // else if (infer->env.capacity > var_to_replace.id.id && infer->env.data[var_to_replace.id.id] != NULL)// && infer->env.data[var_to_replace.id.id]->type_kind != NULL)
    // // {
    // //     NecroType* find_type = necro_find(infer->env.data[var_to_replace.id.id]);
    // //     if (find_type != NULL && find_type->kind != NULL)
    // //         type_var->kind = find_type->kind;
    // // }
    // else
    // {
    //     // TODO: Need to fix this
    //     // Is this bad?
    //     // assert(false);
    // }
    NecroInstSub* sub = necro_paged_arena_alloc(infer->arena, sizeof(NecroInstSub));
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
    type = necro_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        while (subs != NULL)
        {
            // if (type->var.var.id.id == subs->var_to_replace.id.id)
            if (type->var.var_symbol.name == subs->var_to_replace.name)
            {
                return subs->new_name;
            }
            subs = subs->next;
        }
        // assert(!type->var.is_rigid);
        return type;
    }
    case NECRO_TYPE_APP:  return necro_type_app_create(arena, necro_inst_go(arena, type->app.type1, subs, scope), necro_inst_go(arena, type->app.type2, subs, scope));
    case NECRO_TYPE_FUN:  return necro_type_fn_create(arena, necro_inst_go(arena, type->fun.type1, subs, scope), necro_inst_go(arena, type->fun.type2, subs, scope));
    case NECRO_TYPE_CON:  return necro_type_con_create(arena, type->con.con_symbol, necro_inst_go(arena, type->con.args, subs, scope), type->con.arity);
    case NECRO_TYPE_LIST: return necro_type_list_create(arena, necro_inst_go(arena, type->list.item, subs, scope), necro_inst_go(arena, type->list.next, subs, scope));
    case NECRO_TYPE_FOR:  assert(false && "Found poly type in necro_inst_go"); return NULL;
    default:              assert(false && "Non-existent type %d found in necro_inst."); return NULL;
    }
}

NecroType* necro_inst(NecroInfer* infer, NecroType* type, NecroScope* scope)
{
    assert(infer != NULL);
    assert(type != NULL);
    type = necro_find(type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        // subs         = necro_create_inst_sub(infer, current_type->for_all.var, type->source_loc, current_type->for_all.context, subs);
        subs         = necro_create_inst_sub(infer, current_type->for_all.var_symbol, current_type->for_all.context, subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_inst_go(infer->arena, current_type, subs, scope);
    necro_kind_infer(infer, result, result, "While instantiating a type variable");
    return result;
}

NecroType* necro_inst_with_context(NecroInfer* infer, NecroType* type, NecroScope* scope, NecroTypeClassContext** context)
{
    assert(infer != NULL);
    assert(type != NULL);
    type = necro_find(type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    *context                   = NULL;
    NecroTypeClassContext* curr_context = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs = necro_create_inst_sub(infer, current_type->for_all.var_symbol, current_type->for_all.context, subs);
        NecroTypeClassContext* for_all_context = current_type->for_all.context;
        while (for_all_context != NULL)
        {
            if (*context == NULL)
            {
                curr_context = necro_create_type_class_context(infer->arena, for_all_context->type_class, for_all_context->class_symbol, subs->new_name->var.var_symbol, NULL);
                *context     = curr_context;
            }
            else
            {
                curr_context->next = necro_create_type_class_context(infer->arena, for_all_context->type_class, for_all_context->class_symbol, subs->new_name->var.var_symbol, NULL);
                curr_context       = curr_context->next;
            }
            for_all_context = for_all_context->next;
        }
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_inst_go(infer->arena, current_type, subs, scope);
    necro_kind_infer(infer, result, result, "While instantiating a type variable");
    return result;
}

//=====================================================
// Gen
//=====================================================
typedef struct NecroGenSub
{
    struct NecroGenSub* next;
    // NecroVar            var_to_replace;
    NecroAstSymbol      var_to_replace;
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

NecroGenResult necro_gen_go(NecroInfer* infer, NecroType* type, NecroGenResult prev_result, NecroScope* scope)
{
    assert(infer != NULL);
    // if (necro_is_infer_error(infer))
    // {
    //     prev_result.type = NULL;
    //     return prev_result;
    // }
    if (type == NULL)
    {
        prev_result.type = NULL;
        return prev_result;
    }

    NecroType* top = necro_find(type);
    if (top->type == NECRO_TYPE_VAR && top->var.is_rigid && !top->var.is_type_class_var)
    {
        prev_result.type = top;
        return prev_result;
    }

    if (necro_is_bound_in_scope(type, scope) && !type->var.is_type_class_var)
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
            return necro_gen_go(infer, bound_type, prev_result, scope);
        }
        else
        {
            NecroGenSub* subs = prev_result.subs;
            while (subs != NULL)
            {
                // if (subs->var_to_replace.id.id == type->var.var.id.id)
                if (subs->var_to_replace.name == type->var.var_symbol.name)
                {
                    prev_result.type    = subs->type_var;
                    type->var.gen_bound = subs->type_var;
                    return prev_result;
                }
                subs = subs->next;
            }
            NecroGenSub* sub = necro_paged_arena_alloc(infer->arena, sizeof(NecroGenSub));
            NecroType*   type_var;
            if (!type->var.is_type_class_var)
            {
                type_var                             = necro_new_name(infer);
                type_var->var.var_symbol.source_name = type->var.var_symbol.source_name;
                type_var->var.is_rigid               = true;
                type_var->var.context                = type->var.context;
                // Set context vars to new rigid var?
                NecroTypeClassContext* context = type_var->var.context;
                while (context != NULL)
                {
                    if (type_var->var.var_symbol.source_name == NULL)
                    {
                        // TODO: Handle type var names
                        // type_var->var.var_symbol.source_name = necro_intern_string(infer->intern, necro_id_as_character_string(infer->intern, type_var->var.var));
                    }
                    context->var_symbol = type_var->var.var_symbol;
                    context             = context->next;
                }
                type_var->kind      = type->kind;
                // TODO: Do we need to bind!?!?
                // necr_bind_type_var(infer, type->var.var, type_var);
                type->var.gen_bound = type_var;
            }
            else
            {
                type_var = type;
            }
            NecroType* for_all = necro_type_for_all_create(infer->arena, type_var->var.var_symbol, type->var.context, NULL);
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
        NecroGenResult result1 = necro_gen_go(infer, type->app.type1, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(infer, type->app.type2, result1, scope);
        result2.type           = necro_type_app_create(infer->arena, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_FUN:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->fun.type1, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(infer, type->fun.type2, result1, scope);
        result2.type           = necro_type_fn_create(infer->arena, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_LIST:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->list.item, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(infer, type->list.next, result1, scope);
        result2.type           = necro_type_list_create(infer->arena, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_CON:
    {
        NecroGenResult result = necro_gen_go(infer, type->con.args, prev_result, scope);
        result.type           = necro_type_con_create(infer->arena, type->con.con_symbol, result.type, type->con.arity);
        return result;
    }
    case NECRO_TYPE_FOR: assert(false); return (NecroGenResult) { NULL, NULL, NULL };
    default:             assert(false); return (NecroGenResult) { NULL, NULL, NULL };
    }
}

NecroType* necro_gen(NecroInfer* infer, NecroType* type, NecroScope* scope)
{
    assert(infer != NULL);
    assert(type != NULL);
    assert(type->type != NECRO_TYPE_FOR);
    NecroGenResult result = necro_gen_go(infer, type, (NecroGenResult) { NULL, NULL, NULL }, scope);
    if (necro_is_infer_error(infer))
        return NULL;
    necro_kind_infer(infer, type, type, "While generalizing a type:");
    if (necro_is_infer_error(infer))
        return NULL;
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
                necro_kind_infer(infer, head, head, "While generalizing a type:");
                return head;
            }
        }
    }
    else
    {
        necro_kind_infer(infer, result.type, result.type, "While generalizing a type:");
        return result.type;
    }
}

//=====================================================
// Printing
//=====================================================
// char* necro_type_string(NecroInfer* infer, NecroType* type)
// {
//     static const size_t MAX_KIND_BUFFER_LENGTH = 512;
//     char* buffer     = necro_paged_arena_alloc(infer->arena, MAX_KIND_BUFFER_LENGTH * sizeof(char));
//     char* buffer_end = necro_type_sig_snprintf(type, infer->intern, buffer, MAX_KIND_BUFFER_LENGTH);
//     *buffer_end = '\0';
//     return buffer;
// }

// NecroSymbol necro_id_as_symbol(NecroIntern* intern, NecroVar var)
// {
//     if (var.symbol != NULL)
//     {
//         return var.symbol;
//     }
//     NecroID id     = var.id;
//     size_t  length = 2;
//     // Compute length
//     if (id.id > 26)
//     {
//         size_t n = (id.id - 26);
//         length--;
//         while (n > 0)
//         {
//             n /= 26;
//             length++;
//         }
//     }
//     char* buffer = NULL;
//     char  small_buffer[5];
//     if (length < 6)
//         buffer = small_buffer;
//     else
//         buffer = malloc(sizeof(char) * length);
//     if (id.id <= 26)
//         snprintf(buffer, length, "%c", (char)((id.id % 26) + 97));
//     else
//         snprintf(buffer, length, "%c%d", (char)((id.id % 26) + 97), (id.id - 26) / 26);
//     NecroSymbol buf_symbol = necro_intern_string(intern, buffer);
//     if (length >= 6)
//         free(buffer);
//     return buf_symbol;
// }

// const char* necro_id_as_character_string(NecroIntern* intern, NecroVar var)
// {
//     return necro_id_as_symbol(intern, var)->str;
// }

// void necro_print_id_as_characters(NecroVar var)
// {
//     if (var.symbol != NULL)
//     {
//         printf("%s", var.symbol->str);
//         return;
//     }
//     NecroID id = var.id;
//     if (id.id <= 26)
//         printf("%c", (char)((id.id % 26) + 97));
//     else
//         printf("%c%d", (char)((id.id % 26) + 97), (id.id - 26) / 26);

// }

// char* necro_snprintf_id_as_characters(NecroVar var, char* buffer, size_t buffer_size)
// {
//     if (var.symbol != NULL)
//     {
//         return buffer + snprintf(buffer, buffer_size, "%s", var.symbol->str);
//     }
//     NecroID id = var.id;
//     if (id.id <= 26)
//         return buffer + snprintf(buffer, buffer_size, "%c", (char)((id.id % 26) + 97));
//     else
//         return buffer + snprintf(buffer, buffer_size, "%c%d", (char)((id.id % 26) + 97), (id.id - 26) / 26);
// }

bool necro_is_simple_type(NecroType* type)
{
    assert(type != NULL);
    return type->type == NECRO_TYPE_VAR || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0);
}

void necro_type_sig_print_go(NecroType* type);

void necro_print_type_sig_go_maybe_with_parens(NecroType* type)
{
    if (!necro_is_simple_type(type))
        printf("(");
    necro_type_sig_print_go(type);
    if (!necro_is_simple_type(type))
        printf(")");
}

bool necro_print_tuple_sig(NecroType* type)
{
    NecroSymbol con_symbol = type->con.con_symbol.source_name;
    const char* con_string = type->con.con_symbol.source_name->str;

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
            necro_type_sig_print_go(current_element->list.item);
        printf("]");
        return true;
    }

    printf("(");
    while (current_element != NULL)
    {
        necro_type_sig_print_go(current_element->list.item);
        if (current_element->list.next != NULL)
            printf(",");
    }
    printf(")");
    return true;
}

void necro_type_sig_print_go(NecroType* type)
{
    if (type == NULL)
        return;
    while (type->type == NECRO_TYPE_VAR && type->var.bound != NULL)
        type = type->var.bound;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        printf("%s", type->var.var_symbol.source_name->str);
        break;

    case NECRO_TYPE_APP:
        necro_print_type_sig_go_maybe_with_parens(type->app.type1);
        printf(" ");
        necro_print_type_sig_go_maybe_with_parens(type->app.type2);
        break;

    case NECRO_TYPE_FUN:
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            printf("(");
        necro_type_sig_print_go(type->fun.type1);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            printf(")");
        printf(" -> ");
        necro_type_sig_print_go(type->fun.type2);
        break;

    case NECRO_TYPE_CON:
    {
        if (necro_print_tuple_sig(type))
            break;
        bool has_args = necro_type_list_count(type->con.args) > 0;
        printf("%s", type->con.con_symbol.source_name->str);
        if (has_args)
        {
            printf(" ");
            necro_type_sig_print_go(type->con.args);
        }
        break;
    }

    case NECRO_TYPE_LIST:
        necro_print_type_sig_go_maybe_with_parens(type->list.item);
        if (type->list.next != NULL)
        {
            printf(" ");
            necro_type_sig_print_go(type->list.next);
        }
        break;

    case NECRO_TYPE_FOR:
        printf("forall ");

        // Print quantified type vars
        NecroType* for_all_head = type;
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            printf("%s", type->for_all.var_symbol.source_name->str);
            printf(" ");
            type = type->for_all.type;
        }
        printf("%s", type->for_all.var_symbol.source_name->str);
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
                    printf("%s ", context->class_symbol.source_name->str);
                    printf("%s", type->for_all.var_symbol.source_name->str);
                    context = context->next;
                    count++;
                }
                type = type->for_all.type;
            }
            printf(") => ");
        }

        // Print rest of type
        necro_type_sig_print_go(type);
        break;

    default:
        assert(false);
        break;
    }
}

void necro_type_sig_print(NecroType* type)
{
    if (type != NULL)
        necro_type_sig_print_go(type);
    printf("\n");
}

//=====================================================
// Prim Types
//=====================================================
NecroType* necro_type_tuple_con_create(NecroInfer* infer, NecroType* types_list)
{
    size_t     tuple_count  = 0;
    NecroType* current_type = types_list;
    while (current_type != NULL)
    {
        assert(current_type->type == NECRO_TYPE_LIST);
        tuple_count++;
        current_type = current_type->list.next;
    }
    NecroAstSymbol con_symbol = necro_ast_symbol_empty;
    switch (tuple_count)
    {
    case 2:  con_symbol = infer->base->tuple2_con;  break;
    case 3:  con_symbol = infer->base->tuple3_con;  break;
    case 4:  con_symbol = infer->base->tuple4_con;  break;
    case 5:  con_symbol = infer->base->tuple5_con;  break;
    case 6:  con_symbol = infer->base->tuple6_con;  break;
    case 7:  con_symbol = infer->base->tuple7_con;  break;
    case 8:  con_symbol = infer->base->tuple7_con;  break;
    case 9:  con_symbol = infer->base->tuple9_con;  break;
    case 10: con_symbol = infer->base->tuple10_con; break;
    default:
        assert(false && "Tuple size too large");
        break;
    }
    return necro_type_con_create(infer->arena, con_symbol, types_list, tuple_count);
}

NecroType* necro_type_con1_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1)
{
    NecroType* lst1 = necro_type_list_create(arena, arg1, NULL);
    return necro_type_con_create(arena, con, lst1 , 1);
}

NecroType* necro_type_con2_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2)
{
    NecroType* lst2 = necro_type_list_create(arena, arg2, NULL);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 2);
}

NecroType* necro_type_con3_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3)
{
    NecroType* lst3 = necro_type_list_create(arena, arg3, NULL);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 3);
}

NecroType* necro_type_con4_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4)
{
    NecroType* lst4 = necro_type_list_create(arena, arg4, NULL);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 4);
}

NecroType* necro_type_con5_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5)
{
    NecroType* lst5 = necro_type_list_create(arena, arg5, NULL);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 5);
}

NecroType* necro_type_con6_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6)
{
    NecroType* lst6 = necro_type_list_create(arena, arg6, NULL);
    NecroType* lst5 = necro_type_list_create(arena, arg5, lst6);
    NecroType* lst4 = necro_type_list_create(arena, arg4, lst5);
    NecroType* lst3 = necro_type_list_create(arena, arg3, lst4);
    NecroType* lst2 = necro_type_list_create(arena, arg2, lst3);
    NecroType* lst1 = necro_type_list_create(arena, arg1, lst2);
    return necro_type_con_create(arena, con, lst1, 6);
}

NecroType* necro_type_con7_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7)
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

NecroType* necro_type_con8_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8)
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

NecroType* necro_type_con9_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9)
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

NecroType* necro_type_con10_create(NecroPagedArena* arena, NecroAstSymbol con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10)
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
    type = necro_find(type);
    size_t h = 0;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        h = type->var.var_symbol.name->hash;
        break;
    case NECRO_TYPE_APP:
        h = 1361 ^ necro_type_hash(type->app.type1) ^ necro_type_hash(type->app.type2);
        break;
    case NECRO_TYPE_FUN:
        h = 8191 ^ necro_type_hash(type->fun.type1) ^ necro_type_hash(type->fun.type2);
        break;
    case NECRO_TYPE_CON:
    {
        h = 52103 ^ type->con.con_symbol.name->hash;
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
            h = h ^ context->var_symbol.name->hash ^ context->class_symbol.name->hash;
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

bool necro_exact_unify(NecroType* type1, NecroType* type2)
{
    type1 = necro_find(type1);
    type2 = necro_find(type2);
    if (type1 == NULL && type2 == NULL)
        return true;
    if (type1 == NULL || type2 == NULL)
        return false;
    if (type1->type != type2->type)
        return false;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR: return type1->var.var_symbol.name == type2->var.var_symbol.name;
    case NECRO_TYPE_APP: return necro_exact_unify(type1->app.type1, type2->app.type1) && necro_exact_unify(type1->app.type2, type2->app.type2);
    case NECRO_TYPE_FUN: return necro_exact_unify(type1->fun.type1, type2->fun.type1) && necro_exact_unify(type1->fun.type2, type2->fun.type2);
    case NECRO_TYPE_CON: return type1->con.con_symbol.name == type2->con.con_symbol.name && necro_exact_unify(type1->con.args, type2->con.args);
    case NECRO_TYPE_LIST:
    {
        while (type1 != NULL || type2 != NULL)
        {
            if (type1 == NULL || type2 == NULL)
                return false;
            if (!necro_exact_unify(type1->list.item, type2->list.item))
                return false;
            type1 = type1->list.next;
            type2 = type2->list.next;
        }
        return true;
    }
    case NECRO_TYPE_FOR:
    {
        if (type1->for_all.var_symbol.name != type2->for_all.var_symbol.name)
            return false;
        NecroTypeClassContext* context1 = type1->for_all.context;
        NecroTypeClassContext* context2 = type2->for_all.context;
        while (context1 != NULL || context2 != NULL)
        {
            if (context1 == NULL || context2 == NULL)
                return false;
            if (context1->var_symbol.name != context2->var_symbol.name || context1->class_symbol.name != context2->class_symbol.name)
                return false;
            context1 = context1->next;
            context2 = context2->next;
        }
        return necro_exact_unify(type1->for_all.type, type2->for_all.type);
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
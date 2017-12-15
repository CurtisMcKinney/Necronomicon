/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "prim.h"
#include "type.h"

// TODO / NOTE:
//  http://tomasp.net/coeffects/
//  http://tomasp.net/academic/papers/structural/
//  https://www.slideshare.net/tomaspfb/talk-38641264
//  eval_type_to_normal_form

#define NECRO_TYPE_DEBUG           1
#if NECRO_TYPE_DEBUG
#define TRACE_TYPE(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE(...)
#endif

NecroTypeEnv necro_create_type_env(size_t initial_size)
{
    initial_size     = next_highest_pow_of_2(initial_size);
    NecroType** data = malloc(initial_size * sizeof(NecroType*));
    if (data == NULL)
    {
        fprintf(stderr, "Malloc returned null creating NecroTypeEnv!\n");
        exit(1);
    }
    for (size_t i = 0; i < initial_size; ++i)
        data[i] = NULL;
    return (NecroTypeEnv)
    {
        .data     = data,
        .capacity = initial_size,
    };
}

NecroInfer necro_create_infer(NecroIntern* intern, struct NecroSymTable* symtable, NecroPrimTypes prim_types)
{

    NecroInfer infer = (NecroInfer)
    {
        .intern       = intern,
        .arena        = necro_create_paged_arena(),
        .env          = necro_create_type_env(1024),
        .error        = necro_create_error(),
        .highest_id   = symtable->count,
        .symtable     = symtable,
        .prim_types   = prim_types,
    };
    necro_add_prim_type_sigs(prim_types, &infer);
    return infer;
}

void necro_destroy_type_env(NecroTypeEnv* env)
{
    if (env == NULL)
        return;
    if (env->data == NULL)
        return;
    free(env->data);
    env->data     = NULL;
    env->capacity = 0;
    // env->count    = 0;
}

void necro_destroy_infer(NecroInfer* infer)
{
    if (infer == NULL)
        return;
    necro_destroy_type_env(&infer->env);
    necro_destroy_paged_arena(&infer->arena);
    infer->intern = NULL;
}

void necro_reset_infer(NecroInfer* infer)
{
    for (size_t i = 0; i < infer->env.capacity; ++i)
        infer->env.data[i] = NULL;
    // infer->env.count         = 0;
    infer->arena.count       = 0;
    infer->highest_id        = 0;
    infer->error.return_code = NECRO_SUCCESS;
}

//=====================================================
// Utility
//=====================================================
bool necro_is_infer_error(NecroInfer* infer)
{
    return infer->error.return_code != NECRO_SUCCESS;
}

void* necro_infer_error(NecroInfer* infer, NecroType* type, const char* error_message, ...)
{
    if (infer->error.return_code != NECRO_SUCCESS)
        return type;
    va_list args;
	va_start(args, error_message);
    NecroSourceLoc source_loc = (type != NULL) ? type->source_loc : ((NecroSourceLoc){0, 0, 0});

    size_t count1 = necro_verror(&infer->error, source_loc, error_message, args);
    size_t count2 = snprintf(infer->error.error_message + count1, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n    Found in type:\n        ");
    if (type != NULL)
        necro_snprintf_type_sig(type, infer->intern, infer->error.error_message + count1 + count2, NECRO_MAX_ERROR_MESSAGE_LENGTH);

    va_end(args);

    return NULL;
}

bool necro_check_and_print_type_error(NecroInfer* infer)
{
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        printf("Type error:\n    %s", infer->error.error_message);
        return true;
    }
    else
    {
        return false;
    }
}

inline NecroType* necro_alloc_type(NecroInfer* infer)
{
    NecroType* type    = necro_paged_arena_alloc(&infer->arena, sizeof(NecroType));
    type->pre_supplied = false;
    type->source_loc   = (NecroSourceLoc) { 0, 0, 0 };
    return type;
}

NecroType* necro_create_type_var(NecroInfer* infer, NecroVar var)
{
    if (var.id.id > infer->highest_id)
        infer->highest_id = var.id.id + 1;
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_VAR;
    type->var       = (NecroTypeVar)
    {
        .var        = var,
        .is_rigid   = false
    };
    return type;
}

NecroType* necro_create_type_app(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_APP;
    type->app       = (NecroTypeApp)
    {
        .type1 = type1,
        .type2 = type2,
    };
    return type;
}

NecroType* necro_create_type_fun(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_FUN;
    type->fun       = (NecroTypeFun)
    {
        .type1 = type1,
        .type2 = type2,
    };
    return type;
}

NecroType* necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args, size_t arity)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con   = con,
        .args  = args,
        .arity = arity
    };
    return type;
}

NecroType* necro_create_type_list(NecroInfer* infer, NecroType* item, NecroType* next)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_LIST;
    type->list      = (NecroTypeList)
    {
        .item = item,
        .next = next,
    };
    return type;
}

NecroType* necro_create_for_all(NecroInfer* infer, NecroVar var, NecroType* type)
{
    if (var.id.id > infer->highest_id)
        infer->highest_id = var.id.id + 1;
    NecroType* for_all = necro_paged_arena_alloc(&infer->arena, sizeof(NecroType));
    for_all->type      = NECRO_TYPE_FOR;
    for_all->for_all   = (NecroTypeForAll)
    {
        .var  = var,
        .type = type,
    };
    return for_all;
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

NecroType* necro_curry_con(NecroInfer* infer, NecroType* con)
{
    assert(infer != NULL);
    assert(con != NULL);
    assert(con->type == NECRO_TYPE_CON);
    if (necro_is_infer_error(infer))
        return NULL;
    if (con->con.args == NULL)
        return NULL;
    assert(con->con.args->type == NECRO_TYPE_LIST);
    NecroType* curr = con->con.args;
    NecroType* tail = NULL;
    NecroType* head = NULL;
    while (curr->list.next != NULL)
    {
        curr = necro_create_type_list(infer, curr->list.item, curr->list.next);
        if (tail != NULL)
            tail->list.next = curr;
        if (head == NULL)
            head = curr;
        tail = curr;
        curr = curr->list.next;
    }
    if (tail != NULL)
        tail->list.next = NULL;
    NecroType* new_con = necro_create_type_con(infer, con->con.con, head, con->con.arity);
    NecroType* ap      = necro_create_type_app(infer, new_con, curr->list.item);
    return ap;
}

NecroType* necro_new_name(NecroInfer* infer)
{
    infer->highest_id++;
    NecroVar var = (NecroVar) { .id = { infer->highest_id }, .scope = NULL };
    return necro_create_type_var(infer, var);
}

//=====================================================
// NecroTypeEnv
//=====================================================
NecroType* necro_env_get(NecroInfer* infer, NecroVar var)
{
    if (infer->env.capacity > var.id.id)
    {
        return infer->env.data[var.id.id];
    }
    else
    {
        // If you're going to print, don't pass in a null type...
        // necro_infer_error(infer, NULL, "Cannot find type for variable %s", necro_id_as_character_string(infer, var.id));
        return NULL;
    }
}

void necro_env_set(NecroInfer* infer, NecroVar var, NecroType* type)
{
    if (var.id.id >= infer->env.capacity)
    {
        size_t new_size = next_highest_pow_of_2(var.id.id) * 2;
        if (new_size < infer->env.capacity)
            new_size = infer->env.capacity * 2;
        printf("Realloc env, size: %d, new_size: %d, id: %d\n", infer->env.capacity, new_size, var.id.id);
        NecroType** new_data = realloc(infer->env.data, new_size * sizeof(NecroType*));
        if (new_data == NULL)
        {
            if (infer->env.data != NULL)
                free(infer->env.data);
            fprintf(stderr, "Malloc returned NULL in infer env reallocation!\n");
            exit(1);
        }
        for (size_t i = infer->env.capacity; i < new_size; ++i)
            new_data[i] = NULL;
        infer->env.capacity = new_size;
        infer->env.data     = new_data;
    }
    assert(infer->env.data != NULL);
    // infer->env.count
    infer->env.data[var.id.id] = type;
}

//=====================================================
// Normal Form
//=====================================================
NecroType* necro_uncurry(NecroInfer* infer, NecroType* type)
{
    if (necro_is_infer_error(infer))
        return NULL;
    if (type == NULL)
        return NULL;
    switch (type->type)
    {
    case NECRO_TYPE_VAR: return type;
    case NECRO_TYPE_FUN:
    {
        NecroType* type1 = necro_uncurry(infer, type->fun.type1);
        NecroType* type2 = necro_uncurry(infer, type->fun.type2);
        if (type1 == type->fun.type1 && type2 == type->fun.type2)
            return type;
        else
            return necro_create_type_fun(infer, type1, type2);
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* item = necro_uncurry(infer, type->list.item);
        NecroType* next = necro_uncurry(infer, type->list.next);
        if (item == type->list.item && next == type->list.next)
            return type;
        else
            return necro_create_type_list(infer, item, next);
    }
    case NECRO_TYPE_CON:
    {
        NecroType* args = necro_uncurry(infer, type->con.args);
        if (args == type->con.args)
            return type;
        else
            return necro_create_type_con(infer, type->con.con, args, type->con.arity);
    }

    case NECRO_TYPE_APP:
    {
        assert(type->app.type1 != NULL);
        if (type->app.type1->type == NECRO_TYPE_CON)
        {
            NecroType* type1_con = type->app.type1;
            NecroType* type2_arg = necro_create_type_list(infer, type->app.type2, NULL);
            NecroType* head      = type2_arg;
            NecroType* tail      = NULL;
            NecroType* curr      = type1_con->con.args;
            while (curr != NULL)
            {
                curr = necro_create_type_list(infer, curr->list.item, curr->list.next);
                if (tail != NULL)
                    tail->list.next = curr;
                if (head == type2_arg)
                    head = curr;
                tail = curr;
                curr = curr->list.next;
            }
            if (tail != NULL)
                tail->list.next = type2_arg;
            NecroType* rcon = necro_create_type_con(infer, type1_con->con.con, head, type1_con->con.arity);
            return necro_uncurry(infer, rcon);
        }
        else
        {
            NecroType* type1 = necro_uncurry(infer, type->app.type1);
            NecroType* type2 = necro_uncurry(infer, type->app.type2);
            if (type1 == type->app.type1 && type2 == type->app.type2)
                return type;
            else
                return necro_create_type_app(infer, type1, type2);
        }
    }

    case NECRO_TYPE_FOR:
    {
        NecroType* new_type = necro_uncurry(infer, type->for_all.type);
        if (new_type == type->for_all.type)
            return type;
        else
            return necro_create_for_all(infer, type->for_all.var, new_type);
    }

    default:
        return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
    }
}

//=====================================================
// Find
//=====================================================
NecroType* necro_find(NecroInfer* infer, NecroType* type)
{
    NecroType* prev = type;
    while (type != NULL && type->type == NECRO_TYPE_VAR)
    {
        // TRACE_TYPE("necro_find var: %d\n", type->var.var.id.id);
        prev = type;
        type = necro_env_get(infer, type->var.var);
    }
    if (type == NULL)
        return prev;
    else
        return type;
}

bool necro_is_bound_in_scope(NecroInfer* infer, NecroType* type, NecroScope* scope)
{
    if (type->type != NECRO_TYPE_VAR)
        return false;
    if (type->var.var.scope == NULL)
        return false;
    NecroScope* current_scope = scope;
    while (current_scope != NULL)
    {
        if (current_scope == type->var.var.scope)
            return true;
        else
            current_scope = current_scope->parent;
    }
    return false;
}

//=====================================================
// Unify
//=====================================================
// bool necro_occurs_with_name(NecroInfer* infer, NecroID name, NecroType* type)
// {
//     if (type == NULL)
//         return false;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//     {
//         NecroType* prev = type;
//         while (type != NULL && type->type == NECRO_TYPE_VAR)
//         {
//             if (type->var.var.id.id == name.id)
//                 return true;
//             prev = type;
//             type = necro_env_get(infer, type->var.var);
//         }
//         if (type == NULL)
//             type = prev;
//         if (type->type == NECRO_TYPE_VAR)
//             return false;
//         else
//             return necro_occurs_with_name(infer, name, type);
//     }
//     case NECRO_TYPE_APP:  return necro_occurs_with_name(infer, name, type->app.type1) || necro_occurs_with_name(infer, name, type->app.type2);
//     case NECRO_TYPE_FUN:  return necro_occurs_with_name(infer, name, type->fun.type1) || necro_occurs_with_name(infer, name, type->fun.type2);
//     // case NECRO_TYPE_CON:  return type->con.con.symbol.id == name.id         || necro_occurs(infer, name, type->con.args);
//     // No occurs checks on constructors?!?!?!
//     case NECRO_TYPE_CON:  return necro_occurs_with_name(infer, name, type->con.args);
//     case NECRO_TYPE_LIST: return necro_occurs_with_name(infer, name, type->list.item) || necro_occurs_with_name(infer, name, type->list.next);
//     case NECRO_TYPE_FOR:  return necro_infer_error(infer, type, "Found polytype in occurs check!");
//     default:              return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
//     }
// }

// bool necro_occurs(NecroInfer* infer, NecroType* var, NecroType* type)
// {
//     assert(infer != NULL);
//     assert(var != NULL);
//     assert(var->type == NECRO_TYPE_VAR);
//     if (type == NULL)
//         return false;
//     NecroType* current_var = var;
//     while (current_var != NULL && current_var->type == NECRO_TYPE_VAR)
//     {
//         if (necro_occurs_with_name(infer, current_var->var.var.id, type))
//             return true;
//         current_var = necro_env_get(infer, current_var->var.var);
//     }
//     return false;
// }

bool necro_occurs(NecroInfer* infer, NecroType* type_var, NecroType* type)
{
    if (type == NULL)
        return false;
    NecroVar name = type_var->var.var;
    type          = necro_find(infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return name.id.id == type->var.var.id.id;
    case NECRO_TYPE_APP:  return necro_occurs(infer, type_var, type->app.type1) || necro_occurs(infer, type_var, type->app.type2);
    case NECRO_TYPE_FUN:  return necro_occurs(infer, type_var, type->fun.type1) || necro_occurs(infer, type_var, type->fun.type2);
    // case NECRO_TYPE_CON:  return type->con.con.symbol.id == name.id         || necro_occurs(infer, name, type->con.args);
    // No occurs checks on constructors?!?!?!
    case NECRO_TYPE_CON:  return necro_occurs(infer, type_var, type->con.args);
    case NECRO_TYPE_LIST: return necro_occurs(infer, type_var, type->list.item) || necro_occurs(infer, type_var, type->list.next);
    case NECRO_TYPE_FOR:  return necro_infer_error(infer, type, "Found polytype in occurs check!");
    default:              return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
    }
}

// TODO: Type to string function
void necro_rigid_type_variable_error(NecroInfer* infer, NecroVar type_var, NecroType* type)
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
        type_name = necro_id_as_character_string(infer, type->var.var.id);
    else
        assert(false);
    const char* var_name = necro_id_as_character_string(infer, type_var.id);
    necro_infer_error(infer, type, "Couldn't match type \'%s\' with type \'%s\'.\n\'%s\' is a rigid type variable bound by a type signature.", var_name, type_name, var_name);
}

inline void necro_unify_var(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_VAR);
    if (type2 == NULL)
    {
        necro_infer_error(infer, type1, "Mismatched arities");
        return;
    }
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
    {
        NecroType* current_type2 = type2;
        if (type1->var.var.id.id == type2->var.var.id.id)
            return;
        else if (type1->var.is_rigid && type2->var.is_rigid)
            necro_rigid_type_variable_error(infer, type1->var.var, type2);
        else if (necro_occurs(infer, type1, type2))
            necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_id_as_character_string(infer, type1->var.var.id));
        else if (type1->var.is_rigid)
            necro_env_set(infer, type2->var.var, type1);
        else if (type2->var.is_rigid)
            necro_env_set(infer, type1->var.var, type2);
        else if (necro_is_bound_in_scope(infer, type1, scope))
            necro_env_set(infer, type2->var.var, type1);
        else
            necro_env_set(infer, type1->var.var, type2);
        return;
    }
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (type1->var.is_rigid)
        {
            necro_rigid_type_variable_error(infer, type1->var.var, type2);
            return;
        }
        if (necro_occurs(infer, type1, type2))
        {
            necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_id_as_character_string(infer, type1->var.var.id));
            return;
        }
        necro_env_set(infer, type1->var.var, type2);
        return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeVar with type args list."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_app(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_APP);
    if (type2 == NULL)
    {
        necro_infer_error(infer, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol));
        return;
    }
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            necro_rigid_type_variable_error(infer, type2->var.var, type1);
        else if (necro_occurs(infer, type2, type1))
            necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
        else
            necro_env_set(infer, type2->var.var, type1);
        return;
    case NECRO_TYPE_APP:
        necro_unify(infer, type1->app.type1, type2->app.type1, scope);
        necro_unify(infer, type1->app.type2, type2->app.type2, scope);
        return;
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_curry_con(infer, type2);
        if (uncurried_con == NULL)
        {
            necro_infer_error(infer, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type2->con.con.symbol));
        }
        else
        {
            necro_unify_app(infer, type1, uncurried_con, scope);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, type1, "Attempting to unify TypeApp with (->)."); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeApp with type args list."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_fun(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_FUN);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            necro_rigid_type_variable_error(infer, type2->var.var, type1);
        else if (necro_occurs(infer, type2, type1))
            necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
        else
            necro_env_set(infer, type2->var.var, type1);
        return;
    case NECRO_TYPE_FUN:  necro_unify(infer, type1->fun.type1, type2->fun.type1, scope); necro_unify(infer, type1->fun.type2, type2->fun.type2, scope); return;
    case NECRO_TYPE_APP:  necro_infer_error(infer, type1, "Attempting to unify (->) with TypeApp."); return;
    case NECRO_TYPE_CON:  necro_infer_error(infer, type1, "Attempting to unify (->) with TypeCon (%s)", necro_intern_get_string(infer->intern, type2->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify (->) with type args list."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_con(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_CON);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            necro_rigid_type_variable_error(infer, type2->var.var, type1);
        else if (necro_occurs(infer, type2, type1))
            necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
        else
            necro_env_set(infer, type2->var.var, type1);
        return;
    case NECRO_TYPE_CON:
        if (type1->con.con.symbol.id != type2->con.con.symbol.id)
        {
            necro_infer_error(infer, type1, "Attempting to unify two different types, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
        }
        else if (type1->con.arity != type2->con.arity)
        {
            necro_infer_error(infer, type1, "Mismatched arities, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
        }
        else
        {
            type1 = type1->con.args;
            type2 = type2->con.args;
            while (type1 != NULL && type2 != NULL)
            {
                if (type1 == NULL || type2 == NULL)
                {
                    necro_infer_error(infer, type1, "Mismatched arities, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
                    return;
                }
                assert(type1->type == NECRO_TYPE_LIST);
                assert(type2->type == NECRO_TYPE_LIST);
                necro_unify(infer, type1->list.item, type2->list.item, scope);
                type1 = type1->list.next;
                type2 = type2->list.next;
            }
        }
        return;
    case NECRO_TYPE_APP:
    {
        NecroType* uncurried_con = necro_curry_con(infer, type1);
        if (uncurried_con == NULL)
        {
            necro_infer_error(infer, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol));
        }
        else
        {
            necro_unify_app(infer, uncurried_con, type2, scope);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, type1, "Attempting to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Attempted to unify polytype."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

// Can only unify a polymorphic type in first position
void necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(type1 != NULL);
    assert(type2 != NULL);
    type1 = necro_find(infer, type1);
    type2 = necro_find(infer, type2);
    if (type1 == type2)
        return;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  necro_unify_var(infer, type1, type2, scope); return;
    case NECRO_TYPE_APP:  necro_unify_app(infer, type1, type2, scope); return;
    case NECRO_TYPE_FUN:  necro_unify_fun(infer, type1, type2, scope); return;
    case NECRO_TYPE_CON:  necro_unify_con(infer, type1, type2, scope); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Found Type args list in necro_unify"); return;
    // case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Found Polytype in necro_unify"); return;
    case NECRO_TYPE_FOR:
    {
        while (type1->type == NECRO_TYPE_FOR)
            type1 = type1->for_all.type;
        necro_unify(infer, type1, type2, scope);
        return;
    }
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

//=====================================================
// Inst
//=====================================================
typedef struct NecroInstSub
{
    NecroVar             var_to_replace;
    NecroType*           new_name;
    struct NecroInstSub* next;
} NecroInstSub;

NecroInstSub* necro_create_inst_sub(NecroInfer* infer, NecroVar var_to_replace, NecroInstSub* next)
{
    NecroInstSub* sub = necro_paged_arena_alloc(&infer->arena, sizeof(NecroInstSub));
    *sub              = (NecroInstSub)
    {
        .var_to_replace = var_to_replace,
        .new_name       = necro_new_name(infer),
        .next           = next,
    };
    return sub;
}

NecroType* necro_inst_go(NecroInfer* infer, NecroType* type, NecroInstSub* subs, NecroScope* scope)
{
    assert(infer != NULL);
    if (type == NULL)
        return NULL;
    type = necro_find(infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        while (subs != NULL)
        {
            if (type->var.var.id.id == subs->var_to_replace.id.id)
            {
                return subs->new_name;
            }
            subs = subs->next;
        }
        // assert(!type->var.is_rigid);
        return type;
    }
    case NECRO_TYPE_APP:  return necro_create_type_app(infer, necro_inst_go(infer, type->app.type1, subs, scope), necro_inst_go(infer, type->app.type2, subs, scope));
    case NECRO_TYPE_FUN:  return necro_create_type_fun(infer, necro_inst_go(infer, type->fun.type1, subs, scope), necro_inst_go(infer, type->fun.type2, subs, scope));
    case NECRO_TYPE_CON:  return necro_create_type_con(infer, type->con.con, necro_inst_go(infer, type->con.args, subs, scope), type->con.arity);
    case NECRO_TYPE_LIST: return necro_create_type_list(infer, necro_inst_go(infer, type->list.item, subs, scope), necro_inst_go(infer, type->list.next, subs, scope));
    case NECRO_TYPE_FOR:  return necro_infer_error(infer, type, "Compiler bug: Found Polytype in necro_inst_go");
    default:              return necro_infer_error(infer, type, "Compiler bug: Non-existent type %d found in necro_inst.", type->type);
    }
}

NecroType* necro_inst(NecroInfer* infer, NecroType* type, NecroScope* scope)
{
    assert(infer != NULL);
    assert(type != NULL);
    type = necro_find(infer, type);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs         = necro_create_inst_sub(infer, current_type->for_all.var, subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_inst_go(infer, current_type, subs, scope);
    return result;
}

//=====================================================
// Gen
//=====================================================
typedef struct NecroGenSub
{
    struct NecroGenSub* next;
    NecroVar            var_to_replace;
    NecroType*          type_var;
    NecroType*          for_all;
} NecroGenSub;

typedef struct
{
    NecroType*   type;
    NecroGenSub* subs;
    NecroGenSub* sub_tail;
} NecroGenResult;

NecroGenResult necro_gen_go(NecroInfer* infer, NecroType* type, NecroGenResult prev_result, NecroScope* scope)
{
    assert(infer != NULL);
    if (necro_is_infer_error(infer))
    {
        prev_result.type = NULL;
        return prev_result;
    }
    if (type == NULL)
    {
        prev_result.type = NULL;
        return prev_result;
    }

    NecroType* top = necro_find(infer, type);
    if (top->type == NECRO_TYPE_VAR && top->var.is_rigid)
    {
        // necro_infer_error(infer, type, "Compiler bug: Attempting to generalize a rigid type variable!");
        // return (NecroGenResult) { NULL, NULL, NULL };
        prev_result.type = top;
        return prev_result;
    }

    if (necro_is_bound_in_scope(infer, type, scope))
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
            // necro_infer_error(infer, type, "Compiler bug: Attempting to generalize a rigid type variable!");
            // return (NecroGenResult) { NULL, NULL, NULL };
            prev_result.type = type;
            return prev_result;
        }
        NecroType* bound_type = necro_env_get(infer, type->var.var);
        if (bound_type != NULL)
        {
            return necro_gen_go(infer, bound_type, prev_result, scope);
        }
        else
        {
            NecroGenSub* subs = prev_result.subs;
            while (subs != NULL)
            {
                if (subs->var_to_replace.id.id == type->var.var.id.id)
                {
                    prev_result.type = subs->type_var;
                    return prev_result;
                }
                subs = subs->next;
            }
            NecroGenSub* sub       = necro_paged_arena_alloc(&infer->arena, sizeof(NecroGenSub));
            NecroType*   type_var  = necro_new_name(infer);
            type_var->var.is_rigid = true;
            NecroType*   for_all   = necro_create_for_all(infer, type_var->var.var, NULL);
            *sub                   = (NecroGenSub)
            {
                .next           = NULL,
                .var_to_replace = type->var.var,
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
        result2.type           = necro_create_type_app(infer, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_FUN:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->fun.type1, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(infer, type->fun.type2, result1, scope);
        result2.type           = necro_create_type_fun(infer, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_LIST:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->list.item, prev_result, scope);
        NecroGenResult result2 = necro_gen_go(infer, type->list.next, result1, scope);
        result2.type           = necro_create_type_list(infer, result1.type, result2.type);
        return result2;
    }
    case NECRO_TYPE_CON:
    {
        NecroGenResult result = necro_gen_go(infer, type->con.args, prev_result, scope);
        result.type           = necro_create_type_con(infer, type->con.con, result.type, type->con.arity);
        return result;
    }
    case NECRO_TYPE_FOR: necro_infer_error(infer, type, "Compiler bug: Found Polytype in necro_gen"); return (NecroGenResult) { NULL, NULL, NULL };
    default:             necro_infer_error(infer, type, "Compiler bug: Non-existent type %d found in necro_gen.", type->type); return (NecroGenResult) { NULL, NULL, NULL };
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
                return head;
            }
        }
    }
    else
    {
        return result.type;
    }
}

//=====================================================
// Type Sanity
//=====================================================
void necro_check_type_sanity(NecroInfer* infer, NecroType* type)
{
    if (necro_is_infer_error(infer))
        return;
    if (type == NULL)
        return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        break;
    case NECRO_TYPE_FUN:
        if (type->fun.type1 == NULL)
        {
            necro_infer_error(infer, type, "Malformed (->) in type");
            break;
        }
        if (type->fun.type2 == NULL)
        {
            necro_infer_error(infer, type, "Malformed (->) in type");
            break;
        }
        necro_check_type_sanity(infer, type->fun.type1);
        necro_check_type_sanity(infer, type->fun.type2);
        break;
    case NECRO_TYPE_APP:
        if (type->app.type1 == NULL)
        {
            necro_infer_error(infer, type, "Malformed Type Application");
            break;
        }
        if (type->app.type2 == NULL)
        {
            necro_infer_error(infer, type, "Malformed Type Application");
            break;
        }
        necro_check_type_sanity(infer, type->app.type1);
        necro_check_type_sanity(infer, type->app.type2);
        break;
    case NECRO_TYPE_LIST:
        necro_check_type_sanity(infer, type->list.item);
        necro_check_type_sanity(infer, type->list.next);
        break;
    case NECRO_TYPE_CON:
        // Check Con against type sig in symtable
        if (type->con.con.id.id != 0 && necro_symtable_get(infer->symtable, type->con.con.id)->type != NULL)
        {
            assert(necro_symtable_get(infer->symtable, type->con.con.id)->type->type == NECRO_TYPE_CON);
            assert(type->con.con.symbol.id == necro_symtable_get(infer->symtable, type->con.con.id)->type->con.con.symbol.id);
            if ( type->con.arity != necro_symtable_get(infer->symtable, type->con.con.id)->type->con.arity)
            {
                necro_infer_error(infer, type, "Mismatched arity for type %s. Expected arity: %d, found arity %d", necro_intern_get_string(infer->intern, type->con.con.symbol), necro_symtable_get(infer->symtable, type->con.con.id)->type->con.arity, type->con.arity);
                break;
            }
        }
        if (necro_type_list_count(type->con.args) != type->con.arity)
        {
            necro_infer_error(infer, type, "Mismatched arity for type %s. Expected arity: %d, found arity %d", necro_intern_get_string(infer->intern, type->con.con.symbol), type->con.arity, necro_type_list_count(type->con.args));
            break;
        }
        necro_check_type_sanity(infer, type->con.args);
        break;
    case NECRO_TYPE_FOR:
        necro_check_type_sanity(infer, type->for_all.type);
        break;
    default: necro_infer_error(infer, type, "Compiler bug (necro_check_type_sanity): Unrecognized type: %d", type->type); break;
    }
}

bool necro_types_match(NecroType* type1, NecroType* type2)
{
    if (type1 == NULL && type2 == NULL)
        return true;
    else if (type1 != NULL && type2 == NULL)
        return false;
    else if (type1 == NULL && type2 != NULL)
        return false;
    if (type1->type != type2->type)
        return false;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  return type1->var.var.id.id == type2->var.var.id.id;
    case NECRO_TYPE_APP:  return necro_types_match(type1->app.type1, type2->app.type1) && necro_types_match(type1->app.type2, type2->app.type2);
    case NECRO_TYPE_FUN:  return necro_types_match(type1->fun.type1, type2->fun.type1) && necro_types_match(type1->fun.type2, type2->fun.type2);
    case NECRO_TYPE_CON:  return type1->con.con.symbol.id == type2->con.con.symbol.id && necro_types_match(type1->con.args, type2->con.args);
    case NECRO_TYPE_LIST: return necro_types_match(type1->list.item, type2->list.item) && necro_types_match(type1->list.next, type2->list.next);
    case NECRO_TYPE_FOR:  return type1->for_all.var.id.id == type2->var.var.id.id && necro_types_match(type1->for_all.type, type2->for_all.type);
    default:
        printf("Compiler bug: Unrecognized type: %d", type1->type);
        assert(false);
        return false;
    }
}

//=====================================================
// Printing
//=====================================================
const char* necro_id_as_character_string(NecroInfer* infer, NecroID id)
{
    size_t length = 0;
    size_t n      = id.id;
    while (n > 0)
    {
        n /= 26;
    }
    char* buffer = necro_paged_arena_alloc(&infer->arena, (length + 1) * sizeof(char));
    n = id.id;
    size_t i = 0;
    while (n > 0)
    {
        buffer[i] = (n % 26) + 97;
        n /= 26;
        ++i;
    }
    buffer[i] = '\0';
    return buffer;
}

void necro_print_id_as_characters(NecroID id)
{
    size_t n = id.id;
    while (n > 0)
    {
        char c = n % 26;
        printf("%c", c + 97);
        n /= 26;
    }
}

char* necro_snprintf_id_as_characters(NecroID id, char* buffer, size_t buffer_size)
{
    size_t n = id.id;
    while (n > 0)
    {
        *buffer = (n % 26) + 97;
        n /= 26;
        buffer++;
    }
    return buffer;
}

void necro_print_type(NecroType* type, uint32_t whitespace, NecroIntern* intern)
{
    if (type == NULL)
        return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        print_white_space(whitespace);
        printf("TypeVar, name: ");
        necro_print_id_as_characters(type->var.var.id);
        printf(", id: %d\n", type->var.var.id.id);
        break;

    case NECRO_TYPE_APP:
        print_white_space(whitespace);
        printf("App\n");
        print_white_space(whitespace);
        printf("(\n");
        necro_print_type(type->app.type1, whitespace + 4, intern);
        necro_print_type(type->app.type2, whitespace + 4, intern);
        print_white_space(whitespace);
        printf(")\n");
        break;

    case NECRO_TYPE_FUN:
        necro_print_type(type->fun.type1, whitespace + 4, intern);
        print_white_space(whitespace);
        printf("->\n");
        necro_print_type(type->fun.type2, whitespace + 4, intern);
        break;

    case NECRO_TYPE_CON:
        print_white_space(whitespace);
        printf("Con, name: %s, id: %d\n", necro_intern_get_string(intern, type->con.con.symbol), type->con.con.id.id);
        if (necro_type_list_count(type->con.args) > 0)
        {
            print_white_space(whitespace);
            printf("(\n");
            necro_print_type(type->con.args, whitespace + 4, intern);
            print_white_space(whitespace);
            printf(")\n");
        }
        break;

    case NECRO_TYPE_LIST:
        necro_print_type(type->list.item, whitespace, intern);
        necro_print_type(type->list.next, whitespace, intern);
        break;

    case NECRO_TYPE_FOR:
        printf("forall ");
        necro_print_id_as_characters(type->for_all.var.id);
        printf(".");
        necro_print_type(type->for_all.type, whitespace, intern);
        break;

    default:
        printf("Compiler bug: Unrecognized type: %d", type->type);
        assert(false);
    }
}

bool necro_is_simple_type(NecroType* type)
{
    return type->type == NECRO_TYPE_VAR || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0);
}

void necro_print_type_sig_go(NecroType* type, NecroIntern* intern);

void necro_print_type_sig_go_maybe_with_parens(NecroType* type, NecroIntern* intern)
{
    if (!necro_is_simple_type(type))
        printf("(");
    necro_print_type_sig_go(type, intern);
    if (!necro_is_simple_type(type))
        printf(")");
}

bool necro_print_tuple_sig(NecroType* type, NecroIntern* intern)
{
    NecroSymbol con_symbol = type->con.con.symbol;
    const char* con_string = necro_intern_get_string(intern, type->con.con.symbol);
    if (con_string[0] != '(')
        return false;
    NecroType* current_element = type->con.args;

    // 2
    NecroSymbol two_symbol = necro_intern_string(intern, "(,)");
    if (con_symbol.id == two_symbol.id)
    {
        printf("(");
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(")");
        return true;
    }

    // 3
    NecroSymbol three_symbol = necro_intern_string(intern, "(,,)");
    if (con_symbol.id == three_symbol.id)
    {
        printf("(");
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(")");
        return true;
    }

    // 4
    NecroSymbol four_symbol = necro_intern_string(intern, "(,,,)");
    if (con_symbol.id == four_symbol.id)
    {
        printf("(");
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(")");
        return true;
    }

    // 5
    NecroSymbol five_symbol = necro_intern_string(intern, "(,,,,)");
    if (con_symbol.id == five_symbol.id)
    {
        printf("(");
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(", ");
        current_element = current_element->list.next;
        necro_print_type_sig_go(current_element->list.item, intern);
        printf(")");
        return true;
    }

    return false;
}

void necro_print_type_sig_go(NecroType* type, NecroIntern* intern)
{
    if (type == NULL)
        return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        necro_print_id_as_characters(type->var.var.id);
        // if (type->var.is_rigid)
        // {
        //     printf(" [rigid]");
        // }
        break;

    case NECRO_TYPE_APP:
        necro_print_type_sig_go_maybe_with_parens(type->app.type1, intern);
        printf(" ");
        necro_print_type_sig_go_maybe_with_parens(type->app.type2, intern);
        break;

    case NECRO_TYPE_FUN:
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            printf("(");
        necro_print_type_sig_go(type->fun.type1, intern);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            printf(")");
        printf(" -> ");
        necro_print_type_sig_go(type->fun.type2, intern);
        break;

    case NECRO_TYPE_CON:
    {
        if (necro_print_tuple_sig(type, intern))
            break;
        bool has_args = necro_type_list_count(type->con.args) > 0;
        printf("%s", necro_intern_get_string(intern, type->con.con.symbol));
        if (has_args)
        {
            printf(" ");
            necro_print_type_sig_go(type->con.args, intern);
        }
        break;
    }

    case NECRO_TYPE_LIST:
        necro_print_type_sig_go_maybe_with_parens(type->list.item, intern);
        if (type->list.next != NULL)
        {
            printf(" ");
            necro_print_type_sig_go(type->list.next, intern);
        }
        break;

    case NECRO_TYPE_FOR:
        printf("forall ");
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            necro_print_id_as_characters(type->for_all.var.id);
            printf(" ");
            type = type->for_all.type;
        }
        necro_print_id_as_characters(type->for_all.var.id);
        printf(". ");
        necro_print_type_sig_go(type->for_all.type, intern);
        break;

    default:
        printf("Compiler bug: Unrecognized type: %d", type->type);
        assert(false);
    }
}

char* necro_snprintf_tuple_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length)
{
    NecroSymbol con_symbol = type->con.con.symbol;
    const char* con_string = necro_intern_get_string(intern, type->con.con.symbol);
    if (con_string[0] != ')')
        return NULL;
    NecroType* current_element = type->con.args;

    // 2
    NecroSymbol two_symbol = necro_intern_string(intern, "(,)");
    if (con_symbol.id == two_symbol.id)
    {
        buffer += snprintf(buffer, buffer_length, "(");
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ")");
        return buffer;
    }

    // 3
    NecroSymbol three_symbol = necro_intern_string(intern, "(,,)");
    if (con_symbol.id == three_symbol.id)
    {
        buffer += snprintf(buffer, buffer_length, "(");
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ")");
        return buffer;
    }

    // 4
    NecroSymbol four_symbol = necro_intern_string(intern, "(,,,)");
    if (con_symbol.id == four_symbol.id)
    {
        buffer += snprintf(buffer, buffer_length, "(");
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ")");
        return buffer;
    }

    // 5
    NecroSymbol five_symbol = necro_intern_string(intern, "(,,,,)");
    if (con_symbol.id == five_symbol.id)
    {
        buffer += snprintf(buffer, buffer_length, "(");
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ", ");
        current_element = current_element->list.next;
        buffer  = necro_snprintf_type_sig(current_element->list.item, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, ")");
        return buffer;
    }

    return NULL;
}

char* necro_snprintf_type_sig_go_maybe_with_parens(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length)
{
    size_t count = 0;
    char* new_buffer;
    if (!necro_is_simple_type(type))
        count = snprintf(buffer, buffer_length, "(");
    new_buffer = necro_snprintf_type_sig(type, intern, buffer + count, buffer_length);
    if (!necro_is_simple_type(type))
        count = snprintf(new_buffer, buffer_length, ")");
    return new_buffer + count;
}

char* necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length)
{
    if (type == NULL)
        return buffer;
    // size_t count = 0;
    // char*  new_buffer;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        return necro_snprintf_id_as_characters(type->var.var.id, buffer, buffer_length);

    case NECRO_TYPE_APP:
        buffer  = necro_snprintf_type_sig_go_maybe_with_parens(type->app.type1, intern, buffer, buffer_length);
        buffer += snprintf(buffer, buffer_length, " ");
        return necro_snprintf_type_sig_go_maybe_with_parens(type->app.type2, intern, buffer, buffer_length);

    case NECRO_TYPE_FUN:
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            buffer += snprintf(buffer, buffer_length, "(");
        buffer = necro_snprintf_type_sig(type->fun.type1, intern, buffer, buffer_length);
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            buffer += snprintf(buffer, buffer_length, ")");
        buffer += snprintf(buffer, buffer_length, " -> ");
        return necro_snprintf_type_sig(type->fun.type2, intern, buffer, buffer_length);

    case NECRO_TYPE_CON:
    {
        char* tuple_buffer = necro_snprintf_tuple_sig(type, intern, buffer, buffer_length);
        if (tuple_buffer != NULL)
            return tuple_buffer;
        bool has_args = necro_type_list_count(type->con.args) > 0;
        buffer += snprintf(buffer, buffer_length, "%s", necro_intern_get_string(intern, type->con.con.symbol));
        if (has_args)
        {
            buffer += snprintf(buffer, buffer_length, " ");
            return necro_snprintf_type_sig(type->con.args, intern, buffer, buffer_length);
        }
        else
        {
            return buffer;
        }
    }

    case NECRO_TYPE_LIST:
        buffer = necro_snprintf_type_sig_go_maybe_with_parens(type->list.item, intern, buffer, buffer_length);
        if (type->list.next != NULL)
        {
            buffer += snprintf(buffer, buffer_length, " ");
            return necro_snprintf_type_sig(type->list.next, intern, buffer, buffer_length);
        }
        else
        {
            return buffer;
        }

    case NECRO_TYPE_FOR:
        buffer += snprintf(buffer, buffer_length, "forall ");
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            buffer  = necro_snprintf_id_as_characters(type->for_all.var.id, buffer, buffer_length);
            buffer += snprintf(buffer, buffer_length, " ");
            type    = type->for_all.type;
        }
        buffer      = necro_snprintf_id_as_characters(type->for_all.var.id, buffer, buffer_length);
        buffer     += snprintf(buffer, buffer_length, ". ");
        return necro_snprintf_type_sig(type->for_all.type, intern, buffer, buffer_length);

    default:
        return buffer + snprintf(buffer, buffer_length, "Compiler bug: Unrecognized type: %d", type->type);
    }
}

void necro_print_type_sig(NecroType* type, NecroIntern* intern)
{
    if (type != NULL)
        necro_print_type_sig_go(type, intern);
    printf("\n");
}

void necro_print_test_result(bool passed, const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern)
{
    printf("\n%s :: ", test_name);
    necro_print_type_sig(type, intern);
    printf("%s :: ", test_name2);
    necro_print_type_sig(type2, intern);
    if (passed)
        printf(" (passed)\n");
    else
        printf(" (FAILED)\n");
}

void necro_print_type_test_result(const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern)
{
    bool passed = necro_types_match(type, type2);
    printf("\n%s :: ", test_name);
    necro_print_type_sig(type, intern);
    printf("%s :: ", test_name2);
    necro_print_type_sig(type2, intern);
    if (passed)
        printf(" (passed)\n");
    else
        printf(" (FAILED)\n");
}

void necro_print_env(NecroInfer* infer)
{
    printf("Env:\n[\n");
    for (size_t i = 0; i < infer->env.capacity; ++i)
    {
        if (infer->env.data[i] == NULL)
            continue;
        printf("    ");
        necro_print_id_as_characters((NecroID) { i });
        printf(" ==> ");
        necro_print_type_sig(infer->env.data[i], infer->intern);
    }
    printf("]\n");
}

//=====================================================
// Prim Types
//=====================================================
NecroType* necro_make_tuple_con(NecroInfer* infer, NecroType* types_list)
{
    size_t     tuple_count  = 0;
    NecroType* current_type = types_list;
    while (current_type != NULL)
    {
        assert(current_type->type == NECRO_TYPE_LIST);
        tuple_count++;
        current_type = current_type->list.next;
    }
    NecroSymbol symbol;
    switch (tuple_count)
    {
    case 2:  symbol = infer->prim_types.tuple_types.two.symbol;   break;
    case 3:  symbol = infer->prim_types.tuple_types.three.symbol; break;
    case 4:  symbol = infer->prim_types.tuple_types.four.symbol;  break;
    case 5:  symbol = infer->prim_types.tuple_types.five.symbol;  break;
    case 6:  symbol = infer->prim_types.tuple_types.six.symbol;   break;
    case 7:  symbol = infer->prim_types.tuple_types.seven.symbol; break;
    case 8:  symbol = infer->prim_types.tuple_types.eight.symbol; break;
    case 9:  symbol = infer->prim_types.tuple_types.nine.symbol;  break;
    case 10: symbol = infer->prim_types.tuple_types.ten.symbol;   break;
    default: return necro_infer_error(infer, NULL, "Tuple size too large: %d", tuple_count);
    }
    return necro_create_type_con(infer, (NecroCon) { .symbol = symbol }, types_list, tuple_count);
}

NecroType* necro_get_bin_op_type(NecroInfer* infer, NecroAST_BinOpType bin_op_type)
{
    switch (bin_op_type)
    {
    case NECRO_BIN_OP_ADD:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.add_type.id)->type;
    case NECRO_BIN_OP_SUB:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.sub_type.id)->type;
    case NECRO_BIN_OP_MUL:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.mul_type.id)->type;
    case NECRO_BIN_OP_DIV:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.div_type.id)->type;
    case NECRO_BIN_OP_MOD:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.mod_type.id)->type;
    case NECRO_BIN_OP_EQUALS:     return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.eq_type.id)->type;
    case NECRO_BIN_OP_NOT_EQUALS: return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.not_eq_type.id)->type;
    case NECRO_BIN_OP_GT:         return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.gt_type.id)->type;
    case NECRO_BIN_OP_LT:         return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.lt_type.id)->type;
    case NECRO_BIN_OP_GTE:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.gte_type.id)->type;
    case NECRO_BIN_OP_LTE:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.lte_type.id)->type;
    case NECRO_BIN_OP_AND:        return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.and_type.id)->type;
    case NECRO_BIN_OP_OR:         return necro_symtable_get(infer->symtable, infer->prim_types.bin_op_types.or_type.id)->type;
    default: return necro_infer_error(infer, NULL, "bin op not implemented in type checker!: %d", bin_op_type);
    // case NECRO_BIN_OP_COLON,
	// case NECRO_BIN_OP_DOUBLE_COLON,
	// case NECRO_BIN_OP_LEFT_SHIFT,
	// case NECRO_BIN_OP_RIGHT_SHIFT,
	// case NECRO_BIN_OP_PIPE,
	// case NECRO_BIN_OP_FORWARD_PIPE,
	// case NECRO_BIN_OP_BACK_PIPE,
    // case NECRO_BIN_OP_DOT,
    // case NECRO_BIN_OP_DOLLAR,
    // case NECRO_BIN_OP_BIND_RIGHT,
    // case NECRO_BIN_OP_BIND_LEFT,
    // case NECRO_BIN_OP_DOUBLE_EXCLAMATION,
    // case NECRO_BIN_OP_APPEND,
    // case NECRO_BIN_OP_COUNT,
    // case NECRO_BIN_OP_UNDEFINED = NECRO_BIN_OP_COUNT
    }
}

NecroType* necro_make_con_1(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1)
{
    NecroType* lst1 = necro_create_type_list(infer, arg1, NULL);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1 , 1);
}

NecroType* necro_make_con_2(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2)
{
    NecroType* lst2 = necro_create_type_list(infer, arg2, NULL);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 2);
}

NecroType* necro_make_con_3(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3)
{
    NecroType* lst3 = necro_create_type_list(infer, arg3, NULL);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 3);
}

NecroType* necro_make_con_4(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4)
{
    NecroType* lst4 = necro_create_type_list(infer, arg4, NULL);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 4);
}

NecroType* necro_make_con_5(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5)
{
    NecroType* lst5 = necro_create_type_list(infer, arg5, NULL);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 5);
}

NecroType* necro_make_con_6(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6)
{
    NecroType* lst6 = necro_create_type_list(infer, arg6, NULL);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 6);
}

NecroType* necro_make_con_7(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7)
{
    NecroType* lst7 = necro_create_type_list(infer, arg7, NULL);
    NecroType* lst6 = necro_create_type_list(infer, arg6, lst7);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 7);
}

NecroType* necro_make_con_8(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8)
{
    NecroType* lst8 = necro_create_type_list(infer, arg8, NULL);
    NecroType* lst7 = necro_create_type_list(infer, arg7, lst8);
    NecroType* lst6 = necro_create_type_list(infer, arg6, lst7);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 8);
}

NecroType* necro_make_con_9(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9)
{
    NecroType* lst9 = necro_create_type_list(infer, arg9, NULL);
    NecroType* lst8 = necro_create_type_list(infer, arg8, lst9);
    NecroType* lst7 = necro_create_type_list(infer, arg7, lst8);
    NecroType* lst6 = necro_create_type_list(infer, arg6, lst7);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 9);
}

NecroType* necro_make_con_10(NecroInfer* infer, NecroSymbol con_symbol, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10)
{
    NecroType* lst10 = necro_create_type_list(infer, arg10, NULL);
    NecroType* lst9  = necro_create_type_list(infer, arg9, lst10);
    NecroType* lst8  = necro_create_type_list(infer, arg8, lst9);
    NecroType* lst7  = necro_create_type_list(infer, arg7, lst8);
    NecroType* lst6  = necro_create_type_list(infer, arg6, lst7);
    NecroType* lst5  = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4  = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3  = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2  = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1  = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, (NecroCon) { con_symbol, 0 }, lst1, 10);
}

//=====================================================
// Testing
//=====================================================
void necro_test_type()
{
    necro_announce_phase("NecroType");
}


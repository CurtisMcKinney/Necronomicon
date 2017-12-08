/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "type.h"

// TODO / NOTE:
//  http://tomasp.net/coeffects/
//  http://tomasp.net/academic/papers/structural/
//  https://www.slideshare.net/tomaspfb/talk-38641264
//  eval_type_to_normal_form

NecroInfer necro_create_infer(NecroIntern* intern)
{
    return (NecroInfer)
    {
        .intern     = intern,
        .arena      = necro_create_paged_arena(),
        .error      = necro_create_error(),
        .highest_id = 0,
    };
}

void necro_destroy_infer(NecroInfer* infer)
{
    if (infer == NULL)
        return;
    necro_destroy_paged_arena(&infer->arena);
    infer->intern = NULL;
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

    size_t count1 = necro_verror(&infer->error, type->source_loc, error_message, args);
    size_t count2 = snprintf(infer->error.error_message + count1, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n    Found in type:\n        ");
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
    return necro_paged_arena_alloc(&infer->arena, sizeof(NecroType));
}

NecroType* necro_create_type_var(NecroInfer* infer, NecroVar var)
{
    if (var.id.id > infer->highest_id)
        infer->highest_id = var.id.id + 1;
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_VAR;
    type->var       = (NecroTypeVar)
    {
        .var = var
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
    NecroVar var = (NecroVar) { .id = { infer->highest_id } };
    return necro_create_type_var(infer, var);
}

//=====================================================
// NecroTypeEnv
//=====================================================
NecroType* necro_env_get(NecroInfer* infer, NecroVar var)
{
    if (infer->env.count > var.id.id)
    {
        return infer->env.data[var.id.id];
    }
    else
    {
        return NULL;
    }
}

void necro_env_set(NecroInfer* infer, NecroVar var, NecroType* type)
{
    if (infer->env.count <= var.id.id)
    {
        size_t      new_size = next_highest_pow_of_2(var.id.id) * 2;
        NecroType** new_data = realloc(infer->env.data, new_size * sizeof(NecroType*));
        if (new_data == NULL)
        {
            if (infer->env.data != NULL)
                free(infer->env.data);
            fprintf(stderr, "Malloc returned NULL in infer env reallocation!\n");
            exit(1);
        }
        infer->env.data = new_data;
    }
    assert(infer->env.data != NULL);
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
// Unify
//=====================================================
bool necro_occurs(NecroInfer* infer, NecroID name, NecroType* type)
{
    if (type == NULL)
        return false;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return type->var.var.id.id == name.id;
    case NECRO_TYPE_APP:  return necro_occurs(infer, name, type->app.type1) || necro_occurs(infer, name, type->app.type2);
    case NECRO_TYPE_FUN:  return necro_occurs(infer, name, type->fun.type1) || necro_occurs(infer, name, type->fun.type2);
    // case NECRO_TYPE_CON:  return type->con.con.symbol.id == name.id         || necro_occurs(infer, name, type->con.args);
    // No occurs checks on constructors?!?!?!
    case NECRO_TYPE_CON:  return necro_occurs(infer, name, type->con.args);
    case NECRO_TYPE_LIST: return necro_occurs(infer, name, type->list.item) || necro_occurs(infer, name, type->list.next);
    case NECRO_TYPE_FOR:  return necro_infer_error(infer, type, "Found polytype in occurs check!");
    default:              return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
    }
}

inline void  necro_unify_var(NecroInfer* infer, NecroType* type1, NecroType* type2)
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
        *type1 = *type2;
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (necro_occurs(infer, type1->var.var.id, type2))
            necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_id_as_character_string(infer, type1->var.var.id));
        *type1 = *type2;
        return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeVar with type args list."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_app(NecroInfer* infer, NecroType* type1, NecroType* type2)
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
        if (necro_occurs(infer, type2->var.var.id, type1))
        {
            necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
        }
        else
        {
            *type2 = *type1;
        }
        return;
    case NECRO_TYPE_APP:
        necro_unify(infer, type1->app.type1, type2->app.type1);
        necro_unify(infer, type1->app.type2, type2->app.type2);
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
            necro_unify_app(infer, type1, uncurried_con);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, type1, "Attempting to unify TypeApp with (->)."); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeApp with type args list."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_fun(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_FUN);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (necro_occurs(infer, type2->var.var.id, type1))
        {
            necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
        }
        else
        {
            *type2 = *type1;
        }
        return;
    case NECRO_TYPE_FUN:  necro_unify(infer, type1->fun.type1, type2->fun.type1); necro_unify(infer, type1->fun.type2, type2->fun.type2); return;
    case NECRO_TYPE_APP:  necro_infer_error(infer, type1, "Attempting to unify (->) with TypeApp."); return;
    case NECRO_TYPE_CON:  necro_infer_error(infer, type1, "Attempting to unify (->) with TypeCon (%s)", necro_intern_get_string(infer->intern, type2->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify (->) with type args list."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_con(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_CON);
    assert(type2 != NULL);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (necro_occurs(infer, type2->var.var.id, type1))
        {
            necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
        }
        else
        {
            *type2 = *type1;
        }
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
                necro_unify(infer, type1->list.item, type2->list.item);
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
            necro_unify_app(infer, uncurried_con, type2);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, type1, "Attempting to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type2->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type2->con.con.symbol)); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Attempted to unify polytype."); return;
    default:              necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

// Note: Mutates both types
void necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(type1 != NULL);
    assert(type2 != NULL);
    if (type1 == type2)
        return;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  necro_unify_var(infer, type1, type2); return;
    case NECRO_TYPE_APP:  necro_unify_app(infer, type1, type2); return;
    case NECRO_TYPE_FUN:  necro_unify_fun(infer, type1, type2); return;
    case NECRO_TYPE_CON:  necro_unify_con(infer, type1, type2); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, type1, "Compiler bug: Found Type args list in necro_unify"); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, type1, "Compiler bug: Found Polytype unify"); return;
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

NecroType* necro_inst_go(NecroInfer* infer, NecroType* type, NecroInstSub* subs)
{
    assert(infer != NULL);
    if (type == NULL || subs == NULL)
        return NULL;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        while (subs != NULL)
        {
            if (type->var.var.id.id == subs->var_to_replace.id.id)
                return subs->new_name;
        }
        return necro_create_type_var(infer, type->var.var);
    case NECRO_TYPE_APP:  return necro_create_type_app(infer, necro_inst_go(infer, type->app.type1, subs), necro_inst_go(infer, type->app.type2, subs));
    case NECRO_TYPE_FUN:  return necro_create_type_fun(infer, necro_inst_go(infer, type->fun.type1, subs), necro_inst_go(infer, type->fun.type2, subs));
    case NECRO_TYPE_CON:  return necro_create_type_con(infer, type->con.con, necro_inst_go(infer, type->con.args, subs), type->con.arity);
    case NECRO_TYPE_LIST: return necro_create_type_list(infer, necro_inst_go(infer, type->list.item, subs), necro_inst_go(infer, type->list.next, subs));
    case NECRO_TYPE_FOR:  return necro_infer_error(infer, type, "Compiler bug: Found Polytype in necro_inst_go");
    default:              return necro_infer_error(infer, type, "Compiler bug: Non-existent type %d found in necro_inst.", type->type);
    }
}

// Note: Deep copies Type
NecroType* necro_inst(NecroInfer* infer, NecroType* type)
{
    assert(infer != NULL);
    assert(type != NULL);
    assert(type->type == NECRO_TYPE_FOR);
    NecroType*    current_type = type;
    NecroInstSub* subs         = NULL;
    while (current_type->type == NECRO_TYPE_FOR)
    {
        subs         = necro_create_inst_sub(infer, current_type->for_all.var, subs);
        current_type = current_type->for_all.type;
    }
    return necro_inst_go(infer, current_type, subs);
}

//=====================================================
// Gen
//=====================================================
typedef struct
{
    NecroType* type;
    NecroType* maybe_for_all;
} NecroGenResult;

NecroType* merge_for_all(NecroType* for_all1, NecroType* for_all2)
{
    if (for_all2 == NULL)
        return for_all1;
    else if (for_all1 == NULL)
        return for_all2;
    else
        return for_all1->for_all.type = for_all2;
}

NecroGenResult necro_gen_go(NecroInfer* infer, NecroType* type)
{
    assert(infer != NULL);
    if (type == NULL)
        return (NecroGenResult) { NULL, NULL };
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (necro_env_get(infer, type->var.var) != NULL)
        {
            return (NecroGenResult) { necro_create_type_var(infer, type->var.var), NULL };
        }
        else
        {
            NecroType* new_name = necro_new_name(infer);
            return (NecroGenResult) { necro_create_type_var(infer, type->var.var), necro_create_for_all(infer, new_name->var.var, new_name) };
        }
    case NECRO_TYPE_APP:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->app.type1);
        NecroGenResult result2 = necro_gen_go(infer, type->app.type2);
        return (NecroGenResult) { necro_create_type_app(infer, result1.type, result2.type), merge_for_all(result1.maybe_for_all, result2.maybe_for_all) };
    }
    case NECRO_TYPE_FUN:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->fun.type1);
        NecroGenResult result2 = necro_gen_go(infer, type->fun.type2);
        return (NecroGenResult) { necro_create_type_fun(infer, result1.type, result2.type), merge_for_all(result1.maybe_for_all, result2.maybe_for_all) };
    }
    case NECRO_TYPE_LIST:
    {
        NecroGenResult result1 = necro_gen_go(infer, type->list.item);
        NecroGenResult result2 = necro_gen_go(infer, type->list.next);
        return (NecroGenResult) { necro_create_type_list(infer, result1.type, result2.type), merge_for_all(result1.maybe_for_all, result2.maybe_for_all) };
    }
    case NECRO_TYPE_CON:
    {
        NecroGenResult result = necro_gen_go(infer, type->con.args);
        return (NecroGenResult) { necro_create_type_con(infer, type->con.con, result.type, type->con.arity), result.maybe_for_all };
    }
    case NECRO_TYPE_FOR: necro_infer_error(infer, type, "Compiler bug: Found Polytype in necro_inst_go"); return (NecroGenResult) { NULL, NULL };
    default:             necro_infer_error(infer, type, "Compiler bug: Non-existent type %d found in necro_inst.", type->type); return (NecroGenResult) { NULL, NULL };
    }
}

NecroType* necro_gen(NecroInfer* infer, NecroType* type)
{
    assert(infer != NULL);
    assert(type != NULL);
    NecroGenResult result = necro_gen_go(infer, type);
    if (result.maybe_for_all != NULL)
    {
        NecroType* head = result.maybe_for_all;
        NecroType* prev = head;
        NecroType* curr = head;
        while (curr != NULL)
        {
            prev = curr;
            curr = curr->for_all.type;
        }
        prev->for_all.type = result.type;
        return head;
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
        buffer[i] = (n % 26) + 96;
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
        printf("%c", c + 96);
        n /= 26;
    }
}

char* necro_snprintf_id_as_characters(NecroID id, char* buffer, size_t buffer_size)
{
    size_t n = id.id;
    while (n > 0)
    {
        *buffer = (n % 26) + 96;
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

void necro_print_type_sig_go(NecroType* type, NecroIntern* intern)
{
    if (type == NULL)
        return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        necro_print_id_as_characters(type->var.var.id);
        // printf("%s", necro_intern_get_string(intern, type->var.var.id));
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
        necro_print_id_as_characters(type->for_all.var.id);
        printf(". ");
        necro_print_type_sig_go(type->for_all.type, intern);
        break;

    default:
        printf("Compiler bug: Unrecognized type: %d", type->type);
        assert(false);
    }
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
    size_t count = 0;
    char*  new_buffer;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        return necro_snprintf_id_as_characters(type->var.var.id, buffer, buffer_length);

    case NECRO_TYPE_APP:
        new_buffer = necro_snprintf_type_sig_go_maybe_with_parens(type->app.type1, intern, buffer, buffer_length);
        count      = snprintf(new_buffer, buffer_length, " ");
        return necro_snprintf_type_sig_go_maybe_with_parens(type->app.type2, intern, new_buffer + count, buffer_length);

    case NECRO_TYPE_FUN:
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            count = snprintf(buffer, buffer_length, "(");
        new_buffer = necro_snprintf_type_sig(type->fun.type1, intern, buffer + count, buffer_length);
        count = 0;
        if (type->fun.type1->type == NECRO_TYPE_FUN)
            count = snprintf(new_buffer, buffer_length, ")");
        count += snprintf(new_buffer + count, buffer_length, " -> ");
        return necro_snprintf_type_sig(type->fun.type2, intern, buffer + count, buffer_length);

    case NECRO_TYPE_CON:
    {
        bool has_args = necro_type_list_count(type->con.args) > 0;
        count = snprintf(buffer, buffer_length, "%s", necro_intern_get_string(intern, type->con.con.symbol));
        if (has_args)
        {
            count += snprintf(buffer + count, buffer_length, " ");
            return necro_snprintf_type_sig(type->con.args, intern, buffer + count, buffer_length);
        }
        else
        {
            return buffer + count;
        }
    }

    case NECRO_TYPE_LIST:
        new_buffer = necro_snprintf_type_sig_go_maybe_with_parens(type->list.item, intern, buffer, buffer_length);
        if (type->list.next != NULL)
        {
            count = snprintf(new_buffer, buffer_length, " ");
            return necro_snprintf_type_sig(type->list.next, intern, new_buffer + count, buffer_length);
        }
        else
        {
            return new_buffer;
        }

    case NECRO_TYPE_FOR:
        count      = snprintf(buffer, buffer_length, "forall ");
        new_buffer = necro_snprintf_id_as_characters(type->for_all.var.id, buffer + count, buffer_length);
        count     += snprintf(new_buffer + count, buffer_length, ". ");
        return necro_snprintf_type_sig(type->for_all.type, intern, new_buffer + count, buffer_length);

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

//=====================================================
// Testing
//=====================================================
void necro_test_type()
{
    necro_announce_phase("NecroType");

    NecroIntern intern = necro_create_intern();
    NecroInfer  infer  = necro_create_infer(&intern);

    // TODO: Dont make vars by hand, use make_new_name
    // Print test
    {
        NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);

        NecroType* lst1 = necro_create_type_list(&infer, bcon, NULL);
        NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1, 1);

        NecroType* iols = necro_create_type_list(&infer, mcon, NULL);
        NecroType* iocn = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "IO"), { 0 } }, iols, 1);

        NecroType* lst3 = necro_create_type_list(&infer, bcon, NULL);
        NecroType* lst2 = necro_create_type_list(&infer, icon, lst3);
        NecroType* econ = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst2, 2);

        NecroType* avar = necro_new_name(&infer);
        NecroType* bvar = necro_new_name(&infer);
        NecroType* cvar = necro_new_name(&infer);
        NecroType* vapp = necro_create_type_app(&infer, avar, bvar);

        NecroType* fun1 = necro_create_type_fun(&infer, bvar, cvar);
        NecroType* fun2 = necro_create_type_fun(&infer, econ, iocn);
        NecroType* fun3 = necro_create_type_fun(&infer, fun1, fun2);
        NecroType* fun4 = necro_create_type_fun(&infer, vapp, fun3);

        // gen needed
        // NecroType* for2 = necro_create_for_all(&infer, (NecroVar) { { 2 } }, fun4);
        // NecroType* for1 = necro_create_for_all(&infer, (NecroVar) { { 1 } }, for2);

        if (necro_check_and_print_type_error(&infer)) return;
        necro_print_type_sig(fun4, &intern);
    }

    // Unify test
    {
        NecroType* avar = necro_new_name(&infer);
        NecroType* bvar = necro_new_name(&infer);
        NecroType* lst1 = necro_create_type_list(&infer, bvar, NULL);
        NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1, 1);
        if (necro_check_and_print_type_error(&infer)) return;
        necro_unify(&infer, avar, mcon);
        if (necro_check_and_print_type_error(&infer)) return;
        necro_print_type_test_result("expectUnify1", avar, "resultUnify1", mcon, &intern);
    }

    // Unify test 2
    {
        NecroType* avar = necro_new_name(&infer);
        NecroType* bvar = necro_new_name(&infer);
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);

        NecroType* m1   = necro_create_type_list(&infer, avar, NULL);
        NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, m1, 1);

        NecroType* m2    = necro_create_type_list(&infer, icon, NULL);
        NecroType* mcon2 = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, m2, 1);

        if (necro_check_and_print_type_error(&infer)) return;
        necro_unify(&infer, mcon, mcon2);
        if (necro_check_and_print_type_error(&infer)) return;
        necro_print_type_test_result("expectUnify2", mcon, "resultUnify2", mcon2, &intern);
    }

    // Unify test 2
    {
        NecroType* avar  = necro_new_name(&infer);
        NecroType* bvar  = necro_new_name(&infer);
        NecroType* icon  = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
        NecroType* bcon  = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);

        NecroType* e11   = necro_create_type_list(&infer, bcon, NULL);
        NecroType* e1    = necro_create_type_list(&infer, avar, e11);
        NecroType* econ1 = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, e1, 2);

        NecroType* e22   = necro_create_type_list(&infer, bvar, NULL);
        NecroType* e2    = necro_create_type_list(&infer, icon, e22);
        NecroType* econ2 = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, e2, 2);

        if (necro_check_and_print_type_error(&infer)) return;
        necro_unify(&infer, econ1, econ2);
        if (necro_check_and_print_type_error(&infer)) return;
        necro_print_type_test_result("expectUnify3", econ1, "resultUnify3", econ2, &intern);
    }

    necro_destroy_infer(&infer);
    necro_destroy_intern(&intern);
}

// //=====================================================
// // Utility
// //=====================================================
// void necro_print_type_sig(NecroType* type, NecroIntern* intern);
// void necro_print_type_sig_go(NecroType* type, NecroIntern* intern);
// char* necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length);
// const char* necro_id_as_character_string(NecroInfer* infer, NecroID id);

// //=====================================================
// // Substitute
// //=====================================================
// NecroSub* necro_create_sub(NecroInfer* infer, NecroType* type, NecroID name, NecroSub* next)
// {
//     NecroSub* sub = necro_paged_arena_alloc(&infer->arena, sizeof(NecroSub));
//     *sub = (NecroSub)
//     {
//         .type = type,
//         .name = name,
//         .next = next,
//     };
//     return sub;
// }
// void necro_print_sub(NecroSub* sub, NecroIntern* intern);

// NecroType* necro_sub(NecroInfer* infer, NecroSub* subs, NecroType* type)
// {
//     if (necro_is_infer_error(infer))
//         return type;
//     if (subs == NULL || type == NULL)
//         return type;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//     {
//         NecroTypeVar* type_var = &type->var;
//         NecroType*    sub_type = subs->type;
//         NecroID       sub_name = subs->name;
//         if (sub_name.id == type_var->var.id.id)
//             return sub_type;
//         else
//             return necro_sub(infer, subs->next, type);
//     }
//     case NECRO_TYPE_APP:
//     {
//         NecroType* new_type1 = necro_sub(infer, subs, type->app.type1);
//         NecroType* new_type2 = necro_sub(infer, subs, type->app.type2);
//         if (new_type1 != type->app.type1 || new_type2 != type->app.type2)
//             return necro_create_type_app(infer, new_type1, new_type2);
//         else
//             return type;
//     }
//     case NECRO_TYPE_FUN:
//     {
//         NecroType* new_type1 = necro_sub(infer, subs, type->fun.type1);
//         NecroType* new_type2 = necro_sub(infer, subs, type->fun.type2);
//         if (new_type1 != type->fun.type1 || new_type2 != type->fun.type2)
//             return necro_create_type_fun(infer, new_type1, new_type2);
//         else
//             return type;
//     }
//     case NECRO_TYPE_CON:
//     {
//         NecroType* new_args = necro_sub(infer, subs, type->con.args);
//         if (new_args != type->con.args)
//             return necro_create_type_con(infer, type->con.con, new_args, type->con.arity);
//         else
//             return type;
//     }
//     case NECRO_TYPE_LIST:
//     {
//         NecroType* new_item = necro_sub(infer, subs, type->list.item);
//         NecroType* new_next = necro_sub(infer, subs, type->list.next);
//         if (new_item != type->list.item || new_next != type->list.next)
//             return necro_create_type_list(infer, new_item, new_next);
//         else
//             return type;
//     }
//     default:
//         return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
//     }
// }

// //=====================================================
// // Compose
// //=====================================================
// // TODO: Non-recursive implementations
// NecroSub* necro_compose_iter(NecroInfer* infer, NecroSub* sub, NecroSub* subs2)
// {
//     if (!subs2)
//         return NULL;
//     NecroType* type_result = necro_sub(infer, sub, subs2->type);
//     NecroSub*  new_sub     = necro_create_sub(infer, type_result, subs2->name, NULL);
//     new_sub->next          = necro_compose_iter(infer, sub, subs2->next);
//     return new_sub;
// }

// NecroSub* necro_compose(NecroInfer* infer, NecroSub* subs1, NecroSub* subs2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     if (subs1 == NULL)
//         return subs2;
//     NecroSub* new_sub = necro_create_sub(infer, subs1->type, subs1->name, NULL);
//     new_sub->next     = necro_compose_iter(infer, new_sub, subs2);
//     return necro_compose(infer, subs1->next, new_sub);
// }

// //=====================================================
// // Unify
// //=====================================================
// NecroSub* necro_unify_go(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2);

// bool necro_occurs(NecroInfer* infer, NecroID name, NecroType* type)
// {
//     if (type == NULL)
//         return false;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:  return type->var.var.id.id == name.id;
//     case NECRO_TYPE_APP:  return necro_occurs(infer, name, type->app.type1) || necro_occurs(infer, name, type->app.type2);
//     case NECRO_TYPE_FUN:  return necro_occurs(infer, name, type->fun.type1) || necro_occurs(infer, name, type->fun.type2);
//     // case NECRO_TYPE_CON:  return type->con.con.symbol.id == name.id         || necro_occurs(infer, name, type->con.args);
//     // No occurs checks on constructors?!?!?!
//     case NECRO_TYPE_CON:  return necro_occurs(infer, name, type->con.args);
//     case NECRO_TYPE_LIST: return necro_occurs(infer, name, type->list.item) || necro_occurs(infer, name, type->list.next);
//     default:              return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
//     }
// }

// inline NecroSub* necro_reverse_sub(NecroInfer* infer, NecroSub* sub)
// {
//     NecroSub* current = sub;
//     NecroSub* prev    = NULL;
//     NecroSub* new     = NULL;
//     while (current != NULL)
//     {
//         new     = necro_create_sub(infer, current->type, current->name, prev);
//         current = current->next;
//         prev    = new;
//     }
//     return new;
// }

// NecroSub* necro_unify_app_args(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(type1 != NULL);
//     assert(type2 != NULL);
//     assert(type1->type == NECRO_TYPE_APP);
//     assert(type2->type == NECRO_TYPE_APP);
//     NecroSub* new_sub             = necro_unify_go(infer, NULL, necro_sub(infer, unifier_result, type1->app.type1), necro_sub(infer, unifier_result, type2->app.type1));
//     NecroSub* new_unifier_result  = necro_compose(infer, new_sub, unifier_result);
//     NecroSub* new_sub2            = necro_unify_go(infer, NULL, necro_sub(infer, new_unifier_result, type1->app.type2), necro_sub(infer, new_unifier_result, type2->app.type2));
//     NecroSub* new_unifier_result2 = necro_compose(infer, new_sub2, new_unifier_result);
//     // return new_unifier_result2;
//     return necro_reverse_sub(infer, new_unifier_result2);
// }

// NecroSub* necro_unify_fun_args(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(type1 != NULL);
//     assert(type2 != NULL);
//     assert(type1->type == NECRO_TYPE_FUN);
//     assert(type2->type == NECRO_TYPE_FUN);
//     NecroSub* new_sub             = necro_unify_go(infer, NULL, necro_sub(infer, unifier_result, type1->fun.type1), necro_sub(infer, unifier_result, type2->fun.type1));
//     NecroSub* new_unifier_result  = necro_compose(infer, new_sub, unifier_result);
//     NecroSub* new_sub2            = necro_unify_go(infer, NULL, necro_sub(infer, new_unifier_result, type1->fun.type2), necro_sub(infer, new_unifier_result, type2->fun.type2));
//     NecroSub* new_unifier_result2 = necro_compose(infer, new_sub2, new_unifier_result);
//     // return new_unifier_result2;
//     return necro_reverse_sub(infer, new_unifier_result2);
// }

// NecroSub* necro_unify_con_args(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     if (type1 == NULL && type2 == NULL)
//         // return unifier_result; // TODO / NOTE: Reverse or not?!?!?
//         return necro_reverse_sub(infer, unifier_result);
//     if (type1 == NULL && type2 != NULL)
//         return necro_infer_error(infer, type2, "Mismatched arities error");
//     if (type1 != NULL && type2 == NULL)
//         return necro_infer_error(infer, type1, "Mismatched arities error");
//     assert(type1->type == NECRO_TYPE_LIST);
//     assert(type2->type == NECRO_TYPE_LIST);
//     NecroSub* new_sub            = necro_unify_go(infer, NULL, necro_sub(infer, unifier_result, type1->list.item), necro_sub(infer, unifier_result, type2->list.item));
//     NecroSub* new_unifier_result = necro_compose(infer, new_sub, unifier_result);
//     return necro_unify_con_args(infer, new_unifier_result, type1->list.next, type2->list.next);
// }

// inline NecroSub* necro_unify_var(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(type1 != NULL);
//     assert(type1->type == NECRO_TYPE_VAR);
//     if (type2 == NULL)
//         return necro_infer_error(infer, type1, "Mismatched arities");
//     switch (type2->type)
//     {

//     case NECRO_TYPE_VAR:
//         if (type1->var.var.id.id == type2->var.var.id.id)
//             return NULL;
//         else
//             return necro_create_sub(infer, type1, type2->var.var.id, unifier_result);

//     case NECRO_TYPE_APP:
//         if (necro_occurs(infer, type1->var.var.id, type2))
//             return necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_id_as_character_string(infer, type1->var.var.id));
//         else
//             return necro_create_sub(infer, type2, type1->var.var.id, unifier_result);

//     case NECRO_TYPE_FUN:
//         if (necro_occurs(infer, type1->var.var.id, type2))
//             return necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_id_as_character_string(infer, type1->var.var.id));
//         else
//             return necro_create_sub(infer, type2, type1->var.var.id, unifier_result);

//     case NECRO_TYPE_CON:
//         if (type2->con.args == NULL)
//             return necro_create_sub(infer, type2, type1->var.var.id, unifier_result);
//         else if (necro_occurs(infer, type1->var.var.id, type2))
//             return necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_id_as_character_string(infer, type1->var.var.id));
//         else
//             return necro_create_sub(infer, type2, type1->var.var.id, unifier_result);

//     case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeVar with type args list.");
//     default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
//     }
// }

// inline NecroSub* necro_unify_app(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(type1 != NULL);
//     assert(type1->type == NECRO_TYPE_APP);
//     if (type2 == NULL)
//         return necro_infer_error(infer, type1, "Mismatched arities");
//     switch (type2->type)
//     {
//     case NECRO_TYPE_VAR:
//         if (necro_occurs(infer, type2->var.var.id, type1))
//             return necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
//         else
//             return necro_create_sub(infer, type1, type2->var.var.id, unifier_result);
//     case NECRO_TYPE_APP:  return necro_unify_app_args(infer, unifier_result, type1, type2);
//     case NECRO_TYPE_CON:
//     {
//         NecroType* uncurried_con = necro_curry_con(infer, type2);
//         if (uncurried_con == NULL)
//             return necro_infer_error(infer, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type2->con.con.symbol));
//         else
//             return necro_unify_go(infer, unifier_result, type1, uncurried_con);
//     }
//     case NECRO_TYPE_FUN:  return necro_infer_error(infer, type1, "Attempting to unify TypeApp with (->).");
//     case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeApp with type args list.");
//     default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
//     }
// }

// inline NecroSub* necro_unify_fun(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(type1 != NULL);
//     assert(type1->type == NECRO_TYPE_FUN);
//     if (type2 == NULL)
//         return necro_infer_error(infer, type1, "Mismatched arities");
//     switch (type2->type)
//     {
//     case NECRO_TYPE_VAR:
//         if (necro_occurs(infer, type2->var.var.id, type1))
//             return necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
//         else
//             return necro_create_sub(infer, type1, type2->var.var.id, unifier_result);
//     case NECRO_TYPE_FUN:  return necro_unify_fun_args(infer, unifier_result, type1, type2);
//     case NECRO_TYPE_APP:  return necro_infer_error(infer, type1, "Attempting to unify (->) with TypeApp.");
//     case NECRO_TYPE_CON:  return necro_infer_error(infer, type1, "Attempting to unify (->) with TypeCon (%s)", necro_intern_get_string(infer->intern, type2->con.con.symbol));
//     case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify (->) with type args list.");
//     default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
//     }
// }

// inline NecroSub* necro_unify_con(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(type1 != NULL);
//     assert(type1->type == NECRO_TYPE_CON);
//     if (type2 == NULL)
//         return necro_infer_error(infer, type1, "Mismatched arities");
//     switch (type2->type)
//     {
//     case NECRO_TYPE_VAR:
//         if (type2->con.args == NULL)
//             return necro_create_sub(infer, type1, type2->var.var.id, NULL);
//         else if (necro_occurs(infer, type2->var.var.id, type1))
//             return necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_id_as_character_string(infer, type2->var.var.id));
//         else
//             return necro_create_sub(infer, type1, type2->var.var.id, NULL);
//     case NECRO_TYPE_CON:
//         if (type1->con.con.symbol.id == type2->con.con.symbol.id)
//             return necro_unify_con_args(infer, unifier_result, type1->con.args, type2->con.args);
//         else
//             return necro_infer_error(infer, type1, "Attempting to unify two different types, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
//     case NECRO_TYPE_APP:
//     {
//         NecroType* uncurried_con = necro_curry_con(infer, type1);
//         if (uncurried_con == NULL)
//             return necro_infer_error(infer, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol));
//         else
//             return necro_unify_go(infer, unifier_result, uncurried_con, type2);
//     }
//     case NECRO_TYPE_FUN:  return necro_infer_error(infer, type1, "Attempting to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type2->con.con.symbol));
//     case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type2->con.con.symbol));
//     default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
//     }
// }

// NecroSub* necro_unify_go(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
// {
//     if (necro_is_infer_error(infer))
//         return NULL;
//     assert(infer != NULL);
//     if (type1 == NULL)
//         return necro_infer_error(infer, type1, "Mismatched arities");
//     switch (type1->type)
//     {
//     case NECRO_TYPE_VAR:  return necro_unify_var(infer, unifier_result, type1, type2);
//     case NECRO_TYPE_APP:  return necro_unify_app(infer, unifier_result, type1, type2);
//     case NECRO_TYPE_FUN:  return necro_unify_fun(infer, unifier_result, type1, type2);
//     case NECRO_TYPE_CON:  return necro_unify_con(infer, unifier_result, type1, type2);
//     case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Found Type args list in necro_unify");
//     default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
//     }
// }

// NecroSub* necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2)
// {
//     return necro_unify_go(infer, NULL, type1, type2);
// }

// //=====================================================
// // Type Sanity
// //=====================================================
// void necro_check_type_sanity(NecroInfer* infer, NecroType* type);
// void necro_check_type_scheme_sanity(NecroInfer* infer, NecroTypeScheme* scheme)
// {
//     if (necro_is_infer_error(infer))
//         return;
//     if (scheme == NULL)
//         return;
//     if (scheme->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         necro_check_type_sanity(infer, &scheme->term);
//         return;
//     }
//     else
//     {
//         necro_check_type_scheme_sanity(infer, scheme->for_all.scheme);
//         return;
//     }
// }

// void necro_check_gamma_sanity(NecroInfer* infer, NecroGamma* gamma)
// {
//     if (gamma == NULL)
//         return;
//     necro_check_type_scheme_sanity(infer, gamma->scheme);
//     necro_check_gamma_sanity(infer, gamma->next);
// }

// void necro_check_type_sanity(NecroInfer* infer, NecroType* type)
// {
//     if (necro_is_infer_error(infer))
//         return;
//     if (type == NULL)
//         return;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//         break;
//     case NECRO_TYPE_FUN:
//         if (type->fun.type1 == NULL)
//         {
//             necro_infer_error(infer, type, "Malformed (->) in type");
//             break;
//         }
//         if (type->fun.type2 == NULL)
//         {
//             necro_infer_error(infer, type, "Malformed (->) in type");
//             break;
//         }
//         necro_check_type_sanity(infer, type->fun.type1);
//         necro_check_type_sanity(infer, type->fun.type2);
//         break;
//     case NECRO_TYPE_APP:
//         if (type->app.type1 == NULL)
//         {
//             necro_infer_error(infer, type, "Malformed Type Application");
//             break;
//         }
//         if (type->app.type2 == NULL)
//         {
//             necro_infer_error(infer, type, "Malformed Type Application");
//             break;
//         }
//         necro_check_type_sanity(infer, type->app.type1);
//         necro_check_type_sanity(infer, type->app.type2);
//         break;
//     case NECRO_TYPE_LIST:
//         necro_check_type_sanity(infer, type->list.item);
//         necro_check_type_sanity(infer, type->list.next);
//         break;
//     case NECRO_TYPE_CON:
//         if (necro_type_list_count(type->con.args) != type->con.arity)
//         {
//             necro_infer_error(infer, type, "Mismatched arity for type %s. Expected arity: %d, found arity %d", necro_intern_get_string(infer->intern, type->con.con.symbol), type->con.arity, necro_type_list_count(type->con.args));
//             break;
//         }
//         necro_check_type_sanity(infer, type->con.args);
//         break;
//     default: necro_infer_error(infer, type, "Compiler bug (necro_check_type_sanity): Unrecognized type: %d", type->type); break;
//     }
// }

// bool necro_types_match(NecroType* type1, NecroType* type2)
// {
//     if (type1 == NULL && type2 == NULL)
//         return true;
//     else if (type1 != NULL && type2 == NULL)
//         return false;
//     else if (type1 == NULL && type2 != NULL)
//         return false;
//     if (type1->type != type2->type)
//         return false;
//     switch (type1->type)
//     {
//     case NECRO_TYPE_VAR:  return type1->var.var.id.id == type2->var.var.id.id;
//     case NECRO_TYPE_APP:  return necro_types_match(type1->app.type1, type2->app.type1) && necro_types_match(type1->app.type2, type2->app.type2);
//     case NECRO_TYPE_FUN:  return necro_types_match(type1->fun.type1, type2->fun.type1) && necro_types_match(type1->fun.type2, type2->fun.type2);
//     case NECRO_TYPE_CON:  return type1->con.con.symbol.id == type2->con.con.symbol.id && necro_types_match(type1->con.args, type2->con.args);
//     case NECRO_TYPE_LIST: return necro_types_match(type1->list.item, type2->list.item) && necro_types_match(type1->list.next, type2->list.next);
//     default:
//         printf("Compiler bug: Unrecognized type: %d", type1->type);
//         assert(false);
//         return false;
//     }
// }

// bool necro_type_schemes_match(NecroTypeScheme* scheme1, NecroTypeScheme* scheme2)
// {
//     if (scheme1 == NULL && scheme2 == NULL)
//         return true;
//     else if (scheme1 != NULL && scheme2 == NULL)
//         return false;
//     else if (scheme1 == NULL && scheme2 != NULL)
//         return false;
//     if (scheme1->type != scheme2->type)
//         return false;
//     if (scheme1->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         return necro_types_match(&scheme1->term, &scheme2->term);
//     }
//     else
//     {
//         return scheme1->for_all.var.id.id == scheme2->for_all.var.id.id && necro_type_schemes_match(scheme1->for_all.scheme, scheme2->for_all.scheme);
//     }
// }

// bool necro_subs_match(NecroSub* sub1, NecroSub* sub2)
// {
//     if (sub1 == NULL && sub2 == NULL)
//         return true;
//     else if (sub1 == NULL && sub2 != NULL)
//         return true;
//     else if (sub1 != NULL && sub2 == NULL)
//         return true;
//     else
//         return sub1->name.id == sub2->name.id            &&
//                necro_types_match(sub1->type, sub2->type) &&
//                necro_subs_match(sub1->next, sub2->next);
// }

// //=====================================================
// // NecroTypeScheme - i.e. forall
// //=====================================================
// typedef struct NecroVarList
// {
//     NecroVar             var;
//     struct NecroVarList* next;
// } NecroVarList;

// NecroVar necro_new_name(NecroInfer* infer)
// {
//     infer->highest_id++;
//     NecroVar var = (NecroVar) { .id = { infer->highest_id } };
//     return var;
// }

// NecroVarList* necro_create_var_list(NecroInfer* infer, NecroVar var, NecroVarList* next)
// {
//     NecroVarList* var_list = necro_paged_arena_alloc(&infer->arena, sizeof(NecroVarList));
//     var_list->var = var;
//     var_list->next = next;
//     return var_list;
// }

// bool necro_var_list_member(NecroVarList* list, NecroVar var)
// {
//     NecroVarList* current = list;
//     while (current != NULL)
//     {
//         if (current->var.id.id == var.id.id)
//             return true;
//         current = current->next;
//     }
//     return false;
// }

// typedef struct
// {
//     NecroVarList* free;
//     NecroVarList* bound;
// } NecroFreeBoundLists;

// inline NecroVarList* necro_var_list_cons(NecroInfer* infer, NecroVarList* list, NecroVar var)
// {
//     return necro_create_var_list(infer, var, list);
// }

// inline NecroFreeBoundLists necro_cons_free(NecroInfer* infer, NecroFreeBoundLists lists, NecroVar var)
// {
//     return (NecroFreeBoundLists) { .free = necro_var_list_cons(infer, lists.free, var), .bound = lists.bound };
// }

// inline NecroFreeBoundLists necro_cons_bound(NecroInfer* infer, NecroFreeBoundLists lists, NecroVar var)
// {
//     return (NecroFreeBoundLists) { .free = lists.free, .bound = necro_var_list_cons(infer, lists.bound, var) };
// }

// NecroFreeBoundLists necro_free_bound_ty_vars(NecroInfer* infer, NecroType* type, NecroFreeBoundLists lists)
// {
//     if (type == NULL)
//         return lists;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//         if (!necro_var_list_member(lists.bound, type->var.var))
//             return necro_cons_free(infer, lists, type->var.var);
//         else
//             return lists;
//     case NECRO_TYPE_APP:  return necro_free_bound_ty_vars(infer, type->app.type2, necro_free_bound_ty_vars(infer, type->app.type1, lists));
//     case NECRO_TYPE_FUN:  return necro_free_bound_ty_vars(infer, type->fun.type2, necro_free_bound_ty_vars(infer, type->fun.type1, lists));
//     case NECRO_TYPE_CON:  return necro_free_bound_ty_vars(infer, type->con.args, lists);
//     case NECRO_TYPE_LIST: return necro_free_bound_ty_vars(infer, type->list.next, necro_free_bound_ty_vars(infer, type->list.item, lists));
//     default:              necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d", type->type); return lists;
//     }
// }

// NecroFreeBoundLists necro_free_bound_ty_vars_in_scheme(NecroInfer* infer, NecroTypeScheme* scheme, NecroFreeBoundLists lists)
// {
//     if (scheme->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         return necro_free_bound_ty_vars(infer, &scheme->term, lists);
//     }
//     else
//     {
//         return necro_free_bound_ty_vars_in_scheme(infer, scheme->for_all.scheme, necro_cons_bound(infer, lists, scheme->for_all.var));
//     }
// }

// NecroTypeScheme* necro_sub_type_scheme_go(NecroInfer* infer, NecroSub* sub, NecroTypeScheme* scheme)
// {
//     if (necro_is_infer_error(infer))
//         return scheme;
//     if (sub == NULL)
//         return scheme;
//     if (scheme->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         NecroType* type = necro_sub(infer, sub, &scheme->term);
//         if (type == &scheme->term)
//             return scheme;
//         else
//             return necro_create_term(infer, type);
//     }
//     else if (sub->name.id == scheme->for_all.var.id.id)
//     {
//         return scheme;
//     }
//     NecroFreeBoundLists lists = necro_free_bound_ty_vars(infer, sub->type, (NecroFreeBoundLists) { NULL, NULL });
//     if (necro_var_list_member(lists.free, scheme->for_all.var))
//     {
//         NecroVar         new_name   = necro_new_name(infer);
//         NecroSub*        new_sub    = necro_create_sub(infer, necro_create_type_var(infer, new_name), scheme->for_all.var.id, NULL);
//         NecroTypeScheme* new_scheme = necro_sub_type_scheme_go(infer, necro_compose(infer, new_sub, sub), scheme->for_all.scheme);
//         return necro_create_for_all(infer, new_name, new_scheme);
//     }
//     else
//     {
//         NecroTypeScheme* new_scheme = necro_sub_type_scheme_go(infer, sub, scheme->for_all.scheme);
//         if (new_scheme == scheme->for_all.scheme)
//             return scheme;
//         else
//             return necro_create_for_all(infer, scheme->for_all.var, new_scheme);
//     }
// }

// NecroTypeScheme* necro_sub_type_scheme(NecroInfer* infer, NecroSub* sub, NecroTypeScheme* scheme)
// {
//     if (necro_is_infer_error(infer))
//         return scheme;
//     NecroSub* current_sub = sub;
//     while (current_sub != NULL)
//     {
//         if (current_sub->next != NULL)
//             scheme = necro_sub_type_scheme_go(infer, necro_create_sub(infer, current_sub->type, current_sub->name, NULL), scheme);
//         else
//             scheme = necro_sub_type_scheme_go(infer, current_sub, scheme);
//         current_sub = current_sub->next;
//     }
//     return scheme;
// }

// //=====================================================
// // NecroGamma - i.e. assumptions
// // Trying this the slow way first, for correctness sake
// // TODO: Make faster
// //=====================================================
// // TODO: Imperative version
// NecroGamma* necro_sub_gamma(NecroInfer* infer, NecroSub* sub, NecroGamma* gamma)
// {
//     if (necro_is_infer_error(infer))
//         return gamma;
//     assert(infer != NULL);
//     if (gamma == NULL)
//         return NULL;
//     NecroTypeScheme* new_scheme = necro_sub_type_scheme(infer, sub, gamma->scheme);
//     NecroGamma*      new_next   = necro_sub_gamma(infer, sub, gamma->next);
//     if (new_scheme != gamma->scheme || new_next != gamma->next)
//         return necro_create_gamma(infer, gamma->var, new_scheme, new_next);
//     else
//         return gamma;
// }

// NecroTypeScheme* necro_type_scheme_closure(NecroInfer* infer, NecroGamma* gamma, NecroType* type)
// {
//     assert(infer != NULL);
//     assert(type  != NULL);
//     NecroTypeScheme* curr_scheme       = necro_create_term(infer, type);
//     if (gamma == NULL)
//         return curr_scheme;
//     NecroVarList* curr_type_free_vars  = necro_free_bound_ty_vars(infer, type, (NecroFreeBoundLists) { NULL, NULL }).free;
//     NecroVarList* curr_type_bound_vars = NULL;
//     while (gamma != NULL)
//     {
//         NecroVarList*  gamma_free_vars = necro_free_bound_ty_vars_in_scheme(infer, gamma->scheme, (NecroFreeBoundLists) { NULL, NULL }).free;
//         while (curr_type_free_vars != NULL)
//         {
//             if (!necro_var_list_member(gamma_free_vars, curr_type_free_vars->var) && !necro_var_list_member(curr_type_bound_vars, curr_type_free_vars->var))
//             {
//                 curr_type_bound_vars = necro_var_list_cons(infer, curr_type_bound_vars, curr_type_free_vars->var);
//                 curr_scheme          = necro_create_for_all(infer, curr_type_free_vars->var, curr_scheme);
//             }
//             curr_type_free_vars = curr_type_free_vars->next;
//         }
//         gamma = gamma->next;
//     }
//     return curr_scheme;
// }

// NecroTypeScheme* necro_gamma_find(NecroInfer* infer, NecroGamma* gamma, NecroVar var)
// {
//     if (var.id.id > infer->highest_id)
//         infer->highest_id++;
//     while (gamma != NULL)
//     {
//         if (gamma->var.id.id == var.id.id)
//             return gamma->scheme;
//         gamma = gamma->next;
//     }
//     return NULL;
// }

// NecroTypeScheme* necro_principal_type_scheme(NecroInfer* infer, NecroGamma* gamma, NecroType* type)
// {
//     return necro_type_scheme_closure(infer, gamma, type);
// }

// //=====================================================
// // Printing
// //=====================================================
// const char* necro_id_as_character_string(NecroInfer* infer, NecroID id)
// {
//     size_t length = 0;
//     size_t n      = id.id;
//     while (n > 0)
//     {
//         n /= 26;
//     }
//     char* buffer = necro_paged_arena_alloc(&infer->arena, (length + 1) * sizeof(char));
//     n = id.id;
//     size_t i = 0;
//     while (n > 0)
//     {
//         buffer[i] = (n % 26) + 96;
//         n /= 26;
//         ++i;
//     }
//     buffer[i] = '\0';
//     return buffer;
// }

// void necro_print_id_as_characters(NecroID id)
// {
//     size_t n = id.id;
//     while (n > 0)
//     {
//         char c = n % 26;
//         printf("%c", c + 96);
//         n /= 26;
//     }
// }

// char* necro_snprintf_id_as_characters(NecroID id, char* buffer, size_t buffer_size)
// {
//     size_t n = id.id;
//     while (n > 0)
//     {
//         *buffer = (n % 26) + 96;
//         // buffer += snprintf(buffer, buffer_size, "%c", c + 96);
//         n /= 26;
//         buffer++;
//     }
//     return buffer;
// }

// void necro_print_type(NecroType* type, uint32_t whitespace, NecroIntern* intern)
// {
//     if (type == NULL)
//         return;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//         print_white_space(whitespace);
//         printf("TypeVar, name: ");
//         necro_print_id_as_characters(type->var.var.id);
//         printf(", id: %d\n", type->var.var.id.id);
//         break;

//     case NECRO_TYPE_APP:
//         print_white_space(whitespace);
//         printf("App\n");
//         print_white_space(whitespace);
//         printf("(\n");
//         necro_print_type(type->app.type1, whitespace + 4, intern);
//         necro_print_type(type->app.type2, whitespace + 4, intern);
//         print_white_space(whitespace);
//         printf(")\n");
//         break;

//     case NECRO_TYPE_FUN:
//         necro_print_type(type->fun.type1, whitespace + 4, intern);
//         print_white_space(whitespace);
//         printf("->\n");
//         necro_print_type(type->fun.type2, whitespace + 4, intern);
//         break;

//     case NECRO_TYPE_CON:
//         print_white_space(whitespace);
//         printf("Con, name: %s, id: %d\n", necro_intern_get_string(intern, type->con.con.symbol), type->con.con.id.id);
//         if (necro_type_list_count(type->con.args) > 0)
//         {
//             print_white_space(whitespace);
//             printf("(\n");
//             necro_print_type(type->con.args, whitespace + 4, intern);
//             print_white_space(whitespace);
//             printf(")\n");
//         }
//         break;

//     case NECRO_TYPE_LIST:
//         necro_print_type(type->list.item, whitespace, intern);
//         necro_print_type(type->list.next, whitespace, intern);
//         break;

//     default:
//         printf("Compiler bug: Unrecognized type: %d", type->type);
//         assert(false);
//     }
// }

// bool necro_is_simple_type(NecroType* type)
// {
//     return type->type == NECRO_TYPE_VAR || (type->type == NECRO_TYPE_CON && necro_type_list_count(type->con.args) == 0);
// }

// void necro_print_type_sig_go(NecroType* type, NecroIntern* intern);

// void necro_print_type_sig_go_maybe_with_parens(NecroType* type, NecroIntern* intern)
// {
//     if (!necro_is_simple_type(type))
//         printf("(");
//     necro_print_type_sig_go(type, intern);
//     if (!necro_is_simple_type(type))
//         printf(")");
// }

// void necro_print_type_sig_go(NecroType* type, NecroIntern* intern)
// {
//     if (type == NULL)
//         return;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//         necro_print_id_as_characters(type->var.var.id);
//         // printf("%s", necro_intern_get_string(intern, type->var.var.id));
//         break;

//     case NECRO_TYPE_APP:
//         necro_print_type_sig_go_maybe_with_parens(type->app.type1, intern);
//         printf(" ");
//         necro_print_type_sig_go_maybe_with_parens(type->app.type2, intern);
//         break;

//     case NECRO_TYPE_FUN:
//         if (type->fun.type1->type == NECRO_TYPE_FUN)
//             printf("(");
//         necro_print_type_sig_go(type->fun.type1, intern);
//         if (type->fun.type1->type == NECRO_TYPE_FUN)
//             printf(")");
//         printf(" -> ");
//         necro_print_type_sig_go(type->fun.type2, intern);
//         break;

//     case NECRO_TYPE_CON:
//     {
//         bool has_args = necro_type_list_count(type->con.args) > 0;
//         printf("%s", necro_intern_get_string(intern, type->con.con.symbol));
//         if (has_args)
//         {
//             printf(" ");
//             necro_print_type_sig_go(type->con.args, intern);
//         }
//         break;
//     }

//     case NECRO_TYPE_LIST:
//         necro_print_type_sig_go_maybe_with_parens(type->list.item, intern);
//         if (type->list.next != NULL)
//         {
//             printf(" ");
//             necro_print_type_sig_go(type->list.next, intern);
//         }
//         break;

//     default:
//         printf("Compiler bug: Unrecognized type: %d", type->type);
//         assert(false);
//     }
// }

// char* necro_snprintf_type_sig_go_maybe_with_parens(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length)
// {
//     size_t count = 0;
//     char* new_buffer;
//     if (!necro_is_simple_type(type))
//         count = snprintf(buffer, buffer_length, "(");
//     new_buffer = necro_snprintf_type_sig(type, intern, buffer + count, buffer_length);
//     if (!necro_is_simple_type(type))
//         count = snprintf(new_buffer, buffer_length, ")");
//     return new_buffer + count;
// }

// char* necro_snprintf_type_sig(NecroType* type, NecroIntern* intern, char* buffer, const size_t buffer_length)
// {
//     if (type == NULL)
//         return buffer;
//     size_t count = 0;
//     char*  new_buffer;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//         return necro_snprintf_id_as_characters(type->var.var.id, buffer, buffer_length);
//         // return buffer + snprintf(buffer, buffer_length, "%s", necro_intern_get_string(intern, type->var.var.id));

//     case NECRO_TYPE_APP:
//         new_buffer = necro_snprintf_type_sig_go_maybe_with_parens(type->app.type1, intern, buffer, buffer_length);
//         count      = snprintf(new_buffer, buffer_length, " ");
//         return necro_snprintf_type_sig_go_maybe_with_parens(type->app.type2, intern, new_buffer + count, buffer_length);

//     case NECRO_TYPE_FUN:
//         if (type->fun.type1->type == NECRO_TYPE_FUN)
//             count = snprintf(buffer, buffer_length, "(");
//         new_buffer = necro_snprintf_type_sig(type->fun.type1, intern, buffer + count, buffer_length);
//         count = 0;
//         if (type->fun.type1->type == NECRO_TYPE_FUN)
//             count = snprintf(new_buffer, buffer_length, ")");
//         count += snprintf(new_buffer + count, buffer_length, " -> ");
//         return necro_snprintf_type_sig(type->fun.type2, intern, buffer + count, buffer_length);

//     case NECRO_TYPE_CON:
//     {
//         bool has_args = necro_type_list_count(type->con.args) > 0;
//         count = snprintf(buffer, buffer_length, "%s", necro_intern_get_string(intern, type->con.con.symbol));
//         if (has_args)
//         {
//             count += snprintf(buffer + count, buffer_length, " ");
//             return necro_snprintf_type_sig(type->con.args, intern, buffer + count, buffer_length);
//         }
//         else
//         {
//             return buffer + count;
//         }
//     }

//     case NECRO_TYPE_LIST:
//         new_buffer = necro_snprintf_type_sig_go_maybe_with_parens(type->list.item, intern, buffer, buffer_length);
//         if (type->list.next != NULL)
//         {
//             count = snprintf(new_buffer, buffer_length, " ");
//             return necro_snprintf_type_sig(type->list.next, intern, new_buffer + count, buffer_length);
//         }
//         else
//         {
//             return new_buffer;
//         }

//     default:
//         return buffer + snprintf(buffer, buffer_length, "Compiler bug: Unrecognized type: %d", type->type);
//     }
// }

// void necro_print_type_sig(NecroType* type, NecroIntern* intern)
// {
//     if (type != NULL)
//         necro_print_type_sig_go(type, intern);
//     printf("\n");
// }

// void necro_print_sub_go(NecroSub* sub, NecroIntern* intern)
// {
//     necro_print_id_as_characters(sub->name);
//     printf(" ==> ");
//     necro_print_type_sig_go(sub->type, intern);
//     if (sub->next != NULL)
//     {
//         printf(", ");
//         necro_print_sub_go(sub->next, intern);
//     }
// }

// void necro_print_sub(NecroSub* sub, NecroIntern* intern)
// {
//     if (sub == NULL)
//     {
//         printf("()");
//         return;
//     }
//     else
//     {
//         printf("(");
//         necro_print_sub_go(sub->next, intern);
//         printf(")\n");
//     }
// }

// void necro_print_type_scheme_go(NecroTypeScheme* scheme, NecroIntern* intern)
// {
//     if (scheme->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         printf(". ");
//         necro_print_type_sig_go(&scheme->term, intern);
//     }
//     else
//     {
//         printf(" ");
//         necro_print_id_as_characters(scheme->for_all.var.id);
//         necro_print_type_scheme_go(scheme->for_all.scheme, intern);
//     }
// }

// void necro_print_type_scheme(NecroTypeScheme* scheme, NecroIntern* intern)
// {
//     if (scheme->type == NECRO_TYPE_SCHEME_TERM)
//     {
//         necro_print_type_sig_go(&scheme->term, intern);
//     }
//     else
//     {
//         printf("forall ");
//         necro_print_id_as_characters(scheme->for_all.var.id);
//         necro_print_type_scheme_go(scheme->for_all.scheme, intern);
//     }
// }

// void necro_print_gamma_go(NecroGamma* gamma, NecroIntern* intern)
// {
//     if (gamma == NULL)
//         return;
//     printf("    ");
//     necro_print_id_as_characters(gamma->var.id);
//     printf(" ==> ");
//     necro_print_type_scheme(gamma->scheme, intern);
//     printf("\n");
//     necro_print_gamma_go(gamma->next, intern);
// }

// void necro_print_gamma(NecroGamma* gamma, NecroIntern* intern)
// {
//     printf("\n");
//     printf("[Gamma]\n");
//     necro_print_gamma_go(gamma, intern);
//     // printf(")\n");
// }

// void necro_print_test_result(bool passed, const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern)
// {
//     printf("\n%s :: ", test_name);
//     necro_print_type_sig(type, intern);
//     printf("%s :: ", test_name2);
//     necro_print_type_sig(type2, intern);
//     if (passed)
//         printf(" (passed)\n");
//     else
//         printf(" (FAILED)\n");
// }

// void necro_print_type_test_result(const char* test_name, NecroType* type, const char* test_name2, NecroType* type2, NecroIntern* intern)
// {
//     bool passed = necro_types_match(type, type2);
//     printf("\n%s :: ", test_name);
//     necro_print_type_sig(type, intern);
//     printf("%s :: ", test_name2);
//     necro_print_type_sig(type2, intern);
//     if (passed)
//         printf(" (passed)\n");
//     else
//         printf(" (FAILED)\n");
// }

// void necro_print_test_sub_result(bool passed, const char* test_name, NecroSub* sub1, const char* test_name2, NecroSub* sub2, NecroIntern* intern)
// {
//     printf("\n");
//     printf("%s :: ", test_name);
//     necro_print_sub(sub1, intern);
//     printf("%s :: ", test_name2);
//     necro_print_sub(sub2, intern);
//     if (passed)
//         printf(" (passed)\n");
//     else
//         printf(" (FAILED)\n");
// }

// void necro_print_test_scheme_result(bool passed, const char* test_name, NecroTypeScheme* scheme1, const char* test_name2, NecroTypeScheme* scheme2, NecroIntern* intern)
// {
//     printf("\n%s :: ", test_name);
//     necro_print_type_scheme(scheme1, intern);
//     printf("\n");
//     printf("%s :: ", test_name2);
//     necro_print_type_scheme(scheme2, intern);
//     printf("\n");
//     if (passed)
//         printf(" (passed)\n");
//     else
//         printf(" (FAILED)\n");
// }

// void necro_print_type_scheme_test_result(const char* test_name, NecroTypeScheme* scheme1, const char* test_name2, NecroTypeScheme* scheme2, NecroIntern* intern)
// {
//     bool passed = necro_type_schemes_match(scheme1, scheme2);
//     printf("\n%s :: ", test_name);
//     necro_print_type_scheme(scheme1, intern);
//     printf("\n");
//     printf("%s :: ", test_name2);
//     necro_print_type_scheme(scheme2, intern);
//     printf("\n");
//     if (passed)
//         printf(" (passed)\n");
//     else
//         printf(" (FAILED)\n");
// }

// //=====================================================
// // Testing
// //=====================================================
// void necro_test_type()
// {
//     necro_announce_phase("NecroType");

//     NecroIntern intern = necro_create_intern();
//     NecroInfer  infer  = necro_create_infer(&intern);

//     // Print test
//     {
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 1 } }, NULL, 0);

//         NecroType* lst1 = necro_create_type_list(&infer, bcon, NULL);
//         NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 2 } }, lst1, 1);

//         NecroType* iols = necro_create_type_list(&infer, mcon, NULL);
//         NecroType* iocn = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "IO"), { 7 } }, iols, 1);

//         NecroType* lst3 = necro_create_type_list(&infer, bcon, NULL);
//         NecroType* lst2 = necro_create_type_list(&infer, icon, lst3);
//         NecroType* econ = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 3 } }, lst2, 2);

//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* cvar = necro_create_type_var(&infer, (NecroVar) { { 3 } });
//         NecroType* vapp = necro_create_type_app(&infer, avar, bvar);

//         NecroType* fun1 = necro_create_type_fun(&infer, bvar, cvar);
//         NecroType* fun2 = necro_create_type_fun(&infer, econ, iocn);
//         NecroType* fun3 = necro_create_type_fun(&infer, fun1, fun2);
//         NecroType* fun4 = necro_create_type_fun(&infer, vapp, fun3);

//         necro_print_type_sig(fun4, &intern);
//         printf("\n");
//     }

//     // Simple Substitute test
//     {
//         NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bcon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);
//         NecroSub*  sub    = necro_create_sub(&infer, bcon, (NecroID) { 1 }, NULL);
//         NecroType* result = necro_sub(&infer, sub, avar);
//         necro_print_test_result(necro_types_match(result, bcon), "simpleSub1", result, "simpleSub2", bcon, &intern);
//     }

//     // App Substitute test
//     {
//         NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar   = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* app    = necro_create_type_app(&infer, avar, bvar);
//         NecroSub*  sub    = necro_create_sub(&infer, icon, (NecroID) { 2 }, NULL);
//         NecroType* result = necro_sub(&infer, sub, app);
//         NecroType* match  = necro_create_type_app(&infer, avar, icon);
//         necro_print_test_result(necro_types_match(result, match), "appSub1", result, "appSub2", match, &intern);
//     }

//     // Func Substitute test
//     {
//         NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar   = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* fun    = necro_create_type_fun(&infer, bvar, avar);
//         NecroSub*  sub    = necro_create_sub(&infer, icon, (NecroID) { 2 }, NULL);
//         NecroType* result = necro_sub(&infer, sub, fun);
//         NecroType* match  = necro_create_type_fun(&infer, icon, avar);
//         necro_print_test_result(necro_types_match(result, match), "funcSub1", result, "funcSub2", match, &intern);
//     }

//     // Con Substitute test
//     {
//         NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar   = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* ucon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "()"), { 0 } }, NULL, 0);

//         NecroType* lst1   = necro_create_type_list(&infer, bvar, NULL);
//         NecroType* lst2   = necro_create_type_list(&infer, avar, lst1);
//         NecroType* mcon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst2, 2);

//         NecroSub*  sub2   = necro_create_sub(&infer, ucon, (NecroID) { 2 }, NULL);
//         NecroSub*  sub    = necro_create_sub(&infer, icon, (NecroID) { 1 }, sub2);

//         NecroType* lst3   = necro_create_type_list(&infer, ucon, NULL);
//         NecroType* lst4   = necro_create_type_list(&infer, icon, lst3);
//         NecroType* match  = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst4, 2);

//         NecroType* result = necro_sub(&infer, sub, mcon);

//         necro_print_test_result(necro_types_match(result, match), "conSub1", result, "conSub2", match, &intern);
//     }

//     // Sub Print test
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);
//         NecroSub*  sub2 = necro_create_sub(&infer, icon, (NecroID) { 2 }, NULL);
//         NecroSub*  sub  = necro_create_sub(&infer, bcon, (NecroID) { 1 }, sub2);
//         // printf("\n");
//         // necro_print_sub(sub, &intern);
//     }

//     // Compose Subs test
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);
//         NecroSub*  sub  = necro_create_sub(&infer, bcon, (NecroID) { 2 }, NULL);
//         NecroSub*  sub2 = necro_create_sub(&infer, icon, (NecroID) { 1 }, NULL);

//         NecroSub*  sub3 = necro_compose(&infer, sub, sub2);
//         NecroSub*  msub = necro_create_sub(&infer, bcon, (NecroID) { 2 }, sub2);
//         // printf("\n");
//         // necro_print_sub(sub3, &intern);
//         necro_print_test_result(necro_subs_match(sub3, msub), "composeSub1", sub3->type, "composeSub2", msub->type, &intern);
//     }

//     // Compose Subs test 2
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);

//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);
//         NecroType* fun  = necro_create_type_fun(&infer, avar, bcon);

//         NecroSub*  sub  = necro_create_sub(&infer, fun,  (NecroID) { 2 }, NULL);
//         NecroSub*  sub2 = necro_create_sub(&infer, icon, (NecroID) { 1 }, NULL);

//         NecroSub*  sub3 = necro_compose(&infer, sub2, sub);

//         NecroType* fun2 = necro_create_type_fun(&infer, icon, bcon);
//         NecroSub*  msu2 = necro_create_sub(&infer, fun2, (NecroID) { 2 }, NULL);
//         NecroSub*  msur = necro_create_sub(&infer, icon, (NecroID) { 1 }, msu2);

//         // printf("\n");
//         // necro_print_sub(sub3,  &intern);
//         // necro_print_sub(msub3, &intern);
//         necro_print_test_sub_result(necro_subs_match(sub3, msur), "composeSub21", sub3, "composeSub22", msur, &intern);
//     }

//     // occurs test1
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* fun  = necro_create_type_fun(&infer, bvar, avar);
//         necro_print_test_result(necro_occurs(&infer, (NecroID) { 1 }, fun), "occurs1", fun, "occurs1", fun, &intern);
//     }

//     // occurs test2
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* fun  = necro_create_type_fun(&infer, bvar, avar);
//         necro_print_test_result(!necro_occurs(&infer, (NecroID) { 111 }, fun), "occurs2", fun, "occurs2", fun, &intern);
//     }

//     // TODO: Sort out how constructors work with occurs check, maybe move constructors over to using ids as well, and only use symbols for printing!
//     // occurs test3
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* lst1 = necro_create_type_list(&infer, bvar, NULL);
//         NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1, 1);
//         NecroType* fun  = necro_create_type_fun(&infer, avar, mcon);
//         necro_print_test_result(necro_occurs(&infer, (NecroID) { 2 }, fun), "occurs3", fun, "occurs3", fun, &intern);
//     }

//     // Unify test
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* lst1 = necro_create_type_list(&infer, bvar, NULL);
//         NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1, 1);
//         NecroType* fun  = necro_create_type_fun(&infer, avar, mcon);
//         NecroSub*  sub  = necro_unify(&infer, avar, mcon);
//         // printf("\n");
//         // necro_print_sub(sub, &intern);
//         // printf("\n");
//         // necro_print_type()
//         // necro_print_test_result(, "occurs3", fun, "occurs3", fun, &intern);
//     }

//     // Unify test2
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* cvar = necro_create_type_var(&infer, (NecroVar) { { 3 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);

//         NecroType* lst2 = necro_create_type_list(&infer, bvar, NULL);
//         NecroType* lst1 = necro_create_type_list(&infer, bcon, lst2);
//         NecroType* ecn1 = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst1, 2);

//         NecroType* lst4 = necro_create_type_list(&infer, icon, NULL);
//         NecroType* lst3 = necro_create_type_list(&infer, avar, lst4);
//         NecroType* ecn2 = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst3, 2);

//         // printf("\n");
//         // necro_print_type_sig(ecn1, &intern);
//         // printf("\n");
//         // necro_print_type_sig(ecn2, &intern);
//         NecroSub* sub  = necro_unify(&infer, ecn1, ecn2);
//         NecroSub* msub = necro_compose(&infer, necro_create_sub(&infer, bcon, (NecroID) { 1 }, NULL), necro_create_sub(&infer, icon, (NecroID) { 2 }, NULL));

//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }
//         necro_print_test_sub_result(necro_subs_match(sub, msub), "testUnify2", sub, "mtchUnify2", msub, &intern);
//     }

//     // Unify test3
//     {
//         NecroType* xvar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* yvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* zvar = necro_create_type_var(&infer, (NecroVar) { { 3 } });

//         NecroType* acon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "A"), { 0 } }, NULL, 0);

//         NecroType* t1l3 = necro_create_type_list(&infer, zvar, NULL);
//         NecroType* t1l2 = necro_create_type_list(&infer, yvar, t1l3);
//         NecroType* t1l1 = necro_create_type_list(&infer, xvar, t1l2);
//         NecroType* ty1  = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "J"), { 0 } }, t1l1, 3);

//         NecroType* f3l2 = necro_create_type_list(&infer, acon, NULL);
//         NecroType* f3l1 = necro_create_type_list(&infer, acon, f3l2);
//         NecroType* f3   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "F"), { 0 } }, f3l1, 2);

//         NecroType* f2l2 = necro_create_type_list(&infer, zvar, NULL);
//         NecroType* f2l1 = necro_create_type_list(&infer, zvar, f2l2);
//         NecroType* f2   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "F"), { 0 } }, f2l1, 2);

//         NecroType* f1l2 = necro_create_type_list(&infer, yvar, NULL);
//         NecroType* f1l1 = necro_create_type_list(&infer, yvar, f1l2);
//         NecroType* f1   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "F"), { 0 } }, f1l1, 2);

//         NecroType* t2l3 = necro_create_type_list(&infer, f3, NULL);
//         NecroType* t2l2 = necro_create_type_list(&infer, f2, t2l3);
//         NecroType* t2l1 = necro_create_type_list(&infer, f1, t2l2);
//         NecroType* ty2  = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "J"), { 0 } }, t2l1, 3);

//         // printf("\n");
//         // necro_print_type_sig(ty1, &intern);
//         // printf("\n");
//         // necro_print_type_sig(ty2, &intern);
//         NecroSub* sub  = necro_unify(&infer, ty1, ty2);

//         // (sub: x ==> F (F (F A A) (F A A)) (F (F A A) (F A A))
//         //  (sub: y ==> F (F A A) (F A A)
//         //   (sub: z ==> F A A
//         //    (sub: NULL))))

//         // necro_print_sub(sub, &intern);
//         NecroType* sty1 = necro_sub(&infer, sub, ty1);
//         NecroType* sty2 = necro_sub(&infer, sub, ty2);

//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty1));
//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty2));
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }

//         necro_print_test_result(necro_types_match(sty1, sty2), "testUnify3", sty1, "mtchUnify3", sty2, &intern);
//     }

//     // Unify test4
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);

//         NecroType* fun1 = necro_create_type_fun(&infer, avar, bcon);
//         NecroType* fun2 = necro_create_type_fun(&infer, icon, bvar);

//         // printf("\n");
//         // necro_print_type_sig(fun1, &intern);
//         // printf("\n");
//         // necro_print_type_sig(fun2, &intern);
//         NecroSub* sub = necro_unify(&infer, fun1, fun2);
//         // necro_print_sub(sub, &intern);

//         NecroType* sty1 = necro_sub(&infer, sub, fun1);
//         NecroType* sty2 = necro_sub(&infer, sub, fun2);

//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty1));
//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty2));
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }

//         necro_print_test_result(necro_types_match(sty1, sty2), "testUnify4", sty1, "mtchUnify4", sty2, &intern);
//     }

//     // Unify test5
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, NULL, 1);
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);

//         NecroType* app1 = necro_create_type_app(&infer, avar, bcon);
//         NecroType* app2 = necro_create_type_app(&infer, icon, bvar);

//         // printf("\n");
//         // necro_print_type_sig(app1, &intern);
//         // printf("\n");
//         // necro_print_type_sig(app2, &intern);
//         NecroSub* sub = necro_unify(&infer, app1, app2);
//         // necro_print_sub(sub, &intern);

//         NecroType* sty1 = necro_sub(&infer, sub, app1);
//         NecroType* sty2 = necro_sub(&infer, sub, app2);

//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty1));
//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty2));
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }

//         necro_print_test_result(necro_types_match(sty1, sty2), "testUnify5", sty1, "mtchUnify5", sty2, &intern);
//     }

//     // Uncurry test
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });


//         NecroType* lst4 = necro_create_type_list(&infer, bvar, NULL);
//         NecroType* lst3 = necro_create_type_list(&infer, avar, lst4);
//         NecroType* econ = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst3, 2);

//         NecroType* utyp = necro_curry_con(&infer, econ);

//         NecroType* lst5 = necro_create_type_list(&infer, avar, NULL);
//         NecroType* ccon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst5, 2);
//         NecroType* capp = necro_create_type_app(&infer, ccon, bvar);

//         necro_check_type_sanity(&infer, necro_uncurry(&infer, capp));
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("\nType error:\n    %s", infer.error.error_message);
//             return;
//         }
//         necro_print_test_result(necro_types_match(utyp, capp), "testUncurry1", utyp, "mtchUncurry1", capp, &intern);
//     }

//     // Unify test6
//     {
//         NecroType* avar = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL, 0);

//         NecroType* app1 = necro_create_type_app(&infer, avar, bcon);

//         NecroType* lst1 = necro_create_type_list(&infer, bvar, NULL);
//         NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1, 1);

//         // printf("\n");
//         // necro_print_type_sig(app1, &intern);
//         // printf("\n");
//         // necro_print_type_sig(mcon, &intern);
//         NecroSub* sub = necro_unify(&infer, app1, mcon);
//         // necro_print_sub(sub, &intern);

//         NecroType* sty1 = necro_uncurry(&infer, necro_sub(&infer, sub, app1));
//         NecroType* sty2 = necro_uncurry(&infer, necro_sub(&infer, sub, mcon));

//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty1));
//         necro_check_type_sanity(&infer, necro_uncurry(&infer, sty2));
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }
//         necro_print_test_result(necro_types_match(sty1, sty2), "testUnify6", sty1, "mtchUnify6", sty2, &intern);
//     }

//     // Typescheme test 1
//     {
//         NecroType*       avar    = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType*       lst1    = necro_create_type_list(&infer, avar, NULL);
//         NecroType*       mcon    = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1, 1);
//         NecroTypeScheme* term1   = necro_create_term(&infer, mcon);
//         NecroTypeScheme* scheme1 = necro_create_for_all(&infer, (NecroVar) { {1} }, term1);
//         printf("\n");
//         necro_print_type_scheme(scheme1, &intern);
//         printf("\n");
//         NecroType*       icon    = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroSub*        sub     = necro_create_sub(&infer, icon, (NecroID) { 1 }, NULL);
//         NecroTypeScheme* scheme2 = necro_sub_type_scheme(&infer, sub, scheme1);
//         necro_check_type_scheme_sanity(&infer, scheme1);
//         necro_check_type_scheme_sanity(&infer, scheme2);
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }
//         necro_print_test_scheme_result(necro_type_schemes_match(scheme1, scheme2), "testScheme1", scheme1, "mtchScheme1", scheme2, &intern);
//     }

//     // Typescheme test 2
//     {
//         NecroType*       avar    = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType*       bvar    = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType*       lst2    = necro_create_type_list(&infer, bvar, NULL);
//         NecroType*       lst1    = necro_create_type_list(&infer, avar, lst2);
//         NecroType*       econ    = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst1, 2);
//         NecroTypeScheme* term1   = necro_create_term(&infer, econ);
//         NecroTypeScheme* scheme1 = necro_create_for_all(&infer, (NecroVar) { {1} }, term1);

//         // printf("\n");
//         // necro_print_type_scheme(scheme1, &intern);
//         // printf("\n");

//         NecroType*       icon    = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroSub*        sub     = necro_create_sub(&infer, icon, (NecroID) { 2 }, NULL);
//         NecroTypeScheme* scheme2 = necro_sub_type_scheme(&infer, sub, scheme1);

//         // match
//         NecroType*       lst32   = necro_create_type_list(&infer, icon, NULL);
//         NecroType*       lst31   = necro_create_type_list(&infer, avar, lst32);
//         NecroType*       econ2   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst31, 2);
//         NecroTypeScheme* term3   = necro_create_term(&infer, econ2);
//         NecroTypeScheme* scheme3 = necro_create_for_all(&infer, (NecroVar) { { 1 } }, term3);

//         // printf("\n");
//         // necro_print_type_scheme(scheme2, &intern);
//         // printf("\n");

//         necro_check_type_sanity(&infer, icon);
//         necro_check_type_scheme_sanity(&infer, scheme2);
//         necro_check_type_scheme_sanity(&infer, scheme3);
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }
//         necro_print_test_scheme_result(necro_type_schemes_match(scheme2, scheme3), "testScheme2", scheme2, "mtchScheme2", scheme3, &intern);
//     }

//     // Typescheme test 2
//     {
//         NecroType*       avar    = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType*       bvar    = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType*       lst2    = necro_create_type_list(&infer, bvar, NULL);
//         NecroType*       lst1    = necro_create_type_list(&infer, avar, lst2);
//         NecroType*       econ    = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst1, 2);
//         NecroTypeScheme* term1   = necro_create_term(&infer, econ);
//         NecroTypeScheme* scheme1 = necro_create_for_all(&infer, (NecroVar) { {1} }, term1);

//         printf("\n");
//         necro_print_type_scheme(scheme1, &intern);
//         printf("\n");

//         NecroType*       mls1    = necro_create_type_list(&infer, avar, NULL);
//         NecroType*       mcon    = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, mls1, 1);
//         NecroSub*        sub     = necro_create_sub(&infer, mcon, (NecroID) { 2 }, NULL);
//         NecroTypeScheme* scheme2 = necro_sub_type_scheme(&infer, sub, scheme1);

//         // match
//         NecroType*       evar    = necro_create_type_var(&infer, (NecroVar) { { 4 } });
//         NecroType*       m2l1    = necro_create_type_list(&infer, evar, NULL);
//         NecroType*       mcon2   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, m2l1, 1);
//         NecroType*       lst32   = necro_create_type_list(&infer, mcon2, NULL);
//         NecroType*       lst31   = necro_create_type_list(&infer, evar, lst32);
//         NecroType*       econ2   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst31, 2);
//         NecroTypeScheme* term3   = necro_create_term(&infer, econ2);
//         NecroTypeScheme* scheme3 = necro_create_for_all(&infer, (NecroVar) { { 4 } }, term3);

//         // printf("\n");
//         // necro_print_type_scheme(scheme2, &intern);
//         // printf("\n");

//         necro_check_type_scheme_sanity(&infer, scheme2);
//         necro_check_type_scheme_sanity(&infer, scheme3);
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }

//         necro_print_test_scheme_result(necro_type_schemes_match(scheme2, scheme3), "testScheme3", scheme2, "mtchScheme3", scheme3, &intern);
//     }

//     // Gamma test1
//     {
//         NecroType*       avar    = necro_create_type_var(&infer, (NecroVar) { { 1 } });
//         NecroType*       bvar    = necro_create_type_var(&infer, (NecroVar) { { 2 } });
//         NecroType*       lst2    = necro_create_type_list(&infer, bvar, NULL);
//         NecroType*       lst1    = necro_create_type_list(&infer, avar, lst2);
//         NecroType*       econ1   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst1, 2);
//         NecroTypeScheme* term1   = necro_create_term(&infer, econ1);
//         NecroTypeScheme* scheme1 = necro_create_for_all(&infer, (NecroVar) { {1} }, term1);
//         NecroVar         evar    = (NecroVar) { { 5 } };

//         NecroType*       cvar    = necro_create_type_var(&infer, (NecroVar) { { 3 } });
//         NecroType*       dvar    = necro_create_type_var(&infer, (NecroVar) { { 4 } });
//         NecroType*       lst4    = necro_create_type_list(&infer, dvar, NULL);
//         NecroType*       lst3    = necro_create_type_list(&infer, cvar, lst4);
//         NecroType*       econ2   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst3, 2);
//         NecroTypeScheme* term2   = necro_create_term(&infer, econ2);
//         NecroTypeScheme* scheme2 = necro_create_for_all(&infer, (NecroVar) { {3} }, term2);
//         NecroVar         fvar    = (NecroVar) { { 6 } };

//         NecroGamma*      gamma   = necro_create_gamma(&infer, fvar, scheme2, necro_create_gamma(&infer, evar, scheme1, NULL));

//         necro_check_type_scheme_sanity(&infer, scheme1);
//         necro_check_type_scheme_sanity(&infer, scheme2);
//         necro_check_gamma_sanity(&infer, gamma);
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }

//         necro_print_gamma(gamma, &intern);
//         printf(" (passed)\n");

//         NecroTypeScheme* scheme3 = necro_gamma_find(&infer, gamma, evar);
//         NecroTypeScheme* scheme4 = necro_gamma_find(&infer, gamma, fvar);
//         necro_check_type_scheme_sanity(&infer, scheme1);
//         necro_check_type_scheme_sanity(&infer, scheme2);
//         printf("\ntestGammaFind :: ");
//         necro_print_type_scheme(scheme3, &intern);
//         printf("\nmtchGammaFind :: ");
//         necro_print_type_scheme(scheme4, &intern);
//         printf("\n");
//         if (scheme3 == scheme1 && scheme4 == scheme2)
//             printf(" (passed)\n");
//         else
//             printf(" (FAILED)\n");

//         // Gamma Sub test
//         NecroType*  icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
//         NecroType*  fcon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Float"), { 0 } }, NULL, 0);
//         NecroSub*   sub    = necro_create_sub(&infer, fcon, (NecroID) { 2 }, necro_create_sub(&infer, icon, (NecroID) { 4 }, NULL));
//         NecroGamma* gamma2 = necro_sub_gamma(&infer, sub, gamma);
//         necro_check_gamma_sanity(&infer, gamma2);
//         if (infer.error.return_code != NECRO_SUCCESS)
//         {
//             printf("Type error:\n    %s", infer.error.error_message);
//             return;
//         }
//         printf("\ngammaSubTest");
//         necro_print_gamma(gamma2, &intern);
//         printf(" (passed)\n");
//     }

//     necro_destroy_infer(&infer);
//     necro_destroy_intern(&intern);
// }
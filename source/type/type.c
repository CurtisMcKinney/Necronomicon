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
#include "type.h"

#define NECRO_TYPE_DEBUG 0
#if NECRO_TYPE_DEBUG
#define TRACE_TYPE(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE(...)
#endif

 // TODO: make necro_type_string

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

NecroInfer necro_create_infer(NecroIntern* intern, struct NecroSymTable* symtable, NecroPrimTypes prim_types, struct NecroTypeClassEnv* type_class_env)
{

    NecroInfer infer = (NecroInfer)
    {
        .intern         = intern,
        .arena          = necro_create_paged_arena(),
        .env            = necro_create_type_env(512),
        .error          = necro_create_error(),
        .highest_id     = symtable->count,
        .symtable       = symtable,
        .prim_types     = prim_types,
        .type_class_env = type_class_env,
    };
    NecroKind* star_kind = necro_paged_arena_alloc(&infer.arena, sizeof(NecroKind));
    star_kind->kind      = NECRO_KIND_STAR;
    infer.star_kind      = star_kind;
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

void* necro_infer_error(NecroInfer* infer, const char* error_preamble, NecroType* type, const char* error_message, ...)
{
    if (infer->error.return_code != NECRO_SUCCESS)
        return type;
    va_list args;
	va_start(args, error_message);
    NecroSourceLoc source_loc = (type != NULL) ? type->source_loc : infer->error.source_loc;
    size_t count = necro_verror(&infer->error, source_loc, error_message, args);
    if (error_preamble != NULL)
        count += snprintf(infer->error.error_message + count, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n %s", error_preamble);
    count += snprintf(infer->error.error_message + count, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n Found in type: ");
    if (type != NULL)
        necro_snprintf_type_sig(type, infer->intern, infer->error.error_message + count, NECRO_MAX_ERROR_MESSAGE_LENGTH);

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
    type->kind         = NULL;
    // type->kind         = necro_create_kind_init(infer);
    return type;
}

NecroType* necro_create_type_var(NecroInfer* infer, NecroVar var)
{
    if (var.id.id > infer->highest_id)
        infer->highest_id = var.id.id + 1;

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

    if (infer->env.data[var.id.id] != NULL)
        return infer->env.data[var.id.id];

    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_VAR;
    type->kind      = NULL;
    type->var       = (NecroTypeVar)
    {
        .var               = var,
        .arity             = -1,
        .is_rigid          = false,
        .is_type_class_var = false,
        .context           = NULL,
        .bound             = NULL,
        .scope             = NULL,
        .weight            = NECRO_WEIGHT_PI,
    };

    infer->env.data[type->var.var.id.id] = type;
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
    // type->kind = necro_apply_kinds(infer, type1, type2, type1, NULL);
    return type;
}

NecroType* necro_create_type_fun(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_FUN;
    type->fun       = (NecroTypeFun)
    {
        .type1     = type1,
        .type2     = type2,
        .is_linear = false,
    };
    return type;
}

NecroType* necro_declare_type(NecroInfer* infer, NecroCon con, size_t arity)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con      = con,
        .args     = NULL,
        .arity    = arity,
        .is_class = false,
    };
    type->kind         = infer->star_kind;
    type->pre_supplied = true;
    while (arity > 0)
    {
        type->kind = necro_create_kind_app(infer, type->kind, infer->star_kind);
        arity--;
    }
    type->con.arity = arity;
    assert(necro_symtable_get(infer->symtable, con.id)->type == NULL);
    necro_symtable_get(infer->symtable, con.id)->type = type;
    return type;
}

NecroType* necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args, size_t arity)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con      = con,
        .args     = args,
        .arity    = arity,
        .is_class = false,
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
    type->kind = NULL;
    return type;
}

NecroType* necro_create_for_all(NecroInfer* infer, NecroVar var, NecroTypeClassContext* context, NecroType* type)
{
    if (var.id.id > infer->highest_id)
        infer->highest_id = var.id.id + 1;
    NecroType* for_all  = necro_alloc_type(infer);
    for_all->type       = NECRO_TYPE_FOR;
    for_all->for_all    = (NecroTypeForAll)
    {
        .var     = var,
        .context = context,
        .type    = type,
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

NecroType* necro_new_name(NecroInfer* infer, NecroSourceLoc source_loc)
{
    infer->highest_id++;
    NecroVar   var       = (NecroVar) { .id = { infer->highest_id }, .symbol = NULL_SYMBOL };
    NecroType* type_var  = necro_create_type_var(infer, var);
    type_var->kind       = NULL;
    type_var->source_loc = source_loc;
    return type_var;
}

//=====================================================
// Kinds
// ---------------------------------------------------
// Kinds start off as init.
// If no kind is inferred, an init kind defaults to *
//=====================================================
inline NecroKind* necro_alloc_kind(NecroInfer* infer)
{
    NecroKind* kind = necro_paged_arena_alloc(&infer->arena, sizeof(NecroKind));
    kind->kind      = NECRO_KIND_INIT;
    return kind;
}

NecroKind* necro_create_kind_init(NecroInfer* infer)
{
    return necro_alloc_kind(infer);
}

NecroKind* necro_create_kind_app(NecroInfer* infer, NecroKind* kind1, NecroKind* kind2)
{
    NecroKind* kind = necro_alloc_kind(infer);
    kind->kind      = NECRO_KIND_APP;
    kind->app.kind1 = kind1;
    kind->app.kind2 = kind2;
    return kind;
}

NecroKind* necro_not_enough_args_kind_error(NecroInfer* infer, NecroType* type1, NecroType* macro_type, const char* error_preamble)
{
    if (type1->type == NECRO_TYPE_CON)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Type \'%s\' is not applied to enough arguments, found kind: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_kind_string(infer, type1->kind));
    }
    else if (type1->type == NECRO_TYPE_VAR)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Type variable \'%s\' is not applied to enough arguments, found kind: %s", necro_intern_get_string(infer->intern, type1->var.var.symbol), necro_kind_string(infer, type1->kind));
    }
    else
    {
        necro_infer_error(infer, error_preamble, macro_type, "Type is applied to too many arguments, found kind: %s", necro_kind_string(infer, type1->kind));
    }
    return NULL;
}

NecroKind* necro_too_many_args_kind_error(NecroInfer* infer, NecroType* type1, NecroType* macro_type, const char* error_preamble)
{
    if (type1->type == NECRO_TYPE_CON)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Type \'%s\' is applied to too many arguments, found kind: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_kind_string(infer, type1->kind));
    }
    else if (type1->type == NECRO_TYPE_VAR)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Type variable \'%s\' is applied to too many arguments, found kind: %s", necro_intern_get_string(infer->intern, type1->var.var.symbol), necro_kind_string(infer, type1->kind));
    }
    else
    {
        necro_infer_error(infer, error_preamble, macro_type, "Type is applied to too many arguments, found kind: %s", necro_kind_string(infer, type1->kind));
    }
    return NULL;
}

inline bool necro_is_simple_kind(NecroKind* kind)
{
    switch (kind->kind)
    {
    case NECRO_KIND_INIT: return true;
    case NECRO_KIND_STAR: return true;
    case NECRO_KIND_APP:  return false;
    default:              assert(false);  return true;
    }
}

void necro_print_kind(NecroKind* kind)
{
    if (kind == NULL)
    {
        // printf("NULL_KIND");
        return;
    }
    switch (kind->kind)
    {
    case NECRO_KIND_INIT: printf("?"); break;
    case NECRO_KIND_STAR: printf("*"); break;
    case NECRO_KIND_APP:
        // if (!necro_is_simple_kind(kind->app.kind1)) printf("(");
        necro_print_kind(kind->app.kind1);
        // if (!necro_is_simple_kind(kind->app.kind1)) printf(")");
        printf(" -> ");
        necro_print_kind(kind->app.kind2);
        break;
    default: assert(false);
    }
}

void necro_unify_kinds(NecroInfer* infer, NecroType* type1, NecroKind** kind1, NecroKind** kind2, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(type1 != NULL);
    assert(kind1 != NULL);
    assert(*kind1 != NULL);
    assert(kind2 != NULL);
    assert(*kind2 != NULL);
    if (*kind1 == *kind2) return;
    switch ((*kind1)->kind)
    {
    case NECRO_KIND_INIT: *kind1 = *kind2; break;
    case NECRO_KIND_STAR:
        switch ((*kind2)->kind)
        {
        case NECRO_KIND_INIT: (*kind2) = (*kind1); break;
        case NECRO_KIND_STAR: break;
        case NECRO_KIND_APP:  necro_too_many_args_kind_error(infer, type1, macro_type, error_preamble); break;
        default: assert(false);
        }
        break;
    case NECRO_KIND_APP:
        switch ((*kind2)->kind)
        {
        case NECRO_KIND_INIT: (*kind2) = (*kind1); break;
        case NECRO_KIND_STAR: necro_not_enough_args_kind_error(infer, type1, macro_type, error_preamble); break;
        case NECRO_KIND_APP:
            necro_unify_kinds(infer, type1, &(*kind1)->app.kind1, &(*kind1)->app.kind1, macro_type, error_preamble);
            necro_unify_kinds(infer, type1, &(*kind1)->app.kind2, &(*kind2)->app.kind2, macro_type, error_preamble);
            break;
        default: assert(false);
        }
        break;
    default: assert(false);
    }
}

char* necro_kind_string_go(NecroInfer* infer, NecroKind* kind, char* buffer, size_t max_buffer_length)
{
    switch (kind->kind)
    {
    case NECRO_KIND_INIT:
        buffer += snprintf(buffer, max_buffer_length, "?");
        return buffer;
    case NECRO_KIND_STAR:
        buffer += snprintf(buffer, max_buffer_length, "*");
        return buffer;
    case NECRO_KIND_APP:
        buffer  = necro_kind_string_go(infer, kind->app.kind1, buffer, max_buffer_length);
        buffer += snprintf(buffer, max_buffer_length, " -> ");
        buffer  = necro_kind_string_go(infer, kind->app.kind1, buffer, max_buffer_length);
        return buffer;
    default: assert(false);
    }
    return buffer;
}

char* necro_kind_string(NecroInfer* infer, NecroKind* kind)
{
    static const MAX_KIND_BUFFER_LENGTH = 512;
    char* buffer     = necro_paged_arena_alloc(&infer->arena, MAX_KIND_BUFFER_LENGTH * sizeof(char));
    char* buffer_end = necro_kind_string_go(infer, kind, buffer, MAX_KIND_BUFFER_LENGTH);
    *buffer_end = '\0';
    return buffer;
}

NecroKind* necro_infer_kind(NecroInfer* infer, NecroType* type, NecroKind* kind_to_match, NecroType* macro_type, const char* error_preamble)
{
    assert(infer != NULL);
    assert(type != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    if (type->kind != NULL)
    {
        if (kind_to_match != NULL)
            necro_unify_kinds(infer, type, &type->kind, &kind_to_match, macro_type, error_preamble);
        return type->kind;
    }
    // NecroKind  star_kind = (NecroKind) { .kind = NECRO_KIND_STAR };
    // NecroKind* star_kptr = &star_kind;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->kind == NULL)
        {
            type->kind = necro_create_kind_init(infer);
        }
        if (kind_to_match != NULL)
            necro_unify_kinds(infer, type, &type->kind, &kind_to_match, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        return type->kind;
    }
    case NECRO_TYPE_APP:
    {
        NecroKind* type2_kind = infer->star_kind;
        NecroKind* type1_expect = (kind_to_match != NULL) ?
            necro_create_kind_app(infer, kind_to_match, type2_kind) :
            necro_create_kind_app(infer, necro_create_kind_init(infer), type2_kind);
        NecroKind* type1_kind   = necro_infer_kind(infer, type->app.type1, type1_expect, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        type->kind              = type1_kind->app.kind1;
        if (kind_to_match != NULL)
            necro_unify_kinds(infer, type, &type->kind, &kind_to_match, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        return type->kind;
    }
    case NECRO_TYPE_FUN:
    {
        necro_infer_kind(infer, type->fun.type1, infer->star_kind, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        necro_infer_kind(infer, type->fun.type2, infer->star_kind, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        type->kind = infer->star_kind;
        if (kind_to_match != NULL)
            necro_unify_kinds(infer, type, &type->kind, &kind_to_match, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        return type->kind;
    }
    case NECRO_TYPE_LIST: assert(false); return NULL;
    case NECRO_TYPE_CON:
    {
        assert(necro_symtable_get(infer->symtable, type->con.con.id)->type != NULL);
        type->kind = necro_symtable_get(infer->symtable, type->con.con.id)->type->kind;
        if (type->kind != NULL)
        {
            NecroType* args = type->con.args;
            while (args != NULL)
            {
                if (type->kind->kind != NECRO_KIND_APP)
                {
                    necro_too_many_args_kind_error(infer, type, macro_type, error_preamble);
                }
                necro_infer_kind(infer, args->list.item, infer->star_kind, macro_type, error_preamble);
                if (necro_is_infer_error(infer)) return NULL;
                type->kind = type->kind->app.kind1;
                args = args->list.next;
            }
            if (kind_to_match != NULL)
                necro_unify_kinds(infer, type, &type->kind, &kind_to_match, macro_type, error_preamble);
            if (necro_is_infer_error(infer)) return NULL;
        }
        else
        {
            assert(false);
        }
        return type->kind;
    }
    case NECRO_TYPE_FOR:
    {
        // NecroKind* for_all_type_kind = necro_infer_kind(infer, type->for_all.type, infer->star_kind, macro_type, error_preamble);
        NecroKind* for_all_type_kind = necro_infer_kind(infer, type->for_all.type, NULL, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        // type->kind = infer->star_kind;
        type->kind = for_all_type_kind;
        if (kind_to_match != NULL)
            necro_unify_kinds(infer, type, &type->kind, &kind_to_match, macro_type, error_preamble);
        if (necro_is_infer_error(infer)) return NULL;
        return type->kind;
    }
    default: return necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Unimplemented Type in necro_infer_kind: %d", type->type);
    }
}

//=====================================================
// NecroTypeEnv
//=====================================================
void necr_bind_type_var(NecroInfer* infer, NecroVar var, NecroType* type)
{
    if (var.id.id >= infer->env.capacity)
    {
        necro_infer_error(infer, NULL, NULL, "Cannot find variable %s", necro_id_as_character_string(infer, var));
    }
    assert(infer->env.data != NULL);
    infer->env.data[var.id.id]->var.bound = type;
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
            return necro_create_for_all(infer, type->for_all.var, type->for_all.context, new_type);
    }

    default:
        return necro_infer_error(infer, "During necro_uncurry", type, "Compiler bug: Unrecognized type: %d.", type->type);
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
        prev = type;
        type = type->var.bound;
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
void necro_rigid_type_variable_error(NecroInfer* infer, NecroVar type_var, NecroType* type, NecroType* macro_type, const char* error_preamble)
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
        type_name = necro_id_as_character_string(infer, type->var.var);
    else
        assert(false);
    const char* var_name = necro_id_as_character_string(infer, type_var);
    necro_infer_error(infer, error_preamble, macro_type, "Couldn't match type \'%s\' with type \'%s\'.\n\'%s\' is a rigid type variable bound by a type signature.", var_name, type_name, var_name);
}

void necro_type_is_not_instance_error(NecroInfer* infer, NecroType* type, NecroTypeClassContext* type_class, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    necro_infer_error(infer, error_preamble, macro_type, "\'%s\' is not an instance of class \'%s\'", necro_intern_get_string(infer->intern, type->con.con.symbol), necro_intern_get_string(infer->intern, type_class->type_class_name.symbol));
}

void necro_occurs_error(NecroInfer* infer, NecroVar type_var, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    necro_infer_error(infer, error_preamble, macro_type, "Occurs check error, cannot construct infinite type: %s ~ ", necro_id_as_character_string(infer, type_var));
}

bool necro_occurs(NecroInfer* infer, NecroType* type_var, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    if (type == NULL)
        return false;
    NecroVar name = type_var->var.var;
    type          = necro_find(infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        if (name.id.id == type->var.var.id.id)
        {
            necro_occurs_error(infer, type_var->var.var, type, macro_type, error_preamble);
            return true;
        }
        else
        {
            return false;
        }
    case NECRO_TYPE_APP:  return necro_occurs(infer, type_var, type->app.type1, macro_type, error_preamble) || necro_occurs(infer, type_var, type->app.type2, macro_type, error_preamble);
    case NECRO_TYPE_FUN:  return necro_occurs(infer, type_var, type->fun.type1, macro_type, error_preamble) || necro_occurs(infer, type_var, type->fun.type2, macro_type, error_preamble);
    case NECRO_TYPE_CON:  return necro_occurs(infer, type_var, type->con.args , macro_type, error_preamble);
    case NECRO_TYPE_LIST: return necro_occurs(infer, type_var, type->list.item, macro_type, error_preamble) || necro_occurs(infer, type_var, type->list.next, macro_type, error_preamble);
    case NECRO_TYPE_FOR:  return necro_infer_error(infer, error_preamble, type, "Found polytype in occurs check!");
    default:              return necro_infer_error(infer, error_preamble, type, "Compiler bug: Unrecognized type: %d.", type->type);
    }
}

void necro_propogate_type_classes(NecroInfer* infer, NecroTypeClassContext* classes, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    if (classes == NULL)
        return;
    if (type == NULL)
        return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        if (type->var.is_rigid)
        {
            // If it's a rigid variable, make sure it has all of the necessary classes in its context already
            while (classes != NULL)
            {
                if (!necro_context_contains_class(infer->type_class_env, type->var.context, classes))
                {
                    necro_infer_error(infer, error_preamble, macro_type, "No instance for \'%s %s\'", necro_intern_get_string(infer->intern, classes->type_class_name.symbol), necro_id_as_character_string(infer, type->var.var));
                    return;
                }
                classes = classes->next;
            }
        }
        else
        {
            type->var.context = necro_union_contexts(infer, type->var.context, classes);
        }
        return;
    }
    case NECRO_TYPE_CON:
    {
        while (classes != NULL)
        {
            NecroTypeClassInstance* instance = necro_get_instance(infer, type->con.con, classes->type_class_name);
            if (instance == NULL)
            {
                necro_type_is_not_instance_error(infer, type, classes, macro_type, error_preamble);
                return;
            }
            NecroType* current_arg = type->con.args;
            while (current_arg != NULL)
            {
                necro_propogate_type_classes(infer, instance->context, current_arg->list.item, macro_type, error_preamble);
                if (necro_is_infer_error(infer)) return;
                current_arg = current_arg->list.next;
            }
            if (necro_is_infer_error(infer)) return;
            classes = classes->next;
        }
        return;
    }
    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: TypeApp not implemented in necro_propogate_type_classes!"); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Found ConTypeList in necro_propogate_type_classes!"); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Found polytype in necro_propogate_type_classes!"); return;
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "(->) Not implemented for type classes!", type->type); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Unrecognized type: %d.", type->type); return;
    }
}

inline void necro_instantiate_type_var(NecroInfer* infer, NecroTypeVar* type_var, NecroType* type, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    necro_propogate_type_classes(infer, type_var->context, type, macro_type, error_preamble);
    if (necro_is_infer_error(infer)) return;
    necr_bind_type_var(infer, type_var->var, type);
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
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        if (type1->var.var.id.id == type2->var.var.id.id)
            return;
        else if (type1->var.is_rigid && type2->var.is_rigid)
            necro_rigid_type_variable_error(infer, type1->var.var, type2, macro_type, error_preamble);
        else if (necro_occurs(infer, type1, type2, macro_type, error_preamble))
            return;
        else if (type1->var.is_rigid)
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
        else if (type2->var.is_rigid)
            necro_instantiate_type_var(infer, &type1->var, type2, macro_type, error_preamble);
        else if (necro_is_bound_in_scope(infer, type1, scope))
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
        else
            necro_instantiate_type_var(infer, &type1->var, type2, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (type1->var.is_rigid)
        {
            necro_rigid_type_variable_error(infer, type1->var.var, type2, macro_type, error_preamble);
            return;
        }
        if (necro_occurs(infer, type1, type2, macro_type, error_preamble))
        {
            return;
        }
        necro_instantiate_type_var(infer, &type1->var, type2, macro_type, error_preamble);
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify TypeVar with type args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_app(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_APP);
    if (type2 == NULL)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol));
        return;
    }
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->var.is_rigid)
            necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
        else if (necro_occurs(infer, type2, type1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    case NECRO_TYPE_APP:
        necro_unify(infer, type1->app.type1, type2->app.type1, scope, macro_type, error_preamble);
        necro_unify(infer, type1->app.type2, type2->app.type2, scope, macro_type, error_preamble);
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_curry_con(infer, type2);
        // necro_infer_kind(infer, uncurried_con, NULL, macro_type, error_preamble);
        if (uncurried_con == NULL)
        {
            necro_infer_error(infer, error_preamble, macro_type, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type2->con.con.symbol));
        }
        else
        {
            // necro_unify_app(infer, type1, uncurried_con, scope, macro_type, error_preamble);
            necro_unify(infer, type1, uncurried_con, scope, macro_type, error_preamble);
        }
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify TypeApp with (->)."); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify TypeApp with type args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_fun(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
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
            necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
        else if (necro_occurs(infer, type2, type1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
        // necro_unify_kinds(infer, type1, &type1->kind, &type2->kind, macro_type, error_preamble);
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    case NECRO_TYPE_FUN:  necro_unify(infer, type1->fun.type1, type2->fun.type1, scope, macro_type, error_preamble); necro_unify(infer, type1->fun.type2, type2->fun.type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify (->) with TypeApp."); return;
    case NECRO_TYPE_CON:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify (->) with TypeCon (%s)", necro_intern_get_string(infer->intern, type2->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify (->) with type args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

inline void necro_unify_con(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
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
            necro_rigid_type_variable_error(infer, type2->var.var, type1, macro_type, error_preamble);
        else if (necro_occurs(infer, type2, type1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_type_var(infer, &type2->var, type1, macro_type, error_preamble);
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
        if (type1->con.con.symbol.id != type2->con.con.symbol.id)
        {
            necro_infer_error(infer, error_preamble, type1, "Attempting to unify two different types, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
        }
        // else if (type1->con.arity != type2->con.arity)
        // {
        //     necro_infer_error(infer, error_preamble, type1, "Mismatched arities, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
        // }
        else
        {
            necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
            type1 = type1->con.args;
            type2 = type2->con.args;
            while (type1 != NULL && type2 != NULL)
            {
                if (type1 == NULL || type2 == NULL)
                {
                    necro_infer_error(infer, error_preamble, type1, "Mismatched arities, Type1: %s Type2: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol), necro_intern_get_string(infer->intern, type2->con.con.symbol));
                    return;
                }
                assert(type1->type == NECRO_TYPE_LIST);
                assert(type2->type == NECRO_TYPE_LIST);
                necro_unify(infer, type1->list.item, type2->list.item, scope, macro_type, error_preamble);
                necro_unify_kinds(infer, type2, &type2->list.item->kind, &type1->list.item->kind, macro_type, error_preamble);
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
            necro_infer_error(infer, error_preamble, type1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, type1->con.con.symbol));
        }
        else
        {
            necro_unify(infer, uncurried_con, type2, scope, macro_type, error_preamble);
        }
        necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type1->con.con.symbol)); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify polytype."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
    }
}

// Can only unify with a polymorphic type on the left side
void necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(type1 != NULL);
    assert(type2 != NULL);
    necro_infer_kind(infer, type1, NULL, macro_type, error_preamble);
    necro_infer_kind(infer, type2, NULL, macro_type, error_preamble);
    type1 = necro_find(infer, type1);
    type2 = necro_find(infer, type2);
    if (type1 == type2)
        return;
    necro_infer_kind(infer, type1, NULL, macro_type, error_preamble);
    necro_infer_kind(infer, type2, NULL, macro_type, error_preamble);
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  necro_unify_var(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_unify_app(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_FUN:  necro_unify_fun(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:  necro_unify_con(infer, type1, type2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, type1, "Compiler bug: Found Type args list in necro_unify"); return;
    case NECRO_TYPE_FOR:
    {
        while (type1->type == NECRO_TYPE_FOR)
            type1 = type1->for_all.type;
        necro_unify(infer, type1, type2, scope, macro_type, error_preamble);
        return;
    }
    default: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type); return;
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

NecroInstSub* necro_create_inst_sub(NecroInfer* infer, NecroVar var_to_replace, NecroSourceLoc source_loc, NecroTypeClassContext* context, NecroInstSub* next)
{
    NecroType* type_var      = necro_new_name(infer, source_loc);
    type_var->var.var.symbol = var_to_replace.symbol;
    if (necro_symtable_get(infer->symtable, var_to_replace.id) != NULL && necro_symtable_get(infer->symtable, var_to_replace.id)->type != NULL && necro_symtable_get(infer->symtable, var_to_replace.id)->type->kind != NULL)
    {
        type_var->kind = necro_symtable_get(infer->symtable, var_to_replace.id)->type->kind;
    }
    else if (infer->env.capacity > var_to_replace.id.id && infer->env.data[var_to_replace.id.id] != NULL && infer->env.data[var_to_replace.id.id]->kind != NULL)
    {
        type_var->kind = infer->env.data[var_to_replace.id.id]->kind;
    }
    else
    {
        // Is this bad?
        // assert(false);
    }
    NecroInstSub* sub = necro_paged_arena_alloc(&infer->arena, sizeof(NecroInstSub));
    *sub              = (NecroInstSub)
    {
        .var_to_replace = var_to_replace,
        .new_name       = type_var,
        .next           = next,
    };
    sub->new_name->var.context = context;
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
    case NECRO_TYPE_FOR:  return necro_infer_error(infer, NULL, type, "Compiler bug: Found Polytype in necro_inst_go");
    default:              return necro_infer_error(infer, NULL, type, "Compiler bug: Non-existent type %d found in necro_inst.", type->type);
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
        subs         = necro_create_inst_sub(infer, current_type->for_all.var, type->source_loc, current_type->for_all.context, subs);
        current_type = current_type->for_all.type;
    }
    NecroType* result = necro_inst_go(infer, current_type, subs, scope);
    // necro_infer_kind(infer, result, infer->star_kind, result, NULL);
    necro_infer_kind(infer, result, NULL, result, NULL);
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
    if (top->type == NECRO_TYPE_VAR && top->var.is_rigid && !top->var.is_type_class_var)
    {
        prev_result.type = top;
        return prev_result;
    }

    if (necro_is_bound_in_scope(infer, type, scope) && !type->var.is_type_class_var)
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
                if (subs->var_to_replace.id.id == type->var.var.id.id)
                {
                    prev_result.type = subs->type_var;
                    return prev_result;
                }
                subs = subs->next;
            }
            NecroGenSub* sub = necro_paged_arena_alloc(&infer->arena, sizeof(NecroGenSub));
            NecroType*   type_var;
            if (!type->var.is_type_class_var)
            {
                type_var                 = necro_new_name(infer, type->source_loc);
                type_var->var.var.symbol = type->var.var.symbol;
                type_var->var.is_rigid   = true;
                type_var->var.context    = type->var.context;
            }
            else
            {
                type_var = type;
            }
            NecroType* for_all = necro_create_for_all(infer, type_var->var.var, type->var.context, NULL);
            *sub               = (NecroGenSub)
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
    // case NECRO_TYPE_FOR: necro_infer_error(infer, NULL, type, "Compiler bug: Found Polytype in necro_gen"); return (NecroGenResult) { NULL, NULL, NULL };
    case NECRO_TYPE_FOR: assert(false); return (NecroGenResult) { NULL, NULL, NULL };
    default:             necro_infer_error(infer, NULL, type, "Compiler bug: Non-existent type %d found in necro_gen.", type->type); return (NecroGenResult) { NULL, NULL, NULL };
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
                // necro_infer_kind(infer, head, infer->star_kind, head, NULL);
                necro_infer_kind(infer, head, NULL, head, NULL);
                return head;
            }
        }
    }
    else
    {
        // necro_infer_kind(infer, result.type, infer->star_kind, result.type, NULL);
        necro_infer_kind(infer, result.type, NULL, result.type, NULL);
        return result.type;
    }
}

//=====================================================
// Printing
//=====================================================
const char* necro_id_as_character_string(NecroInfer* infer, NecroVar var)
{
    if (var.symbol.id != 0)
    {
        return necro_intern_get_string(infer->intern, var.symbol);
    }
    NecroID id = var.id;
    size_t length = (id.id / 26) + 1;
    char*  buffer = necro_paged_arena_alloc(&infer->arena, (length + 1) * sizeof(char));
    if (id.id <= 26)
        snprintf(buffer, length, "%c", (char)((id.id % 26) + 97));
    else
        snprintf(buffer, length, "%c%d", (char)((id.id % 26) + 97), (id.id - 26) / 26);
    buffer[length+1] = '\0';
    return buffer;
}

void necro_print_id_as_characters(NecroVar var, NecroIntern* intern)
{
    if (var.symbol.id != 0)
    {
        printf("%s", necro_intern_get_string(intern, var.symbol));
        return;
    }
    NecroID id = var.id;
    if (id.id <= 26)
        printf("%c", (char)((id.id % 26) + 97));
    else
        printf("%c%d", (char)((id.id % 26) + 97), (id.id - 26) / 26);

}

char* necro_snprintf_id_as_characters(NecroVar var, NecroIntern* intern, char* buffer, size_t buffer_size)
{
    if (var.symbol.id != 0)
    {
        return buffer + snprintf(buffer, buffer_size, "%s", necro_intern_get_string(intern, var.symbol));
    }
    NecroID id = var.id;
    if (id.id <= 26)
        return buffer + snprintf(buffer, buffer_size, "%c", (char)((id.id % 26) + 97));
    else
        return buffer + snprintf(buffer, buffer_size, "%c%d", (char)((id.id % 26) + 97), (id.id - 26) / 26);
}

bool necro_is_simple_type(NecroType* type)
{
    assert(type != NULL);
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

    if (type->con.args == NULL) return true;

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
        necro_print_id_as_characters(type->var.var, intern);
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

        // Print quantified type vars
        NecroType* for_all_head = type;
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            necro_print_id_as_characters(type->for_all.var, intern);
            printf(" ");
            type = type->for_all.type;
        }
        necro_print_id_as_characters(type->for_all.var, intern);
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
                NecroVar               current_context_var = type->for_all.var;
                NecroTypeClassContext* context             = type->for_all.context;
                while (context != NULL)
                {
                    if (count > 0)
                        printf(", ");
                    printf("%s ", necro_intern_get_string(intern, context->type_class_name.symbol));
                    necro_print_id_as_characters(current_context_var, intern);
                    context = context->next;
                    count++;
                }
                type = type->for_all.type;
            }
            printf(") => ");
        }

        // Print rest of type
        necro_print_type_sig_go(type, intern);
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
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        return necro_snprintf_id_as_characters(type->var.var, intern, buffer, buffer_length);

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
    {
        NecroType* for_all_head = type;
        // TODO: Add context printing!
        buffer += snprintf(buffer, buffer_length, "forall ");
        while (type->for_all.type->type == NECRO_TYPE_FOR)
        {
            buffer  = necro_snprintf_id_as_characters(type->for_all.var, intern, buffer, buffer_length);
            buffer += snprintf(buffer, buffer_length, " ");
            type    = type->for_all.type;
        }
        buffer      = necro_snprintf_id_as_characters(type->for_all.var, intern, buffer, buffer_length);
        buffer     += snprintf(buffer, buffer_length, ". ");

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
            buffer += snprintf(buffer, buffer_length, "(");
            size_t count = 0;
            type = for_all_head;
            while (type->type == NECRO_TYPE_FOR)
            {
                NecroVar               current_context_var = type->for_all.var;
                NecroTypeClassContext* context             = type->for_all.context;
                while (context != NULL)
                {
                    if (count > 0)
                        buffer += snprintf(buffer, buffer_length, ", ");
                    buffer += snprintf(buffer, buffer_length, "%s ", necro_intern_get_string(intern, context->type_class_name.symbol));
                    buffer = necro_snprintf_id_as_characters(current_context_var, intern, buffer, buffer_length);
                    context = context->next;
                    count++;
                }
                type = type->for_all.type;
            }
            buffer += snprintf(buffer, buffer_length, ") => ");
        }

        return necro_snprintf_type_sig(type, intern, buffer, buffer_length);
    }

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
    // redo with unification
    bool passed = true;
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
        necro_print_id_as_characters((NecroVar) { .id = (NecroID) { i }, .symbol = (NecroSymbol) { .id = 0, .hash = 0 } }, infer->intern);
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
    NecroCon con;
    switch (tuple_count)
    {
    case 2:  con = infer->prim_types.tuple_types.two;   break;
    case 3:  con = infer->prim_types.tuple_types.three; break;
    case 4:  con = infer->prim_types.tuple_types.four;  break;
    case 5:  con = infer->prim_types.tuple_types.five;  break;
    case 6:  con = infer->prim_types.tuple_types.six;   break;
    case 7:  con = infer->prim_types.tuple_types.seven; break;
    case 8:  con = infer->prim_types.tuple_types.eight; break;
    case 9:  con = infer->prim_types.tuple_types.nine;  break;
    case 10: con = infer->prim_types.tuple_types.ten;   break;
    default: return necro_infer_error(infer, NULL, NULL, "Tuple size too large: %d", tuple_count);
    }
    return necro_create_type_con(infer, con, types_list, tuple_count);
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
    default: return necro_infer_error(infer, NULL, NULL, "bin op not implemented in type checker!: %d", bin_op_type);
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

NecroType* necro_make_con_1(NecroInfer* infer, NecroCon con, NecroType* arg1)
{
    NecroType* lst1 = necro_create_type_list(infer, arg1, NULL);
    return necro_create_type_con(infer, con, lst1 , 1);
}

NecroType* necro_make_con_2(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2)
{
    NecroType* lst2 = necro_create_type_list(infer, arg2, NULL);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 2);
}

NecroType* necro_make_con_3(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3)
{
    NecroType* lst3 = necro_create_type_list(infer, arg3, NULL);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 3);
}

NecroType* necro_make_con_4(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4)
{
    NecroType* lst4 = necro_create_type_list(infer, arg4, NULL);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 4);
}

NecroType* necro_make_con_5(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5)
{
    NecroType* lst5 = necro_create_type_list(infer, arg5, NULL);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 5);
}

NecroType* necro_make_con_6(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6)
{
    NecroType* lst6 = necro_create_type_list(infer, arg6, NULL);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 6);
}

NecroType* necro_make_con_7(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7)
{
    NecroType* lst7 = necro_create_type_list(infer, arg7, NULL);
    NecroType* lst6 = necro_create_type_list(infer, arg6, lst7);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 7);
}

NecroType* necro_make_con_8(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8)
{
    NecroType* lst8 = necro_create_type_list(infer, arg8, NULL);
    NecroType* lst7 = necro_create_type_list(infer, arg7, lst8);
    NecroType* lst6 = necro_create_type_list(infer, arg6, lst7);
    NecroType* lst5 = necro_create_type_list(infer, arg5, lst6);
    NecroType* lst4 = necro_create_type_list(infer, arg4, lst5);
    NecroType* lst3 = necro_create_type_list(infer, arg3, lst4);
    NecroType* lst2 = necro_create_type_list(infer, arg2, lst3);
    NecroType* lst1 = necro_create_type_list(infer, arg1, lst2);
    return necro_create_type_con(infer, con, lst1, 8);
}

NecroType* necro_make_con_9(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9)
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
    return necro_create_type_con(infer, con, lst1, 9);
}

NecroType* necro_make_con_10(NecroInfer* infer, NecroCon con, NecroType* arg1, NecroType* arg2, NecroType* arg3, NecroType* arg4, NecroType* arg5, NecroType* arg6, NecroType* arg7, NecroType* arg8, NecroType* arg9, NecroType* arg10)
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
    return necro_create_type_con(infer, con, lst1, 10);
}

//=====================================================
// Testing
//=====================================================
void necro_test_type()
{
    necro_announce_phase("NecroType");
}


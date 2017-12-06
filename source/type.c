/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "type.h"

NecroInfer necro_create_infer(NecroIntern* intern)
{
    return (NecroInfer)
    {
        .intern = intern,
        .arena  = necro_create_paged_arena(),
        .error  = necro_create_error()
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
inline bool necro_is_infer_error(NecroInfer* infer)
{
    return infer->error.return_code != NECRO_SUCCESS;
}

void* necro_infer_error(NecroInfer* infer, NecroType* type, const char* error_message, ...)
{
    if (infer->error.return_code != NECRO_SUCCESS)
        return type;
    va_list args;
	va_start(args, error_message);

    necro_verror(&infer->error, type->source_loc, error_message, args);

    va_end(args);

    return NULL;
}

NecroType* necro_alloc_type(NecroInfer* infer)
{
    return necro_paged_arena_alloc(&infer->arena, sizeof(NecroType));
}

NecroType* necro_create_type_var(NecroInfer* infer, NecroVar var)
{
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

NecroType* necro_create_type_con(NecroInfer* infer, NecroCon con, NecroType* args)
{
    NecroType* type = necro_alloc_type(infer);
    type->type      = NECRO_TYPE_CON;
    type->con       = (NecroTypeCon)
    {
        .con  = con,
        .args = args
    };
    return type;
}

NecroType* necro_create_for_all(NecroInfer* infer, NecroVar var, NecroType* type)
{
    NecroType* type_ = necro_alloc_type(infer);
    type_->type      = NECRO_TYPE_FOR;
    type_->for_all   = (NecroTypeFor)
    {
        .var  = var,
        .type = type
    };
    return type_;
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

typedef struct NecroSub
{
    NecroType*        type;
    NecroSymbol       name;
    struct NecroSub*  next;
} NecroSub;

NecroSub* necro_create_sub(NecroInfer* infer, NecroType* type, NecroSymbol name, NecroSub* next)
{
    NecroSub* sub = necro_paged_arena_alloc(&infer->arena, sizeof(NecroSub));
    *sub = (NecroSub)
    {
        .type = type,
        .name = name,
        .next = next,
    };
    return sub;
}

NecroType* necro_reverse_type_app(NecroType* type_app)
{
    assert(type_app->type == NECRO_TYPE_APP);
    NecroType* current = type_app;
    NecroType* new     = NULL;
    NecroType* prev    = NULL;
    while (current != NULL)
    {
        prev           = new;
        new            = current;
        current        = current->app.type2;
        new->app.type2 = prev;
    }
    return new;
}

NecroType* necro_reverse_type_fun(NecroType* type_fun)
{
    assert(type_fun->type == NECRO_TYPE_FUN);
    NecroType* current = type_fun;
    NecroType* new     = NULL;
    NecroType* prev    = NULL;
    while (current != NULL)
    {
        prev           = new;
        new            = current;
        current        = current->fun.type2;
        new->app.type2 = prev;
    }
    return new;
}

NecroType* necro_reverse_type_list(NecroType* type_list)
{
    assert(type_list->type == NECRO_TYPE_LIST);
    NecroType* current = type_list;
    NecroType* new     = NULL;
    NecroType* prev    = NULL;
    while (current != NULL)
    {
        prev           = new;
        new            = current;
        current        = current->list.next;
        new->list.next = prev;
    }
    return new;
}

//=====================================================
// Substitute
//=====================================================
NecroType* necro_sub(NecroInfer* infer, NecroSub* subs, NecroType* type)
{
    if (necro_is_infer_error(infer))
        return type;
    if (subs == NULL || type == NULL)
        return type;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        NecroTypeVar* type_var = &type->var;
        NecroType*    sub_type = subs->type;
        NecroSymbol   sub_name = subs->name;
        if (sub_name.id == type_var->var.symbol.id)
            return sub_type;
        else
            return necro_sub(infer, subs->next, type);
    }
    case NECRO_TYPE_APP:
    {
        NecroType* new_type1 = necro_sub(infer, subs, type->app.type1);
        NecroType* new_type2 = necro_sub(infer, subs, type->app.type2);
        if (new_type1 != type->app.type1 || new_type2 != type->app.type2)
            return necro_create_type_app(infer, new_type1, new_type2);
        else
            return type;
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* new_type1 = necro_sub(infer, subs, type->fun.type1);
        NecroType* new_type2 = necro_sub(infer, subs, type->fun.type2);
        if (new_type1 != type->fun.type1 || new_type2 != type->fun.type2)
            return necro_create_type_fun(infer, new_type1, new_type2);
        else
            return type;
    }
    case NECRO_TYPE_CON:
    {
        NecroType* new_args = necro_sub(infer, subs, type->con.args);
        if (new_args != type->con.args)
            return necro_create_type_con(infer, type->con.con, new_args);
        else
            return type;
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* new_item = necro_sub(infer, subs, type->list.item);
        NecroType* new_next = necro_sub(infer, subs, type->list.next);
        if (new_item != type->list.item || new_next != type->list.next)
            return necro_create_type_list(infer, new_item, new_next);
        else
            return type;
    }
    case NECRO_TYPE_LIT:
        return necro_infer_error(infer, type, "Compiler bug: NecroTypeLit not implemented.");
    case NECRO_TYPE_FOR:
        return necro_infer_error(infer, type, "Compiler bug: ForAll type found in necro_sub.");
    default:
        return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
    }
}

//=====================================================
// Compose
//=====================================================
// TODO: Non-recursive implementations
NecroSub* necro_compose_iter(NecroInfer* infer, NecroSub* sub, NecroSub* subs2)
{
    if (!subs2)
        return NULL;
    NecroType* type_result = necro_sub(infer, sub, subs2->type);
    NecroSub*  new_sub     = necro_create_sub(infer, type_result, subs2->name, NULL);
    new_sub->next          = necro_compose_iter(infer, sub, subs2->next);
    return new_sub;
}

NecroSub* necro_compose(NecroInfer* infer, NecroSub* subs1, NecroSub* subs2)
{
    if (necro_is_infer_error(infer))
        return NULL;
    if (subs1 == NULL)
        return subs2;
    NecroSub* new_sub = necro_create_sub(infer, subs1->type, subs1->name, NULL);
    new_sub->next     = necro_compose_iter(infer, new_sub, subs2);
    return necro_compose(infer, subs1->next, new_sub);
}

//=====================================================
// Unify
//=====================================================
bool necro_occurs(NecroInfer* infer, NecroSymbol name, NecroType* type)
{
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return type->var.var.symbol.id == name.id;
    case NECRO_TYPE_APP:  return necro_occurs(infer, name, type->app.type1) || necro_occurs(infer, name, type->app.type2);
    case NECRO_TYPE_FUN:  return necro_occurs(infer, name, type->fun.type1) || necro_occurs(infer, name, type->fun.type2);
    case NECRO_TYPE_CON:  return type->con.con.symbol.id == name.id || necro_occurs(infer, name, type->con.args);
    case NECRO_TYPE_LIST: return necro_occurs(infer, name, type->list.item) || necro_occurs(infer, name, type->list.next);
    case NECRO_TYPE_LIT:
        return necro_infer_error(infer, type, "Compiler bug: NecroTypeLit not implemented.");
    case NECRO_TYPE_FOR:
        return necro_infer_error(infer, type, "Compiler bug: ForAll type found in necro_occurs.");
    default:
        return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
    }
}

NecroSub* necro_unify_args(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    if (type1 == NULL && type2 == NULL)
        return NULL;
    // if (type1 != NULL)
    // {
    //     assert(type1->)
    //     // if (type2 == NULL);
    // }
    // switch (type->type)
    // {
    // case NECRO_TYPE_VAR:  return type->var.var.symbol.id == name.id;
    // case NECRO_TYPE_APP:  return necro_occurs(infer, name, type->app.type1) || necro_occurs(infer, name, type->app.type2);
    // case NECRO_TYPE_FUN:  return necro_occurs(infer, name, type->fun.type1) || necro_occurs(infer, name, type->fun.type2);
    // case NECRO_TYPE_CON:  return type->con.con.symbol.id == name.id || necro_occurs(infer, name, type->con.args);
    // case NECRO_TYPE_LIST: return necro_occurs(infer, name, type->list.item) || necro_occurs(infer, name, type->list.next);
    // case NECRO_TYPE_LIT:
    //     return necro_infer_error(infer, type, "Compiler bug: NecroTypeLit not implemented.");
    // case NECRO_TYPE_FOR:
    //     return necro_infer_error(infer, type, "Compiler bug: ForAll type found in necro_occurs.");
    // default:
    //     return necro_infer_error(infer, type, "Compiler bug: Unrecognized type: %d.", type->type);
    // }
    return NULL;
}

// TODO: Add accumulator for sub being built up!!!!
// TODO: Add reverse NecroSub function
NecroSub* necro_unify_con_args(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
{
    if (type1 == NULL && type2 == NULL)
        return NULL;
    return NULL;
}
inline NecroSub* necro_unify_var(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_VAR);
    switch (type2->type)
    {

    case NECRO_TYPE_VAR:
        if (type1->var.var.symbol.id == type2->var.var.symbol.id)
            return NULL;
        else
            return necro_create_sub(infer, type1, type2->var.var.symbol, NULL);

    case NECRO_TYPE_APP:
        if (necro_occurs(infer, type1->var.var.symbol, type2))
            return necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_intern_get_string(infer->intern, type1->var.var.symbol));
        else
            return necro_create_sub(infer, type2, type1->var.var.symbol, NULL);

    case NECRO_TYPE_FUN:
        if (necro_occurs(infer, type1->var.var.symbol, type2))
            return necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_intern_get_string(infer->intern, type1->var.var.symbol));
        else
            return necro_create_sub(infer, type2, type1->var.var.symbol, NULL);

    case NECRO_TYPE_CON:
        if (type2->con.args == NULL)
            return necro_create_sub(infer, type2, type1->var.var.symbol, NULL);
        else if (necro_occurs(infer, type1->var.var.symbol, type2))
            return necro_infer_error(infer, type1, "Occurs check error, name1: %s", necro_intern_get_string(infer->intern, type1->var.var.symbol));
        else
            return necro_create_sub(infer, type2, type1->var.var.symbol, NULL);

    case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeVar with type args list.");
    default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
    }
}

inline NecroSub* necro_unify_app(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_APP);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (necro_occurs(infer, type2->var.var.symbol, type1))
            return necro_infer_error(infer, type1, "Occurs check error, name2: %s", necro_intern_get_string(infer->intern, type2->var.var.symbol));
        else
            return necro_create_sub(infer, type1, type2->var.var.symbol, NULL);
    case NECRO_TYPE_APP:  return necro_unify_args(infer, type1->con.args, type2->con.args);
    case NECRO_TYPE_CON:  return necro_infer_error(infer, type1, "Attempting unify two constants, name1: (TypeApp) name2: %s", necro_intern_get_string(infer->intern, type2->var.var.symbol));
    case NECRO_TYPE_FUN:  return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeApp with (->).");
    case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeApp with type args list.");
    default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
    }
}

inline NecroSub* necro_unify_fun(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_FUN);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (necro_occurs(infer, type2->var.var.symbol, type1))
            return necro_infer_error(infer, type1, "Occurs check error, name2: %d", necro_intern_get_string(infer->intern, type2->var.var.symbol));
        else
            return necro_create_sub(infer, type1, type2->var.var.symbol, NULL);
    case NECRO_TYPE_FUN:  return necro_unify_args(infer, type1->con.args, type2->con.args);
    case NECRO_TYPE_APP:  return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify (->) with TypeApp.");
    case NECRO_TYPE_CON:  return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify (->) with TypeCon (%s)", necro_intern_get_string(infer->intern, type2->con.con.symbol));
    case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify (->) with type args list.");
    default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
    }
}

inline NecroSub* necro_unify_con(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
{
    assert(type1 != NULL);
    assert(type1->type == NECRO_TYPE_CON);
    switch (type2->type)
    {
    case NECRO_TYPE_VAR:
        if (type2->con.args == NULL)
            return necro_create_sub(infer, type1, type2->var.var.symbol, NULL);
        else if (necro_occurs(infer, type2->var.var.symbol, type1))
            return necro_infer_error(infer, type1, "Occurs check error, name2: %d", necro_intern_get_string(infer->intern, type2->var.var.symbol));
        else
            return necro_create_sub(infer, type1, type2->var.var.symbol, NULL);
    case NECRO_TYPE_CON:
        if (type1->con.con.symbol.id == type2->con.con.symbol.id)
            return necro_unify_con_args(infer, unifier_result, type1->con.args, type2->con.args);
        else
            return necro_infer_error(infer, type1, "Attempting unify two constants, name1: %s name2: %s", necro_intern_get_string(infer->intern, type1->var.var.symbol), necro_intern_get_string(infer->intern, type2->var.var.symbol));
    case NECRO_TYPE_APP:  return necro_infer_error(infer, type1, "Attempting unify two constants, name1: %s name2: (TypeApp)", necro_intern_get_string(infer->intern, type1->var.var.symbol));
    case NECRO_TYPE_FUN:  return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, type2->con.con.symbol));
    case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Attempted to unify TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, type2->con.con.symbol));
    default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
    }
}

NecroSub* necro_unify_go(NecroInfer* infer, NecroSub* unifier_result, NecroType* type1, NecroType* type2)
{
    if (necro_is_infer_error(infer))
        return NULL;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  return necro_unify_var(infer, type1, type2);
    case NECRO_TYPE_APP:  return necro_unify_app(infer, type1, type2);
    case NECRO_TYPE_FUN:  return necro_unify_fun(infer, type1, type2);
    case NECRO_TYPE_CON:  return necro_unify_con(infer, unifier_result, type1, type2);
    case NECRO_TYPE_LIST: return necro_infer_error(infer, type1, "Compiler bug: Found Type args list in necro_unify");
    default:              return necro_infer_error(infer, type1, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", type1->type, type2->type);
    }
}

NecroSub* necro_unify(NecroInfer* infer, NecroType* type1, NecroType* type2)
{
    return necro_unify_go(infer, NULL, type1, type2);
}

//=====================================================
// Printing
//=====================================================
void necro_print_type(NecroType* type, uint32_t whitespace, NecroIntern* intern)
{
    if (type == NULL)
        return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
        print_white_space(whitespace);
        printf("TypeVar, name: %s, id: %d\n", necro_intern_get_string(intern, type->var.var.symbol), type->var.var.id.id);
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

    case NECRO_TYPE_FOR:
        print_white_space(whitespace);
        printf("ForAll, name: %s, id: %d\n", necro_intern_get_string(intern, type->for_all.var.symbol), type->for_all.var.id.id);
        necro_print_type(type->con.args, whitespace + 4, intern);
        break;

    case NECRO_TYPE_LIST:
        necro_print_type(type->list.item, whitespace, intern);
        necro_print_type(type->list.next, whitespace, intern);
        break;

    case NECRO_TYPE_LIT:
        printf("Compiler bug: NecroTypeLit not implemented");
        assert(false);
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
        printf("%s", necro_intern_get_string(intern, type->var.var.symbol));
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

    case NECRO_TYPE_FOR:
    {
        printf("forall %s", necro_intern_get_string(intern, type->for_all.var.symbol));
        NecroType* current = type->for_all.type;
        while (current->type == NECRO_TYPE_FOR)
        {
            printf(" %s", necro_intern_get_string(intern, current->for_all.var.symbol));
            current = current->for_all.type;
        }
        printf(". ");
        necro_print_type_sig_go(current, intern);
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

    case NECRO_TYPE_LIT:
        printf("Compiler bug: NecroTypeLit not implemented");
        assert(false);
        break;

    default:
        printf("Compiler bug: Unrecognized type: %d", type->type);
        assert(false);
    }
}

void necro_print_type_sig(NecroType* type, NecroIntern* intern)
{
    if (type != NULL)
        necro_print_type_sig_go(type, intern);
    printf("\n");
}

void necro_print_sub(NecroSub* sub, NecroIntern* intern, size_t white_count)
{
    if (sub == NULL)
        return;
    printf("\n");
    print_white_space(white_count);
    printf("(sub: %s ==> ", necro_intern_get_string(intern, sub->name));
    necro_print_type_sig_go(sub->type, intern);
    necro_print_sub(sub->next, intern, white_count + 1);
    printf(")");
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

void necro_print_test_sub_result(bool passed, const char* test_name, NecroSub* sub1, const char* test_name2, NecroSub* sub2, NecroIntern* intern)
{
    printf("\n%s :: ", test_name);
    necro_print_sub(sub1, intern, 0);
    printf("\n");
    printf("%s :: ", test_name2);
    necro_print_sub(sub2, intern, 0);
    printf("\n");
    if (passed)
        printf(" (passed)\n");
    else
        printf(" (FAILED)\n");
}

bool necro_types_match(NecroType* type1, NecroType* type2)
{
    if (type1 == NULL && type2 == NULL)
        return true;
    else if (type1 != NULL && type2 == NULL)
        return false;
    else if (type2 == NULL && type2 != NULL)
        return false;
    else if (type1->type != type2->type)
        return false;
    switch (type1->type)
    {
    case NECRO_TYPE_VAR:  return type1->var.var.symbol.id == type2->var.var.symbol.id;
    case NECRO_TYPE_APP:  return necro_types_match(type1->app.type1, type2->app.type1) && necro_types_match(type1->app.type2, type2->app.type2);
    case NECRO_TYPE_FUN:  return necro_types_match(type1->fun.type1, type2->fun.type1) && necro_types_match(type1->fun.type2, type2->fun.type2);
    case NECRO_TYPE_CON:  return type1->con.con.symbol.id == type2->con.con.symbol.id && necro_types_match(type1->con.args, type2->con.args);
    case NECRO_TYPE_FOR:  return type1->for_all.var.symbol.id == type2->for_all.var.symbol.id && necro_types_match(type1->for_all.type, type2->for_all.type);
    case NECRO_TYPE_LIST: return necro_types_match(type1->list.item, type2->list.item) && necro_types_match(type1->list.next, type2->list.next);
    case NECRO_TYPE_LIT:
        printf("Compiler bug: NecroTypeLit not implemented");
        assert(false);
        return false;
    default:
        printf("Compiler bug: Unrecognized type: %d", type1->type);
        assert(false);
        return false;
    }
}

bool necro_subs_match(NecroSub* sub1, NecroSub* sub2)
{
    if (sub1 == NULL && sub2 == NULL)
        return true;
    else if (sub1 == NULL && sub2 != NULL)
        return true;
    else if (sub1 != NULL && sub2 == NULL)
        return true;
    else
        return sub1->name.id == sub2->name.id            &&
               necro_types_match(sub1->type, sub2->type) &&
               necro_subs_match(sub1->next, sub2->next);
}

//=====================================================
// Testing
//=====================================================
void necro_test_infer()
{
    necro_announce_phase("NecroInfer");

    NecroIntern intern = necro_create_intern();
    NecroInfer  infer  = necro_create_infer(&intern);

    // Print test
    {
        NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL);
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 1 } }, NULL);

        NecroType* lst1 = necro_create_type_list(&infer, bcon, NULL);
        NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 2 } }, lst1);

        NecroType* iols = necro_create_type_list(&infer, mcon, NULL);
        NecroType* iocn = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "IO"), { 7 } }, iols);

        NecroType* lst3 = necro_create_type_list(&infer, bcon, NULL);
        NecroType* lst2 = necro_create_type_list(&infer, icon, lst3);
        NecroType* econ = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 3 } }, lst2);

        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 4 } });
        NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 5 } });
        NecroType* cvar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "c"), { 6 } });
        NecroType* vapp = necro_create_type_app(&infer, avar, bvar);

        NecroType* fun1 = necro_create_type_fun(&infer, bvar, cvar);
        NecroType* fun2 = necro_create_type_fun(&infer, econ, iocn);
        NecroType* fun3 = necro_create_type_fun(&infer, fun1, fun2);
        NecroType* fun4 = necro_create_type_fun(&infer, vapp, fun3);

        NecroType* for3 = necro_create_for_all(&infer, (NecroVar) { necro_intern_string(&intern, "c"), { 10 } }, fun4);
        NecroType* for2 = necro_create_for_all(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 9  } }, for3);
        NecroType* for1 = necro_create_for_all(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 8  } }, for2);

        // necro_print_type(for1, 0, &intern);
        // printf("\n");
        necro_print_type_sig(for1, &intern);
        printf("\n");
    }

    // Simple Substitute test
    {
        NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bcon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL);
        NecroSub*  sub    = necro_create_sub(&infer, bcon, necro_intern_string(&intern, "a"), NULL);
        NecroType* result = necro_sub(&infer, sub, avar);
        necro_print_test_result(necro_types_match(result, bcon), "simpleSub1", result, "simpleSub2", bcon, &intern);
    }

    // App Substitute test
    {
        NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bvar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 0 } });
        NecroType* icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* app    = necro_create_type_app(&infer, avar, bvar);
        NecroSub*  sub    = necro_create_sub(&infer, icon, necro_intern_string(&intern, "b"), NULL);
        NecroType* result = necro_sub(&infer, sub, app);
        NecroType* match  = necro_create_type_app(&infer, avar, icon);
        necro_print_test_result(necro_types_match(result, match), "appSub1", result, "appSub2", match, &intern);
    }

    // Func Substitute test
    {
        NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bvar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 0 } });
        NecroType* icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* fun    = necro_create_type_fun(&infer, bvar, avar);
        NecroSub*  sub    = necro_create_sub(&infer, icon, necro_intern_string(&intern, "b"), NULL);
        NecroType* result = necro_sub(&infer, sub, fun);
        NecroType* match  = necro_create_type_fun(&infer, icon, avar);
        necro_print_test_result(necro_types_match(result, match), "funcSub1", result, "funcSub2", match, &intern);
    }

    // Con Substitute test
    {
        NecroType* avar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bvar   = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 0 } });
        NecroType* icon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* ucon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "()"), { 0 } }, NULL);

        NecroType* lst1   = necro_create_type_list(&infer, bvar, NULL);
        NecroType* lst2   = necro_create_type_list(&infer, avar, lst1);
        NecroType* mcon   = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst2);

        NecroSub*  sub2   = necro_create_sub(&infer, ucon, necro_intern_string(&intern, "b"), NULL);
        NecroSub*  sub    = necro_create_sub(&infer, icon, necro_intern_string(&intern, "a"), sub2);

        NecroType* lst3   = necro_create_type_list(&infer, ucon, NULL);
        NecroType* lst4   = necro_create_type_list(&infer, icon, lst3);
        NecroType* match  = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Either"), { 0 } }, lst4);

        NecroType* result = necro_sub(&infer, sub, mcon);

        necro_print_test_result(necro_types_match(result, match), "conSub1", result, "conSub2", match, &intern);
    }

    // Sub Print test
    {
        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL);
        NecroSub*  sub2 = necro_create_sub(&infer, icon, necro_intern_string(&intern, "a"), NULL);
        NecroSub*  sub  = necro_create_sub(&infer, bcon, necro_intern_string(&intern, "b"), sub2);
        // printf("\n");
        // necro_print_sub(sub, &intern, 0);
    }

    // Compose Subs test
    {
        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL);
        NecroSub*  sub  = necro_create_sub(&infer, bcon, necro_intern_string(&intern, "b"), NULL);
        NecroSub*  sub2 = necro_create_sub(&infer, icon, necro_intern_string(&intern, "a"), NULL);

        NecroSub*  sub3 = necro_compose(&infer, sub, sub2);
        NecroSub*  msub = necro_create_sub(&infer, bcon, necro_intern_string(&intern, "b"), sub2);
        printf("\n");
        necro_print_sub(sub3, &intern, 0);
        necro_print_test_result(necro_subs_match(sub3, msub), "composeSub1", sub3->type, "composeSub2", msub->type, &intern);
    }

    // Compose Subs test 2
    {
        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);

        NecroType* bcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Bool"), { 0 } }, NULL);
        NecroType* fun  = necro_create_type_fun(&infer, avar, bcon);

        NecroSub*  sub  = necro_create_sub(&infer, fun, necro_intern_string(&intern, "b"), NULL);
        NecroSub*  sub2 = necro_create_sub(&infer, icon, necro_intern_string(&intern, "a"), NULL);

        NecroSub*  sub3 = necro_compose(&infer, sub2, sub);

        NecroType* fun2 = necro_create_type_fun(&infer, icon, bcon);
        NecroSub*  msu2 = necro_create_sub(&infer, fun2, necro_intern_string(&intern, "b"), NULL);
        NecroSub*  msur = necro_create_sub(&infer, icon, necro_intern_string(&intern, "a"), msu2);

        printf("\n");
        // necro_print_sub(sub3,  &intern, 0);
        // necro_print_sub(msub3, &intern, 0);
        necro_print_test_sub_result(necro_subs_match(sub3, msur), "composeSub1", sub3, "composeSub2", msur, &intern);
    }

    // occurs test1
    {
        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 0 } });
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* fun  = necro_create_type_fun(&infer, bvar, avar);
        necro_print_test_result(necro_occurs(&infer, necro_intern_string(&intern, "a"), fun), "occurs1", fun, "occurs1", fun, &intern);
    }

    // occurs test2
    {
        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 0 } });
        NecroType* icon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL);
        NecroType* fun  = necro_create_type_fun(&infer, bvar, avar);
        necro_print_test_result(!necro_occurs(&infer, necro_intern_string(&intern, "notHere"), fun), "occurs2", fun, "occurs2", fun, &intern);
    }

    // occurs test3
    {
        NecroType* avar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "a"), { 0 } });
        NecroType* bvar = necro_create_type_var(&infer, (NecroVar) { necro_intern_string(&intern, "b"), { 0 } });
        NecroType* lst1 = necro_create_type_list(&infer, bvar, NULL);
        NecroType* mcon = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Maybe"), { 0 } }, lst1);
        NecroType* fun  = necro_create_type_fun(&infer, avar, mcon);
        necro_print_test_result(necro_occurs(&infer, necro_intern_string(&intern, "Maybe"), fun), "occurs3", fun, "occurs3", fun, &intern);
    }

    necro_destroy_infer(&infer);
    necro_destroy_intern(&intern);
}
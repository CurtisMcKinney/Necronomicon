/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "type_class.h"
#include "infer.h"
#include "symtable.h"
#include "kind.h"
#include "type/prim.h"
#include "type/derive.h"
#include "base.h"

/*
BACKBURNER
    - Require type signature for all top level bindings?!?!?
    - Pattern literal cleanup: Require Eq for pattern literals, also make sure it's getting type class translated correctly
    - split NECRO_DECLARATION into:
        case NECRO_VAR_PATTERN?
        case NECRO_VAR_ARG?
    - Do statements
*/

//=====================================================
// Forward Declarations
//=====================================================
NecroTypeClassInstance* necro_get_type_class_instance(NecroAstSymbol* data_type_symbol, NecroAstSymbol* type_class_symbol);

//=====================================================
// Type Class Instances
//=====================================================
void necro_apply_constraints(NecroPagedArena* arena, NecroType* type, NecroTypeClassContext* context)
{
    if (type == NULL) return;
    if (context == NULL) return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        while (context != NULL)
        {
            if (type->var.var_symbol->name == context->var_symbol->name)
            {
                NecroTypeClassContext* class_context = necro_create_type_class_context(arena, context->type_class, context->class_symbol, context->var_symbol, NULL);
                type->var.context = necro_union_contexts(arena, type->var.context, class_context);
            }
            context = context->next;
        }
        break;
    }
    case NECRO_TYPE_APP:  necro_apply_constraints(arena, type->app.type1, context); necro_apply_constraints(arena, type->app.type2, context); break;
    case NECRO_TYPE_FUN:  necro_apply_constraints(arena, type->fun.type1, context); necro_apply_constraints(arena, type->fun.type2, context); break;
    case NECRO_TYPE_CON:  necro_apply_constraints(arena, type->con.args,  context); break;
    case NECRO_TYPE_LIST: necro_apply_constraints(arena, type->list.item, context); necro_apply_constraints(arena, type->list.next, context); break;
    case NECRO_TYPE_FOR:  assert(false); break;
    default:              assert(false); break;
    }
}

NecroType* necro_instantiate_method_sig(NecroInfer* infer, NecroAstSymbol* type_class_var, NecroType* a_method_type, NecroType* a_data_type)
{
    NecroInstSub* subs = NULL;
    NecroType*    type = unwrap(NecroType, necro_type_instantiate_with_subs(infer->arena, infer->base, a_method_type, NULL, &subs));
    a_data_type        = unwrap(NecroType, necro_type_instantiate(infer->arena, infer->base, a_data_type, NULL));
    while (subs != NULL)
    {
        if (subs->var_to_replace->name == type_class_var->name)
        {
            unwrap(NecroType, necro_type_unify(infer->arena, infer->base, subs->new_name, a_data_type, NULL));
            unwrap(void, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type, NULL, NULL_LOC, NULL_LOC));
            type = unwrap(NecroType, necro_type_generalize(infer->arena, infer->base, type, NULL));
            unwrap(void, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type, NULL, NULL_LOC, NULL_LOC));
            return type;
        }
        subs = subs->next;
    }
    assert(false);
    return NULL;
}

//=====================================================
// Contexts
//=====================================================
bool necro_context_contains_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class)
{
    while (context != NULL)
    {
        if (context->class_symbol == type_class->class_symbol)
        {
            return true;
        }
        context = context->next;
    }
    return false;
}

bool necro_context_and_super_classes_contain_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class)
{
    while (context != NULL)
    {
        if (context->class_symbol == type_class->class_symbol)
        {
            return true;
        }
        NecroTypeClassContext* super_classes = context->type_class->context;
        if (necro_context_and_super_classes_contain_class(super_classes, type_class))
        {
            return true;
        }
        context = context->next;
    }
    return false;
}

bool necro_is_subclass(NecroTypeClassContext* maybe_subclass_context, NecroTypeClassContext* maybe_super_class_context)
{
    if (maybe_subclass_context->var_symbol != maybe_super_class_context->var_symbol)
        return false;
    NecroTypeClassContext* super_classes = maybe_subclass_context->type_class->context;
    return necro_context_and_super_classes_contain_class(super_classes, maybe_super_class_context);
}

bool necro_context_contains_class_and_type_var(NecroTypeClassContext* context, NecroTypeClassContext* type_class, NecroAstSymbol* var_symbol)
{
    while (context != NULL)
    {
        if (context->class_symbol == type_class->class_symbol && context->var_symbol == var_symbol)
            return true;
        context = context->next;
    }
    return false;
}

NecroTypeClassContext* necro_remove_super_classes(NecroTypeClassContext* sub_class, NecroTypeClassContext* context)
{
    NecroTypeClassContext* head = context;
    NecroTypeClassContext* curr = head;
    NecroTypeClassContext* prev = NULL;
    while (curr != NULL)
    {
        if (necro_is_subclass(sub_class, curr))
        {
            if (curr == head)
            {
                head = head->next;
            }
            if (prev != NULL)
            {
                prev->next = curr->next;
            }
        }
        else
        {
            prev = curr;
        }
        curr = curr->next;
    }
    return head;
}

NecroTypeClassContext* necro_scrub_super_classes(NecroTypeClassContext* context)
{
    if (context == NULL || context->next == NULL)
        return context;
    NecroTypeClassContext* head = context;
    NecroTypeClassContext* curr = head;
    while (curr != NULL)
    {
        head = necro_remove_super_classes(curr, head);
        curr = curr->next;
    }
    return head;
}

NecroTypeClassContext* necro_union_contexts_to_same_var(NecroPagedArena* arena, NecroTypeClassContext* context1, NecroTypeClassContext* context2, NecroAstSymbol* var_symbol)
{
    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        if (!necro_context_contains_class(head, context1))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(arena, context1->type_class, context1->class_symbol, var_symbol, NULL);
            if (curr == NULL)
            {
                head = new_context;
                curr = head;
            }
            else
            {
                curr->next = new_context;
                curr       = curr->next;
            }
        }
        context1 = context1->next;
    }

    // Insert context2 classes if they aren't already in context1
    while (context2 != NULL)
    {
        if (!necro_context_contains_class(head, context2))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(arena, context2->type_class, context2->class_symbol, var_symbol, NULL);
            if (curr == NULL)
            {
                head = new_context;
                curr = head;
            }
            else
            {
                curr->next = new_context;
                curr       = curr->next;
            }
        }
        context2 = context2->next;
    }

    // Scrub super classes
    return necro_scrub_super_classes(head);
}

NecroTypeClassContext* necro_union_contexts(NecroPagedArena* arena, NecroTypeClassContext* context1, NecroTypeClassContext* context2)
{
    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        assert(context1->type_class != NULL);
        if (!necro_context_contains_class_and_type_var(head, context1, context1->var_symbol))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(arena, context1->type_class, context1->class_symbol, context1->var_symbol, NULL);
            if (curr == NULL)
            {
                head = new_context;
                curr = head;
            }
            else
            {
                curr->next = new_context;
                curr       = curr->next;
            }
        }
        context1 = context1->next;
    }

    // Insert context2 classes if they aren't already in context1
    while (context2 != NULL)
    {
        assert(context2->type_class != NULL);
        if (!necro_context_contains_class_and_type_var(head, context2, context2->var_symbol))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(arena, context2->type_class, context2->class_symbol, context2->var_symbol, NULL);
            if (curr == NULL)
            {
                head = new_context;
                curr = head;
            }
            else
            {
                curr->next = new_context;
                curr       = curr->next;
            }
        }
        context2 = context2->next;
    }

    // Scrub super classes
    return necro_scrub_super_classes(head);
}

NecroTypeClassContext* necro_create_type_class_context(NecroPagedArena* arena, NecroTypeClass* type_class, NecroAstSymbol* type_class_name, NecroAstSymbol* type_var, NecroTypeClassContext* next)
{
    NecroTypeClassContext* context = necro_paged_arena_alloc(arena, sizeof(NecroTypeClassContext));
    context->class_symbol          = type_class_name;
    context->var_symbol            = type_var;
    context->type_class            = type_class;
    context->next                  = next;
    return context;
}

bool necro_ambig_occurs(NecroAstSymbol* name, NecroType* type)
{
    if (type == NULL)
        return false;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return name == type->var.var_symbol;
    case NECRO_TYPE_APP:  return necro_ambig_occurs(name, type->app.type1) || necro_ambig_occurs(name, type->app.type2);
    case NECRO_TYPE_FUN:  return necro_ambig_occurs(name, type->fun.type1) || necro_ambig_occurs(name, type->fun.type2);
    case NECRO_TYPE_CON:  return necro_ambig_occurs(name, type->con.args);
    case NECRO_TYPE_LIST: return necro_ambig_occurs(name, type->list.item) || necro_ambig_occurs(name, type->list.next);
    case NECRO_TYPE_FOR:  assert(false); break;
    default:              assert(false); break;
    }
    return false;
}

NecroResult(NecroType) necro_ambiguous_type_class_check(NecroAstSymbol* type_sig_name, NecroTypeClassContext* context, NecroType* type)
{
    if (type == NULL)
        return ok(NecroType, NULL);
    while (context != NULL)
    {
        if (!necro_ambig_occurs(context->var_symbol, type))
            return necro_type_ambiguous_class_error(context->class_symbol, type_sig_name->ast->source_loc, type_sig_name->ast->end_loc, context->var_symbol, NULL_LOC, NULL_LOC);
        context = context->next;
    }
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_constrain_class_variable_check(NecroAstSymbol* type_var, NecroAstSymbol* type_sig_symbol, NecroTypeClassContext* context)
{
    while (context != NULL)
    {
        if (context->var_symbol == type_var)
            return necro_type_constrains_only_class_var_error(context->class_symbol, type_sig_symbol->ast->source_loc, type_sig_symbol->ast->end_loc, type_var, NULL_LOC, NULL_LOC);
        context = context->next;
    }
    return ok(NecroType, NULL);
}

//=====================================================
// Printing
//=====================================================
void necro_print_type_class_member(NecroTypeClassMember* member, size_t white_count)
{
    if (member == NULL)
        return;
    NecroType* member_type = member->member_varid->type;
    print_white_space(white_count + 4);
    printf("%s :: ", member->member_varid->name->str);
    necro_type_print(member_type);
    printf("\n");
    necro_print_type_class_member(member->next, white_count);
}

void necro_print_type_class_context(NecroTypeClassContext* context, size_t white_count)
{
    if (context == NULL)
        return;
    if (context->next != NULL)
        printf("%s %s,", context->class_symbol->name->str, context->var_symbol->name->str);
    else
        printf("%s %s", context->class_symbol->name->str, context->var_symbol->name->str);
    necro_print_type_class_context(context->next, white_count);
}

void necro_print_type_class(NecroTypeClass* type_class, size_t white_count)
{
    if (type_class == NULL)
        return;
    print_white_space(white_count);
    printf("class %s %s\n", type_class->type_class_name->source_name->str, type_class->type_var->source_name->str);
    print_white_space(white_count);
    printf("{\n");
    print_white_space(white_count + 4);
    printf("type:    ");
    necro_type_print(type_class->type);
    printf("\n");
    print_white_space(white_count + 4);
    printf("context: (");
    necro_print_type_class_context(type_class->context, white_count + 4);
    printf(") =>\n");
    print_white_space(white_count + 4);
    printf("members:\n");
    print_white_space(white_count + 4);
    printf("[\n");
    necro_print_type_class_member(type_class->members, white_count + 4);
    print_white_space(white_count + 4);
    printf("]\n");

    printf("}\n");
}

void necro_print_dictionary_prototype(NecroDictionaryPrototype* prototype, size_t white_count)
{
    if (prototype == NULL)
        return;
    NecroType* member_type = prototype->instance_member_ast_symbol->type;
    print_white_space(white_count + 4);
    printf("%s :: ", prototype->instance_member_ast_symbol->name->str);
    necro_type_print(member_type);
    printf("\n");
    necro_print_dictionary_prototype(prototype->next, white_count);
}

void necro_print_instance(NecroTypeClassInstance* instance, size_t white_count)
{
    if (instance == NULL)
        return;
    print_white_space(white_count);
    printf("instance %s %s\n", instance->type_class_name->source_name->str, instance->data_type_name->source_name->str);
    print_white_space(white_count);
    printf("{\n");
    print_white_space(white_count + 4);
    printf("context: (");
    necro_print_type_class_context(instance->context, white_count + 4);
    printf(") =>\n");
    print_white_space(white_count + 4);
    printf("data_type: ");
    if (instance->data_type != NULL)
        necro_type_print(instance->data_type);
    else
        printf(" null_type");
    printf("\n");
    print_white_space(white_count + 4);
    printf("dictionary prototype:\n");
    print_white_space(white_count + 4);
    printf("[\n");
    necro_print_dictionary_prototype(instance->dictionary_prototype, white_count + 4);
    print_white_space(white_count + 4);
    printf("]\n");

    print_white_space(white_count);
    printf("}\n");
}

void necro_print_type_class_instance_go(NecroTypeClassInstance* instance)
{
    necro_print_instance(instance, 4);
}

void necro_print_type_classes(NecroInfer* infer)
{
    UNUSED(infer);
    printf("\nTypeClasses\n{\n");
    // TODO: Redo type class printing
    // for (size_t i = 0; i < infer->symtable->count; ++i)
    // {
    //     if (infer->symtable->data[i].type_class != NULL)
    //         necro_print_type_class(infer->symtable->data[i].type_class, 4);
    // }
    printf("}\n");
    printf("\nInstances\n{\n");
    // TODO: Redo type class printing
    // for (size_t i = 0; i < infer->symtable->count; ++i)
    // {
    //     if (infer->symtable->data[i].type_class_instance != NULL)
    //         necro_print_instance(infer->symtable->data[i].type_class_instance, 4);
    // }
    printf("}\n");
}

NecroResult(NecroType) necro_create_type_class(NecroInfer* infer, NecroAst* type_class_ast)
{
    assert(infer != NULL);
    assert(type_class_ast != NULL);
    assert(type_class_ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);

    //------------------------------------
    // Create type class
    NecroAstSymbol* data        = type_class_ast->type_class_declaration.tycls->conid.ast_symbol;
    NecroTypeClass* type_class  = necro_paged_arena_alloc(infer->arena, sizeof(NecroTypeClass));
    type_class->type_class_name = type_class_ast->type_class_declaration.tycls->conid.ast_symbol;
    type_class->members         = NULL;
    type_class->type_var        = type_class_ast->type_class_declaration.tyvar->variable.ast_symbol;
    type_class->context         = NULL;
    type_class->dependency_flag = 0;
    type_class->ast             = type_class_ast;
    data->type_class            = type_class;

    //------------------------------------
    // Create type_var for type_class
    NecroType* type_class_var    = necro_type_var_create(infer->arena, type_class_ast->type_class_declaration.tyvar->variable.ast_symbol);
    type_class_var->var.is_rigid = true;
    type_class_var->var.order    = NECRO_TYPE_ZERO_ORDER;
    type_class_var->ownership    = NULL;
    type_class->type             = necro_type_con1_create(infer->arena, type_class->type_class_name, necro_type_list_create(infer->arena, type_class_var, NULL));
    type_class->context          = necro_try_map(NecroTypeClassContext, NecroType, necro_ast_to_context(infer, type_class_ast->type_class_declaration.context));
    data->type                   = type_class_var;

    type_class_var->var.context = necro_create_type_class_context(infer->arena, type_class, type_class->type_class_name, type_class->type_var, type_class->context);
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type_class_var, type_class_ast->source_loc, type_class_ast->end_loc));
    NecroTypeClassContext* class_context = necro_create_type_class_context(infer->arena, type_class, type_class->type_class_name, type_class->type_var, NULL);

    //--------------------------------
    // Build Member Type Signatures
    if (type_class->ast->type_class_declaration.declarations != NULL)
    {
        NecroAst* group_list = type_class->ast->type_class_declaration.declarations;
        assert(group_list->type == NECRO_AST_DECLARATION_GROUP_LIST);
        while (group_list != NULL)
        {
            NecroAst* declaration_group = group_list->declaration_group_list.declaration_group;
            while (declaration_group != NULL)
            {
                assert(declaration_group->type == NECRO_AST_DECL);
                NecroAst* method_ast = declaration_group->declaration.declaration_impl;
                if (method_ast->type != NECRO_AST_TYPE_SIGNATURE)
                    continue;

                type_class_var->ownership = NULL;
                NecroType* type_sig       = necro_try(NecroType, necro_ast_to_type_sig_go(infer, method_ast->type_signature.type, NECRO_TYPE_ZERO_ORDER, NECRO_TYPE_ATTRIBUTE_NONE));
                type_class_var->ownership = NULL;

                //---------------------------------
                // Infer and check method type sig
                // NOTE: TypeClass's context should ALWAYS BE FIRST!
                NecroTypeClassContext* method_context = necro_try_map(NecroTypeClassContext, NecroType, necro_ast_to_context(infer, method_ast->type_signature.context));
                necro_try(NecroType, necro_constrain_class_variable_check(type_class->type_var, method_ast->type_signature.var->variable.ast_symbol, method_context));
                NecroTypeClassContext* context = necro_union_contexts(infer->arena, class_context, method_context);
                assert(context->type_class == class_context->type_class);
                necro_try(NecroType, necro_ambiguous_type_class_check(method_ast->type_signature.var->variable.ast_symbol, context, type_sig));
                necro_apply_constraints(infer->arena, type_sig, context);
                type_sig->pre_supplied = true;
                type_sig               = necro_try(NecroType, necro_type_generalize(infer->arena, infer->base, type_sig, NULL));
                necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type_sig, method_ast->source_loc, method_ast->end_loc));
                // type_sig->kind         = necro_kind_gen(infer->arena, infer->base, type_sig->kind);
                necro_try(NecroType, necro_kind_unify(type_sig->kind, infer->base->star_kind->type, NULL));
                // necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type_sig, NULL, method_ast->source_loc, method_ast->end_loc));
                method_ast->type_signature.var->variable.ast_symbol->type              = type_sig;
                method_ast->type_signature.var->variable.ast_symbol->method_type_class = type_class;

                //---------------------------------
                // kind check for type context!
                NecroTypeClassContext* curr_context = context;
                while (curr_context != NULL)
                {
                    // NecroTypeClass* type_class     = necro_type_class_table_get(&infer->type_class_env->class_table, curr_context->type_class_name.id.id);
                    NecroTypeClass* curr_type_class      = curr_context->type_class;
                    NecroType*      curr_type_class_var  = curr_type_class->type_var->type;
                    NecroType*      var_type             = curr_context->var_symbol->type;
                    necro_try(NecroType, necro_kind_unify(var_type->kind, curr_type_class_var->kind, method_ast->scope));
                    curr_context = curr_context->next;
                }

                //---------------------------------
                // Store type class members
                NecroTypeClassMember* new_member  = necro_paged_arena_alloc(infer->arena, sizeof(NecroTypeClassMember));
                new_member->member_varid = method_ast->type_signature.var->variable.ast_symbol;
                new_member->next         = NULL;
                if (type_class->members == NULL)
                {
                    type_class->members = new_member;
                }
                else
                {
                    NecroTypeClassMember* curr_member = type_class->members;
                    while (curr_member != NULL)
                    {
                        if (curr_member->next == NULL)
                        {
                            curr_member->next = new_member;
                            break;
                        }
                        curr_member = curr_member->next;
                    }
                }
                // next
                declaration_group = declaration_group->declaration.next_declaration;
            }
            group_list = group_list->declaration_group_list.next;
        }
    }
    necro_kind_default_type_kinds(infer->arena, infer->base, type_class_var);
    type_class_var->ownership = NULL;
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_create_type_class_instance(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);

    //------------------
    // Info and Names
    NecroAstSymbol* data            = ast->type_class_instance.ast_symbol;
    NecroAstSymbol* type_class_name = ast->type_class_instance.qtycls->conid.ast_symbol;
    NecroAstSymbol* data_type_name;
    if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
    {
        data_type_name = ast->type_class_instance.inst->conid.ast_symbol;
    }
    else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
    {
        data_type_name = ast->type_class_instance.inst->constructor.conid->conid.ast_symbol;
    }
    else
    {
        necro_unreachable(NecroType);
    }

    //------------------
    // Data, Class, Duplicate checks
    assert(data_type_name != NULL);
    assert(data_type_name->type != NULL);
    NecroTypeClass* type_class = type_class_name->type_class;
    if (type_class == NULL)
    {
        return necro_error_map(NecroTypeClassContext, NecroType, necro_type_not_a_class_error(type_class_name, type_class_name->type, ast->type_class_instance.qtycls->source_loc, ast->type_class_instance.qtycls->end_loc));
    }

    // Multiple instance declarations!
    NecroInstanceList* instances = data_type_name->instance_list;
    while (instances != NULL)
    {
        if (instances->data->type_class_name == type_class_name)
            return necro_type_multiple_instance_declarations_error(type_class_name, data_type_name->type, NULL, NULL, NULL, ast->type_class_instance.qtycls->source_loc, ast->type_class_instance.qtycls->end_loc);
        instances = instances->next;
    }

    //------------------
    // Create instance
    NecroTypeClassInstance* const instance = necro_paged_arena_alloc(infer->arena, sizeof(NecroTypeClassInstance));
    // instance->instance_name            = ast->type_class_instance.ast_symbol;
    instance->type_class_name              = type_class_name;
    instance->data_type_name               = data_type_name;
    instance->context                      = necro_try_map(NecroTypeClassContext, NecroType, necro_ast_to_context(infer, ast->type_class_instance.context));
    instance->dictionary_prototype         = NULL;
    instance->ast                          = ast;
    instance->data_type                    = necro_try(NecroType, necro_ast_to_type_sig_go(infer, instance->ast->type_class_instance.inst, NECRO_TYPE_ZERO_ORDER, NECRO_TYPE_ATTRIBUTE_NONE));
    // instance->super_instances              = NULL;
    data->type_class_instance              = instance;

    // Add to instance list
    data_type_name->instance_list = necro_cons_instance_list(infer->arena, instance, data_type_name->instance_list);

    NecroType* class_var               = type_class->type_var->type;
    assert(class_var != NULL);

    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, instance->data_type, instance->ast->source_loc, instance->ast->end_loc));
    necro_try(NecroType, necro_kind_unify_with_info(class_var->kind, instance->data_type->kind, NULL, ast->type_class_instance.inst->source_loc, ast->type_class_instance.inst->end_loc));

    // Apply constraints
    necro_apply_constraints(infer->arena, instance->data_type, instance->context);
    instance->data_type = necro_try(NecroType, necro_type_generalize(infer->arena, infer->base, instance->data_type, NULL));

    //--------------------------------
    // Dictionary Prototype
    if (instance->ast->type_class_instance.declarations != NULL)
    {
        NecroAst* group_list = instance->ast->type_class_instance.declarations;
        assert(group_list->type == NECRO_AST_DECLARATION_GROUP_LIST);
        while (group_list != NULL)
        {
            NecroAst* declaration_group = group_list->declaration_group_list.declaration_group;
            while (declaration_group != NULL)
            {
                assert(declaration_group->type == NECRO_AST_DECL);
                NecroAst* method_ast = declaration_group->declaration.declaration_impl;
                assert(method_ast != NULL);
                assert(method_ast->type == NECRO_AST_SIMPLE_ASSIGNMENT || method_ast->type == NECRO_AST_APATS_ASSIGNMENT);

                //--------------------------------
                // Create dictionary prototype entry
                NecroDictionaryPrototype* prev_dictionary = instance->dictionary_prototype;
                instance->dictionary_prototype            = necro_paged_arena_alloc(infer->arena, sizeof(NecroDictionaryPrototype));
                if (method_ast->type == NECRO_AST_SIMPLE_ASSIGNMENT)
                    instance->dictionary_prototype->instance_member_ast_symbol = method_ast->simple_assignment.ast_symbol;
                else if (method_ast->type == NECRO_AST_APATS_ASSIGNMENT)
                    instance->dictionary_prototype->instance_member_ast_symbol = method_ast->apats_assignment.ast_symbol;
                else
                    assert(false);
                instance->dictionary_prototype->next                         = NULL;
                instance->dictionary_prototype->type_class_member_ast_symbol = necro_scope_find_ast_symbol(infer->scoped_symtable->top_scope, necro_intern_get_type_class_member_symbol_from_instance_symbol(infer->intern, instance->dictionary_prototype->instance_member_ast_symbol->source_name));

                if (instance->dictionary_prototype->type_class_member_ast_symbol == NULL)
                {
                    return necro_type_not_a_visible_method_error(instance->dictionary_prototype->instance_member_ast_symbol, instance->data_type, type_class->type, NULL, NULL, method_ast->source_loc, method_ast->end_loc);
                }

                //--------------------------------
                // Assemble types for overloaded methods
                NecroType* method_type                                                         = instance->dictionary_prototype->type_class_member_ast_symbol->type;
                NecroType* inst_method_type                                                    = necro_instantiate_method_sig(infer, type_class->type_var, method_type, instance->data_type);
                instance->dictionary_prototype->instance_member_ast_symbol->type               = inst_method_type;
                instance->dictionary_prototype->instance_member_ast_symbol->type->pre_supplied = true;

                //--------------------------------
                // next
                instance->dictionary_prototype->next = prev_dictionary;
                declaration_group                    = declaration_group->declaration.next_declaration;
            }
            group_list = group_list->declaration_group_list.next;
        }
    }

    //--------------------------------
    // Infer declarations
    if (instance->ast->type_class_instance.declarations != NULL)
    {
        necro_try(NecroType, necro_infer_go(infer, instance->ast->type_class_instance.declarations));
    }

    //--------------------------------
    // Missing members check
    NecroTypeClassMember* type_class_members = type_class->members;
    while (type_class_members != NULL)
    {
        NecroDictionaryPrototype* dictionary_prototype = instance->dictionary_prototype;
        bool matched = false;
        while (dictionary_prototype != NULL)
        {
            if (dictionary_prototype->type_class_member_ast_symbol == type_class_members->member_varid)
            {
                matched = true;
                break;
            }
            dictionary_prototype = dictionary_prototype->next;
        }
        if (!matched)
        {
            return necro_type_no_explicit_implementation_error(type_class_members->member_varid, instance->data_type, type_class->type, NULL, NULL, instance->ast->source_loc, instance->ast->end_loc);
        }
        type_class_members = type_class_members->next;
    }

    //---------------------------------
    // Resolve Super instance symbols
    NecroTypeClassContext* super_classes = type_class->context;
    while (super_classes != NULL)
    {
        NecroTypeClassInstance* super_instance = necro_get_type_class_instance(instance->data_type_name, super_classes->class_symbol);
        if (super_instance == NULL)
        {
            return necro_type_does_not_implement_super_class_error(super_classes->class_symbol, instance->data_type, type_class->type, NULL, NULL, ast->type_class_instance.inst->source_loc, ast->type_class_instance.inst->end_loc);
        }
        // assert(super_instance != NULL);
        // instance->super_instances = necro_cons_instance_list(infer->arena, super_instance, instance->super_instances);
        super_classes = super_classes->next;
    }

    return ok(NecroType, NULL);
}

NecroResult(NecroTypeClassContext) necro_ast_to_context(NecroInfer* infer, NecroAst* context_ast)
{
    NecroTypeClassContext* context      = NULL;
    NecroTypeClassContext* context_head = NULL;
    while (context_ast != NULL)
    {
        if (context_ast->type == NECRO_AST_LIST_NODE)
        {
            assert(context_ast->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroTypeClassContext* new_context = necro_create_type_class_context(
                infer->arena,
                context_ast->list.item->type_class_context.conid->conid.ast_symbol->type_class,
                context_ast->list.item->type_class_context.conid->conid.ast_symbol,
                context_ast->list.item->type_class_context.varid->variable.ast_symbol,
                NULL
            );
            if (context_head == NULL)
            {
                context      = new_context;
                context_head = context;
            }
            else
            {
                context->next = new_context;
                context       = context->next;
            }
            if (context->class_symbol->type_class == NULL)
            {
                return necro_type_not_a_class_error(context->class_symbol, context->class_symbol->type, context_ast->source_loc, context_ast->end_loc);
            }
            context_ast = context_ast->list.next_item;
        }
        else if (context_ast->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        {
            context = necro_create_type_class_context(
                infer->arena,
                context_ast->type_class_context.conid->conid.ast_symbol->type_class,
                context_ast->type_class_context.conid->conid.ast_symbol,
                context_ast->type_class_context.varid->variable.ast_symbol,
                NULL
            );
            if (context->class_symbol->type_class == NULL)
            {
                return necro_type_not_a_class_error(context->class_symbol, context->class_symbol->type, context_ast->source_loc, context_ast->end_loc);
            }
            return ok(NecroTypeClassContext, context);
        }
        else
        {
            necro_unreachable(NecroTypeClassContext);
        }
    }
    return ok(NecroTypeClassContext, context_head);
}

NecroTypeClassInstance* necro_get_type_class_instance(NecroAstSymbol* data_type_symbol, NecroAstSymbol* type_class_symbol)
{
    NecroInstanceList* instance_list = data_type_symbol->instance_list;
    while (instance_list != NULL)
    {
        if (instance_list->data->type_class_name == type_class_symbol)
            return instance_list->data;
        instance_list = instance_list->next;
    }
    return NULL;
}

NecroResult(NecroType) necro_propagate_type_classes(NecroPagedArena* arena, NecroBase* base, NecroTypeClassContext* classes, NecroType* type, NecroScope* scope)
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
                necro_try(NecroType, necro_propagate_type_classes(arena, base, instance->context, current_arg->list.item, instance->ast->scope));
                current_arg = current_arg->list.next;
            }
            classes = classes->next;
        }
        return ok(NecroType, NULL);

    case NECRO_TYPE_FUN:
        // TODO: Type classes for functions!!!
        necro_try(NecroType, necro_propagate_type_classes(arena, base, classes, type->fun.type1, scope));
        return necro_propagate_type_classes(arena, base, classes, type->fun.type2, scope);

    case NECRO_TYPE_APP:
        // necro_propagate_type_classes(infer, classes, type->app.type1, macro_type, error_preamble);
        // necro_propagate_type_classes(infer, classes, type->app.type2, macro_type, error_preamble);
        assert(false && "Compiler bug: TypeApp not implemented in necro_propagate_type_classes! (i.e. constraints of the form: Num (f a), or (c a), are not currently supported)");
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

// // TODO: Knock this around a bit!
// void necro_type_class_translate_constant(NecroInfer* infer, NecroAst* ast, NecroTypeClassDictionaryContext* dictionary_context)
// {
//     assert(infer != NULL);
//     assert(ast != NULL);
//     assert(ast->type == NECRO_AST_CONSTANT);
//
//     // Removing overloaded numeric literal patterns for now.
// #if 0
//     if (ast->constant.type != NECRO_AST_CONSTANT_FLOAT_PATTERN && ast->constant.type != NECRO_AST_CONSTANT_INTEGER_PATTERN) return;
//
//     const char*     from_name  = NULL;
//     NecroTypeClass* type_class = NULL;
//     NecroType*      num_type   = NULL;
//     switch (ast->constant.type)
//     {
//     case NECRO_AST_CONSTANT_INTEGER_PATTERN:
//         from_name  = "fromInt";
//         type_class = necro_symtable_get(infer->symtable, infer->prim_types->num_type_class.id)->type_class;
//         num_type   = necro_symtable_get(infer->symtable, infer->prim_types->int_type.id)->type;
//         break;
//     case NECRO_AST_CONSTANT_FLOAT_PATTERN:
//         from_name  = "fromRational";
//         type_class = necro_symtable_get(infer->symtable, infer->prim_types->fractional_type_class.id)->type_class;
//         num_type   = necro_symtable_get(infer->symtable, infer->prim_types->rational_type.id)->type;
//         break;
//     default: assert(false);
//     }
//
//     //-----------------
//     // from method
//     {
//         NecroASTNode* from_ast  = necro_create_variable_ast(infer->arena, infer->intern, from_name, NECRO_VAR_VAR);
//         from_ast->scope         = ast->scope;
//         NecroType* from_type    = necro_create_type_fun(infer, num_type, ast->necro_type);
//         from_type->type_kind    = infer->star_type_kind;
//         from_ast->necro_type    = from_type;
//         necro_rename_var_pass(infer->renamer, infer->arena, from_ast);
//         NecroTypeClassContext* inst_context = necro_create_type_class_context(infer->arena, type_class, type_class->type_class_name, necro_var_to_con(ast->necro_type->var.var), NULL);
//         NecroASTNode* m_var = necro_resolve_method(infer, type_class, inst_context, from_ast, dictionary_context);
//         if (necro_is_infer_error(infer)) return;
//         assert(m_var != NULL);
//         m_var->scope   = ast->scope;
//         necro_rename_var_pass(infer->renamer, infer->arena, m_var);
//         if (necro_is_infer_error(infer)) return;
//         ast->constant.pat_from_ast = m_var;
//     }
//
//     //-----------------
//     // eq method
//     {
//         type_class           = necro_symtable_get(infer->symtable, infer->prim_types->eq_type_class.id)->type_class;
//         NecroASTNode* eq_ast = necro_create_variable_ast(infer->arena, infer->intern, "eq", NECRO_VAR_VAR);
//         eq_ast->scope        = ast->scope;
//         NecroType* eq_type   = necro_create_type_fun(infer, ast->necro_type, necro_create_type_fun(infer, ast->necro_type, necro_symtable_get(infer->symtable, infer->prim_types->bool_type.id)->type));
//         eq_type->type_kind   = infer->star_type_kind;
//         eq_ast->necro_type   = eq_type;
//         necro_rename_var_pass(infer->renamer, infer->arena, eq_ast);
//         NecroTypeClassContext* inst_context = necro_create_type_class_context(infer->arena, type_class, type_class->type_class_name, necro_var_to_con(ast->necro_type->var.var), NULL);
//         NecroASTNode* m_var = necro_resolve_method(infer, type_class, inst_context, eq_ast, dictionary_context);
//         if (necro_is_infer_error(infer)) return;
//         assert(m_var != NULL);
//         m_var->scope   = ast->scope;
//         necro_rename_var_pass(infer->renamer, infer->arena, m_var);
//         if (necro_is_infer_error(infer)) return;
//         ast->constant.pat_from_ast = m_var;
//     }
// #else
//     UNUSED(dictionary_context);
// #endif
// }


/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"
#include "symtable.h"
#include "type_class.h"

#define NECRO_TYPE_CLASS_DEBUG 0
#if NECRO_TYPE_CLASS_DEBUG
#define TRACE_TYPE_CLASS(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE_CLASS(...)
#endif

// TODO:
//    * Multiple pattern case declarations
//    * Insure instance are also instance of super classes!
//    * ... Lots of shit
//    * Kinds test on fucking constraints, fuuuuuuck

NecroTypeClassEnv necro_create_type_class_env()
{
    return (NecroTypeClassEnv)
    {
        .class_table    = necro_create_type_class_table(),
        .instance_table = necro_create_type_class_instance_table(),
        .arena          = necro_create_paged_arena(),
    };
}

void necro_destroy_type_class_env(NecroTypeClassEnv* env)
{
    necro_destroy_type_class_table(&env->class_table);
    necro_destroy_type_class_instance_table(&env->instance_table);
    necro_destroy_paged_arena(&env->arena);
}

NecroTypeClassContext* necro_gather_class_declaration_context(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class, NecroTypeClassContext* in_context)
{
    NecroTypeClassContext* context             = necro_union_contexts(infer, type_class->context, in_context);
    NecroTypeClassContext* current_super_class = type_class->context;
    while (current_super_class != NULL)
    {
        NecroTypeClass* super_class = necro_type_class_table_get(&env->class_table, current_super_class->type_class_name.id.id);
        if (super_class == NULL)
        {
            necro_infer_ast_error(infer, NULL, NULL, "%s is not a constraint", necro_intern_get_string(infer->intern, current_super_class->type_class_name.symbol));
            return NULL;
        }
    }
    return NULL;
}

//=====================================================
// Type Class Declaration member
//=====================================================
void necro_create_type_class_declaration_pass1(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    TRACE_TYPE_CLASS("necro_create_type_class\n");

    NecroTypeClass* type_class       = necro_type_class_table_insert(&env->class_table, ast->type_class_declaration.tycls->conid.id.id, NULL);
    type_class->type_class_name      = (NecroCon) { .symbol = ast->type_class_declaration.tycls->conid.symbol, .id = ast->type_class_declaration.tycls->conid.id };
    type_class->members              = NULL;
    type_class->type_var             = (NecroCon) { .symbol = ast->type_class_declaration.tyvar->variable.symbol, .id = ast->type_class_declaration.tyvar->variable.id };
    type_class->context              = NULL;

    // Create type for type_class
    NecroType* ty_var              = necro_new_name(infer);
    ty_var->var.is_rigid           = true;
    NecroType* arg_list            = necro_create_type_list(infer, ty_var, NULL);
    type_class->type               = necro_make_con_1(infer, ast->type_class_declaration.tycls->conid.symbol, arg_list);
    type_class->type->con.is_class = true;

    // Construct context
    // 1. Add TypeClass as context to member type signatures
    // 2. Add ambiguity check to make sure (what exactly)???
    NecroNode* contexts = ast->type_class_declaration.context;
    while (contexts != NULL)
    {
        TRACE_TYPE_CLASS("context type: %d\n", contexts->type);
        if (contexts->type == NECRO_AST_LIST_NODE)
        {
            assert(contexts->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroTypeClassContext* new_context = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassContext));
            new_context->type_class_name       = (NecroCon) { .symbol = contexts->list.item->type_class_context.conid->conid.symbol,    .id = contexts->list.item->type_class_context.conid->conid.id };
            new_context->type_var              = (NecroCon) { .symbol = contexts->list.item->type_class_context.varid->variable.symbol, .id = contexts->list.item->type_class_context.varid->variable.id };
            new_context->next                  = type_class->context;
            type_class->context                = new_context;
            contexts                           = contexts->list.next_item;
        }
        else if (contexts->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        {
            type_class->context                  = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassContext));
            type_class->context->type_class_name = (NecroCon) { .symbol = contexts->type_class_context.conid->conid.symbol,    .id = contexts->type_class_context.conid->conid.id };
            type_class->context->type_var        = (NecroCon) { .symbol = contexts->type_class_context.varid->variable.symbol, .id = contexts->type_class_context.varid->variable.id };
            type_class->context->next            = NULL;
            break;
        }
        else
        {
            assert(false);
        }
    }
}

void necro_create_type_class_declaration_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, ast->type_class_declaration.tycls->conid.id.id);
    assert(type_class != NULL);

    // Member Type Signatures
    NecroNode* declarations = ast->type_class_declaration.declarations;
    while (declarations != NULL)
    {
        // assert(declarations->declaration.declaration_impl->type == NECRO_AST_TYPE_SIGNATURE);
        if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
            continue;
        // declarations.
        necro_infer_type_sig(infer, declarations->declaration.declaration_impl);
        NecroType* declaration_type = necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type;
        necro_check_type_sanity(infer, declaration_type);
        if (necro_is_infer_error(infer)) return;
        necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->is_method = true;
        // unify, then generalize?
        // declaration_type = necro_gen(infer, declaration_type, NULL);
        // necro_print_type_sig(declaration_type, infer->intern);
        // necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type = declaration_type;

        NecroTypeClassMember* prev_member = type_class->members;
        type_class->members               = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
        type_class->members->member_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->type_signature.var->variable.symbol, .id = declarations->declaration.declaration_impl->type_signature.var->variable.id };
        type_class->members->next         = prev_member;

        declarations = declarations->declaration.next_declaration;
    }

    // // Pass 2 - Default Member Implementations
    // while (declarations != NULL)
    // {
    //     if (declarations->declaration.declaration_impl->type != NECRO_AST_SIMPLE_ASSIGNMENT || declarations->declaration.declaration_impl->type != NECRO_AST_APATS_ASSIGNMENT)
    //         continue;
    //     NecroType* declaration_type = necro_infer_go(env->infer, declarations->declaration.declaration_impl);
    //     necro_check_type_sanity(env->infer, declaration_type);
    //     if (necro_is_infer_error(env->infer)) return;
    //     type_class->member_type_sig_list = necro_create_type_list(env->infer, declaration_type, type_class->member_type_sig_list);
    //     declarations = declarations->declaration.next_declaration;
    // }

}

//=====================================================
// Type Class Instance
//=====================================================
void necro_create_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    TRACE_TYPE_CLASS("necro_create_type_class_instance\n");
    return;

    NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
    NecroCon data_type_name;
    if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
    {
        data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
    }
    else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
    {
        data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
    }
    else
    {
        assert(false);
    }

    uint64_t key = necro_create_instance_key(data_type_name, type_class_name);
    if (necro_type_class_instance_table_get(&env->instance_table, key) != NULL)
    {
        necro_infer_ast_error(infer, NULL, ast, "Duplicate Type Class Instance\n     class: %s\n     type: %s", necro_intern_get_string(infer->intern, type_class_name.symbol), necro_intern_get_string(infer->intern, data_type_name.symbol));
        return;
    }

    NecroTypeClassInstance* instance   = necro_type_class_instance_table_insert(&env->instance_table, key, NULL);
    instance->type_class_name          = type_class_name;
    instance->data_type_name           = data_type_name;
    instance->context                  = NULL;
    instance->dictionary_prototype     = NULL;

    NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, type_class_name.id.id);
    if (type_class == NULL)
    {
        necro_infer_ast_error(infer, NULL, ast, "%s cannot be an instance of %s, because %s is not a type class.", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
        return;
    }

    // Construct context
    NecroNode* contexts = ast->type_class_instance.context;
    while (contexts != NULL)
    {
        TRACE_TYPE_CLASS("context type: %d\n", contexts->type);
        if (contexts->type == NECRO_AST_LIST_NODE)
        {
            assert(contexts->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroTypeClassContext* new_context = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassContext));
            new_context->type_class_name       = (NecroCon) { .symbol = contexts->list.item->type_class_context.conid->conid.symbol,    .id = contexts->list.item->type_class_context.conid->conid.id };
            new_context->type_var              = (NecroCon) { .symbol = contexts->list.item->type_class_context.varid->variable.symbol, .id = contexts->list.item->type_class_context.varid->variable.id };
            new_context->next                  = instance->context;
            instance->context                  = new_context;
            contexts                           = contexts->list.next_item;
        }
        else if (contexts->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        {
            instance->context                  = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassContext));
            instance->context->type_class_name = (NecroCon) { .symbol = contexts->type_class_context.conid->conid.symbol,    .id = contexts->type_class_context.conid->conid.id };
            instance->context->type_var        = (NecroCon) { .symbol = contexts->type_class_context.varid->variable.symbol, .id = contexts->type_class_context.varid->variable.id };
            instance->context->next            = NULL;
            break;
        }
        else
        {
            assert(false);
        }
    }

    // Dictionary Prototype
    NecroNode* declarations = ast->type_class_instance.declarations;
    while (declarations != NULL)
    {
        assert(declarations->type == NECRO_AST_DECL);
        assert(declarations->declaration.declaration_impl != NULL);

        NecroDictionaryPrototype* prev_dictionary = instance->dictionary_prototype;
        instance->dictionary_prototype            = necro_paged_arena_alloc(&env->arena, sizeof(NecroDictionaryPrototype));
        if (declarations->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT)
        {
            instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->simple_assignment.variable_name, .id = declarations->declaration.declaration_impl->simple_assignment.id };
        }
        else if (declarations->declaration.declaration_impl->type == NECRO_AST_APATS_ASSIGNMENT)
        {
            instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->apats_assignment.variable_name, .id = declarations->declaration.declaration_impl->apats_assignment.id };
        }
        else
        {
            assert(false);
        }
        NecroSymbol member_symbol = necro_intern_get_type_class_member_symbol_from_instance_symbol(infer->intern, instance->dictionary_prototype->prototype_varid.symbol);

        // Search for Type Class member
        bool                  found   = false;
        NecroTypeClassMember* members = type_class->members;
        while (members != NULL)
        {
            if (members->member_varid.symbol.id == member_symbol.id)
            {
                instance->dictionary_prototype->type_class_member_varid = (NecroCon) { .symbol = members->member_varid.symbol, .id = members->member_varid.id };
                found = true;
            }
            members = members->next;
        }
        if (found == false)
        {
            necro_infer_ast_error(infer, NULL, ast, "\'%s\' is not a (visible) method of class \'%s\'", necro_intern_get_string(infer->intern, member_symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
            return;
        }

        instance->dictionary_prototype->next = prev_dictionary;
        declarations = declarations->declaration.next_declaration;
    }

}

uint64_t necro_create_instance_key(NecroCon data_type_name, NecroCon type_class_name)
{
    return (((uint64_t)data_type_name.id.id & 0xFFFFFFFF) << 32) | ((uint64_t)type_class_name.id.id & 0xFFFFFFFF);
}

NecroTypeClassInstance* necro_get_instance(NecroInfer* infer, NecroCon data_type_name, NecroCon type_class_name)
{
    NecroTypeClass* type_class = necro_type_class_table_get(&infer->type_class_env->class_table, type_class_name.id.id);
    if (type_class == NULL)
    {
        necro_infer_ast_error(infer, NULL, NULL, "\'%s\' is not a type class", necro_intern_get_string(infer->intern, type_class_name.symbol));
        return false;
    }
    uint64_t key = necro_create_instance_key(data_type_name, type_class_name);
    return necro_type_class_instance_table_get(&infer->type_class_env->instance_table, type_class_name.id.id);
}

bool necro_is_data_type_instance_of_class(NecroInfer* infer, NecroCon data_type_name, NecroCon type_class_name)
{
    return necro_get_instance(infer, data_type_name, type_class_name) != NULL;
}

//=====================================================
// Contexts
//=====================================================
bool necro_context_contains_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class)
{
    while (context != NULL)
    {
        if (context->type_class_name.id.id == type_class->type_class_name.id.id)
            return true;
        context = context->next;
    }
    return false;
}

NecroTypeClassContext* necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2)
{
    if (context1 == NULL)
        return context2;
    if (context2 == NULL)
        return context1;

    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        NecroTypeClassContext* new_context = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassContext));
        new_context->type_class_name       = context1->type_class_name;
        new_context->type_var              = context1->type_var;
        new_context->next                  = NULL;
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
        context1 = context1->next;
    }

    // Insert context2 classes if they aren't already in context1
    while (context2 != NULL)
    {
        if (!necro_context_contains_class(head, context2))
        {
            NecroTypeClassContext* new_context = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassContext));
            new_context->type_class_name       = context2->type_class_name;
            new_context->type_var              = context2->type_var;
            new_context->next                  = NULL;
            curr->next                         = new_context;
            curr                               = curr->next;
        }
        context2 = context2->next;
    }
    return head;
}

//=====================================================
// Printing
//=====================================================
void necro_print_type_class_member(NecroTypeClassMember* member, NecroInfer* infer, NecroIntern* intern, size_t white_count)
{
    if (member == NULL)
        return;
    NecroType* member_type = necro_symtable_get(infer->symtable, member->member_varid.id)->type;
    print_white_space(white_count + 4);
    printf("%s :: ", necro_intern_get_string(intern, member->member_varid.symbol));
    necro_print_type_sig(member_type, intern);
    necro_print_type_class_member(member->next, infer, intern, white_count);
}

void necro_print_type_class_context(NecroTypeClassContext* context, NecroInfer* infer, NecroIntern* intern, size_t white_count)
{
    if (context == NULL)
        return;
    if (context->next != NULL)
        printf("%s %s,", necro_intern_get_string(intern, context->type_class_name.symbol), necro_intern_get_string(intern, context->type_var.symbol));
    else
        printf("%s %s", necro_intern_get_string(intern, context->type_class_name.symbol), necro_intern_get_string(intern, context->type_var.symbol));
    necro_print_type_class_context(context->next, infer, intern, white_count);
}

void necro_print_type_class(NecroTypeClass* type_class, NecroInfer* infer, NecroIntern* intern, size_t white_count)
{
    if (type_class == NULL)
        return;
    print_white_space(white_count);
    printf("class %s %s\n", necro_intern_get_string(intern, type_class->type_class_name.symbol), necro_intern_get_string(intern, type_class->type_var.symbol));
    print_white_space(white_count);
    printf("{\n");
    print_white_space(white_count + 4);
    printf("type:    ");
    necro_print_type_sig(type_class->type, intern);
    print_white_space(white_count + 4);
    printf("context: (");
    necro_print_type_class_context(type_class->context, infer, intern, white_count + 4);
    printf(") =>\n");
    print_white_space(white_count + 4);
    printf("members:\n");
    print_white_space(white_count + 4);
    printf("[\n");
    necro_print_type_class_member(type_class->members, infer, intern, white_count + 4);
    print_white_space(white_count + 4);
    printf("]\n");
    print_white_space(white_count);
    printf("}\n");
}

void necro_print_dictionary_prototype(NecroDictionaryPrototype* prototype, NecroInfer* infer, NecroIntern* intern, size_t white_count)
{
    if (prototype == NULL)
        return;
    NecroType* member_type = necro_symtable_get(infer->symtable, prototype->type_class_member_varid.id)->type;
    print_white_space(white_count + 4);
    printf("%s :: ", necro_intern_get_string(intern, prototype->prototype_varid.symbol));
    necro_print_type_sig(member_type, intern);
    necro_print_dictionary_prototype(prototype->next, infer, intern, white_count);
}


void necro_print_instance(NecroTypeClassInstance* instance, NecroInfer* infer, NecroIntern* intern, size_t white_count)
{
    if (instance == NULL)
        return;
    print_white_space(white_count);
    printf("instance %s %s\n", necro_intern_get_string(intern, instance->type_class_name.symbol), necro_intern_get_string(intern, instance->data_type_name.symbol));
    print_white_space(white_count);
    printf("{\n");
    print_white_space(white_count + 4);
    printf("context: (");
    necro_print_type_class_context(instance->context, infer, intern, white_count + 4);
    printf(") =>\n");
    print_white_space(white_count + 4);
    printf("dictionary prototype:\n");
    print_white_space(white_count + 4);
    printf("[\n");
    necro_print_dictionary_prototype(instance->dictionary_prototype, infer, intern, white_count + 4);
    print_white_space(white_count + 4);
    printf("]\n");
    print_white_space(white_count);
    printf("}\n");
}

typedef struct
{
    NecroInfer*  infer;
    NecroIntern* intern;
} NecroPrintTypeClassArgs;

void necro_print_type_class_go(NecroTypeClass* type_class, NecroPrintTypeClassArgs* args)
{
    necro_print_type_class(type_class, args->infer, args->intern, 4);
}

void necro_print_type_class_instance_go(NecroTypeClassInstance* instance, NecroPrintTypeClassArgs* args)
{
    necro_print_instance(instance, args->infer, args->intern, 4);
}

void necro_print_type_class_env(NecroTypeClassEnv* env, NecroInfer* infer, NecroIntern* intern)
{
    NecroPrintTypeClassArgs args = { infer, intern };

    printf("\nTypeClasses\n{\n");
    necro_type_class_table_iterate(&env->class_table, &necro_print_type_class_go, &args);
    printf("}\n");

    printf("\nInstances\n{\n");
    necro_type_class_instance_table_iterate(&env->instance_table, &necro_print_type_class_instance_go, &args);
    printf("}\n");

}
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

// TODO:
//    * Insert checks to make sure that instances conform to the type class and implement the super class, and implement ALL methods!
//    * Also type check and unify instance method implementations!!!!!!
//    * Multiple pattern case declarations
//    * Insure instance are also instance of super classes!
//    * Kinds test on fucking constraints, fuuuuuuck
//    * Equipped with type classes we can make primitive functions: addInt, addFloat, addAudio, etc
//    * ... Lots of shit

#define NECRO_TYPE_CLASS_DEBUG 0
#if NECRO_TYPE_CLASS_DEBUG
#define TRACE_TYPE_CLASS(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE_CLASS(...)
#endif

//=====================================================
// Forward Declarations
//=====================================================
NecroTypeClassContext* necro_create_type_class_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next);

//=====================================================
// NecroTypeClassEnv
//=====================================================
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

    type_class->context            = necro_ast_to_context(infer, env, ast->type_class_declaration.context);
}

void necro_create_type_class_declaration_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, ast->type_class_declaration.tycls->conid.id.id);
    assert(type_class != NULL);

    //--------------------------------
    //Member Type Signatures
    NecroNode* declarations = ast->type_class_declaration.declarations;

    // NecroTypeClassContext* context = necro_union_contexts(infer, necro_ast_to_context(infer, env, declarations->declaration.declaration_impl->type_signature.context), necro_gather_class_declaration_context(infer, env, type_class));
    // NecroTypeClassContext* context = necro_union_contexts(infer, necro_ast_to_context(infer, env, declarations->declaration.declaration_impl->type_signature.context), type_class->context);
    NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, type_class->type_class_name, type_class->type_var, NULL);
    NecroTypeClassContext* context       = necro_union_contexts(infer, necro_ast_to_context(infer, env, declarations->declaration.declaration_impl->type_signature.context), class_context);
    NecroTyVarContextList* context_list  = necro_create_ty_var_context_list(infer, context);

    while (declarations != NULL)
    {
        if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
            continue;

        NecroType* type_sig = necro_ast_to_type_sig_go(infer, declarations->declaration.declaration_impl->type_signature.type);
        if (necro_is_infer_error(infer)) return;
        necro_check_type_sanity(infer, type_sig);
        if (necro_is_infer_error(infer)) return;
        type_sig->pre_supplied = true;
        type_sig->source_loc   = declarations->declaration.declaration_impl->source_loc;
        if (necro_is_context_ambiguous(infer, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, context, type_sig))
            return;
        necro_add_constraints_to_ty_vars(infer, type_sig, context_list);
        type_sig               = necro_gen(infer, type_sig, NULL);
        necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type      = type_sig;
        necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->is_method = true;

        NecroTypeClassMember* prev_member = type_class->members;
        type_class->members               = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
        type_class->members->member_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->type_signature.var->variable.symbol, .id = declarations->declaration.declaration_impl->type_signature.var->variable.id };
        type_class->members->next         = prev_member;

        declarations = declarations->declaration.next_declaration;
    }

    //--------------------------------
    //Default Member Implementations
}

//=====================================================
// Type Class Instance
//=====================================================
void necro_create_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    TRACE_TYPE_CLASS("necro_create_type_class_instance\n");

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

    NecroTypeClassInstance* instance = necro_type_class_instance_table_insert(&env->instance_table, key, NULL);
    instance->type_class_name        = type_class_name;
    instance->data_type_name         = data_type_name;
    instance->context                = NULL;
    instance->dictionary_prototype   = NULL;

    NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, type_class_name.id.id);
    if (type_class == NULL)
    {
        necro_infer_ast_error(infer, NULL, ast, "%s cannot be an instance of %s, because %s is not a type class.", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
        return;
    }

    // Construct context
    instance->context = necro_ast_to_context(infer, env, ast->type_class_instance.context);

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
    return necro_type_class_instance_table_get(&infer->type_class_env->instance_table, key);
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

// NecroTypeClassContext* necro_gather_class_declaration_context(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class)
// {
//     NecroTypeClassContext* context             = NULL;
//     NecroTypeClassContext* current_super_class = type_class->context;
//     while (current_super_class != NULL)
//     {
//         NecroTypeClass* super_class = necro_type_class_table_get(&env->class_table, current_super_class->type_class_name.id.id);
//         if (super_class == NULL)
//         {
//             necro_infer_ast_error(infer, NULL, NULL, "%s is not a constraint", necro_intern_get_string(infer->intern, current_super_class->type_class_name.symbol));
//             return NULL;
//         }
//         context = necro_union_contexts(infer, context, necro_gather_class_declaration_context(infer, env, super_class));
//         current_super_class = current_super_class->next;
//     }
//     return context;
// }

NecroTypeClassContext* necro_create_type_class_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next)
{
    NecroTypeClassContext* context = necro_paged_arena_alloc(arena, sizeof(NecroTypeClassContext));
    context->type_class_name       = type_class_name;
    context->type_var              = type_var;
    context->next                  = next;
    return context;
}

NecroTypeClassContext* necro_ast_to_context(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* context_ast)
{
    NecroTypeClassContext* context      = NULL;
    NecroTypeClassContext* context_head = NULL;
    while (context_ast != NULL)
    {
        // TRACE_TYPE_CLASS("context type: %d\n", contexts->type);
        if (context_ast->type == NECRO_AST_LIST_NODE)
        {
            assert(context_ast->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroTypeClassContext* new_context = necro_create_type_class_context(
                &env->arena,
                (NecroCon) { .symbol = context_ast->list.item->type_class_context.conid->conid.symbol,    .id = context_ast->list.item->type_class_context.conid->conid.id },
                (NecroCon) { .symbol = context_ast->list.item->type_class_context.varid->variable.symbol, .id = context_ast->list.item->type_class_context.varid->variable.id },
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
            context_ast = context_ast->list.next_item;
        }
        else if (context_ast->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        {
            context = necro_create_type_class_context(
                &env->arena,
                (NecroCon) { .symbol = context_ast->type_class_context.conid->conid.symbol,    .id = context_ast->type_class_context.conid->conid.id },
                (NecroCon) { .symbol = context_ast->type_class_context.varid->variable.symbol, .id = context_ast->type_class_context.varid->variable.id },
             NULL
            );
            return context;
        }
        else
        {
            assert(false);
        }
    }
    return context_head;
}

bool necro_context_list_contains_ty_var(NecroTyVarContextList* context_list, NecroCon ty_var)
{
    while (context_list != NULL)
    {
        if (context_list->type_var.id.id == ty_var.id.id)
            return true;
        context_list = context_list->next;
    }
    return false;
}

NecroTyVarContextList* necro_create_ty_var_context_list(NecroInfer* infer, NecroTypeClassContext* context)
{
    NecroCon               current_var;
    NecroTyVarContextList* context_list        = NULL;
    NecroTypeClassContext* current_var_context = NULL;
    NecroTypeClassContext* sub_context         = NULL;
    NecroTypeClassContext* current_context     = context;
    while (current_context != NULL)
    {
        if (necro_context_list_contains_ty_var(context_list, current_context->type_var))
            continue;
        current_var_context = NULL;
        current_var         = current_context->type_var;
        sub_context         = context;
        while (sub_context != NULL)
        {
            if (sub_context->type_var.id.id == current_var.id.id)
            {
                NecroTypeClassContext* new_context = necro_create_type_class_context(&infer->arena, sub_context->type_class_name, sub_context->type_var, current_var_context);
                current_var_context                = new_context;
            }
            sub_context = sub_context->next;
        }
        if (current_var_context != NULL)
        {
            NecroTyVarContextList* new_context_list = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTyVarContextList));
            new_context_list->type_var              = current_var;
            new_context_list->context               = current_var_context;
            new_context_list->next                  = context_list;
            context_list                            = new_context_list;
        }
        current_context = current_context->next;
    }
    return context_list;
}

void necro_add_constraints_to_ty_vars(NecroInfer* infer, NecroType* type, NecroTyVarContextList* context_list)
{
    assert(infer != NULL);
    if (necro_is_infer_error(infer)) return;
    if (type == NULL) return;
    if (context_list == NULL) return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        while (context_list != NULL)
        {
            if (type->var.var.id.id == context_list->type_var.id.id)
            {
                type->var.context = context_list->context;
                return;
            }
            context_list = context_list->next;
        }
        break;
    }
    case NECRO_TYPE_APP:  necro_add_constraints_to_ty_vars(infer, type->app.type1, context_list); necro_add_constraints_to_ty_vars(infer, type->app.type2, context_list); break;
    case NECRO_TYPE_FUN:  necro_add_constraints_to_ty_vars(infer, type->fun.type1, context_list); necro_add_constraints_to_ty_vars(infer, type->fun.type2, context_list); break;
    case NECRO_TYPE_CON:  necro_add_constraints_to_ty_vars(infer, type->con.args, context_list); break;
    case NECRO_TYPE_LIST: necro_add_constraints_to_ty_vars(infer, type->list.item, context_list); necro_add_constraints_to_ty_vars(infer, type->list.next, context_list); break;
    case NECRO_TYPE_FOR:  necro_infer_ast_error(infer, type, NULL, "Compiler bug: ForAll Type in add_constrants_to_ty_vars: %d", type->type); break;
    default:              necro_infer_ast_error(infer, type, NULL, "Compiler bug: Unimplemented Type in add_constrants_to_ty_vars: %d", type->type); break;
    }
}

bool necro_ambig_occurs(NecroInfer* infer, NecroCon name, NecroType* type)
{
    if (type == NULL)
        return false;
    type = necro_find(infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return name.id.id == type->var.var.id.id;
    case NECRO_TYPE_APP:  return necro_ambig_occurs(infer, name, type->app.type1) || necro_ambig_occurs(infer, name, type->app.type2);
    case NECRO_TYPE_FUN:  return necro_ambig_occurs(infer, name, type->fun.type1) || necro_ambig_occurs(infer, name, type->fun.type2);
    case NECRO_TYPE_CON:  return necro_ambig_occurs(infer, name, type->con.args);
    case NECRO_TYPE_LIST: return necro_ambig_occurs(infer, name, type->list.item) || necro_ambig_occurs(infer, name, type->list.next);
    case NECRO_TYPE_FOR:  necro_infer_ast_error(infer, type, NULL, "Found polytype in occurs check!"); return false;
    default:              necro_infer_ast_error(infer, type, NULL, "Compiler bug: Unrecognized type: %d.", type->type); return false;
    }
    return false;
}

bool necro_is_context_ambiguous(NecroInfer* infer, NecroSymbol type_sig_name, NecroTypeClassContext* context, NecroType* type)
{
    if (necro_is_infer_error(infer)) return false;
    if (type == NULL)
        return false;
    while (context != NULL)
    {
        if (!necro_ambig_occurs(infer, context->type_var, type))
        {
            necro_infer_ast_error(infer, type, NULL, "Could not deduce (%s %s) in ambiguity check for \'%s\'",
                necro_intern_get_string(infer->intern, context->type_class_name.symbol),
                necro_id_as_character_string(infer, context->type_var.id),
                necro_intern_get_string(infer->intern, type_sig_name));
            return true;
        }
        context = context->next;
    }
    return false;
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
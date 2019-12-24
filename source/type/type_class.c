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
#include "type/derive.h"
#include "base.h"

/*
TODO
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
NecroType* necro_instantiate_method_sig(NecroInfer* infer, NecroType* a_method_type, NecroType* a_data_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    a_data_type->ownership = NULL;
    NecroInstSub* subs     = NULL;
    NecroType*    type     = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, a_method_type, NULL, &subs, source_loc, end_loc);
    a_data_type            = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, a_data_type, NULL, source_loc, end_loc);
    while (subs != NULL)
    {
        if (subs->var_to_replace->is_class_head_var)
        {
            unwrap(NecroType, necro_type_unify(infer->arena, &infer->con_env, infer->base, subs->new_name, a_data_type, NULL));
            unwrap(void, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type, NULL, NULL_LOC, NULL_LOC));
            unwrap(NecroType, necro_uniqueness_propagate(infer->arena, &infer->con_env, infer->base, infer->intern, type, NULL, NULL, true, source_loc, end_loc, NECRO_CONSTRAINT_UCOERCE));
            type = unwrap_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, type, NULL));
            unwrap(void, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type, NULL, NULL_LOC, NULL_LOC));
            return type;
        }
        subs = subs->next;
    }
    // TODO: method not part of class error?!
    assert(false && "TODO: method not part of class error");
    return NULL;
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
    // printf("context: (");
    // necro_print_type_class_context(type_class->context, white_count + 4);
    // printf(") =>\n");
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
    // printf("context: (");
    // necro_print_type_class_context(instance->context, white_count + 4);
    // printf(") =>\n");
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
    // type_class->context         = NULL;
    type_class->dependency_flag = 0;
    type_class->ast             = type_class_ast;
    data->type_class            = type_class;

    //------------------------------------
    // Create type_var for type_class
    NecroType* type_class_var                   = necro_type_var_create(infer->arena, type_class_ast->type_class_declaration.tyvar->variable.ast_symbol, NULL);
    type_class_var->var.is_rigid                = true;
    type_class_var->ownership                   = NULL;
    type_class->type                            = necro_type_con1_create(infer->arena, type_class->type_class_name, necro_type_list_create(infer->arena, type_class_var, NULL));
    data->type                                  = type_class_var;
    type_class->super_classes                   = necro_try_map_result(NecroConstraintList, NecroType, necro_constraint_list_from_ast(infer, type_class_ast->type_class_declaration.context));
    type_class_var->var.var_symbol->constraints = necro_constraint_append_class_constraint(infer->arena, type_class, type_class_var, type_class_ast->source_loc, type_class_ast->end_loc, NULL);

    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type_class_var, type_class_ast->source_loc, type_class_ast->end_loc));

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
                NecroType* type_sig       = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, method_ast->type_signature.type, NECRO_TYPE_ATTRIBUTE_NONE));
                type_class_var->ownership = NULL;

                //---------------------------------
                // Infer and check method type sig
                // NOTE: TypeClass's context should ALWAYS BE FIRST!
                NecroConstraintList* method_cons = necro_try_map_result(NecroConstraintList, NecroType, necro_constraint_list_from_ast(infer, method_ast->type_signature.context));

                necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type_sig, method_ast->source_loc, method_ast->end_loc));
                necro_try(NecroType, necro_kind_unify(type_sig->kind, infer->base->star_kind->type, NULL));
                necro_try(NecroType, necro_uniqueness_propagate(infer->arena, &infer->con_env, infer->base, infer->intern, type_sig, method_ast->scope, NULL, true, method_ast->source_loc, method_ast->end_loc, NECRO_CONSTRAINT_UCOERCE));

                type_sig->pre_supplied = true;
                type_sig               = necro_try_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, type_sig, NULL));
                necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type_sig, method_ast->source_loc, method_ast->end_loc));
                necro_try(NecroType, necro_kind_unify(type_sig->kind, infer->base->star_kind->type, NULL));
                method_ast->type_signature.var->variable.ast_symbol->type              = type_sig;
                method_ast->type_signature.var->variable.ast_symbol->method_type_class = type_class;

                // printf("%s :: ", method_ast->type_signature.var->variable.ast_symbol->source_name->str);
                // necro_type_print(type_sig);
                // puts("");

                // Add Class constraint
                necro_try(NecroType, necro_constraint_class_variable_check(type_class, type_class->type_var, method_ast->type_signature.var->variable.ast_symbol, method_cons));
                NecroType*           for_alls = type_sig;
                NecroConstraintList* cons     = NULL;
                while (for_alls->type == NECRO_TYPE_FOR)
                {
                    if (for_alls->for_all.var_symbol->source_name == type_class_var->var.var_symbol->source_name)
                    {
                        for_alls->for_all.var_symbol->is_class_head_var = true;
                        cons                                            = necro_constraint_append_class_constraint(infer->arena, type_class, for_alls->for_all.var_symbol->type, type_class_ast->source_loc, type_class_ast->end_loc, method_cons);
                        assert(cons->data->cls.type_class == type_class);
                        break;
                    }
                    for_alls = for_alls->for_all.type;
                }
                if (cons == NULL)
                    return necro_type_ambiguous_class_error(type_class->type_class_name, type_sig, method_ast->source_loc, method_ast->end_loc);
                // Apply constraints
                necro_try(NecroType, necro_constraint_list_kinds_check(infer->arena, infer->base, cons, method_ast->scope));
                necro_try(NecroType, necro_constraint_ambiguous_type_class_check(method_ast->type_signature.var->variable.ast_symbol, cons, type_sig));
                necro_constraint_list_apply(infer->arena, type_sig, cons);

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
        return necro_error_map(NecroConstraintList, NecroType, necro_type_not_a_class_error(type_class_name, type_class_name->type, ast->type_class_instance.qtycls->source_loc, ast->type_class_instance.qtycls->end_loc));
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
    // instance->context                      = necro_try_map_result(NecroTypeClassContext, NecroType, necro_ast_to_context(infer, ast->type_class_instance.context));
    instance->dictionary_prototype         = NULL;
    instance->ast                          = ast;
    instance->data_type                    = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, instance->ast->type_class_instance.inst, NECRO_TYPE_ATTRIBUTE_NONE));
    // instance->super_instances              = NULL;
    data->type_class_instance              = instance;

    // Add to instance list
    data_type_name->instance_list          = necro_cons_instance_list(infer->arena, instance, data_type_name->instance_list);

    NecroType* class_var                   = type_class->type_var->type;
    assert(class_var != NULL);

    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, instance->data_type, instance->ast->source_loc, instance->ast->end_loc));
    necro_try(NecroType, necro_kind_unify_with_info(class_var->kind, instance->data_type->kind, NULL, ast->type_class_instance.inst->source_loc, ast->type_class_instance.inst->end_loc));

    // Apply constraints
    // necro_apply_constraints(infer->arena, instance->data_type, instance->context);
    instance->data_type              = necro_try_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, instance->data_type, NULL));
    NecroConstraintList* constraints = necro_try_map_result(NecroConstraintList, NecroType, necro_constraint_list_from_ast(infer, ast->type_class_instance.context));
    necro_constraint_list_apply(infer->arena, instance->data_type, constraints);

    //---------------------------------
    // Super classes check
    // NecroTypeClassContext* super_classes = type_class->context;
    NecroConstraintList* super_classes = type_class->super_classes;
    while (super_classes != NULL)
    {
        NecroAstSymbol*         super_class_symbol = super_classes->data->cls.type_class->type_class_name;
        NecroTypeClassInstance* super_instance     = necro_get_type_class_instance(instance->data_type_name, super_class_symbol);
        if (super_instance == NULL)
        {
            return necro_type_does_not_implement_super_class_error(super_class_symbol, instance->data_type, type_class->type, NULL, NULL, ast->type_class_instance.inst->source_loc, ast->type_class_instance.inst->end_loc);
        }
        // assert(super_instance != NULL);
        // instance->super_instances = necro_cons_instance_list(infer->arena, super_instance, instance->super_instances);
        super_classes = super_classes->next;
    }

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
                // if (strcmp("accumulate1", instance->dictionary_prototype->type_class_member_ast_symbol->source_name->str) == 0 && strcmp("Mono", instance->data_type_name->source_name->str) == 0)
                // {
                //     printf("found it!\n");
                // }
                NecroType* method_type                                                         = instance->dictionary_prototype->type_class_member_ast_symbol->type;
                NecroType* inst_method_type                                                    = necro_instantiate_method_sig(infer, method_type, instance->data_type, method_ast->source_loc, method_ast->end_loc);
                instance->dictionary_prototype->instance_member_ast_symbol->type               = inst_method_type;
                instance->dictionary_prototype->instance_member_ast_symbol->type->pre_supplied = true;

                // printf("%s :: ", instance->dictionary_prototype->instance_member_ast_symbol->source_name->str);
                // necro_type_print(inst_method_type);
                // puts("");

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

    return ok(NecroType, NULL);
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


///////////////////////////////////////////////////////
// New Constraints based system
///////////////////////////////////////////////////////
NecroConstraintList* necro_constraint_list_union(NecroPagedArena* arena, NecroConstraintList* constraints1, NecroConstraintList* constraints2)
{
    if (constraints1 == NULL)
        return constraints2;
    if (constraints2 == NULL)
        return constraints1;
    NecroConstraintList* new_constraints = NULL;
    while (constraints1 != NULL)
    {
        bool                 is_new_constraint    = true;
        NecroConstraintList* curr_new_constraints = new_constraints;
        while (curr_new_constraints != NULL)
        {
            if (necro_constraint_is_equal(constraints1->data, curr_new_constraints->data))
            {
                is_new_constraint = false;
                break;
            }
            curr_new_constraints = curr_new_constraints->next;
        }
        if (is_new_constraint)
            new_constraints = necro_cons_constraint_list(arena, constraints1->data, new_constraints);
        constraints1 = constraints1->next;
    }
    while (constraints2 != NULL)
    {
        bool                 is_new_constraint    = true;
        NecroConstraintList* curr_new_constraints = new_constraints;
        while (curr_new_constraints != NULL)
        {
            if (necro_constraint_is_equal(constraints2->data, curr_new_constraints->data))
            {
                is_new_constraint = false;
                break;
            }
            curr_new_constraints = curr_new_constraints->next;
        }
        if (is_new_constraint)
            new_constraints = necro_cons_constraint_list(arena, constraints2->data, new_constraints);
        constraints2 = constraints2->next;
    }
    return new_constraints;
}

NecroResult(NecroConstraintList) necro_constraint_list_from_ast(NecroInfer* infer, NecroAst* constraints_ast)
{
    if (constraints_ast == NULL)
    {
        return ok(NecroConstraintList, NULL);
    }
    else if (constraints_ast->type == NECRO_AST_LIST_NODE)
    {
        NecroConstraintList* cons = NULL;
        while (constraints_ast != NULL)
        {
            NecroAst*       constraint_ast   = constraints_ast->list.item;
            assert(constraint_ast->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroAstSymbol* class_symbol     = constraint_ast->type_class_context.conid->conid.ast_symbol;
            NecroType*      constrained_type = necro_try_map_result(NecroType, NecroConstraintList, necro_ast_to_type_sig_go(infer, constraint_ast->type_class_context.varid, NECRO_TYPE_ATTRIBUTE_VOID));
            if (class_symbol->type_class == NULL)
            {
                return necro_type_not_a_class_error(class_symbol, class_symbol->type, constraint_ast->source_loc, constraint_ast->end_loc);
            }
            cons = necro_constraint_append_class_constraint(infer->arena, class_symbol->type_class, constrained_type, constraints_ast->source_loc, constraints_ast->end_loc, cons);
            if (constrained_type->type != NECRO_TYPE_VAR && constrained_type->type != NECRO_TYPE_APP)
            {
                return necro_type_malformed_constraint(cons->data);
            }
            constraints_ast = constraints_ast->list.next_item;
        }
        cons = necro_constraint_list_union(infer->arena, cons, NULL);
        return ok(NecroConstraintList, cons);
    }
    else if (constraints_ast->type == NECRO_AST_TYPE_CLASS_CONTEXT)
    {
        NecroAstSymbol* class_symbol     = constraints_ast->type_class_context.conid->conid.ast_symbol;
        NecroType*      constrained_type = necro_try_map_result(NecroType, NecroConstraintList, necro_ast_to_type_sig_go(infer, constraints_ast->type_class_context.varid, NECRO_TYPE_ATTRIBUTE_VOID));
        if (class_symbol->type_class == NULL)
        {
            return necro_type_not_a_class_error(class_symbol, class_symbol->type, constraints_ast->source_loc, constraints_ast->end_loc);
        }
        NecroConstraintList* cons = necro_constraint_append_class_constraint(infer->arena, class_symbol->type_class, constrained_type, constraints_ast->source_loc, constraints_ast->end_loc, NULL);
        if (constrained_type->type != NECRO_TYPE_VAR && constrained_type->type != NECRO_TYPE_APP)
        {
            return necro_type_malformed_constraint(cons->data);
        }
        return ok(NecroConstraintList, cons);
    }
    else
    {
        necro_unreachable(NecroConstraintList);
    }
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
    case NECRO_TYPE_FOR:
        if (type->for_all.var_symbol == name)
            return true;
        else
            return necro_ambig_occurs(name, type->for_all.type);
    default:              assert(false); break;
    }
    return false;
}

bool necro_type_ambig_occurs_over_type_sig(NecroType* type, NecroType* type_sig)
{
    if (type == NULL)
        return false;
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return necro_ambig_occurs(type->var.var_symbol, type_sig);
    case NECRO_TYPE_APP:  return necro_type_ambig_occurs_over_type_sig(type->app.type1, type_sig) || necro_type_ambig_occurs_over_type_sig(type->app.type2, type_sig);
    case NECRO_TYPE_FUN:  return necro_type_ambig_occurs_over_type_sig(type->fun.type1, type_sig) || necro_type_ambig_occurs_over_type_sig(type->fun.type2, type_sig);
    case NECRO_TYPE_CON:  return necro_type_ambig_occurs_over_type_sig(type->con.args,  type_sig);
    case NECRO_TYPE_LIST: return necro_type_ambig_occurs_over_type_sig(type->list.item, type_sig) || necro_type_ambig_occurs_over_type_sig(type->list.next, type_sig);
    case NECRO_TYPE_FOR:  assert(false); break;
    default:              assert(false); break;
    }
    return false;
}

NecroResult(NecroType) necro_constraint_ambiguous_type_class_check(NecroAstSymbol* type_sig_name, NecroConstraintList* constraints, NecroType* type_sig)
{
    if (type_sig == NULL)
        return ok(NecroType, NULL);
    while (constraints != NULL)
    {
        if (constraints->data->type != NECRO_CONSTRAINT_CLASS)
        {
            constraints = constraints->next;
            continue;
        }
        NecroAstSymbol* class_symbol     = constraints->data->cls.type_class->type_class_name;
        NecroType*      constrained_type = constraints->data->cls.type1;
        if (!necro_type_ambig_occurs_over_type_sig(constrained_type, type_sig))
            return necro_type_ambiguous_class_error(class_symbol, constrained_type, type_sig_name->ast->source_loc, type_sig_name->ast->end_loc);
        constraints = constraints->next;
    }
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_constraint_class_variable_check(NecroTypeClass* type_class, NecroAstSymbol* type_var, NecroAstSymbol* type_sig_symbol, NecroConstraintList* constraints)
{
    while (constraints != NULL)
    {
        NecroType* constraint_type_var = necro_type_find(constraints->data->cls.type1);
        // while (constraint_type_var->type == NECRO_TYPE_APP)
        //     constraint_type_var = necro_type_find(constraint_type_var->app.type1);
        // assert(constraint_type_var->type == NECRO_TYPE_VAR);
        if (constraint_type_var->type == NECRO_TYPE_VAR &&
            constraints->data->type == NECRO_CONSTRAINT_CLASS &&
            constraints->data->cls.type_class == type_class &&
            constraint_type_var->var.var_symbol->source_name == type_var->source_name)
            return necro_type_constrains_only_class_var_error(constraints->data->cls.type_class->type_class_name, type_sig_symbol->ast->source_loc, type_sig_symbol->ast->end_loc, type_var, NULL_LOC, NULL_LOC);
        constraints = constraints->next;
    }
    return ok(NecroType, NULL);
}

void necro_constraint_list_apply(NecroPagedArena* arena, NecroType* type, NecroConstraintList* constraints)
{
    if (type->type != NECRO_TYPE_FOR || constraints == NULL)
        return;
    while (constraints != NULL)
    {
        NecroType* constrained_type = NULL;
        switch (constraints->data->type)
        {
        case NECRO_CONSTRAINT_EQUAL:       constrained_type = constraints->data->equal.type1;    break;
        case NECRO_CONSTRAINT_UCONSTRAINT: constrained_type = constraints->data->uconstraint.u1; break;
        case NECRO_CONSTRAINT_CLASS:       constrained_type = constraints->data->cls.type1;      break;
        default: assert(false); break;
        }
        while (constrained_type->type == NECRO_TYPE_APP)
            constrained_type = constrained_type->app.type1;
        if (constrained_type->type == NECRO_TYPE_VAR)
        {
            const bool is_type_class_var = constraints->data->type == NECRO_CONSTRAINT_CLASS && constraints->data->cls.type1->var.var_symbol == constraints->data->cls.type_class->type_var;
            assert(!is_type_class_var);
            constrained_type->var.var_symbol->constraints = necro_cons_constraint_list(arena, constraints->data, constrained_type->var.var_symbol->constraints);
        }
        constraints = constraints->next;
    }
}

NecroResult(NecroType) necro_constraint_list_kinds_check(NecroPagedArena* arena, NecroBase* base, NecroConstraintList* constraints, NecroScope* scope)
{
    while (constraints != NULL)
    {
        if (constraints->data->type == NECRO_CONSTRAINT_CLASS)
        {
            NecroTypeClass* type_class       = constraints->data->cls.type_class;
            NecroType*      type_class_var   = type_class->type_var->type;
            NecroType*      constrained_type = constraints->data->cls.type1;
            if (constrained_type->kind == NULL)
                constrained_type->kind = unwrap_result(NecroType, necro_kind_infer(arena, base, constrained_type, zero_loc, zero_loc));
            necro_try(NecroType, necro_kind_unify_with_info(type_class_var->kind, constrained_type->kind, scope, constraints->data->source_loc, constraints->data->end_loc));
        }
        constraints = constraints->next;
    }
    return ok(NecroType, NULL);
}

bool necro_constraint_find_with_super_classes(NecroConstraintList* constraints_to_check, NecroType* original_object_type, NecroConstraint* constraint_to_find)
{
    while (constraints_to_check != NULL)
    {
        if (constraints_to_check->data->type == NECRO_CONSTRAINT_CLASS)
        {
            if (original_object_type == NULL)
            {
                if (necro_constraint_is_equal(constraints_to_check->data, constraint_to_find))
                    return true;
                if (necro_constraint_find_with_super_classes(constraints_to_check->data->cls.type_class->super_classes, constraints_to_check->data->cls.type1, constraint_to_find))
                    return true;
            }
            else
            {
                NecroConstraint constraint_to_check = *constraints_to_check->data;
                constraint_to_check.cls.type1       = original_object_type;
                if (necro_constraint_is_equal(&constraint_to_check, constraint_to_find))
                    return true;
                if (necro_constraint_find_with_super_classes(constraints_to_check->data->cls.type_class->super_classes, original_object_type, constraint_to_find))
                    return true;
            }
        }
        constraints_to_check = constraints_to_check->next;
    }
    return false;
}

NecroResult(void) necro_constraint_simplify_class_constraint(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroConstraint* constraint)
{
    assert(constraint != NULL);
    assert(constraint->type == NECRO_CONSTRAINT_CLASS);
    NecroTypeClass* type_class = constraint->cls.type_class;
    NecroType*      type       = necro_type_find(constraint->cls.type1);
    necro_try_map(NecroType, void, necro_kind_unify_with_info(type_class->type_var->type->kind, type->kind, NULL, constraint->source_loc, constraint->end_loc));
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        const bool class_found = necro_constraint_find_with_super_classes(type->var.var_symbol->constraints, NULL, constraint);
        if (!class_found)
        {
            if (type->var.is_rigid)
            {
                necro_try_map(NecroType, void, necro_type_not_an_instance_of_error(type_class->type_class_name, type, NULL, type, NULL, constraint->source_loc, constraint->end_loc));
            }
            else
            {
                type->var.var_symbol->constraints = necro_cons_constraint_list(arena, constraint, type->var.var_symbol->constraints);
            }
        }
        return ok_void();
    }

    case NECRO_TYPE_APP:
    {
        // Check to see if var is rigid or not
        NecroType* app_var = type;
        while (app_var->type == NECRO_TYPE_APP)
            app_var = necro_type_find(app_var->app.type1);
        if (app_var->type == NECRO_TYPE_CON)
        {
            *type = *necro_type_uncurry_app(arena, base, type);
            return necro_constraint_simplify_class_constraint(arena, con_env, base, constraint);
        }
        assert(app_var->type == NECRO_TYPE_VAR);
        const bool class_found = necro_constraint_find_with_super_classes(app_var->var.var_symbol->constraints, NULL, constraint);
        if (!class_found)
        {
            if (app_var->var.is_rigid)
            {
                return necro_error_map(NecroType, void, necro_type_not_an_instance_of_error(type_class->type_class_name, app_var, NULL, type, NULL, constraint->source_loc, constraint->end_loc));
            }
            else
            {
                app_var->var.var_symbol->constraints = necro_cons_constraint_list(arena, constraint, app_var->var.var_symbol->constraints);
            }
        }
        return ok_void();
    }

    case NECRO_TYPE_CON:
    {
        NecroTypeClassInstance* instance = necro_get_type_class_instance(type->con.con_symbol, type_class->type_class_name);
        if (instance == NULL)
        {
            return necro_error_map(NecroType, void, necro_type_not_an_instance_of_error(type_class->type_class_name, type, NULL, type, NULL, constraint->source_loc, constraint->end_loc));
        }
        NecroType* instance_data_inst = necro_type_instantiate(arena, con_env, base, instance->data_type, instance->ast->scope, constraint->source_loc, constraint->end_loc);
        necro_try_map(NecroType, void, necro_type_unify_with_full_info(arena, con_env, base, instance_data_inst, type, NULL, constraint->source_loc, constraint->end_loc, instance_data_inst, type));
        return ok_void();
    }

    case NECRO_TYPE_FUN:
    {
        // TODO: Type classes for functions!!!
        // necro_try(NecroType, necro_propagate_type_classes(arena, con_env, base, classes, type->fun.type1, scope));
        // return necro_propagate_type_classes(arena, con_env, base, classes, type->fun.type2, scope);
        assert(false && "Compiler bug: (->) not implemented in necro_constraint_simplify_class_constraint! (i.e. constraints of the form: Num (a -> b), are not currently supported)");
        return ok_void();
    }

    case NECRO_TYPE_NAT:
        assert(false && "Compiler bug: Types of kind Nat are not implemented in necro_constraint_simplify_class_constraint! (i.e. constraints of the form: Num (a -> b), are not currently supported)");
        return ok_void();

    case NECRO_TYPE_SYM:
        assert(false && "Compiler bug: Types of kind Sym are not implemented in necro_constraint_simplify_class_constraint! (i.e. constraints of the form: Num (a -> b), are not currently supported)");
        return ok_void();

    case NECRO_TYPE_LIST:
    case NECRO_TYPE_FOR:
    default:
        necro_unreachable(void);
    }
}

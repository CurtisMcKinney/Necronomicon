/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"
#include "symtable.h"
#include "kind.h"
#include "type_class.h"

// TODO:
//    * Printing variable names gets wonky when retaining printed names across methods and instances of type class, look at Either instance of Functor for example
//    * Better unification error messaging! This is especially true when it occurs during SuperClass constraint checking. Make necro_unify return struct with either Success or Error data
//    * AST transformation creating and passing dictionaries around for type classes

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
bool                   necro_constrain_class_variable_check(NecroInfer* infer, NecroCon type_class_name, NecroCon type_var, NecroSymbol type_sig_symbol, NecroTypeClassEnv* env, NecroTypeClassContext* context);
NecroType*             necro_instantiate_method_sig(NecroInfer* infer, NecroCon type_class_var, NecroType* method_type, NecroType* data_type);
void                   necro_finish_declaring_type_class(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class);

//=====================================================
// NecroTypeClassEnv
//=====================================================
NecroTypeClassEnv necro_create_type_class_env()
{
    return (NecroTypeClassEnv)
    {
        .class_table             = necro_create_type_class_table(),
        .instance_table          = necro_create_type_class_instance_table(),
        .arena                   = necro_create_paged_arena(),
    };
}

void necro_destroy_type_class_env(NecroTypeClassEnv* env)
{
    necro_destroy_type_class_table(&env->class_table);
    necro_destroy_type_class_instance_table(&env->instance_table);
    necro_destroy_paged_arena(&env->arena);
}

//=====================================================
// Type Class Declaration - Refactor
//=====================================================
NecroTypeClass* necro_get_super_class(NecroInfer* infer, NecroTypeClassEnv* env, NecroCon type_class_name, NecroCon sub_class_name)
{
    assert(infer != NULL);
    assert(env != NULL);
    if (necro_is_infer_error(infer)) return false;
    NecroTypeClass* super_class = necro_type_class_table_get(&env->class_table, type_class_name.id.id);
    if (super_class == NULL)
    {
        necro_infer_ast_error(infer, NULL, NULL, "%s is not a constraint", necro_intern_get_string(infer->intern, type_class_name.symbol));
        return NULL;
    }
    else if (super_class->dependency_flag == 1)
    {
        necro_infer_ast_error(infer, NULL, NULL, "Superclass cycle for classes \'%s\' and \'%s\'", necro_intern_get_string(infer->intern, sub_class_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
        return NULL;
    }
    return super_class;
}

void necro_dependency_analyze_type_class_declarations(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class);

void necro_declare_type_classes(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
{
    assert(infer != NULL);
    assert(top_level_declarations != NULL);
    assert(top_level_declarations->type == NECRO_AST_TOP_DECL);
    if (necro_is_infer_error(infer)) return;

    //---------------------------------------------------------------
    // Pass 1, creating initial struct and insert into tables
    NecroNode* current_decl = top_level_declarations;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_DECLARATION)
        {
            // Create type class
            NecroNode*      type_class_ast = current_decl->top_declaration.declaration;
            NecroTypeClass* type_class     = necro_type_class_table_insert(&env->class_table, type_class_ast->type_class_declaration.tycls->conid.id.id, NULL);
            type_class->type_class_name    = (NecroCon) { .symbol = type_class_ast->type_class_declaration.tycls->conid.symbol, .id = type_class_ast->type_class_declaration.tycls->conid.id };
            type_class->members            = NULL;
            type_class->type_var           = (NecroCon) { .symbol = type_class_ast->type_class_declaration.tyvar->variable.symbol, .id = type_class_ast->type_class_declaration.tyvar->variable.id };
            type_class->context            = NULL;
            type_class->dependency_flag    = 0;
            type_class->ast                = type_class_ast;

            // Create type_var for type_class
            NecroType* ty_var              = necro_create_type_var(infer, (NecroVar) { .id = type_class->type_var.id, .symbol = type_class_ast->type_class_declaration.tyvar->variable.symbol});
            // ty_var->var.is_rigid           = false;
            ty_var->var.is_rigid           = true;
            ty_var->var.is_type_class_var  = true;
            type_class->type               = necro_make_con_1(infer, type_class->type_class_name, necro_create_type_list(infer, ty_var, NULL));
            type_class->type->con.is_class = true;
            type_class->context            = necro_ast_to_context(infer, env, type_class_ast->type_class_declaration.context);
            necro_symtable_get(infer->symtable, type_class->type_var.id)->type = ty_var;
        }
        if (necro_is_infer_error(infer)) return;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return;

    //---------------------------------------------------------------
    // Pass 2, Dependency Analyze and Finish
    current_decl = top_level_declarations;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_DECLARATION)
        {
            // Retrieve type class
            NecroNode*      type_class_ast = current_decl->top_declaration.declaration;
            NecroTypeClass* type_class     = necro_type_class_table_get(&env->class_table, type_class_ast->type_class_declaration.tycls->conid.id.id);
            necro_dependency_analyze_type_class_declarations(infer, env, type_class);
        }
        if (necro_is_infer_error(infer)) return;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return;
}

void necro_dependency_analyze_type_class_declarations(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class)
{
    assert(infer != NULL);
    assert(env != NULL);
    assert(type_class != NULL);
    if (necro_is_infer_error(infer)) return;
    if (type_class->dependency_flag == 2) return;
    assert(type_class->dependency_flag != 1);

    // Set Flag to (adding super classes)
    type_class->dependency_flag = 1;

    // Iterate through super classes
    NecroTypeClassContext* context = type_class->context;
    while (context != NULL)
    {
        NecroTypeClass* super_class = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
        if (necro_is_infer_error(infer)) return;
        if (super_class->dependency_flag == 2)
        {
            context = context->next;
            continue;
        }
        if (necro_is_infer_error(infer)) return;
        necro_dependency_analyze_type_class_declarations(infer, env, super_class);
        if (necro_is_infer_error(infer)) return;
        context = context->next;
    }

    necro_finish_declaring_type_class(infer, env, type_class);
    type_class->dependency_flag = 2;
}

void necro_finish_declaring_type_class(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class)
{
    assert(infer != NULL);
    assert(env != NULL);
    assert(type_class != NULL);
    if (necro_is_infer_error(infer)) return;

    NecroType* type_class_var   = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
    type_class_var->var.context = necro_create_type_class_context(&infer->arena, type_class->type_class_name, type_class->type_var, type_class->context);
    necro_kind_infer(infer, type_class_var, type_class_var, "While declaring a type class: ");

    // //--------------------------------
    // // Constrain type class variable via super classes
    // NecroTypeClassContext* context = type_class->context;
    // while (context != NULL)
    // {
    //     NecroTypeClass* super_class = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
    //     if (necro_is_infer_error(infer)) return;
    //     NecroType*      super_class_var = necro_gen(infer, necro_symtable_get(infer->symtable, super_class->type_var.id)->type, NULL);
    //     NecroType*      inst_super_var  = necro_inst(infer, super_class_var, NULL);
    //     type_class_var->var.context = necro_union_contexts(infer, super_class->context, type_class_var->var.context);
    //     necro_unify(infer, type_class_var, inst_super_var, NULL, type_class_var, "While declaring a type class: ");
    //     if (necro_is_infer_error(infer)) return;
    //     context = context->next;
    // }

    NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, type_class->type_class_name, type_class->type_var, NULL);

    //--------------------------------
    // Build Member Type Signatures
    NecroNode* declarations = type_class->ast->type_class_declaration.declarations;
    while (declarations != NULL)
    {
        if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
            continue;

        NecroType* type_sig = necro_ast_to_type_sig_go(infer, declarations->declaration.declaration_impl->type_signature.type);

        if (necro_is_infer_error(infer)) return;
        NecroTypeClassContext* method_context = necro_ast_to_context(infer, env, declarations->declaration.declaration_impl->type_signature.context);
        if (necro_constrain_class_variable_check(infer, type_class->type_class_name, type_class->type_var, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, env, method_context)) return;
        NecroTypeClassContext* context        = necro_union_contexts(infer, method_context, class_context);
        if (necro_ambiguous_type_class_check(infer, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, context, type_sig)) return;
        necro_apply_constraints(infer, type_sig, context);
        type_sig->pre_supplied = true;
        type_sig->source_loc   = declarations->declaration.declaration_impl->source_loc;
        type_sig               = necro_gen(infer, type_sig, NULL);
        // necro_infer_kind(infer, type_sig, infer->star_kind, type_sig, "While declaring a method of a type class: ");
        necro_kind_infer(infer, type_sig, type_sig, "While declaring a method of a type class: ");
        necro_kind_gen(infer, type_sig->type_kind);
        necro_kind_unify(infer, type_sig->type_kind, infer->star_type_kind, NULL, type_sig, "While declaring a method of a type class");
        if (necro_is_infer_error(infer)) return;
        necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type      = type_sig;
        necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->is_method = true;
        NecroTypeClassMember* new_member  = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
        // type_class->members               = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
        new_member->member_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->type_signature.var->variable.symbol, .id = declarations->declaration.declaration_impl->type_signature.var->variable.id };
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
        // prev_member->next                 =
        // type_class->members->next         = prev_member;
        declarations = declarations->declaration.next_declaration;
    }
    TRACE_TYPE_CLASS("Finishing type_class: %s\n", necro_intern_get_string(infer->intern, type_class->type_class_name.symbol));

    NecroDeclarationGroup* dictionary_declaration_group = NULL;
    NecroASTNode*          dictionary_declarations      = type_class->ast->type_class_declaration.dictionary_data_declaration;
    while (dictionary_declarations != NULL)
    {
        dictionary_declaration_group = necro_append_declaration_group(&infer->arena, dictionary_declarations->top_declaration.declaration, dictionary_declaration_group);
        dictionary_declarations = dictionary_declarations->top_declaration.next_top_decl;
    }
    necro_infer_assignment(infer, dictionary_declaration_group);
    if (necro_is_infer_error(infer)) return;

    // Generalize type class kind
    type_class_var->type_kind = necro_kind_gen(infer, type_class_var->type_kind);
}

//=====================================================
// Type Class Instances - Refactor
//=====================================================
void necro_dependency_analyze_type_class_instances(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClassInstance* instance);
void necro_finish_declaring_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class, NecroTypeClassInstance* instance);
void necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context);

NecroTypeClassInstance* necro_get_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroCon type_class_name, NecroCon data_type_name)
{
    assert(infer != NULL);
    assert(env != NULL);
    if (necro_is_infer_error(infer)) return false;
    assert(necro_symtable_get(infer->symtable, data_type_name.id)->type != NULL);
    uint64_t                key      = necro_create_instance_key(data_type_name, type_class_name);
    NecroTypeClassInstance* instance = necro_type_class_instance_table_get(&env->instance_table, key);
    if (instance == NULL)
    {
        necro_infer_ast_error(infer, NULL, NULL, "%s is not an instance of %s", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
        return NULL;
    }
    return instance;
}

void necro_type_class_instances_pass1(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
{
    assert(infer != NULL);
    assert(top_level_declarations != NULL);
    assert(top_level_declarations->type == NECRO_AST_TOP_DECL);
    if (necro_is_infer_error(infer)) return;

    //---------------------------------------------------------------
    // Pass 1, creating initial struct and insert into tables
    NecroNode* current_decl = top_level_declarations;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_INSTANCE)
        {
            NecroNode* ast             = current_decl->top_declaration.declaration;
            NecroCon   type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
            NecroCon   data_type_name;
            if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
                data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
            else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
                data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
            else
                assert(false);

            if (necro_symtable_get(infer->symtable, data_type_name.id)->type == NULL)
            {
                necro_infer_ast_error(infer, NULL, ast, "No type named \'%s\', in instance declaration for class \'%s\'", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
                return;
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
            instance->context                = necro_ast_to_context(infer, env, ast->type_class_instance.context);
            instance->dictionary_prototype   = NULL;
            instance->ast                    = ast;
            instance->dependency_flag        = 0;
            instance->data_type              = NULL;
        }
        if (necro_is_infer_error(infer)) return;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
}

void necro_type_class_instances_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
{
    if (necro_is_infer_error(infer)) return;
    //---------------------------------------------------------------
    // Pass 2, Dependency Analyze and Finish
    NecroNode* current_decl = top_level_declarations;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_INSTANCE)
        {
            // Retrieve type class instance
            NecroNode* ast             = current_decl->top_declaration.declaration;
            NecroCon   type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
            NecroCon   data_type_name;
            if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
                data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
            else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
                data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
            else
                assert(false);
            NecroTypeClassInstance* instance = necro_get_type_class_instance(infer, env, type_class_name, data_type_name);
            if (necro_is_infer_error(infer)) return;
            necro_dependency_analyze_type_class_instances(infer, env, instance);
        }
        if (necro_is_infer_error(infer)) return;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return;
}

void necro_dependency_analyze_type_class_instances(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClassInstance* instance)
{
    assert(infer != NULL);
    assert(env != NULL);
    assert(instance != NULL);
    if (necro_is_infer_error(infer)) return;
    if (instance->dependency_flag == 2) return;
    assert(instance->dependency_flag != 1);

    // Set Flag to (adding super classes)
    instance->dependency_flag = 1;

    NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, instance->type_class_name.id.id);
    if (type_class == NULL)
    {
        necro_infer_ast_error(infer, NULL, instance->ast, "%s cannot be an instance of %s, because %s is not a type class",
            necro_intern_get_string(infer->intern, instance->data_type_name.symbol),
            necro_intern_get_string(infer->intern, instance->type_class_name.symbol),
            necro_intern_get_string(infer->intern, instance->type_class_name.symbol));
        return;
    }

    // Iterate through super classes
    NecroTypeClassContext* context = type_class->context;
    while (context != NULL)
    {
        NecroTypeClass*          super_class    = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
        if (necro_is_infer_error(infer)) return;
        NecroTypeClassInstance*  super_instance = necro_get_type_class_instance(infer, env, super_class->type_class_name, instance->data_type_name);
        if (necro_is_infer_error(infer)) return;
        if (super_instance->dependency_flag == 2)
        {
            context = context->next;
            continue;
        }
        if (necro_is_infer_error(infer)) return;
        necro_dependency_analyze_type_class_instances(infer, env, super_instance);
        if (necro_is_infer_error(infer)) return;
        context = context->next;
    }

    necro_finish_declaring_type_class_instance(infer, env, type_class, instance);
    instance->dependency_flag = 2;
}

void necro_finish_declaring_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class, NecroTypeClassInstance* instance)
{
    assert(infer != NULL);
    assert(env != NULL);
    assert(instance != NULL);
    if (necro_is_infer_error(infer)) return;
    TRACE_TYPE_CLASS("INSTANCE: %s %s\n", necro_intern_get_string(infer->intern, instance->type_class_name.symbol), necro_intern_get_string(infer->intern, instance->data_type_name.symbol));

    instance->data_type = necro_ast_to_type_sig_go(infer, instance->ast->type_class_instance.inst);

    if (necro_is_infer_error(infer)) return;
    NecroType* class_var  = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
    assert(class_var != NULL);
    // necro_infer_kind(infer, instance->data_type, class_var->kind, instance->data_type, "While creating an instance of a type class: ");
    necro_kind_infer(infer, instance->data_type, instance->data_type, "While creating an instance of a type class");
    necro_kind_unify(infer, instance->data_type->type_kind, class_var->type_kind, NULL, instance->data_type, "While creating an instance of a type class");
    if (necro_is_infer_error(infer)) return;

    // Apply constraints
    necro_apply_constraints(infer, instance->data_type, instance->context);
    instance->data_type = necro_gen(infer, instance->data_type, NULL);

    // NecroType*             type_var = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
    // NecroTypeClassContext* context  = type_var->var.context;
    // while (context != NULL)
    // {
    //     // NecroTypeClass*          super_class    = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
    //     // if (necro_is_infer_error(infer)) return;
    //     // NecroTypeClassInstance*  super_instance = necro_get_type_class_instance(infer, env, super_class->type_class_name, instance->data_type_name);
    //     // if (necro_is_infer_error(infer)) return;

    //     // NecroType* super_type = necro_inst(infer, super_instance->data_type, NULL);
    //     // necro_unify(infer, instance->data_type, super_type, NULL, instance->data_type, "While creating an instance of a type class: ");

    //     // if (necro_is_infer_error(infer)) return;
    //     context = context->next;
    // }

    NecroType* inst_data_type = necro_inst(infer, instance->data_type, NULL);
    // necro_infer_kind(infer, inst_data_type, NULL, inst_data_type, "While creating an instance method: ");
    // necro_kind_infer(infer, inst_data_type, inst_data_type, "While creating an instance method: ");

    //--------------------------------
    // Dictionary Prototype
    NecroNode* declarations = instance->ast->type_class_instance.declarations;
    while (declarations != NULL)
    {
        assert(declarations->type == NECRO_AST_DECL);
        assert(declarations->declaration.declaration_impl != NULL);
        if (necro_is_infer_error(infer)) return;

        //--------------------------------
        // Create dictionary prototype entry
        NecroDictionaryPrototype* prev_dictionary = instance->dictionary_prototype;
        instance->dictionary_prototype            = necro_paged_arena_alloc(&env->arena, sizeof(NecroDictionaryPrototype));
        if (declarations->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT)
            instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->simple_assignment.variable_name, .id = declarations->declaration.declaration_impl->simple_assignment.id };
        else if (declarations->declaration.declaration_impl->type == NECRO_AST_APATS_ASSIGNMENT)
            instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->apats_assignment.variable_name, .id = declarations->declaration.declaration_impl->apats_assignment.id };
        else
            assert(false);
        NecroSymbol member_symbol = necro_intern_get_type_class_member_symbol_from_instance_symbol(infer->intern, instance->dictionary_prototype->prototype_varid.symbol);

        //--------------------------------
        // Search for Type Class member
        bool                  found   = false;
        NecroTypeClassMember* members = type_class->members;
        while (members != NULL)
        {
            if (members->member_varid.symbol.id == member_symbol.id)
            {
                instance->dictionary_prototype->type_class_member_varid = (NecroCon) { .symbol = members->member_varid.symbol, .id = members->member_varid.id };
                found = true;
                break;
            }
            members = members->next;
        }

        //--------------------------------
        // Didn't find a matching method in class, bail out!
        if (found == false)
        {
            instance->dictionary_prototype->next                          = NULL;
            instance->dictionary_prototype->prototype_varid.id.id         = 0;
            instance->dictionary_prototype->type_class_member_varid.id.id = 0;
            necro_infer_ast_error(infer, NULL, instance->ast, "\'%s\' is not a (visible) method of class \'%s\'", necro_intern_get_string(infer->intern, member_symbol), necro_intern_get_string(infer->intern, instance->type_class_name.symbol));
            return;
        }

        //--------------------------------
        // Assemble types for overloaded methods
        assert(members != NULL);
        assert(necro_symtable_get(infer->symtable, members->member_varid.id) != NULL);
        NecroType* method_type      = necro_symtable_get(infer->symtable, members->member_varid.id)->type;
        NecroType* inst_method_type = necro_instantiate_method_sig(infer, type_class->type_var, method_type, inst_data_type);
        necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = necro_inst(infer, inst_method_type, NULL);
        necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = necro_gen(infer, necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type, NULL);
        necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type->pre_supplied = true;

        //--------------------------------
        // next
        instance->dictionary_prototype->next = prev_dictionary;
        declarations = declarations->declaration.next_declaration;
    }

    //--------------------------------
    // Infer declarations with types in symtable
    NecroDeclarationGroup* declaration_group = NULL;
    declarations = instance->ast->type_class_instance.declarations;
    while (declarations != NULL)
    {
        declaration_group = necro_append_declaration_group(&infer->arena, declarations->declaration.declaration_impl, declaration_group);
        declarations = declarations->declaration.next_declaration;
    }
    if (declaration_group != NULL)
        necro_infer_assignment(infer, declaration_group);

    //--------------------------------
    // Missing members check
    NecroTypeClassMember* type_class_members = type_class->members;
    while (type_class_members != NULL)
    {
        if (necro_is_infer_error(infer)) return;
        NecroDictionaryPrototype* dictionary_prototype = instance->dictionary_prototype;
        bool matched = false;
        while (dictionary_prototype != NULL)
        {
            if (dictionary_prototype->type_class_member_varid.id.id == type_class_members->member_varid.id.id)
            {
                matched = true;
                break;
            }
            dictionary_prototype = dictionary_prototype->next;
        }
        if (!matched)
        {
            necro_infer_ast_error(infer, NULL, instance->ast, "No explicit implementation for \'%s\' in the instance declaration for \'%s %s\'",
                necro_intern_get_string(infer->intern, type_class_members->member_varid.symbol),
                necro_intern_get_string(infer->intern, type_class->type_class_name.symbol),
                necro_intern_get_string(infer->intern, instance->data_type_name.symbol));
        }
        type_class_members = type_class_members->next;
    }

    //--------------------------------
    // Create Dictionary Instance
    necro_create_dictionary_instance2(infer, env, instance, type_class);
}

void necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context)
{
    assert(infer != NULL);
    if (necro_is_infer_error(infer)) return;
    if (type == NULL) return;
    if (context == NULL) return;
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        while (context != NULL)
        {
            if (type->var.var.id.id == context->type_var.id.id)
            {
                NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, context->type_class_name, context->type_var, NULL);
                type->var.context = necro_union_contexts(infer, type->var.context, class_context);
                if (necro_is_infer_error(infer)) return;
                // type->var.is_type_class_var = context_list->is_type_class_var;
            }
            context = context->next;
        }
        break;
    }
    case NECRO_TYPE_APP:  necro_apply_constraints(infer, type->app.type1, context); necro_apply_constraints(infer, type->app.type2, context); break;
    case NECRO_TYPE_FUN:  necro_apply_constraints(infer, type->fun.type1, context); necro_apply_constraints(infer, type->fun.type2, context); break;
    case NECRO_TYPE_CON:  necro_apply_constraints(infer, type->con.args,  context); break;
    case NECRO_TYPE_LIST: necro_apply_constraints(infer, type->list.item, context); necro_apply_constraints(infer, type->list.next, context); break;
    case NECRO_TYPE_FOR:  necro_infer_ast_error(infer, type, NULL, "Compiler bug: ForAll Type in necro_apply_constraints: %d", type->type); break;
    default:              necro_infer_ast_error(infer, type, NULL, "Compiler bug: Unimplemented Type in necro_apply_constraints: %d", type->type); break;
    }
}

NecroType* necro_instantiate_method_sig_go(NecroInfer* infer, NecroCon type_class_var, NecroType* method_type, NecroType* data_type)
{
    assert(infer != NULL);
    assert(data_type != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    if (method_type == NULL) return NULL;
    switch (method_type->type)
    {
    case NECRO_TYPE_VAR:
        if (method_type->var.var.id.id == type_class_var.id.id)
        {
            assert(method_type->var.is_type_class_var == true);
            return data_type;
        }
        else
        {
            return method_type;
        }
    case NECRO_TYPE_APP:
    {
        NecroType* type1 = necro_instantiate_method_sig_go(infer, type_class_var, method_type->app.type1, data_type);
        NecroType* type2 = necro_instantiate_method_sig_go(infer, type_class_var, method_type->app.type2, data_type);
        if (type1 != method_type->app.type1 || type2 != method_type->app.type2)
            return necro_create_type_app(infer, type1, type2);
        else
            return method_type;
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* type1 = necro_instantiate_method_sig_go(infer, type_class_var, method_type->fun.type1, data_type);
        NecroType* type2 = necro_instantiate_method_sig_go(infer, type_class_var, method_type->fun.type2, data_type);
        if (type1 != method_type->fun.type1 || type2 != method_type->fun.type2)
            return necro_create_type_fun(infer, type1, type2);
        else
            return method_type;
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* item = necro_instantiate_method_sig_go(infer, type_class_var, method_type->list.item, data_type);
        NecroType* next = necro_instantiate_method_sig_go(infer, type_class_var, method_type->list.next, data_type);
        if (item != method_type->list.item || next != method_type->list.next)
            return necro_create_type_list(infer, item, next);
        else
            return method_type;
    }
    case NECRO_TYPE_CON:
    {
        NecroType* args = necro_instantiate_method_sig_go(infer, type_class_var, method_type->con.args, data_type);
        if (args != method_type->con.args)
            return necro_create_type_con(infer, method_type->con.con, args, method_type->con.arity);
        else
            return method_type;
    }
    case NECRO_TYPE_FOR:
        // Drop type_class_var for_all
        if (method_type->for_all.var.id.id == type_class_var.id.id)
            return necro_instantiate_method_sig_go(infer, type_class_var, method_type->for_all.type, data_type);
        else
            return necro_create_for_all(infer, method_type->for_all.var, method_type->for_all.context, necro_instantiate_method_sig_go(infer, type_class_var, method_type->for_all.type, data_type));
    default: return necro_infer_ast_error(infer, method_type, NULL, "Compiler bug: Unimplemented Type in add_constrants_to_ty_vars: %d", method_type->type);
    }
}

NecroType* necro_instantiate_method_sig(NecroInfer* infer, NecroCon type_class_var, NecroType* method_type, NecroType* data_type)
{
    if (necro_is_infer_error(infer)) return NULL;
    assert(data_type != NULL);
    NecroType* data_type_for_all      = NULL;
    NecroType* data_type_for_all_curr = NULL;
    while (data_type->type == NECRO_TYPE_FOR)
    {
        if (data_type_for_all == NULL)
        {
            data_type_for_all      = necro_create_for_all(infer, data_type->for_all.var, data_type->for_all.context, NULL);
            data_type_for_all_curr = data_type_for_all;
        }
        else
        {
            data_type_for_all_curr->for_all.type = necro_create_for_all(infer, data_type->for_all.var, data_type->for_all.context, NULL);
            data_type_for_all_curr               = data_type_for_all_curr->for_all.type;
        }
        data_type = data_type->for_all.type;
    }
    NecroType* inst_method_type = necro_instantiate_method_sig_go(infer, type_class_var, method_type, data_type);
    if (data_type_for_all != NULL && data_type_for_all_curr != NULL)
    {
        data_type_for_all_curr->for_all.type = inst_method_type;
        data_type_for_all->pre_supplied      = true;
        return data_type_for_all;
    }
    else
    {
        inst_method_type->pre_supplied = true;
        return inst_method_type;
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

//=====================================================
// Contexts
//=====================================================
bool necro_context_contains_class(NecroTypeClassEnv* env, NecroTypeClassContext* context, NecroTypeClassContext* type_class)
{
    while (context != NULL)
    {
        if (context->type_class_name.id.id == type_class->type_class_name.id.id)
        {
            return true;
        }
        context = context->next;
    }
    return false;
}

bool necro_context_and_super_classes_contain_class(NecroTypeClassEnv* env, NecroTypeClassContext* context, NecroTypeClassContext* type_class)
{
    while (context != NULL)
    {
        if (context->type_class_name.id.id == type_class->type_class_name.id.id)
        {
            return true;
        }
        NecroTypeClassContext* super_classes = necro_type_class_table_get(&env->class_table, context->type_class_name.id.id)->context;
        if (necro_context_and_super_classes_contain_class(env, super_classes, type_class))
        {
            return true;
        }
        context = context->next;
    }
    return false;
}

bool necro_is_subclass(NecroTypeClassEnv* env, NecroTypeClassContext* maybe_subclass_context, NecroTypeClassContext* maybe_super_class_context)
{
    NecroTypeClassContext* super_classes = necro_type_class_table_get(&env->class_table, maybe_subclass_context->type_class_name.id.id)->context;
    return necro_context_and_super_classes_contain_class(env, super_classes, maybe_super_class_context);
}

bool necro_context_contains_class_and_type_var(NecroTypeClassContext* context, NecroTypeClassContext* type_class, NecroCon type_var)
{
    while (context != NULL)
    {
        if (context->type_class_name.id.id == type_class->type_class_name.id.id && context->type_var.id.id == type_var.id.id)
            return true;
        context = context->next;
    }
    return false;
}

NecroTypeClassContext* necro_remove_super_classes(NecroInfer* infer, NecroTypeClassContext* sub_class, NecroTypeClassContext* context)
{
    NecroTypeClassContext* head = context;
    NecroTypeClassContext* curr = head;
    NecroTypeClassContext* prev = NULL;
    while (curr != NULL)
    {
        if (necro_is_subclass(infer->type_class_env, sub_class, curr))
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
        prev = curr;
        curr = curr->next;
    }
    return head;
}

NecroTypeClassContext* necro_scrub_super_classes(NecroInfer* infer, NecroTypeClassContext* context)
{
    if (context == NULL || context->next == NULL)
        return context;
    NecroTypeClassContext* head = context;
    NecroTypeClassContext* curr = head;
    while (curr != NULL)
    {
        head = necro_remove_super_classes(infer, curr, head);
        curr = curr->next;
    }
    return head;
}

NecroTypeClassContext* necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2)
{
    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        if (!necro_context_contains_class(infer->type_class_env, head, context1))
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
        }
        context1 = context1->next;
    }

    // Insert context2 classes if they aren't already in context1
    while (context2 != NULL)
    {
        if (!necro_context_contains_class(infer->type_class_env, head, context2))
        {
            NecroTypeClassContext* new_context = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassContext));
            new_context->type_class_name       = context2->type_class_name;
            new_context->type_var              = context2->type_var;
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
        }
        context2 = context2->next;
    }

    // Scrub super classes
    return necro_scrub_super_classes(infer, head);
}

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

bool necro_ambiguous_type_class_check(NecroInfer* infer, NecroSymbol type_sig_name, NecroTypeClassContext* context, NecroType* type)
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
                necro_id_as_character_string(infer, (NecroVar) { .id = context->type_var.id, .symbol = context->type_var.symbol }),
                necro_intern_get_string(infer->intern, type_sig_name));
            return true;
        }
        context = context->next;
    }
    return false;
}

bool necro_constrain_class_variable_check(NecroInfer* infer, NecroCon type_class_name, NecroCon type_var, NecroSymbol type_sig_symbol, NecroTypeClassEnv* env, NecroTypeClassContext* context)
{
    assert(infer != NULL);
    assert(env != NULL);
    if (necro_is_infer_error(infer)) return false;
    while (context != NULL)
    {
        if (context->type_var.id.id == type_var.id.id)
        {
            necro_infer_ast_error(infer, NULL, NULL, "Constraint \'%s %s\' in the type for method \'%s\' constrains only the type class variable",
                necro_intern_get_string(infer->intern, context->type_class_name.symbol),
                necro_intern_get_string(infer->intern, context->type_var.symbol),
                necro_intern_get_string(infer->intern, type_sig_symbol));
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
    NecroType* member_type = NULL;
    if (necro_symtable_get(infer->symtable, prototype->prototype_varid.id) != NULL)
        member_type = necro_symtable_get(infer->symtable, prototype->prototype_varid.id)->type;
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
    printf("data_type: ");
    if (instance->data_type != NULL)
        necro_print_type_sig(instance->data_type, intern);
    else
        printf(" null_type");
    printf("\n");
    print_white_space(white_count + 4);
    printf("dictionary prototype:\n");
    print_white_space(white_count + 4);
    printf("[\n");
    necro_print_dictionary_prototype(instance->dictionary_prototype, infer, intern, white_count + 4);
    print_white_space(white_count + 4);
    printf("]\n");
    print_white_space(white_count + 4);
    printf("dictionary: \n\n");
    // print_white_space(white_count + 4);
    necro_print_reified_ast_node(instance->ast->type_class_instance.dictionary_instance, intern);
    puts("");
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
//=====================================================
// Dictionary Translation
//=====================================================
void necro_type_class_translate_go(NecroTypeClassTranslator* class_translator, NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(class_translator != NULL);
    assert(infer != NULL);
    assert(env != NULL);
    if (necro_is_infer_error(infer)) return;
    if (ast == NULL)
        return;
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_TOP_DECL:
    {
        NecroASTNode* curr = ast;
        while (curr != NULL)
        {
            if (curr->top_declaration.declaration->type == NECRO_AST_SIMPLE_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_APATS_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_PAT_ASSIGNMENT)
                necro_type_class_translate_go(class_translator, infer, env, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }
        break;
    }

    case NECRO_AST_DECL:
    {
        NecroASTNode* curr = ast;
        while (curr != NULL)
        {
            if (curr->top_declaration.declaration->type == NECRO_AST_SIMPLE_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_APATS_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_PAT_ASSIGNMENT)
                necro_type_class_translate_go(class_translator, infer, env, curr->top_declaration.declaration);
            curr = curr->declaration.next_declaration;
        }
        break;
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
    {
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_instance.context);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_instance.qtycls);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_instance.inst);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_instance.declarations);
        break;
    }

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        necro_type_class_translate_go(class_translator, infer, env, ast->simple_assignment.rhs);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        necro_type_class_translate_go(class_translator, infer, env, ast->apats_assignment.apats);
        necro_type_class_translate_go(class_translator, infer, env, ast->apats_assignment.rhs);
        break;
    }

    case NECRO_AST_PAT_ASSIGNMENT:
        necro_type_class_translate_go(class_translator, infer, env, ast->pat_assignment.rhs);
        break;

    case NECRO_AST_DATA_DECLARATION:
        necro_type_class_translate_go(class_translator, infer, env, ast->data_declaration.simpletype);
        necro_type_class_translate_go(class_translator, infer, env, ast->data_declaration.constructor_list);
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:                  break;
        case NECRO_VAR_DECLARATION:          break;
        case NECRO_VAR_SIG:                  break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        break;

    case NECRO_AST_CONID:
        break;

    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        necro_type_class_translate_go(class_translator, infer, env, ast->bin_op.lhs);
        necro_type_class_translate_go(class_translator, infer, env, ast->bin_op.rhs);
        break;
    case NECRO_AST_IF_THEN_ELSE:
        necro_type_class_translate_go(class_translator, infer, env, ast->if_then_else.if_expr);
        necro_type_class_translate_go(class_translator, infer, env, ast->if_then_else.then_expr);
        necro_type_class_translate_go(class_translator, infer, env, ast->if_then_else.else_expr);
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_type_class_translate_go(class_translator, infer, env, ast->op_left_section.left);
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_type_class_translate_go(class_translator, infer, env, ast->op_right_section.right);
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_type_class_translate_go(class_translator, infer, env, ast->right_hand_side.declarations);
        necro_type_class_translate_go(class_translator, infer, env, ast->right_hand_side.expression);
        break;
    case NECRO_AST_LET_EXPRESSION:
        necro_type_class_translate_go(class_translator, infer, env, ast->let_expression.declarations);
        necro_type_class_translate_go(class_translator, infer, env, ast->let_expression.expression);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_type_class_translate_go(class_translator, infer, env, ast->fexpression.aexp);
        necro_type_class_translate_go(class_translator, infer, env, ast->fexpression.next_fexpression);
        break;
    case NECRO_AST_APATS:
        necro_type_class_translate_go(class_translator, infer, env, ast->apats.apat);
        necro_type_class_translate_go(class_translator, infer, env, ast->apats.next_apat);
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        necro_type_class_translate_go(class_translator, infer, env, ast->lambda.apats);
        necro_type_class_translate_go(class_translator, infer, env, ast->lambda.expression);
        break;
    case NECRO_AST_DO:
        necro_type_class_translate_go(class_translator, infer, env, ast->do_statement.statement_list);
        break;
    case NECRO_AST_LIST_NODE:
        necro_type_class_translate_go(class_translator, infer, env, ast->list.item);
        necro_type_class_translate_go(class_translator, infer, env, ast->list.next_item);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        necro_type_class_translate_go(class_translator, infer, env, ast->expression_list.expressions);
        break;
    case NECRO_AST_EXPRESSION_SEQUENCE:
        necro_type_class_translate_go(class_translator, infer, env, ast->expression_sequence.expressions);
        break;
    case NECRO_AST_TUPLE:
        necro_type_class_translate_go(class_translator, infer, env, ast->tuple.expressions);
        break;
    case NECRO_BIND_ASSIGNMENT:
        necro_type_class_translate_go(class_translator, infer, env, ast->bind_assignment.expression);
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_type_class_translate_go(class_translator, infer, env, ast->pat_bind_assignment.pat);
        necro_type_class_translate_go(class_translator, infer, env, ast->pat_bind_assignment.expression);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_type_class_translate_go(class_translator, infer, env, ast->arithmetic_sequence.from);
        necro_type_class_translate_go(class_translator, infer, env, ast->arithmetic_sequence.then);
        necro_type_class_translate_go(class_translator, infer, env, ast->arithmetic_sequence.to);
        break;
    case NECRO_AST_CASE:
        necro_type_class_translate_go(class_translator, infer, env, ast->case_expression.expression);
        necro_type_class_translate_go(class_translator, infer, env, ast->case_expression.alternatives);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_type_class_translate_go(class_translator, infer, env, ast->case_alternative.pat);
        necro_type_class_translate_go(class_translator, infer, env, ast->case_alternative.body);
        break;
    case NECRO_AST_TYPE_APP:
        necro_type_class_translate_go(class_translator, infer, env, ast->type_app.ty);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_app.next_ty);
        break;
    case NECRO_AST_BIN_OP_SYM:
        necro_type_class_translate_go(class_translator, infer, env, ast->bin_op_sym.left);
        necro_type_class_translate_go(class_translator, infer, env, ast->bin_op_sym.op);
        necro_type_class_translate_go(class_translator, infer, env, ast->bin_op_sym.right);
        break;
    case NECRO_AST_CONSTRUCTOR:
        necro_type_class_translate_go(class_translator, infer, env, ast->constructor.conid);
        necro_type_class_translate_go(class_translator, infer, env, ast->constructor.arg_list);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        necro_type_class_translate_go(class_translator, infer, env, ast->simple_type.type_con);
        necro_type_class_translate_go(class_translator, infer, env, ast->simple_type.type_var_list);
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_declaration.context);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_declaration.tycls);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_declaration.tyvar);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_declaration.declarations);
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        necro_type_class_translate_go(class_translator, infer, env, ast->type_signature.var);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_signature.context);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_signature.type);
        break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_context.conid);
        necro_type_class_translate_go(class_translator, infer, env, ast->type_class_context.varid);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        necro_type_class_translate_go(class_translator, infer, env, ast->function_type.type);
        necro_type_class_translate_go(class_translator, infer, env, ast->function_type.next_on_arrow);
        break;

    default:
        necro_error(&infer->error, ast->source_loc, "Unrecognized AST Node type found in type class translation: %d", ast->type);
        break;
    }
}

inline NecroSymbol necro_type_class_name_to_dictionary_name(NecroIntern* intern, NecroSymbol type_class_name)
{
    return necro_intern_concat_symbols(intern, type_class_name, necro_intern_string(intern, "@Dictionary"));
}

inline NecroSymbol necro_create_dictionary_arg_name(NecroIntern* intern, NecroSymbol type_class_name)
{
    return necro_intern_concat_symbols(intern, necro_intern_string(intern, "dictionaryArg@"), type_class_name);
}

inline NecroSymbol necro_create_selector_name(NecroIntern* intern, NecroSymbol member_name, NecroSymbol dictionary_name)
{
    // NecroSymbol   member_name   = member_ast->declaration.declaration_impl->type_signature.var->variable.symbol;
    NecroSymbol   select_symbol = necro_intern_string(intern, "select@");
    NecroSymbol   at_symbol     = necro_intern_string(intern, "@");
    NecroSymbol   selector_name = necro_intern_concat_symbols(intern, select_symbol, member_name);
    selector_name = necro_intern_concat_symbols(intern, selector_name, at_symbol);
    selector_name = necro_intern_concat_symbols(intern, selector_name, dictionary_name);
    return selector_name;
}

inline NecroSymbol necro_create_dictionary_instance_name(NecroIntern* intern, NecroSymbol type_class_name, NecroASTNode* type_class_instance_ast)
{
    NecroSymbol dictionary_name          = necro_type_class_name_to_dictionary_name(intern, type_class_name);
    NecroSymbol dictionary_symbol        = necro_intern_string(intern, "dictionaryInstance@");
    NecroSymbol dictionary_instance_name;
    if (type_class_instance_ast->type_class_instance.inst->type == NECRO_AST_CONID)
    {
        dictionary_instance_name = necro_intern_concat_symbols(intern, dictionary_symbol, type_class_instance_ast->type_class_instance.inst->conid.symbol);
    }
    else if (type_class_instance_ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
    {
        dictionary_instance_name = necro_intern_concat_symbols(intern, dictionary_symbol, type_class_instance_ast->type_class_instance.inst->constructor.conid->conid.symbol);
    }
    else
    {
        assert(false);
    }
    dictionary_instance_name = necro_intern_concat_symbols(intern, dictionary_instance_name, dictionary_name);
    return dictionary_instance_name;
}

// inline NecroSymbol necro_create_(NecroIntern* intern, NecroSymbol type_class_name, NecroASTNode* type_class_instance_ast)

// generalize this to also look up super class dictionaries!
// NecroASTNode* necro_create_selector(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast, NecroSymbol dictionary_name, NecroASTNode* member_ast)
NecroASTNode* necro_create_selector(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast, NecroSymbol dictionary_name, void* member_to_select, NecroSymbol selector_name)
{
    NecroASTNode* pat_var  = necro_create_variable_ast(arena, intern, "var@", NECRO_VAR_DECLARATION);
    NecroASTNode* rhs_var  = necro_create_variable_ast(arena, intern, "var@", NECRO_VAR_VAR);
    NecroASTNode* con_args = NULL;
    NecroASTNode* con_head = NULL;
    NecroASTNode* members  = type_class_ast->type_class_declaration.declarations;
    while (members != NULL)
    {
        if ((void*)members == member_to_select)
        {
            if (con_head == NULL)
            {
                con_args = necro_create_apat_list_ast(arena, pat_var, NULL);
                con_head = con_args;
            }
            else
            {
                con_args->apats.next_apat = necro_create_apat_list_ast(arena, pat_var, NULL);
                con_args                  = con_args->apats.next_apat;
            }
        }
        else
        {
            if (con_head == NULL)
            {
                con_args = necro_create_apat_list_ast(arena, necro_create_wild_card_ast(arena), NULL);
                con_head = con_args;
            }
            else
            {
                con_args->apats.next_apat = necro_create_apat_list_ast(arena, necro_create_wild_card_ast(arena), NULL);
                con_args                  = con_args->list.next_item;
            }
        }
        members = members->declaration.next_declaration;
    }
    NecroASTNode* context = type_class_ast->type_class_declaration.context;
    if (context != NULL && context->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        context = necro_create_ast_list(arena, context, NULL);
    while (context != NULL)
    {
        if ((void*)(context->list.item) == member_to_select)
        {
            if (con_head == NULL)
            {
                con_args = necro_create_apat_list_ast(arena, pat_var, NULL);
                con_head = con_args;
            }
            else
            {
                con_args->apats.next_apat = necro_create_apat_list_ast(arena, pat_var, NULL);
                con_args                  = con_args->apats.next_apat;
            }
        }
        else
        {
            if (con_head == NULL)
            {
                con_args = necro_create_apat_list_ast(arena, necro_create_wild_card_ast(arena), NULL);
                con_head = con_args;
            }
            else
            {
                con_args->apats.next_apat = necro_create_apat_list_ast(arena, necro_create_wild_card_ast(arena), NULL);
                con_args                  = con_args->list.next_item;
            }
        }
        context = context->list.next_item;
    }
    NecroASTNode* constructor   = necro_create_data_constructor_ast(arena, intern, necro_intern_get_string(intern, dictionary_name), con_head);
    constructor->constructor.conid->conid.con_type = NECRO_CON_VAR;
    NecroASTNode* selector_args = necro_create_apat_list_ast(arena, constructor, NULL);
    // NecroSymbol   selector_name = necro_create_selector_name(intern, member_ast->declaration.declaration_impl->type_signature.var->variable.symbol, dictionary_name);
    NecroASTNode* selector_rhs  = necro_create_rhs_ast(arena, rhs_var, NULL);
    NecroASTNode* selector_ast  = necro_create_apats_assignment_ast(arena, intern, necro_intern_get_string(intern, selector_name), selector_args, selector_rhs);
    puts("--");
    necro_print_reified_ast_node(selector_ast, intern);
    return selector_ast;
}

// TODO: Add extra parameter to hold super class dictionaries, but what form does it take, and how to select it?
void necro_create_dictionary_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast)
{
    NecroSymbol   dictionary_name        = necro_type_class_name_to_dictionary_name(intern, type_class_ast->type_class_declaration.tycls->conid.symbol);
    NecroASTNode* dictionary_simple_type = necro_create_simple_type_ast(arena, intern, necro_intern_get_string(intern, dictionary_name), NULL);
    NecroASTNode* dictionary_args_head   = NULL;
    NecroASTNode* dictionary_args        = NULL;
    NecroASTNode* members                = type_class_ast->type_class_declaration.declarations;
    while (members != NULL)
    {
        if (dictionary_args_head == NULL)
        {
            dictionary_args_head = necro_create_ast_list(arena, necro_create_conid_ast(arena, intern, "Prim@Any", NECRO_CON_TYPE_VAR), NULL);
            dictionary_args      = dictionary_args_head;
        }
        else
        {
            dictionary_args->list.next_item = necro_create_ast_list(arena, necro_create_conid_ast(arena, intern, "Prim@Any", NECRO_CON_TYPE_VAR), NULL);
            dictionary_args                 = dictionary_args->list.next_item;
        }
        members = members->declaration.next_declaration;
    }
    NecroASTNode* context = type_class_ast->type_class_declaration.context;
    if (context != NULL && context->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        context = necro_create_ast_list(arena, context, NULL);
    while (context != NULL)
    {
        NecroSymbol super_dictionary_name = necro_type_class_name_to_dictionary_name(intern, context->list.item->type_class_context.conid->conid.symbol);
        if (dictionary_args_head == NULL)
        {
            dictionary_args_head = necro_create_ast_list(arena, necro_create_conid_ast(arena, intern, necro_intern_get_string(intern, super_dictionary_name), NECRO_CON_TYPE_VAR), NULL);
            dictionary_args      = dictionary_args_head;
        }
        else
        {
            dictionary_args->list.next_item = necro_create_ast_list(arena, necro_create_conid_ast(arena, intern, necro_intern_get_string(intern, super_dictionary_name), NECRO_CON_TYPE_VAR), NULL);
            dictionary_args                 = dictionary_args->list.next_item;
        }
        context = context->list.next_item;
    }
    NecroASTNode* dictionary_constructor      = necro_create_data_constructor_ast(arena, intern, necro_intern_get_string(intern, dictionary_name), dictionary_args_head);
    NecroASTNode* dictionary_constructor_list = necro_create_ast_list(arena, dictionary_constructor, NULL);
    NecroASTNode* dictionary_data_declaration = necro_create_data_declaration_ast(arena, intern, dictionary_simple_type, dictionary_constructor_list);
    NecroASTNode* dictionary_declaration_list = NULL;

    // Member selectors
    members = type_class_ast->type_class_declaration.declarations;
    while (members != NULL)
    {
        dictionary_declaration_list = necro_create_top_level_declaration_list(arena,
            necro_create_selector(arena, intern, type_class_ast, dictionary_name, (void*)members, necro_create_selector_name(intern, members->declaration.declaration_impl->type_signature.var->variable.symbol, dictionary_name)),
                dictionary_declaration_list);
        members = members->declaration.next_declaration;
    }

    // Super class selectors
    // Messy and inefficient, but oh well...
    // TODO: Make context ALWAYS a list, even if it's just one item...
    context = type_class_ast->type_class_declaration.context;
    if (context != NULL && context->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        context = necro_create_ast_list(arena, context, NULL);
    while (context != NULL)
    {
        dictionary_declaration_list = necro_create_top_level_declaration_list(arena,
            necro_create_selector(arena, intern, type_class_ast, dictionary_name, (void*)(context->list.item), necro_create_selector_name(intern, context->list.item->type_class_context.conid->conid.symbol, dictionary_name)),
                dictionary_declaration_list);
        context = context->list.next_item;
    }

    type_class_ast->type_class_declaration.dictionary_data_declaration = necro_create_top_level_declaration_list(arena, dictionary_data_declaration, dictionary_declaration_list);
}

void necro_create_dictionary_instance(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_instance_ast)
{
    NecroSymbol dictionary_instance_name = necro_create_dictionary_instance_name(intern, type_class_instance_ast->type_class_instance.qtycls->conid.symbol, type_class_instance_ast);
    // Leave rhs empty for first pass
    NecroASTNode* dictionary_instance_rhs  = necro_create_rhs_ast(arena, necro_create_conid_ast(arena, intern, "()", NECRO_CON_VAR), NULL);
    NecroASTNode* dictionary_instance      = NULL;
    if (type_class_instance_ast->type_class_instance.context == NULL)
    {
        dictionary_instance = necro_create_simple_assignment(arena, intern, necro_intern_get_string(intern, dictionary_instance_name), dictionary_instance_rhs);
    }
    else
    {
        NecroASTNode* dictionary_instance_args_head = NULL;
        NecroASTNode* dictionary_instance_args      = NULL;
        NecroASTNode* context                       = type_class_instance_ast->type_class_instance.context;
        if (context->type == NECRO_AST_TYPE_CLASS_CONTEXT)
            context = necro_create_ast_list(arena, context, NULL);
        while (context != NULL)
        {
            NecroSymbol   arg_name = necro_create_dictionary_arg_name(intern, context->list.item->type_class_context.conid->conid.symbol);
            NecroASTNode* arg_var  = necro_create_variable_ast(arena, intern, necro_intern_get_string(intern, arg_name), NECRO_VAR_DECLARATION);
            if (dictionary_instance_args_head == NULL)
            {
                dictionary_instance_args_head = necro_create_apat_list_ast(arena, arg_var, NULL);
                dictionary_instance_args      = dictionary_instance_args_head;
            }
            else
            {
                dictionary_instance_args->apats.next_apat = necro_create_apat_list_ast(arena, arg_var, NULL);
                dictionary_instance_args                  = dictionary_instance_args->apats.next_apat;
            }
            context = context->list.next_item;
        }
        dictionary_instance = necro_create_apats_assignment_ast(arena, intern, necro_intern_get_string(intern, dictionary_instance_name), dictionary_instance_args_head, dictionary_instance_rhs);
    }
    type_class_instance_ast->type_class_instance.dictionary_instance = dictionary_instance;
}

NecroASTNode* retrieveDictionaryFromContext(NecroPagedArena* arena, NecroIntern* intern, NecroTypeClassEnv* env, NecroTypeClassContext* context, NecroTypeClassContext* dictionary_to_retrieve)
{
    // NecroASTNode* dictionaryAST = NULL;
    while (context != NULL)
    {
        // if (necro_context_and_super_classes_contain_class(env, context, dictionary_to_retrieve))
        // {
            if (context->type_class_name.id.id == dictionary_to_retrieve->type_class_name.id.id)
            {
                return necro_create_variable_ast(arena, intern, necro_intern_get_string(intern, necro_type_class_name_to_dictionary_name(intern, dictionary_to_retrieve->type_class_name.symbol)), NECRO_VAR_VAR);
            }
            else
            {
                NecroTypeClassContext* super_classes = necro_type_class_table_get(&env->class_table, context->type_class_name.id.id)->context;
                while (super_classes != NULL)
                {
                    NecroASTNode* super_ast = retrieveDictionaryFromContext(arena, intern, env, super_classes, dictionary_to_retrieve);
                    if (super_ast != NULL)
                    {
                        // NecroTypeClassContext*
                    }
                    super_classes = super_classes->next;
                }
            }
        // }
        context = context->next;
    }
    return NULL;
}

// TODO: Make sure to monomorphize!!!!
// TODO: How the fuck do super classes play into this?
// TODO: Actual variable ID (Put var in dictionary prototype?)
// TODO: Finish super classes in dictionaries!
// TODO: Super class dictionary selectors!
// TODO: Take into account different type vars!
void necro_create_dictionary_instance2(NecroInfer* infer, NecroTypeClassEnv* class_env, NecroTypeClassInstance* instance, NecroTypeClass* type_class)
{
    // Second pass, make sure all names have resolved ids generated by the renamer
    NecroASTNode* dictionary_instance = instance->ast->type_class_instance.dictionary_instance;
    NecroSymbol   dictionary_name     = type_class->ast->type_class_declaration.dictionary_data_declaration->top_declaration.declaration->data_declaration.simpletype->simple_type.type_con->conid.symbol;
    NecroASTNode* dictionary_conid    = necro_create_conid_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_name), NECRO_CON_VAR);
    dictionary_conid->conid.id        = type_class->ast->type_class_declaration.dictionary_data_declaration->top_declaration.declaration->data_declaration.simpletype->simple_type.type_con->conid.id;
    NecroASTNode* dictionary_fexpr    = dictionary_conid;
    NecroTypeClassMember* members     = type_class->members;
    while (members != NULL)
    {
        NecroDictionaryPrototype* dictionary_prototype = instance->dictionary_prototype;
        while (dictionary_prototype != NULL)
        {
            if (dictionary_prototype->type_class_member_varid.id.id == members->member_varid.id.id)
            {
                NecroASTNode* mvar = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_prototype->prototype_varid.symbol), NECRO_VAR_VAR);
                mvar->variable.id  = dictionary_prototype->prototype_varid.id;
                NecroType* prototype_member_type = necro_symtable_get(infer->symtable, dictionary_prototype->prototype_varid.id)->type;
                while (prototype_member_type->type == NECRO_TYPE_FOR)
                {
                    NecroTypeClassContext* for_all_context = prototype_member_type->for_all.context;
                    while (for_all_context != NULL)
                    {
                        if (necro_context_contains_class_and_type_var(instance->context, for_all_context, for_all_context->type_var))
                        {
                            NecroASTNode* dvar = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, necro_create_dictionary_arg_name(infer->intern, for_all_context->type_class_name.symbol)), NECRO_VAR_VAR);
                            // TODO: Actual ID
                            // dvar->variable.id  = dictionary_prototype->prototype_varid.id;
                            mvar = necro_create_fexpr_ast(&infer->arena, mvar, dvar);
                        }
                        for_all_context = for_all_context->next;
                    }
                    prototype_member_type = prototype_member_type->for_all.type;
                }
                dictionary_fexpr = necro_create_fexpr_ast(&infer->arena, dictionary_fexpr, mvar);
                break;
            }
            dictionary_prototype = dictionary_prototype->next;
        }
        members = members->next;
    }
    NecroTypeClassContext* super_classes = type_class->context;
    while (super_classes != NULL)
    {
        NecroSymbol   super_class_dictionary_name = necro_create_dictionary_instance_name(infer->intern, super_classes->type_class_name.symbol, instance->ast);
        NecroASTNode* dvar                        = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, super_class_dictionary_name), NECRO_VAR_VAR);
        // TODO: Super class dictionary arguments
        // TODO: Actual ID
        // dvar->variable.id  = dictionary_prototype->prototype_varid.id;
        dictionary_fexpr = necro_create_fexpr_ast(&infer->arena, dictionary_fexpr, dvar);
        super_classes = super_classes->next;
    }
    if (instance->context == NULL)
        dictionary_instance->simple_assignment.rhs->right_hand_side.expression = dictionary_fexpr;
    else
        dictionary_instance->apats_assignment.rhs->right_hand_side.expression = dictionary_fexpr;
    necro_print_reified_ast_node(dictionary_instance, infer->intern);
}
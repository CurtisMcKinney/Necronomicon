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
//    * Type signature context handling and union-ing
//    * Ambiguous type class check
//    * Constrains only type class head (i.e. var) check
//    * Printing variable names gets wonky when retaining printed names across methods and instances of type class, look at Either instance of Functor for example
//    * Better unification error messaging! This is especially true when it occurs during SuperClass constraint checking. Make necro_unify return struct with either Success or Error data
//    * Better prim handling, and start using shiny new type class capability
//    * Equipped with type classes we can make primitive functions: addInt, addFloat, addAudio, etc
//    * AST transformation creating and passing dictionaries around for type classes
//    * Strongly Connected Component analysis for declarations
//    * Language semantics

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
            ty_var->var.is_rigid           = false;
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
    necro_infer_kind(infer, type_class_var, NULL, type_class_var, "While declaring a type class: ");

    //--------------------------------
    // Constrain type class variable via super classes
    NecroTypeClassContext* context = type_class->context;
    while (context != NULL)
    {
        NecroTypeClass* super_class = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
        if (necro_is_infer_error(infer)) return;
        NecroType*      super_class_var = necro_gen(infer, necro_symtable_get(infer->symtable, super_class->type_var.id)->type, NULL);
        NecroType*      inst_super_var  = necro_inst(infer, super_class_var, NULL);
        type_class_var->var.context = necro_union_contexts(infer, super_class->context, type_class_var->var.context);
        necro_unify(infer, type_class_var, inst_super_var, NULL, type_class_var, "While declaring a type class: ");
        if (necro_is_infer_error(infer)) return;
        context = context->next;
    }

    //--------------------------------
    // Build Member Type Signatures
    NecroNode* declarations = type_class->ast->type_class_declaration.declarations;
    while (declarations != NULL)
    {
        if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
            continue;
        NecroType* type_sig    = necro_ast_to_type_sig_go(infer, declarations->declaration.declaration_impl->type_signature.type);
        type_sig->pre_supplied = true;
        type_sig->source_loc   = declarations->declaration.declaration_impl->source_loc;
        type_sig               = necro_gen(infer, type_sig, NULL);
        // TODO: Type signature contexts and ambiguity checks
        // NecroTypeClassContext* method_context = type_sig
        // if (necro_ambiguous_type_class_check(infer, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, method_context, type_sig)) return;
        // if (necro_constrain_class_variable_check(infer, type_class->type_class_name, type_class->type_var, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, env, method_context)) return;
        necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type = type_sig;
        if (necro_is_infer_error(infer)) return;
        NecroTypeClassMember* prev_member = type_class->members;
        type_class->members               = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
        type_class->members->member_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->type_signature.var->variable.symbol, .id = declarations->declaration.declaration_impl->type_signature.var->variable.id };
        type_class->members->next         = prev_member;
        declarations = declarations->declaration.next_declaration;
    }
    printf("Finishing type_class: %s\n", necro_intern_get_string(infer->intern, type_class->type_class_name.symbol));
}

//=====================================================
// Type Class Instances - Refactor
//=====================================================
void necro_dependency_analyze_type_class_instances(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClassInstance* instance);
void necro_finish_declaring_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class, NecroTypeClassInstance* instance);
void necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context);
bool necro_check_constraints(NecroInfer* infer, NecroTypeClassInstance* instance, NecroType* type, NecroTypeClassContext* context);

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

void necro_type_class_instances(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
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
    if (necro_is_infer_error(infer)) return;

    //---------------------------------------------------------------
    // Pass 2, Dependency Analyze and Finish
    current_decl = top_level_declarations;
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
    printf("INSTANCE: %s %s\n", necro_intern_get_string(infer->intern, instance->type_class_name.symbol), necro_intern_get_string(infer->intern, instance->data_type_name.symbol));

    instance->data_type = necro_ast_to_type_sig_go(infer, instance->ast->type_class_instance.inst);

    if (necro_is_infer_error(infer)) return;
    NecroType* class_var  = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
    assert(class_var != NULL);
    necro_infer_kind(infer, instance->data_type, class_var->kind, instance->data_type, "While creating an instance of a type class: ");
    if (necro_is_infer_error(infer)) return;

    // Apply constraints
    necro_apply_constraints(infer, instance->data_type, instance->context);
    instance->data_type = necro_gen(infer, instance->data_type, NULL);

    NecroType*             type_var = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
    NecroTypeClassContext* context  = type_var->var.context;
    while (context != NULL)
    {
        NecroTypeClass*          super_class    = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
        if (necro_is_infer_error(infer)) return;
        NecroTypeClassInstance*  super_instance = necro_get_type_class_instance(infer, env, super_class->type_class_name, instance->data_type_name);
        if (necro_is_infer_error(infer)) return;

        NecroType* super_type = necro_inst(infer, super_instance->data_type, NULL);
        necro_unify(infer, instance->data_type, super_type, NULL, instance->data_type, "While creating an instance of a type class: ");

        if (necro_is_infer_error(infer)) return;
        context = context->next;
    }

    NecroType* inst_data_type = necro_inst(infer, instance->data_type, NULL);
    necro_infer_kind(infer, inst_data_type, NULL, inst_data_type, "While creating an instance method: ");

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
        // Assemble and type check instance method
        assert(members != NULL);
        assert(necro_symtable_get(infer->symtable, members->member_varid.id) != NULL);
        NecroType* method_type      = necro_symtable_get(infer->symtable, members->member_varid.id)->type;
        NecroType* inst_method_type = necro_instantiate_method_sig(infer, type_class->type_var, method_type, inst_data_type);
        necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = necro_inst(infer, inst_method_type, NULL);
        necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = necro_gen(infer, necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type, NULL);
        necro_infer_go(infer, declarations->declaration.declaration_impl);

        //--------------------------------
        // next
        instance->dictionary_prototype->next = prev_dictionary;
        declarations = declarations->declaration.next_declaration;
    }

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
}

void necro_apply_super_class_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context)
{
    assert(infer != NULL);
    assert(type->type == NECRO_TYPE_VAR);
    if (necro_is_infer_error(infer)) return;
    if (type == NULL) return;
    if (context == NULL) return;
    NecroTypeClass*        type_class    = necro_type_class_table_get(&infer->type_class_env->class_table, context->type_class_name.id.id);
    assert(type_class != NULL);
    NecroTypeClassContext* super_classes = type_class->context;
    while (super_classes != NULL)
    {
        NecroTypeClassContext* super_class = necro_create_type_class_context(&infer->arena, super_classes->type_class_name, (NecroCon) { .id = type->var.var.id, .symbol = type->var.var.symbol }, NULL);
        type->var.context = necro_union_contexts(infer, type->var.context, super_class);
        necro_apply_super_class_constraints(infer, type, super_classes);
        super_classes = super_classes->next;
    }
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
                necro_apply_super_class_constraints(infer, type, context);
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

// void necro_create_type_class_declaration_pass3(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
// {
//     assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
//     NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, ast->type_class_declaration.tycls->conid.id.id);
//     assert(type_class != NULL);

//     //--------------------------------
//     // Context contraints
//     // TODO: Finish
//     NecroTypeClassContext* context = type_class->context;
//     while (context != NULL)
//     {
//     }

//     // //--------------------------------
//     // // SuperClass check
//     // if (necro_super_class_check(infer, type_class->type_class_name, necro_symtable_get(infer->symtable, type_class->type_var.id)->type->var.arity, env, type_class->context)) return;

//     // //--------------------------------
//     // // Constraint Member Type Signatures
//     // NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, type_class->type_class_name, type_class->type_var, NULL);
//     // declarations = ast->type_class_declaration.declarations;
//     // while (declarations != NULL)
//     // {
//     //     if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
//     //         continue;

//     //     NecroType* type_sig = necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type;

//     //     NecroTypeClassContext* method_context = necro_ast_to_context(infer, env, declarations->declaration.declaration_impl->type_signature.context);
//     //     if (necro_constrain_class_variable_check(infer, type_class->type_class_name, type_class->type_var, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, env, method_context)) return;
//     //     NecroTypeClassContext* context       = necro_union_contexts(infer, method_context, class_context);
//     //     NecroTyVarContextList* context_list  = necro_create_ty_var_context_list(infer, &type_class->type_var, context);
//     //     if (necro_ambiguous_type_class_check(infer, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, context, type_sig))
//     //         return;
//     //     // simply union these things right?
//     //     necro_add_constraints_to_ty_vars(infer, type_sig, context_list);

//     //     type_sig               = necro_gen(infer, type_sig, NULL);
//     //     necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type      = type_sig;
//     //     necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->is_method = true;

//     //     NecroTypeClassMember* prev_member = type_class->members;
//     //     type_class->members               = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
//     //     type_class->members->member_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->type_signature.var->variable.symbol, .id = declarations->declaration.declaration_impl->type_signature.var->variable.id };
//     //     type_class->members->next         = prev_member;

//     //     declarations = declarations->declaration.next_declaration;
//     // }

//     //--------------------------------
//     //Default Member Implementations
// }

// //=====================================================
// // Type Class Instance
// //=====================================================
// void necro_create_type_class_instance_pass1(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
// {
//     assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
//     TRACE_TYPE_CLASS("necro_create_type_class_instance\n");

//     NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
//     NecroCon data_type_name;

//     if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
//     {
//         data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
//     }
//     else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
//     {
//         data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
//     }
//     else
//     {
//         assert(false);
//     }

//     assert(necro_symtable_get(infer->symtable, data_type_name.id)->type != NULL);
//     uint64_t key = necro_create_instance_key(data_type_name, type_class_name);
//     if (necro_type_class_instance_table_get(&env->instance_table, key) != NULL)
//     {
//         necro_infer_ast_error(infer, NULL, ast, "Duplicate Type Class Instance\n     class: %s\n     type: %s", necro_intern_get_string(infer->intern, type_class_name.symbol), necro_intern_get_string(infer->intern, data_type_name.symbol));
//         return;
//     }

//     NecroTypeClassInstance* instance = necro_type_class_instance_table_insert(&env->instance_table, key, NULL);
//     instance->type_class_name        = type_class_name;
//     instance->data_type_name         = data_type_name;
//     instance->context                = NULL;
//     instance->dictionary_prototype   = NULL;

//     NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, type_class_name.id.id);
//     assert(type_class != NULL);

//     //--------------------------------
//     // Construct context and data type sig
//     instance->context    = necro_ast_to_context(infer, env, ast->type_class_instance.context);
//     NecroType* data_type = necro_create_data_type_sig(infer, type_class, env, ast->type_class_instance.inst, instance->context);
//     instance->data_type  = data_type;
// }

// void necro_create_type_class_instance_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
// {
//     assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
//     TRACE_TYPE_CLASS("necro_create_type_class_instance\n");

//     NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
//     NecroCon data_type_name;

//     if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
//     {
//         data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
//     }
//     else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
//     {
//         data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
//     }
//     else
//     {
//         assert(false);
//     }

//     uint64_t                key      = necro_create_instance_key(data_type_name, type_class_name);
//     NecroTypeClassInstance* instance = necro_type_class_instance_table_get(&env->instance_table, key);
//     assert(instance != NULL);

//     NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, type_class_name.id.id);
//     assert(type_class != NULL);

//     //--------------------------------
//     // Construct context and data type sig
//     // instance->context    = necro_ast_to_context(infer, env, ast->type_class_instance.context);
//     NecroType* data_type = instance->data_type;
//     if (necro_is_infer_error(infer)) return;

//     //--------------------------------
//     // Super Class check
//     NecroTypeClassContext* super_classes = type_class->context;
//     while (super_classes != NULL)
//     {
//         if (necro_is_infer_error(infer)) return;
//         uint64_t super_key = necro_create_instance_key(data_type_name, super_classes->type_class_name);
//         if (necro_type_class_instance_table_get(&env->instance_table, super_key) == NULL)
//         {
//             necro_infer_ast_error(infer, NULL, ast, "No instance for \'%s %s\'. \'%s\' is a required super class of \'%s\'.",
//                 necro_intern_get_string(infer->intern, super_classes->type_class_name.symbol),
//                 necro_intern_get_string(infer->intern, instance->data_type_name.symbol),
//                 necro_intern_get_string(infer->intern, super_classes->type_class_name.symbol),
//                 necro_intern_get_string(infer->intern, type_class->type_class_name.symbol));
//             return;
//         }
//         NecroType* super_class_data_type = necro_inst(infer, necro_type_class_instance_table_get(&env->instance_table, super_key)->data_type, NULL);
//         if (necro_is_infer_error(infer)) return;
//         assert(super_class_data_type != NULL);
//         necro_unify(infer, data_type, super_class_data_type, NULL, data_type, "Required by a super class instance of this instance declaration.");
//         if (necro_is_infer_error(infer))
//         {
//             // TODO: Better unification error messaging!
//             // snprintf(infer->error.error_message, NECRO_MAX_ERROR_MESSAGE_LENGTH, "No instance for \'%s %s\', while attempting to satisfy the constraints placed by the super class instance \'%s %s\',\n during the instance declaration for \'%s %s\'",
//             //     necro_intern_get_string(infer->intern, necro_type_class_instance_table_get(&env->instance_table, super_key)->type_class_name.symbol),
//             //     necro_intern_get_string(infer->intern, necro_type_class_instance_table_get(&env->instance_table, super_key)->data_type_name.symbol),
//             //     necro_intern_get_string(infer->intern, instance->type_class_name.symbol),
//             //     necro_intern_get_string(infer->intern, instance->data_type_name.symbol));
//             return;
//         }
//         super_classes = super_classes->next;
//     }

//     //--------------------------------
//     // Dictionary Prototype
//     NecroNode* declarations = ast->type_class_instance.declarations;
//     while (declarations != NULL)
//     {
//         assert(declarations->type == NECRO_AST_DECL);
//         assert(declarations->declaration.declaration_impl != NULL);
//         if (necro_is_infer_error(infer)) return;

//         //--------------------------------
//         // Create dictionary prototype entry
//         NecroDictionaryPrototype* prev_dictionary = instance->dictionary_prototype;
//         instance->dictionary_prototype            = necro_paged_arena_alloc(&env->arena, sizeof(NecroDictionaryPrototype));
//         if (declarations->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT)
//             instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->simple_assignment.variable_name, .id = declarations->declaration.declaration_impl->simple_assignment.id };
//         else if (declarations->declaration.declaration_impl->type == NECRO_AST_APATS_ASSIGNMENT)
//             instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->apats_assignment.variable_name, .id = declarations->declaration.declaration_impl->apats_assignment.id };
//         else
//             assert(false);
//         NecroSymbol member_symbol = necro_intern_get_type_class_member_symbol_from_instance_symbol(infer->intern, instance->dictionary_prototype->prototype_varid.symbol);

//         //--------------------------------
//         // Search for Type Class member
//         bool                  found   = false;
//         NecroTypeClassMember* members = type_class->members;
//         while (members != NULL)
//         {
//             if (members->member_varid.symbol.id == member_symbol.id)
//             {
//                 instance->dictionary_prototype->type_class_member_varid = (NecroCon) { .symbol = members->member_varid.symbol, .id = members->member_varid.id };
//                 found = true;
//                 break;
//             }
//             members = members->next;
//         }

//         //--------------------------------
//         // Didn't find a matching method in class, bail out!
//         if (found == false)
//         {
//             instance->dictionary_prototype->next                          = NULL;
//             instance->dictionary_prototype->prototype_varid.id.id         = 0;
//             instance->dictionary_prototype->type_class_member_varid.id.id = 0;
//             necro_infer_ast_error(infer, NULL, ast, "\'%s\' is not a (visible) method of class \'%s\'", necro_intern_get_string(infer->intern, member_symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
//             return;
//         }

//         //--------------------------------
//         // Assemble and type check instance method
//         assert(members != NULL);
//         assert(necro_symtable_get(infer->symtable, members->member_varid.id) != NULL);
//         NecroType* method_type      = necro_symtable_get(infer->symtable, members->member_varid.id)->type;
//         NecroType* inst_method_type = necro_instantiate_method_sig(infer, type_class->type_var, method_type, data_type);
//         necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = inst_method_type;
//         necro_infer_go(infer, declarations->declaration.declaration_impl);

//         //--------------------------------
//         // next
//         instance->dictionary_prototype->next = prev_dictionary;
//         declarations = declarations->declaration.next_declaration;
//     }

//     //--------------------------------
//     // Missing members check
//     NecroTypeClassMember* type_class_members = type_class->members;
//     while (type_class_members != NULL)
//     {
//         if (necro_is_infer_error(infer)) return;
//         NecroDictionaryPrototype* dictionary_prototype = instance->dictionary_prototype;
//         bool matched = false;
//         while (dictionary_prototype != NULL)
//         {
//             if (dictionary_prototype->type_class_member_varid.id.id == type_class_members->member_varid.id.id)
//             {
//                 matched = true;
//                 break;
//             }
//             dictionary_prototype = dictionary_prototype->next;
//         }
//         if (!matched)
//         {
//             necro_infer_ast_error(infer, NULL, ast, "No explicit implementation for \'%s\' in the instance declaration for \'%s %s\'",
//                 necro_intern_get_string(infer->intern, type_class_members->member_varid.symbol),
//                 necro_intern_get_string(infer->intern, type_class->type_class_name.symbol),
//                 necro_intern_get_string(infer->intern, data_type_name.symbol));
//         }
//         type_class_members = type_class_members->next;
//     }
// }

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

// NecroType* necro_create_data_type_sig(NecroInfer* infer, NecroTypeClass* type_class, NecroTypeClassEnv* env, NecroNode* ast_data_type, NecroTypeClassContext* instance_context)
// {
//     NecroType* data_type = necro_ast_to_type_sig_go(infer, ast_data_type);
//     if (necro_is_infer_error(infer)) return NULL;
//     assert(data_type != NULL);
//     if (necro_is_infer_error(infer)) return NULL;

//     assert(data_type->type == NECRO_TYPE_CON);

//     // No Sanity check?
//     // necro_check_type_sanity(infer, data_type);
//     // if (necro_is_infer_error(infer)) return NULL;

//     // Kind check!!!!
//     {
//         size_t type_arity       = necro_symtable_get(infer->symtable, data_type->con.con.id)->type->con.arity;
//         size_t data_type_arity  = data_type->con.arity;
//         size_t num_args         = necro_type_list_count(data_type->con.args);
//         size_t type_class_arity = necro_symtable_get(infer->symtable, type_class->type_var.id)->type->var.arity;
//         size_t applied_arity    = data_type_arity - num_args;
//         assert(data_type_arity == type_arity);
//         if (type_class_arity != -1 && type_class_arity != applied_arity)
//         {
//             necro_infer_ast_error(infer, data_type, ast_data_type, "Type class \'%s\' expects kind: %d, but found kind %d in \'%s\'",
//                 necro_intern_get_string(infer->intern, type_class->type_class_name.symbol),
//                 type_class_arity,
//                 applied_arity,
//                 necro_intern_get_string(infer->intern, data_type->con.con.symbol)
//             );
//             return NULL;
//         }
//     }

//     data_type->pre_supplied = true;
//     data_type->source_loc = ast_data_type->source_loc;
//     // if (necro_constrain_class_variable_check(infer, type_class->type_class_name, type_class->type_var, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, env, method_context)) return;
//     NecroTyVarContextList* context_list  = necro_create_ty_var_context_list(infer, NULL, instance_context);
//     // if (necro_ambiguous_type_class_check(infer, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, context, type_sig)) return;
//     necro_add_constraints_to_ty_vars(infer, data_type, context_list);
//     NecroType* gen_type = necro_gen(infer, data_type, NULL);
//     assert(gen_type != NULL);
//     return gen_type;
// }

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
// Searches context and all superclasses of the given context
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

NecroTypeClassContext* necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2)
{
    if (context1 == NULL)
        return context2;

    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        // if (!necro_context_contains_class_and_type_var(head, context1, context1->type_var))
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
        // if (!necro_context_contains_class_and_type_var(head, context2, context2->type_var))
        if (!necro_context_contains_class(infer->type_class_env, head, context2))
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

// NecroTyVarContextList* necro_create_ty_var_context_list(NecroInfer* infer, NecroCon* type_class_var, NecroTypeClassContext* context)
// {
//     NecroCon               current_var;
//     NecroTyVarContextList* context_list        = NULL;
//     NecroTypeClassContext* current_var_context = NULL;
//     NecroTypeClassContext* sub_context         = NULL;
//     NecroTypeClassContext* current_context     = context;
//     while (current_context != NULL)
//     {
//         if (necro_context_list_contains_ty_var(context_list, current_context->type_var))
//         {
//             current_context = current_context->next;
//             continue;
//         }
//         current_var_context = NULL;
//         current_var         = current_context->type_var;
//         sub_context         = context;
//         while (sub_context != NULL)
//         {
//             if (sub_context->type_var.id.id == current_var.id.id)
//             {
//                 NecroTypeClassContext* new_context = necro_create_type_class_context(&infer->arena, sub_context->type_class_name, sub_context->type_var, current_var_context);
//                 current_var_context                = new_context;
//             }
//             sub_context = sub_context->next;
//         }
//         if (current_var_context != NULL)
//         {
//             NecroTyVarContextList* new_context_list = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTyVarContextList));
//             new_context_list->type_var              = current_var;
//             new_context_list->context               = current_var_context;
//             new_context_list->is_type_class_var     = (type_class_var == NULL) ? false : (current_var.id.id == type_class_var->id.id);
//             new_context_list->next                  = context_list;
//             context_list                            = new_context_list;
//         }
//         current_context = current_context->next;
//     }
//     return context_list;
// }

// void necro_add_constraints_to_ty_vars(NecroInfer* infer, NecroType* type, NecroTyVarContextList* context_list)
// {
//     assert(infer != NULL);
//     if (necro_is_infer_error(infer)) return;
//     if (type == NULL) return;
//     if (context_list == NULL) return;
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR:
//     {
//         while (context_list != NULL)
//         {
//             if (type->var.var.id.id == context_list->type_var.id.id)
//             {
//                 type->var.context           = context_list->context;
//                 // type->var.is_rigid          = true;
//                 type->var.is_type_class_var = context_list->is_type_class_var;
//                 return;
//             }
//             context_list = context_list->next;
//         }
//         break;
//     }
//     case NECRO_TYPE_APP:  necro_add_constraints_to_ty_vars(infer, type->app.type1, context_list); necro_add_constraints_to_ty_vars(infer, type->app.type2, context_list); break;
//     case NECRO_TYPE_FUN:  necro_add_constraints_to_ty_vars(infer, type->fun.type1, context_list); necro_add_constraints_to_ty_vars(infer, type->fun.type2, context_list); break;
//     case NECRO_TYPE_CON:  necro_add_constraints_to_ty_vars(infer, type->con.args, context_list); break;
//     case NECRO_TYPE_LIST: necro_add_constraints_to_ty_vars(infer, type->list.item, context_list); necro_add_constraints_to_ty_vars(infer, type->list.next, context_list); break;
//     case NECRO_TYPE_FOR:  necro_infer_ast_error(infer, type, NULL, "Compiler bug: ForAll Type in add_constrants_to_ty_vars: %d", type->type); break;
//     default:              necro_infer_ast_error(infer, type, NULL, "Compiler bug: Unimplemented Type in add_constrants_to_ty_vars: %d", type->type); break;
//     }
// }

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
    // NecroType* member_type = necro_symtable_get(infer->symtable, prototype->type_class_member_varid.id)->type;
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
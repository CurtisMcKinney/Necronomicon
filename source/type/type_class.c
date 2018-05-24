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
//    * Make sure we iterate over declaration groups lists!!!
//    * wtf is this "selected_member@" naming stuff about!?!?
//    * Printing variable names gets wonky when retaining printed names across methods and instances of type class, look at Either instance of Functor for example
//    * Better unification error messaging! This is especially true when it occurs during SuperClass constraint checking. Make necro_unify return struct with either Success or Error data

#define NECRO_TYPE_CLASS_DEBUG 0
#if NECRO_TYPE_CLASS_DEBUG
#define TRACE_TYPE_CLASS(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE_CLASS(...)
#endif

//=====================================================
// Forward Declarations
//=====================================================
// NecroTypeClassContext* necro_create_type_class_context(NecroPagedArena* arena, NecroTypeClass* type_class, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next);
bool                   necro_constrain_class_variable_check(NecroInfer* infer, NecroCon type_class_name, NecroCon type_var, NecroSymbol type_sig_symbol, NecroTypeClassContext* context);
NecroType*             necro_instantiate_method_sig(NecroInfer* infer, NecroCon type_class_var, NecroType* method_type, NecroType* data_type);
// void                   necro_finish_declaring_type_class(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class);
// void                   necro_create_dictionary_instance(NecroInfer* infer, NecroTypeClassInstance* instance, NecroTypeClass* type_class);
NecroSymbol            necro_create_dictionary_name(NecroIntern* intern, NecroSymbol type_class_name);
NecroSymbol            necro_create_dictionary_arg_name(NecroIntern* intern, NecroSymbol type_class_name, NecroSymbol type_var_name);
NecroSymbol            necro_create_selector_name(NecroIntern* intern, NecroSymbol member_name, NecroSymbol dictionary_name);
NecroSymbol            necro_create_dictionary_instance_name(NecroIntern* intern, NecroSymbol type_class_name, NecroASTNode* type_class_instance_ast);

// //=====================================================
// // NecroTypeClassEnv
// //=====================================================
// NecroTypeClassEnv necro_create_type_class_env(NecroScopedSymTable* scoped_symtable, NecroRenamer* renamer)
// {
//     return (NecroTypeClassEnv)
//     {
//         .class_table     = necro_create_type_class_table(),
//         .instance_table  = necro_create_type_class_instance_table(),
//         .arena           = necro_create_paged_arena(),
//         .scoped_symtable = scoped_symtable,
//         .renamer         = renamer
//     };
// }

// void necro_destroy_type_class_env(NecroTypeClassEnv* env)
// {
//     necro_destroy_type_class_table(&env->class_table);
//     necro_destroy_type_class_instance_table(&env->instance_table);
//     necro_destroy_paged_arena(&env->arena);
// }

//=====================================================
// Type Class Declaration - Refactor
//=====================================================
// NecroTypeClass* necro_get_super_class(NecroInfer* infer, NecroCon type_class_name, NecroCon sub_class_name)
// {
//     assert(infer != NULL);
//     if (necro_is_infer_error(infer)) return false;
//     NecroTypeClass* super_class = necro_type_class_table_get(&env->class_table, type_class_name.id.id);
//     if (super_class == NULL)
//     {
//         necro_infer_ast_error(infer, NULL, NULL, "%s is not a constraint", necro_intern_get_string(infer->intern, type_class_name.symbol));
//         return NULL;
//     }
//     else if (super_class->dependency_flag == 1)
//     {
//         necro_infer_ast_error(infer, NULL, NULL, "Superclass cycle for classes \'%s\' and \'%s\'", necro_intern_get_string(infer->intern, sub_class_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
//         return NULL;
//     }
//     return super_class;
// }

// void necro_dependency_analyze_type_class_declarations(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class);

// void necro_declare_type_classes(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
// {
//     assert(infer != NULL);
//     assert(top_level_declarations != NULL);
//     assert(top_level_declarations->type == NECRO_AST_TOP_DECL);
//     if (necro_is_infer_error(infer)) return;

//     //---------------------------------------------------------------
//     // Pass 1, creating initial struct and insert into tables
//     NecroNode* current_decl = top_level_declarations;
//     while (current_decl != NULL)
//     {
//         assert(current_decl->type == NECRO_AST_TOP_DECL);
//         if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_DECLARATION)
//         {
//             // Create type class
//             NecroNode*      type_class_ast = current_decl->top_declaration.declaration;
//             NecroTypeClass* type_class     = necro_type_class_table_insert(&env->class_table, type_class_ast->type_class_declaration.tycls->conid.id.id, NULL);
//             type_class->type_class_name    = (NecroCon) { .symbol = type_class_ast->type_class_declaration.tycls->conid.symbol, .id = type_class_ast->type_class_declaration.tycls->conid.id };
//             type_class->members            = NULL;
//             type_class->type_var           = (NecroCon) { .symbol = type_class_ast->type_class_declaration.tyvar->variable.symbol, .id = type_class_ast->type_class_declaration.tyvar->variable.id };
//             type_class->context            = NULL;
//             type_class->dependency_flag    = 0;
//             type_class->ast                = type_class_ast;
//             type_class->dictionary_name    = necro_create_dictionary_name(infer->intern, type_class->type_class_name.symbol);

//             // Create type_var for type_class
//             NecroType* ty_var              = necro_create_type_var(infer, (NecroVar) { .id = type_class->type_var.id, .symbol = type_class_ast->type_class_declaration.tyvar->variable.symbol});
//             // ty_var->var.is_rigid           = false;
//             ty_var->var.is_rigid           = true;
//             ty_var->var.is_type_class_var  = true;
//             type_class->type               = necro_make_con_1(infer, type_class->type_class_name, necro_create_type_list(infer, ty_var, NULL));
//             type_class->type->con.is_class = true;
//             type_class->context            = necro_ast_to_context(infer, env, type_class_ast->type_class_declaration.context);
//             if (necro_is_infer_error(infer)) return;
//             necro_symtable_get(infer->symtable, type_class->type_var.id)->type = ty_var;
//         }
//         if (necro_is_infer_error(infer)) return;
//         current_decl = current_decl->top_declaration.next_top_decl;
//     }
//     if (necro_is_infer_error(infer)) return;

//     //---------------------------------------------------------------
//     // Pass 2, Dependency Analyze and Finish
//     current_decl = top_level_declarations;
//     while (current_decl != NULL)
//     {
//         assert(current_decl->type == NECRO_AST_TOP_DECL);
//         if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_DECLARATION)
//         {
//             // Retrieve type class
//             NecroNode*      type_class_ast = current_decl->top_declaration.declaration;
//             NecroTypeClass* type_class     = necro_type_class_table_get(&env->class_table, type_class_ast->type_class_declaration.tycls->conid.id.id);
//             necro_dependency_analyze_type_class_declarations(infer, env, type_class);
//         }
//         if (necro_is_infer_error(infer)) return;
//         current_decl = current_decl->top_declaration.next_top_decl;
//     }
//     if (necro_is_infer_error(infer)) return;
// }

// void necro_dependency_analyze_type_class_declarations(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class)
// {
//     assert(infer != NULL);
//     assert(env != NULL);
//     assert(type_class != NULL);
//     if (necro_is_infer_error(infer)) return;
//     if (type_class->dependency_flag == 2) return;
//     assert(type_class->dependency_flag != 1);

//     // Set Flag to (adding super classes)
//     type_class->dependency_flag = 1;

//     // Iterate through super classes
//     NecroTypeClassContext* context = type_class->context;
//     while (context != NULL)
//     {
//         NecroTypeClass* super_class = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
//         if (necro_is_infer_error(infer)) return;
//         if (super_class->dependency_flag == 2)
//         {
//             context = context->next;
//             continue;
//         }
//         if (necro_is_infer_error(infer)) return;
//         necro_dependency_analyze_type_class_declarations(infer, env, super_class);
//         if (necro_is_infer_error(infer)) return;
//         context = context->next;
//     }

//     necro_finish_declaring_type_class(infer, env, type_class);
//     type_class->dependency_flag = 2;
// }

// void necro_finish_declaring_type_class(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class)
// {
//     assert(infer != NULL);
//     assert(env != NULL);
//     assert(type_class != NULL);
//     if (necro_is_infer_error(infer)) return;

//     NecroType* type_class_var   = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
//     type_class_var->var.context = necro_create_type_class_context(&infer->arena, type_class, type_class->type_class_name, type_class->type_var, type_class->context);
//     necro_kind_infer(infer, type_class_var, type_class_var, "While declaring a type class: ");

//     NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, type_class, type_class->type_class_name, type_class->type_var, NULL);

//     //--------------------------------
//     // Build Member Type Signatures
//     NecroNode* declarations = type_class->ast->type_class_declaration.declarations;
//     while (declarations != NULL)
//     {
//         if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
//             continue;

//         NecroType* type_sig = necro_ast_to_type_sig_go(infer, declarations->declaration.declaration_impl->type_signature.type);

//         if (necro_is_infer_error(infer)) return;
//         NecroTypeClassContext* method_context = necro_ast_to_context(infer, env, declarations->declaration.declaration_impl->type_signature.context);
//         if (necro_is_infer_error(infer)) return;
//         if (necro_constrain_class_variable_check(infer, type_class->type_class_name, type_class->type_var, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, method_context)) return;
//         NecroTypeClassContext* context        = necro_union_contexts(infer, method_context, class_context);
//         if (necro_ambiguous_type_class_check(infer, declarations->declaration.declaration_impl->type_signature.var->variable.symbol, context, type_sig)) return;
//         necro_apply_constraints(infer, type_sig, context);
//         type_sig->pre_supplied = true;
//         type_sig->source_loc   = declarations->declaration.declaration_impl->source_loc;
//         type_sig               = necro_gen(infer, type_sig, NULL);
//         necro_kind_infer(infer, type_sig, type_sig, "While declaring a method of a type class: ");
//         necro_kind_gen(infer, type_sig->type_kind);
//         necro_kind_unify(infer, type_sig->type_kind, infer->star_type_kind, NULL, type_sig, "While declaring a method of a type class");
//         if (necro_is_infer_error(infer)) return;
//         necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type              = type_sig;
//         necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->method_type_class = type_class;

//         // kind check for type context!
//         NecroTypeClassContext* curr_context = context;
//         while (curr_context != NULL)
//         {
//             NecroTypeClass* type_class     = necro_type_class_table_get(&infer->type_class_env->class_table, curr_context->type_class_name.id.id);
//             NecroType*      type_class_var = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
//             NecroType*      var_type       = necro_symtable_get(infer->symtable, curr_context->type_var.id)->type;
//             necro_kind_unify(infer, var_type->type_kind, type_class_var->type_kind, declarations->declaration.declaration_impl->scope, type_sig, "While inferring the type of a method type signature");
//             if (necro_is_infer_error(infer)) return;
//             curr_context = curr_context->next;
//         }

//         NecroTypeClassMember* new_member  = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
//         // type_class->members               = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassMember));
//         new_member->member_varid = (NecroCon) { .symbol = declarations->declaration.declaration_impl->type_signature.var->variable.symbol, .id = declarations->declaration.declaration_impl->type_signature.var->variable.id };
//         new_member->next         = NULL;
//         if (type_class->members == NULL)
//         {
//             type_class->members = new_member;
//         }
//         else
//         {
//             NecroTypeClassMember* curr_member = type_class->members;
//             while (curr_member != NULL)
//             {
//                 if (curr_member->next == NULL)
//                 {
//                     curr_member->next = new_member;
//                     break;
//                 }
//                 curr_member = curr_member->next;
//             }
//         }
//         // prev_member->next                 =
//         // type_class->members->next         = prev_member;
//         declarations = declarations->declaration.next_declaration;
//     }
//     TRACE_TYPE_CLASS("Finishing type_class: %s\n", necro_intern_get_string(infer->intern, type_class->type_class_name.symbol));

//     //---------------------------
//     // Build dictionary declarations
//     // NecroASTNode* dictionary_declaration_ast = type_class->ast->type_class_declaration.dictionary_data_declaration;
//     necro_create_dictionary_data_declaration(&infer->arena, env->scoped_symtable->global_table->intern, type_class->ast);



//     //---------------------------
//     // Infer dictionary data declaration types
//     NecroDeclarationGroup* dictionary_declaration_group = NULL;
//     NecroASTNode*          dictionary_declarations      = type_class->ast->type_class_declaration.dictionary_data_declaration;
//     while (dictionary_declarations != NULL)
//     {

//         //---------------------------
//         // Build scopes and rename
//         necro_build_scopes_go(env->scoped_symtable, dictionary_declarations->top_declaration.declaration);
//         infer->error = env->scoped_symtable->error;
//         if (necro_is_infer_error(infer)) return;
//         necro_rename_declare_pass(env->renamer, &infer->arena, dictionary_declarations->top_declaration.declaration);
//         infer->error = env->renamer->error;
//         if (necro_is_infer_error(infer)) return;
//         necro_rename_var_pass(env->renamer, &infer->arena, dictionary_declarations->top_declaration.declaration);
//         infer->error = env->renamer->error;
//         if (necro_is_infer_error(infer)) return;

//         dictionary_declaration_group = necro_append_declaration_group(&infer->arena, dictionary_declarations->top_declaration.declaration, dictionary_declaration_group);
//         dictionary_declarations = dictionary_declarations->top_declaration.next_top_decl;
//     }
//     necro_infer_assignment(infer, dictionary_declaration_group);
//     if (necro_is_infer_error(infer)) return;

//     // Generalize type class kind
//     type_class_var->type_kind = necro_kind_gen(infer, type_class_var->type_kind);

// }

//=====================================================
// Type Class Instances
//=====================================================
// void necro_dependency_analyze_type_class_instances(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClassInstance* instance);
// void necro_finish_declaring_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class, NecroTypeClassInstance* instance);
void necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context);
void necro_apply_constraints_with_class_context(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context, NecroTypeClassContext* class_context);

// NecroTypeClassInstance* necro_get_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroCon type_class_name, NecroCon data_type_name)
// {
//     assert(infer != NULL);
//     assert(env != NULL);
//     if (necro_is_infer_error(infer)) return false;
//     assert(necro_symtable_get(infer->symtable, data_type_name.id)->type != NULL);
//     uint64_t                key      = necro_create_instance_key(data_type_name, type_class_name);
//     NecroTypeClassInstance* instance = necro_type_class_instance_table_get(&env->instance_table, key);
//     if (instance == NULL)
//     {
//         necro_infer_ast_error(infer, NULL, NULL, "%s is not an instance of %s", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
//         return NULL;
//     }
//     return instance;
// }

// void necro_type_class_instances_pass1(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
// {
//     assert(infer != NULL);
//     assert(top_level_declarations != NULL);
//     assert(top_level_declarations->type == NECRO_AST_TOP_DECL);
//     if (necro_is_infer_error(infer)) return;

//     //---------------------------------------------------------------
//     // Pass 1, creating initial struct and insert into tables
//     NecroNode* current_decl = top_level_declarations;
//     while (current_decl != NULL)
//     {
//         assert(current_decl->type == NECRO_AST_TOP_DECL);
//         if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_INSTANCE)
//         {
//             NecroNode* ast             = current_decl->top_declaration.declaration;
//             NecroCon   type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
//             NecroCon   data_type_name;
//             if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
//                 data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
//             else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
//                 data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
//             else
//                 assert(false);

//             if (necro_symtable_get(infer->symtable, data_type_name.id)->type == NULL)
//             {
//                 necro_infer_ast_error(infer, NULL, ast, "No type named \'%s\', in instance declaration for class \'%s\'", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
//                 return;
//             }

//             uint64_t key = necro_create_instance_key(data_type_name, type_class_name);
//             if (necro_type_class_instance_table_get(&env->instance_table, key) != NULL)
//             {
//                 necro_infer_ast_error(infer, NULL, ast, "Duplicate Type Class Instance\n     class: %s\n     type: %s", necro_intern_get_string(infer->intern, type_class_name.symbol), necro_intern_get_string(infer->intern, data_type_name.symbol));
//                 return;
//             }

//             NecroTypeClassInstance* instance   = necro_type_class_instance_table_insert(&env->instance_table, key, NULL);
//             instance->type_class_name          = type_class_name;
//             instance->data_type_name           = data_type_name;
//             instance->context                  = necro_ast_to_context(infer, env, ast->type_class_instance.context);
//             if (necro_is_infer_error(infer)) return;
//             instance->dictionary_prototype     = NULL;
//             instance->ast                      = ast;
//             instance->dependency_flag          = 0;
//             instance->data_type                = NULL;
//             instance->dictionary_instance_name = necro_create_dictionary_instance_name(infer->intern, type_class_name.symbol, ast);
//         }
//         if (necro_is_infer_error(infer)) return;
//         current_decl = current_decl->top_declaration.next_top_decl;
//     }
// }

// void necro_type_class_instances_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations)
// {
//     if (necro_is_infer_error(infer)) return;
//     //---------------------------------------------------------------
//     // Pass 2, Dependency Analyze and Finish
//     NecroNode* current_decl = top_level_declarations;
//     while (current_decl != NULL)
//     {
//         assert(current_decl->type == NECRO_AST_TOP_DECL);
//         if (current_decl->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_INSTANCE)
//         {
//             // Retrieve type class instance
//             NecroNode* ast             = current_decl->top_declaration.declaration;
//             NecroCon   type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
//             NecroCon   data_type_name;
//             if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
//                 data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
//             else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
//                 data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
//             else
//                 assert(false);
//             // TODO: Removing for now
//             // NecroTypeClassInstance* instance = necro_get_type_class_instance(infer, env, type_class_name, data_type_name);
//             if (necro_is_infer_error(infer)) return;
//             // necro_dependency_analyze_type_class_instances(infer, env, instance);
//         }
//         if (necro_is_infer_error(infer)) return;
//         current_decl = current_decl->top_declaration.next_top_decl;
//     }
//     if (necro_is_infer_error(infer)) return;
// }

// void necro_dependency_analyze_type_class_instances(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClassInstance* instance)
// {
//     assert(infer != NULL);
//     assert(env != NULL);
//     assert(instance != NULL);
//     if (necro_is_infer_error(infer)) return;
//     if (instance->dependency_flag == 2) return;
//     assert(instance->dependency_flag != 1);

//     // Set Flag to (adding super classes)
//     instance->dependency_flag = 1;

//     NecroTypeClass* type_class = necro_type_class_table_get(&env->class_table, instance->type_class_name.id.id);
//     if (type_class == NULL)
//     {
//         necro_infer_ast_error(infer, NULL, instance->ast, "%s cannot be an instance of %s, because %s is not a type class",
//             necro_intern_get_string(infer->intern, instance->data_type_name.symbol),
//             necro_intern_get_string(infer->intern, instance->type_class_name.symbol),
//             necro_intern_get_string(infer->intern, instance->type_class_name.symbol));
//         return;
//     }

//     // Iterate through super classes
//     NecroTypeClassContext* context = type_class->context;
//     while (context != NULL)
//     {
//         NecroTypeClass*          super_class    = necro_get_super_class(infer, env, context->type_class_name, type_class->type_class_name);
//         if (necro_is_infer_error(infer)) return;
//         NecroTypeClassInstance*  super_instance = necro_get_type_class_instance(infer, env, super_class->type_class_name, instance->data_type_name);
//         if (necro_is_infer_error(infer)) return;
//         if (super_instance->dependency_flag == 2)
//         {
//             context = context->next;
//             continue;
//         }
//         if (necro_is_infer_error(infer)) return;
//         necro_dependency_analyze_type_class_instances(infer, env, super_instance);
//         if (necro_is_infer_error(infer)) return;
//         context = context->next;
//     }

//     necro_finish_declaring_type_class_instance(infer, env, type_class, instance);
//     instance->dependency_flag = 2;
// }

// void necro_finish_declaring_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroTypeClass* type_class, NecroTypeClassInstance* instance)
// {
//     assert(infer != NULL);
//     assert(env != NULL);
//     assert(instance != NULL);
//     if (necro_is_infer_error(infer)) return;
//     TRACE_TYPE_CLASS("INSTANCE: %s %s\n", necro_intern_get_string(infer->intern, instance->type_class_name.symbol), necro_intern_get_string(infer->intern, instance->data_type_name.symbol));

//     instance->data_type = necro_ast_to_type_sig_go(infer, instance->ast->type_class_instance.inst);

//     if (necro_is_infer_error(infer)) return;
//     NecroType* class_var  = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
//     assert(class_var != NULL);
//     // necro_infer_kind(infer, instance->data_type, class_var->kind, instance->data_type, "While creating an instance of a type class: ");
//     necro_kind_infer(infer, instance->data_type, instance->data_type, "While creating an instance of a type class");
//     necro_kind_unify(infer, instance->data_type->type_kind, class_var->type_kind, NULL, instance->data_type, "While creating an instance of a type class");
//     if (necro_is_infer_error(infer)) return;

//     // Apply constraints
//     necro_apply_constraints(infer, instance->data_type, instance->context);
//     instance->data_type = necro_gen(infer, instance->data_type, NULL);

//     NecroType* inst_data_type = necro_inst(infer, instance->data_type, NULL);
//     // necro_infer_kind(infer, inst_data_type, NULL, inst_data_type, "While creating an instance method: ");
//     // necro_kind_infer(infer, inst_data_type, inst_data_type, "While creating an instance method: ");

//     //--------------------------------
//     // Dictionary Prototype
//     NecroNode* declarations = instance->ast->type_class_instance.declarations;
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
//             necro_infer_ast_error(infer, NULL, instance->ast, "\'%s\' is not a (visible) method of class \'%s\'", necro_intern_get_string(infer->intern, member_symbol), necro_intern_get_string(infer->intern, instance->type_class_name.symbol));
//             return;
//         }

//         //--------------------------------
//         // Assemble types for overloaded methods
//         assert(members != NULL);
//         assert(necro_symtable_get(infer->symtable, members->member_varid.id) != NULL);
//         NecroType* method_type      = necro_symtable_get(infer->symtable, members->member_varid.id)->type;
//         NecroType* inst_method_type = necro_instantiate_method_sig(infer, type_class->type_var, method_type, inst_data_type);
//         necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = necro_inst(infer, inst_method_type, NULL);
//         necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type = necro_gen(infer, necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type, NULL);
//         necro_symtable_get(infer->symtable, instance->dictionary_prototype->prototype_varid.id)->type->pre_supplied = true;

//         //--------------------------------
//         // next
//         instance->dictionary_prototype->next = prev_dictionary;
//         declarations = declarations->declaration.next_declaration;
//     }

//     //--------------------------------
//     // Infer declarations with types in symtable
//     NecroDeclarationGroup* declaration_group = NULL;
//     declarations = instance->ast->type_class_instance.declarations;
//     while (declarations != NULL)
//     {
//         declaration_group = necro_append_declaration_group(&infer->arena, declarations->declaration.declaration_impl, declaration_group);
//         declarations = declarations->declaration.next_declaration;
//     }
//     if (declaration_group != NULL)
//         necro_infer_assignment(infer, declaration_group);

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
//             necro_infer_ast_error(infer, NULL, instance->ast, "No explicit implementation for \'%s\' in the instance declaration for \'%s %s\'",
//                 necro_intern_get_string(infer->intern, type_class_members->member_varid.symbol),
//                 necro_intern_get_string(infer->intern, type_class->type_class_name.symbol),
//                 necro_intern_get_string(infer->intern, instance->data_type_name.symbol));
//         }
//         type_class_members = type_class_members->next;
//     }

//     //--------------------------------
//     // Create Dictionary Instance
//     necro_create_dictionary_instance(infer, instance, type_class);
// }

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
                NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, context->type_class, context->type_class_name, context->type_var, NULL);
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

void necro_apply_constraints_with_class_context(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context, NecroTypeClassContext* class_context)
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
            if (type->var.var.id.id == context->type_var.id.id && context->type_class != class_context->type_class)
            {
                NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, context->type_class, context->type_class_name, context->type_var, NULL);
                type->var.context = necro_union_contexts(infer, type->var.context, class_context);
                if (necro_is_infer_error(infer)) return;
                // type->var.is_type_class_var = context_list->is_type_class_var;
            }
            context = context->next;
        }
        // Strip class context and move it to the front!
        if (type->var.var.id.id == class_context->type_var.id.id)
        {
            NecroTypeClassContext* prev_context = NULL;
            NecroTypeClassContext* var_context  = type->var.context;
            while (var_context != NULL)
            {
                if (var_context->type_class == class_context->type_class)
                {
                    if (prev_context)
                    {
                        prev_context->next = var_context->next;
                    }
                }
                else
                {
                    prev_context = var_context;
                }
                var_context  = var_context->next;
            }
            type->var.context = necro_create_type_class_context(&infer->arena, class_context->type_class, class_context->type_class_name, class_context->type_var, type->var.context);
        }
        break;
    }
    case NECRO_TYPE_APP:  necro_apply_constraints_with_class_context(infer, type->app.type1, context, class_context); necro_apply_constraints_with_class_context(infer, type->app.type2, context, class_context); break;
    case NECRO_TYPE_FUN:  necro_apply_constraints_with_class_context(infer, type->fun.type1, context, class_context); necro_apply_constraints_with_class_context(infer, type->fun.type2, context, class_context); break;
    case NECRO_TYPE_CON:  necro_apply_constraints_with_class_context(infer, type->con.args,  context, class_context); break;
    case NECRO_TYPE_LIST: necro_apply_constraints_with_class_context(infer, type->list.item, context, class_context); necro_apply_constraints_with_class_context(infer, type->list.next, context, class_context); break;
    case NECRO_TYPE_FOR:  necro_infer_ast_error(infer, type, NULL, "Compiler bug: ForAll Type in necro_apply_constraints_with_class_context: %d", type->type); break;
    default:              necro_infer_ast_error(infer, type, NULL, "Compiler bug: Unimplemented Type in necro_apply_constraints_with_class_context: %d", type->type); break;
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
        // if (type1 != method_type->app.type1 || type2 != method_type->app.type2)
            return necro_create_type_app(infer, type1, type2);
        // else
        //     return method_type;
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* type1 = necro_instantiate_method_sig_go(infer, type_class_var, method_type->fun.type1, data_type);
        NecroType* type2 = necro_instantiate_method_sig_go(infer, type_class_var, method_type->fun.type2, data_type);
        // if (type1 != method_type->fun.type1 || type2 != method_type->fun.type2)
            return necro_create_type_fun(infer, type1, type2);
        // else
        //     return method_type;
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* item = necro_instantiate_method_sig_go(infer, type_class_var, method_type->list.item, data_type);
        NecroType* next = necro_instantiate_method_sig_go(infer, type_class_var, method_type->list.next, data_type);
        // if (item != method_type->list.item || next != method_type->list.next)
            return necro_create_type_list(infer, item, next);
        // else
        //     return method_type;
    }
    case NECRO_TYPE_CON:
    {
        NecroType* args = necro_instantiate_method_sig_go(infer, type_class_var, method_type->con.args, data_type);
        // if (args != method_type->con.args)
            return necro_create_type_con(infer, method_type->con.con, args, method_type->con.arity);
        // else
        //     return method_type;
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

// NecroTypeClassInstance* necro_get_instance(NecroInfer* infer, NecroCon data_type_name, NecroCon type_class_name)
// {
//     NecroTypeClass* type_class = necro_type_class_table_get(&infer->type_class_env->class_table, type_class_name.id.id);
//     if (type_class == NULL)
//     {
//         necro_infer_ast_error(infer, NULL, NULL, "\'%s\' is not a type class", necro_intern_get_string(infer->intern, type_class_name.symbol));
//         return false;
//     }
//     uint64_t key = necro_create_instance_key(data_type_name, type_class_name);
//     return necro_type_class_instance_table_get(&infer->type_class_env->instance_table, key);
// }

//=====================================================
// Contexts
//=====================================================
bool necro_context_contains_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class)
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

bool necro_context_and_super_classes_contain_class(NecroTypeClassContext* context, NecroTypeClassContext* type_class)
{
    while (context != NULL)
    {
        if (context->type_class_name.id.id == type_class->type_class_name.id.id)
        {
            return true;
        }
        // NecroTypeClassContext* super_classes = necro_type_class_table_get(&env->class_table, context->type_class_name.id.id)->context;
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
    if (maybe_subclass_context->type_var.id.id != maybe_super_class_context->type_var.id.id)
        return false;
    // NecroTypeClassContext* super_classes = necro_type_class_table_get(&env->class_table, maybe_subclass_context->type_class_name.id.id)->context;
    NecroTypeClassContext* super_classes = maybe_subclass_context->type_class->context;
    return necro_context_and_super_classes_contain_class(super_classes, maybe_super_class_context);
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

NecroTypeClassContext* necro_union_contexts_to_same_var(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2, NecroCon type_var)
{
    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        if (!necro_context_contains_class(head, context1))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(&infer->arena, context1->type_class, context1->type_class_name, type_var, NULL);
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
            NecroTypeClassContext* new_context = necro_create_type_class_context(&infer->arena, context2->type_class, context2->type_class_name, type_var, NULL);
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

NecroTypeClassContext* necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2)
{
    assert(infer != NULL);

    NecroTypeClassContext* head = NULL;
    NecroTypeClassContext* curr = NULL;

    // Copy context1
    while (context1 != NULL)
    {
        assert(context1->type_class != NULL);
        if (!necro_context_contains_class_and_type_var(head, context1, context1->type_var))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(&infer->arena, context1->type_class, context1->type_class_name, context1->type_var, NULL);
            // NecroTypeClassContext* new_context = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassContext));
            // new_context->type_class_name       = context1->type_class_name;
            // new_context->type_var              = context1->type_var;
            // new_context->type_class            = context1->type_class;
            // new_context->next                  = NULL;
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
        if (!necro_context_contains_class_and_type_var(head, context2, context2->type_var))
        {
            NecroTypeClassContext* new_context = necro_create_type_class_context(&infer->arena, context2->type_class, context2->type_class_name, context2->type_var, NULL);
            // NecroTypeClassContext* new_context = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassContext));
            // new_context->type_class_name       = context2->type_class_name;
            // new_context->type_var              = context2->type_var;
            // new_context->type_class            = context2->type_class;
            // new_context->next                  = NULL;
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

NecroTypeClassContext* necro_create_type_class_context(NecroPagedArena* arena, NecroTypeClass* type_class, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next)
{
    NecroTypeClassContext* context = necro_paged_arena_alloc(arena, sizeof(NecroTypeClassContext));
    context->type_class_name       = type_class_name;
    context->type_var              = type_var;
    context->type_class            = type_class;
    context->next                  = next;
    return context;
}

// NecroTypeClassContext* necro_ast_to_context(NecroInfer* infer, NecroNode* context_ast)
// {
//     NecroTypeClassContext* context      = NULL;
//     NecroTypeClassContext* context_head = NULL;
//     while (context_ast != NULL)
//     {
//         // TRACE_TYPE_CLASS("context type: %d\n", contexts->type);
//         if (context_ast->type == NECRO_AST_LIST_NODE)
//         {
//             assert(context_ast->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
//             NecroTypeClassContext* new_context = necro_create_type_class_context(
//                 &infer->arena,
//                 necro_symtable_get(infer->symtable, context_ast->list.item->type_class_context.conid->conid.id)->type_class,
//                 (NecroCon) { .symbol = context_ast->list.item->type_class_context.conid->conid.symbol,    .id = context_ast->list.item->type_class_context.conid->conid.id },
//                 (NecroCon) { .symbol = context_ast->list.item->type_class_context.varid->variable.symbol, .id = context_ast->list.item->type_class_context.varid->variable.id },
//                 NULL
//             );
//             if (context_head == NULL)
//             {
//                 context      = new_context;
//                 context_head = context;
//             }
//             else
//             {
//                 context->next = new_context;
//                 context       = context->next;
//             }
//             if (necro_type_class_table_get(&env->class_table, context->type_class_name.id.id) == NULL)
//             {
//                 necro_infer_ast_error(infer, NULL, NULL, "\'%s\' is not a type class", necro_intern_get_string(infer->intern, context->type_class_name.symbol));
//                 return NULL;
//             }
//             context_ast = context_ast->list.next_item;
//         }
//         else if (context_ast->type == NECRO_AST_TYPE_CLASS_CONTEXT)
//         {
//             context = necro_create_type_class_context(
//                 &infer->arena,
//                 necro_symtable_get(infer->symtable, context_ast->type_class_context.conid->conid.id)->type_class,
//                 (NecroCon) { .symbol = context_ast->type_class_context.conid->conid.symbol,    .id = context_ast->type_class_context.conid->conid.id },
//                 (NecroCon) { .symbol = context_ast->type_class_context.varid->variable.symbol, .id = context_ast->type_class_context.varid->variable.id },
//                 NULL
//             );
//             if (necro_type_class_table_get(&env->class_table, context->type_class_name.id.id) == NULL)
//             {
//                 necro_infer_ast_error(infer, NULL, NULL, "\'%s\' is not a type class", necro_intern_get_string(infer->intern, context->type_class_name.symbol));
//                 return NULL;
//             }
//             return context;
//         }
//         else
//         {
//             assert(false);
//         }
//     }
//     return context_head;
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

bool necro_constrain_class_variable_check(NecroInfer* infer, NecroCon type_class_name, NecroCon type_var, NecroSymbol type_sig_symbol, NecroTypeClassContext* context)
{
    assert(infer != NULL);
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

    // Print dictionary declaration ast
    // printf("dictionary declaration: \n\n");
    // necro_print_reified_ast_node(type_class->ast->type_class_declaration.dictionary_data_declaration, intern);
    // puts("");
    // print_white_space(white_count);

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

    // Print dictionary ast
    print_white_space(white_count + 4);
    printf("dictionary: \n\n");
    necro_print_reified_ast_node(instance->ast->type_class_instance.dictionary_instance, intern);
    puts("");

    print_white_space(white_count);
    printf("}\n");
}

void necro_print_type_class_instance_go(NecroInfer* infer, NecroTypeClassInstance* instance)
{
    necro_print_instance(instance, infer, infer->intern, 4);
}

void necro_print_type_classes(NecroInfer* infer)
{
    printf("\nTypeClasses\n{\n");
    for (size_t i = 0; i < infer->symtable->count; ++i)
    {
        if (infer->symtable->data[i].type_class != NULL)
            necro_print_type_class(infer->symtable->data[i].type_class, infer, infer->intern, 4);
    }
    printf("}\n");
    printf("\nInstances\n{\n");
    for (size_t i = 0; i < infer->symtable->count; ++i)
    {
        if (infer->symtable->data[i].type_class_instance != NULL)
            necro_print_instance(infer->symtable->data[i].type_class_instance, infer, infer->intern, 4);
    }
    printf("}\n");
}

// void necro_print_type_class_env(NecroTypeClassEnv* env, NecroInfer* infer, NecroIntern* intern)
// {
//     NecroPrintTypeClassArgs args = { infer, intern };

//     printf("\nTypeClasses\n{\n");
//     necro_type_class_table_iterate(&env->class_table, &necro_print_type_class_go, &args);
//     printf("}\n");

//     printf("\nInstances\n{\n");
//     necro_type_class_instance_table_iterate(&env->instance_table, &necro_print_type_class_instance_go, &args);
//     printf("}\n");

// }

//=====================================================
// Dictionary Translation
//=====================================================
NecroSymbol necro_create_dictionary_name(NecroIntern* intern, NecroSymbol type_class_name)
{
    return necro_intern_concat_symbols(intern, type_class_name, necro_intern_string(intern, "@Dictionary"));
}

NecroSymbol necro_create_dictionary_arg_name(NecroIntern* intern, NecroSymbol type_class_name, NecroSymbol type_var_name)
{
    NecroSymbol dictionary_arg_name = necro_intern_concat_symbols(intern, necro_intern_string(intern, "dictionaryArg@"), type_class_name);
    dictionary_arg_name = necro_intern_concat_symbols(intern, dictionary_arg_name, necro_intern_string(intern, "@"));
    dictionary_arg_name = necro_intern_concat_symbols(intern, dictionary_arg_name, type_var_name);
    return dictionary_arg_name;
}

NecroSymbol necro_create_selector_name(NecroIntern* intern, NecroSymbol member_name, NecroSymbol dictionary_name)
{
    NecroSymbol   select_symbol = necro_intern_string(intern, "select@");
    NecroSymbol   at_symbol     = necro_intern_string(intern, "@");
    NecroSymbol   selector_name = necro_intern_concat_symbols(intern, select_symbol, member_name);
    selector_name = necro_intern_concat_symbols(intern, selector_name, at_symbol);
    selector_name = necro_intern_concat_symbols(intern, selector_name, dictionary_name);
    return selector_name;
}

NecroSymbol necro_create_dictionary_instance_name(NecroIntern* intern, NecroSymbol type_class_name, NecroASTNode* type_class_instance_ast)
{
    NecroSymbol dictionary_name          = necro_create_dictionary_name(intern, type_class_name);
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

NecroASTNode* necro_create_selector(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast, NecroSymbol dictionary_name, void* member_to_select, NecroSymbol selector_name)
{
    NecroASTNode* pat_var  = necro_create_variable_ast(arena, intern, "selected_member@", NECRO_VAR_DECLARATION);
    NecroASTNode* rhs_var  = necro_create_variable_ast(arena, intern, "selected_member@", NECRO_VAR_VAR);
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
    NecroASTNode* selector_rhs  = necro_create_rhs_ast(arena, rhs_var, NULL);
    NecroASTNode* selector_ast  = necro_create_apats_assignment_ast(arena, intern, necro_intern_get_string(intern, selector_name), selector_args, selector_rhs);
    // puts("--");
    // necro_print_reified_ast_node(selector_ast, intern);
    return selector_ast;
}

void necro_create_dictionary_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast)
{
    NecroSymbol   dictionary_name        = necro_create_dictionary_name(intern, type_class_ast->type_class_declaration.tycls->conid.symbol);
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
        NecroSymbol super_dictionary_name = necro_create_dictionary_name(intern, context->list.item->type_class_context.conid->conid.symbol);
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

NecroASTNode* retrieveNestedDictionaryFromContext(NecroInfer* infer, NecroTypeClassContext* context, NecroTypeClassContext* dictionary_to_retrieve, NecroASTNode* ast_so_far)
{
    if (context->type_class_name.id.id == dictionary_to_retrieve->type_class_name.id.id)
    {
        assert(ast_so_far != NULL);
        return ast_so_far;
    }
    else
    {
        // NecroTypeClassContext* super_classes = necro_type_class_table_get(&env->class_table, context->type_class_name.id.id)->context;
        NecroTypeClassContext* super_classes = context->type_class->context;
        while (super_classes != NULL)
        {
            // NOTE / HACK: Instead of allocating more memory every iterations to get isolated versions of each super class we just temporarily set the next pointer to NULL. Hackey, but it works.
            NecroTypeClassContext* super_next = super_classes->next;
            super_classes->next = NULL;
            if (necro_context_and_super_classes_contain_class(super_classes, dictionary_to_retrieve))
            {
                NecroSymbol   dictionary_name = necro_create_dictionary_name(infer->intern, context->type_class_name.symbol);
                NecroSymbol   selector_symbol = necro_create_selector_name(infer->intern, super_classes->type_class_name.symbol, dictionary_name);
                NecroASTNode* selector_var    = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, selector_symbol), NECRO_VAR_VAR);
                selector_var->scope           = infer->scoped_symtable->top_scope;
                necro_rename_var_pass(infer->renamer, &infer->arena, selector_var);
                NecroASTNode* super_ast       = retrieveNestedDictionaryFromContext(infer, super_classes, dictionary_to_retrieve, necro_create_fexpr_ast(&infer->arena, selector_var, ast_so_far));
                assert(super_ast != NULL);
                super_classes->next = super_next;
                return super_ast;
            }
            super_classes->next = super_next;
            super_classes = super_classes->next;
        }
    }
    return NULL;
}

NecroASTNode* retrieveDictionaryFromContext(NecroInfer* infer, NecroTypeClassDictionaryContext* dictionary_context, NecroTypeClassContext* dictionary_to_retrieve, NecroVar type_var)
{
    while (dictionary_context != NULL)
    {
        if (dictionary_context->type_class_name.id.id == dictionary_to_retrieve->type_class_name.id.id && dictionary_context->type_var_name.id.id == type_var.id.id)
        {
            // return necro_create_variable_ast(arena, intern, necro_intern_get_string(intern, necro_create_dictionary_name(intern, dictionary_to_retrieve->type_class_name.symbol)), NECRO_VAR_VAR);
            return dictionary_context->dictionary_arg_ast;
        }
        else if (dictionary_context->type_var_name.id.id == type_var.id.id)
        {
            NecroTypeClassContext context;
            context.type_class_name = dictionary_context->type_class_name;
            context.type_class      = necro_symtable_get(infer->symtable, dictionary_context->type_class_name.id)->type_class;
            context.type_var        = necro_var_to_con(type_var);
            context.next            = NULL;
            // NecroTypeClassContext* context_next = context->next;
            // context->next = NULL;
            if (necro_context_and_super_classes_contain_class(&context, dictionary_to_retrieve))
            {
                // NecroSymbol   d_arg_name = necro_create_dictionary_arg_name(intern, context->type_class_name.symbol, type_var_name);
                // NecroASTNode* d_arg_var  = necro_create_variable_ast(arena, intern, necro_intern_get_string(intern, d_arg_name), NECRO_VAR_VAR);
                NecroASTNode* super_ast = retrieveNestedDictionaryFromContext(infer, &context, dictionary_to_retrieve, dictionary_context->dictionary_arg_ast);
                assert(super_ast != NULL);
                // context->next = context_next;
                return super_ast;
            }
            // context->next = context_next;
        }
        dictionary_context = dictionary_context->next;
    }
    return NULL;
}

NecroASTNode* necro_resolve_context_to_dictionary(NecroInfer* infer, NecroTypeClassContext* context, NecroTypeClassDictionaryContext* dictionary_context)
{
    if (necro_is_infer_error(infer)) return NULL;
    assert(infer != NULL);
    assert(context != NULL);
    NecroType* starting_type = NULL;
    if (infer->env.capacity > context->type_var.id.id && infer->env.data[context->type_var.id.id] != NULL)
        starting_type = infer->env.data[context->type_var.id.id];
    else
        starting_type = necro_symtable_get(infer->symtable, context->type_var.id)->type;
    NecroType* var_type = necro_find(infer, starting_type);
    assert(var_type != NULL);
    switch (var_type->type)
    {
    case NECRO_TYPE_VAR:
    {
        assert(var_type->var.context != NULL);
        // return retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, context, necro_con_to_var(var_type->var.context->type_var));
        if (var_type->var.is_rigid)
        {
            // NecroASTNode* d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, var_type->var.context, var_type->var.var);
            // NecroASTNode* d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, var_type->var.context, necro_con_to_var(var_type->var.context->type_var));
            NecroASTNode* d_var = retrieveDictionaryFromContext(infer, dictionary_context, context, necro_con_to_var(var_type->var.context->type_var));
            assert(d_var != NULL);
            return d_var;
        }
        else
        {
            if (var_type->var.gen_bound == NULL)
            {
                // TODO: Defaulting
                const char* var_string = necro_id_as_character_string(infer, var_type->var.var);
                return necro_infer_error(infer, "", starting_type, "Could not deduce (%s %s). The type variable %s is ambiguous",
                    necro_intern_get_string(infer->intern, var_type->var.context->type_class_name.symbol),
                    var_string,
                    var_string);
            }
            else
            {
                assert(var_type->var.gen_bound->type == NECRO_TYPE_VAR);
                assert(var_type->var.gen_bound->var.is_rigid == true);
                // NecroASTNode* d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, var_type->var.context, var_type->var.gen_bound->var.var);
                // NecroASTNode* d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, var_type->var.context, necro_find(infer, var_type->var.gen_bound)->var.var);
                NecroASTNode* d_var = retrieveDictionaryFromContext(infer, dictionary_context, context, necro_find(infer, var_type->var.gen_bound)->var.var);
                assert(d_var != NULL);
                return d_var;
            }
        }
    }
    case NECRO_TYPE_CON:
    {
        NecroTypeClassInstance* instance     = necro_get_type_class_instance(infer, var_type->con.con.symbol, context->type_class_name.symbol);
        NecroTypeClassContext*  inst_context = NULL;
        NecroType*              inst_type    = necro_inst_with_context(infer, instance->data_type, NULL, &inst_context);
        necro_unify(infer, var_type, inst_type, NULL, inst_type, "Compiler bug during necro_resolve_context_to_dictionary");
        NecroASTNode*           d_var        = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, instance->dictionary_instance_name), NECRO_VAR_VAR);
        d_var->scope = infer->scoped_symtable->top_type_scope;
        while (inst_context != NULL)
        {
            NecroASTNode* new_d_var = necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context);
            new_d_var->scope = infer->scoped_symtable->top_type_scope;
            d_var = necro_create_fexpr_ast(&infer->arena, d_var, new_d_var);
            if (necro_is_infer_error(infer)) return NULL;
            inst_context = inst_context->next;
        }
        assert(d_var != NULL);
        return d_var;
    }
    case NECRO_TYPE_FUN:
    {
        return necro_infer_error(infer, "", var_type, "Type classes on functions are not currently supported");
    }
    default:
        assert(false);
        return NULL;
    }
    return NULL;
}

NecroASTNode* necro_resolve_method(NecroInfer* infer, NecroTypeClass* method_type_class, NecroTypeClassContext* context, NecroASTNode* ast, NecroTypeClassDictionaryContext* dictionary_context)
{
    if (necro_is_infer_error(infer)) return NULL;
    assert(infer != NULL);
    assert(method_type_class != NULL);
    assert(context != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_VARIABLE);
    NecroTypeClassContext* type_class_context = context;
    while (type_class_context != NULL && type_class_context->type_class != method_type_class)
        type_class_context = type_class_context->next;
    assert(type_class_context != NULL);
    assert(type_class_context->type_class == method_type_class);
    NecroType* starting_type = NULL;
    if (infer->env.capacity > type_class_context->type_var.id.id && infer->env.data[type_class_context->type_var.id.id] != NULL)
        starting_type = infer->env.data[type_class_context->type_var.id.id];
    else
        starting_type = necro_symtable_get(infer->symtable, type_class_context->type_var.id)->type;
    NecroType* var_type = necro_find(infer, starting_type);
    assert(var_type != NULL);
    switch (var_type->type)
    {

    case NECRO_TYPE_VAR:
    {
        assert(var_type->var.context != NULL);
        // NecroASTNode* d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, context, necro_con_to_var(var_type->var.context->type_var));
        NecroASTNode* d_var           = NULL;
        if (var_type->var.is_rigid)
        {
            d_var = retrieveDictionaryFromContext(infer, dictionary_context, type_class_context, necro_con_to_var(var_type->var.context->type_var));
            d_var->scope = ast->scope;
            if (necro_is_infer_error(infer)) return NULL;
            assert(d_var != NULL);
        }
        else
        {
            if (var_type->var.gen_bound == NULL)
            {
                // TODO: Defaulting
                const char* var_string = necro_id_as_character_string(infer, var_type->var.var);
                return necro_infer_error(infer, "", starting_type, "Could not deduce (%s %s).\n  Arising from the use of %s.\n  The type variable %s is ambiguous",
                    necro_intern_get_string(infer->intern, var_type->var.context->type_class_name.symbol),
                    var_string,
                    necro_intern_get_string(infer->intern, ast->variable.symbol),
                    var_string);
            }
            else
            {
                assert(var_type->var.gen_bound != NULL);
                assert(var_type->var.gen_bound->type == NECRO_TYPE_VAR);
                assert(var_type->var.gen_bound->var.is_rigid == true);
                // d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, var_type->var.context, var_type->var.gen_bound->var.var);
                // d_var = retrieveDictionaryFromContext(&infer->arena, infer->intern, env, dictionary_context, var_type->var.context, necro_find(infer, var_type->var.gen_bound)->var.var);
                d_var = retrieveDictionaryFromContext(infer, dictionary_context, type_class_context, necro_find(infer, var_type->var.gen_bound)->var.var);
                d_var->scope = ast->scope;
                if (necro_is_infer_error(infer)) return NULL;
                assert(d_var != NULL);
            }
        }
        NecroSymbol   dictionary_name               = necro_create_dictionary_name(infer->intern, type_class_context->type_class_name.symbol);
        NecroSymbol   selector_name                 = necro_create_selector_name(infer->intern, ast->variable.symbol, dictionary_name);
        NecroASTNode* selector_var                  = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, selector_name), NECRO_VAR_VAR);
        selector_var->scope                         = infer->scoped_symtable->top_scope;
        necro_rename_var_pass(infer->renamer, &infer->arena, selector_var);
        NecroASTNode* m_var                         = necro_create_fexpr_ast(&infer->arena, selector_var, d_var);
        m_var->scope                                = ast->scope;
        while (context != NULL)
        {
            if (context != type_class_context)
            {
                d_var = necro_resolve_context_to_dictionary(infer, context, dictionary_context);
                d_var->scope = ast->scope;
                if (necro_is_infer_error(infer)) return NULL;
                assert(d_var != NULL);
                m_var = necro_create_fexpr_ast(&infer->arena, m_var, d_var);
                m_var->scope = ast->scope;
            }
            context = context->next;
        }
        return m_var;
    }

    case NECRO_TYPE_CON:
    {
        // Resolve directly to method instance
        // uint64_t                  key       = necro_create_instance_key(var_type->con.con, context->type_class_name);
        // NecroTypeClassInstance*   instance  = necro_type_class_instance_table_get(&env->instance_table, key);
        NecroTypeClassInstance*   instance  = necro_get_type_class_instance(infer, var_type->con.con.symbol, type_class_context->type_class_name.symbol);
        NecroDictionaryPrototype* prototype = instance->dictionary_prototype;
        while (prototype != NULL)
        {
            if (prototype->type_class_member_varid.id.id == ast->variable.id.id)
            {
                NecroASTNode* m_var                         = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, prototype->prototype_varid.symbol), NECRO_VAR_VAR);
                m_var->variable.id                          = prototype->prototype_varid.id;
                m_var->scope                                = ast->scope;
                NecroType*             member_instance_type = necro_symtable_get(infer->symtable, prototype->prototype_varid.id)->type;
                NecroTypeClassContext* inst_context         = NULL;
                NecroType*             inst_type            = necro_inst_with_context(infer, member_instance_type, NULL, &inst_context);
                necro_unify(infer, ast->necro_type, inst_type, NULL, inst_type, "Compiler bug during necro_resolve_context_to_dictionary");
                if (necro_is_infer_error(infer)) return NULL;
                // NecroTypeClassContext*  inst_context = NULL;
                // NecroType*              inst_type    = necro_inst_with_context(infer, instance->data_type, NULL, &inst_context);
                // necro_unify(infer, var_type, inst_type, NULL, inst_type, "Compiler bug during necro_resolve_context_to_dictionary");
                // if (necro_is_infer_error(infer)) return NULL;
                // prototype->
                // Trying to get correct context order!!!!!
                // NecroTypeClassContext* for_all_context = type->for_all.context;
                // while (for_all_context != NULL)
                // context = context->next;
                // while (context != NULL)
                // {
                //     if (context != type_class_context)
                //     {
                //         NecroASTNode* d_var = necro_resolve_context_to_dictionary(infer, context, dictionary_context);
                //         if (necro_is_infer_error(infer)) return NULL;
                //         d_var->scope        = ast->scope;
                //         assert(d_var != NULL);
                //         m_var               = necro_create_fexpr_ast(&infer->arena, m_var, d_var);
                //     }
                //     context = context->next;
                // }
                // TODO: Look at the ordering here...is it wrong!?!?!
                while (inst_context != NULL)
                {
                    NecroASTNode* d_var = necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context);
                    if (necro_is_infer_error(infer)) return NULL;
                    d_var->scope        = ast->scope;
                    assert(d_var != NULL);
                    m_var               = necro_create_fexpr_ast(&infer->arena, m_var, d_var);
                    inst_context = inst_context->next;
                }
                assert(m_var != NULL);
                return m_var;
            }
            prototype = prototype->next;
        }
        // TODO: SHOULD THIS BE IN OR NOT!?!?!
        // I THINK SO!
        assert(false);
        return NULL;
    }

    case NECRO_TYPE_FUN:
    {
        return necro_infer_error(infer, "", var_type, "Type classes on functions are not currently supported");
    }

    default:
        assert(false);
        return NULL;
    }
    return NULL;
}

NecroTypeClassDictionaryContext* necro_create_type_class_dictionary_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var_name, NecroASTNode* dictionary_arg_ast, NecroTypeClassDictionaryContext* next)
{
    NecroTypeClassDictionaryContext* dictionary_context = necro_paged_arena_alloc(arena, sizeof(NecroTypeClassDictionaryContext));
    dictionary_context->type_class_name    = type_class_name;
    dictionary_context->type_var_name      = type_var_name;
    dictionary_context->dictionary_arg_ast = dictionary_arg_ast;
    dictionary_context->next               = next;
    return dictionary_context;
}

void necro_type_class_translate_go(NecroTypeClassDictionaryContext* dictionary_context, NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    if (necro_is_infer_error(infer)) return;
    if (ast == NULL)
        return;
    // assert(dictionary_context != NULL);
    switch (ast->type)
    {

        //=====================================================
        // Declaration type things
        //=====================================================
    case NECRO_AST_TOP_DECL:
    {
        NecroDeclarationGroupList* group_list = ast->top_declaration.group_list;
        while (group_list != NULL)
        {
            NecroDeclarationGroup* declaration_group = group_list->declaration_group;
            while (declaration_group != NULL)
            {
                necro_type_class_translate_go(dictionary_context, infer, declaration_group->declaration_ast);
                declaration_group = declaration_group->next;
            }
            group_list = group_list->next;
        }
        // NecroASTNode* curr = ast;
        // while (curr != NULL)
        // {
        //     if (curr->top_declaration.declaration->type == NECRO_AST_SIMPLE_ASSIGNMENT ||
        //         curr->top_declaration.declaration->type == NECRO_AST_APATS_ASSIGNMENT  ||
        //         curr->top_declaration.declaration->type == NECRO_AST_PAT_ASSIGNMENT    ||
        //         curr->top_declaration.declaration->type == NECRO_AST_TYPE_CLASS_INSTANCE)
        //         necro_type_class_translate_go(dictionary_context, infer, curr->top_declaration.declaration);
        //     curr = curr->top_declaration.next_top_decl;
        // }
        // Iterate Instances!
        break;
    }

    case NECRO_AST_DECL:
    {
        NecroDeclarationGroupList* group_list = ast->declaration.group_list;
        while (group_list != NULL)
        {
            NecroDeclarationGroup* declaration_group = group_list->declaration_group;
            while (declaration_group != NULL)
            {
                necro_type_class_translate_go(dictionary_context, infer, declaration_group->declaration_ast);
                declaration_group = declaration_group->next;
            }
            group_list = group_list->next;
        }
        // NecroASTNode* curr = ast;
        // while (curr != NULL)
        // {
        //     if (curr->top_declaration.declaration->type == NECRO_AST_SIMPLE_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_APATS_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_PAT_ASSIGNMENT)
        //         necro_type_class_translate_go(dictionary_context, infer, curr->top_declaration.declaration);
        //     curr = curr->declaration.next_declaration;
        // }
        break;
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
    {
        // TODO: FINISH!
        // break;
        // NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
        // NecroCon data_type_name;
        // if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
        //     data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
        // else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
        //     data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
        // else
        //     assert(false);
        // NecroTypeClassInstance*          instance               = necro_get_type_class_instance(infer, data_type_name.symbol, type_class_name.symbol);
        // NecroSymbol                      dictionary_arg_name    = necro_create_dictionary_arg_name(infer->intern, type_class_name.symbol, data_type_name.symbol);
        // NecroASTNode*                    dictionary_arg_var     = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_arg_name), NECRO_VAR_DECLARATION);
        // dictionary_arg_var->scope                               = ast->scope;
        // necro_rename_declare_pass(infer->renamer, &infer->arena, dictionary_arg_var);
        // if (necro_is_infer_error(infer)) return;
        // NecroTypeClassDictionaryContext* new_dictionary_context = necro_create_type_class_dictionary_context(&infer->arena, type_class_name, data_type_name, dictionary_arg_var, dictionary_context);
        // necro_type_class_translate_go(new_dictionary_context, infer, env, ast->type_class_instance.context);
        // necro_type_class_translate_go(new_dictionary_context, infer, env, ast->type_class_instance.qtycls);
        // necro_type_class_translate_go(new_dictionary_context, infer, env, ast->type_class_instance.inst);
        // TODO: PUT BACK
        // necro_type_class_translate_go(new_dictionary_context, infer, ast->type_class_instance.declarations);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_instance.declarations);
        break;
    }

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        // TODO: DEFAULTING!
        NecroType*                       type                   = necro_symtable_get(infer->symtable, ast->simple_assignment.id)->type;
        NecroTypeClassDictionaryContext* new_dictionary_context = dictionary_context;
        NecroASTNode*                    dictionary_args        = NULL;
        NecroASTNode*                    dictionary_args_head   = NULL;
        NecroASTNode*                    rhs = ast->simple_assignment.rhs;
        while (type->type == NECRO_TYPE_FOR)
        {
            NecroTypeClassContext* for_all_context = type->for_all.context;
            while (for_all_context != NULL)
            {
                NecroSymbol   dictionary_arg_name = necro_create_dictionary_arg_name(infer->intern, for_all_context->type_class_name.symbol, for_all_context->type_var.symbol);
                NecroASTNode* dictionary_arg_var  = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_arg_name), NECRO_VAR_DECLARATION);
                dictionary_arg_var->scope         = ast->simple_assignment.rhs->scope;
                necro_rename_declare_pass(infer->renamer, &infer->arena, dictionary_arg_var);
                if (necro_is_infer_error(infer)) return;
                new_dictionary_context = necro_create_type_class_dictionary_context(&infer->arena, for_all_context->type_class_name, for_all_context->type_var, dictionary_arg_var, new_dictionary_context);
                if (dictionary_args_head == NULL)
                {
                    dictionary_args = necro_create_apat_list_ast(&infer->arena, dictionary_arg_var, NULL);
                    dictionary_args_head = dictionary_args;
                }
                else
                {
                    dictionary_args->apats.next_apat = necro_create_apat_list_ast(&infer->arena, dictionary_arg_var, NULL);
                    dictionary_args = dictionary_args->apats.next_apat;
                }
                for_all_context = for_all_context->next;
            }
            type = type->for_all.type;
        }
        if (dictionary_args_head != NULL)
        {
            NecroASTNode* new_rhs = necro_create_wild_card_ast(&infer->arena);
            *new_rhs = *rhs;
            *rhs = *necro_create_lambda_ast(&infer->arena, dictionary_args_head, new_rhs);
        }
        necro_type_class_translate_go(new_dictionary_context, infer, rhs);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        // TODO: FINISH!
        NecroType*                       type = necro_symtable_get(infer->symtable, ast->apats_assignment.id)->type;
        NecroTypeClassDictionaryContext* new_dictionary_context = dictionary_context;
        NecroASTNode*                    dictionary_args = NULL;
        NecroASTNode*                    dictionary_args_head = NULL;
        while (type->type == NECRO_TYPE_FOR)
        {
            NecroTypeClassContext* for_all_context = type->for_all.context;
            while (for_all_context != NULL)
            {
                NecroSymbol   dictionary_arg_name = necro_create_dictionary_arg_name(infer->intern, for_all_context->type_class_name.symbol, for_all_context->type_var.symbol);
                NecroASTNode* dictionary_arg_var = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_arg_name), NECRO_VAR_DECLARATION);
                dictionary_arg_var->scope = ast->apats_assignment.rhs->scope;
                necro_rename_declare_pass(infer->renamer, &infer->arena, dictionary_arg_var);
                if (necro_is_infer_error(infer)) return;
                new_dictionary_context = necro_create_type_class_dictionary_context(&infer->arena, for_all_context->type_class_name, for_all_context->type_var, dictionary_arg_var, new_dictionary_context);
                if (dictionary_args_head == NULL)
                {
                    dictionary_args = necro_create_apat_list_ast(&infer->arena, dictionary_arg_var, NULL);
                    dictionary_args_head = dictionary_args;
                }
                else
                {
                    dictionary_args->apats.next_apat = necro_create_apat_list_ast(&infer->arena, dictionary_arg_var, NULL);
                    dictionary_args = dictionary_args->apats.next_apat;
                }
                for_all_context = for_all_context->next;
            }
            type = type->for_all.type;
        }
        if (dictionary_args_head != NULL)
        {
            dictionary_args->apats.next_apat = ast->apats_assignment.apats;
            ast->apats_assignment.apats = dictionary_args_head;
        }
        necro_type_class_translate_go(new_dictionary_context, infer, ast->apats_assignment.rhs);
        break;
    }

    case NECRO_AST_PAT_ASSIGNMENT:
        // TODO: START!
        necro_type_class_translate_go(dictionary_context, infer, ast->pat_assignment.rhs);
        break;

    case NECRO_AST_DATA_DECLARATION:
        necro_type_class_translate_go(dictionary_context, infer, ast->data_declaration.simpletype);
        necro_type_class_translate_go(dictionary_context, infer, ast->data_declaration.constructor_list);
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
        {
            NecroSymbolInfo* var_info = necro_symtable_get(infer->symtable, ast->variable.id);
            if (var_info->method_type_class != NULL)
            {
                if (necro_is_infer_error(infer)) return;
                NecroASTNode* var_ast = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, ast->variable.symbol), NECRO_VAR_VAR);
                *var_ast = *ast;
                NecroTypeClassContext* inst_context = var_ast->variable.inst_context;
                assert(inst_context != NULL);
                NecroASTNode* m_var = necro_resolve_method(infer, var_info->method_type_class, inst_context, var_ast, dictionary_context);
                if (necro_is_infer_error(infer)) return;
                assert(m_var != NULL);
                m_var->scope   = ast->scope;
                necro_rename_var_pass(infer->renamer, &infer->arena, m_var);
                if (necro_is_infer_error(infer)) return;
                *ast = *m_var;
            }
            else if (ast->variable.inst_context != NULL)
            {
                // TODO: IS THIS RIGHT?!?!?!?
                NecroASTNode* var_ast = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, ast->variable.symbol), NECRO_VAR_VAR);
                *var_ast = *ast;
                var_ast->scope = ast->scope;
                NecroTypeClassContext* inst_context = ast->variable.inst_context;
                while (inst_context != NULL)
                {
                    NecroASTNode* d_var = necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context);
                    if (necro_is_infer_error(infer)) return;
                    assert(d_var != NULL);
                    var_ast = necro_create_fexpr_ast(&infer->arena, var_ast, d_var);
                    d_var->scope   = ast->scope;
                    necro_rename_var_pass(infer->renamer, &infer->arena, d_var);
                    var_ast->scope = ast->scope;
                    inst_context = inst_context->next;
                }
                *ast = *var_ast;
            }
            break;
        }
        // TODO: split NECRO_DECLARATION into:
        // case NECRO_VAR_PATTERN?
        // case NECRO_VAR_ARG?
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
    {
        if (ast->bin_op.inst_context != NULL)
        {
            NecroASTNode* op_ast          = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, ast->bin_op.symbol), NECRO_VAR_VAR);
            op_ast->variable.inst_context = ast->bin_op.inst_context;
            op_ast->variable.id           = ast->bin_op.id;
            op_ast->variable.symbol       = ast->bin_op.symbol;
            op_ast->scope                 = ast->scope;
            // necro_build_scopes_go(env->scoped_symtable, lambda_ast);
            // necro_rename_declare_pass(env->renamer, &infer->arena, lambda_ast);
            NecroTypeClassContext* inst_context = ast->bin_op.inst_context;
            while (inst_context != NULL)
            {
                NecroASTNode* d_var = necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context);
                if (necro_is_infer_error(infer)) return;
                assert(d_var != NULL);
                op_ast        = necro_create_fexpr_ast(&infer->arena, op_ast, d_var);
                op_ast->scope = ast->scope;
                d_var->scope  = ast->scope;
                inst_context  = inst_context->next;
            }
            op_ast->scope                 = ast->scope;
            necro_rename_var_pass(infer->renamer, &infer->arena, op_ast);
            op_ast = necro_create_fexpr_ast(&infer->arena, op_ast, ast->bin_op.lhs);
            op_ast = necro_create_fexpr_ast(&infer->arena, op_ast, ast->bin_op.rhs);
            necro_type_class_translate_go(dictionary_context, infer, ast->bin_op.lhs);
            necro_type_class_translate_go(dictionary_context, infer, ast->bin_op.rhs);
            *ast = *op_ast;
        }
        else
        {
            necro_type_class_translate_go(dictionary_context, infer, ast->bin_op.lhs);
            necro_type_class_translate_go(dictionary_context, infer, ast->bin_op.rhs);
        }
        break;
    }

    case NECRO_AST_OP_LEFT_SECTION:
        if (ast->op_left_section.inst_context != NULL)
        {
            NecroASTNode* op_ast          = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, ast->op_left_section.symbol), NECRO_VAR_VAR);
            op_ast->variable.inst_context = ast->op_left_section.inst_context;
            op_ast->variable.id           = ast->op_left_section.id;
            op_ast->variable.symbol       = ast->op_left_section.symbol;
            op_ast->scope                 = ast->scope;
            NecroTypeClassContext* inst_context = ast->op_left_section.inst_context;
            while (inst_context != NULL)
            {
                NecroASTNode* d_var = necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context);
                if (necro_is_infer_error(infer)) return;
                assert(d_var != NULL);
                op_ast        = necro_create_fexpr_ast(&infer->arena, op_ast, d_var);
                op_ast->scope = ast->scope;
                d_var->scope  = ast->scope;
                inst_context  = inst_context->next;
            }
            op_ast->scope = ast->scope;
            necro_rename_var_pass(infer->renamer, &infer->arena, op_ast);
            op_ast = necro_create_fexpr_ast(&infer->arena, op_ast, ast->op_left_section.left);
            necro_type_class_translate_go(dictionary_context, infer, ast->op_left_section.left);
            *ast = *op_ast;
        }
        else
        {
            necro_type_class_translate_go(dictionary_context, infer, ast->op_left_section.left);
        }
        break;

    // TODO: Make sure we aren't duplicating names / scopes
    case NECRO_AST_OP_RIGHT_SECTION:
        if (ast->op_right_section.inst_context != NULL)
        {
            NecroASTNode* x_var_arg       = necro_create_variable_ast(&infer->arena, infer->intern, "x@rightSection", NECRO_VAR_DECLARATION);
            NecroASTNode* x_var_var       = necro_create_variable_ast(&infer->arena, infer->intern, "x@rightSection", NECRO_VAR_VAR);
            NecroASTNode* op_ast          = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, ast->op_right_section.symbol), NECRO_VAR_VAR);
            op_ast->variable.inst_context = ast->op_right_section.inst_context;
            op_ast->variable.id           = ast->op_right_section.id;
            op_ast->variable.symbol       = ast->op_right_section.symbol;
            NecroTypeClassContext* inst_context = ast->op_right_section.inst_context;
            while (inst_context != NULL)
            {
                NecroASTNode* d_var = necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context);
                if (necro_is_infer_error(infer)) return;
                assert(d_var != NULL);
                op_ast       = necro_create_fexpr_ast(&infer->arena, op_ast, d_var);
                inst_context = inst_context->next;
            }
            op_ast                   = necro_create_fexpr_ast(&infer->arena, op_ast, x_var_var);
            op_ast                   = necro_create_fexpr_ast(&infer->arena, op_ast, ast->op_right_section.right);
            NecroASTNode* lambda_ast = necro_create_lambda_ast(&infer->arena, necro_create_apat_list_ast(&infer->arena, x_var_arg, NULL), op_ast);
            infer->scoped_symtable->current_scope       = ast->scope;
            infer->scoped_symtable->current_delay_scope = ast->delay_scope;
            necro_build_scopes_go(infer->scoped_symtable, lambda_ast);
            necro_rename_declare_pass(infer->renamer, &infer->arena, lambda_ast);
            necro_rename_var_pass(infer->renamer, &infer->arena, lambda_ast);
            necro_type_class_translate_go(dictionary_context, infer, ast->op_right_section.right);
            *ast = *lambda_ast;
        }
        else
        {
            necro_type_class_translate_go(dictionary_context, infer, ast->op_right_section.right);
        }
        break;

    case NECRO_AST_IF_THEN_ELSE:
        necro_type_class_translate_go(dictionary_context, infer, ast->if_then_else.if_expr);
        necro_type_class_translate_go(dictionary_context, infer, ast->if_then_else.then_expr);
        necro_type_class_translate_go(dictionary_context, infer, ast->if_then_else.else_expr);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_type_class_translate_go(dictionary_context, infer, ast->right_hand_side.declarations);
        necro_type_class_translate_go(dictionary_context, infer, ast->right_hand_side.expression);
        break;

    case NECRO_AST_LET_EXPRESSION:
        necro_type_class_translate_go(dictionary_context, infer, ast->let_expression.declarations);
        necro_type_class_translate_go(dictionary_context, infer, ast->let_expression.expression);
        break;

    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_type_class_translate_go(dictionary_context, infer, ast->fexpression.aexp);
        necro_type_class_translate_go(dictionary_context, infer, ast->fexpression.next_fexpression);
        break;

    case NECRO_AST_APATS:
        necro_type_class_translate_go(dictionary_context, infer, ast->apats.apat);
        necro_type_class_translate_go(dictionary_context, infer, ast->apats.next_apat);
        break;

    case NECRO_AST_WILDCARD:
        break;

    case NECRO_AST_LAMBDA:
        necro_type_class_translate_go(dictionary_context, infer, ast->lambda.apats);
        necro_type_class_translate_go(dictionary_context, infer, ast->lambda.expression);
        break;

    case NECRO_AST_DO:
    {
        NecroAST_Node_Reified* bind_node = necro_create_variable_ast(&infer->arena, infer->intern, "bind", NECRO_VAR_VAR);
        NecroScope* scope = ast->scope;
        while (scope->parent != NULL)
            scope = scope->parent;
        bind_node->scope = bind_node->scope = scope;
        necro_rename_var_pass(infer->renamer, &infer->arena, bind_node);
        NecroTypeClassContext* bind_context     = necro_symtable_get(infer->symtable, bind_node->variable.id)->type->for_all.context;
        NecroTypeClassContext* monad_context    = ast->do_statement.monad_var->var.context;
        while (monad_context->type_class_name.id.id != bind_context->type_class_name.id.id)
            monad_context = monad_context->next;
        assert(monad_context->type_class_name.id.id != bind_context->type_class_name.id.id);
        NecroASTNode*          bind_method_inst = necro_resolve_method(infer, monad_context->type_class, monad_context, bind_node, dictionary_context);
        necro_print_reified_ast_node(bind_method_inst, infer->intern);
        // bind_node->do_statement.
        // TODO: FINISH
        necro_type_class_translate_go(dictionary_context, infer, ast->do_statement.statement_list);
        break;
    }

    case NECRO_AST_LIST_NODE:
        necro_type_class_translate_go(dictionary_context, infer, ast->list.item);
        necro_type_class_translate_go(dictionary_context, infer, ast->list.next_item);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        necro_type_class_translate_go(dictionary_context, infer, ast->expression_list.expressions);
        break;

    case NECRO_AST_EXPRESSION_SEQUENCE:
        necro_type_class_translate_go(dictionary_context, infer, ast->expression_sequence.expressions);
        break;

    case NECRO_AST_TUPLE:
        necro_type_class_translate_go(dictionary_context, infer, ast->tuple.expressions);
        break;

    case NECRO_BIND_ASSIGNMENT:
        necro_type_class_translate_go(dictionary_context, infer, ast->bind_assignment.expression);
        break;

    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_type_class_translate_go(dictionary_context, infer, ast->pat_bind_assignment.pat);
        necro_type_class_translate_go(dictionary_context, infer, ast->pat_bind_assignment.expression);
        break;

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_type_class_translate_go(dictionary_context, infer, ast->arithmetic_sequence.from);
        necro_type_class_translate_go(dictionary_context, infer, ast->arithmetic_sequence.then);
        necro_type_class_translate_go(dictionary_context, infer, ast->arithmetic_sequence.to);
        break;

    case NECRO_AST_CASE:
        necro_type_class_translate_go(dictionary_context, infer, ast->case_expression.expression);
        necro_type_class_translate_go(dictionary_context, infer, ast->case_expression.alternatives);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        necro_type_class_translate_go(dictionary_context, infer, ast->case_alternative.pat);
        necro_type_class_translate_go(dictionary_context, infer, ast->case_alternative.body);
        break;

    case NECRO_AST_TYPE_APP:
        necro_type_class_translate_go(dictionary_context, infer, ast->type_app.ty);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_app.next_ty);
        break;

    case NECRO_AST_BIN_OP_SYM:
        necro_type_class_translate_go(dictionary_context, infer, ast->bin_op_sym.left);
        necro_type_class_translate_go(dictionary_context, infer, ast->bin_op_sym.op);
        necro_type_class_translate_go(dictionary_context, infer, ast->bin_op_sym.right);
        break;

    case NECRO_AST_CONSTRUCTOR:
        necro_type_class_translate_go(dictionary_context, infer, ast->constructor.conid);
        necro_type_class_translate_go(dictionary_context, infer, ast->constructor.arg_list);
        break;

    case NECRO_AST_SIMPLE_TYPE:
        necro_type_class_translate_go(dictionary_context, infer, ast->simple_type.type_con);
        necro_type_class_translate_go(dictionary_context, infer, ast->simple_type.type_var_list);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        // return, nothing to do here
        break;
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.context);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.tycls);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.tyvar);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.declarations);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        // return, nothing to do here
        break;
        necro_type_class_translate_go(dictionary_context, infer, ast->type_signature.var);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_signature.context);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_signature.type);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        // return, nothing to do here
        break;
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_context.conid);
        necro_type_class_translate_go(dictionary_context, infer, ast->type_class_context.varid);
        break;

    case NECRO_AST_FUNCTION_TYPE:
        // return, nothing to do here
        break;
        necro_type_class_translate_go(dictionary_context, infer, ast->function_type.type);
        necro_type_class_translate_go(dictionary_context, infer, ast->function_type.next_on_arrow);
        break;

    default:
        necro_error(&infer->error, ast->source_loc, "Unrecognized AST Node type found in type class translation: %d", ast->type);
        break;
    }
}

NECRO_RETURN_CODE necro_type_class_translate(NecroInfer* infer, NecroNode* ast)
{
    necro_type_class_translate_go(NULL, infer, ast);
    return infer->error.return_code;
}

//=====================================================
// Refactor
//=====================================================
NecroSymbol necro_create_type_class_instance_name(NecroIntern* intern, NecroNode* ast)
{
    NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
    NecroCon data_type_name;
    if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
        data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
    else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
        data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
    else
        assert(false);
    return necro_intern_create_type_class_instance_symbol(intern, data_type_name.symbol, type_class_name.symbol);
}

NecroTypeClassContext* necro_ast_to_context(NecroInfer* infer, NecroNode* context_ast)
{
    if (necro_is_infer_error(infer)) return NULL;
    NecroTypeClassContext* context      = NULL;
    NecroTypeClassContext* context_head = NULL;
    while (context_ast != NULL)
    {
        // TRACE_TYPE_CLASS("context type: %d\n", contexts->type);
        if (context_ast->type == NECRO_AST_LIST_NODE)
        {
            assert(context_ast->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroTypeClassContext* new_context = necro_create_type_class_context(
                &infer->arena,
                necro_symtable_get(infer->symtable, context_ast->list.item->type_class_context.conid->conid.id)->type_class,
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
            if (necro_symtable_get(infer->symtable, context->type_class_name.id)->type_class == NULL)
            {
                necro_infer_ast_error(infer, NULL, NULL, "\'%s\' is not a type class", necro_intern_get_string(infer->intern, context->type_class_name.symbol));
                return NULL;
            }
            context_ast = context_ast->list.next_item;
        }
        else if (context_ast->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        {
            context = necro_create_type_class_context(
                &infer->arena,
                necro_symtable_get(infer->symtable, context_ast->type_class_context.conid->conid.id)->type_class,
                (NecroCon) { .symbol = context_ast->type_class_context.conid->conid.symbol,    .id = context_ast->type_class_context.conid->conid.id },
                (NecroCon) { .symbol = context_ast->type_class_context.varid->variable.symbol, .id = context_ast->type_class_context.varid->variable.id },
                NULL
            );
            if (necro_symtable_get(infer->symtable, context->type_class_name.id)->type_class == NULL)
            {
                necro_infer_ast_error(infer, NULL, NULL, "\'%s\' is not a type class", necro_intern_get_string(infer->intern, context->type_class_name.symbol));
                return NULL;
            }
            return context;
        }
        else
        {
            assert(false);
        }
    }
    return context_head;
}

// NecroType* necro_move_class_for_all_to_front(NecroInfer* infer, NecroType* type_sig, NecroTypeClassContext* class_context)
// {
//     if (necro_is_infer_error(infer)) return NULL;
//     assert(infer != NULL);
//     if (type_sig->for_all.context && type_sig->for_all.context->type_class == class_context->type_class && type_sig->for_all.var.id.id == class_context->type_var.id.id)
//         return type_sig;
//     NecroType* class_for_all = NULL;
//     NecroType* prev_sig      = NULL;
//     NecroType* curr_sig      = type_sig;
//     while (curr_sig != NULL)
//     {
//         assert(curr_sig->type == NECRO_TYPE_FOR);
//         if (curr_sig->for_all.context && curr_sig->for_all.context->type_class == class_context->type_class && curr_sig->for_all.var.id.id == class_context->type_var.id.id)
//         {
//             class_for_all = curr_sig;
//             if (prev_sig != NULL)
//             {
//                 prev_sig->for_all.type = curr_sig->for_all.type;
//             }
//             break;
//         }
//         prev_sig = curr_sig;
//         curr_sig = curr_sig->for_all.type;
//     }
//     class_for_all->for_all.type = type_sig;
//     assert(class_for_all->for_all.context->type_class == class_context->type_class);
//     return class_for_all;
// }

// TODO: TEST
void necro_create_type_class(NecroInfer* infer, NecroNode* type_class_ast)
{
    if (necro_is_infer_error(infer)) return;
    assert(infer != NULL);
    assert(type_class_ast != NULL);
    assert(type_class_ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);

    //------------------------------------
    // Create type class
    NecroSymbolInfo* info       = necro_symtable_get(infer->symtable, type_class_ast->type_class_declaration.tycls->conid.id);
    NecroTypeClass*  type_class = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClass));
    type_class->type_class_name = (NecroCon) { .symbol = type_class_ast->type_class_declaration.tycls->conid.symbol, .id = type_class_ast->type_class_declaration.tycls->conid.id };
    type_class->members         = NULL;
    type_class->type_var        = (NecroCon) { .symbol = type_class_ast->type_class_declaration.tyvar->variable.symbol, .id = type_class_ast->type_class_declaration.tyvar->variable.id };
    type_class->context         = NULL;
    type_class->dependency_flag = 0;
    type_class->ast             = type_class_ast;
    type_class->dictionary_name = necro_create_dictionary_name(infer->intern, type_class->type_class_name.symbol);
    info->type_class            = type_class;
    if (necro_is_infer_error(infer)) return;

    //------------------------------------
    // Create type_var for type_class
    NecroType* type_class_var             = necro_create_type_var(infer, (NecroVar) { .id = type_class->type_var.id, .symbol = type_class_ast->type_class_declaration.tyvar->variable.symbol});
    // ty_var->var.is_rigid                 = false;
    type_class_var->var.is_rigid          = true;
    type_class_var->var.is_type_class_var = true;
    type_class->type                      = necro_make_con_1(infer, type_class->type_class_name, necro_create_type_list(infer, type_class_var, NULL));
    type_class->type->con.is_class        = true;
    type_class->context                   = necro_ast_to_context(infer, type_class_ast->type_class_declaration.context);
    info->type                            = type_class_var;
    if (necro_is_infer_error(infer)) return;

    type_class_var->var.context = necro_create_type_class_context(&infer->arena, type_class, type_class->type_class_name, type_class->type_var, type_class->context);
    necro_kind_infer(infer, type_class_var, type_class_var, "While declaring a type class: ");
    NecroTypeClassContext* class_context = necro_create_type_class_context(&infer->arena, type_class, type_class->type_class_name, type_class->type_var, NULL);

    //--------------------------------
    // Build Member Type Signatures
    if (type_class->ast->type_class_declaration.declarations != NULL)
    {
        NecroDeclarationGroupList* group_list = type_class->ast->type_class_declaration.declarations->declaration.group_list;
        while (group_list != NULL)
        {
            NecroDeclarationGroup* declaration_group = group_list->declaration_group;
            while (declaration_group != NULL)
            {
                NecroNode* method_ast = declaration_group->declaration_ast;
                if (method_ast->type != NECRO_AST_TYPE_SIGNATURE)
                    continue;

                NecroType* type_sig = necro_ast_to_type_sig_go(infer, method_ast->type_signature.type);
                if (necro_is_infer_error(infer)) return;

                //---------------------------------
                // Infer and check method type sig
                // TODO: TypeClass's context should ALWAYS BE FIRST!
                NecroTypeClassContext* method_context = necro_ast_to_context(infer, method_ast->type_signature.context);
                if (necro_is_infer_error(infer)) return;
                if (necro_constrain_class_variable_check(infer, type_class->type_class_name, type_class->type_var, method_ast->type_signature.var->variable.symbol, method_context)) return;
                NecroTypeClassContext* context = necro_union_contexts(infer, class_context, method_context);
                assert(context->type_class == class_context->type_class);
                if (necro_ambiguous_type_class_check(infer, method_ast->type_signature.var->variable.symbol, context, type_sig)) return;
                necro_apply_constraints(infer, type_sig, context);
                type_sig->pre_supplied = true;
                type_sig->source_loc   = method_ast->source_loc;
                type_sig               = necro_gen(infer, type_sig, NULL);
                // // Ensure that type class for_all is first!
                // assert(type_sig->type == NECRO_TYPE_FOR);
                // type_sig = necro_move_class_for_all_to_front(infer, type_sig, class_context);
                // type_sig->pre_supplied = true;
                // type_sig->source_loc   = method_ast->source_loc;
                if (necro_is_infer_error(infer)) return;
                necro_kind_infer(infer, type_sig, type_sig, "While declaring a method of a type class: ");
                if (necro_is_infer_error(infer)) return;
                necro_kind_gen(infer, type_sig->type_kind);
                if (necro_is_infer_error(infer)) return;
                necro_kind_unify(infer, type_sig->type_kind, infer->star_type_kind, NULL, type_sig, "While declaring a method of a type class");
                if (necro_is_infer_error(infer)) return;
                necro_symtable_get(infer->symtable, method_ast->type_signature.var->variable.id)->type              = type_sig;
                necro_symtable_get(infer->symtable, method_ast->type_signature.var->variable.id)->method_type_class = type_class;

                //---------------------------------
                // kind check for type context!
                NecroTypeClassContext* curr_context = context;
                while (curr_context != NULL)
                {
                    // NecroTypeClass* type_class     = necro_type_class_table_get(&infer->type_class_env->class_table, curr_context->type_class_name.id.id);
                    NecroTypeClass* type_class     = curr_context->type_class;
                    NecroType*      type_class_var = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
                    NecroType*      var_type       = necro_symtable_get(infer->symtable, curr_context->type_var.id)->type;
                    necro_kind_unify(infer, var_type->type_kind, type_class_var->type_kind, method_ast->scope, type_sig, "While inferring the type of a method type signature");
                    if (necro_is_infer_error(infer)) return;
                    curr_context = curr_context->next;
                }

                //---------------------------------
                // Store type class members
                NecroTypeClassMember* new_member  = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassMember));
                new_member->member_varid = (NecroCon) { .symbol = method_ast->type_signature.var->variable.symbol, .id = method_ast->type_signature.var->variable.id };
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
                // declarations = declarations->declaration.next_declaration;
                declaration_group = declaration_group->next;
            }
            TRACE_TYPE_CLASS("Finishing type_class: %s\n", necro_intern_get_string(infer->intern, type_class->type_class_name.symbol));
            group_list = group_list->next;
        }
    }

    //---------------------------
    // Build dictionary declarations
    // NecroASTNode* dictionary_declaration_ast = type_class->ast->type_class_declaration.dictionary_data_declaration;
    necro_create_dictionary_data_declaration(&infer->arena, infer->intern, type_class->ast);

    //---------------------------
    // Infer dictionary data declaration types
    NecroDeclarationGroup* dictionary_declaration_group = NULL;
    NecroASTNode*          dictionary_declarations      = type_class->ast->type_class_declaration.dictionary_data_declaration;
    while (dictionary_declarations != NULL)
    {
        //---------------------------
        // Build scopes and rename
        necro_build_scopes_go(infer->scoped_symtable, dictionary_declarations->top_declaration.declaration);
        infer->error = infer->scoped_symtable->error;
        if (necro_is_infer_error(infer)) return;
        necro_rename_declare_pass(infer->renamer, &infer->arena, dictionary_declarations->top_declaration.declaration);
        infer->error = infer->renamer->error;
        if (necro_is_infer_error(infer)) return;
        necro_rename_var_pass(infer->renamer, &infer->arena, dictionary_declarations->top_declaration.declaration);
        infer->error = infer->renamer->error;
        if (necro_is_infer_error(infer)) return;

        dictionary_declaration_group = necro_append_declaration_group(&infer->arena, dictionary_declarations->top_declaration.declaration, dictionary_declaration_group);
        dictionary_declarations      = dictionary_declarations->top_declaration.next_top_decl;
    }
    necro_infer_declaration_group(infer, dictionary_declaration_group);
    if (necro_is_infer_error(infer)) return;

    // Generalize type class kind
    type_class_var->type_kind = necro_kind_gen(infer, type_class_var->type_kind);
}

NecroASTNode* necro_create_type_class_instance_dictionary_member(NecroInfer* infer, NecroCon prototype_name, NecroTypeClassContext* instance_context, NecroTypeClassDictionaryContext** dictionary_context)
{
    assert(infer != NULL);
    assert(prototype_name.id.id != 0);
    NecroASTNode* apats_head  = NULL;
    NecroASTNode* apats       = NULL;
    NecroASTNode* expr        = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, prototype_name.symbol), NECRO_VAR_VAR);
    expr->variable.id         = prototype_name.id;
    NecroType*    member_type = necro_symtable_get(infer->symtable, prototype_name.id)->type;
    assert(member_type != NULL);
    NecroSymbol   arg_post    = necro_intern_string(infer->intern, "@class");
    // TODO: FINISH!!!!
    // TODO: FIGURE OUT WHAT TO DO ABOUT THIS!!!!
    // Arguments
    while (member_type->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* context = instance_context;
        while (context != NULL)
        {
            if (context->type_class_name.id.id == member_type->for_all.context->type_class_name.id.id && context->type_var.id.id == member_type->for_all.var.id.id)
                goto instance_member_arg_next;
            context = context->next;
        }
        NecroSymbol   arg_name = necro_create_dictionary_arg_name(infer->intern, member_type->for_all.context->type_class_name.symbol, member_type->for_all.var.symbol);
        arg_name               = necro_intern_concat_symbols(infer->intern, arg_name, arg_post);
        NecroASTNode* arg_var  = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, arg_name), NECRO_VAR_DECLARATION);
        if (apats_head == NULL)
        {

            apats_head = necro_create_apat_list_ast(&infer->arena, arg_var, NULL);
            apats      = apats_head;
        }
        else
        {
            apats->apats.next_apat = necro_create_apat_list_ast(&infer->arena, arg_var, NULL);
            apats                  = apats->apats.next_apat;
        }
instance_member_arg_next:
        member_type = member_type->for_all.type;
    }
    member_type = necro_symtable_get(infer->symtable, prototype_name.id)->type;
    // Applications
    while (member_type->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* context = instance_context;
        while (context != NULL)
        {
            // Apply instance dictionary arg
            if (context->type_class_name.id.id == member_type->for_all.context->type_class_name.id.id && context->type_var.id.id == member_type->for_all.var.id.id)
            {
                NecroASTNode* dvar  = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, necro_create_dictionary_arg_name(infer->intern, context->type_class_name.symbol, context->type_var.symbol)), NECRO_VAR_VAR);
                *dictionary_context = necro_create_type_class_dictionary_context(&infer->arena, context->type_class_name, context->type_var, dvar, *dictionary_context);
                expr                = necro_create_fexpr_ast(&infer->arena, expr, dvar);
                goto instance_member_application_next;
            }
            context = context->next;
        }
        // Apply non instance dictionary arg
        NecroSymbol   arg_name = necro_create_dictionary_arg_name(infer->intern, member_type->for_all.context->type_class_name.symbol, member_type->for_all.var.symbol);
        arg_name               = necro_intern_concat_symbols(infer->intern, arg_name, arg_post);
        NecroASTNode* arg_var  = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, arg_name), NECRO_VAR_VAR);
        expr = necro_create_fexpr_ast(&infer->arena, expr, arg_var);
instance_member_application_next:
        member_type = member_type->for_all.type;
    }
    // do renaming here
    NecroASTNode* member_ast = NULL;
    if (apats_head != NULL)
        member_ast = necro_create_lambda_ast(&infer->arena, apats_head, expr);
    else
        member_ast = expr;
    return member_ast;
}

// TODO: TEST
void necro_create_type_class_instance(NecroInfer* infer, NecroNode* ast)
{
    if (necro_is_infer_error(infer)) return;
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);

    //------------------
    // Info and Names
    NecroSymbolInfo* info    = necro_symtable_get(infer->symtable, ast->type_class_instance.instance_id);
    NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
    NecroCon data_type_name;
    if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
        data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
    else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
        data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
    else
        assert(false);

    //------------------
    // Data, Class, Duplicate checks
    if (necro_symtable_get(infer->symtable, data_type_name.id)->type == NULL)
    {
        necro_infer_ast_error(infer, NULL, ast, "No type named \'%s\', in instance declaration for class \'%s\'", necro_intern_get_string(infer->intern, data_type_name.symbol), necro_intern_get_string(infer->intern, type_class_name.symbol));
        return;
    }
    NecroTypeClass* type_class = necro_symtable_get(infer->symtable, type_class_name.id)->type_class;
    if (type_class == NULL)
    {
        necro_infer_ast_error(infer, NULL, ast, "%s cannot be an instance of %s, because %s is not a type class",
            necro_intern_get_string(infer->intern, data_type_name.symbol),
            necro_intern_get_string(infer->intern, type_class_name.symbol),
            necro_intern_get_string(infer->intern, type_class_name.symbol));
        return;
    }
    if (info->type_class_instance != NULL)
    {
        necro_infer_ast_error(infer, NULL, ast, "Duplicate Type Class Instance\n     class: %s\n     type: %s", necro_intern_get_string(infer->intern, type_class_name.symbol), necro_intern_get_string(infer->intern, data_type_name.symbol));
        return;
    }

    //------------------
    // Create instance
    NecroTypeClassInstance* instance   = necro_paged_arena_alloc(&infer->arena, sizeof(NecroTypeClassInstance));
    instance->instance_name            = (NecroCon) { .symbol = ast->type_class_instance.instance_name, .id = ast->type_class_instance.instance_id };
    instance->type_class_name          = type_class_name;
    instance->data_type_name           = data_type_name;
    instance->context                  = necro_ast_to_context(infer, ast->type_class_instance.context);
    if (necro_is_infer_error(infer)) return;
    instance->dictionary_prototype     = NULL;
    instance->ast                      = ast;
    instance->dependency_flag          = 0;
    instance->dictionary_instance_name = necro_create_dictionary_instance_name(infer->intern, type_class_name.symbol, ast);
    instance->data_type                = necro_ast_to_type_sig_go(infer, instance->ast->type_class_instance.inst);
    instance->super_instances          = NULL;
    info->type_class_instance          = instance;
    if (necro_is_infer_error(infer)) return;

    NecroType* class_var  = necro_symtable_get(infer->symtable, type_class->type_var.id)->type;
    assert(class_var != NULL);

    necro_kind_infer(infer, instance->data_type, instance->data_type, "While creating an instance of a type class");
    necro_kind_unify(infer, instance->data_type->type_kind, class_var->type_kind, NULL, instance->data_type, "While creating an instance of a type class");
    if (necro_is_infer_error(infer)) return;

    // Apply constraints
    necro_apply_constraints(infer, instance->data_type, instance->context);
    instance->data_type = necro_gen(infer, instance->data_type, NULL);

    NecroType* inst_data_type = necro_inst(infer, instance->data_type, NULL);

    //--------------------------------
    // Dictionary Prototype
    if (instance->ast->type_class_instance.declarations != NULL)
    {
        NecroDeclarationGroupList* group_list = instance->ast->type_class_instance.declarations->declaration.group_list;
        while (group_list != NULL)
        {
            NecroDeclarationGroup* declaration_group = group_list->declaration_group;
            while (declaration_group != NULL)
            {
                NecroNode* method_ast = declaration_group->declaration_ast;
                assert(method_ast != NULL);
                assert(method_ast->type == NECRO_AST_SIMPLE_ASSIGNMENT || method_ast->type == NECRO_AST_APATS_ASSIGNMENT);
                if (necro_is_infer_error(infer)) return;

                //--------------------------------
                // Create dictionary prototype entry
                NecroDictionaryPrototype* prev_dictionary = instance->dictionary_prototype;
                instance->dictionary_prototype            = necro_paged_arena_alloc(&infer->arena, sizeof(NecroDictionaryPrototype));
                if (method_ast->type == NECRO_AST_SIMPLE_ASSIGNMENT)
                    instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = method_ast->simple_assignment.variable_name, .id = method_ast->simple_assignment.id };
                else if (method_ast->type == NECRO_AST_APATS_ASSIGNMENT)
                    instance->dictionary_prototype->prototype_varid = (NecroCon) { .symbol = method_ast->apats_assignment.variable_name, .id = method_ast->apats_assignment.id };
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
                declaration_group = declaration_group->next;
                // declarations = declarations->declaration.next_declaration;
            }
            group_list = group_list->next;
        }
    }

    //--------------------------------
    // Infer declarations
    if (instance->ast->type_class_instance.declarations != NULL)
    {
        assert(instance->ast->type_class_instance.declarations->declaration.group_list != NULL);
        necro_infer(infer, instance->ast->type_class_instance.declarations);
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

    //--------------------------------
    // Create Dictionary Instance
    {
        NecroSymbol           dictionary_name          = type_class->dictionary_name;
        NecroSymbol           dictionary_instance_name = necro_create_dictionary_instance_name(infer->intern, type_class->type_class_name.symbol, instance->ast);
        NecroASTNode*         dictionary_conid         = necro_create_conid_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_name), NECRO_CON_VAR);
        dictionary_conid->conid.id                     = type_class->ast->type_class_declaration.dictionary_data_declaration->top_declaration.declaration->data_declaration.simpletype->simple_type.type_con->conid.id;
        NecroASTNode*         dictionary_fexpr         = dictionary_conid;
        NecroTypeClassMember* members                  = type_class->members;
        // NecroVar              type_var                 = necro_con_to_var(type_class->type_var);

        //---------------------------------
        // Dictionary context and args
        NecroASTNode*                    dictionary_instance           = NULL;
        NecroTypeClassDictionaryContext* dictionary_context            = NULL;
        NecroTypeClassDictionaryContext* dictionary_context_curr       = NULL;
        NecroTypeClassContext*           inst_context                  = instance->context;
        NecroASTNode*                    dictionary_instance_args_head = NULL;
        NecroASTNode*                    dictionary_instance_args      = NULL;
        if (instance->context != NULL)
        {
            inst_context = instance->context;
            while (inst_context != NULL)
            {
                // d_var
                // NecroSymbol   arg_name = necro_create_dictionary_arg_name(infer->intern, inst_context->type_class_name.symbol, type_var.symbol);
                NecroSymbol   arg_name = necro_create_dictionary_arg_name(infer->intern, inst_context->type_class_name.symbol, inst_context->type_var.symbol);
                NecroASTNode* d_var    = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, arg_name), NECRO_VAR_DECLARATION);
                if (dictionary_instance_args_head == NULL)
                {
                    dictionary_instance_args_head = necro_create_apat_list_ast(&infer->arena, d_var, NULL);
                    dictionary_instance_args      = dictionary_instance_args_head;
                }
                else
                {
                    dictionary_instance_args->apats.next_apat = necro_create_apat_list_ast(&infer->arena, d_var, NULL);
                    dictionary_instance_args                  = dictionary_instance_args->apats.next_apat;
                }
                // dictionary_context
                if (dictionary_context == NULL)
                {
                    dictionary_context      = necro_create_type_class_dictionary_context(&infer->arena, inst_context->type_class_name, inst_context->type_var, d_var, NULL);
                    dictionary_context_curr = dictionary_context;
                }
                else
                {
                    dictionary_context_curr->next = necro_create_type_class_dictionary_context(&infer->arena, inst_context->type_class_name, inst_context->type_var, d_var, NULL);
                    dictionary_context_curr       = dictionary_context_curr->next;
                }
                inst_context = inst_context->next;
            }
        }

        //---------------------------------
        // Dictionary member app
        while (members != NULL)
        {
            NecroDictionaryPrototype* dictionary_prototype = instance->dictionary_prototype;
            while (dictionary_prototype != NULL)
            {
                if (dictionary_prototype->type_class_member_varid.id.id == members->member_varid.id.id)
                {
                    dictionary_fexpr = necro_create_fexpr_ast(&infer->arena, dictionary_fexpr, necro_create_type_class_instance_dictionary_member(infer, dictionary_prototype->prototype_varid, instance->context, &dictionary_context));
                    // NecroASTNode* mvar = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_prototype->prototype_varid.symbol), NECRO_VAR_VAR);
                    // mvar->variable.id  = dictionary_prototype->prototype_varid.id;
                    // NecroType* prototype_member_type = necro_symtable_get(infer->symtable, dictionary_prototype->prototype_varid.id)->type;
                    // while (prototype_member_type->type == NECRO_TYPE_FOR)
                    // {
                    //     inst_context = instance->context;
                    //     while (inst_context != NULL)
                    //     {
                    //         NecroASTNode* dvar = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, necro_create_dictionary_arg_name(infer->intern, inst_context->type_class_name.symbol, type_var.symbol)), NECRO_VAR_VAR);
                    //         dictionary_context = necro_create_type_class_dictionary_context(&infer->arena, inst_context->type_class_name, necro_var_to_con(type_var), dvar, dictionary_context);
                    //         mvar               = necro_create_fexpr_ast(&infer->arena, mvar, dvar);
                    //         inst_context       = inst_context->next;
                    //     }
                    //     prototype_member_type = prototype_member_type->for_all.type;
                    // }
                    // dictionary_fexpr = necro_create_fexpr_ast(&infer->arena, dictionary_fexpr, mvar);
                    break;
                }
                dictionary_prototype = dictionary_prototype->next;
            }
            members = members->next;
        }

        //---------------------------------
        // Resolve Super instance symbols
        NecroTypeClassContext* super_classes = type_class->context;
        while (super_classes != NULL)
        {
            NecroTypeClassInstance* super_instance = necro_get_type_class_instance(infer, instance->data_type_name.symbol, super_classes->type_class_name.symbol);
            assert(super_instance != NULL);
            instance->super_instances              = necro_cons_instance_list(&infer->arena, super_instance, instance->super_instances);
            super_classes = super_classes->next;
        }

        //---------------------------------
        // Dictionary super class dictionary app
        NecroInstanceList* super_instances = instance->super_instances;
        while(super_instances != NULL)
        {
            NecroSymbol             super_class_dictionary_name = necro_create_dictionary_instance_name(infer->intern, super_instances->data->type_class_name.symbol, super_instances->data->ast);
            NecroASTNode*           dvar                        = necro_create_variable_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, super_class_dictionary_name), NECRO_VAR_VAR);
            NecroTypeClassContext*  super_context               = super_instances->data->context;
            // Super class dictionary arguments
            // TODO: Is this fully recursive!?!?!?
            while (super_context != NULL)
            {
                NecroASTNode* argument_dictionary = retrieveDictionaryFromContext(infer, dictionary_context, super_context, necro_con_to_var(type_class->type_var)); // is using the type class var correct here!?
                assert(argument_dictionary != NULL);
                dvar          = necro_create_fexpr_ast(&infer->arena, dvar, argument_dictionary);
                super_context = super_context->next;
            }
            dictionary_fexpr = necro_create_fexpr_ast(&infer->arena, dictionary_fexpr, dvar);
            super_instances = super_instances->next;
        }
        if (instance->context == NULL)
            dictionary_instance = necro_create_simple_assignment(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_instance_name), necro_create_rhs_ast(&infer->arena, dictionary_fexpr, NULL));
        else
            dictionary_instance = necro_create_apats_assignment_ast(&infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_instance_name), dictionary_instance_args_head, necro_create_rhs_ast(&infer->arena, dictionary_fexpr, NULL));
        instance->ast->type_class_instance.dictionary_instance = dictionary_instance;
        if (necro_is_infer_error(infer)) return;

        //---------------------------
        // Build scopes and rename
        necro_build_scopes_go(infer->scoped_symtable, dictionary_instance);
        infer->error = infer->scoped_symtable->error;
        if (necro_is_infer_error(infer)) return;
        necro_rename_declare_pass(infer->renamer, &infer->arena, dictionary_instance);
        infer->error = infer->renamer->error;
        if (necro_is_infer_error(infer)) return;
        necro_rename_var_pass(infer->renamer, &infer->arena, dictionary_instance);
        infer->error = infer->renamer->error;
        if (necro_is_infer_error(infer)) return;
    }
}

// TODO: OPTIMIZE THIS!!!!!
NecroTypeClassInstance* necro_get_type_class_instance(NecroInfer* infer, NecroSymbol data_type_name, NecroSymbol type_class_name)
{
    NecroSymbol             super_symbol   = necro_intern_create_type_class_instance_symbol(infer->intern, data_type_name, type_class_name);
    NecroID                 super_id       = necro_scope_find(infer->scoped_symtable->top_type_scope, super_symbol);
    NecroTypeClassInstance* super_instance = necro_symtable_get(infer->symtable, super_id)->type_class_instance;
    assert(super_instance != NULL);
    return super_instance;
}

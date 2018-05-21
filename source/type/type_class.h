/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_CLASS_H
#define TYPE_CLASS_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "hash_table.h"
#include "renamer.h"
#include "type.h"

typedef NecroAST_Node_Reified NecroNode;

typedef struct NecroTypeClassContext
{
    NecroCon                      type_class_name;
    NecroCon                      type_var;
    struct NecroTypeClassContext* next;
} NecroTypeClassContext;

typedef struct NecroTyVarContextList
{
    struct NecroTyVarContextList* next;
    NecroCon                      type_var;
    NecroTypeClassContext*        context;
    bool                          is_type_class_var;
} NecroTyVarContextList;

typedef struct NecroTypeClassMember
{
    NecroCon                     member_varid;
    struct NecroTypeClassMember* next;
} NecroTypeClassMember;

typedef struct NecroTypeClass
{
    NecroNode*             ast;
    NecroType*             type;
    NecroCon               type_class_name;
    NecroCon               type_var;
    NecroTypeClassMember*  members;
    NecroTypeClassContext* context;
    size_t                 dependency_flag;
    NecroSymbol            dictionary_name;
} NecroTypeClass;

typedef struct NecroDictionaryPrototype
{
    NecroCon                         type_class_member_varid;
    NecroCon                         prototype_varid;
    struct NecroDictionaryPrototype* next;
} NecroDictionaryPrototype;

typedef struct
{
    NecroNode*                ast;
    NecroCon                  data_type_name;
    NecroCon                  type_class_name;
    NecroTypeClassContext*    context;
    NecroDictionaryPrototype* dictionary_prototype;
    NecroType*                data_type;
    size_t                    dependency_flag;
    NecroSymbol               dictionary_instance_name;
} NecroTypeClassInstance;

NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClassInstance, TypeClassInstance, type_class_instance)
NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClass, TypeClass, type_class)

typedef struct NecroTypeClassEnv
{
    NecroTypeClassTable         class_table;
    NecroTypeClassInstanceTable instance_table;
    NecroPagedArena             arena;
    NecroScopedSymTable*        scoped_symtable;
    NecroRenamer*               renamer;
} NecroTypeClassEnv;

NecroTypeClassEnv       necro_create_type_class_env(NecroScopedSymTable* scoped_symtable, NecroRenamer* renamer);
void                    necro_destroy_type_class_env(NecroTypeClassEnv* env);

void                    necro_declare_type_classes(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations);
void                    necro_type_class_instances_pass1(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations);
void                    necro_type_class_instances_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations);

NecroSymbol             necro_create_type_class_instance_name(NecroIntern* intern, NecroNode* ast);
void                    necro_print_type_class_env(NecroTypeClassEnv* env, NecroInfer* infer, NecroIntern* intern);
NecroTypeClassInstance* necro_get_instance(NecroInfer* infer, NecroCon data_type_name, NecroCon type_class_name);
uint64_t                necro_create_instance_key(NecroCon data_type_name, NecroCon type_class_name);
bool                    necro_context_contains_class(NecroTypeClassEnv* env, NecroTypeClassContext* context, NecroTypeClassContext* type_class);
bool                    necro_context_and_super_classes_contain_class(NecroTypeClassEnv* env, NecroTypeClassContext* context, NecroTypeClassContext* type_class);
NecroTypeClassContext*  necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2);
NecroTypeClassContext*  necro_union_contexts_to_same_var(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2, NecroCon type_var);
bool                    necro_ambiguous_type_class_check(NecroInfer* infer, NecroSymbol type_sig_name, NecroTypeClassContext* context, NecroType* type);
NecroTypeClassContext*  necro_ast_to_context(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* context_ast);
void                    necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context);
NecroTypeClassContext*  necro_create_type_class_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next);
NecroTypeClassContext*  necro_scrub_super_classes(NecroInfer* infer, NecroTypeClassContext* context);

typedef struct NecroTypeClassDictionaryContext
{
    NecroCon      type_class_name;
    NecroCon      type_var_name;
    NecroASTNode* dictionary_arg_ast;
    struct NecroTypeClassDictionaryContext* next;
} NecroTypeClassDictionaryContext;
NecroTypeClassDictionaryContext* necro_create_type_class_dictionary_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var_name, NecroASTNode* dictionary_arg_ast, NecroTypeClassDictionaryContext* next);
NECRO_RETURN_CODE                necro_type_class_translate(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast);
void                             necro_type_class_translate_go(NecroTypeClassDictionaryContext* dictionary_context, NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast);
void                             necro_create_dictionary_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroASTNode* type_class_ast);

//=====================================================
// Refactor
//=====================================================
void necro_create_type_class(NecroInfer* infer, NecroNode* ast);
void necro_create_type_class_instance(NecroInfer* infer, NecroNode* ast);

#endif // TYPE_CLASS_H
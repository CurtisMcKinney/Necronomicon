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

typedef struct
{
    NecroNode*             ast;
    NecroType*             type;
    NecroCon               type_class_name;
    NecroCon               type_var;
    NecroTypeClassMember*  members;
    NecroTypeClassContext* context;
    size_t                 dependency_flag;
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
} NecroTypeClassInstance;

NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClassInstance, TypeClassInstance, type_class_instance)
NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClass, TypeClass, type_class)

typedef struct NecroTypeClassEnv
{
    NecroTypeClassTable         class_table;
    NecroTypeClassInstanceTable instance_table;
    NecroPagedArena             arena;
} NecroTypeClassEnv;

NecroTypeClassEnv       necro_create_type_class_env();
void                    necro_destroy_type_class_env(NecroTypeClassEnv* env);

void                    necro_declare_type_classes(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations);
void                    necro_type_class_instances_pass1(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations);
void                    necro_type_class_instances_pass2(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* top_level_declarations);

void                    necro_print_type_class_env(NecroTypeClassEnv* env, NecroInfer* infer, NecroIntern* intern);
NecroTypeClassInstance* necro_get_instance(NecroInfer* infer, NecroCon data_type_name, NecroCon type_class_name);
uint64_t                necro_create_instance_key(NecroCon data_type_name, NecroCon type_class_name);
bool                    necro_context_contains_class(NecroTypeClassEnv* env, NecroTypeClassContext* context, NecroTypeClassContext* type_class);
NecroTypeClassContext*  necro_union_contexts(NecroInfer* infer, NecroTypeClassContext* context1, NecroTypeClassContext* context2);
bool                    necro_ambiguous_type_class_check(NecroInfer* infer, NecroSymbol type_sig_name, NecroTypeClassContext* context, NecroType* type);
NecroTypeClassContext*  necro_ast_to_context(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* context_ast);
void                    necro_apply_constraints(NecroInfer* infer, NecroType* type, NecroTypeClassContext* context);
NecroTypeClassContext*  necro_create_type_class_context(NecroPagedArena* arena, NecroCon type_class_name, NecroCon type_var, NecroTypeClassContext* next);

#endif // TYPE_CLASS_H
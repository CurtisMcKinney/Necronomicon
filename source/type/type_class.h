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

struct NecroTypeClassContext;
typedef NecroAST_Node_Reified NecroNode;

typedef struct
{
    NecroCon                      type_class_name;
    NecroCon                      type_var;
    NecroType*                    member_type_sig_list;
    struct NecroTypeClassContext* context;
} NecroTypeClass;

typedef struct
{
    NecroCon                      data_type_name;
    NecroCon                      type_class_name;
    NecroType*                    dictionary_type_sig_list;
    struct NecroTypeClassContext* context;
} NecroTypeClassInstance;

typedef struct NecroTypeClassContext
{
    NecroCon                      type_class_name;
    NecroCon                      type_var;
    struct NecroTypeClassContext* next_context;
} NecroTypeClassContext;

NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClassInstance, TypeClassInstance, type_class_instance)
NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClass, TypeClass, type_class)

typedef struct NecroTypeClassEnv
{
    NecroTypeClassTable         class_table;
    NecroTypeClassInstanceTable instance_table;
    NecroPagedArena             arena;
} NecroTypeClassEnv;

NecroTypeClassEnv necro_create_type_class_env();
void              necro_destroy_type_class_env(NecroTypeClassEnv* env);
void              necro_create_type_class_declaration(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast);
void              necro_create_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast);

#endif // TYPE_CLASS_H
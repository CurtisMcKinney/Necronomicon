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
    struct NecroTypeClassContext* context;
    NecroID                       type_id;
    NecroCon                      type_var;
    NecroType*                    member_type_sig_list;
} NecroTypeClass;

typedef struct NecroTypeClassContext
{
    NecroTypeClass* classes;
    size_t          classes_count;
} NecroTypeClassContext;

NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroTypeClass, TypeClass, type_class)

typedef struct
{
    NecroTypeClassTable table;
    NecroInfer*         infer;
} NecroTypeClassEnv;

NecroTypeClassEnv necro_create_type_class_env(NecroInfer* infer);
void              necro_destroy_type_class_env(NecroTypeClassEnv* env);
void              necro_create_type_class(NecroTypeClassEnv* env, NecroNode* ast);

#endif // TYPE_CLASS_H
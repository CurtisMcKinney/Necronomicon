/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"
#include "type_class.h"

NecroTypeClassEnv necro_create_type_class_env(NecroInfer* infer)
{
    return (NecroTypeClassEnv)
    {
        .table = necro_create_type_class_table(),
        .infer = infer
    };
}

void necro_destroy_type_class_env(NecroTypeClassEnv* env)
{
    necro_destroy_type_class_table(&env->table);
}

void necro_create_type_class(NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    NecroTypeClass* type_class       = necro_type_class_table_insert(&env->table, ast->type_class_declaration.tycls->conid.id.id, NULL);
    type_class->type_id              = ast->type_class_declaration.tycls->conid.id;
    type_class->member_type_sig_list = NULL;
    type_class->type_var             = (NecroCon) { .symbol = ast->type_class_declaration.tyvar->variable.symbol, .id = ast->type_class_declaration.tyvar->variable.id };
    // TODO: Pass in Context to NecroTypeClass somehow
    NecroNode* declarations = ast->type_class_declaration.declarations;
    while (declarations != NULL)
    {
        NecroType* declaration_type = necro_infer_go(env->infer, declarations->declaration.declaration_impl);
        if (necro_is_infer_error(env->infer)) return;
        type_class->member_type_sig_list = necro_create_type_list(env->infer, declaration_type, type_class->member_type_sig_list);
        declarations = declarations->declaration.next_declaration;
    }
}
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef INFER_H
#define INFER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "type.h"
#include "type_class.h"
#include "ast.h"

typedef NecroAST_Node_Reified NecroNode;
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroNode* ast, const char* error_message, ...);
NecroType* necro_infer_type_sig(NecroInfer* infer, NecroNode* ast);
NecroType* necro_ast_to_type_sig_go(NecroInfer* infer, NecroNode* ast);
NecroType* necro_create_data_constructor(NecroInfer* infer, NecroNode* ast, NecroType* data_type);
NecroType* necro_ty_vars_to_args(NecroInfer* infer, NecroNode* ty_vars);
NecroType* necro_infer_declaration_group(NecroInfer* infer, NecroDeclarationGroup* declaration_group);

void necro_test_infer();

#endif // INFER_H
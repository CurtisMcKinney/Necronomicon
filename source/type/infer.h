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

NecroType* necro_infer_go(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroAst* ast, const char* error_message, ...);
NecroType* necro_infer_type_sig(NecroInfer* infer, NecroAst* ast);
NecroType* necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast);
NecroType* necro_create_data_constructor(NecroInfer* infer, NecroAst* ast, NecroType* data_type, size_t con_num);
NecroType* necro_ty_vars_to_args(NecroInfer* infer, NecroAst* ty_vars);
NecroType* necro_infer_declaration_group(NecroInfer* infer, NecroDeclarationGroup* declaration_group);
bool       necro_is_fully_concrete(NecroSymTable* symtable, NecroType* type);
bool       necro_is_sized(NecroSymTable* symtable, NecroType* type);

void necro_test_infer();

#endif // INFER_H
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
#include "ast.h"

typedef NecroAST_Node_Reified NecroNode;
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer(NecroInfer* infer, NecroAST_Node_Reified* ast);
NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroNode* ast, const char* error_message, ...);

void necro_test_infer();

#endif // INFER_H
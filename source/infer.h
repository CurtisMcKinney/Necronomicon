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

// typedef struct
// {
//     NecroSub*        sub;
//     NecroTypeScheme* type;
// } NecroInferResult;

// NecroTypeScheme* necro_infer(NecroInfer* infer, NecroGamma* gamma, NecroAST_Node_Reified* ast);

void necro_test_infer();

#endif // INFER_H
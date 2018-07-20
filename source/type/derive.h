/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_DERIVE_H
#define TYPE_DERIVE_H 1


#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "utility.h"
#include "type.h"

// void necro_derive_show(NecroInfer* infer, NecroASTNode* data_ast);
// void necro_derive_eq(NecroInfer* infer, NecroASTNode* data_ast);
// void necro_derive_ord(NecroInfer* infer, NecroASTNode* data_ast);
void necro_derive_specialized_clone(NecroInfer* infer, NecroType* data_type);

#endif // TYPE_DERIVE_H
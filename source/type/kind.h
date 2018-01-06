/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef KIND_H
#define KIND_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "kind.h"

typedef NecroType NecroTypeKind;

NecroTypeKind* necro_create_star_kind(NecroInfer* infer);
void           necro_infer_kinds_for_data_declaration(NecroInfer* infer, NecroASTNode* ast);
void           necro_kind_unify(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble);

#endif // KIND_H
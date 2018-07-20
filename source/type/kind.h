/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef KIND_H
#define KIND_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "type.h"

typedef NecroType NecroTypeKind;

NecroTypeKind* necro_create_star_kind(NecroInfer* infer);
void           necro_infer_kinds_for_data_declaration(NecroInfer* infer, NecroASTNode* ast);
void           necro_kind_unify(NecroInfer* infer, NecroTypeKind* kind1, NecroTypeKind* kind2, NecroScope* scope, NecroType* macro_type, const char* error_preamble);
NecroTypeKind* necro_kind_inst(NecroInfer* infer, NecroTypeKind* kind, NecroScope* scope);
NecroTypeKind* necro_kind_gen(NecroInfer* infer, NecroTypeKind* kind);
NecroTypeKind* necro_kind_infer(NecroInfer* infer, NecroType* type, NecroType* macro_type, const char* error_preamble);
size_t         necro_kind_arity(NecroType* type);

#endif // KIND_H
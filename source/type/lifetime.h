/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef LIFETIME_H
#define LIFETIME_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "type.h"
#include "ast.h"

typedef NecroType NecroLifetime;

void necro_infer_lifetime(NecroInfer* infer, NecroASTNode* ast);
void necro_unify_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble);

#endif // LIFETIME_H
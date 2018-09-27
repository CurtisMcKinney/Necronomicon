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

struct NecroBase;

NecroAstSymbol*        necro_create_star_kind(NecroPagedArena* arena, NecroIntern* intern);

NecroResult(NecroType) necro_kind_unify(NecroType* kind1, NecroType* kind2, NecroScope* scope);
NecroResult(NecroType) necro_kind_infer(NecroPagedArena* arena, struct NecroBase* base, NecroType* type);
NecroType*             necro_kind_gen(NecroPagedArena* arena, struct NecroBase* base, NecroType* kind);

#endif // KIND_H
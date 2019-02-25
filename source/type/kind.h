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

NecroAstSymbol*        necro_kind_create_star(NecroPagedArena* arena, NecroIntern* intern);
NecroAstSymbol*        necro_kind_create_nat(NecroPagedArena* arena, NecroIntern* intern);
NecroAstSymbol*        necro_kind_create_sym(NecroPagedArena* arena, NecroIntern* intern);
NecroResult(NecroType) necro_kind_unify_with_info(NecroType* kind1, NecroType* kind2, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType) necro_kind_unify(NecroType* kind1, NecroType* kind2, NecroScope* scope);
NecroResult(NecroType) necro_kind_infer(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroType*             necro_kind_gen(NecroPagedArena* arena, struct NecroBase* base, NecroType* kind);
NecroResult(void)      necro_kind_infer_gen_unify_with_star(NecroPagedArena* arena, struct NecroBase* base, NecroType* type, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

#endif // KIND_H
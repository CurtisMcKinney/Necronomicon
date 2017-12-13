/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_PRIM_H
#define TYPE_PRIM_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "utility/intern.h"
#include "type.h"
#include "symtable.h"

NecroPrimTypes necro_create_prim_types(NecroIntern* intern);
void           necro_add_prim_type_symbol_info(NecroPrimTypes* prim_types, NecroScopedSymTable* scoped_symtable);
void           necro_add_prim_type_sigs(NecroPrimTypes prim_types, NecroInfer* infer);

#endif // TYPE_PRIM_H
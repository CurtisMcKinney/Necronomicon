/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef MONOMORPHIZE_H
#define MONOMORPHIZE_H 1

#include "type_class.h"

NecroResult(void) necro_monomorphize(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);
void necro_monomorphize_test();

#endif // MONOMORPHIZE_H
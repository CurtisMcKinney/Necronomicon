/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_CLASS_TRANSLATE_H
#define TYPE_CLASS_TRANSLATE_H 1

#include "type_class.h"

NecroResult(void) necro_type_class_translate(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);

#endif // TYPE_CLASS_TRANSLATE_H
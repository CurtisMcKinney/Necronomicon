/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_INFER_H
#define NECRO_INFER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "type.h"
#include "type_class.h"
#include "ast.h"
#include "d_analyzer.h"
#include "result.h"

NecroResult(void)      necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);
NecroResult(NecroType) necro_infer_go(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_type_sig(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast);

bool                   necro_is_sized(NecroType* type);
bool                   necro_is_fully_concrete(struct NecroBase* base, NecroType* type);
void                   necro_test_infer();

#endif // NECRO_INFER_H
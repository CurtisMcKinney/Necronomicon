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

typedef struct NecroInfer
{
    struct NecroScopedSymTable* scoped_symtable;
    struct NecroBase*           base;
    NecroIntern*                intern;
    NecroPagedArena*            arena;
    NecroSnapshotArena          snapshot_arena;
    NecroAstArena*              ast_arena;
} NecroInfer;

NecroInfer             necro_infer_empty();
NecroInfer             necro_infer_create(NecroPagedArena* arena, NecroIntern* intern, struct NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);
void                   necro_infer_destroy(NecroInfer* infer);
NecroResult(void)      necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);
NecroResult(NecroType) necro_infer_go(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_type_sig(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast, NECRO_TYPE_ORDER variable_type_order, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type);
void                   necro_test_infer();

#endif // NECRO_INFER_H
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_INLINE_H
#define CORE_INLINE_H 1

#include <stdio.h>
#include "core_ast.h"
#include "ast_symbol.h"

typedef struct NecroCoreAstSymbolSub
{
    NecroCoreAstSymbol* symbol_to_replace;
    NecroCoreAst*       new_ast;
    NecroCoreAst*       new_lambda_var;
} NecroCoreAstSymbolSub;
NECRO_DECLARE_ARENA_LIST(NecroCoreAstSymbolSub, CoreAstSymbolSub, core_ast_symbol_sub);

typedef struct NecroCorePreSimplify
{
    NecroPagedArena* arena;
    NecroIntern*     intern;
    NecroBase*       base;
} NecroCorePreSimplify;

NecroCoreAst* necro_core_ast_duplicate_with_subs(NecroPagedArena* arena, NecroIntern* intern, NecroCoreAst* ast, NecroCoreAstSymbolSubList* subs);
void          necro_core_ast_pre_simplify(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena);
NecroCoreAst* necro_core_ast_pre_simplify_go(NecroCorePreSimplify* context, NecroCoreAst* ast);
void          necro_core_ast_pre_simplify_test();

#endif // CORE_INLINE_H

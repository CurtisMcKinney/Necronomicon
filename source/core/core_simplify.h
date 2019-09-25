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
    NecroCoreAstSymbol* old_symbol;
    NecroCoreAstSymbol* new_symbol;
} NecroCoreAstSymbolSub;
NECRO_DECLARE_ARENA_LIST(NecroCoreAstSymbolSub, CoreAstSymbolSub, core_ast_symbol_sub);

NecroCoreAst* necro_core_ast_duplicate_with_subs(NecroPagedArena* arena, NecroCoreAst* ast, NecroCoreAstSymbolSubList* subs);
// NecroCoreAst* necro_core_ast_inline(NecroPagedArena* arena, NecroCoreAst* ast, NecroCoreAst** inlined_arg_values, size_t num_inlined_arg_values);

void necro_core_ast_pre_simplify(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena);
void necro_core_ast_pre_simplify_test();

#endif // CORE_INLINE_H

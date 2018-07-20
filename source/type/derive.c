/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "symtable.h"
#include "derive.h"
#include "type_class.h"
#include "prim.h"
#include "type.h"
#include "arena.h"

// size_t necro_kind_arity(NecroType* type)
// {
//     switch (type->type)
//     {
//     case NECRO_TYPE_VAR: return true;
//     case NECRO_TYPE_APP: return necro_data_type_is_polymorphic(type->app.type1, type->app.type2);
//     case NECRO_TYPE_FUN: return necro_data_type_is_polymorphic(type->fun.type1, type->fun.type2);
//     case NECRO_TYPE_FOR: return true;
//     }
// }

///////////////////////////////////////////////////////
// _Clone
///////////////////////////////////////////////////////
NecroType*    necro_derive_clone_method_sig(NecroInfer* infer, NecroType* type);
NecroASTNode* necro_derive_clone_method(NecroInfer* infer, NecroASTNode* ast);

NecroSymbol necro_create_clone_method_name(NecroInfer* infer, NecroType* data_type)
{
    NecroType*         principal_type        = necro_symtable_get(infer->symtable, data_type->con.con.id)->type;
    size_t             principal_type_arity  = principal_type->con.arity;
    size_t             num_clone_method_strs = 2 + principal_type->con.arity * 2;
    const char**       clone_method_strs     = necro_snapshot_arena_alloc(&infer->snapshot_arena, num_clone_method_strs * sizeof(const char*));
    clone_method_strs[0]                     = "_clone@";
    clone_method_strs[1]                     = necro_intern_get_string(infer->intern, data_type->con.con.symbol);
    NecroType*         con_args              = data_type->con.args;
    for (size_t i = 2; i < num_clone_method_strs; i+=2)
    {
        assert(con_args != NULL);
        NecroType* arg_type = necro_find(con_args->list.item);
        assert(arg_type->type == NECRO_TYPE_CON);
        clone_method_strs[i]     = "$";
        clone_method_strs[i + 1] = necro_intern_get_string(infer->intern, arg_type->con.con.symbol);
        con_args = con_args->list.next;
    }
    const char* clone_method_name = necro_concat_strings(&infer->snapshot_arena, num_clone_method_strs, clone_method_strs);
    return necro_intern_string(infer->intern, clone_method_name);
}

void necro_derive_specialized_clone(NecroInfer* infer, NecroType* data_type)
{
    assert(data_type->type == NECRO_TYPE_CON);
    NecroArenaSnapshot snapshot            = necro_get_arena_snapshot(&infer->snapshot_arena);
    NecroSymbol        clone_method_symbol = necro_create_clone_method_name(infer, data_type);
    NecroID            clone_method_id     = necro_this_scope_find(infer->scoped_symtable->top_scope, clone_method_symbol);
    if (clone_method_id.id != 0)
        return;
    else
        clone_method_id = necro_scoped_symtable_new_symbol_info(infer->scoped_symtable, infer->scoped_symtable->top_scope, necro_create_initial_symbol_info(clone_method_symbol, (NecroSourceLoc) { 0, 0, 0 }, infer->scoped_symtable->top_scope, infer->intern));
    NecroSymbolInfo*   clone_method_info   = necro_symtable_get(infer->symtable, clone_method_id);
    clone_method_info->type                = necro_derive_clone_method_sig(infer, data_type);
    clone_method_info->ast                 = necro_derive_clone_method(infer, necro_symtable_get(infer->symtable, data_type->con.con.id)->ast);
    necro_rewind_arena(&infer->snapshot_arena, snapshot);
}

NecroType* necro_derive_clone_method_sig(NecroInfer* infer, NecroType* type)
{
    NecroType* arg_type    = necro_duplicate_type(infer, type);
    NecroType* return_type = necro_duplicate_type(infer, type);
    return necro_create_type_fun(infer, arg_type, return_type);
}

NecroASTNode* necro_derive_clone_method(NecroInfer* infer, NecroASTNode* ast)
{
    return NULL;
}
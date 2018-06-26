/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "closure.h"
#include <ctype.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include "symtable.h"
#include "runtime/runtime.h"

/*
    TODO:
        * Un-pointfree-ify functions
        * Closure construction
        * Partial application construction (PAPP)
        * Closure application
        * Hoist nested functions
        * Captured variables
        * Envs
        * Anonymous functions
*/

///////////////////////////////////////////////////////
// NecroClosure
///////////////////////////////////////////////////////
LLVMTypeRef necro_env_type(NecroCodeGen* codegen, LLVMTypeRef* elems, size_t elem_count)
{
    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMTypeRef*       env_elems = necro_snapshot_arena_alloc(&codegen->snapshot_arena, (elem_count + 1) * sizeof(LLVMTypeRef));
    env_elems[0]                 = codegen->necro_data_type;
    for (size_t i = 0; i < elem_count; ++i)
    {
        env_elems[1 + i] = elems[i];
    }
    LLVMTypeRef env_type = LLVMStructTypeInContext(codegen->context, env_elems, elem_count + 1, false);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return env_type;
}

LLVMTypeRef necro_closure_type(NecroCodeGen* codegen, LLVMTypeRef function_ptr_type)
{
    LLVMTypeRef closure_elems[4] = { codegen->necro_data_type, function_ptr_type, LLVMPointerType(codegen->necro_val_type, 0), LLVMPointerType(codegen->necro_env_type, 0) };
    return LLVMStructTypeInContext(codegen->context, closure_elems, 4, false);
}

LLVMValueRef necro_mk_env(NecroCodeGen* codegen, LLVMTypeRef env_type, LLVMValueRef* elems, uint16_t elem_count)
{
    LLVMValueRef void_ptr = necro_alloc_codegen(codegen, env_type, elem_count);
    LLVMValueRef env_ptr  = LLVMBuildBitCast(codegen->builder, void_ptr, env_type, "env_ptr");
    necro_init_necro_data(codegen, env_ptr, 0, elem_count);
    for (uint16_t i = 0; i < elem_count; ++elem_count)
    {
        LLVMValueRef slot_ptr = necro_snapshot_gep(codegen, "slot_ptr", env_ptr, 2, (int32_t[]) { 0, i + 1 });
        LLVMBuildStore(codegen->builder, elems[i], slot_ptr);
    }
    return env_ptr;
}

LLVMValueRef necro_mk_closure(NecroCodeGen* codegen, LLVMTypeRef closure_type, LLVMValueRef function_ptr, LLVMValueRef node_mk_function_ptr, LLVMValueRef env)
{
    LLVMValueRef void_ptr    = necro_alloc_codegen(codegen, closure_type, 3);
    LLVMValueRef closure_ptr = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(closure_type, 0), "closure_ptr");
    // Statically determine tag
    // Tag is the presence/non-presence of node_mk_function and env
    if (node_mk_function_ptr == NULL && env == NULL)
        necro_init_necro_data(codegen, closure_ptr, 0, 0);
    else if (node_mk_function_ptr != NULL && env == NULL)
        necro_init_necro_data(codegen, closure_ptr, 0, 1);
    else if (node_mk_function_ptr == NULL && env != NULL)
        necro_init_necro_data(codegen, closure_ptr, 0, 2);
    else
        necro_init_necro_data(codegen, closure_ptr, 0, 3);
    LLVMValueRef function_ptr_ptr = necro_snapshot_gep(codegen, "function_ptr_ptr", closure_ptr, 2, (int32_t[]) { 0, 1 });
    LLVMBuildStore(codegen->builder, function_ptr, function_ptr_ptr);
    LLVMValueRef node_mk_function_ptr_ptr = necro_snapshot_gep(codegen, "node_mk_function_ptr_ptr", closure_ptr, 2, (int32_t[]) { 0, 2 });
    LLVMValueRef necro_val_mk_function;
    if (node_mk_function_ptr != NULL)
        necro_val_mk_function = LLVMBuildBitCast(codegen->builder, node_mk_function_ptr, LLVMPointerType(codegen->necro_val_type, 0), "necro_val_mk");
    else
        necro_val_mk_function = LLVMConstPointerNull(LLVMPointerType(codegen->necro_val_type, 0));
    LLVMBuildStore(codegen->builder, necro_val_mk_function, node_mk_function_ptr_ptr);
    LLVMValueRef env_ptr = necro_snapshot_gep(codegen, "env_ptr", closure_ptr, 2, (int32_t[]) { 0, 3 });
    if (env != NULL)
        LLVMBuildStore(codegen->builder, env, env_ptr);
    else
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(codegen->necro_env_type, 0)), env_ptr);
    return closure_ptr;
}

LLVMValueRef necro_hoist_function(NecroCodeGen* codegen, NecroCoreAST_Expression* function)
{
    return NULL;
}

LLVMTypeRef necro_function_type(NecroCodeGen* codegen, NecroType* type, bool is_top_level)
{
    assert(type != NULL);
    type = necro_find(codegen->infer, type);
    assert(type->type == NECRO_TYPE_FOR || type->type == NECRO_TYPE_FUN);
    size_t arg_count = 0;
    size_t arg_index = 0;
    // Count args
    NecroType* for_alls = type;
    NecroType* arrows   = NULL;
    while (for_alls->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* for_all_context = type->for_all.context;
        while (for_all_context != NULL)
        {
            arg_count++;
            for_all_context = for_all_context->next;
        }
        for_alls = for_alls->for_all.type;
        for_alls = necro_find(codegen->infer, for_alls);
    }
    arrows = for_alls;
    arrows = necro_find(codegen->infer, arrows);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        arg_count++;
        arrows = arrows->fun.type2;
        arrows = necro_find(codegen->infer, arrows);
    }
    LLVMTypeRef* args = necro_paged_arena_alloc(&codegen->arena, arg_count * sizeof(LLVMTypeRef));
    for_alls = type;
    while (for_alls->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* for_all_context = type->for_all.context;
        while (for_all_context != NULL)
        {
            // necro_get_type_class_instance(codegen->infer, for_alls->for_all.var,)
            arg_index++;
            args[arg_index] = necro_get_value_ptr(codegen);
            for_all_context = for_all_context->next;
        }
        for_alls = for_alls->for_all.type;
        for_alls = necro_find(codegen->infer, for_alls);
    }
    arrows = for_alls;
    arrows = necro_find(codegen->infer, arrows);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        args[arg_index] = LLVMPointerType(necro_type_to_llvm_type(codegen, arrows->fun.type1, false), 0);
        arg_index++;
        arrows = arrows->fun.type2;
        arrows = necro_find(codegen->infer, arrows);
    }
    LLVMTypeRef return_type   = LLVMPointerType(necro_type_to_llvm_type(codegen, arrows, false), 0);
    LLVMTypeRef function_type = LLVMFunctionType(return_type, args, arg_count, false);
    LLVMTypeRef final_type;
    if (!is_top_level)
        final_type = necro_closure_type(codegen, LLVMPointerType(function_type, 0));
    else
        final_type = function_type;
    // TODO: Remove!
    const char* type_string = LLVMPrintTypeToString(final_type);
    return final_type;
}

bool necro_calculate_node_prototype_app_closure(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return false;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    assert(ast->app.exprA != NULL);
    assert(ast->app.exprB != NULL);

    NecroCoreAST_Expression*  function  = ast;
    size_t                    arg_count = 0;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    if (function->expr_type == NECRO_CORE_EXPR_VAR && necro_symtable_get(codegen->symtable, function->var.id)->node_prototype != NULL && necro_symtable_get(codegen->symtable, function->var.id)->node_prototype->outer == NULL)
        return false;
    if (function->expr_type != NECRO_CORE_EXPR_VAR)
    {
        necro_calculate_node_prototype_app_closure(codegen, function, outer_node);
    }
    NecroCoreAST_Expression** args      = necro_paged_arena_alloc(&codegen->arena, arg_count * sizeof(NecroCoreAST_Expression*));
    size_t                    arg_index = arg_count - 1;
    function                            = ast;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = function->app.exprB;
        necro_calculate_node_prototype(codegen, args[arg_index], outer_node);
        arg_index--;
        function = function->app.exprA;
    }

    NecroClosureApp closure_app = (NecroClosureApp) { .slot = 0, .app = &ast->app };
    outer_node->closure_apps    = necro_cons_closure_app_list(&codegen->arena, closure_app, outer_node->closure_apps);
    return true;
}

bool necro_calculate_node_call_app_closure(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer, LLVMValueRef* out_result)
{
    if (necro_is_codegen_error(codegen)) return false;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);

    NecroCoreAST_Expression*  function        = ast;
    size_t                    arg_count       = 0;
    size_t                    persistent_slot = ast->app.persistent_slot;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    if (function->expr_type == NECRO_CORE_EXPR_VAR && necro_symtable_get(codegen->symtable, function->var.id)->node_prototype != NULL && necro_symtable_get(codegen->symtable, function->var.id)->node_prototype->outer == NULL)
        return false;

    LLVMValueRef closure_value = necro_calculate_node_call(codegen, function, outer);
    LLVMTypeRef  closure_type  = LLVMTypeOf(closure_value);

    LLVMValueRef fn_ptr        = necro_snapshot_gep(codegen, "fn_ptr", closure_value, 2, (uint32_t[]) { 0, 1 });
    fn_ptr                     = LLVMBuildLoad(codegen->builder, fn_ptr, "fn_val");
    LLVMTypeRef  fn_type       = LLVMTypeOf(fn_ptr);

    const char*  type_string = LLVMPrintTypeToString(fn_type);
    const char*  fn_string   = LLVMPrintValueToString(fn_ptr);

    NecroArenaSnapshot snapshot   = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMTypeRef*       params     = necro_snapshot_arena_alloc(&codegen->snapshot_arena, arg_count * sizeof(LLVMTypeRef));
    LLVMValueRef*      args       = necro_paged_arena_alloc(&codegen->arena, arg_count * sizeof(LLVMValueRef));
    size_t             arg_index  = arg_count - 1;
    function                      = ast;

    LLVMGetParamTypes(LLVMGetElementType(fn_type), params);
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_calculate_node_call(codegen, function->app.exprB, outer);
        args[arg_index] = necro_maybe_cast(codegen, args[arg_index], params[arg_index]);
        arg_index--;
        function        = function->app.exprA;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    // // Pass in persistent var
    // if (persistent_slot != 0)
    // {
    //     LLVMValueRef node_ptr = necro_snapshot_gep(codegen, "per_tmp_ptr", LLVMGetParam(outer->call_function, 0), 2, (uint32_t[]) { 0, persistent_slot });
    //     args[0]               = LLVMBuildLoad(codegen->builder, node_ptr, "per_tmp_node");
    // }
    // const char*  result_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(codegen->intern, function->var.symbol), "_result" });
    LLVMValueRef result      = necro_build_call(codegen, fn_ptr, args, arg_count, "fn_ptr_result");
    *out_result = result;

    return true;
}
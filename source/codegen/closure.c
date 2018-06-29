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
#include "codegen.h"
#include "symtable.h"
#include "runtime/runtime.h"

/*
    TODO:
        * Make a distinction between closure-ified function values and normal functions / values
        * Handle over application
        * Partial application construction (PAPP)
        * Hoist nested functions
        * Captured variables
        * Envs
        * Anonymous functions
    TEST:
        * Closure construction
        * Closure application
*/


///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
LLVMTypeRef necro_get_closure_type(NecroClosureTypeTable* table, NecroCodeGen* codegen, LLVMTypeRef function_type);
NecroClosureTypeBucket necro_get_closure_type_bucket(NecroClosureTypeTable* table, NecroCodeGen* codegen, LLVMTypeRef function_type);

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
    return necro_get_closure_type(&codegen->closure_type_table, codegen, function_ptr_type);
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
    NecroClosureTypeBucket closure_type_bucket   = necro_get_closure_type_bucket(&codegen->closure_type_table, codegen, LLVMTypeOf(function_ptr));
    LLVMValueRef           necro_val_mk_function = LLVMBuildBitCast(codegen->builder, node_mk_function_ptr, LLVMPointerType(LLVMFunctionType(LLVMPointerType(codegen->necro_val_type, 0), NULL, 0, false), 0), "necro_val_mk");
    LLVMValueRef           closure               = necro_build_call(codegen, closure_type_bucket.mk_closure, (LLVMValueRef[]) { function_ptr, necro_val_mk_function, env }, 3, "closure");
    return closure;
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
    // const char* type_string = LLVMPrintTypeToString(final_type);
    return final_type;
}

bool necro_is_node_ast_a_closure_value(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroType* type = NULL;
    if (ast->expr_type == NECRO_CORE_EXPR_BIND)
    {
        type = necro_symtable_get(codegen->symtable, ast->bind.var.id)->type;
        ast  = ast->bind.expr;
    }
    else if (ast->expr_type == NECRO_CORE_EXPR_LET)
    {
        type = necro_symtable_get(codegen->symtable, ast->let.bind->bind.var.id)->type;
        ast  = ast->let.bind->bind.expr;
    }
    else
    {
        assert(false && "necro_is_node_ast_a_closure_value called on non-bind/non-let ast");
    }
    NecroType* for_alls = type;
    NecroType* arrows   = NULL;
    while (for_alls->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* for_all_context = type->for_all.context;
        while (for_all_context != NULL)
        {
            if (ast->expr_type == NECRO_CORE_EXPR_VAR)
                return true;
            else
                return false;
            for_all_context = for_all_context->next;
        }
        for_alls = for_alls->for_all.type;
        for_alls = necro_find(codegen->infer, for_alls);
    }
    arrows = for_alls;
    arrows = necro_find(codegen->infer, arrows);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        if (ast->expr_type == NECRO_CORE_EXPR_VAR)
            return true;
        else
            return false;
        arrows = arrows->fun.type2;
        arrows = necro_find(codegen->infer, arrows);
    }
    return false;
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

    NecroArenaSnapshot snapshot   = necro_get_arena_snapshot(&codegen->snapshot_arena);
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
    closure_value              = necro_maybe_cast(codegen, closure_value, LLVMPointerType(necro_get_ast_llvm_type(codegen, function), 0));
    LLVMTypeRef  closure_type  = LLVMTypeOf(closure_value);
    // TODO: Need a maybe_cast here for polymorphic return types!

    size_t       num_c_elems   = LLVMCountStructElementTypes(LLVMGetElementType(closure_type));
    LLVMTypeRef* closure_elems = necro_snapshot_arena_alloc(&codegen->snapshot_arena, num_c_elems * sizeof(LLVMTypeRef));
    LLVMGetStructElementTypes(LLVMGetElementType(closure_type), closure_elems);
    LLVMTypeRef   fn_type      = closure_elems[1];

    // const char* fn_type_str  = LLVMPrintTypeToString(closure_type);

    LLVMTypeRef*       params     = necro_snapshot_arena_alloc(&codegen->snapshot_arena, arg_count * sizeof(LLVMTypeRef));
    LLVMValueRef*      args       = necro_paged_arena_alloc(&codegen->arena, (arg_count + 2) * sizeof(LLVMValueRef));
    size_t             arg_index  = arg_count + 1;

    LLVMGetParamTypes(LLVMGetElementType(fn_type), params);
    function                      = ast;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_calculate_node_call(codegen, function->app.exprB, outer);
        args[arg_index] = necro_maybe_cast(codegen, args[arg_index], params[arg_index - 2]);
        arg_index--;
        function        = function->app.exprA;
    }
    // TODO: Need to load this!
    args[0]                  = necro_snapshot_gep(codegen, "necro_app_ptr", LLVMGetParam(outer->call_function, 0), 2, (uint32_t[]) { 0, persistent_slot });
    args[0]                  = LLVMBuildLoad(codegen->builder, args[0], "necro_app_val");
    args[1]                  = closure_value;
    LLVMValueRef app_closure = necro_get_closure_type_bucket(&codegen->closure_type_table, codegen, fn_type).closure_app;
    LLVMValueRef result      = necro_build_call(codegen, app_closure, args, arg_count + 2, "app_closure_result");
    *out_result = result;
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return true;
}

///////////////////////////////////////////////////////
// NecroClosureType
///////////////////////////////////////////////////////
LLVMValueRef necro_create_closure_mk_fn(NecroCodeGen* codegen, LLVMTypeRef closure_type, LLVMTypeRef function_ptr_type, const char* closure_type_string)
{
    const char*       mk_closure_fn_name      = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "mk_", closure_type_string });
    LLVMTypeRef       mk_closure_fn_params[3] = { function_ptr_type, LLVMPointerType(LLVMFunctionType(LLVMPointerType(codegen->necro_val_type, 0), NULL, 0, false), 0), LLVMPointerType(codegen->necro_env_type, 0) };
    LLVMValueRef      mk_closure_fn           = necro_snapshot_add_function(codegen, mk_closure_fn_name, LLVMPointerType(closure_type, 0), mk_closure_fn_params, 3);
    LLVMBasicBlockRef entry                   = LLVMAppendBasicBlock(mk_closure_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);

    LLVMValueRef function_ptr            = LLVMGetParam(mk_closure_fn, 0);
    LLVMValueRef node_mk_function_ptr    = LLVMGetParam(mk_closure_fn, 1);
    LLVMValueRef env                     = LLVMGetParam(mk_closure_fn, 2);

    LLVMValueRef void_ptr    = necro_alloc_codegen(codegen, closure_type, 4);
    LLVMValueRef closure_ptr = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(closure_type, 0), "closure_ptr");

    LLVMValueRef has_node = LLVMBuildICmp(codegen->builder, LLVMIntEQ, node_mk_function_ptr, LLVMConstPointerNull(LLVMPointerType(LLVMFunctionType(LLVMPointerType(codegen->necro_val_type, 0), NULL, 0, false), 0)), "has_node");
    has_node              = LLVMBuildCast(codegen->builder, LLVMZExt, has_node, LLVMInt32TypeInContext(codegen->context), "has_node");
    LLVMValueRef has_env  = LLVMBuildICmp(codegen->builder, LLVMIntEQ, env, LLVMConstPointerNull(LLVMPointerType(codegen->necro_env_type, 0)), "has_env");
    has_env               = LLVMBuildCast(codegen->builder, LLVMZExt, has_env, LLVMInt32TypeInContext(codegen->context), "has_env");
    has_env               = LLVMBuildShl(codegen->builder, has_env, LLVMConstInt(LLVMTypeOf(has_env), 1, 0), "has_env");
    LLVMValueRef tag      = LLVMBuildOr(codegen->builder, has_node, has_env, "tag");
    LLVMValueRef tag_ptr  = necro_snapshot_gep(codegen, "tag_ptr", closure_ptr, 3, (uint32_t[]) { 0, 0, 1 });
    LLVMBuildStore(codegen->builder, tag, tag_ptr);

    LLVMValueRef function_ptr_ptr = necro_snapshot_gep(codegen, "function_ptr_ptr", closure_ptr, 2, (int32_t[]) { 0, 1 });
    LLVMBuildStore(codegen->builder, function_ptr, function_ptr_ptr);

    LLVMValueRef node_mk_function_ptr_ptr = necro_snapshot_gep(codegen, "node_mk_function_ptr_ptr", closure_ptr, 2, (int32_t[]) { 0, 2 });
    LLVMBuildStore(codegen->builder, node_mk_function_ptr, node_mk_function_ptr_ptr);

    LLVMValueRef env_ptr = necro_snapshot_gep(codegen, "env_ptr", closure_ptr, 2, (int32_t[]) { 0, 3 });
    LLVMBuildStore(codegen->builder, env, env_ptr);

    LLVMBuildRet(codegen->builder, closure_ptr);
    return mk_closure_fn;
}

NecroClosureTypeBucket necro_create_closure_type(NecroCodeGen* codegen, LLVMTypeRef function_ptr_type, size_t hash)
{
    LLVMTypeRef function_type   = LLVMGetElementType(function_ptr_type);
    // Closure Type
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char* function_type_string  = LLVMPrintTypeToString(function_ptr_type);
    char* curr                  = function_type_string;
    char* new_type_string       = necro_snapshot_arena_alloc(&codegen->snapshot_arena, strlen(function_type_string));
    size_t i = 0;
    while (*curr != '\0')
    {
        if (isalnum(*curr) || *curr == '[' || *curr == ']' || *curr == '(' || *curr == ')' || *curr == ',')
        {
            new_type_string[i] = *curr;
            i++;
        }
        curr++;
    }
    new_type_string[i] = '\0';
    LLVMTypeRef mk_node_fn_type      = LLVMPointerType(LLVMFunctionType(LLVMPointerType(codegen->necro_val_type, 0), NULL, 0, false), 0);
    const char* closure_type_string  = necro_concat_strings(&codegen->snapshot_arena, 3, (const char*[]) { "[", new_type_string, "]"});
    LLVMTypeRef closure_elems[4]     = { codegen->necro_data_type, function_ptr_type, mk_node_fn_type, LLVMPointerType(codegen->necro_env_type, 0) };
    LLVMTypeRef closure_type         = LLVMStructCreateNamed(codegen->context, closure_type_string);
    LLVMStructSetBody(closure_type, closure_elems, 4, false);

    // Closure app
    const char*        app_name       = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "app_", closure_type_string });
    size_t             num_params     = LLVMCountParamTypes(function_type);
    LLVMTypeRef*       app_params     = necro_paged_arena_alloc(&codegen->arena, (1 + num_params) * sizeof(LLVMTypeRef));
    app_params[0]                     = LLVMPointerType(codegen->necro_closure_app_type, 0);
    app_params[1]                     = LLVMPointerType(closure_type, 0);
    LLVMGetParamTypes(function_type, app_params + 2);
    LLVMValueRef       app_closure    = necro_snapshot_add_function(codegen, app_name, LLVMGetReturnType(function_type), app_params, num_params + 2);
    LLVMBasicBlockRef  entry          = LLVMAppendBasicBlock(app_closure, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef       necro_app_val  = LLVMGetParam(app_closure, 0);
    LLVMValueRef       closure_val    = LLVMGetParam(app_closure, 1);
    LLVMValueRef       closure_fn_ptr = necro_snapshot_gep(codegen, "closure_fn_ptr", closure_val, 2, (uint32_t[]) { 0, 1 });
    LLVMValueRef       closure_fn     = LLVMBuildLoad(codegen->builder, closure_fn_ptr, "closure_fn");
    LLVMValueRef       app_node_ptr   = necro_snapshot_gep(codegen, "app_node_ptr", necro_app_val, 2, (uint32_t[]) { 0, 1 });
    LLVMBasicBlockRef  null_null      = LLVMAppendBasicBlock(app_closure, "null_null");
    LLVMBasicBlockRef  node_null      = LLVMAppendBasicBlock(app_closure, "node_null");
    LLVMBasicBlockRef  node_null_init = LLVMAppendBasicBlock(app_closure, "node_null_init");
    LLVMBasicBlockRef  node_null_cont = LLVMAppendBasicBlock(app_closure, "node_null_cont");
    LLVMBasicBlockRef  null_env       = LLVMAppendBasicBlock(app_closure, "null_env");
    LLVMBasicBlockRef  node_env       = LLVMAppendBasicBlock(app_closure, "node_env");
    LLVMBasicBlockRef  node_env_init  = LLVMAppendBasicBlock(app_closure, "node_env_init");
    LLVMBasicBlockRef  node_env_cont  = LLVMAppendBasicBlock(app_closure, "node_env_cont");
    LLVMBasicBlockRef  error          = LLVMAppendBasicBlock(app_closure, "error");
    LLVMValueRef       tag            = LLVMBuildLoad(codegen->builder, necro_snapshot_gep(codegen, "tag_ptr", closure_val, 3, (uint32_t[]) { 0, 0, 1 }), "tag_val");
    LLVMValueRef prev_mk_node_fn_ptr  = necro_snapshot_gep(codegen, "prev_mk_node_fn_ptr", necro_app_val, 2, (uint32_t[]) { 0, 2 });
    LLVMValueRef       switch_val     = LLVMBuildSwitch(codegen->builder, tag, error, 4);
    LLVMAddCase(switch_val, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 3, false), null_null);
    LLVMAddCase(switch_val, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 2, false), node_null);
    LLVMAddCase(switch_val, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false), null_env);
    LLVMAddCase(switch_val, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), node_env);

    // null_null
    {
        LLVMPositionBuilderAtEnd(codegen->builder, null_null);
        LLVMBuildStore(codegen->builder, LLVMConstNull(LLVMPointerType(codegen->necro_val_type, 0)), app_node_ptr);
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(LLVMFunctionType(LLVMPointerType(codegen->necro_val_type, 0), NULL, 0, false), 0)), prev_mk_node_fn_ptr);
        LLVMValueRef* null_null_args = necro_paged_arena_alloc(&codegen->arena, num_params * sizeof(LLVMValueRef));
        for (size_t i = 0; i < num_params; ++i)
            null_null_args[i] = LLVMGetParam(app_closure, i + 2);
        LLVMValueRef  null_null_result = necro_build_call(codegen, closure_fn, null_null_args, num_params, "null_null_result");
        LLVMBuildRet(codegen->builder, null_null_result);
    }

    // node_null
    {
        LLVMPositionBuilderAtEnd(codegen->builder, node_null);
        LLVMValueRef mk_node_fn_ptr      = necro_snapshot_gep(codegen, "mk_node_fn_ptr", closure_val, 2, (uint32_t[]) { 0, 2 });
        LLVMValueRef mk_node_fn          = LLVMBuildLoad(codegen->builder, mk_node_fn_ptr, "mk_node_fn");
        LLVMValueRef prev_mk_node_fn     = LLVMBuildLoad(codegen->builder, prev_mk_node_fn_ptr, "prev_mk_node_fn");
        LLVMValueRef is_prev_mk_node     = LLVMBuildICmp(codegen->builder, LLVMIntEQ, mk_node_fn, prev_mk_node_fn, "is_prev_mk_node");
        LLVMBuildCondBr(codegen->builder, is_prev_mk_node, node_null_cont, node_null_init);

        // init
        LLVMPositionBuilderAtEnd(codegen->builder, node_null_init);
        LLVMBuildStore(codegen->builder, mk_node_fn, prev_mk_node_fn_ptr);
        LLVMValueRef new_node_val = necro_build_call(codegen, mk_node_fn, NULL, 0, "new_node_val");
        LLVMBuildStore(codegen->builder, new_node_val, app_node_ptr);
        LLVMBuildBr(codegen->builder, node_null_cont);

        // cont
        LLVMPositionBuilderAtEnd(codegen->builder, node_null_cont);
        LLVMValueRef  node_val       = LLVMBuildLoad(codegen->builder, app_node_ptr, "node_val");
        LLVMTypeRef*  node_fn_params = necro_paged_arena_alloc(&codegen->arena, (1 + num_params) * sizeof(LLVMTypeRef));
        node_fn_params[0]            = LLVMTypeOf(node_val);
        LLVMGetParamTypes(function_type, node_fn_params + 1);
        LLVMTypeRef   node_fn_type   = LLVMFunctionType(LLVMGetReturnType(function_type), node_fn_params, 1 + num_params, false);
        LLVMValueRef  node_fn        = LLVMBuildBitCast(codegen->builder, closure_fn, LLVMPointerType(node_fn_type, 0), "node_fn");
        LLVMValueRef* args           = necro_paged_arena_alloc(&codegen->arena, (1 + num_params) * sizeof(LLVMValueRef));
        args[0]                      = node_val;
        for (size_t i = 0; i < num_params; ++i)
            args[i + 1] = LLVMGetParam(app_closure, i + 2);
        LLVMValueRef  node_null_result = necro_build_call(codegen, node_fn, args, num_params + 1, "node_null_result");
        LLVMBuildRet(codegen->builder, node_null_result);
    }

    // null_env
    {
        LLVMPositionBuilderAtEnd(codegen->builder, null_env);
        LLVMBuildStore(codegen->builder, LLVMConstNull(LLVMPointerType(codegen->necro_val_type, 0)), app_node_ptr);
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(LLVMFunctionType(LLVMPointerType(codegen->necro_val_type, 0), NULL, 0, false), 0)), prev_mk_node_fn_ptr);
        LLVMValueRef  env_ptr       = necro_snapshot_gep(codegen, "env_ptr", closure_val, 2, (uint32_t[]) { 0, 3 });
        LLVMValueRef  env_val       = LLVMBuildLoad(codegen->builder, env_ptr, "env_val");
        LLVMTypeRef*  env_fn_params = necro_paged_arena_alloc(&codegen->arena, (1 + num_params) * sizeof(LLVMTypeRef));
        env_fn_params[0]            = LLVMPointerType(codegen->necro_env_type, 0);
        LLVMGetParamTypes(function_type, env_fn_params + 1);
        LLVMTypeRef   env_fn_type   = LLVMFunctionType(LLVMGetReturnType(function_type), env_fn_params, 1 + num_params, false);
        LLVMValueRef  env_fn        = LLVMBuildBitCast(codegen->builder, closure_fn, LLVMPointerType(env_fn_type, 0), "env_fn");
        LLVMValueRef* null_env_args = necro_paged_arena_alloc(&codegen->arena, (1 + num_params) * sizeof(LLVMValueRef));
        null_env_args[0]            = env_val;
        for (size_t i = 0; i < num_params; ++i)
            null_env_args[i + 1] = LLVMGetParam(app_closure, i + 2);
        LLVMValueRef  null_env_result = necro_build_call(codegen, env_fn, null_env_args, num_params + 1, "null_env_result");
        LLVMBuildRet(codegen->builder, null_env_result);
    }

    // node_env
    {
        LLVMPositionBuilderAtEnd(codegen->builder, node_env);
        LLVMValueRef mk_node_fn_ptr      = necro_snapshot_gep(codegen, "mk_node_fn_ptr", closure_val, 2, (uint32_t[]) { 0, 2 });
        LLVMValueRef mk_node_fn          = LLVMBuildLoad(codegen->builder, mk_node_fn_ptr, "mk_node_fn");
        // LLVMValueRef prev_mk_node_fn_ptr = necro_snapshot_gep(codegen, "prev_mk_node_fn_ptr", necro_app_val, 2, (uint32_t[]) { 0, 2 });
        LLVMValueRef prev_mk_node_fn     = LLVMBuildLoad(codegen->builder, prev_mk_node_fn_ptr, "prev_mk_node_fn");
        LLVMValueRef is_prev_mk_node     = LLVMBuildICmp(codegen->builder, LLVMIntEQ, mk_node_fn, prev_mk_node_fn, "is_prev_mk_node");
        LLVMBuildCondBr(codegen->builder, is_prev_mk_node, node_env_cont, node_env_init);

        // init
        LLVMPositionBuilderAtEnd(codegen->builder, node_env_init);
        LLVMBuildStore(codegen->builder, mk_node_fn, prev_mk_node_fn_ptr);
        LLVMValueRef new_node_val = necro_build_call(codegen, mk_node_fn, NULL, 0, "new_node_val");
        LLVMBuildStore(codegen->builder, new_node_val, app_node_ptr);
        LLVMBuildBr(codegen->builder, node_env_cont);

        // cont
        LLVMPositionBuilderAtEnd(codegen->builder, node_env_cont);
        LLVMValueRef  node_val       = LLVMBuildLoad(codegen->builder, app_node_ptr, "node_val");
        LLVMValueRef  env_ptr        = necro_snapshot_gep(codegen, "env_ptr", closure_val, 2, (uint32_t[]) { 0, 3 });
        LLVMValueRef  env_val        = LLVMBuildLoad(codegen->builder, env_ptr, "env_val");
        LLVMTypeRef*  node_fn_params = necro_paged_arena_alloc(&codegen->arena, (2 + num_params) * sizeof(LLVMTypeRef));
        node_fn_params[0]            = LLVMTypeOf(node_val);
        node_fn_params[1]            = LLVMTypeOf(env_val);
        LLVMGetParamTypes(function_type, node_fn_params + 2);
        LLVMTypeRef   node_fn_type   = LLVMFunctionType(LLVMGetReturnType(function_type), node_fn_params, 2 + num_params, false);
        LLVMValueRef  node_env_fn    = LLVMBuildBitCast(codegen->builder, closure_fn, LLVMPointerType(node_fn_type, 0), "node_env_fn");
        LLVMValueRef* args           = necro_paged_arena_alloc(&codegen->arena, (2 + num_params) * sizeof(LLVMValueRef));
        args[0]                      = node_val;
        args[1]                      = env_val;
        for (size_t i = 0; i < num_params; ++i)
            args[i + 2] = LLVMGetParam(app_closure, i + 2);
        LLVMValueRef  node_env_result = necro_build_call(codegen, node_env_fn, args, num_params + 2, "node_env_result");
        LLVMBuildRet(codegen->builder, node_env_result);
    }

    // error
    LLVMPositionBuilderAtEnd(codegen->builder, error);
    LLVMValueRef error_call = necro_build_call(codegen, codegen->runtime->functions.necro_error_exit, (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 2, false) }, 1, "");
    LLVMSetInstructionCallConv(error_call, LLVMCCallConv);
    LLVMBuildUnreachable(codegen->builder);

    LLVMValueRef mk_closure = necro_create_closure_mk_fn(codegen, closure_type, function_ptr_type, closure_type_string);

    // Cleanup and return
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    LLVMDisposeMessage(function_type_string);
    return (NecroClosureTypeBucket)
    {
        .function_type = function_ptr_type,
        .closure_type  = closure_type,
        .closure_app   = app_closure,
        .mk_closure    = mk_closure,
        .hash          = hash
    };
}

NecroClosureTypeTable necro_create_closure_type_table()
{
    static const size_t initial_size = 1024;
    NecroClosureTypeTable table;
    table.buckets = malloc(sizeof(NecroClosureTypeBucket) * initial_size);
    table.count   = 0;
    table.size    = initial_size;
    for (size_t i = 0; i < initial_size; ++i)
    {
        table.buckets[i].function_type = NULL;
        table.buckets[i].closure_type  = NULL;
        table.buckets[i].mk_closure    = NULL;
        table.buckets[i].hash          = 0;
    }
    return table;
}

void necro_destroy_closure_type_table(NecroClosureTypeTable* table)
{
    if (table->buckets != NULL)
        free(table->buckets);
    table->count = 0;
    table->size  = 0;
}

void necro_closure_type_table_grow(NecroClosureTypeTable* table, NecroCodeGen* codegen)
{
    assert(table != NULL);
    NecroClosureTypeBucket* old_buckets = table->buckets;
    size_t                  old_size    = table->size;
    table->size                         = old_size * 2;
    table->buckets                      = malloc(table->size * sizeof(NecroClosureTypeBucket));
    for (size_t i = 0; i < table->size; ++i)
    {
        table->buckets[i] = (NecroClosureTypeBucket) { NULL, NULL, NULL, 0 };
    }
    for (size_t i = 0; i < old_size; ++i)
    {
        NecroClosureTypeBucket old_bucket = old_buckets[i];
        if (old_bucket.closure_type == NULL)
            continue;
        size_t hash   = old_bucket.hash;
        size_t bucket = hash & (table->size - 1);
        while (true)
        {
            if (table->buckets[bucket].closure_type == NULL)
            {
                table->buckets[bucket] = old_bucket;
                break;
            }
            bucket = (bucket + 1) & (table->size - 1);
        }
    }
    free(old_buckets);
}

NecroClosureTypeBucket necro_get_closure_type_bucket(NecroClosureTypeTable* table, NecroCodeGen* codegen, LLVMTypeRef function_type)
{
    assert(table != NULL);
    assert(function_type != NULL);
    if (table->count >= table->size * 2)
        necro_closure_type_table_grow(table, codegen);
    size_t hash   = ((size_t)function_type) * 37;
    size_t bucket = hash & (table->size - 1);
    while (true)
    {
        if (table->buckets[bucket].function_type == function_type)
        {
            return table->buckets[bucket];
        }
        else if (table->buckets[bucket].function_type == NULL)
        {
            table->buckets[bucket] = necro_create_closure_type(codegen, function_type, hash);
            table->count++;
            return table->buckets[bucket];
        }
        bucket = (bucket + 1) & (table->size - 1);
    }
}

LLVMTypeRef necro_get_closure_type(NecroClosureTypeTable* table, NecroCodeGen* codegen, LLVMTypeRef function_type)
{
    NecroClosureTypeBucket bucket = necro_get_closure_type_bucket(table, codegen, function_type);
    return bucket.closure_type;
}

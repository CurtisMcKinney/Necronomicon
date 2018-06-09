/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen.h"
#include <stdarg.h>
#include <ctype.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include "symtable.h"
#include "runtime/runtime.h"

// things to reference:
//     * https://pauladamsmith.com/blog/2015/01/how-to-get-started-with-llvm-c-api.html
//     * https://llvm.org/docs/LangRef.html#instruction-reference
//     * https://llvm.org/docs/tutorial/LangImpl04.html
//     * http://www.stephendiehl.com/llvm/
//     * https://lowlevelbits.org/how-to-use-llvm-api-with-swift.-addendum/

// TODO: Easy method to grab top level things with symbols
// TODO: LLVM License
// TODO: Core really should have source locations...
// TODO: Things that really need to happen:
//    - A Runtime with:
//        * Memory allocation onto the heap
//        * Value retrieval from the heap
//        * GC
//        * Continuous updates

//=====================================================
// Forward declaration
//=====================================================
void                necro_calculate_node_prototype(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node);
NecroNodePrototype* necro_create_necro_node_prototype(NecroCodeGen* codegen, NecroVar bind_var, const char* name, LLVMTypeRef node_type, LLVMTypeRef node_value_type, NecroNodePrototype* outer, NECRO_NODE_TYPE type);
NecroNodePrototype* necro_declare_node_prototype(NecroCodeGen* codegen, NecroVar bind_var);

//=====================================================
// Create / Destroy
//=====================================================
NecroCodeGen necro_create_codegen(NecroInfer* infer, NecroIntern* intern, NecroSymTable* symtable, NecroRuntime* runtime, const char* module_name)
{
    LLVMLinkInMCJIT();
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    LLVMContextRef     context          = LLVMContextCreate();
    LLVMModuleRef      mod              = LLVMModuleCreateWithNameInContext(module_name, context);
    // LLVMTargetDataRef  target          = LLVMCreateTargetData(LLVMGetDefaultTargetTriple());
    // LLVMSetTarget(mod, LLVMGetDefaultTargetTriple());
    // LLVMCreateTargetDataLayout()
    // LLVMSetDataLayout(mod, LLVMCreateDataLayout
    LLVMTargetDataRef  target           = LLVMCreateTargetData(LLVMGetTarget(mod));
    // Optimizations, turn on when you want the code to go fast!
    LLVMPassManagerRef fn_pass_manager  = LLVMCreateFunctionPassManagerForModule(mod);
    LLVMPassManagerRef mod_pass_manager = LLVMCreatePassManager();
    // LLVMAddTailCallEliminationPass(fn_pass_manager);
    // LLVMAddCFGSimplificationPass(fn_pass_manager);
    // LLVMAddDeadStoreEliminationPass(fn_pass_manager);
    // LLVMAddInstructionCombiningPass(fn_pass_manager);
    LLVMInitializeFunctionPassManager(fn_pass_manager);
    LLVMFinalizeFunctionPassManager(fn_pass_manager);
    // LLVMPassManagerBuilderRef pass_manager_builder = LLVMPassManagerBuilderCreate();
    // LLVMPassManagerBuilderSetOptLevel(pass_manager_builder, 2);
    // LLVMPassManagerBuilderUseInlinerWithThreshold(pass_manager_builder, 2);
    // LLVMPassManagerBuilderPopulateModulePassManager(pass_manager_builder, mod_pass_manager);
    return (NecroCodeGen)
    {
        .arena            = necro_create_paged_arena(),
        .snapshot_arena   = necro_create_snapshot_arena(),
        .infer            = infer,
        .intern           = intern,
        .symtable         = symtable,
        .runtime          = runtime,
        .context          = context,
        .mod              = mod,
        .builder          = LLVMCreateBuilderInContext(context),
        .target           = target,
        .fn_pass_manager  = fn_pass_manager,
        .mod_pass_manager = mod_pass_manager,
        .error            = necro_create_error(),
    };
}

void necro_destroy_codegen(NecroCodeGen* codegen)
{
    necro_destroy_paged_arena(&codegen->arena);
    necro_destroy_snapshot_arena(&codegen->snapshot_arena);
    LLVMContextDispose(codegen->context);
    LLVMDisposeBuilder(codegen->builder);
}

//=====================================================
// Utility
//=====================================================
size_t necro_expression_list_count(NecroCoreAST_Expression* list)
{
    if (list == NULL) return 0;
    assert(list->expr_type == NECRO_CORE_EXPR_LIST);
    size_t count = 0;
    while (list != NULL)
    {
        count++;
        list = list->list.next;
    }
    return count;
}

size_t necro_data_con_count(NecroCoreAST_DataCon* con)
{
    if (con == NULL) return 0;
    size_t count = 0;
    while (con != NULL)
    {
        count++;
        con = con->next;
    }
    return count;
}

inline LLVMTypeRef necro_get_data_type(NecroCodeGen* codegen)
{
    return necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_data_type.id)->llvm_type;
}

inline LLVMTypeRef necro_get_value_ptr(NecroCodeGen* codegen)
{
    return LLVMPointerType(necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->llvm_type, 0);
}

LLVMValueRef necro_snapshot_gep(NecroCodeGen* codegen, const char* ptr_name, LLVMValueRef data_ptr, size_t num_indices, uint32_t* indices)
{
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef* index_refs = necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMValueRef) * num_indices);
    for (size_t i = 0; i < num_indices; ++i)
    {
        index_refs[i] = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), indices[i], false);
    }
    LLVMValueRef gep = LLVMBuildGEP(codegen->builder, data_ptr, index_refs, num_indices, ptr_name);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return gep;
}

LLVMValueRef necro_snapshot_add_function(NecroCodeGen* codegen, const char* function_name, LLVMTypeRef return_type, LLVMTypeRef* arg_refs, size_t arg_count)
{
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef function = LLVMAddFunction(codegen->mod, function_name, LLVMFunctionType(return_type, arg_refs, arg_count, false));
    LLVMSetFunctionCallConv(function, LLVMFastCallConv);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return function;
}

LLVMValueRef necro_build_call(NecroCodeGen* codegen, LLVMValueRef function, LLVMValueRef* args, uint32_t arg_count, const char* result_name)
{
    LLVMValueRef result = LLVMBuildCall(codegen->builder, function, args, arg_count, result_name);
    LLVMSetInstructionCallConv(result, LLVMFastCallConv);
    return result;
}

NECRO_DECLARE_SMALL_ARRAY(LLVMTypeRef, LLVMTypeRef, llvm_type_ref, 5);

size_t necro_codegen_count_num_args(NecroCodeGen* codegen, NecroCoreAST_DataCon* con)
{
    if (necro_is_codegen_error(codegen)) return 0;
    assert(codegen != NULL);
    assert(con != NULL);
    size_t count = 0;
    NecroCoreAST_Expression* args = con->arg_list;
    while (args != NULL)
    {
        assert(args->expr_type == NECRO_CORE_EXPR_LIST);
        count++;
        args = args->list.next;
    }
    return count;
}

// TODO: Make non-variadic
// char* necro_concat_strings(NecroSnapshotArena* arena,...)
char* necro_concat_strings(NecroSnapshotArena* arena, uint32_t string_count, const char** strings)
{
    size_t total_length = 1;
    for (size_t i = 0; i < string_count; ++i)
    {
        total_length += strlen(strings[i]);
    }
    char* buffer = necro_snapshot_arena_alloc(arena, total_length);
    buffer[0] = '\0';
    for (size_t i = 0; i < string_count; ++i)
    {
        strcat(buffer, strings[i]);
    }
    return buffer;
}

void necro_init_necro_data(NecroCodeGen* codegen, LLVMValueRef data_ptr, uint32_t value_1, uint32_t value_2)
{
    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef       necro_data_1     = necro_snapshot_gep(codegen, "necro_data_1", data_ptr, 3, (uint32_t[]) { 0, 0, 0 });
    LLVMValueRef       necro_data_2     = necro_snapshot_gep(codegen, "necro_data_2", data_ptr, 3, (uint32_t[]) { 0, 0, 1 });
    LLVMValueRef       const_value_1    = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), value_1, false);
    LLVMValueRef       const_value_2    = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), value_2, false);
    LLVMBuildStore(codegen->builder, const_value_1, necro_data_1);
    LLVMBuildStore(codegen->builder, const_value_2, necro_data_2);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

LLVMValueRef necro_gen_alloc_boxed_value(NecroCodeGen* codegen, LLVMTypeRef boxed_type, uint32_t necro_value1, uint32_t necro_value2, const char* name)
{
    NecroArenaSnapshot snapshot     = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef       void_ptr     = necro_alloc_codegen(codegen, LLVMABISizeOfType(codegen->target, boxed_type));
    LLVMValueRef       data_ptr     = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(boxed_type, 0), name);
    LLVMValueRef       necro_data_1 = necro_snapshot_gep(codegen, "necro_data_1", data_ptr, 3, (uint32_t[]) { 0, 0, 0 });
    LLVMValueRef       necro_data_2 = necro_snapshot_gep(codegen, "necro_data_2", data_ptr, 3, (uint32_t[]) { 0, 0, 1 });
    LLVMBuildStore(codegen->builder, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), necro_value1, false), necro_data_1);
    LLVMBuildStore(codegen->builder, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), necro_value2, false), necro_data_2);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return data_ptr;
}

//=====================================================
// CodeGen
//=====================================================
// TODO: I imagine this will be much more complicated once we take into account continuous update semantics
// Or maybe not? We're not lazy after all. Strict semantics means value will be precomputed...
// However, at the very least we're going to have to think about retrieving the value from the heap
// and GC.
LLVMValueRef necro_codegen_variable(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    LLVMValueRef value = necro_symtable_get(codegen->symtable, ast->var.id)->llvm_value;
    if (value == NULL)
    {
        assert(false && "Compiler bug: NULL value in necro_codegen_variable");
        return NULL;
    }
    return value;
}

// TODO: FINISH!
bool is_app_stateless(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    return false;
}

bool necro_is_a_function_type(NecroCodeGen* codegen, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(codegen->infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_FOR:
    {
        NecroType* for_alls = type;
        while (for_alls->type == NECRO_TYPE_FOR)
        {
            NecroTypeClassContext* for_all_context = type->for_all.context;
            if (for_all_context != NULL)
                return true;
            for_alls = for_alls->for_all.type;
        }
    }
    case NECRO_TYPE_FUN: return true;
    default:             return false;
    }
}

LLVMTypeRef necro_type_to_llvm_type(NecroCodeGen* codegen, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(codegen->infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:
    {
        LLVMTypeRef llvm_type = necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->llvm_type; // TODO: Is this right?!?!
        if (llvm_type == NULL)
        {
            llvm_type = necro_type_to_llvm_type(codegen, necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->type);
            necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->llvm_type = llvm_type;
        }
        return llvm_type;
    }
    case NECRO_TYPE_APP:  assert(false); return NULL;
    case NECRO_TYPE_LIST: return necro_symtable_get(codegen->symtable, codegen->infer->prim_types->list_type.id)->llvm_type;
    case NECRO_TYPE_CON:  return necro_symtable_get(codegen->symtable, type->con.con.id)->llvm_type;
    case NECRO_TYPE_FUN:  // !FALLTHROUGH!
    case NECRO_TYPE_FOR:
    {
        size_t arg_count = 0;
        size_t arg_index = 0;
        // Count args
        NecroType* for_alls = type;
        while (for_alls->type == NECRO_TYPE_FOR)
        {
            NecroTypeClassContext* for_all_context = type->for_all.context;
            while (for_all_context != NULL)
            {
                arg_count++;
                for_all_context = for_all_context->next;
            }
            for_alls = for_alls->for_all.type;
        }
        NecroType* arrows = for_alls;
        while (arrows->type == NECRO_TYPE_FUN)
        {
            arg_count++;
            arrows = arrows->fun.type2;
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
        }
        arrows = for_alls;
        while (arrows->type == NECRO_TYPE_FUN)
        {
            args[arg_index] = LLVMPointerType(necro_type_to_llvm_type(codegen, arrows->fun.type1), 0);
            arg_index++;
            arrows = arrows->fun.type2;
        }
        LLVMTypeRef return_type   = LLVMPointerType(necro_type_to_llvm_type(codegen, arrows), 0);
        LLVMTypeRef function_type = LLVMFunctionType(return_type, args, arg_count, false);
        return function_type;
    }
    default: assert(false);
    }
    return NULL;
}

// TODO: FINISH!
// TODO: Only meant for constructors!?
LLVMTypeRef necro_get_ast_llvm_type(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER: return LLVMInt64TypeInContext(codegen->context);
        case NECRO_AST_CONSTANT_FLOAT:   return LLVMFloatTypeInContext(codegen->context);
        default:                         assert(false); return NULL;
        }
    case NECRO_CORE_EXPR_VAR:
    {
        NecroSymbolInfo* info = necro_symtable_get(codegen->symtable, ast->var.id);
        if (info->llvm_type == NULL)
            info->llvm_type = necro_type_to_llvm_type(codegen, info->type);
        assert(info->llvm_type != NULL);
        return info->llvm_type;
    }
    case NECRO_CORE_EXPR_BIND:
    {
        NecroSymbolInfo* info = necro_symtable_get(codegen->symtable, ast->bind.var.id);
        if (info->llvm_type == NULL)
            info->llvm_type = necro_type_to_llvm_type(codegen, info->type);
        assert(info->llvm_type != NULL);
        return info->llvm_type;
    }
    case NECRO_CORE_EXPR_LAM:
    {
        // Function binding
        // Derive LLVMType from function signature!
        NecroCoreAST_Expression* lambdas = ast;
        while (lambdas->expr_type == NECRO_CORE_EXPR_LAM)
        {
            lambdas = lambdas->lambda.expr;
        }
        assert(false && "TODO: Finish!");
        return NULL;
    }
    case NECRO_CORE_EXPR_DATA_CON:  return necro_symtable_get(codegen->symtable, ast->data_con.condid.id)->llvm_type;
    case NECRO_CORE_EXPR_APP:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
}

//=====================================================
// DataCon
//=====================================================
void necro_codegen_data_con(NecroCodeGen* codegen, NecroCoreAST_DataCon* con, LLVMTypeRef data_type_ref, size_t max_arg_count, uint32_t con_number)
{
    if (necro_is_codegen_error(codegen)) return;
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*  con_name  = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(codegen->intern, con->condid.symbol) });
    size_t arg_count = necro_codegen_count_num_args(codegen, con);
    assert(arg_count <= max_arg_count);
    NecroLLVMTypeRefArray    args      = necro_create_llvm_type_ref_array(arg_count);
    NecroCoreAST_Expression* arg       = con->arg_list;
    for (size_t i = 0; i < arg_count; ++i)
    {
        assert(arg->expr_type == NECRO_CORE_EXPR_LIST);
        LLVMTypeRef arg_type = necro_get_ast_llvm_type(codegen, arg->list.expr);
        if (necro_is_codegen_error(codegen))
            goto necro_codegen_data_con_cleanup;
        *necro_llvm_type_ref_array_get(&args, i) = LLVMPointerType(arg_type, 0);
        arg = arg->list.next;
    }
    LLVMTypeRef       ret_type = LLVMFunctionType(LLVMPointerType(data_type_ref, 0), necro_llvm_type_ref_array_get(&args, 0), arg_count, 0);
    LLVMValueRef      make_con = LLVMAddFunction(codegen->mod, con_name, ret_type);
    LLVMSetFunctionCallConv(make_con, LLVMFastCallConv);
    LLVMBasicBlockRef entry    = LLVMAppendBasicBlock(make_con, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef      void_ptr = necro_alloc_codegen(codegen, LLVMABISizeOfType(codegen->target, data_type_ref));
    LLVMValueRef      data_ptr = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(data_type_ref, 0), "data_ptr");
    necro_init_necro_data(codegen, data_ptr, 0, con_number);
    //--------------
    // Parameters
    for (size_t i = 0; i < max_arg_count; ++i)
    {
        char itoa_buff[6];
        char* location_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "slot_", itoa(i, itoa_buff, 10) });
        LLVMValueRef slot = necro_snapshot_gep(codegen, location_name, data_ptr, 2, (uint32_t[]) { 0, i + 1 });
        if (i < arg_count)
        {
            char itoa_buff_2[6];
            char* value_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "param_", itoa(i, itoa_buff_2, 10) });
            LLVMValueRef value = LLVMBuildBitCast(codegen->builder, LLVMGetParam(make_con, i), necro_get_value_ptr(codegen), value_name);
            LLVMBuildStore(codegen->builder, value, slot);
        }
        else
        {
            LLVMBuildStore(codegen->builder, LLVMConstPointerNull(necro_get_value_ptr(codegen)), slot);
        }
    }
    LLVMBuildRet(codegen->builder, data_ptr);
    // Create empty NecroNodePrototype that is stateless!
    // struct NecroNodePrototype* data_con_prototype = necro_declare_node_prototype(codegen, (NecroVar) { .id = con->condid.id, .symbol = con->condid.symbol });
    // data_con_prototype->type = NECRO_NODE_STATELESS;
    NecroNodePrototype* prototype = necro_create_prim_node_prototype(codegen, con->condid, data_type_ref, make_con, NECRO_NODE_STATELESS);
    prototype->call_function = make_con;
necro_codegen_data_con_cleanup:
    necro_destroy_llvm_type_ref_array(&args);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

LLVMValueRef necro_codegen_data_declaration(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);

    size_t max_arg_count = 0;
    NecroCoreAST_DataCon* con = ast->data_decl.con_list;
    while (con != NULL)
    {
        size_t arg_count = necro_codegen_count_num_args(codegen, con);
        max_arg_count    = (arg_count > max_arg_count) ? arg_count : max_arg_count;
        con = con->next;
    }
    NecroLLVMTypeRefArray elems = necro_create_llvm_type_ref_array(max_arg_count + 1);
    *necro_llvm_type_ref_array_get(&elems, 0) = necro_get_data_type(codegen);
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    {
        *necro_llvm_type_ref_array_get(&elems, i) = necro_get_value_ptr(codegen);
    }
    // Data Type
    LLVMTypeRef data_type_ref = LLVMStructCreateNamed(codegen->context, necro_intern_get_string(codegen->intern, ast->data_con.condid.symbol));
    LLVMStructSetBody(data_type_ref, necro_llvm_type_ref_array_get(&elems, 0), max_arg_count + 1, false);
    necro_symtable_get(codegen->symtable, ast->data_con.condid.id)->llvm_type = data_type_ref;

    con = ast->data_decl.con_list;
    size_t con_number = 0;
    while (con != NULL)
    {
        necro_codegen_data_con(codegen, con, data_type_ref, max_arg_count, con_number);
        con = con->next;
        con_number++;
    }

    // Cleanup
    necro_destroy_llvm_type_ref_array(&elems);
    return NULL;
}

//=====================================================
// Node
//=====================================================

// Lit doesn't define a Node, and doesn't Update
// (Let / Bind !?!?) without args defines a Node (struct, init_function, update_function), and introduces Node instance and updates it.
// (Let / Bind ?!?!!?) with args defines a Node (struct, init_function, update_function), but doesn't introduce a Node instance, and doesn't Update.
// Var with no args doesn't define a Node but queries the state of a Node instance or is applied as a function
// App doesn't define a Node, introduces a node instance and updates it.
// Lam at the begining of a Bind defines a function / Node. Doesn't introduce a Node type or update function, and doesn't introduce a node instance or update it.
// Lam as a value is part of the mess related to Higher-order functions/closures. Left for future Curtis to figure out.
// Case doesn't introduce an Node or Update, but doesn't generate stateless code.

// Notes:
//     * Inline plain values!...But how to distingush!? Higher order functions strike again
//     * If all owned nodes are cached at the very top level node
//     * Then it is simple to capture variables, even for functions!?
//     * A function is necessarily stateless if it's node contains no state nodes (no captured variables and no owned nodes)

NecroNodePrototype* necro_create_necro_node_prototype(NecroCodeGen* codegen, NecroVar bind_var, const char* name, LLVMTypeRef node_type, LLVMTypeRef node_value_type, NecroNodePrototype* outer, NECRO_NODE_TYPE type)
{
    NecroNodePrototype* node_prototype   = necro_paged_arena_alloc(&codegen->arena, sizeof(NecroNodePrototype));
    node_prototype->bind_var             = bind_var;
    node_prototype->name                 = necro_intern_string(codegen->intern, name);
    node_prototype->node_type            = node_type;
    node_prototype->node_value_type      = node_value_type;
    node_prototype->args                 = NULL;
    node_prototype->loaded_vars          = NULL;
    node_prototype->called_functions     = NULL;
    node_prototype->local_vars           = NULL;
    node_prototype->persistent_vars      = NULL;
    node_prototype->captured_vars        = NULL;
    node_prototype->mk_function          = NULL;
    node_prototype->init_function        = NULL;
    node_prototype->call_function        = NULL;
    node_prototype->was_captured         = false;
    node_prototype->outer                = outer;
    node_prototype->slot_count           = 0;
    node_prototype->type                 = type;
    return node_prototype;
}

NecroNodePrototype* necro_create_prim_node_prototype(NecroCodeGen* codegen, NecroVar prim_var, LLVMTypeRef prim_result_type, LLVMValueRef prim_call_function, NECRO_NODE_TYPE type)
{
    NecroNodePrototype* prototype = necro_create_necro_node_prototype(codegen, prim_var, necro_intern_get_string(codegen->intern, prim_var.symbol), prim_result_type, prim_result_type, NULL, type);
    prototype->call_function = prim_call_function;
    necro_symtable_get(codegen->symtable, prim_var.id)->node_prototype = prototype;
    return prototype;
}

// Takes a list of captured variables as arguments to make functions, along with pointer to memory!
LLVMValueRef necro_codegen_mk_node(NecroCodeGen* codegen, NecroNodePrototype* node)
{
    // if (node->type == NECRO_NODE_STATELESS) return NULL;
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(node != NULL);

    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*              mk_name   = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(codegen->intern, node->name) });
    LLVMValueRef       mk_alloc  = necro_snapshot_add_function(codegen, mk_name, LLVMPointerType(node->node_type, 0), NULL, 0);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(mk_alloc, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef       void_ptr  = necro_alloc_codegen(codegen, LLVMABISizeOfType(codegen->target, node->node_type));
    LLVMValueRef       node_ptr  = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(node->node_type, 0), "node_ptr");
    necro_init_necro_data(codegen, node_ptr, 0, 0);
    LLVMValueRef       value_ptr = necro_snapshot_gep(codegen, "value_ptr", node_ptr, 2, (uint32_t[]) { 0, 1 });
    LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(node->node_value_type, 0)), value_ptr);

    //--------------
    // NULL out Captured Vars
    size_t i = 2;
    NecroCapturedVarList* captured_variables = node->captured_vars;
    while (captured_variables != NULL)
    {
        LLVMValueRef node = necro_snapshot_gep(codegen, "captured", node_ptr, 2, (uint32_t[]) { 0, i });
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(captured_variables->data.prototype->node_type, 0)), node);
        i++;
        captured_variables = captured_variables->next;
    }

    //--------------
    // NULL out Persistent Vars
    NecroPersistentVarList* persistent_vars = node->persistent_vars;
    while (persistent_vars != NULL)
    {
        LLVMValueRef node = necro_snapshot_gep(codegen, "persistent", node_ptr, 2, (uint32_t[]) { 0, i });
        LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(persistent_vars->data.prototype->node_type, 0)), node);
        i++;
        persistent_vars = persistent_vars->next;
    }

    LLVMBuildRet(codegen->builder, node_ptr);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return mk_alloc;
}

// TODO: FINISH!
// TODO: delay primitive!
// Only call init at the beginning of the first block that the node will be used in!
// This preserves the ability to do self and mutual recursion without going infinite!
LLVMValueRef necro_codegen_init_node(NecroCodeGen* codegen, NecroNodePrototype* node)
{
    // if (node->type == NECRO_NODE_STATELESS) return NULL;
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(node != NULL);
    assert(node->node_type != NULL);
    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);

    // Construct init function type
    char*              init_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "_init", necro_intern_get_string(codegen->intern, node->name) });
    // const size_t       arg_count = 1 + necro_count_captured_var_list(node->captured_vars);
    const size_t       arg_count = 1;
    LLVMTypeRef*       args      = necro_paged_arena_alloc(&codegen->arena, sizeof(LLVMTypeRef) * arg_count);
    args[0]                      = LLVMPointerType(node->node_type, 0);

    // TODO: New scheme for captured vars!!!
    // //--------------
    // // NULL out Captured Vars
    // NecroCapturedVarList* captured_variables = node->captured_vars;
    // size_t arg_index = 1;
    // while (captured_variables != NULL)
    // {
    //     args[arg_index] = LLVMPointerType(captured_variables->data.prototype->node_type, 0);
    //     arg_index++;
    //     captured_variables = captured_variables->next;
    // }

    LLVMValueRef       init_fn   = necro_snapshot_add_function(codegen, init_name, LLVMVoidTypeInContext(codegen->context), args, arg_count);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(init_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef       tag_ptr   = necro_snapshot_gep(codegen, "tag_ptr", LLVMGetParam(init_fn, 0), 3, (uint32_t[]) { 0, 0, 1 });
    LLVMBuildStore(codegen->builder, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false), tag_ptr);

    // //--------------
    // // Assign captured variables
    // size_t i = 2;
    // captured_variables = node->captured_vars;
    // while (captured_variables != NULL)
    // {
    //     LLVMValueRef node = necro_snapshot_gep(codegen, "captured", LLVMGetParam(init_fn, 0), 2, (uint32_t[]) { 0, i });
    //     LLVMBuildStore(codegen->builder, LLVMGetParam(init_fn, i - 1), node);
    //     i++;
    //     captured_variables = captured_variables->next;
    // }

    //--------------
    // alloc each persistent node with appropriate mk_node function
    NecroPersistentVarList* persistent_vars = node->persistent_vars;
    while (persistent_vars != NULL)
    {
        // TODO: This is NULL on mutally recursive functions...how to fix!?
        LLVMValueRef owned_data = necro_build_call(codegen, persistent_vars->data.prototype->mk_function, NULL, 0, "persistent");
        LLVMValueRef owned_ptr  = necro_snapshot_gep(codegen, "persistent_ptr", LLVMGetParam(init_fn, 0), 2, (uint32_t[]) { 0, persistent_vars->data.slot + 2 });
        LLVMBuildStore(codegen->builder, owned_data, owned_ptr);
        // i++;
        persistent_vars = persistent_vars->next;
    }

    LLVMBuildRetVoid(codegen->builder);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return init_fn;
}

NecroNodePrototype* necro_declare_node_prototype(NecroCodeGen* codegen, NecroVar bind_var)
{
    NecroNodePrototype* prototype = necro_symtable_get(codegen->symtable, bind_var.id)->node_prototype;
    if (prototype != NULL)
        return prototype;
    char  buf2[10];
    char* node_name = necro_concat_strings(&codegen->snapshot_arena, 4, (const char*[]) { necro_intern_get_string(codegen->intern, bind_var.symbol), "_", itoa(bind_var.id.id, buf2, 10), "_Node" });
    node_name[0]    = toupper(node_name[0]);
    LLVMTypeRef         node_type_ref  = LLVMStructCreateNamed(codegen->context, node_name);
    LLVMTypeRef         value_type     = necro_type_to_llvm_type(codegen, necro_symtable_get(codegen->symtable, bind_var.id)->type);
    if (necro_is_a_function_type(codegen, necro_symtable_get(codegen->symtable, bind_var.id)->type))
        value_type = LLVMGetElementType(LLVMGetReturnType(value_type));
    assert(value_type != NULL);
    NecroNodePrototype* node_prototype = necro_create_necro_node_prototype(codegen, bind_var, node_name, node_type_ref, value_type, NULL, NECRO_NODE_STATEFUL);
    // NecroNodePrototype* node_prototype = necro_create_necro_node_prototype(codegen, bind_var, node_name, node_type_ref, value_type, NULL, NECRO_NODE_STATELESS); // IS THIS A BAD IDEA!?!?!?
    necro_symtable_get(codegen->symtable, bind_var.id)->node_prototype = node_prototype;
    return node_prototype;
}

// TODO: How to detect statefulness across mutually recursive variables!?
void necro_calculate_captured_vars(NecroCodeGen* codegen, NecroNodePrototype* prototype)
{
    NecroVarList* loaded_vars = prototype->loaded_vars;
    while (loaded_vars != NULL)
    {
        if (necro_symtable_get(codegen->symtable, loaded_vars->data.id)->is_constructor)
            goto necro_calculate_captured_vars_next; // Constructor, don't capture
        NecroArgList* args = prototype->args;
        while (args != NULL)
        {
            if (loaded_vars->data.id.id == args->data.var.id.id)
                goto necro_calculate_captured_vars_next; // Arg, don't capture
            args = args->next;
        }
        NecroVarList* local_vars = prototype->local_vars;
        while (local_vars != NULL)
        {
            if (loaded_vars->data.id.id == local_vars->data.id.id)
                goto necro_calculate_captured_vars_next; // Local var, don't capture
            local_vars = local_vars->next;
        }
        NecroNodePrototype* loaded_prototype = necro_declare_node_prototype(codegen, loaded_vars->data);
        assert(loaded_prototype != NULL);
        if (loaded_prototype->outer == NULL)
            goto necro_calculate_captured_vars_next; // Global var, don't capture
        NecroCapturedVarList* captured_vars = prototype->captured_vars;
        while (captured_vars != NULL)
        {
            if (loaded_vars->data.id.id == captured_vars->data.var.id.id)
                goto necro_calculate_captured_vars_next; // Already captured, don't capture again
            captured_vars = captured_vars->next;
        }
        loaded_prototype->was_captured = true;
        NecroCapturedVar captured_var  = (NecroCapturedVar) { .prototype = loaded_prototype, .var = loaded_vars->data, .slot = prototype->slot_count };
        prototype->captured_vars       = necro_cons_captured_var_list(&codegen->arena, captured_var, prototype->captured_vars);
        prototype->slot_count++;
necro_calculate_captured_vars_next:
        loaded_vars = loaded_vars->next;
    }
    prototype->captured_vars = necro_reverse_captured_var_list(&codegen->arena, prototype->captured_vars);
}

void necro_calculate_persistent_vars(NecroCodeGen* codegen, NecroNodePrototype* prototype)
{
    // Nodes are persistent if they are either captured or they contain state.
    NecroVarList* local_vars = prototype->local_vars;
    while (local_vars != NULL)
    {
        NecroNodePrototype* local_prototype = necro_declare_node_prototype(codegen, local_vars->data);
        assert(local_prototype != NULL);
        if (local_prototype->was_captured || local_prototype->type == NECRO_NODE_STATEFUL)
        {
            NecroPersistentVar persistent_var = (NecroPersistentVar) { .prototype = local_prototype, .slot = prototype->slot_count };
            prototype->persistent_vars = necro_cons_persistent_var_list(&codegen->arena, persistent_var, prototype->persistent_vars);
            necro_symtable_get(codegen->symtable, local_vars->data.id)->persistent_slot = prototype->slot_count + 2;
            prototype->slot_count++;
        }
        local_vars = local_vars->next;
    }
    // Iterate through function calls as well
    NecroCallList* calls = prototype->called_functions;
    while (calls != NULL)
    {
        NecroNodePrototype* call_prototype = necro_declare_node_prototype(codegen, calls->data.var);
        assert(call_prototype != NULL);
        if (call_prototype->type == NECRO_NODE_STATEFUL && call_prototype != prototype)
        {
            NecroPersistentVar persistent_var = (NecroPersistentVar) { .prototype = call_prototype, .slot = prototype->slot_count };
            prototype->persistent_vars = necro_cons_persistent_var_list(&codegen->arena, persistent_var, prototype->persistent_vars);
            calls->data.is_persistent  = true;
            calls->data.slot           = prototype->slot_count + 2;
            necro_symtable_get(codegen->symtable, calls->data.var.id)->persistent_slot = 1; // This is not the ACTUAL slot used for function calls, which are instanced at each call site
            calls->data.app->persistent_slot = prototype->slot_count + 2;
            prototype->slot_count++;
        }
        else
        {
            calls->data.is_persistent = false;
        }
        calls = calls->next;
    }
}

void necro_fixup_stateful_recursion(NecroCodeGen* codegen, NecroNodePrototype* prototype)
{
    // If we turn out to be stateful in the end, make sure any recursive calls to ourself is also stateful!
    NecroCallList* calls = prototype->called_functions;
    while (calls != NULL)
    {
        NecroNodePrototype* call_prototype = necro_declare_node_prototype(codegen, calls->data.var);
        assert(call_prototype != NULL);
        if (call_prototype->type == NECRO_NODE_STATEFUL && call_prototype == prototype)
        {
            NecroPersistentVar persistent_var = (NecroPersistentVar) { .prototype = call_prototype, .slot = prototype->slot_count };
            prototype->persistent_vars = necro_cons_persistent_var_list(&codegen->arena, persistent_var, prototype->persistent_vars);
            calls->data.is_persistent  = true;
            calls->data.slot           = prototype->slot_count + 2;
            necro_symtable_get(codegen->symtable, calls->data.var.id)->persistent_slot = 1; // This is not the ACTUAL slot used for function calls, which are instanced at each call site
            calls->data.app->persistent_slot = prototype->slot_count + 2;
            prototype->slot_count++;
        }
        else
        {
            calls->data.is_persistent = false;
        }
        calls = calls->next;
    }
}

// A node is stateful if it is recursive, or has captured variables or has persistent variables, or calls a stateful function
void necro_calc_statefulness(NecroCodeGen* codegen, NecroNodePrototype* prototype, bool is_recursive)
{
    if (is_recursive)
    {
        prototype->type = NECRO_NODE_STATEFUL;
        return;
    }
    if (prototype->captured_vars != NULL)
    {
        prototype->type = NECRO_NODE_STATEFUL;
        return;
    }
    if (prototype->persistent_vars != NULL)
    {
        prototype->type = NECRO_NODE_STATEFUL;
        return;
    }
    NecroCallList* calls = prototype->called_functions;
    while (calls != NULL)
    {
        if (calls->data.is_persistent)
        {
            prototype->type = NECRO_NODE_STATEFUL;
            return;
        }
        calls = calls->next;
    }
    prototype->type = NECRO_NODE_STATELESS;
}

void necro_finish_node_prototype(NecroCodeGen* codegen, NecroNodePrototype* prototype, bool is_recursive)
{
    // We consed onto lists, need to reverse
    prototype->args             = necro_reverse_arg_list(&codegen->arena,  prototype->args);
    prototype->loaded_vars      = necro_reverse_var_list(&codegen->arena,  prototype->loaded_vars);
    prototype->called_functions = necro_reverse_call_list(&codegen->arena, prototype->called_functions);
    prototype->local_vars       = necro_reverse_var_list(&codegen->arena,  prototype->local_vars);
    necro_calculate_captured_vars(codegen, prototype);
    necro_calculate_persistent_vars(codegen, prototype);
    necro_calc_statefulness(codegen, prototype, is_recursive);
    necro_fixup_stateful_recursion(codegen, prototype);
    prototype->persistent_vars = necro_reverse_persistent_var_list(&codegen->arena, prototype->persistent_vars);
    //--------------------
    // Create Node body
    size_t       num_persistent_vars = necro_count_persistent_var_list(prototype->persistent_vars);
    size_t       num_captured_vars   = necro_count_captured_var_list(prototype->captured_vars);
    size_t       num_elems           = 2 + num_persistent_vars + num_captured_vars;
    LLVMTypeRef* node_elems          = necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMTypeRef) * num_elems);
    node_elems[0]                    = necro_get_data_type(codegen);
    node_elems[1]                    = LLVMPointerType(prototype->node_value_type, 0);
    size_t elem = 2;
    NecroPersistentVarList* persistent_vars = prototype->persistent_vars;
    while (persistent_vars != NULL)
    {
        node_elems[elem] = LLVMPointerType(persistent_vars->data.prototype->node_type, 0);
        elem++;
        persistent_vars  = persistent_vars->next;
    }
    NecroCapturedVarList* captured_variables = prototype->captured_vars;
    while (captured_variables != NULL)
    {
        node_elems[elem] = LLVMPointerType(captured_variables->data.prototype->node_type, 0);
        elem++;
        captured_variables  = captured_variables->next;
    }
    LLVMStructSetBody(prototype->node_type, node_elems, num_elems, false);

    // TODO: Finish!
    //--------------------
    // Create Node functions
    prototype->mk_function   = necro_codegen_mk_node(codegen, prototype);
    prototype->init_function = necro_codegen_init_node(codegen, prototype);
    // necro_codegen_update_node(codegen, node_prototype);
}

void necro_calculate_node_prototype_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    NecroArenaSnapshot  snapshot       = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroNodePrototype* node_prototype = necro_declare_node_prototype(codegen, ast->bind.var);
    node_prototype->outer = outer_node;
    // TODO: Better way to distinguish function from value!
    if (ast->bind.expr->expr_type != NECRO_CORE_EXPR_LAM)
    {
        // Go Deeper
        // Always use the most outer node?!?!!?
        // Does this work with persistence and recursion!?!?!?
        if (outer_node == NULL)
        // if (outer_node == NULL || ast->bind.is_recursive)
            necro_calculate_node_prototype(codegen, ast->bind.expr, node_prototype);
        else
            necro_calculate_node_prototype(codegen, ast->bind.expr, outer_node);
        assert(node_prototype->args == NULL);
        if (outer_node == NULL)
        {
            //--------------------
            // Global
            // Create global instance
            char buf2[10];
            const char* bind_name = necro_concat_strings(&codegen->snapshot_arena, 3, (const char*[]) { necro_intern_get_string(codegen->intern, ast->bind.var.symbol), "_", itoa(ast->bind.var.id.id, buf2, 10) });
            LLVMTypeRef bind_type = necro_get_ast_llvm_type(codegen, ast);
            assert(bind_type != NULL);
            LLVMValueRef bind_value = LLVMAddGlobal(codegen->mod, node_prototype->node_type, bind_name);
            LLVMSetLinkage(bind_value, LLVMCommonLinkage);
            // LLVMSetInitializer(bind_value, LLVMConstPointerNull(LLVMPointerType(node_prototype->node_type, 0)));
            LLVMSetInitializer(bind_value, LLVMConstNamedStruct(node_prototype->node_type, NULL, 0));
            LLVMSetGlobalConstant(bind_value, false);
            necro_symtable_get(codegen->symtable, ast->bind.var.id)->llvm_value = bind_value;
        }
        else
        {
            //--------------------
            // Local
            outer_node->local_vars = necro_cons_var_list(&codegen->arena, ast->bind.var, outer_node->local_vars);
        }
    }
    else
    {
        // Collect args
        NecroCoreAST_Expression* lambdas = ast->bind.expr;
        while (lambdas->expr_type == NECRO_CORE_EXPR_LAM)
        {

            NecroArg necro_arg = (NecroArg) { .llvm_type = necro_get_ast_llvm_type(codegen, lambdas->lambda.arg), .var = (NecroVar) { .id = lambdas->lambda.arg->var.id, .symbol = lambdas->lambda.arg->var.symbol } };
            node_prototype->args = necro_cons_arg_list(&codegen->arena, necro_arg, node_prototype->args);
            lambdas = lambdas->lambda.expr;
        }
        // Go Deeper (Lambda always goes deeper)
        necro_calculate_node_prototype(codegen, lambdas, node_prototype);
    }
    necro_finish_node_prototype(codegen, node_prototype, ast->bind.is_recursive);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

void necro_calculate_node_prototype_let(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LET);
    assert(ast->let.bind != NULL);
    assert(ast->let.bind->expr_type == NECRO_CORE_EXPR_BIND);
    necro_calculate_node_prototype_bind(codegen, ast->let.bind, outer_node);
    necro_calculate_node_prototype(codegen, ast->let.expr, outer_node);
}

void necro_calculate_node_prototype_var(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    outer_node->loaded_vars = necro_cons_var_list(&codegen->arena, ast->bind.var, outer_node->loaded_vars);
}

void necro_calculate_node_prototype_lam(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LAM);
    // ONLY USE THIS FOR TRULY ANONYMOUS FUNCTIONS!!!!
    assert(false && "TODO: Finish!");
}

void necro_calculate_node_prototype_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(false && "TODO: Finish!");
}

void necro_calculate_node_prototype_app(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    assert(ast->app.exprA != NULL);
    assert(ast->app.exprB != NULL);
    // TODO: FINISH!
    // NOTE/HACK: For now assuming the arguments provided exactly match.
    // NOTE/HACK: For now assuming that the function is variable and not an anonymous function
    NecroCoreAST_Expression*  function  = ast;
    size_t                    arg_count = 0;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    if (function->expr_type != NECRO_CORE_EXPR_VAR)
    {
        assert(false && "Currently not supporting anonymous function or higher order functions!");
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
    // assert(necro_symtable_get(codegen->symtable, function->var.id)->node_prototype != NULL);
    // TODO/HACK: Forward declaring for now...but we need some kind of safety net to make sure we haven't bungled things seriously!
    necro_declare_node_prototype(codegen, function->var);
    NecroCall call = (NecroCall)
    {
        .var           = function->var,
        .is_persistent = false,
        .slot          = 0,
        .app           = &ast->app
    };
    outer_node->called_functions = necro_cons_call_list(&codegen->arena, call, outer_node->called_functions);
}

// Perhaps do full on ast crawl and store these into symtable.
// Then we can reference them whenever we want!
void necro_calculate_node_prototype(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LET:       necro_calculate_node_prototype_let(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_BIND:      necro_calculate_node_prototype_bind(codegen, ast, outer_node); return;
    case NECRO_CORE_EXPR_APP:       necro_calculate_node_prototype_app(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_VAR:       necro_calculate_node_prototype_var(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_LAM:       necro_calculate_node_prototype_lam(codegen, ast, outer_node);  return;
    case NECRO_CORE_EXPR_CASE:      necro_calculate_node_prototype_case(codegen, ast, outer_node); return;
    case NECRO_CORE_EXPR_LIT:       return; // No node behavior
    // Should never happen
    case NECRO_CORE_EXPR_DATA_DECL: assert(false && "Should never reach DATA_DECL case in necro_calculate_node_prototype"); return;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "Should never reach DATA_CON  case in necro_calculate_node_prototype"); return;
    case NECRO_CORE_EXPR_TYPE:      assert(false && "Should never reach EXPR_TYPE case in necro_calculate_node_prototype"); return;
    case NECRO_CORE_EXPR_LIST:      assert(false && "Should never reach EXPR_LIST case in necro_calculate_node_prototype"); return; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_calculate_node_prototype"); return;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

//=====================================================
// Codegen Call function
//=====================================================
LLVMValueRef necro_calculate_node_call(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer);
// Bind returns call function!
LLVMValueRef necro_calculate_node_call_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    assert(outer == NULL);

    NecroArenaSnapshot  snapshot       = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroNodePrototype* node_prototype = necro_symtable_get(codegen->symtable, ast->bind.var.id)->node_prototype;
    assert(node_prototype != NULL);
    assert(node_prototype->outer == NULL);
    // const char*  call_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]){ "_call", necro_intern_get_string(codegen->intern, ast->bind.var.symbol) });
    char buf2[10];
    char* call_name = necro_concat_strings(&codegen->snapshot_arena, 5, (const char*[]) { "_call", necro_intern_get_string(codegen->intern, ast->bind.var.symbol), "_", itoa(ast->bind.var.id.id, buf2, 10), "_Node" });
    call_name[5] = toupper(call_name[5]);
    // LLVMValueRef call_fn   = NULL;
    //-----------------
    // Set up args
    uint32_t arg_count = necro_count_arg_list(node_prototype->args);
    uint32_t arg_index = 0;
    if (node_prototype->type == NECRO_NODE_STATEFUL)
    {
        arg_count++;
        arg_index++;
    }
    LLVMTypeRef*  args     = necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMTypeRef)* arg_count);
    NecroArgList* arg_list = node_prototype->args;
    while (arg_list != NULL)
    {
        args[arg_index] = LLVMPointerType(arg_list->data.llvm_type, 0);
        arg_index++;
        arg_list = arg_list->next;
    }
    NecroCoreAST_Expression* bind_expr = ast->bind.expr;
    while (bind_expr->expr_type == NECRO_CORE_EXPR_LAM)
        bind_expr = bind_expr->lambda.expr;
    //-----------------
    // Declare function
    if (node_prototype->type == NECRO_NODE_STATEFUL)
        args[0] = LLVMPointerType(node_prototype->node_type, 0);
    LLVMValueRef call_fn = necro_snapshot_add_function(codegen, call_name, LLVMPointerType(node_prototype->node_value_type, 0), args, arg_count);
    node_prototype->call_function = call_fn;
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(call_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    //-----------------
    // Cache arg locations
    arg_list = node_prototype->args;
    if (node_prototype->type == NECRO_NODE_STATEFUL)
        arg_index = 1;
    else
        arg_index = 0;
    while (arg_list != NULL)
    {
        necro_symtable_get(codegen->symtable, arg_list->data.var.id)->llvm_value = LLVMGetParam(call_fn, arg_index);
        arg_index++;
        arg_list = arg_list->next;
    }
    if (node_prototype->type == NECRO_NODE_STATEFUL)
    {
        // TODO: Move captured variable parameters into mk_ function, not init_ function!
        LLVMValueRef      tag_ptr    = necro_snapshot_gep(codegen, "tag_ptr", LLVMGetParam(call_fn, 0), 3, (uint32_t[]) { 0, 0, 1 });
        LLVMValueRef      tag_value  = LLVMBuildLoad(codegen->builder, tag_ptr, "tag_value");
        LLVMValueRef      needs_init = LLVMBuildICmp(codegen->builder, LLVMIntEQ, LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false), tag_value, "needs_init");
        LLVMBasicBlockRef init_block = LLVMAppendBasicBlock(call_fn, "init_node");
        LLVMBasicBlockRef cont_block = LLVMAppendBasicBlock(call_fn, "cont_block");
        LLVMBuildCondBr(codegen->builder, needs_init, init_block, cont_block);
        LLVMPositionBuilderAtEnd(codegen->builder, init_block);
        necro_build_call(codegen, node_prototype->init_function, (LLVMValueRef[]) { LLVMGetParam(call_fn, 0) }, 1, "");
        LLVMBuildBr(codegen->builder, cont_block);
        LLVMPositionBuilderAtEnd(codegen->builder, cont_block);
        LLVMValueRef result = necro_calculate_node_call(codegen, bind_expr, node_prototype);
        if (LLVMTypeOf(result) != LLVMPointerType(node_prototype->node_value_type, 0))
        {
            result = LLVMBuildBitCast(codegen->builder, result, LLVMPointerType(node_prototype->node_value_type, 0), "cast_result");
        }
        if (arg_count == 1)
        {
            LLVMValueRef top_node_ptr = necro_snapshot_gep(codegen, "top_node_ptr", LLVMGetParam(call_fn, 0), 2, (uint32_t[]) { 0, 1 });
            LLVMBuildStore(codegen->builder, result, top_node_ptr);
        }
        else if (LLVMIsACallInst(result))
        {
            LLVMSetTailCall(result, true);
        }
        LLVMBuildRet(codegen->builder, result);
    }
    else
    {
        LLVMValueRef result = necro_calculate_node_call(codegen, bind_expr, node_prototype);
        if (LLVMIsACallInst(result))
            LLVMSetTailCall(result, true);
        LLVMBuildRet(codegen->builder, result);
    }
    LLVMRunFunctionPassManager(codegen->fn_pass_manager, call_fn);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    // TODO: Way to access global bound values!
    return call_fn;
}

LLVMValueRef necro_calculate_node_call_let(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LET);
    assert(ast->let.bind != NULL);
    assert(ast->let.bind->expr_type == NECRO_CORE_EXPR_BIND);

    NecroArenaSnapshot  snapshot       = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroSymbolInfo*    bind_var_info  = necro_symtable_get(codegen->symtable, ast->let.bind->bind.var.id);
    NecroNodePrototype* bind_prototype = bind_var_info->node_prototype;
    assert(bind_prototype != NULL);
    char buf[10];
    const char* var_name = necro_concat_strings(&codegen->snapshot_arena, 3, (const char*[]) { necro_intern_get_string(codegen->intern, ast->let.bind->bind.var.symbol), "_", itoa(ast->let.bind->bind.var.id.id, buf, 10) });
    LLVMValueRef feed_ptr = NULL;
    if (bind_var_info->persistent_slot != 0)
    {
        bind_var_info->llvm_type    = bind_prototype->node_value_type;
        const char* node_ptr_name   = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { var_name, "_node_ptr" });
        LLVMValueRef node_ptr       = necro_snapshot_gep(codegen, node_ptr_name, LLVMGetParam(outer->call_function, 0), 2, (uint32_t[]) { 0, bind_var_info->persistent_slot });
        const char* node_val_name   = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { var_name, "_node_val" });
        LLVMValueRef node_val       = LLVMBuildLoad(codegen->builder, node_ptr, node_val_name);
        const char* per_ptr_name    = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { var_name, "_per_ptr" });
        feed_ptr                    = necro_snapshot_gep(codegen, per_ptr_name, node_val, 2, (uint32_t[]) { 0, 1 });
        const char* per_val_name    = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { var_name, "_val_ptr" });
        LLVMValueRef persistent_reg = LLVMBuildLoad(codegen->builder, feed_ptr, per_val_name);
        bind_var_info->llvm_value   = persistent_reg;
    }
    LLVMValueRef bind_result = necro_calculate_node_call(codegen, ast->let.bind->bind.expr, outer);
    LLVMSetValueName2(bind_result, var_name, strlen(var_name));
    bind_var_info->llvm_value = bind_result;

    // Store it back at the end!
    if (bind_var_info->persistent_slot != 0)
    {
        LLVMBuildStore(codegen->builder, bind_result, feed_ptr);
    }

    LLVMValueRef result = necro_calculate_node_call(codegen, ast->let.expr, outer);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return result;
}

LLVMValueRef necro_calculate_node_call_var(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    NecroSymbolInfo* info = necro_symtable_get(codegen->symtable, ast->var.id);
    if (info->is_constructor)
    {
        LLVMValueRef con_mk_function = info->node_prototype->call_function;
        const char*  result_name     = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(codegen->intern, ast->var.symbol), "_result" });
        LLVMValueRef result          = necro_build_call(codegen, con_mk_function, NULL, 0, result_name);
        return result;
    }
    else if (info->node_prototype != NULL && info->node_prototype->outer == NULL)
    {
        // Global!
        assert(info->llvm_value != NULL && "Value not present. Maybe infinite loop?");
        LLVMValueRef global_ptr = necro_snapshot_gep(codegen, "global_node_ptr", info->llvm_value, 2, (uint32_t[]) { 0, 1 });
        return LLVMBuildLoad(codegen->builder, global_ptr, "global_node_val");
    }
    else
    {
        // assert(info->node_prototype != NULL && "Prototype should never be NULL!");
        assert(info->llvm_value != NULL && "Value not present. Maybe infinite loop?");
        return info->llvm_value;
    }
}

LLVMValueRef necro_calculate_node_call_app(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    // TODO: FINISH!
    // NOTE/HACK: For now assuming the arguments provided exactly match.
    // NOTE/HACK: For now assuming that the function is variable and not an anonymous function
    NecroCoreAST_Expression*  function        = ast;
    size_t                    arg_count       = 0;
    size_t                    persistent_slot = ast->app.persistent_slot;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    if (function->expr_type != NECRO_CORE_EXPR_VAR)
    {
        assert(false && "Currently not supporting anonymous function or higher order functions!");
    }
    if (persistent_slot != 0)
        arg_count++;
    NecroNodePrototype* call_prototype = necro_symtable_get(codegen->symtable, function->var.id)->node_prototype;
    assert(call_prototype != NULL);
    assert(call_prototype->call_function != NULL);
    // TODO: Support polymorphic functions!!!!
    NecroArenaSnapshot snapshot   = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef*      params     = necro_snapshot_arena_alloc(&codegen->snapshot_arena, arg_count * sizeof(LLVMValueRef));
    LLVMValueRef*      args       = necro_paged_arena_alloc(&codegen->arena, arg_count * sizeof(LLVMValueRef));
    size_t             arg_index  = arg_count - 1;
    function                      = ast;
    LLVMGetParams(call_prototype->call_function, params);
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_calculate_node_call(codegen, function->app.exprB, outer);
        if (LLVMTypeOf(args[arg_index]) != LLVMTypeOf(params[arg_index]))
        {
            // TODO: Put checks in to make sure we're only casting to NecroVals and back!?
            args[arg_index] = LLVMBuildBitCast(codegen->builder, args[arg_index], LLVMTypeOf(params[arg_index]), "cast_arg");
        }
        arg_index--;
        function   = function->app.exprA;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    // Pass in persistent var
    if (persistent_slot != 0)
    {
        LLVMValueRef node_ptr = necro_snapshot_gep(codegen, "per_tmp_ptr", LLVMGetParam(outer->call_function, 0), 2, (uint32_t[]) { 0, persistent_slot });
        args[0]               = LLVMBuildLoad(codegen->builder, node_ptr, "per_tmp_node");
    }
    const char*  result_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(codegen->intern, function->var.symbol), "_result" });
    LLVMValueRef result      = necro_build_call(codegen, call_prototype->call_function, args, arg_count, result_name);
    return result;
}

LLVMValueRef necro_calculate_node_call_lam(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LAM);
    // ONLY USE THIS FOR TRULY ANONYMOUS FUNCTIONS!!!!
    assert(false && "TODO: Finish!");
    return NULL;
}

LLVMValueRef necro_calculate_node_call_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(false && "TODO: Finish!");
    return NULL;
}

// TODO: Turn all of this into a separate function to be called?!
LLVMValueRef necro_calculate_node_call_lit(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LIT);
    // LLVMTypeRef  boxed_type;
    // LLVMValueRef raw_value;
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_INTEGER:
        LLVMValueRef unboxed_int  = LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->lit.int_literal, true);
        return necro_build_call(codegen, necro_symtable_get(codegen->symtable, codegen->infer->prim_types->int_type.id)->node_prototype->mk_function, (LLVMValueRef[]) { unboxed_int }, 1, "boxed_int");
    case NECRO_AST_CONSTANT_FLOAT:
        LLVMValueRef unboxed_float = LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), ast->lit.double_literal);
        return necro_build_call(codegen, necro_symtable_get(codegen->symtable, codegen->infer->prim_types->float_type.id)->node_prototype->mk_function, (LLVMValueRef[]) { unboxed_float }, 1, "boxed_float");
    case NECRO_AST_CONSTANT_CHAR:
        // raw_value = LLVMConstInt(LLVMInt8TypeInContext(codegen->context), ast->lit.char_literal, true);
        // boxed_type = necro_symtable_get(codegen->symtable, codegen->infer->prim_types->char_type.id)->llvm_type;
        return NULL;
    default:
        return NULL;
    }
}

LLVMValueRef necro_calculate_node_call(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LET:       return necro_calculate_node_call_let(codegen,  ast, outer);
    case NECRO_CORE_EXPR_BIND:      return necro_calculate_node_call_bind(codegen, ast, outer);
    case NECRO_CORE_EXPR_APP:       return necro_calculate_node_call_app(codegen,  ast, outer);
    case NECRO_CORE_EXPR_VAR:       return necro_calculate_node_call_var(codegen,  ast, outer);
    case NECRO_CORE_EXPR_LAM:       return necro_calculate_node_call_lam(codegen,  ast, outer);
    case NECRO_CORE_EXPR_CASE:      return necro_calculate_node_call_case(codegen, ast, outer);
    case NECRO_CORE_EXPR_LIT:       return necro_calculate_node_call_lit(codegen,  ast, outer);
    // Should never happen
    case NECRO_CORE_EXPR_DATA_DECL: assert(false && "Should never reach DATA_DECL case in necro_calculate_node_prototype"); return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "Should never reach DATA_CON  case in necro_calculate_node_prototype"); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false && "Should never reach EXPR_TYPE case in necro_calculate_node_prototype"); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false && "Should never reach EXPR_LIST case in necro_calculate_node_prototype"); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_calculate_node_prototype"); return NULL;
    }
}

//=====================================================
// Gen Main
//=====================================================
void necro_codegen_main(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LIST);
    NecroArenaSnapshot       snapshot    = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroSymbol              main_symbol = necro_intern_string(codegen->intern, "main");
    NecroNodePrototype*      main_node   = NULL;
    NecroVar                 main_var;
    NecroCoreAST_Expression* top         = ast;
    // Find necro main
    while (top != NULL)
    {
        if (top->list.expr != NULL && top->list.expr->expr_type == NECRO_CORE_EXPR_BIND)
        {
            if (top->list.expr->bind.var.symbol.id == main_symbol.id)
            {
                main_node = necro_symtable_get(codegen->symtable, top->list.expr->bind.var.id)->node_prototype;
                main_var  = top->list.expr->bind.var;
                break;
            }
        }
        top = top->list.next;
    }
    if (main_node == NULL)
    {
        necro_throw_codegen_error(codegen, ast, "No main function defined.");
        return;
    }

    // Create actual main
    LLVMValueRef       main_fn    = necro_snapshot_add_function(codegen, "main", LLVMVoidTypeInContext(codegen->context), NULL, 0);
    LLVMBasicBlockRef  entry      = LLVMAppendBasicBlock(main_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    // Initialize global nodes
    top = ast;
    while (top != NULL)
    {
        if (top->list.expr != NULL && top->list.expr->expr_type == NECRO_CORE_EXPR_BIND)
        {
            NecroSymbolInfo*    node_info = necro_symtable_get(codegen->symtable, top->list.expr->bind.var.id);
            NecroNodePrototype* node      = node_info->node_prototype;
            if (node->args != NULL)
            {
                top = top->list.next;
                continue;
            }
            LLVMValueRef node_ptr    = necro_build_call(codegen, node->mk_function, NULL, 0, "node_ptr");
            LLVMValueRef cast_node   = LLVMBuildBitCast(codegen->builder, node_ptr, LLVMPointerType(LLVMInt64TypeInContext(codegen->context), 0), "cast_node");
            LLVMValueRef cast_glob   = LLVMBuildBitCast(codegen->builder, node_info->llvm_value, LLVMPointerType(LLVMInt64TypeInContext(codegen->context), 0), "cast_glob");
            LLVMValueRef data_size   = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), LLVMABISizeOfType(codegen->target, node->node_type), false);
            LLVMValueRef is_volatile = LLVMConstInt(LLVMInt1TypeInContext(codegen->context), 1, false);
            necro_build_call(codegen, codegen->llvm_intrinsics.mem_cpy, (LLVMValueRef[]) { cast_glob, cast_node, data_size, is_volatile }, 4, "");
            // LLVMBuildStore(codegen->builder, node_ptr, node_info->llvm_value);
        }
        top = top->list.next;
    }
    // Setup main loop
    LLVMBasicBlockRef main_loop = LLVMAppendBasicBlock(main_fn, "main_loop");
    LLVMBuildBr(codegen->builder, main_loop);
    LLVMPositionBuilderAtEnd(codegen->builder, main_loop);
    // Evaluate top level nodes in a loop
    top = ast;
    while (top != NULL)
    {
        if (top->list.expr != NULL && top->list.expr->expr_type == NECRO_CORE_EXPR_BIND)
        {
            NecroSymbolInfo*    node_info = necro_symtable_get(codegen->symtable, top->list.expr->bind.var.id);
            NecroNodePrototype* node      = node_info->node_prototype;
            if (node->args != NULL)
            {
                top = top->list.next;
                continue;
            }
            LLVMValueRef node_result;
            if (node->type == NECRO_NODE_STATEFUL)
            {
                node_result = necro_build_call(codegen, node->call_function, (LLVMValueRef[]) { node_info->llvm_value }, 1, "node_result");
            }
            else
            {
                node_result = necro_build_call(codegen, node->call_function, NULL, 0, "node_result");
                LLVMValueRef value_ptr   = necro_snapshot_gep(codegen, "value_ptr", node_info->llvm_value, 2, (uint32_t[]) { 0, 1 });
                LLVMBuildStore(codegen->builder, node_result, value_ptr);
            }
            if (node == main_node)
            {
                LLVMValueRef result_ptr     = necro_snapshot_gep(codegen, "result_ptr", node_result, 2, (uint32_t[]) { 0, 1 });
                LLVMValueRef unboxed_result = LLVMBuildLoad(codegen->builder, result_ptr, "unboxed_result");
                LLVMValueRef print_instr    = necro_build_call(codegen, codegen->runtime->functions.necro_print, (LLVMValueRef[]) { unboxed_result }, 1, "");
                LLVMSetInstructionCallConv(print_instr, LLVMCCallConv);

            }
        }
        top = top->list.next;
    }
    LLVMValueRef sleep_instr = necro_build_call(codegen, codegen->runtime->functions.necro_sleep, (LLVMValueRef[]) { LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 250, false) }, 1, "");
    LLVMSetInstructionCallConv(sleep_instr, LLVMCCallConv);
    // Looping logic...
    LLVMBuildBr(codegen->builder, main_loop);
    // LLVMBuildRetVoid(codegen->builder);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

//=====================================================
// Go
//=====================================================
LLVMValueRef necro_codegen_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    necro_calculate_node_prototype(codegen, ast, NULL);
    necro_calculate_node_call(codegen, ast, NULL);
    return NULL;
}

LLVMValueRef necro_codegen_go(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_BIND:      return necro_codegen_bind(codegen, ast);
    case NECRO_CORE_EXPR_DATA_DECL: return necro_codegen_data_declaration(codegen, ast);
    case NECRO_CORE_EXPR_APP:       assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_LAM:       assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_LET:       assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_LIT:       assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_VAR:       assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "Should never reach in necro_codegen_go"); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return NULL;
}

NECRO_RETURN_CODE necro_verify_and_dump_codegen(NecroCodeGen* codegen)
{
    LLVMRunPassManager(codegen->mod_pass_manager, codegen->mod);
    char* error = NULL;
    LLVMVerifyModule(codegen->mod, LLVMAbortProcessAction, &error);
    if (strlen(error) != 0)
    {
        fprintf(stderr, "LLVM error: %s\n", error);
        LLVMDisposeMessage(error);
        return NECRO_ERROR;
    }
    LLVMDisposeMessage(error);
    const char* module_string_rep = LLVMPrintModuleToString(codegen->mod);
    printf("\n%s\n", module_string_rep);

    // LLVMDisposeBuilder(codegen->builder);
    return NECRO_SUCCESS;
}

LLVMValueRef necro_alloc_codegen(NecroCodeGen* codegen, uint64_t size)
{
    LLVMValueRef data_size = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), size, false);
    LLVMValueRef void_ptr  = necro_build_call(codegen, codegen->runtime->functions.necro_alloc, &data_size, 1, "void_ptr");
    LLVMSetInstructionCallConv(void_ptr, LLVMCCallConv);
    return void_ptr;
}

NECRO_RETURN_CODE necro_jit(NecroCodeGen* codegen)
{
    LLVMExecutionEngineRef engine;
    char* error = NULL;
    if (LLVMCreateJITCompilerForModule(&engine, codegen->mod, 0, &error) != 0)
    {
        fprintf(stderr, "necro error: %s\n", error);
        LLVMDisposeMessage(error);
        return NECRO_ERROR;
    }
    necro_bind_runtime_functions(codegen->runtime, engine);
    void(*fun)() = (void(*)())LLVMGetFunctionAddress(engine, "main");
    // printf("\n");
    // printf("Necronomicon arises...\n");
    puts("__/\\\\/\\\\\\\\\\\\_______/\\\\\\\\\\\\\\\\______/\\\\\\\\\\\\\\\\__/\\\\/\\\\\\\\\\\\\\______/\\\\\\\\\\____ ");
    puts(" _\\/\\\\\\////\\\\\\____/\\\\\\/////\\\\\\___/\\\\\\//////__\\/\\\\\\/////\\\\\\___/\\\\\\///\\\\\\__");
    puts("  _\\/\\\\\\__\\//\\\\\\__/\\\\\\\\\\\\\\\\\\\\\\___/\\\\\\_________\\/\\\\\\___\\///___/\\\\\\__\\//\\\\\\");
    puts("   _\\/\\\\\\___\\/\\\\\\_\\//\\\\///////___\\//\\\\\\________\\/\\\\\\_________\\//\\\\\\__/\\\\\\");
    puts("    _\\/\\\\\\___\\/\\\\\\__\\//\\\\\\\\\\\\\\\\\\\\__\\///\\\\\\\\\\\\\\\\_\\/\\\\\\__________\\///\\\\\\\\\\/");
    puts("     _\\///____\\///____\\//////////_____\\////////__\\///_____________\\/////_");
    (*fun)();
    // TODO: Need to dispose execution engine correctly...Put in NecroCodeGen?
    return NECRO_SUCCESS;
}

NECRO_RETURN_CODE necro_codegen(NecroCodeGen* codegen, NecroCoreAST* core_ast)
{
    assert(codegen != NULL);
    assert(core_ast != NULL);
    assert(core_ast->root->expr_type == NECRO_CORE_EXPR_LIST);
    if (necro_codegen_primitives(codegen) == NECRO_ERROR)
        return codegen->error.return_code;
    NecroCoreAST_Expression* top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_codegen_go(codegen, top_level_list->list.expr);
        if (necro_is_codegen_error(codegen)) return codegen->error.return_code;
        top_level_list = top_level_list->list.next;
    }
    necro_codegen_main(codegen, core_ast->root);
    if (necro_is_codegen_error(codegen)) return codegen->error.return_code;
    if (necro_verify_and_dump_codegen(codegen) == NECRO_ERROR)
        return NECRO_ERROR;
    return NECRO_SUCCESS;
}
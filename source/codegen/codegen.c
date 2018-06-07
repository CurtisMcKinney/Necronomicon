/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen.h"
#include <stdarg.h>
#include <ctype.h>
#include <llvm-c/Analysis.h>
#include "core/core.h"
#include "symtable.h"

// things to reference:
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

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

extern DLLEXPORT uint64_t* _necro_alloc(uint32_t size)
{
    return malloc(size);
}

//=====================================================
// Forward declaration
//=====================================================
void                necro_calculate_node_prototype(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node);
NecroNodePrototype* necro_create_necro_node_prototype(NecroCodeGen* codegen, NecroVar bind_var, const char* name, LLVMTypeRef node_type, LLVMTypeRef node_value_type, NecroNodePrototype* outer, NECRO_NODE_TYPE type);
NecroNodePrototype* necro_declare_node_prototype(NecroCodeGen* codegen, NecroVar bind_var);

//=====================================================
// Create / Destroy
//=====================================================
NecroCodeGen necro_create_codegen(NecroInfer* infer, NecroIntern* intern, NecroSymTable* symtable, const char* module_name)
{
    // LLVMInitializeNativeTarget();
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef  mod     = LLVMModuleCreateWithNameInContext(module_name, context);

    LLVMTypeRef  necro_alloc_args[1] = { LLVMInt32TypeInContext(context) };
    LLVMValueRef necro_alloc         = LLVMAddFunction(mod, "_necro_alloc", LLVMFunctionType(LLVMPointerType(LLVMInt64TypeInContext(context), 0), necro_alloc_args, 1, false));
    LLVMSetLinkage(necro_alloc, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_alloc, LLVMFastCallConv);
    _necro_alloc(0);

    return (NecroCodeGen)
    {
        .arena          = necro_create_paged_arena(),
        .snapshot_arena = necro_create_snapshot_arena(),
        .infer          = infer,
        .intern         = intern,
        .symtable       = symtable,
        .context        = context,
        .mod            = mod,
        .builder        = LLVMCreateBuilderInContext(context),
        .target         = LLVMCreateTargetData(LLVMGetTarget(mod)),
        .necro_alloc    = necro_alloc,
        .error          = necro_create_error(),
        .current_block  = (NecroBlockIndex) { .index = 0 },
        .blocks         = 0,
        .is_top_level   = true
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

LLVMValueRef necro_alloc_codegen(NecroCodeGen* codegen, uint64_t bytes)
{
    LLVMValueRef data_size     = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), bytes, false);
    LLVMValueRef void_ptr      = LLVMBuildCall(codegen->builder, codegen->necro_alloc, &data_size, 1, "void_ptr");
    LLVMSetInstructionCallConv(void_ptr, LLVMFastCallConv);
    return void_ptr;
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
    const size_t       arg_count = 1 + necro_count_captured_var_list(node->captured_vars);
    LLVMTypeRef*       args      = necro_paged_arena_alloc(&codegen->arena, sizeof(LLVMTypeRef) * arg_count);
    args[0]                      = LLVMPointerType(node->node_type, 0);

    //--------------
    // NULL out Captured Vars
    NecroCapturedVarList* captured_variables = node->captured_vars;
    size_t arg_index = 1;
    while (captured_variables != NULL)
    {
        args[arg_index] = LLVMPointerType(captured_variables->data.prototype->node_type, 0);
        arg_index++;
        captured_variables = captured_variables->next;
    }

    LLVMValueRef       init_fn   = necro_snapshot_add_function(codegen, init_name, LLVMVoidTypeInContext(codegen->context), args, arg_count);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(init_fn, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);

    //--------------
    // Assign captured variables
    size_t i = 2;
    captured_variables = node->captured_vars;
    while (captured_variables != NULL)
    {
        LLVMValueRef node = necro_snapshot_gep(codegen, "captured", LLVMGetParam(init_fn, 0), 2, (uint32_t[]) { 0, i });
        LLVMBuildStore(codegen->builder, LLVMGetParam(init_fn, i - 1), node);
        i++;
        captured_variables = captured_variables->next;
    }

    //--------------
    // alloc each persistent node with appropriate mk_node function
    NecroPersistentVarList* persistent_vars = node->persistent_vars;
    while (persistent_vars != NULL)
    {
        // TODO: This is NULL on mutally recursive functions...how to fix!?
        LLVMValueRef owned_data = LLVMBuildCall(codegen->builder, persistent_vars->data.prototype->mk_function, NULL, 0, "persistent");
        LLVMSetInstructionCallConv(owned_data, LLVMFastCallConv);
        LLVMValueRef owned_ptr = necro_snapshot_gep(codegen, "persistent_ptr", LLVMGetParam(init_fn, 0), 2, (uint32_t[]) { 0, i });
        LLVMBuildStore(codegen->builder, owned_data, owned_ptr);
        i++;
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
    assert(value_type != NULL);
    NecroNodePrototype* node_prototype = necro_create_necro_node_prototype(codegen, bind_var, node_name, node_type_ref, value_type, NULL, NECRO_NODE_STATEFUL);
    // NecroNodePrototype* node_prototype = necro_create_necro_node_prototype(codegen, bind_var, node_name, node_type_ref, value_type, NULL, NECRO_NODE_STATELESS); // IS THIS A BAD IDEA!?!?!?
    necro_symtable_get(codegen->symtable, bind_var.id)->node_prototype = node_prototype;
    return node_prototype;
}

// How to detect statefulness across mutually recursive variables!?
void necro_calculate_captured_vars(NecroCodeGen* codegen, NecroNodePrototype* prototype)
{
    // TODO: OR part of a mutually recursive group!?
    // Need to weed out declaration groups with multiple of the same function
    // Capture variables that aren't a constructor, an arg, a local var, or a global var
    // Should be able to detect with a letrec construct in core?!?!
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
            prototype->slot_count++;
        }
        local_vars = local_vars->next;
    }
    // if (prototype->persistent_vars != NULL)
    //     puts("");
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
            calls->data.slot           = prototype->slot_count;
            prototype->slot_count++;
        }
        else
        {
            calls->data.is_persistent = false;
        }
        calls = calls->next;
    }
    prototype->persistent_vars = necro_reverse_persistent_var_list(&codegen->arena, prototype->persistent_vars);
}

void necro_calc_statefulness(NecroCodeGen* codegen, NecroNodePrototype* prototype, bool is_recursive)
{
    // A node is stateful if it is recursive, or has captured variables or has persistent variables, or calls a stateful function
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
        .slot          = 0
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
LLVMValueRef necro_calculate_node_call(NecroCodeGen* codegen, NecroCoreAST_Expression* ast);
// Bind returns call function!
LLVMValueRef necro_calculate_node_call_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);

    NecroArenaSnapshot  snapshot       = necro_get_arena_snapshot(&codegen->snapshot_arena);
    NecroNodePrototype* node_prototype = necro_symtable_get(codegen->symtable, ast->bind.var.id)->node_prototype;
    assert(node_prototype != NULL);

    if (node_prototype->outer != NULL)
        return necro_calculate_node_call(codegen, ast->bind.expr);
    const char*  call_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]){ "_call", necro_intern_get_string(codegen->intern, ast->bind.var.symbol) });
    LLVMValueRef call_fn   = NULL;
    // TODO: Better way to distinguish function from value!
    if (ast->bind.expr->expr_type != NECRO_CORE_EXPR_LAM)
    {
        if (node_prototype->type == NECRO_NODE_STATEFUL)
        {
            LLVMTypeRef args[1] = { LLVMPointerType(node_prototype->node_type, 0) };
            call_fn = necro_snapshot_add_function(codegen, call_name, LLVMPointerType(node_prototype->node_value_type, 0), args, 1);
            LLVMBasicBlockRef entry = LLVMAppendBasicBlock(call_fn, "entry");
            LLVMPositionBuilderAtEnd(codegen->builder, entry);
            LLVMValueRef result    = necro_calculate_node_call(codegen, ast->bind.expr);
            LLVMValueRef value_ptr = necro_snapshot_gep(codegen, "value_ptr", LLVMGetParam(call_fn, 0), 2, (uint32_t[]) { 0, 1 });
            LLVMBuildStore(codegen->builder, result, value_ptr);
            LLVMBuildRet(codegen->builder, result);
        }
        else
        {
            // LLVMTypeRef        args[0]   = { };
            call_fn = necro_snapshot_add_function(codegen, call_name, LLVMPointerType(node_prototype->node_value_type, 0), NULL, 0);
            LLVMBasicBlockRef entry = LLVMAppendBasicBlock(call_fn, "entry");
            LLVMPositionBuilderAtEnd(codegen->builder, entry);
            LLVMValueRef result = necro_calculate_node_call(codegen, ast->bind.expr);
            LLVMBuildRet(codegen->builder, result);
            return call_fn;
        }
    }
    else
    {
        // LLVMTypeRef        args[0]   = { };
        call_fn = necro_snapshot_add_function(codegen, call_name, LLVMPointerType(node_prototype->node_value_type, 0), NULL, 0);
        LLVMBasicBlockRef entry = LLVMAppendBasicBlock(call_fn, "entry");
        LLVMPositionBuilderAtEnd(codegen->builder, entry);
        necro_calculate_node_call(codegen, ast->bind.expr);
        LLVMBuildRet(codegen->builder, LLVMConstPointerNull(LLVMPointerType(node_prototype->node_value_type, 0)));
    }
    node_prototype->call_function = call_fn;
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return call_fn;
}

LLVMValueRef necro_calculate_node_call_let(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LET);
    assert(ast->let.bind != NULL);
    assert(ast->let.bind->expr_type == NECRO_CORE_EXPR_BIND);
    LLVMValueRef        bind_result    = necro_calculate_node_call_bind(codegen, ast->let.bind);
    NecroNodePrototype* bind_prototype = necro_symtable_get(codegen->symtable, ast->let.bind->bind.var.id)->node_prototype;
    assert(bind_prototype != NULL);
    // TODO: Determine if function or not
    if (bind_prototype->type == NECRO_NODE_STATEFUL)
    {
        // TODO: Need to retrieve slot somehow...
        assert(false && "TODO: Finish!");
    }
    else
    {
        char  buf[10];
        const char* var_name = necro_concat_strings(&codegen->snapshot_arena, 3, (const char*[]) { necro_intern_get_string(codegen->intern, ast->let.bind->bind.var.symbol), "_", itoa(ast->let.bind->bind.var.id.id, buf, 10) });
        LLVMSetValueName2(bind_result, var_name, strlen(var_name));
        necro_symtable_get(codegen->symtable, ast->let.bind->var.id)->llvm_value = bind_result;
        return necro_calculate_node_call(codegen, ast->let.expr);
    }
    return NULL;
}

LLVMValueRef necro_calculate_node_call_var(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (necro_symtable_get(codegen->symtable, ast->var.id)->is_constructor)
    {
        LLVMValueRef con_mk_function = necro_symtable_get(codegen->symtable, ast->var.id)->node_prototype->call_function;
        const char*  result_name     = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(codegen->intern, ast->var.symbol), "_result" });
        LLVMValueRef result          = LLVMBuildCall(codegen->builder, con_mk_function, NULL, 0, result_name);
        LLVMSetInstructionCallConv(result, LLVMFastCallConv);
        return result;
    }
    else
    {
        NecroSymbolInfo* info = necro_symtable_get(codegen->symtable, ast->var.id);
        assert(info->node_prototype != NULL && "Prototype should never be NULL!");
        assert(info->llvm_value != NULL && "Value not present. Maybe infinite loop?");
        return info->llvm_value;
    }
}

LLVMValueRef necro_calculate_node_call_app(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
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

    NecroNodePrototype* call_prototype = necro_symtable_get(codegen->symtable, function->var.id)->node_prototype;
    assert(call_prototype != NULL);
    assert(call_prototype->call_function != NULL);
    // TODO: Support polymorphic functions!!!!
    // LLVMTypeRef   fun_type  = LLVMTuple
    LLVMValueRef* args      = necro_paged_arena_alloc(&codegen->arena, arg_count * sizeof(LLVMValueRef));
    size_t        arg_index = arg_count - 1;
    function                = ast;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_calculate_node_call(codegen, function->app.exprB);
        arg_index--;
        function = function->app.exprA;
    }

    const char*  result_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(codegen->intern, function->var.symbol), "_result" });
    LLVMValueRef result      = LLVMBuildCall(codegen->builder, call_prototype->call_function, args, arg_count, result_name);
    LLVMSetInstructionCallConv(result, LLVMFastCallConv);
    return result;
}

LLVMValueRef necro_calculate_node_call_lam(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LAM);
    // ONLY USE THIS FOR TRULY ANONYMOUS FUNCTIONS!!!!
    assert(false && "TODO: Finish!");
    return NULL;
}

LLVMValueRef necro_calculate_node_call_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_CASE);
    assert(false && "TODO: Finish!");
    return NULL;
}

LLVMValueRef necro_calculate_node_call_lit(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_LIT);
    LLVMTypeRef  boxed_type;
    LLVMValueRef raw_value;
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_INTEGER:
        raw_value  = LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->lit.int_literal, true);
        boxed_type = necro_symtable_get(codegen->symtable, codegen->infer->prim_types->int_type.id)->llvm_type;
        break;
    case NECRO_AST_CONSTANT_FLOAT:
        raw_value = LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), ast->lit.double_literal);
        boxed_type = necro_symtable_get(codegen->symtable, codegen->infer->prim_types->float_type.id)->llvm_type;
        break;
    case NECRO_AST_CONSTANT_CHAR:
        raw_value = LLVMConstInt(LLVMInt8TypeInContext(codegen->context), ast->lit.char_literal, true);
        boxed_type = necro_symtable_get(codegen->symtable, codegen->infer->prim_types->char_type.id)->llvm_type;
        break;
    default:
        break;
    }
    LLVMValueRef boxed_ptr = necro_gen_alloc_boxed_value(codegen, boxed_type, 0, 0, "boxed_lit");
    LLVMBuildStore(codegen->builder, raw_value, boxed_ptr);
    return boxed_ptr;
}

LLVMValueRef necro_calculate_node_call(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LET:       return necro_calculate_node_call_let(codegen, ast);
    case NECRO_CORE_EXPR_BIND:      return necro_calculate_node_call_bind(codegen, ast);
    case NECRO_CORE_EXPR_APP:       return necro_calculate_node_call_app(codegen, ast);
    case NECRO_CORE_EXPR_VAR:       return necro_calculate_node_call_var(codegen, ast);
    case NECRO_CORE_EXPR_LAM:       return necro_calculate_node_call_lam(codegen, ast);
    case NECRO_CORE_EXPR_CASE:      return necro_calculate_node_call_case(codegen, ast);
    case NECRO_CORE_EXPR_LIT:       return necro_calculate_node_call_lit(codegen, ast);
    // Should never happen
    case NECRO_CORE_EXPR_DATA_DECL: assert(false && "Should never reach DATA_DECL case in necro_calculate_node_prototype"); return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "Should never reach DATA_CON  case in necro_calculate_node_prototype"); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false && "Should never reach EXPR_TYPE case in necro_calculate_node_prototype"); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false && "Should never reach EXPR_LIST case in necro_calculate_node_prototype"); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_calculate_node_prototype"); return NULL;
    }
}

//=====================================================
// Go
//=====================================================

LLVMValueRef necro_codegen_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    necro_calculate_node_prototype(codegen, ast, NULL);
    necro_calculate_node_call(codegen, ast);
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
    char* error = NULL;
    LLVMVerifyModule(codegen->mod, LLVMAbortProcessAction, &error);
    if (strlen(error) != 0)
    {
        printf("LLVM error message: %s\n", error);
        LLVMDisposeMessage(error);
        return NECRO_ERROR;
    }
    LLVMDisposeMessage(error);
    const char* module_string_rep = LLVMPrintModuleToString(codegen->mod);
    printf("\n%s\n", module_string_rep);

    // LLVMDisposeBuilder(codegen->builder);
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
   if (necro_verify_and_dump_codegen(codegen) == NECRO_ERROR)
        return NECRO_ERROR;
    return NECRO_SUCCESS;
}
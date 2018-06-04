/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen.h"
#include <stdarg.h>
#include <ctype.h>
#include <llvm-c/Analysis.h>
#include "type/infer.h"
#include "core/core.h"
#include "symtable.h"
#include "type/prim.h"
#include "intern.h"
#include "utility/small_array.h"

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

LLVMValueRef necro_snapshot_gep(NecroCodeGen* codegen, const char* ptr_name, LLVMValueRef data_ptr, size_t num_indices, ...)
{
    va_list args;
    va_start(args, num_indices);
    uint32_t index = va_arg(args, uint32_t);
    LLVMValueRef* index_refs = necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMValueRef) * num_indices);
    for (size_t i = 0; i < num_indices; ++i)
    {
        index_refs[i] = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), index, false);
        index         = va_arg(args, uint32_t);
    }
    va_end(args);
    return LLVMBuildGEP(codegen->builder, data_ptr, index_refs, num_indices, ptr_name);
}

LLVMValueRef necro_snapshot_add_function(NecroCodeGen* codegen, const char* function_name, LLVMTypeRef return_type, size_t arg_count, ...)
{
    va_list args;
    va_start(args, arg_count);
    LLVMTypeRef  arg      = va_arg(args, LLVMTypeRef);
    LLVMTypeRef* arg_refs = (arg_count > 0) ? necro_snapshot_arena_alloc(&codegen->snapshot_arena, sizeof(LLVMValueRef) * arg_count) : NULL;
    for (size_t i = 0; i < arg_count; ++i)
    {
        arg_refs[i] = va_arg(args, LLVMTypeRef);
    }
    va_end(args);
    return LLVMAddFunction(codegen->mod, function_name, LLVMFunctionType(return_type, arg_refs, arg_count, false));
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

char* necro_concat_strings(NecroSnapshotArena* arena,...)
{
    va_list args;
    va_start(args, arena);
    const char* arg = va_arg(args, const char*);
    size_t total_length = 1;
    while (arg != NULL)
    {
        total_length += strlen(arg);
        arg = va_arg(args, const char*);
    }
    va_end(args);
    char* buffer = necro_snapshot_arena_alloc(arena, total_length);
    buffer[0] = '\0';
    va_list args2;
    va_start(args2, arena);
    arg = va_arg(args2, const char*);
    while (arg != NULL)
    {
        strcat(buffer, arg);
        arg = va_arg(args2, const char*);
    }
    va_end(args2);
    return buffer;
}

void necro_init_necro_data(NecroCodeGen* codegen, LLVMValueRef data_ptr, uint32_t value_1, uint32_t value_2)
{
    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&codegen->snapshot_arena);
    LLVMValueRef       necro_data_1     = necro_snapshot_gep(codegen, "necro_data_1", data_ptr, 3, 0, 0, 0);
    LLVMValueRef       necro_data_2     = necro_snapshot_gep(codegen, "necro_data_2", data_ptr, 3, 0, 0, 1);
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

//=====================================================
// CodeGen
//=====================================================
LLVMValueRef necro_codegen_literal(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_LIT);
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_INTEGER:
        LLVMConstInt(LLVMInt64TypeInContext(codegen->context), ast->lit.int_literal, true);
        break;
    case NECRO_AST_CONSTANT_FLOAT:
        LLVMConstReal(LLVMDoubleTypeInContext(codegen->context), ast->lit.double_literal);
        break;
    case NECRO_AST_CONSTANT_CHAR:
        LLVMConstInt(LLVMInt8TypeInContext(codegen->context), ast->lit.char_literal, true);
        break;
    }
    return NULL;
}

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
        necro_throw_codegen_error(codegen, ast, "Compiler bug: NULL value in necro_codegen_variable");
        return NULL;
    }
    return value;
}

// TODO: FINISH!
LLVMTypeRef necro_type_to_llvm_type(NecroCodeGen* codegen, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(codegen->infer, type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_val_type.id)->llvm_type; // TODO: Is this right?!?!
    case NECRO_TYPE_APP:  assert(false); return NULL;
    case NECRO_TYPE_LIST: assert(false); return NULL;
    case NECRO_TYPE_FOR:  return NULL; // TODO: Finish
    case NECRO_TYPE_FUN:  return NULL; // TODO: Finish
    case NECRO_TYPE_CON:  return necro_symtable_get(codegen->symtable, type->con.con.id)->llvm_type;
    default: assert(false);
    }
    return NULL;
}

// TODO: FINISH!
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
    case NECRO_CORE_EXPR_DATA_CON:  return necro_symtable_get(codegen->symtable, ast->data_con.condid.id)->llvm_type;
    case NECRO_CORE_EXPR_APP:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LAM:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        necro_throw_codegen_error(codegen, ast, "Unimplemented AST type in necro_codegen_go"); assert(false); return NULL;
    }
}

//=====================================================
// DataCon
//=====================================================
void necro_codegen_data_con(NecroCodeGen* codegen, NecroCoreAST_DataCon* con, LLVMTypeRef data_type_ref, size_t max_arg_count, uint32_t con_number)
{
    if (necro_is_codegen_error(codegen)) return;
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*  con_name  = necro_concat_strings(&codegen->snapshot_arena, "_mk", necro_intern_get_string(codegen->intern, con->condid.symbol), NULL);
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
        char* location_name = necro_concat_strings(&codegen->snapshot_arena,"slot_", itoa(i, itoa_buff, 10), NULL);
        LLVMValueRef slot = necro_snapshot_gep(codegen, location_name, data_ptr, 2, 0, i + 1);
        if (i < arg_count)
        {
            char itoa_buff_2[6];
            char* value_name   = necro_concat_strings(&codegen->snapshot_arena,"param_", itoa(i, itoa_buff_2, 10), NULL);
            LLVMValueRef value = LLVMBuildBitCast(codegen->builder, LLVMGetParam(make_con, i), necro_get_value_ptr(codegen), value_name);
            LLVMBuildStore(codegen->builder, value, slot);
        }
        else
        {
            LLVMBuildStore(codegen->builder, LLVMConstPointerNull(necro_get_value_ptr(codegen)), slot);
        }
    }
    LLVMBuildRet(codegen->builder, data_ptr);
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
// Store in symtable!?!?!
typedef struct
{
    LLVMTypeRef  node_type;       // Type of the actual node
    LLVMTypeRef  node_value_type; // The value type that the node calculates
    LLVMTypeRef* args;            // NodeType, followed by each arg to function (0 if a simple assignment)
    size_t       args_count;
    LLVMTypeRef* node_storage;    // All inner nodes this node contains
    size_t       node_storage_count;
    bool         stateless;
} NecroNodePrototype;

void necro_codegen_mk_node(NecroCodeGen* codegen, const char* node_type_name, LLVMTypeRef value_type, LLVMTypeRef node_type)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(value_type != NULL);
    assert(node_type != NULL);
    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&codegen->snapshot_arena);
    char*              mk_name   = necro_concat_strings(&codegen->snapshot_arena, "_mk", node_type_name, NULL);
    LLVMValueRef       mk_alloc  = necro_snapshot_add_function(codegen, mk_name, LLVMPointerType(node_type, 0), 0);
    LLVMBasicBlockRef  entry     = LLVMAppendBasicBlock(mk_alloc, "entry");
    LLVMPositionBuilderAtEnd(codegen->builder, entry);
    LLVMValueRef       void_ptr  = necro_alloc_codegen(codegen, LLVMABISizeOfType(codegen->target, node_type));
    LLVMValueRef       node_ptr  = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(node_type, 0), "node_ptr");
    necro_init_necro_data(codegen, node_ptr, 0, 0);
    LLVMValueRef       value_ptr = necro_snapshot_gep(codegen, "value_ptr", node_ptr, 2, 0, 1);
    LLVMBuildStore(codegen->builder, LLVMConstPointerNull(LLVMPointerType(value_type, 0)), value_ptr);
    LLVMBuildRet(codegen->builder, node_ptr);
    // Cleanup
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

// Perhaps do full on ast crawl and store these into symtable.
// Then we can reference them whenever we want!
void necro_calculate_node_prototype(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return;
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    // Would need to store ALL temporaries as well!
    // perhaps crawl this, then once we have the top prototype we simply crawl the prototype and gen code from there!?
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:       return;
    case NECRO_CORE_EXPR_VAR:       return;
    case NECRO_CORE_EXPR_BIND:      return;
    case NECRO_CORE_EXPR_APP:       return;
    case NECRO_CORE_EXPR_LAM:       return;
    case NECRO_CORE_EXPR_LET:       return;
    case NECRO_CORE_EXPR_CASE:      return;
    case NECRO_CORE_EXPR_TYPE:      return;
    case NECRO_CORE_EXPR_DATA_DECL: return;
    case NECRO_CORE_EXPR_DATA_CON:  return;
    case NECRO_CORE_EXPR_LIST:      assert(false); return; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_calculate_node_prototype"); return;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
}

// Calculate node prototype
// init_node
// update_node

LLVMTypeRef necro_gen_bind_node_type(NecroCodeGen* codegen, NecroCoreAST_Bind* bind, LLVMTypeRef value_type)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(bind != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);

    char buf2[10];
    char* node_name = necro_concat_strings(&codegen->snapshot_arena, necro_intern_get_string(codegen->intern, bind->var.symbol), "_", itoa(bind->var.id.id, buf2, 10), "_Node", NULL);
    node_name[0] = toupper(node_name[0]);

    LLVMTypeRef node_type_ref = LLVMStructCreateNamed(codegen->context, node_name);
    LLVMTypeRef node_elems[2] = { necro_get_data_type(codegen), LLVMPointerType(value_type, 0) };
    LLVMStructSetBody(node_type_ref, node_elems, 2, false);
    // necro_symtable_get(codegen->symtable, prim_types->audio_type.id)->llvm_type = audio_type_ref;

    necro_codegen_mk_node(codegen, node_name, value_type, node_type_ref);
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return node_type_ref;
}

// TODO: I imagine this will be much more complicated once we take into account continuous update semantics
// Or maybe not? We're not lazy after all. Strict semantics means value will be precomputed...
// However, at the very least we're going to have to think about retrieving the value from the heap
// and GC.
LLVMValueRef necro_codegen_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    bool prev_is_top_level = codegen->is_top_level;
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    if (codegen->is_top_level)
    {
        char buf2[10];
        const char* bind_name = necro_concat_strings(&codegen->snapshot_arena, necro_intern_get_string(codegen->intern, ast->bind.var.symbol), "_", itoa(ast->bind.var.id.id, buf2, 10), NULL);
        LLVMTypeRef bind_type = necro_get_ast_llvm_type(codegen, ast);
        assert(bind_type != NULL);
        LLVMTypeRef  node_type  = necro_gen_bind_node_type(codegen, &ast->bind, bind_type);
        LLVMValueRef bind_value = LLVMAddGlobal(codegen->mod, LLVMPointerType(node_type, 0), bind_name);
        // TODO: Fix this!
        // LLVMSetLinkage(bind_value, LLVMCommonLinkage);
        codegen->is_top_level = false;
        codegen->is_top_level = prev_is_top_level;
        return bind_value;
    }
    else
    {
        codegen->is_top_level = prev_is_top_level;
    }
    necro_rewind_arena(&codegen->snapshot_arena, snapshot);
    return NULL;
}

//=====================================================
// Go
//=====================================================
LLVMValueRef necro_codegen_go(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:       return necro_codegen_literal(codegen, ast);
    case NECRO_CORE_EXPR_VAR:       return necro_codegen_variable(codegen, ast);
    case NECRO_CORE_EXPR_BIND:      return necro_codegen_bind(codegen, ast);
    case NECRO_CORE_EXPR_APP:       return NULL;
    case NECRO_CORE_EXPR_LAM:       return NULL;
    case NECRO_CORE_EXPR_LET:       return NULL;
    case NECRO_CORE_EXPR_CASE:      return NULL;
    case NECRO_CORE_EXPR_TYPE:      return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: return necro_codegen_data_declaration(codegen, ast);
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
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
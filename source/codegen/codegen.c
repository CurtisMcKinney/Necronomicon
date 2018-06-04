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

// TODO: LLVM License
// things to reference:
//     * https://llvm.org/docs/LangRef.html#instruction-reference
//     * https://llvm.org/docs/tutorial/LangImpl04.html
//     * http://www.stephendiehl.com/llvm/
//     * https://lowlevelbits.org/how-to-use-llvm-api-with-swift.-addendum/

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

extern DLLEXPORT int8_t* _necro_alloc(uint32_t size)
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
    LLVMValueRef necro_alloc         = LLVMAddFunction(mod, "_necro_alloc", LLVMFunctionType(LLVMPointerType(LLVMInt8TypeInContext(context), 0), necro_alloc_args, 1, false));
    LLVMSetLinkage(necro_alloc, LLVMExternalLinkage);
    _necro_alloc(0);

    return (NecroCodeGen)
    {
        .arena         = necro_create_paged_arena(),
        .infer         = infer,
        .intern        = intern,
        .symtable      = symtable,
        .context       = context,
        .mod           = mod,
        .builder       = LLVMCreateBuilderInContext(context),
        .target        = LLVMCreateTargetData(LLVMGetTarget(mod)),
        .necro_alloc   = necro_alloc,
        .error         = necro_create_error(),
        .current_block = (NecroBlockIndex) { .index = 0 },
        .blocks        = 0,
        .is_top_level  = true
    };
}

void necro_destroy_codegen(NecroCodeGen* codegen)
{
    necro_destroy_paged_arena(&codegen->arena);
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

char* necro_concat_strings(const char* string1,...)
{
    va_list args;
    va_start(args, string1);
    const char* arg = va_arg(args, const char*);
    size_t total_length = strlen(string1) + 1;
    while (arg != NULL)
    {
        total_length += strlen(arg);
        arg = va_arg(args, const char*);
    }
    va_end(args);
    char* buffer = malloc(sizeof(char) * total_length);
    buffer[0] = '\0';
    strcat(buffer, string1);
    va_list args2;
    va_start(args2, string1);
    arg = va_arg(args2, const char*);
    while (arg != NULL)
    {
        strcat(buffer, arg);
        arg = va_arg(args2, const char*);
    }
    va_end(args2);
    return buffer;
}

void necro_codegen_data_con(NecroCodeGen* codegen, NecroCoreAST_DataCon* con, LLVMTypeRef data_type_ref, size_t max_arg_count)
{
    if (necro_is_codegen_error(codegen)) return;
    char* con_name         = necro_concat_strings("_mk", necro_intern_get_string(codegen->intern, con->condid.symbol), NULL);
    size_t                   arg_count = necro_codegen_count_num_args(codegen, con);
    assert(arg_count <= arg_count);
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
    LLVMValueRef data_size     = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), LLVMABISizeOfType(codegen->target, data_type_ref), false);
    LLVMValueRef void_ptr      = LLVMBuildCall(codegen->builder, codegen->necro_alloc, &data_size, 1, "void_ptr");
    LLVMValueRef data_ptr      = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(data_type_ref, 0), "data_ptr");
    //--------------
    // NecroData
    LLVMValueRef const_zero             = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false);
    LLVMValueRef const_one              = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 1, false);
    LLVMValueRef necro_data_indexes1[3] = { const_zero, const_zero, const_zero };
    LLVMValueRef necro_data_1           = LLVMBuildGEP(codegen->builder, data_ptr, necro_data_indexes1, 3, "necro_data_1");
    LLVMValueRef necro_data_indexes2[3] = { const_zero, const_zero, const_one };
    LLVMValueRef necro_data_2           = LLVMBuildGEP(codegen->builder, data_ptr, necro_data_indexes2, 3, "necro_data_2");
    LLVMBuildStore(codegen->builder, const_zero, necro_data_1);
    LLVMBuildStore(codegen->builder, const_zero, necro_data_2);
    //--------------
    // Parameters
    for (size_t i = 0; i < max_arg_count; ++i)
    {
        if (i < arg_count)
        {
            char itoa_buff[6];
            char* location_name = necro_concat_strings("slot_", itoa(i, itoa_buff, 10), NULL);
            LLVMValueRef index1 = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false);
            LLVMValueRef index2 = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), i + 1, false);
            LLVMValueRef indexes[2] = { index1, index2 };
            LLVMValueRef slot   = LLVMBuildGEP(codegen->builder, data_ptr, indexes, 2, location_name);
            char itoa_buff_2[6];
            char* value_name = necro_concat_strings("param_", itoa(i, itoa_buff_2, 10), NULL);
            LLVMValueRef value  = LLVMBuildBitCast(codegen->builder, LLVMGetParam(make_con, i), necro_get_value_ptr(codegen), value_name);
            LLVMBuildStore(codegen->builder, value, slot);
            free(location_name);
            free(value_name);
        }
        else
        {
            char itoa_buff[6];
            char* location_name = necro_concat_strings("slot_", itoa(i, itoa_buff, 10), NULL);
            LLVMValueRef index1 = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), 0, false);
            LLVMValueRef index2 = LLVMConstInt(LLVMInt32TypeInContext(codegen->context), i + 1, false);
            LLVMValueRef indexes[2] = { index1, index2 };
            LLVMValueRef slot   = LLVMBuildGEP(codegen->builder, data_ptr, indexes, 2, location_name);
            LLVMBuildStore(codegen->builder, LLVMConstPointerNull(necro_get_value_ptr(codegen)), slot);
            free(location_name);
        }
    }
    LLVMBuildRet(codegen->builder, data_ptr);
necro_codegen_data_con_cleanup:
    free(con_name);
    necro_destroy_llvm_type_ref_array(&args);
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
    while (con != NULL)
    {
        necro_codegen_data_con(codegen, con, data_type_ref, max_arg_count);
        con = con->next;
    }

    // Cleanup
    necro_destroy_llvm_type_ref_array(&elems);
    return NULL;
}

LLVMTypeRef necro_gen_bind_node_type(NecroCodeGen* codegen, NecroCoreAST_Bind* bind, LLVMTypeRef value_type)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(bind != NULL);

    // TODO: Better string manipulation functions!
    char buf1[40];
    char buf2[10];
    memset(buf1, '\0', 40);
    strcat(buf1, necro_intern_get_string(codegen->intern, bind->var.symbol));
    buf1[0] = toupper(buf1[0]);
    strcat(buf1, "_");
    const char* id_string = itoa(bind->var.id.id, buf2, 10);
    strcat(buf1, id_string);
    strcat(buf1, "_Node");

    LLVMTypeRef node_type_ref = LLVMStructCreateNamed(codegen->context, buf1);
    LLVMTypeRef node_elems[2] = { necro_get_data_type(codegen), LLVMPointerType(value_type, 0) };
    LLVMStructSetBody(node_type_ref, node_elems, 2, false);
    // necro_symtable_get(codegen->symtable, prim_types->audio_type.id)->llvm_type = audio_type_ref;

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
    if (codegen->is_top_level)
    {
        // TODO: Better string manipulation functions!
        char buf1[40];
        char buf2[10];
        memset(buf1, '\0', 40);
        strcat(buf1, necro_intern_get_string(codegen->intern, ast->bind.var.symbol));
        strcat(buf1, "_");
        const char* id_string = itoa(ast->bind.var.id.id, buf2, 10);
        strcat(buf1, id_string);
        LLVMTypeRef bind_type = necro_get_ast_llvm_type(codegen, ast);
        assert(bind_type != NULL);
        LLVMTypeRef  node_type  = necro_gen_bind_node_type(codegen, &ast->bind, bind_type);
        LLVMValueRef bind_value = LLVMAddGlobal(codegen->mod, LLVMPointerType(node_type, 0), buf1);
        // TODO: Fix this!
        // LLVMSetLinkage(bind_value, LLVMCommonLinkage);
        codegen->is_top_level = false;
        codegen->is_top_level = prev_is_top_level;
        return bind_value; // Fully resolve name...append with ID?
    }
    else
    {
        codegen->is_top_level = prev_is_top_level;
    }
    return NULL;
}

LLVMValueRef necro_codegen_go(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
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
    default:                        necro_throw_codegen_error(codegen, ast, "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
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
    // Only worrying about primitives for now
    // return NECRO_SUCCESS;
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
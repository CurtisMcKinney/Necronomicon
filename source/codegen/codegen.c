/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen.h"
#include "type/infer.h"
#include "core/core.h"
#include "llvm-c/Analysis.h"
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

//=====================================================
// Forward Declarations
//=====================================================

//=====================================================
// Create / Destroy
//=====================================================
NecroCodeGen necro_create_codegen(NecroInfer* infer, NecroIntern* intern, NecroSymTable* symtable, const char* module_name)
{
    LLVMContextRef llvm_context = LLVMContextCreate();
    return (NecroCodeGen)
    {
        .arena         = necro_create_paged_arena(),
        .infer         = infer,
        .intern        = intern,
        .symtable      = symtable,
        .mod           = LLVMModuleCreateWithNameInContext(module_name, llvm_context),
        .builder       = LLVMCreateBuilderInContext(llvm_context),
        .context       = llvm_context,
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

inline LLVMTypeRef necro_codegen_get_necro_data_type(NecroCodeGen* codegen)
{
    return necro_symtable_get(codegen->symtable, codegen->infer->prim_types->necro_data_type.id)->llvm_type;
}

// const char* necro_get_llvm_name(NecroCodeGen* codegen, NecroSymbol symbol, NecroID id)
// {
//     char buf1[40];
//     char buf2[10];
//     memset(buf1, '\0', 40);
//     // NecroSymbol name      = necro_intern_concat_symbols(codegen->intern, ast->bind.var.symbol, necro_intern_string(codegen->intern, itoa(ast->bind.var.id.id, buf, 10)));
//     strcat(buf1, necro_intern_get_string(codegen->intern, ast->bind.var.symbol));
//     strcat(buf1, "_");
//     const char* id_string = itoa(ast->bind.var.id.id, buf2, 10);
//     strcat(buf1, id_string);
// }


// void necro_test_codegen(NecroCodeGen* codegen)
// {
//     LLVMTypeRef       param_types[] = { LLVMInt64Type(), LLVMInt64Type() };
//     LLVMTypeRef       ret_type      = LLVMFunctionType(LLVMInt64Type(), param_types, 2, 0);
//     LLVMValueRef      sum           = LLVMAddFunction(codegen->mod, "sum", ret_type);
//     LLVMBasicBlockRef entry         = LLVMAppendBasicBlock(sum, "entry");
//     LLVMPositionBuilderAtEnd(codegen->builder, entry);
//     LLVMValueRef      tmp           = LLVMBuildAdd(codegen->builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
//     LLVMBuildRet(codegen->builder, tmp);
//     char              *error        = NULL;
//     LLVMVerifyModule(codegen->mod, LLVMAbortProcessAction, &error);
//     if (strlen(error) != 0)
//         printf("LLVM error message: %s\n", error);
//     LLVMDisposeMessage(error);
//     const char* module_string_rep = LLVMPrintModuleToString(codegen->mod);
//     printf("\n%s\n", module_string_rep);
//     // if (LLVMWriteBitcodeToFile(sum->mod, "codegen_dmp.bc") != 0) { fprintf(stderr, "error writing bitcode to file, skipping\n"); }

//     //------------------
//     // TODO: Theoretical MCJIT stuff

//     LLVMDisposeBuilder(codegen->builder);
// }

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
        LLVMConstInt(LLVMInt64Type(), ast->lit.int_literal, true);
        break;
    case NECRO_AST_CONSTANT_FLOAT:
        LLVMConstReal(LLVMDoubleType(), ast->lit.double_literal);
        break;
    case NECRO_AST_CONSTANT_CHAR:
        LLVMConstInt(LLVMInt8Type(), ast->lit.int_literal, false);
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
    case NECRO_TYPE_VAR:  assert(false); return NULL;
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
        case NECRO_AST_CONSTANT_INTEGER: return LLVMInt64Type();
        case NECRO_AST_CONSTANT_FLOAT:   return LLVMFloatType();
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
    case NECRO_CORE_EXPR_APP:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LAM:       assert(false); return NULL;
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_TYPE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        necro_throw_codegen_error(codegen, ast, "Unimplemented AST type in necro_codegen_go"); assert(false); return NULL;
    }
}

NECRO_DECLARE_SMALL_ARRAY(LLVMTypeRef, LLVMTypeRef, llvm_type_ref, 5);

LLVMTypeRef necro_codegen_create_constructor_struct(NecroCodeGen* codegen, NecroCoreAST_DataCon* con)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    // Constructor Type
    NecroSymbol name         = necro_intern_concat_symbols(codegen->intern, con->condid.symbol, necro_intern_string(codegen->intern, "@Con"));
    LLVMTypeRef con_type_ref = LLVMStructCreateNamed(codegen->context, necro_intern_get_string(codegen->intern, name));
    LLVMTypeRef con_elems[1] = { LLVMInt64Type() };
    LLVMStructSetBody(con_type_ref, con_elems, 1, false);
    return con_type_ref;
}

LLVMValueRef necro_codegen_data_declaration(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);

    size_t                count  = necro_data_con_count(ast->data_decl.con_list);
    NecroCoreAST_DataCon* con    = ast->data_decl.con_list;
    NecroLLVMTypeRefArray values = necro_create_llvm_type_ref_array(count);
    assert(count > 0);
    // Local malloc and free
    for (size_t i = 0; i < count; ++i)
    {
        *necro_llvm_type_ref_array_get(&values, i) = necro_codegen_create_constructor_struct(codegen, con);
        con = con->next;
    }
    // Data Type
    LLVMTypeRef data_type_ref = LLVMStructCreateNamed(codegen->context, necro_intern_get_string(codegen->intern, ast->data_con.condid.symbol));
    if (count == 1)
    {
        LLVMTypeRef data_elems[2] = { necro_codegen_get_necro_data_type(codegen), *necro_llvm_type_ref_array_get(&values, 0) };
        LLVMStructSetBody(data_type_ref, data_elems, 2, false);
    }
    else
    {
        // *necro_llvm_type_ref_array_get(&values, 0) = necro_codegen_get_necro_data_type(codegen);
        // LLVMTypeRef data_elems[2] = { , LLVMInt64Type() };
        // LLVMStructSetBody(int_type_ref, int_elems, 2, false);
    }
    necro_symtable_get(codegen->symtable, ast->data_con.condid.id)->llvm_type = data_type_ref;

    // Cleanup
    necro_destroy_llvm_type_ref_array(&values);
    return NULL;
}

// TODO: I imagine this will be much more complicated once we take into account continuous update semantics
// Or maybe not? We're not lazy after all. Strict semantics means value will be precomputed...
// However, at the very least we're going to have to think about retrieving the value from the heap
// and GC.
LLVMValueRef necro_codegen_bind(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(ast->expr_type == NECRO_CORE_EXPR_BIND);
    bool prev_is_top_level = codegen->is_top_level;
    if (codegen->is_top_level)
    {
        // // For now assume 0 arguments
        // NecroSymbol name      = necro_intern_concat_symbols(codegen->intern, ast->bind.var.symbol, necro_id_as_symbol(codegen->intern, ast->bind.var));
        // TODO: CHECK FOR OVERFLOW!!!
        char buf1[40];
        char buf2[10];
        memset(buf1, '\0', 40);
        // NecroSymbol name      = necro_intern_concat_symbols(codegen->intern, ast->bind.var.symbol, necro_intern_string(codegen->intern, itoa(ast->bind.var.id.id, buf, 10)));
        strcat(buf1, necro_intern_get_string(codegen->intern, ast->bind.var.symbol));
        strcat(buf1, "_");
        const char* id_string = itoa(ast->bind.var.id.id, buf2, 10);
        strcat(buf1, id_string);
        LLVMTypeRef bind_type = necro_get_ast_llvm_type(codegen, ast);
        assert(bind_type != NULL);
        LLVMValueRef bind_value = LLVMAddGlobal(codegen->mod, bind_type, buf1);
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

    LLVMDisposeBuilder(codegen->builder);
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
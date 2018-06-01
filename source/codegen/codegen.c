/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "codegen.h"
#include "type/infer.h"
#include "core/core.h"
#include "llvm-c/Analysis.h"
#include "symtable.h"

// TODO: LLVM License
// things to reference:
//     * http://www.stephendiehl.com/llvm/
//     * https://lowlevelbits.org/how-to-use-llvm-api-with-swift.-addendum/

// TODO: Core really should have source locations...

// TODO: Things that really need to happen:
//    - A Runtime with:
//        * Memory allocation onto the heap
//        * Value retrieval from the heap
//        * GC
//        * Continuous updates

NecroCodeGen necro_create_codegen(NecroInfer* infer, NecroIntern* intern, NecroSymTable* symtable, const char* module_name)
{
    return (NecroCodeGen)
    {
        .arena         = necro_create_paged_arena(),
        .infer         = infer,
        .intern        = intern,
        .symtable      = symtable,
        .mod           = LLVMModuleCreateWithName(module_name),
        .error         = necro_create_error(),
        .current_block = (NecroBlockIndex) { .index = 0 },
        .blocks        = 0
    };
}

void necro_destroy_codegen(NecroCodeGen* codegen)
{
    necro_destroy_paged_arena(&codegen->arena);
}

void necro_test_codegen(NecroCodeGen* codegen)
{
    LLVMTypeRef       param_types[] = { LLVMInt64Type(), LLVMInt64Type() };
    LLVMTypeRef       ret_type      = LLVMFunctionType(LLVMInt64Type(), param_types, 2, 0);
    LLVMValueRef      sum           = LLVMAddFunction(codegen->mod, "sum", ret_type);
    LLVMBasicBlockRef entry         = LLVMAppendBasicBlock(sum, "entry");
    LLVMBuilderRef    builder       = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef      tmp           = LLVMBuildAdd(builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
    LLVMBuildRet(builder, tmp);
    char              *error        = NULL;
    LLVMVerifyModule(codegen->mod, LLVMAbortProcessAction, &error);
    if (strlen(error) != 0)
        printf("LLVM error message: %s\n", error);
    LLVMDisposeMessage(error);
    const char* module_string_rep = LLVMPrintModuleToString(codegen->mod);
    printf("\n%s\n", module_string_rep);
    // if (LLVMWriteBitcodeToFile(sum->mod, "codegen_dmp.bc") != 0) { fprintf(stderr, "error writing bitcode to file, skipping\n"); }

    //------------------
    // TODO: Theoretical MCJIT stuff

    LLVMDisposeBuilder(builder);
}

inline bool necro_is_codegen_error(NecroCodeGen* codegen)
{
    return codegen->error.return_code == NECRO_ERROR;
}

void necro_throw_codegen_error(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, const char* error_message)
{
    necro_error(&codegen->error, (NecroSourceLoc) {0}, error_message);
}

NECRO_RETURN_CODE necro_codegen(NecroCodeGen* codegen, NecroCoreAST* core_ast)
{
    return NECRO_SUCCESS;
}

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

LLVMValueRef necro_codegen_go(NecroCodeGen* codegen, NecroCoreAST_Expression* ast)
{
    if (necro_is_codegen_error(codegen)) return NULL;
    assert(codegen != NULL);
    assert(ast != NULL);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:       return necro_codegen_literal(codegen, ast);
    case NECRO_CORE_EXPR_VAR:       return necro_codegen_variable(codegen, ast);
    case NECRO_CORE_EXPR_BIND:      return NULL;
    case NECRO_CORE_EXPR_APP:       return NULL;
    case NECRO_CORE_EXPR_LAM:       return NULL;
    case NECRO_CORE_EXPR_LET:       return NULL;
    case NECRO_CORE_EXPR_CASE:      return NULL;
    case NECRO_CORE_EXPR_TYPE:      return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
    case NECRO_CORE_EXPR_LIST:      return NULL; // used for top decls not language lists
    default:                        necro_throw_codegen_error(codegen, ast, "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
    return NULL;
}
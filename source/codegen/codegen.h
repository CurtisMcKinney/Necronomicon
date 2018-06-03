/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_CODEGEN_H
#define TYPE_CODEGEN_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <stdio.h>
#include "utility.h"
#include "arena.h"
#include "intern.h"
#include "list.h"

struct NecroInfer;
struct NecroIntern;
struct NecroCoreAST;
struct NecroSymTable;

// NECRO_DECLARE_ARENA_LIST(LLVMValueRef, LLVMValue, llvm_value);
// Do we really need something like this!?
// typedef struct NecroBlockState
// {
//     size_t             index;
//     NecroLLVMValueList instructions;
//     NecroSymbol        terminator;
// } NecroBlockState;

typedef struct
{
    uint64_t index;
} NecroBlockIndex;

typedef struct NecroCodeGen
{
    NecroPagedArena       arena;
    struct NecroInfer*    infer;
    struct NecroIntern*   intern;
    struct NecroSymTable* symtable;
    LLVMContextRef        context;
    LLVMModuleRef         mod;
    LLVMBuilderRef        builder;
    LLVMValueRef          necro_alloc;
    LLVMTargetDataRef     target;
    NecroError            error;
    NecroBlockIndex       current_block;
    void*                 blocks;
    bool                  is_top_level;
} NecroCodeGen;

NecroCodeGen      necro_create_codegen(struct NecroInfer* infer, struct NecroIntern* intern, struct NecroSymTable* symtable, const char* module_name);
void              necro_destroy_codegen(NecroCodeGen* codegen);
void              necro_test_codegen(NecroCodeGen* codegen);
NecroSymbol       necro_new_block_name(NecroCodeGen* codegen);
NECRO_RETURN_CODE necro_codegen(NecroCodeGen* codegen, struct NecroCoreAST* core_ast);
NECRO_RETURN_CODE necro_verify_and_dump_codegen(NecroCodeGen* codegen);

inline bool necro_is_codegen_error(NecroCodeGen* codegen)
{
    return codegen->error.return_code == NECRO_ERROR;
}

inline void necro_throw_codegen_error(NecroCodeGen* codegen, struct NecroCoreAST_Expression* ast, const char* error_message)
{
    necro_error(&codegen->error, (NecroSourceLoc) {0}, error_message);
}


#endif // TYPE_CODEGEN_H
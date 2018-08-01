/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef TYPE_CODEGEN_LLVM_H
#define TYPE_CODEGEN_LLVM_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/ExecutionEngine.h>

#include "utility.h"
#include "arena.h"
#include "intern.h"
#include "type/prim.h"
#include "core/core.h"
#include "machine/machine.h"

struct NecroSymTable;

typedef struct NecroCodeGenSymbolInfo
{
    LLVMTypeRef       type;
    LLVMValueRef      value;
    LLVMBasicBlockRef block;
} NecroCodeGenSymbolInfo;
NECRO_DECLARE_VECTOR(NecroCodeGenSymbolInfo, NecroCodeGenSymbol, necro_codegen_symbol);

typedef struct NecroRuntimeMapping
{
    NecroVar     var;
    LLVMValueRef value;
    void*        addr;
} NecroRuntimeMapping;
NECRO_DECLARE_VECTOR(NecroRuntimeMapping, NecroRuntimeMapping, necro_runtime_mapping);

typedef struct NecroCodeGenLLVM
{
    NecroPagedArena           arena;
    NecroSnapshotArena        snapshot_arena;
    NecroIntern*              intern;
    NecroSymTable*            symtable;
    NecroPrimTypes*           prim_types;
    NecroCodeGenSymbolVector  codegen_symtable;

    LLVMContextRef            context;
    LLVMModuleRef             mod;
    LLVMBuilderRef            builder;
    LLVMTargetDataRef         target;
    LLVMPassManagerRef        fn_pass_manager;
    LLVMPassManagerRef        mod_pass_manager;

    LLVMValueRef              memcpy_fn;
    LLVMTypeRef               poly_type;
    LLVMTypeRef               poly_ptr_type;
    LLVMTypeRef               word_int_type;
    LLVMTypeRef               word_uint_type;
    LLVMTypeRef               word_float_type;
    NecroVar                  necro_from_alloc_var;
    NecroVar                  necro_const_alloc_var;
    NecroRuntimeMappingVector runtime_mapping;
    bool                      should_optimize;
    NecroMemberVector*        member_map;
    NecroDataMapVector*       data_map;
} NecroCodeGenLLVM;

NecroCodeGenLLVM necro_create_codegen_llvm(NecroIntern* intern, struct NecroSymTable* symtable, NecroPrimTypes* prim_types, bool should_optimize);
void             necro_destroy_codegen_llvm(NecroCodeGenLLVM* codegen);
NECRO_RETURN_CODE necro_codegen_llvm(NecroCodeGenLLVM* codegen, NecroMachineProgram* program);
NECRO_RETURN_CODE necro_jit_llvm(NecroCodeGenLLVM* codegen);
void              necro_print_codegen_llvm(NecroCodeGenLLVM* codegen);

#endif // TYPE_CODEGEN_LLVM_H
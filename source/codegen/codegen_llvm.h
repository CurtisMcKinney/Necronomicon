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
#include "closure.h"
#include "machine/machine.h"

struct NecroSymTable;

typedef struct NecroCodeGenSymbolInfo
{
    LLVMTypeRef       type;
    LLVMValueRef      value;
    LLVMBasicBlockRef block;
} NecroCodeGenSymbolInfo;
NECRO_DECLARE_VECTOR(NecroCodeGenSymbolInfo, NecroCodeGenSymbol, necro_codegen_symbol);

typedef struct NecroCodeGenLLVM
{
    // Useful necro struct
    NecroPagedArena          arena;
    NecroSnapshotArena       snapshot_arena;
    NecroIntern*             intern;
    NecroSymTable*           symtable;
    NecroPrimTypes*          prim_types;
    NecroCodeGenSymbolVector codegen_symtable;
    // NecroRuntime*   runtime;

    LLVMContextRef           context;
    LLVMModuleRef            mod;
    LLVMBuilderRef           builder;
    LLVMTargetDataRef        target;
    LLVMPassManagerRef       fn_pass_manager;
    LLVMPassManagerRef       mod_pass_manager;

    // LLVMTypeRef           necro_data_type;
    // LLVMTypeRef           necro_val_type;
    // LLVMTypeRef           necro_env_type;
    // LLVMTypeRef           necro_closure_app_type;
    // NecroClosureTypeTable closure_type_table;
    LLVMValueRef             nalloc_value;
    LLVMTypeRef              poly_type;
} NecroCodeGenLLVM;

NecroCodeGenLLVM necro_create_codegen_llvm(NecroIntern* intern, struct NecroSymTable* symtable, NecroPrimTypes* prim_types);
void             necro_destroy_codegen_llvm(NecroCodeGenLLVM* codegen);

NECRO_RETURN_CODE necro_codegen_llvm(NecroCodeGenLLVM* codegen, NecroMachineProgram* program);

#endif // TYPE_CODEGEN_LLVM_H
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_LLVM_H
#define NECRO_LLVM_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/ExecutionEngine.h>

#include "utility.h"
#include "arena.h"
#include "intern.h"
#include "mach_ast.h"

///////////////////////////////////////////////////////
// NecroLLVM
///////////////////////////////////////////////////////
typedef struct NecroDelayedPhiNodeValue
{
    NecroSymbol       value_name;
    LLVMBasicBlockRef block;
    LLVMValueRef      phi_node;
} NecroDelayedPhiNodeValue;
NECRO_DECLARE_VECTOR(NecroDelayedPhiNodeValue, NecroDelayedPhiNodeValue, delayed_phi_node_value)

typedef struct NecroLLVM
{
    NecroPagedArena                arena;
    NecroSnapshotArena             snapshot_arena;
    NecroIntern*                   intern;
    NecroBase*                     base;
    NecroMachProgram*              program;

    LLVMContextRef                 context;
    LLVMModuleRef                  mod;
    LLVMBuilderRef                 builder;
    LLVMTargetMachineRef           target_machine;
    LLVMTargetDataRef              target;
    LLVMPassManagerRef             fn_pass_manager;
    LLVMPassManagerRef             mod_pass_manager;
    LLVMExecutionEngineRef         engine;

    bool                           should_optimize;
    NecroDelayedPhiNodeValueVector delayed_phi_node_values;
} NecroLLVM;

NecroLLVM necro_llvm_empty();
void      necro_llvm_destroy(NecroLLVM* codegen);
void      necro_llvm_codegen(NecroCompileInfo info, NecroMachProgram* program, NecroLLVM* codegen);
void      necro_llvm_jit(NecroCompileInfo info, NecroLLVM* codegen);
void      necro_llvm_test();
void      necro_llvm_test_jit();

#endif // NECRO_LLVM_H
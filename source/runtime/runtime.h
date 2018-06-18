/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "codegen/codegen.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

typedef struct
{
    LLVMValueRef necro_alloc;
    LLVMValueRef necro_print;
    LLVMValueRef necro_print_u64;
    LLVMValueRef necro_sleep;
    LLVMValueRef necro_collect;
    LLVMValueRef necro_initialize_root_set;
    LLVMValueRef necro_set_root;
    LLVMValueRef necro_error_exit;
    LLVMValueRef necro_init_runtime;
    LLVMValueRef necro_update_runtime;
    LLVMValueRef necro_mouse_x;
    LLVMValueRef necro_mouse_y;
} NecroRuntimeFunctions;

typedef struct NecroRuntime
{
    NecroRuntimeFunctions functions;
} NecroRuntime;

NecroRuntime necro_create_runtime();
void         necro_destroy_runtime(NecroRuntime* runtime);
void         necro_declare_runtime_functions(NecroRuntime* runtime, NecroCodeGen* codegen);
void         necro_bind_runtime_functions(NecroRuntime* runtime, LLVMExecutionEngineRef engine);

#endif // RUNTIME_H
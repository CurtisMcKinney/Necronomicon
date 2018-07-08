/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CODEGEN_CASE_H
#define CODEGEN_CASE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/ExecutionEngine.h>

#include "codegen.h"

LLVMValueRef necro_codegen_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer);
void         necro_calculate_node_prototype_case(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node);

#endif // CODEGEN_CASE_H
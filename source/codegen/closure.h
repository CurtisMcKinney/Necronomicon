/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CODEGEN_CLOSURE_H
#define CODEGEN_CLOSURE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/ExecutionEngine.h>

#include "codegen.h"

/*
    _NecroClosure_:
        { NecroData, Function*, NecroVal* (mkNode), NecroEnv* }
    Notes:
        1. The function can be stateful or stateless (needing a node or not)
        2. If stateful the mkNode function will construct the state node it requires (Each call site needs a unique state node), otherwise it is NULL
        3. The function can have an Env or not (depending on whether or not it captured variables), a NULL indicates it requires no env
        4. These are the cases, identified by an int stored in the tag value:
            * 0 - Stateless, NoEnv: args -> b
            * 1 - Stateful,  NoEnv: Node* -> args -> b
            * 2 - Stateless, Env:   Env*  -> args -> b
            * 3 - Stateful,  Env:   Node* -> Env* -> args -> b
        5. Some kind of unique id?!?!?!

    NecroApp persistent vars
    { NecroData, NecroVal* (node) }
*/

LLVMTypeRef  necro_env_type(NecroCodeGen* codegen, LLVMTypeRef* elems, size_t elem_count);
LLVMTypeRef  necro_closure_type(NecroCodeGen* codegen, LLVMTypeRef function_ptr_type);
LLVMValueRef necro_mk_env(NecroCodeGen* codegen, LLVMTypeRef env_type, LLVMValueRef* elems, uint16_t elem_count);
LLVMValueRef necro_mk_closure(NecroCodeGen* codegen, LLVMTypeRef closure_type, LLVMValueRef function_ptr, LLVMValueRef node_mk_function_ptr, LLVMValueRef env);
LLVMValueRef necro_hoist_function(NecroCodeGen* codegen, NecroCoreAST_Expression* function);
LLVMTypeRef  necro_function_type(NecroCodeGen* codegen, NecroType* type, bool is_top_level);


bool         necro_calculate_node_prototype_app_closure(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer_node);
bool         necro_calculate_node_call_app_closure(NecroCodeGen* codegen, NecroCoreAST_Expression* ast, NecroNodePrototype* outer, LLVMValueRef* out_result);
#endif // CODEGEN_CLOSURE_H
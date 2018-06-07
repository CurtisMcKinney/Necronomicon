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
#include "type/infer.h"
#include "type/prim.h"
#include "intern.h"
#include "utility/small_array.h"
#include "utility/utility.h"
#include "core/core.h"

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

//=====================================================
// Types
//=====================================================

typedef struct
{
    uint64_t index;
} NecroBlockIndex;

typedef struct NecroCodeGen
{
    NecroPagedArena       arena;
    NecroSnapshotArena    snapshot_arena;
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

typedef enum
{
    NECRO_NODE_CONSTANT,
    NECRO_NODE_STATELESS,
    NECRO_NODE_STATEFUL
} NECRO_NODE_TYPE;

typedef struct
{
    LLVMTypeRef llvm_type;
    NecroVar    var;
} NecroArg;

typedef struct
{
    NecroVar                  var;
    bool                      is_persistent;
    size_t                    slot;
    NecroCoreAST_Application* app;
} NecroCall;

typedef struct
{
    struct NecroNodePrototype* prototype;
    size_t                     slot;
} NecroPersistentVar;

typedef struct
{
    struct NecroNodePrototype* prototype;
    NecroVar                   var;
    size_t                     slot;
} NecroCapturedVar;

NECRO_DECLARE_ARENA_LIST(LLVMTypeRef, LLVMType, llvm_type);
NECRO_DECLARE_ARENA_LIST(NecroCon, Con, necro_con);
NECRO_DECLARE_ARENA_LIST(NecroVar, Var, var);
NECRO_DECLARE_ARENA_LIST(NecroCall, Call, call);
NECRO_DECLARE_ARENA_LIST(NecroPersistentVar, PersistentVar, persistent_var);
NECRO_DECLARE_ARENA_LIST(NecroCapturedVar, CapturedVar, captured_var);
NECRO_DECLARE_ARENA_LIST(NecroArg, Arg, arg);

typedef struct NecroNodePrototype
{
    NecroVar                   bind_var;
    NecroSymbol                name;
    LLVMTypeRef                node_type;          // Type of the actual node
    LLVMTypeRef                node_value_type;    // The value type that the node calculates
    NecroArgList*              args;               // Arguments to be passed in. Will be passed in as raw values in registers or on the stack
    NecroVarList*              loaded_vars;
    NecroCallList*             called_functions;
    NecroVarList*              local_vars;
    NecroPersistentVarList*    persistent_vars;
    NecroCapturedVarList*      captured_vars;
    LLVMValueRef               mk_function;
    LLVMValueRef               init_function;
    LLVMValueRef               call_function;
    bool                       is_function;        // Is this necessary?!
    bool                       was_captured;
    struct NecroNodePrototype* outer;
    size_t                     slot_count;
    NECRO_NODE_TYPE            type;
} NecroNodePrototype;

NecroCodeGen        necro_create_codegen(struct NecroInfer* infer, struct NecroIntern* intern, struct NecroSymTable* symtable, const char* module_name);
void                necro_destroy_codegen(NecroCodeGen* codegen);
void                necro_test_codegen(NecroCodeGen* codegen);
NecroSymbol         necro_new_block_name(NecroCodeGen* codegen);
NECRO_RETURN_CODE   necro_codegen(NecroCodeGen* codegen, struct NecroCoreAST* core_ast);
NECRO_RETURN_CODE   necro_verify_and_dump_codegen(NecroCodeGen* codegen);
LLVMValueRef        necro_snapshot_add_function(NecroCodeGen* codegen, const char* function_name, LLVMTypeRef return_type, LLVMTypeRef* arg_refs, size_t arg_count);
LLVMValueRef        necro_snapshot_gep(NecroCodeGen* codegen, const char* ptr_name, LLVMValueRef data_ptr, size_t num_indices, uint32_t* indices);
LLVMValueRef        necro_alloc_codegen(NecroCodeGen* codegen, uint64_t bytes);
char*               necro_concat_strings(NecroSnapshotArena* arena, uint32_t string_count, const char** strings);
NecroNodePrototype* necro_create_necro_node_prototype(NecroCodeGen* codegen, NecroVar bind_var, const char* name, LLVMTypeRef node_type, LLVMTypeRef node_value_type, NecroNodePrototype* outer, NECRO_NODE_TYPE type);
NecroNodePrototype* necro_create_prim_node_prototype(NecroCodeGen* codegen, NecroVar prim_var, LLVMTypeRef prim_result_type, LLVMValueRef prim_call_function, NECRO_NODE_TYPE type);
LLVMValueRef        necro_gen_alloc_boxed_value(NecroCodeGen* codegen, LLVMTypeRef boxed_type, uint32_t necro_value1, uint32_t necro_value2, const char* name);

inline bool necro_is_codegen_error(NecroCodeGen* codegen)
{
    return codegen->error.return_code == NECRO_ERROR;
}

inline void necro_throw_codegen_error(NecroCodeGen* codegen, struct NecroCoreAST_Expression* ast, const char* error_message)
{
    necro_error(&codegen->error, (NecroSourceLoc) {0}, error_message);
}

#endif // TYPE_CODEGEN_H
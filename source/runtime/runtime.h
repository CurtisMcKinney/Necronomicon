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
// #include "slab.h"
// #include "vault.h"
// #include "region.h"

typedef struct
{
    LLVMValueRef necro_alloc;
    LLVMValueRef necro_print;
    LLVMValueRef necro_print_u64;
    LLVMValueRef necro_sleep;
    LLVMValueRef necro_collect;
    LLVMValueRef necro_initialize_root_set;
    LLVMValueRef necro_set_root;
} NecroRuntimeFunctions;

typedef struct NecroRuntime
{
    NecroRuntimeFunctions functions;
} NecroRuntime;

NecroRuntime necro_create_runtime();
void         necro_destroy_runtime(NecroRuntime* runtime);
void         necro_declare_runtime_functions(NecroRuntime* runtime, NecroCodeGen* codegen);
void         necro_bind_runtime_functions(NecroRuntime* runtime, LLVMExecutionEngineRef engine);

//=====================================================
// VERY OLD VM STUFF
//=====================================================
// /*
//     Lexer:            Text         -> Lex Tokens
//     Parser:           Lex Tokens   -> AST Symbol
//     Desugar:          AST Symbol   -> AST Symbol
//     Regnaming:        AST Symbol   -> AST ID
//     Type Checking:    AST ID       -> TypedAST ID
//     Core translation: TypedAST ID  -> CoreAST
//     CodeGenerator:    CoreAST      -> ByteCode
//     VM:               ByteCode     -> Execution
// */

// /*
//     Necronomicon is demand-rate, much like Lucid.
//     There is no fby, sequences  are the main data flow primitive instead.
//     Sequences are denoted by [].
//     [x, y, z] is a sequence always proceeds as {x0..xYield, y0..yYield, z0..zYield}, after which the sequence yields
//     loop is a combinator which takes a sequence and loop it forever without yielding
//     seq is a combinator which takes a sequences, remembers each sub-stream's last yield point and keeps iterating past it, with yields at every loop point.
//     seq [x, y, z] => {x0..xYield, y0..yYield, z0..zYield, YIELD, xYield+1..xYield', yYield+1..yYield', zYield+1..zYield'}
//     ....GC / Memory management....How?!?!!??!!?
//     current combinator calculates stream to the current streams time slice

//     myCoolPattern = seq [mx, [300, 400], mx, [500, 600]]
//         where
//             mx = mouseX |> current |> yieldEvery 1

//     myCoolSynth = synth |> gain 0.1
//         where
//             synth = 440 + [0, synth * 440] |> sin

//     myCoolSynth = do
//         r1 <- rand 0.1 1
//         mx <- mouseX
//         let synth = 440 + mx + r1 + [0, synth * 440] |> sin
//         synth |> gain 0.1 |> return

//     myCoolSynth = synth |> gain 0.1
//         where
//             synth = 440 + current mouseX * [0, synth * 440] |> sin

//     // Env "yields"
//     myCoolSynths = loop [s1, s2]
//         where
//             s1 = sin 440 |> perc 0.1 |> gain 0.1
//             s2 = sin 440 |> perc 0.2 |> gain 0.2

//     If we use Lucid style immutable purity and demand-driven evaluation (every value has a "time" and a "place"), then you HAVE
//     to store the entire history of IO. What is the way out of that??? Or do we just say fuck it?
//     If we do it like Lucid, plus keep the entire IO Log, then we can have dynamic signals from things like poly
//     simply create another way to locate something in the vault: (relative) time, place, dimension.
//     Then we don't need "true" dynamic signals, and we don't need traditional GC, and can use a variation of the Lucid retirement plan GC

//     Evaluation then simply turns into:
//         - Request a value at a specific (relative) time, place, from the vault.
//         - If it exists:   Retrieve the value and increase its retirement value
//         - If it does not: Allocate the necessary memory, evaluate that bit of code store that in the vault at that time, place, and dimension, then set its retirement age. (Vault = Multiple Slab allocators with some kind of easy look up method)
//         - Each Tick store all possible IO in the vault.

//     Memory reclamation then turns into:
//         - Each tick decrease the retirement age of each value (besides IO which is immortal) in the vault
//         - For each item in the vault that reaches a 0 retirement age, reclaim its memory

//     Where does Place come from?:
//         - Places are thunks, which both serve as a unique identifier
//           as well as a container for goodies that need to be pushed onto the stack
//         - Demand then turns into a force, but with a time element.
//         - Calculations for when to make thunks, how to enter thunks,
//           and what to push to the stack are done during code generation

//     Where are thunks stored?:
//         - Into regions using a NON-GC region allocator
//         - Region Allocator is O(1) to allocate and O(1) to collect
//         - This essentially translates into 0 pause times.
//         - Time variance might mean that multiple regions will be created
//           to represent the same synth / new region at different points in time
//           but since we're immutable this is actually ok, and everything will get sorted out....right?

//     - Ideas #2:
//         * Thunks are lazy bits of code just like Haskell
//         * Unlike Haskell thunks can memoize values at different points in time
//         * Each thunk has a fixed buffer (NecroVault) which is an HashTable from time -> value
//         * Thunks and their associated NecroVault buffers are allocated using a region allocation scheme
//         * When the region is freed all memory associated with the Thunks and their value buffers are also freed
//         * This scheme doesn't handle large values or variable-sized values (like linked lists) well at all.
//           (To be fair the previous scheme doesn't handle variable-sized values well either [Not true, linked lists can operate like a separate time stream?])
//         * Switch Thunk value buffers to being backed by something like a red black tree (something which handles small values of N and growing / shrinking better?)
// */

// //=====================================================
// // VM
// //=====================================================

// typedef enum
// {
//     // Integer operations
//     N_PUSH_I,
//     N_ADD_I,
//     N_SUB_I,
//     N_MUL_I,
//     N_NEG_I,
//     N_DIV_I,
//     N_MOD_I,
//     N_EQ_I,
//     N_NEQ_I,
//     N_LT_I,
//     N_LTE_I,
//     N_GT_I,
//     N_GTE_I,
//     N_BIT_AND_I,
//     N_BIT_OR_I,
//     N_BIT_XOR_I,
//     N_BIT_LS_I,
//     N_BIT_RS_I,

//     // Functions
//     N_CALL,
//     N_RETURN,
//     N_C_CALL_1,
//     N_C_CALL_2,
//     N_C_CALL_3,
//     N_C_CALL_4,
//     N_C_CALL_5,

//     // structs
//     N_MAKE_STRUCT,
//     N_GET_FIELD,
//     N_SET_FIELD,

//     // Memory
//     N_LOAD_L,
//     N_STORE_L,

//     // Jumping
//     N_JMP,
//     N_JMP_IF,
//     N_JMP_IF_NOT,

//     // Commands
//     N_POP,
//     N_PRINT,
//     N_PRINT_STACK,
//     N_HALT
// } NECRO_BYTE_CODE;

// // Type information, needed for GC
// // Types should be completely monomorphized!
// typedef struct
// {
//     uint64_t boxed_slot_bit_field;
//     uint32_t segment_size_in_bytes;
//     uint32_t gc_segment;
//     uint32_t num_slots;
// } NecroTypeInfo;

// // In-band 8 byte struct information.
// // Every struct has this appended
// // Used for pattern matching and for GC
// typedef struct
// {
//     uint32_t pattern_tag;
//     uint32_t type_index; // Types should be completely monomorphized!
// } NecroStructInfo;

// typedef union NecroVal
// {
//     int64_t         int_value;
//     double          float_value;
//     union NecroVal* necro_pointer;
//     NecroStructInfo struct_info;
// } NecroVal;

// typedef NecroVal (*necro_c_call_1)(NecroVal);
// typedef NecroVal (*necro_c_call_2)(NecroVal, NecroVal);
// typedef NecroVal (*necro_c_call_3)(NecroVal, NecroVal, NecroVal);
// typedef NecroVal (*necro_c_call_4)(NecroVal, NecroVal, NecroVal, NecroVal);
// typedef NecroVal (*necro_c_call_5)(NecroVal, NecroVal, NecroVal, NecroVal, NecroVal);

// // Tick based
// //=====================================================
// // Treadmill Memory management:
// //     * Incremental
// //     * Precise
// //     * Non-copying (Never invalidates pointers, never reallocates, never copies)
// //     * Segmented Block sizes, up to 64 slots in size (512 bytes, will need a different allocator for differently sized objects)
// //     * Fixed Page length
// //     * Collection called every tick
// //     * Bounded collection time
// //     * Adds up to trading overall efficiency
// //       in space and time for lower latency.
// //=====================================================

// #define NECRO_NUM_TM_SEGMENTS 6
// #define NECRO_TM_PAGE_SIZE    2048
// #define DEBUG_TM              1

// #if DEBUG_TM
// #define TRACE_TM(...) printf(__VA_ARGS__)
// #else
// #define TRACE_TM(...)
// #endif

// // Out of band 16 byte Tag
// typedef struct NecroTMTag
// {
//     struct NecroTMTag* prev;
//     struct NecroTMTag* next;
// } NecroTMTag;

// // Page header where out of band GC tags and ecru flags are stored
// typedef struct NecroTMPageHeader
// {
//     NecroTMTag                tags[NECRO_TM_PAGE_SIZE];
//     uint64_t                  ecru_flags[NECRO_TM_PAGE_SIZE / 64];
//     struct NecroTMPageHeader* next_page;
// } NecroTMPageHeader;

// // Main GC struct
// typedef struct
// {
//     bool               complete;
//     NecroTypeInfo*     type_infos; // TODO: Have to get TypeInfo into here!
//     NecroTMPageHeader* pages;
//     size_t             allocated_blocks[NECRO_NUM_TM_SEGMENTS];
//     NecroTMTag*        top[NECRO_NUM_TM_SEGMENTS];
//     NecroTMTag*        bottom[NECRO_NUM_TM_SEGMENTS];
//     NecroTMTag*        free[NECRO_NUM_TM_SEGMENTS];
//     NecroTMTag*        scan[NECRO_NUM_TM_SEGMENTS];
// } NecroTreadmill;

// NecroTreadmill necro_create_treadmill(size_t num_initial_pages, NecroTypeInfo* type_info_table);
// void           necro_destroy_treadmill(NecroTreadmill* treadmill);
// NecroVal       necro_treadmill_alloc(NecroTreadmill* treadmill, uint32_t type_index);
// void           necro_print_treadmill(NecroTreadmill* treadmill);
// void           necro_test_treadmill();

// //=====================================================
// // Testing
// //=====================================================
// void print_test_result(const char* print_string, bool result);
// void necro_test_vm();
// void necro_trace_stack(int64_t opcode);
// // void necro_test_region();
// void necro_test_slab();

// #define NECRO_STACK_SIZE 1024
// #define DEBUG_VM 0

// #if DEBUG_VM
// #define TRACE_STACK(opcode) necro_trace_stack(opcode)
// #else
// // #define TRACE_STACK(opcode)
// #endif

// //=====================================================
// // Demand VM
// //=====================================================

// /*
//     I = Int
//     F = Float
//     A = Audio
//     S = Struct
//     R = Region
//     T = Thunk
// */

// typedef enum
// {
//     // Region Memory
//     DVM_New_R,
//     DVM_Free_R,
//     DVM_PUSH_R,
//     DVM_POP_R,

//     DVM_MAKE_T,

//     // Demand
//     DVM_DEMAND_I,
//     DVM_DEMAND_F,
//     DVM_DEMAND_A,
//     DVM_DEMAND_S,
//     DVM_STORE_I,
//     DVM_STORE_F,
//     DVM_STORE_A,
//     DVM_STORE_S,

//     // Integer operations
//     DVM_PUSH_I,
//     DVM_ADD_I,
//     DVM_SUB_I,
//     DVM_MUL_I,
//     DVM_NEG_I,
//     DVM_DIV_I,
//     DVM_MOD_I,
//     DVM_EQ_I,
//     DVM_NEQ_I,
//     DVM_LT_I,
//     DVM_LTE_I,
//     DVM_GT_I,
//     DVM_GTE_I,
//     DVM_BIT_AND_I,
//     DVM_BIT_OR_I,
//     DVM_BIT_XOR_I,
//     DVM_BIT_LS_I,
//     DVM_BIT_RS_I,

//     // Functions
//     DVM_CALL,
//     DVM_RETURN,
//     DVM_C_CALL_1,
//     DVM_C_CALL_2,
//     DVM_C_CALL_3,
//     DVM_C_CALL_4,
//     DVM_C_CALL_5,

//     // structs
//     DVM_MAKE_STRUCT,
//     DVM_GET_FIELD,
//     DVM_SET_FIELD,

//     // Local Memory (VM Used only?)
//     DVM_LOAD_L,
//     DVM_STORE_L,

//     // Jumping
//     DVM_JMP,
//     DVM_JMP_IF,
//     DVM_JMP_IF_NOT,

//     // Commands
//     DVM_POP,
//     DVM_PRINT,
//     DVM_PRINT_STACK,
//     DVM_HALT
// } DVM_NECRO_BYTE_CODE;

// void necro_trace_stack_dvm(int64_t opcode);
// void necro_test_dvm();

// #define DEBUG_DVM 0

// #if DEBUG_DVM
// #define TRACE_STACK(opcode) necro_trace_stack_dvm(opcode)
// #else
// #define TRACE_STACK(opcode)
// #endif

#endif // RUNTIME_H
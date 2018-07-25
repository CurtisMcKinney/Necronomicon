/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACHINE_H
#define NECRO_MACHINE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"
#include "utility/arena.h"
#include "machine_type.h"
#include "machine_copy.h"
#include "utility/list.h"

///////////////////////////////////////////////////////
// NecroMachine
//-----------
// * Abstract machine target for necrolang
// * LLVM-IR like, but simpler and necrolang specific
// * Infinite registers
// * Heap defined by a graph of statically calculated blocks containing blocks
// * Machines retain state across multiple calls to main
///////////////////////////////////////////////////////

/*
    TODO:

        * New memory scheme:
            - delay && trimDelay: delay :: a -> a -> a (but disallows recursive value), trimDelay :: Trimmable a => const Int -> a -> a -> a (allows recursive constructors but they must implement trimmable)
              bufDelay :: const Int -> a -> a
            - class Trimmable a where
                nil  :: a (must be a non-recursive contructor)
            - trim :: Trimmable a => const Int -> a -> a (auto-magically crawls the data type and replaces recursions beyond N with the nil constructor
            - using delay and trimDelay we can statically calculate the size of each block
            - delay and trimDelay allocate a statically sized area of memory. When something is delayed it is deep copied into this memory area.
            - A block is a statically known area of data
            - Somethings dynamically create new blocks (recursion, applying closures, poly, etc)
            - Blocks are allocated and freed explicity by the runtime.
            - All IO must use fixed size data types
            - Thus, there is no garbage collection at runtime.
            - How to also get unboxed and untagged Int and Float types...
            - We should statically know ALL the types going into delay (since it requires the type be fully concrete)
            - call a fully resolved _clone on the type, a compiler generated method for known sized types that recursively copies
            - Everything is given a specific address in contiguous memory and does in place overwriting.
            - Necrolang: A real-time audio programming language (without garbage collection)
            - also need to deep copy into global roots!

        * Break API into machine_build.h/c
        * null out and zero out memory when allocated with nalloc via memcpy NULL....should be faster than doing it in code!?
        * closures
        * Proper delay (Keyword? Laziness? ...?).
        * prev Keyword? prev Const Ident, i.e. 1 + prev 0 x
        * necro_verify_machine_program

    TEST:
        * case
        * runtime
        * Construct main function
        * codegen
        * init_and_load_dyn function (will require breakcond and other stuff to work)
        * dyn fn machines need NecroData?
*/

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
struct NecroMachineAST;
struct NecroMachineLit;
struct NecroMachineDef;

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_NAME_UNIQUE,
    NECRO_NAME_NOT_UNIQUE
} NECRO_NAME_UNIQUENESS;

typedef enum
{
    NECRO_MACHINE_VALUE_VOID,
    NECRO_MACHINE_VALUE_REG,
    NECRO_MACHINE_VALUE_PARAM,
    NECRO_MACHINE_VALUE_GLOBAL,
    NECRO_MACHINE_VALUE_UINT1_LITERAL,
    NECRO_MACHINE_VALUE_UINT8_LITERAL,
    NECRO_MACHINE_VALUE_UINT16_LITERAL,
    NECRO_MACHINE_VALUE_UINT32_LITERAL,
    NECRO_MACHINE_VALUE_UINT64_LITERAL,
    NECRO_MACHINE_VALUE_INT32_LITERAL,
    NECRO_MACHINE_VALUE_INT64_LITERAL,
    NECRO_MACHINE_VALUE_F32_LITERAL,
    NECRO_MACHINE_VALUE_F64_LITERAL,
    NECRO_MACHINE_VALUE_NULL_PTR_LITERAL,
} NECRO_MACHINE_VALUE_TYPE;

typedef struct NecroMachineValue
{
    union
    {
        NecroVar reg_name;
        struct NecroParamReg
        {
            NecroVar fn_name;
            size_t   param_num;
        } param_reg;
        bool     uint1_literal;
        uint8_t  uint8_literal;
        uint16_t uint16_literal;
        uint32_t uint32_literal;
        uint64_t uint64_literal;
        int32_t  int32_literal;
        int64_t  int64_literal;
        float    f32_literal;
        double   f64_literal;
        NecroVar global_name;
    };
    NECRO_MACHINE_VALUE_TYPE value_type;
} NecroMachineValue;

typedef struct NecroSlot
{
    size_t                  slot_num;
    NecroMachineType*       necro_machine_type;
    struct NecroMachineDef* machine_def;
} NecroSlot;

typedef struct NecroMachineSwitchData
{
    struct NecroMachineAST* block;
    size_t                  value;
} NecroMachineSwitchData;
NECRO_DECLARE_ARENA_LIST(NecroMachineSwitchData, MachineSwitch, machine_switch);

typedef struct NecroTerminator
{
    union
    {
        struct NecroSwitchTerminator
        {
            NecroMachineSwitchList* values;
            struct NecroMachineAST* else_block;
            struct NecroMachineAST* choice_val;
        } switch_terminator;
        struct NecroReturnTerminator
        {
            struct NecroMachineAST* return_value;
        } return_terminator;
        struct NecroBreakTerminator
        {
            struct NecroMachineAST* block_to_jump_to;
        } break_terminator;
        struct NecroCondBreak
        {
            struct NecroMachineAST* true_block;
            struct NecroMachineAST* false_block;
            struct NecroMachineAST* cond_value;
        } cond_break_terminator;
    };
    enum
    {
        NECRO_TERM_SWITCH,
        NECRO_TERM_RETURN,
        NECRO_TERM_BREAK,
        NECRO_TERM_COND_BREAK,
        NECRO_TERM_UNREACHABLE,
    } type;
} NecroTerminator;

///////////////////////////////////////////////////////
// NecroMachineAST
///////////////////////////////////////////////////////
typedef struct NecroMachineStructDef
{
    NecroVar        name;
} NecroMachineStructDef;

typedef struct NecroMachineBlock
{
    NecroVar                 name;
    struct NecroMachineAST** statements;
    size_t                   num_statements;
    size_t                   statements_size;
    NecroTerminator*         terminator;
    struct NecroMachineAST*  next_block;
} NecroMachineBlock;

typedef struct NecroMachineDef
{
    NecroVar                bind_name;
    NecroVar                machine_name;
    NecroVar                state_name;

    struct NecroMachineAST* mk_fn;
    struct NecroMachineAST* init_fn;
    struct NecroMachineAST* update_fn;
    NecroMachineBlock*      update_error_block;
    uint32_t                initial_tag;
    NECRO_STATE_TYPE        state_type;
    struct NecroMachineAST* outer;
    NecroMachineType*       value_type;
    NecroMachineType*       fn_type;
    bool                    is_pushed;
    bool                    is_recursive;
    bool                    is_persistent_slot_set;
    bool                    references_stateful_global;

    // args
    NecroVar*               arg_names;
    NecroType**             arg_types;
    size_t                  num_arg_names;

    // members
    NecroSlot*              members;
    size_t                  num_members;
    size_t                  members_size;
    int32_t                 _first_dynamic;
    size_t                  total_slots;

    struct NecroMachineAST* global_value; // If global
    struct NecroMachineAST* global_state; // If global
    // cache if and where slots have been loaded!?
} NecroMachineDef;

typedef enum
{
    NECRO_FN_FN,
    NECRO_FN_RUNTIME,
} NECRO_FN_TYPE;

typedef struct NecroMachineFnDef
{
    NecroVar                name;
    struct NecroMachineAST* call_body;
    NECRO_FN_TYPE           fn_type;
    struct NecroMachineAST* fn_value;
    void*                   runtime_fn_addr;
    // TODO: instead of hacks like this, put NECRO_STATE_TYPE into here
    bool                    is_primitively_stateful;
    //-------------------
    // compile time data
    struct NecroMachineAST* _curr_block;
    struct NecroMachineAST* _init_block;
    struct NecroMachineAST* _cont_block;
    struct NecroMachineAST* _err_block;
} NecroMachineFnDef;

typedef struct NecroMachineCall
{
    struct NecroMachineAST*  fn_value;
    struct NecroMachineAST** parameters;
    size_t                   num_parameters;
    struct NecroMachineAST*  result_reg;
} NecroMachineCall;

typedef struct NecroMachineLoad
{
    union
    {
        struct NecroMachineAST* source_ptr;
        struct
        {
            struct NecroMachineAST* source_ptr;
            size_t                  source_slot;
        } load_slot;
    };
    enum
    {
        NECRO_LOAD_TAG,
        NECRO_LOAD_PTR,
        NECRO_LOAD_SLOT,
    } load_type;
    struct NecroMachineAST* dest_value;
} NecroMachineLoad;

typedef struct NecroMachineStore
{
    struct NecroMachineAST* source_value;
    union
    {
        struct NecroMachineAST* dest_ptr;
        struct
        {
            struct NecroMachineAST* dest_ptr;
            size_t                  dest_slot;
        } store_slot;
    };
    enum
    {
        NECRO_STORE_TAG,
        NECRO_STORE_PTR,
        NECRO_STORE_SLOT,
    } store_type;
} NecroMachineStore;

typedef struct NecroMachineConstantDef
{
    union
    {
        int64_t constant_i64;
        double  constant_double;
        char    constant_char;
        struct NecroConstantStruct
        {
            NecroVar name;
        } constant_struct;
    };
    enum
    {
        NECRO_CONSTANT_INT64,
        NECRO_CONSTANT_F64,
        NECRO_CONSTANT_CHAR,
        NECRO_CONSTANT_STRUCT
    } constant_type;
} NecroMachineConstantDef;

// TODO/NOTE: Do we need this if we use load slot / store tag, etc?
typedef struct NecroMachineGetElementPtr
{
    struct NecroMachineAST* source_value;
    uint32_t*            indices;
    size_t               num_indices;
    struct NecroMachineAST* dest_value;
} NecroMachineGetElementPtr;

typedef struct NecroMachineBitCast
{
    struct NecroMachineAST* from_value;
    struct NecroMachineAST* to_value;
} NecroMachineBitCast;

typedef struct NecroMachineNAlloc
{
    NecroMachineType*       type_to_alloc;
    uint32_t                slots_used;
    struct NecroMachineAST* result_reg;
} NecroMachineNAlloc;

typedef enum
{
    NECRO_MACHINE_BINOP_IADD,
    NECRO_MACHINE_BINOP_ISUB,
    NECRO_MACHINE_BINOP_IMUL,
    NECRO_MACHINE_BINOP_IDIV,
    NECRO_MACHINE_BINOP_FADD,
    NECRO_MACHINE_BINOP_FSUB,
    NECRO_MACHINE_BINOP_FMUL,
    NECRO_MACHINE_BINOP_FDIV,
} NECRO_MACHINE_BINOP_TYPE;

typedef struct NecroMachineBinOp
{
    NECRO_MACHINE_BINOP_TYPE binop_type;
    struct NecroMachineAST*  left;
    struct NecroMachineAST*  right;
    struct NecroMachineAST*  result;
} NecroMachineBinOp;

typedef enum
{
    NECRO_MACHINE_CMP_EQ,
    NECRO_MACHINE_CMP_NE,
    NECRO_MACHINE_CMP_GT,
    NECRO_MACHINE_CMP_GE,
    NECRO_MACHINE_CMP_LT,
    NECRO_MACHINE_CMP_LE,
} NECRO_MACHINE_CMP_TYPE;

typedef struct NecroMachineCmp
{
    struct NecroMachineAST*  left;
    struct NecroMachineAST*  right;
    struct NecroMachineAST*  result;
    NECRO_MACHINE_CMP_TYPE   cmp_type;
} NecroMachineCmp;

typedef struct NecroMachinePhiData
{
    struct NecroMachineAST* block;
    struct NecroMachineAST* value;
} NecroMachinePhiData;

NECRO_DECLARE_ARENA_LIST(NecroMachinePhiData, MachinePhi, machine_phi);

typedef struct NecroMachinePhi
{
    NecroMachinePhiList*     values;
    struct NecroMachineAST*  result;
} NecroMachinePhi;

typedef struct NecroMachineMemCpy
{
    struct NecroMachineAST* dest;
    struct NecroMachineAST* source;
    struct NecroMachineAST* num_bytes;
} NecroMachineMemCpy;

/*
    Notes:
        * Machines retain state
        * Constants never change (constructed at compile time?). Can be: Literal or Aggregate types only. hoisted and burned into data section.
        * Closures are a specific kind of machine
        * Env are a specific kind of data structure which are projected via case (cleanup work on projection only case statements!)
*/
typedef enum
{
    NECRO_MACHINE_VALUE,
    NECRO_MACHINE_BLOCK,

    // ops
    NECRO_MACHINE_CALL,
    NECRO_MACHINE_LOAD,
    NECRO_MACHINE_STORE,
    NECRO_MACHINE_NALLOC,
    NECRO_MACHINE_BIT_CAST,
    NECRO_MACHINE_GEP,
    NECRO_MACHINE_BINOP,
    NECRO_MACHINE_CMP,
    NECRO_MACHINE_PHI,
    NECRO_MACHINE_MEMCPY,

    // Defs
    NECRO_MACHINE_STRUCT_DEF,
    NECRO_MACHINE_FN_DEF,
    NECRO_MACHINE_DEF,
    // NECRO_MACHINE_CONSTANT_DEF,
} NECRO_MACHINE_AST_TYPE;

typedef struct NecroMachineAST
{
    union
    {
        NecroMachineValue         value;
        NecroMachineCall          call;
        NecroMachineLoad          load;
        NecroMachineStore         store;
        NecroMachineBlock         block;
        NecroMachineDef           machine_def;
        NecroMachineFnDef         fn_def;
        NecroMachineStructDef     struct_def;
        NecroMachineConstantDef   constant;
        NecroMachineGetElementPtr gep;
        NecroMachineBitCast       bit_cast;
        NecroMachineNAlloc        nalloc;
        NecroMachineBinOp         binop;
        NecroMachineCmp           cmp;
        NecroMachinePhi           phi;
        NecroMachineMemCpy        memcpy;
    };
    NECRO_MACHINE_AST_TYPE type;
    NecroMachineType*      necro_machine_type;
} NecroMachineAST;

NECRO_DECLARE_VECTOR(NecroMachineAST*, NecroMachineAST, necro_machine_ast);

typedef struct NecroRuntime
{
    NecroVar _necro_init_runtime;
    NecroVar _necro_update_runtime;
    NecroVar _necro_error_exit;
    NecroVar _necro_sleep;
    NecroVar _necro_print;

    //---------------
    // old mark sweep gc
    // NecroVar _necro_initialize_root_set;
    // NecroVar _necro_set_root;
    // NecroVar _necro_alloc;
    // NecroVar _necro_collect;

    //---------------
    // new copy_gc
    NecroVar _necro_from_alloc;
    NecroVar _necro_to_alloc;
    NecroVar _necro_copy_gc_initialize_root_set;
    NecroVar _necro_copy_gc_set_root;
    NecroVar _necro_copy_gc_collect;
} NecroMachineRuntime;

typedef enum
{
    NECRO_WORD_4_BYTES, // 32-bit
    NECRO_WORD_8_BYTES, // 64-bit
} NECRO_WORD_SIZE;

typedef struct NecroMachineProgram
{
    // Program info
    NecroMachineASTVector structs;
    NecroMachineASTVector functions;
    NecroMachineASTVector machine_defs;
    NecroMachineASTVector globals;
    NecroMachineAST*      necro_main;
    NECRO_WORD_SIZE       word_size;

    // Useful structs
    NecroPagedArena       arena;
    NecroSnapshotArena    snapshot_arena;
    NecroIntern*          intern;
    NecroSymTable*        symtable;
    NecroScopedSymTable*  scoped_symtable;
    NecroPrimTypes*       prim_types;
    NecroInfer*           infer;

    // Cached data
    size_t                gen_vars;
    NecroMachineType*     necro_uint_type;
    NecroMachineType*     necro_int_type;
    NecroMachineType*     necro_float_type;
    NecroMachineType*     necro_poly_type;
    NecroMachineType*     necro_poly_ptr_type;
    NecroMachineType*     necro_data_type;
    NecroMachineType*     world_type;
    NecroMachineType*     the_world_type;
    NecroMachineAST*      mkIntFnValue;
    NecroMachineAST*      mkFloatFnValue;
    NecroSymbol           main_symbol;
    NecroMachineAST*      program_main;
    NecroMachineRuntime   runtime;
    NecroMachineCopyTable copy_table;
} NecroMachineProgram;

///////////////////////////////////////////////////////
// Core to Machine API
///////////////////////////////////////////////////////
NecroMachineProgram necro_core_to_machine(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer);
void                necro_destroy_machine_program(NecroMachineProgram* program);
void                necro_core_to_machine_1_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
void                necro_core_to_machine_2_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
NecroMachineAST*    necro_core_to_machine_3_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
NECRO_WORD_SIZE     necro_get_word_size();

#endif // NECRO_MACHINE_H
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
#include "core/closure_conversion.h"
#include "machine_closure.h"

///////////////////////////////////////////////////////
// NecroMachine
//-----------
// * Abstract machine target for necrolang
// * LLVM-IR like, but simpler and necrolang specific
// * Infinite registers
// * Persistent data defined by a graph of statically calculated blocks containing blocks
// * Machines retain state across multiple calls to main
///////////////////////////////////////////////////////

/*
    TODO:
        * closures
        * necro_verify_machine_program

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
    struct NecroMachineAST* slot_ast;
    size_t                  data_id;
    bool                    is_dynamic;
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
        NECRO_TERM_RETURN_VOID,
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
    // Vars
    NecroVar                bind_name;
    NecroVar                machine_name;
    NecroVar                state_name;

    struct NecroMachineAST* mk_fn;
    struct NecroMachineAST* init_fn;
    struct NecroMachineAST* update_fn;
    NecroMachineBlock*      update_error_block;
    NECRO_STATE_TYPE        state_type;
    struct NecroMachineAST* outer;
    NecroType*              necro_value_type;
    NecroMachineType*       value_type;
    NecroMachineType*       fn_type;
    bool                    is_pushed;
    bool                    is_recursive;
    bool                    is_persistent_slot_set;
    // NECRO_STATE_TYPE        most_stateful_type_referenced;

    // args
    NecroVar*               arg_names;
    NecroType**             arg_types;
    size_t                  num_arg_names;

    // members
    NecroSlot*              members;
    size_t                  num_members;
    size_t                  members_size;
    int32_t                 _first_dynamic;

    // cache if and where slots have been loaded!?
    struct NecroMachineAST* global_value; // If global
    struct NecroMachineAST* global_state; // If global

    // data ids for GC
    size_t                  data_id;
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
    NECRO_STATE_TYPE        state_type;
    // bool                    is_primitively_stateful;
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
    NECRO_MACHINE_CALL_TYPE  call_type;
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

typedef struct NecroMachineGetElementPtr
{
    struct NecroMachineAST*  source_value;
    struct NecroMachineAST** indices;
    // uint32_t*            indices;
    size_t                   num_indices;
    struct NecroMachineAST*  dest_value;
} NecroMachineGetElementPtr;

typedef struct NecroMachineBitCast
{
    struct NecroMachineAST* from_value;
    struct NecroMachineAST* to_value;
} NecroMachineBitCast;

typedef struct NecroMachineZExt
{
    struct NecroMachineAST* from_value;
    struct NecroMachineAST* to_value;
} NecroMachineZExt;

typedef struct NecroMachineNAlloc
{
    NecroMachineType*       type_to_alloc;
    uint32_t                slots_used;
    // bool                    is_constant;
    struct NecroMachineAST* result_reg;
} NecroMachineNAlloc;

typedef struct NecroMachineAlloca
{
    size_t                  num_slots;
    struct NecroMachineAST* result;
} NecroMachineAlloca;

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
    NECRO_MACHINE_BINOP_AND,
    NECRO_MACHINE_BINOP_OR,
    NECRO_MACHINE_BINOP_SHL,
    NECRO_MACHINE_BINOP_SHR,
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

typedef struct NecroMachineMemSet
{
    struct NecroMachineAST* ptr;
    struct NecroMachineAST* value;
    struct NecroMachineAST* num_bytes;
} NecroMachineMemSet;

typedef struct NecroMachineSelect
{
    struct NecroMachineAST* cmp_value;
    struct NecroMachineAST* left;
    struct NecroMachineAST* right;
    struct NecroMachineAST* result;
} NecroMachineSelect;

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
    NECRO_MACHINE_ZEXT,
    NECRO_MACHINE_GEP,
    NECRO_MACHINE_BINOP,
    NECRO_MACHINE_CMP,
    NECRO_MACHINE_PHI,
    NECRO_MACHINE_MEMCPY, // TODO: Maybe remove
    NECRO_MACHINE_MEMSET, // TODO: Maybe remove
    NECRO_MACHINE_ALLOCA, // TODO: Maybe remove
    NECRO_MACHINE_SELECT, // TODO: Maybe remove

    // Defs
    NECRO_MACHINE_STRUCT_DEF,
    NECRO_MACHINE_FN_DEF,
    NECRO_MACHINE_DEF,
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
        NecroMachineZExt          zext;
        NecroMachineNAlloc        nalloc;
        NecroMachineAlloca        alloca;
        NecroMachineBinOp         binop;
        NecroMachineCmp           cmp;
        NecroMachinePhi           phi;
        NecroMachineMemCpy        memcpy;
        NecroMachineMemSet        memset;
        NecroMachineSelect        select;
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
    NecroVar _necro_debug_print;
    NecroVar _necro_from_alloc;
    NecroVar _necro_to_alloc;
    // NecroVar _necro_const_alloc;
    NecroVar _necro_flip_const;
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
    NecroMachineAST*      world_value;
    NecroMachineAST*      mkIntFnValue;
    NecroMachineAST*      mkFloatFnValue;
    NecroSymbol           main_symbol;
    NecroMachineAST*      program_main;
    NecroMachineRuntime   runtime;
    NecroMachineCopyTable copy_table;
    NecroCon              dyn_state_con;
    NecroCon              null_con;

    // Closures
    NecroMachineType*      dyn_state_type;
    NecroCon               closure_con;
    NecroMachineType*      closure_type;
    NecroClosureConVector  closure_cons;
    NecroClosureTypeVector closure_types;
    NecroClosureDefVector  closure_defs;
    NecroMachineType*      apply_state_type;
    NecroApplyFnVector     apply_fns;
    NecroMachineAST*       get_apply_state_fn;
} NecroMachineProgram;

///////////////////////////////////////////////////////
// Core to Machine API
///////////////////////////////////////////////////////
NecroMachineProgram necro_core_to_machine(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer, NecroClosureDefVector closure_defs);
void                necro_destroy_machine_program(NecroMachineProgram* program);
void                necro_core_to_machine_1_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
void                necro_core_to_machine_2_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
NecroMachineAST*    necro_core_to_machine_3_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
NECRO_WORD_SIZE     necro_get_word_size();
void                necro_build_debug_print(NecroMachineProgram* program, NecroMachineAST* fn_def, int print_value);

#endif // NECRO_MACHINE_H
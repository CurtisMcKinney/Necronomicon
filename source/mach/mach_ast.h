/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACH_H
#define NECRO_MACH_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "type.h"
#include "utility/arena.h"
#include "utility/list.h"
#include "intern.h"
#include "symtable.h"
#include "base.h"
#include "mach_type.h"

///////////////////////////////////////////////////////
// NecroMach
//-----------
// * Abstract machine target for necrolang
// * LLVM-IR like, but simpler and necrolang specific
// * Infinite registers
// * Main is called in cycles
// * Uses large scale region based memory management
// * Machines retain state in these regions across multiple calls to main
///////////////////////////////////////////////////////

/*
    TODO / NOTES / Thoughts:
        * Region structs instead of completely inlined type data?
        * Then regions stack into eachother.
        * Type monomorphization / caching / uniquing.
*/


//--------------------
// Forward Declarations
//--------------------
struct NecroMachAst;
struct NecroMachLit;
struct NecroMachDef;
struct NecroMachType;


//--------------------
// NecroMachAstSymbol
//--------------------
typedef struct NecroMachAstSymbol
{
    NecroSymbol           name;
    struct NecroMachAst*  ast;
    struct NecroMachType* mach_type;
    NecroType*            necro_type;
    NECRO_STATE_TYPE      state_type;
    bool                  is_enum;
    bool                  is_constructor;
    bool                  con_num;
    bool                  is_primitive;
} NecroMachAstSymbol;


//--------------------
// Value
//--------------------
typedef enum
{
    NECRO_MACH_VALUE_VOID,
    NECRO_MACH_VALUE_REG,
    NECRO_MACH_VALUE_PARAM,
    NECRO_MACH_VALUE_GLOBAL,
    NECRO_MACH_VALUE_UINT1_LITERAL,
    NECRO_MACH_VALUE_UINT8_LITERAL,
    NECRO_MACH_VALUE_UINT16_LITERAL,
    NECRO_MACH_VALUE_UINT32_LITERAL,
    NECRO_MACH_VALUE_UINT64_LITERAL,
    NECRO_MACH_VALUE_INT32_LITERAL,
    NECRO_MACH_VALUE_INT64_LITERAL,
    NECRO_MACH_VALUE_F32_LITERAL,
    NECRO_MACH_VALUE_F64_LITERAL,
    NECRO_MACH_VALUE_NULL_PTR_LITERAL,
} NECRO_MACH_VALUE_TYPE;

typedef struct NecroMachParamReg
{
    NecroMachAstSymbol* fn_symbol;
    size_t              param_num;
} NecroMachParamReg;

typedef struct NecroMachValue
{
    union
    {
        NecroMachAstSymbol* global_symbol;
        NecroMachAstSymbol* reg_symbol;
        NecroMachParamReg   param_reg;
        bool                uint1_literal;
        uint8_t             uint8_literal;
        uint16_t            uint16_literal;
        uint32_t            uint32_literal;
        uint64_t            uint64_literal;
        int32_t             int32_literal;
        int64_t             int64_literal;
        float               f32_literal;
        double              f64_literal;
    };
    NECRO_MACH_VALUE_TYPE value_type;
} NecroMachValue;


//--------------------
// Slot
//--------------------
typedef struct NecroMachSlot
{
    size_t                slot_num;
    NECRO_STATE_TYPE      state_type;
    struct NecroMachType* necro_machine_type;
    struct NecroMachDef*  machine_def;
    struct NecroMachAst*  slot_ast;
} NecroSlot;


//--------------------
// Switch
//--------------------
typedef struct NecroMachSwitchData
{
    struct NecroMachAst* block;
    size_t               value;
} NecroMachSwitchData;
NECRO_DECLARE_ARENA_LIST(NecroMachSwitchData, MachSwitch, mach_switch);

typedef struct NecroMachSwitchTerminator
{
    NecroMachSwitchList*    values;
    struct NecroMachAst* else_block;
    struct NecroMachAst* choice_val;
} NecroMachSwitchTerminator;

typedef struct NecroMachReturnTerminator
{
    struct NecroMachAst* return_value;
} NecroMachReturnTerminator;

typedef struct NecroMachBreakTerminator
{
    struct NecroMachAst* block_to_jump_to;
} NecroMachBreakTerminator;

typedef struct NecroMachCondBreakTerminator
{
    struct NecroMachAst* true_block;
    struct NecroMachAst* false_block;
    struct NecroMachAst* cond_value;
} NecroMachCondBreakTerminator;

typedef struct NecroMachTerminator
{
    union
    {
        NecroMachSwitchTerminator    switch_terminator;
        NecroMachReturnTerminator    return_terminator;
        NecroMachBreakTerminator     break_terminator;
        NecroMachCondBreakTerminator cond_break_terminator;
    };
    enum
    {
        NECRO_MACH_TERM_SWITCH,
        NECRO_MACH_TERM_RETURN,
        NECRO_MACH_TERM_RETURN_VOID,
        NECRO_MACH_TERM_BREAK,
        NECRO_MACH_TERM_COND_BREAK,
        NECRO_MACH_TERM_UNREACHABLE,
    } type;
} NecroMachTerminator;


//--------------------
// Stuct
//--------------------
typedef struct NecroMachStructDef
{
    NecroMachAstSymbol* symbol;
} NecroMachStructDef;


//--------------------
// Block
//--------------------
typedef struct NecroMachBlock
{
    NecroMachAstSymbol*   symbol;
    struct NecroMachAst** statements;
    size_t                num_statements;
    size_t                statements_size;
    NecroMachTerminator*  terminator;
    struct NecroMachAst*  next_block;
} NecroMachBlock;


//--------------------
// Machine
//--------------------
typedef enum
{
    NECRO_MACH_GLOBAL,
    NECRO_MACH_LOCAL,
    NECRO_MACH_FN,
} NECRO_MACH_DEF_TYPE;

typedef struct NecroMachDef
{
    NecroMachAstSymbol*   symbol;
    NecroMachAstSymbol*   machine_name;
    NecroMachAstSymbol*   state_name;

    struct NecroMachAst*  mk_fn;
    struct NecroMachAst*  init_fn;
    struct NecroMachAst*  update_fn;
    NecroMachBlock*       update_error_block;
    NECRO_STATE_TYPE      state_type;
    struct NecroMachAst*  outer;
    NecroType*            necro_value_type;
    struct NecroMachType* value_type;
    struct NecroMachType* fn_type;
    bool                  is_persistent_slot_set;

    // args
    NecroMachAstSymbol**  arg_names;
    size_t                num_arg_names;

    // Region members
    NecroSlot*            members;
    size_t                num_members;
    size_t                members_size;

    // cache if and where slots have been loaded!?
    struct NecroMachAst*  global_value; // If global
    struct NecroMachAst*  global_state; // If global

    NECRO_MACH_DEF_TYPE   def_type;
} NecroMachDef;


//--------------------
// Fn
//--------------------
typedef enum
{
    NECRO_MACH_FN_FN,
    NECRO_MACH_FN_RUNTIME,
} NECRO_MACH_FN_TYPE;

typedef void* NecroMachFnPtr;

typedef struct NecroMachFnDef
{
    NecroMachAstSymbol*  symbol;
    struct NecroMachAst* call_body;
    NECRO_MACH_FN_TYPE   fn_type;
    struct NecroMachAst* fn_value;
    NecroMachFnPtr       runtime_fn_addr;
    NECRO_STATE_TYPE     state_type;
    //-------------------
    // compile time data
    struct NecroMachAst* _curr_block;
    struct NecroMachAst* _init_block;
    struct NecroMachAst* _cont_block;
    struct NecroMachAst* _err_block;
} NecroMachFnDef;

typedef struct NecroMachCall
{
    struct NecroMachAst*  fn_value;
    struct NecroMachAst** parameters;
    size_t                num_parameters;
    struct NecroMachAst*  result_reg;
    NECRO_MACH_CALL_TYPE  call_type;
} NecroMachCall;


//--------------------
// Load / Store
//--------------------
typedef struct NecroMachLoadSlot
{
    struct NecroMachAst* source_ptr;
    size_t               source_slot;
} NecroMachLoadSlot;

typedef enum
{
    NECRO_MACH_LOAD_TAG,
    NECRO_MACH_LOAD_PTR,
    NECRO_MACH_LOAD_SLOT,
} NECRO_MACH_LOAD_TYPE;

typedef struct NecroMachLoad
{
    union
    {
        struct NecroMachAst* source_ptr;
        NecroMachLoadSlot    load_slot;
    };
    NECRO_MACH_LOAD_TYPE load_type;
    struct NecroMachAst* dest_value;
} NecroMachLoad;

typedef struct NecroMachStoreSlot
{
    struct NecroMachAst* dest_ptr;
    size_t               dest_slot;
} NecroMachStoreSlot;

typedef enum
{
    NECRO_MACH_STORE_TAG,
    NECRO_MACH_STORE_PTR,
    NECRO_MACH_STORE_SLOT,
} NECRO_MACH_STORE_TYPE;

typedef struct NecroMachStore
{
    struct NecroMachAst* source_value;
    union
    {
        struct NecroMachAst* dest_ptr;
        NecroMachStoreSlot   store_slot;
    };
    NECRO_MACH_STORE_TYPE store_type;
} NecroMachStore;


//--------------------
// Constant
//--------------------
typedef enum
{
    NECRO_MACH_CONSTANT_INT64,
    NECRO_MACH_CONSTANT_F64,
    NECRO_MACH_CONSTANT_CHAR,
    NECRO_MACH_CONSTANT_STRUCT
} NECRO_MACH_CONSTANT_TYPE;

typedef struct NecroMachConstantDef
{
    union
    {
        int64_t             constant_i64;
        double              constant_double;
        char                constant_char;
        NecroMachAstSymbol* constant_struct_symbol;
    };
    NECRO_MACH_CONSTANT_TYPE constant_type;
} NecroMachConstantDef;

//--------------------
// Ops
//--------------------
typedef struct NecroMachGetElementPtr
{
    struct NecroMachAst*  source_value;
    struct NecroMachAst** indices;
    size_t                num_indices;
    struct NecroMachAst*  dest_value;
} NecroMachGetElementPtr;

typedef struct NecroMachBitCast
{
    struct NecroMachAst* from_value;
    struct NecroMachAst* to_value;
} NecroMachBitCast;

typedef struct NecroMachZExt
{
    struct NecroMachAst* from_value;
    struct NecroMachAst* to_value;
} NecroMachZExt;

typedef struct NecroMachNAlloc
{
    struct NecroMachType* type_to_alloc;
    struct NecroMachAst*  result_reg;
    size_t                slots_used;
} NecroMachNAlloc;

typedef struct NecroMachAlloca
{
    size_t               num_slots;
    struct NecroMachAst* result;
} NecroMachAlloca;

typedef enum
{
    NECRO_MACH_BINOP_IADD,
    NECRO_MACH_BINOP_ISUB,
    NECRO_MACH_BINOP_IMUL,
    NECRO_MACH_BINOP_IDIV,

    NECRO_MACH_BINOP_UADD,
    NECRO_MACH_BINOP_USUB,
    NECRO_MACH_BINOP_UMUL,
    NECRO_MACH_BINOP_UDIV,

    NECRO_MACH_BINOP_FADD,
    NECRO_MACH_BINOP_FSUB,
    NECRO_MACH_BINOP_FMUL,
    NECRO_MACH_BINOP_FDIV,
    NECRO_MACH_BINOP_AND,
    NECRO_MACH_BINOP_OR,
    NECRO_MACH_BINOP_SHL,
    NECRO_MACH_BINOP_SHR,
} NECRO_MACH_BINOP_TYPE;

typedef struct NecroMachBinOp
{
    struct NecroMachAst*  left;
    struct NecroMachAst*  right;
    struct NecroMachAst*  result;
    NECRO_MACH_BINOP_TYPE binop_type;
} NecroMachBinOp;

typedef enum
{
    NECRO_MACH_UOP_IABS,
    NECRO_MACH_UOP_UABS,
    NECRO_MACH_UOP_FABS,

    NECRO_MACH_UOP_ISGN,
    NECRO_MACH_UOP_USGN,
    NECRO_MACH_UOP_FSGN,

    NECRO_MACH_UOP_ITOI,
    NECRO_MACH_UOP_ITOU,
    NECRO_MACH_UOP_ITOF,

    NECRO_MACH_UOP_UTOI,

    NECRO_MACH_UOP_FTRI,
    NECRO_MACH_UOP_FRNI,
    NECRO_MACH_UOP_FTOF,

} NECRO_MACH_UOP_TYPE;

typedef struct NecroMachUOp
{
    struct NecroMachAst* param;
    struct NecroMachAst* result;
    NECRO_MACH_UOP_TYPE  uop_type;
} NecroMachUOp;

typedef enum
{
    NECRO_MACH_CMP_EQ,
    NECRO_MACH_CMP_NE,
    NECRO_MACH_CMP_GT,
    NECRO_MACH_CMP_GE,
    NECRO_MACH_CMP_LT,
    NECRO_MACH_CMP_LE,
} NECRO_MACH_CMP_TYPE;

typedef struct NecroMachCmp
{
    struct NecroMachAst* left;
    struct NecroMachAst* right;
    struct NecroMachAst* result;
    NECRO_MACH_CMP_TYPE  cmp_type;
} NecroMachCmp;


typedef struct NecroMachMemCpy
{
    struct NecroMachAst* dest;
    struct NecroMachAst* source;
    struct NecroMachAst* num_bytes;
} NecroMachMemCpy;

typedef struct NecroMachMemSet
{
    struct NecroMachAst* ptr;
    struct NecroMachAst* value;
    struct NecroMachAst* num_bytes;
} NecroMachMemSet;

typedef struct NecroMachSelect
{
    struct NecroMachAst* cmp_value;
    struct NecroMachAst* left;
    struct NecroMachAst* right;
    struct NecroMachAst* result;
} NecroMachSelect;

//--------------------
// Phi
//--------------------
typedef struct NecroMachPhiData
{
    struct NecroMachAst* block;
    struct NecroMachAst* value;
} NecroMachPhiData;

NECRO_DECLARE_ARENA_LIST(NecroMachPhiData, MachPhi, mach_phi);

typedef struct NecroMachPhi
{
    NecroMachPhiList*     values;
    struct NecroMachAst*  result;
} NecroMachPhi;


//--------------------
// NecroMachAst
//--------------------
typedef enum
{
    NECRO_MACH_VALUE,
    NECRO_MACH_BLOCK,

    // ops
    NECRO_MACH_CALL,
    NECRO_MACH_LOAD,
    NECRO_MACH_STORE,
    NECRO_MACH_NALLOC,   // TODO: Replace with system for locating slot within region?
    NECRO_MACH_BIT_CAST, // TODO / NOTE: Hopefully we can avoid this...
    NECRO_MACH_ZEXT,
    NECRO_MACH_GEP,
    NECRO_MACH_BINOP,
    NECRO_MACH_UOP,
    NECRO_MACH_CMP,
    NECRO_MACH_PHI,
    NECRO_MACH_MEMCPY, // TODO: Maybe remove
    NECRO_MACH_MEMSET, // TODO: Maybe remove
    // NECRO_MACH_ALLOCA, // TODO: Maybe remove
    // NECRO_MACH_SELECT, // TODO: Maybe remove

    // Defs
    NECRO_MACH_STRUCT_DEF,
    NECRO_MACH_FN_DEF,
    NECRO_MACH_DEF,
} NECRO_MACH_AST_TYPE;

typedef struct NecroMachAst
{
    union
    {
        NecroMachValue         value;
        NecroMachCall          call;
        NecroMachLoad          load;
        NecroMachStore         store;
        NecroMachBlock         block;
        NecroMachDef           machine_def;
        NecroMachFnDef         fn_def;
        NecroMachStructDef     struct_def;
        NecroMachConstantDef   constant;
        NecroMachGetElementPtr gep;
        NecroMachBitCast       bit_cast;
        NecroMachZExt          zext;
        NecroMachNAlloc        nalloc;
        NecroMachAlloca        alloca;
        NecroMachBinOp         binop;
        NecroMachUOp           uop;
        NecroMachCmp           cmp;
        NecroMachPhi           phi;
        NecroMachMemCpy        memcpy;
        NecroMachMemSet        memset;
        NecroMachSelect        select;
    };
    NECRO_MACH_AST_TYPE   type;
    struct NecroMachType* necro_machine_type;
} NecroMachAst;


//--------------------
// Runtime / Program
//--------------------
NECRO_DECLARE_VECTOR(NecroMachAst*, NecroMachAst, necro_mach_ast);
typedef struct NecroMachRuntime
{
    NecroMachAstSymbol* _necro_init_runtime;
    NecroMachAstSymbol* _necro_update_runtime;
    NecroMachAstSymbol* _necro_error_exit;
    NecroMachAstSymbol* _necro_sleep;
    NecroMachAstSymbol* _necro_print;
    NecroMachAstSymbol* _necro_debug_print;
    NecroMachAstSymbol* _necro_alloc_region;
    NecroMachAstSymbol* _necro_free_region;
} NecroMachRuntime;

typedef enum
{
    NECRO_WORD_4_BYTES = 4, // 32-bit
    NECRO_WORD_8_BYTES = 8, // 64-bit
    NECRO_WORD_INVALID = -1
} NECRO_WORD_SIZE;

typedef struct NecroMachProgram
{
    // Program info
    NecroMachAstVector    structs;
    NecroMachAstVector    functions;
    NecroMachAstVector    machine_defs;
    NecroMachAstVector    globals;
    NecroMachAst*         necro_main;
    NECRO_WORD_SIZE       word_size;

    // Useful structs
    NecroPagedArena       arena;
    NecroSnapshotArena    snapshot_arena;
    NecroBase*            base;
    NecroIntern*          intern;

    // Cached data
    size_t                gen_vars;
    struct NecroMachType* necro_uint_type;
    struct NecroMachType* necro_int_type;
    struct NecroMachType* necro_float_type;
    struct NecroMachType* necro_data_type;
    NecroMachAst*         mkIntFnValue;
    NecroMachAst*         mkFloatFnValue;
    NecroMachAstSymbol*   main_symbol;
    NecroMachAst*         program_main;
    NecroMachRuntime      runtime;
    NecroMachAstSymbol*   null_con;
    size_t                clash_suffix;
} NecroMachProgram;

///////////////////////////////////////////////////////
// Ast Creation
///////////////////////////////////////////////////////

//--------------------
// Utility
//--------------------
NecroMachAstSymbol* necro_mach_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name);
NecroMachAstSymbol* necro_mach_ast_symbol_create_from_core_ast_symbol(NecroPagedArena* arena, NecroCoreAstSymbol* core_ast_symbol);
NecroMachAstSymbol* necro_mach_ast_symbol_gen(NecroMachProgram* program, NecroMachAst* ast, const char* str, NECRO_MANGLE_TYPE mangle_type);

//--------------------
// Values
//--------------------
NecroMachAst* necro_mach_value_create(NecroMachProgram* program, NecroMachValue value, struct NecroMachType* necro_machine_type);
NecroMachAst* necro_mach_value_create_reg(NecroMachProgram* program, struct NecroMachType* necro_machine_type, const char* reg_name);
NecroMachAst* necro_mach_value_create_global(NecroMachProgram* program, NecroMachAstSymbol* symbol, struct NecroMachType* necro_machine_type);
NecroMachAst* necro_mach_value_create_param_reg(NecroMachProgram* program, NecroMachAst* fn_def, size_t param_num);
NecroMachAst* necro_mach_value_create_uint1(NecroMachProgram* program, bool uint1_literal);
NecroMachAst* necro_mach_value_create_uint8(NecroMachProgram* program, uint8_t uint8_literal);
NecroMachAst* necro_mach_value_create_uint16(NecroMachProgram* program, uint16_t uint16_literal);
NecroMachAst* necro_mach_value_create_uint32(NecroMachProgram* program, uint32_t uint32_literal);
NecroMachAst* necro_mach_value_create_uint64(NecroMachProgram* program, uint64_t uint64_literal);
NecroMachAst* necro_mach_value_create_i32(NecroMachProgram* program, int32_t int32_literal);
NecroMachAst* necro_mach_value_create_i64(NecroMachProgram* program, int64_t int64_literal);
NecroMachAst* necro_mach_value_create_f32(NecroMachProgram* program, float f32_literal);
NecroMachAst* necro_mach_value_create_f64(NecroMachProgram* program, double f64_literal);
NecroMachAst* necro_mach_value_create_null(NecroMachProgram* program, struct NecroMachType* ptr_type);
NecroMachAst* necro_mach_value_create_word_uint(NecroMachProgram* program, uint64_t int_literal);
NecroMachAst* necro_mach_value_create_word_int(NecroMachProgram* program, int64_t int_literal);
NecroMachAst* necro_mach_value_create_word_float(NecroMachProgram* program, double float_literal);

//--------------------
// Blocks
//--------------------
NecroMachAst* necro_mach_block_create(NecroMachProgram* program, const char* name, NecroMachAst* next_block);
void          necro_mach_block_add_statement(NecroMachProgram* program, NecroMachAst* block, NecroMachAst* statement);
NecroMachAst* necro_mach_block_append(NecroMachProgram* program, NecroMachAst* fn_def, const char* block_name);
NecroMachAst* necro_mach_block_insert_before(NecroMachProgram* program, NecroMachAst* fn_def, const char* block_name, NecroMachAst* block_to_precede);
void          necro_mach_block_move_to(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* block);

//--------------------
// Memory
//--------------------
// NecroMachAst* necro_mach_build_nalloc(NecroMachProgram* program, NecroMachAst* fn_def, struct NecroMachType* type, uint32_t a_slots_used);
// NecroMachAst* necro_mach_build_alloca(NecroMachProgram* program, NecroMachAst* fn_def, size_t num_slots);
NecroMachAst* necro_mach_build_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name);
NecroMachAst* necro_mach_build_non_const_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, NecroMachAst** a_indices, size_t num_indices, const char* dest_name, struct NecroMachType* result_type);
NecroMachAst* necro_mach_build_bit_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
NecroMachAst* necro_mach_build_zext(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
void          necro_mach_build_memcpy(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* dest, NecroMachAst* source, NecroMachAst* num_bytes);
void          necro_mach_build_memset(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* ptr, NecroMachAst* value, NecroMachAst* num_bytes);

//--------------------
// Load / Store
//--------------------
void          necro_mach_build_store(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, NecroMachAst* dest_ptr);
NecroMachAst* necro_mach_build_load(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_ptr_ast, const char* dest_name);

//--------------------
// Functions
//--------------------
NecroMachAst* necro_mach_build_call(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* fn_value_ast, NecroMachAst** a_parameters, size_t num_parameters, NECRO_MACH_CALL_TYPE call_type, const char* dest_name);
NecroMachAst* necro_mach_build_binop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* left, NecroMachAst* right, NECRO_MACH_BINOP_TYPE op_type);
NecroMachAst* necro_mach_build_uop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* param, NECRO_MACH_UOP_TYPE op_type);

//--------------------
// Branching
//--------------------
void          necro_mach_build_return_void(NecroMachProgram* program, NecroMachAst* fn_def);
void          necro_mach_build_return(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* return_value);
void          necro_mach_build_break(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* block_to_jump_to);
void          necro_mach_build_cond_break(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* cond, NecroMachAst* true_block, NecroMachAst* false_block);
NecroMachAst* necro_mach_build_cmp(NecroMachProgram* program, NecroMachAst* fn_def, NECRO_MACH_CMP_TYPE cmp_type, NecroMachAst* left, NecroMachAst* right);
NecroMachAst* necro_mach_build_phi(NecroMachProgram* program, NecroMachAst* fn_def, struct NecroMachType* type, NecroMachPhiList* values);
void          necro_mach_add_incoming_to_phi(NecroMachProgram* program, NecroMachAst* phi, NecroMachAst* block, NecroMachAst* value);
void          necro_mach_build_unreachable(NecroMachProgram* program, NecroMachAst* fn_def);
void          necro_mach_add_case_to_switch(NecroMachProgram* program, NecroMachSwitchTerminator* switch_term, NecroMachAst* block, size_t value);
NecroMachSwitchTerminator* necro_mach_build_switch(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* choice_val, NecroMachSwitchList* values, NecroMachAst* else_block);
// NecroMachAst* necro_build_select(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* cmp_value, NecroMachAst* left, NecroMachAst* right);

//--------------------
// Defs
//--------------------
NecroMachAst* necro_mach_create_struct_def(NecroMachProgram* program, NecroMachAstSymbol* symbol, struct NecroMachType** members, size_t num_members);
NecroMachAst* necro_mach_create_fn(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachAst* call_body, struct NecroMachType* necro_machine_type);
NecroMachAst* necro_mach_create_runtime_fn(NecroMachProgram* program, NecroMachAstSymbol* symbol, struct NecroMachType* necro_machine_type, NecroMachFnPtr runtime_fn_addr, NECRO_STATE_TYPE state_type);
NecroMachAst* necro_mach_create_initial_machine_def(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachAst* outer, struct NecroMachType* value_type, NecroType* necro_value_type);

//--------------------
// Program
//--------------------

NecroMachProgram necro_mach_program_empty();
NecroMachProgram necro_mach_program_create(NecroIntern* intern, NecroBase* base);
void             necro_mach_program_destroy(NecroMachProgram* program);

void             necro_mach_program_add_struct(NecroMachProgram* program, NecroMachAst* struct_ast);
void             necro_mach_program_add_function(NecroMachProgram* program, NecroMachAst* function);
void             necro_mach_program_add_machine_def(NecroMachProgram* program, NecroMachAst* machine_def);
void             necro_mach_program_add_global(NecroMachProgram* program, NecroMachAst* global);

#endif // NECRO_MACH_H

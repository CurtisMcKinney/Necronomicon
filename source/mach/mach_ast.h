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
typedef struct
{
    NecroSymbol          name;
    struct NecroMachAst* ast;
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
typedef struct NecroMachDef
{
    NecroMachAstSymbol* symbol;
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

typedef enum
{
    NECRO_MACH_CALL_LANG,
    NECRO_MACH_CALL_C
} NECRO_MACH_CALL_TYPE;

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
    struct NecroMachAST*  source_value;
    struct NecroMachAST** indices;
    size_t                num_indices;
    struct NecroMachAST*  dest_value;
} NecroMachGetElementPtr;

typedef struct NecroMachBitCast
{
    struct NecroMachAST* from_value;
    struct NecroMachAST* to_value;
} NecroMachBitCast;

typedef struct NecroMachZExt
{
    struct NecroMachAST* from_value;
    struct NecroMachAST* to_value;
} NecroMachZExt;

typedef struct NecroMachNAlloc
{
    struct NecroMachType* type_to_alloc;
    struct NecroMachAST*  result_reg;
    size_t                slots_used;
} NecroMachNAlloc;

typedef struct NecroMachAlloca
{
    size_t               num_slots;
    struct NecroMachAST* result;
} NecroMachAlloca;

typedef enum
{
    NECRO_MACH_BINOP_IADD,
    NECRO_MACH_BINOP_ISUB,
    NECRO_MACH_BINOP_IMUL,
    NECRO_MACH_BINOP_IDIV,
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
    struct NecroMachAST*  left;
    struct NecroMachAST*  right;
    struct NecroMachAST*  result;
    NECRO_MACH_BINOP_TYPE binop_type;
} NecroMachBinOp;

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
    struct NecroMachAST* left;
    struct NecroMachAST* right;
    struct NecroMachAST* result;
    NECRO_MACH_CMP_TYPE  cmp_type;
} NecroMachCmp;


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
    NECRO_MACH_CMP,
    NECRO_MACH_PHI,
    NECRO_MACH_MEMCPY, // TODO: Maybe remove
    NECRO_MACH_MEMSET, // TODO: Maybe remove
    NECRO_MACH_ALLOCA, // TODO: Maybe remove
    NECRO_MACH_SELECT, // TODO: Maybe remove

    // Defs
    NECRO_MACH_STRUCT_DEF,
    NECRO_MACH_FN_DEF,
    NECRO_MACH_DEF,
} NECRO_MACH_AST_TYPE;


#endif // NECRO_MACH_H
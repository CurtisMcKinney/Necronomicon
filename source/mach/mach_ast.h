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

//--------------------
// Forward Declarations
//--------------------
struct NecroMachAst;
struct NecroMachLit;
struct NecroMachDef;
struct NecroMachType;

//--------------------
// Slot
//--------------------
typedef struct NecroMachSlot
{
    size_t                slot_num;
    struct NecroMachType* necro_machine_type;
    struct NecroMachDef*  machine_def;
    struct NecroMachAst*  slot_ast;
    struct NecroMachAst*  const_init_value;
} NecroMachSlot;

//--------------------
// NecroMachAstSymbol
//--------------------
struct NecroLLVMSymbol; // TODO: Place holder. Replace with void* for better flexibility after testing is done
typedef struct NecroMachAstSymbol
{
    NecroSymbol             name;
    struct NecroMachAst*    ast;
    struct NecroMachType*   mach_type;
    NecroType*              necro_type;
    struct NecroLLVMSymbol* codegen_symbol;
    NecroSymbol             global_string_symbol;
    NECRO_STATE_TYPE        state_type;
    NECRO_PRIMOP_TYPE       primop_type;
    size_t                  con_num;
    bool                    is_enum;
    bool                    is_constructor;
    bool                    is_primitive;
    bool                    is_unboxed;
    bool                    is_deep_copy_fn;
} NecroMachAstSymbol;

//--------------------
// Value
//--------------------
typedef enum
{
    NECRO_MACH_VALUE_VOID,
    NECRO_MACH_VALUE_UNDEFINED,
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
    NecroMachSwitchList* values;
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
    NecroMachAstSymbol*   symbol;
    NecroMachAstSymbol*   machine_name;
    NecroMachAstSymbol*   state_name;

    struct NecroMachAst*  mk_fn;
    struct NecroMachAst*  init_fn;
    struct NecroMachAst*  update_fn;
    struct NecroMachAst*  update_error_tag_phi;
    struct NecroMachAst*  update_error_tag_value;
    NECRO_STATE_TYPE      state_type;
    struct NecroMachAst*  outer;
    NecroType*            necro_value_type;
    struct NecroMachType* value_type;
    struct NecroMachType* fn_type;

    // args
    NecroMachAstSymbol**  arg_names;
    size_t                num_arg_names;

    // State members
    NecroMachSlot*        members;
    size_t                num_members;
    size_t                members_size;

    struct NecroMachAst*  global_value;
    struct NecroMachAst*  global_state;

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
    struct NecroMachAst* state_ptr;
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

typedef struct NecroMachCallIntrinsic
{
    NECRO_PRIMOP_TYPE     intrinsic;
    NecroMachType*        intrinsic_type;
    struct NecroMachAst** parameters;
    size_t                num_parameters;
    struct NecroMachAst*  result_reg;
} NecroMachCallIntrinsic;


//--------------------
// Load / Store
//--------------------
typedef struct NecroMachLoad
{
    struct NecroMachAst* source_ptr;
    struct NecroMachAst* dest_value;
} NecroMachLoad;

typedef struct NecroMachStore
{
    struct NecroMachAst* source_value;
    struct NecroMachAst* dest_ptr;
} NecroMachStore;


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

typedef struct NecroMachInsertValue
{
    struct NecroMachAst*  aggregate_value;
    struct NecroMachAst*  inserted_value;
    size_t                index;
    struct NecroMachAst*  dest_value;
} NecroMachInsertValue;

typedef struct NecroMachExtractValue
{
    struct NecroMachAst*  aggregate_value;
    size_t                index;
    struct NecroMachAst*  dest_value;
} NecroMachExtractValue;

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

typedef struct NecroMachSizeOf
{
    struct NecroMachType* type_to_get_size_of;
    struct NecroMachAst*  result_reg;
} NecroMachSizeOf;

typedef struct NecroMachAlloca
{
    size_t               num_slots;
    struct NecroMachAst* result;
} NecroMachAlloca;

typedef struct NecroMachBinOp
{
    struct NecroMachAst* left;
    struct NecroMachAst* right;
    struct NecroMachAst* result;
    NECRO_PRIMOP_TYPE    binop_type;
} NecroMachBinOp;

typedef struct NecroMachUOp
{
    struct NecroMachAst* param;
    struct NecroMachAst* result;
    NECRO_PRIMOP_TYPE   uop_type;
} NecroMachUOp;

typedef struct NecroMachCmp
{
    struct NecroMachAst* left;
    struct NecroMachAst* right;
    struct NecroMachAst* result;
    NECRO_PRIMOP_TYPE    cmp_type;
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
    NECRO_MACH_CALLI,
    NECRO_MACH_LOAD,
    NECRO_MACH_STORE,
    NECRO_MACH_BIT_CAST,
    NECRO_MACH_ZEXT,
    NECRO_MACH_GEP,
    NECRO_MACH_INSERT_VALUE,
    NECRO_MACH_EXTRACT_VALUE,
    NECRO_MACH_UOP,
    NECRO_MACH_BINOP,
    NECRO_MACH_CMP,
    NECRO_MACH_PHI,
    NECRO_MACH_SIZE_OF,

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
        NecroMachCallIntrinsic call_intrinsic;
        NecroMachLoad          load;
        NecroMachStore         store;
        NecroMachBlock         block;
        NecroMachDef           machine_def;
        NecroMachFnDef         fn_def;
        NecroMachStructDef     struct_def;
        NecroMachGetElementPtr gep;
        NecroMachInsertValue   insert_value;
        NecroMachExtractValue  extract_value;
        NecroMachBitCast       bit_cast;
        NecroMachZExt          zext;
        NecroMachAlloca        alloca;
        NecroMachBinOp         binop;
        NecroMachUOp           uop;
        NecroMachCmp           cmp;
        NecroMachPhi           phi;
        NecroMachSelect        select;
        NecroMachSizeOf        size_of;
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
    NecroMachAstSymbol* necro_init_runtime;
    NecroMachAstSymbol* necro_update_runtime;
    NecroMachAstSymbol* necro_error_exit;
    NecroMachAstSymbol* necro_inexhaustive_case_exit;
    NecroMachAstSymbol* necro_sleep;
    NecroMachAstSymbol* necro_print;
    NecroMachAstSymbol* necro_debug_print;
    NecroMachAstSymbol* necro_print_int;
    NecroMachAstSymbol* necro_print_i64;
    NecroMachAstSymbol* necro_print_char;
    NecroMachAstSymbol* necro_runtime_get_mouse_x;
    NecroMachAstSymbol* necro_runtime_get_mouse_y;
    NecroMachAstSymbol* necro_runtime_is_done;
    NecroMachAstSymbol* necro_runtime_alloc;
    NecroMachAstSymbol* necro_runtime_realloc;
    NecroMachAstSymbol* necro_runtime_free;
    NecroMachAstSymbol* necro_runtime_out_audio_block;
    // NecroMachAstSymbol* necro_runtime_print_audio_block;
    NecroMachAstSymbol* necro_runtime_test_assertion;
    NecroMachAstSymbol* necro_runtime_panic;
    NecroMachAstSymbol* necro_runtime_print_string;
    // NOTE: Don't forget to add mappings to mach_ast.c and codegen_llvm.c when you add new runtime symbols
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
    NecroMachAstVector      structs;
    NecroMachAstVector      functions;
    NecroMachAstVector      machine_defs;
    NecroMachAstVector      globals;
    NecroMachAst*           necro_init;
    NecroMachAst*           necro_main;
    NecroMachAst*           necro_shutdown;
    NECRO_WORD_SIZE         word_size;

    // Useful structs
    NecroPagedArena         arena;
    NecroSnapshotArena      snapshot_arena;
    NecroBase*              base;
    NecroIntern*            intern;

    // Cached data
    NecroMachTypeCache      type_cache;
    NecroMachAst*           program_main;
    size_t                  clash_suffix;
    NecroMachRuntime        runtime;
    NecroMachAstSymbol*     current_binding;
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
// NecroMachAst* necro_mach_value_create_tuple(NecroMachProgram* program, NecroMachAst** values, size_t num_values);
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
NecroMachAst* necro_mach_value_create_undefined(NecroMachProgram* program, struct NecroMachType* necro_machine_type);

//--------------------
// Blocks
//--------------------
NecroMachAst* necro_mach_block_create(NecroMachProgram* program, const char* name, NecroMachAst* next_block);
void          necro_mach_block_add_statement(NecroMachProgram* program, NecroMachAst* block, NecroMachAst* statement);
NecroMachAst* necro_mach_block_append(NecroMachProgram* program, NecroMachAst* fn_def, const char* block_name);
NecroMachAst* necro_mach_block_insert_before(NecroMachProgram* program, NecroMachAst* fn_def, const char* block_name, NecroMachAst* block_to_precede);
void          necro_mach_block_move_to(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* block);
NecroMachAst* necro_mach_block_get_current(NecroMachAst* fn_def);

//--------------------
// Memory
//--------------------
NecroMachAst* necro_mach_build_nalloc(NecroMachProgram* program, NecroMachAst* fn_def, struct NecroMachType* type);
// NecroMachAst* necro_mach_build_alloca(NecroMachProgram* program, NecroMachAst* fn_def, size_t num_slots);
NecroMachAst* necro_mach_build_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, size_t* a_indices, size_t num_indices, const char* dest_name);
NecroMachAst* necro_mach_build_insert_value(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* aggregate_value, NecroMachAst* inserted_value, size_t index, const char* dest_name);
NecroMachAst* necro_mach_build_extract_value(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* aggregate_value, size_t index, const char* dest_name);
NecroMachAst* necro_mach_build_non_const_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, NecroMachAst** a_indices, size_t num_indices, const char* dest_name, struct NecroMachType* result_type);
NecroMachAst* necro_mach_build_bit_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
NecroMachAst* necro_mach_build_up_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
NecroMachAst* necro_mach_build_down_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
NecroMachAst* necro_mach_build_maybe_bit_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
NecroMachAst* necro_mach_build_zext(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, struct NecroMachType* to_type);
NecroMachAst* necro_mach_build_size_of(NecroMachProgram* program, NecroMachAst* fn_def, struct NecroMachType* type_to_get_size_of);

//--------------------
// Load / Store
//--------------------
void          necro_mach_build_store(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, NecroMachAst* dest_ptr);
NecroMachAst* necro_mach_build_load(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_ptr_ast, const char* dest_name);

//--------------------
// Functions
//--------------------
NecroMachAst* necro_mach_build_call(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* fn_value_ast, NecroMachAst** a_parameters, size_t num_parameters, NECRO_MACH_CALL_TYPE call_type, const char* dest_name);
NecroMachAst* necro_mach_build_call_intrinsic(NecroMachProgram* program, NecroMachAst* fn_def, NECRO_PRIMOP_TYPE intrinsic, NecroMachType* intrinsic_type, NecroMachAst** a_parameters, size_t num_parameters, const char* dest_name);
NecroMachAst* necro_mach_build_binop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* left, NecroMachAst* right, NECRO_PRIMOP_TYPE op_type);
NecroMachAst* necro_mach_build_uop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* param, NecroMachType* result_type, NECRO_PRIMOP_TYPE op_type);

//--------------------
// Branching
//--------------------
void                       necro_mach_build_return_void(NecroMachProgram* program, NecroMachAst* fn_def);
void                       necro_mach_build_return(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* return_value);
void                       necro_mach_build_break(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* block_to_jump_to);
void                       necro_mach_build_cond_break(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* cond, NecroMachAst* true_block, NecroMachAst* false_block);
NecroMachAst*              necro_mach_build_cmp(NecroMachProgram* program, NecroMachAst* fn_def, NECRO_PRIMOP_TYPE cmp_type, NecroMachAst* left, NecroMachAst* right);
NecroMachAst*              necro_mach_build_phi(NecroMachProgram* program, NecroMachAst* fn_def, struct NecroMachType* type, NecroMachPhiList* values);
void                       necro_mach_add_incoming_to_phi(NecroMachProgram* program, NecroMachAst* phi, NecroMachAst* block, NecroMachAst* value);
void                       necro_mach_build_unreachable(NecroMachProgram* program, NecroMachAst* fn_def);
void                       necro_mach_add_case_to_switch(NecroMachProgram* program, NecroMachSwitchTerminator* switch_term, NecroMachAst* block, size_t value);
NecroMachSwitchTerminator* necro_mach_build_switch(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* choice_val, NecroMachSwitchList* values, NecroMachAst* else_block);
// NecroMachAst* necro_build_select(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* cmp_value, NecroMachAst* left, NecroMachAst* right);

//--------------------
// Defs
//--------------------
NecroMachAst* necro_mach_create_struct_def(NecroMachProgram* program, NecroMachAstSymbol* symbol, struct NecroMachType** members, size_t num_members);
NecroMachAst* necro_mach_create_struct_def_with_sum_type(NecroMachProgram* program, NecroMachAstSymbol* symbol, struct NecroMachType** members, size_t num_members, NecroMachAstSymbol* sum_type_symbol);
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
NecroMachAst*    necro_mach_create_string_global_constant(NecroMachProgram* program, NecroSymbol string_symbol);

#endif // NECRO_MACH_H

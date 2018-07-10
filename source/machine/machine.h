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

///////////////////////////////////////////////////////
// Machine
//-----------
// * Abstract machine target for necrolang
// * LLVM-IR like, but simpler and necrolang specific
// * Infinite registers
// * Heap defined by a graph of machines containing machines
// * Machines retain state across multiple calls to main
///////////////////////////////////////////////////////

/*
    TODO:
        * runtime
        * Construct main function
        * null out and zero out memory when allocated with nalloc via memcpy NULL....should be faster than doing it in code!?
        * case
        * closures

    TEST:
        * init_and_load_dyn function (will require breakcond and other stuff to work)
        * dyn fn machines need NecroData?
*/

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
struct NecroMachineAST;
struct NecroMachineLit;

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
    NECRO_MACHINE_VALUE_REG,
    NECRO_MACHINE_VALUE_PARAM,
    NECRO_MACHINE_VALUE_GLOBAL,
    NECRO_MACHINE_VALUE_UINT1_LITERAL,
    NECRO_MACHINE_VALUE_UINT8_LITERAL,
    NECRO_MACHINE_VALUE_UINT16_LITERAL,
    NECRO_MACHINE_VALUE_UINT32_LITERAL,
    NECRO_MACHINE_VALUE_UINT64_LITERAL,
    NECRO_MACHINE_VALUE_INT64_LITERAL,
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
        int64_t  int64_literal;
        double   f64_literal;
        NecroVar global_name;
    };
    NECRO_MACHINE_VALUE_TYPE value_type;
} NecroMachineValue;

typedef struct NecroSlot
{
    size_t         slot_num;
    NecroMachineType* necro_machine_type;
} NecroSlot;

typedef struct NecroTerminator
{
    union
    {
        struct NecroSwitchTerminator
        {
            struct NecroMachineAST* case_blocks;
            size_t*              case_switch_values;
            size_t               num_cases;
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
    NecroVar             bind_name;
    NecroVar             machine_name;

    struct NecroMachineAST* mk_fn;
    struct NecroMachineAST* init_fn;
    struct NecroMachineAST* update_fn;
    NecroMachineBlock*      update_error_block;
    uint32_t             initial_tag;
    NECRO_STATE_TYPE     state_type;
    struct NecroMachineAST* outer;
    NecroMachineType*       value_type;
    NecroMachineType*       fn_type;
    bool                 default_mk;
    bool                 default_init;
    bool                 is_pushed;
    bool                 is_recursive;
    bool                 is_persistent_slot_set;

    // args
    NecroVar*            arg_names;
    NecroType**          arg_types;
    size_t               num_arg_names;

    // members
    NecroSlot*           members;
    size_t               num_members;
    size_t               members_size;

    struct NecroMachineAST* global_value; // If global
    // cache if and where slots have been loaded!?
} NecroMachineDef;

typedef enum
{
    NECRO_FN_FN,
    NECRO_FN_RUNTIME,
    // NECRO_FN_PRIM_OP_CMPI,
    // NECRO_FN_PRIM_OP_ZEXT,
    // NECRO_FN_PRIM_OP_BIT_OR,
    // NECRO_FN_PRIM_OP_BIT_AND,
} NECRO_FN_TYPE;

typedef struct NecroMachineFnDef
{
    NecroVar             name;
    struct NecroMachineAST* call_body;
    NECRO_FN_TYPE        fn_type;
    struct NecroMachineAST* fn_value;
    //-------------------
    // compile time data
    struct NecroMachineAST* _curr_block;
    struct NecroMachineAST* _init_block;
    struct NecroMachineAST* _cont_block;
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
    uint16_t                slots_used;
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

typedef struct NecroMachinePhi
{
    struct NecroMachineAST** blocks;
    struct NecroMachineAST** values;
    size_t                num_values;
    struct NecroMachineAST*  result;
} NecroMachinePhi;

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
    };
    NECRO_MACHINE_AST_TYPE type;
    NecroMachineType*      necro_machine_type;
} NecroMachineAST;

NECRO_DECLARE_VECTOR(NecroMachineAST*, NecroMachineAST, necro_machine_ast);

typedef struct NecroMachineProgram
{
    // Program info
    NecroMachineASTVector structs;
    NecroMachineASTVector functions;
    NecroMachineASTVector machine_defs;
    NecroMachineASTVector globals;
    NecroMachineAST*      main;

    // Useful structs
    NecroPagedArena       arena;
    NecroSnapshotArena    snapshot_arena;
    NecroIntern*          intern;
    NecroSymTable*        symtable;
    NecroScopedSymTable*  scoped_symtable;
    NecroPrimTypes*       prim_types;

    // Cached data
    size_t                gen_vars;
    NecroMachineType*     necro_poly_type;
    NecroMachineType*     necro_poly_ptr_type;
    NecroMachineType*     necro_data_type;
    NecroMachineAST*      mkIntFnValue;
    NecroMachineAST*      mkFloatFnValue;
} NecroMachineProgram;

// TODO: necro_verify_machine_program

NecroMachineProgram necro_core_to_machine(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types);
void             necro_destroy_machine_program(NecroMachineProgram* program);

///////////////////////////////////////////////////////
// create / build API
///////////////////////////////////////////////////////
NecroVar         necro_gen_var(NecroMachineProgram* program, NecroMachineAST* necro_machine_ast, const char* var_header, NECRO_NAME_UNIQUENESS uniqueness);
NecroMachineAST* necro_create_machine_struct_def(NecroMachineProgram* program, NecroVar name, NecroMachineType** members, size_t num_members);
NecroMachineAST* necro_create_machine_block(NecroMachineProgram* program, const char* name, NecroMachineAST* next_block);
NecroMachineAST* necro_create_machine_fn(NecroMachineProgram* program, NecroVar name, NecroMachineAST* call_body, NecroMachineType* necro_machine_type);
NecroMachineAST* necro_create_param_reg(NecroMachineProgram* program, NecroMachineAST* fn_def, size_t param_num);
NecroMachineAST* necro_create_uint32_necro_machine_value(NecroMachineProgram* program, uint32_t uint_literal);
NecroMachineAST* necro_create_null_necro_machine_value(NecroMachineProgram* program, NecroMachineType* ptr_type);
NecroMachineAST* necro_build_nalloc(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineType* type, uint16_t a_slots_used);
void             necro_build_store_into_tag(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr);
void             necro_build_store_into_slot(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr, size_t dest_slot);
void             necro_build_return(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* return_value);
NecroMachineAST* necro_build_binop(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* left, NecroMachineAST* right, NECRO_MACHINE_BINOP_TYPE op_type);
NecroMachineAST* necro_build_load_from_slot(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_ptr_ast, size_t source_slot, const char* dest_name_header);
NecroMachineAST* necro_build_call(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* fn_to_call_value, NecroMachineAST** a_parameters, size_t num_parameters, const char* dest_name_header);

#endif // NECRO_MACHINE_H
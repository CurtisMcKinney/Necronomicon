/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_NODE_H
#define NECRO_NODE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"
#include "utility/arena.h"
#include "node_type.h"

///////////////////////////////////////////////////////
// Node
//-----------
// * Abstract machine target for necrolang
// * LLVM-IR like, but simpler and necrolang specific
// * Infinite registers
// * Heap defined by a graph of nodes containing nodes
// * Nodes retain state across multiple calls to main
///////////////////////////////////////////////////////

/*
    TODO:
        * null out and zero out memory when allocated with nalloc via memcpy NULL....should be faster than doing it in code!?
        * init_and_load_dyn function (will require breakcond and other stuff to work)
        * dync fn nodes need NecroData?
        * prim ops
        * runtime
        * case
        * closures
        * globals
*/

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
struct NecroNodeAST;
struct NecroNodeLit;

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
    NECRO_NODE_VALUE_REG,
    NECRO_NODE_VALUE_PARAM,
    NECRO_NODE_VALUE_GLOBAL,
    NECRO_NODE_VALUE_UINT16_LITERAL,
    NECRO_NODE_VALUE_UINT32_LITERAL,
    NECRO_NODE_VALUE_INT_LITERAL,
    NECRO_NODE_VALUE_F64_LITERAL,
    NECRO_NODE_VALUE_NULL_PTR_LITERAL,
} NECRO_NODE_VALUE_TYPE;

typedef struct NecroNodeValue
{
    union
    {
        NecroVar reg_name;
        struct NecroParamReg
        {
            NecroVar fn_name;
            size_t   param_num;
        } param_reg;
        uint16_t uint16_literal;
        uint32_t uint_literal;
        int64_t  int_literal;
        double   f64_literal;
        NecroVar global_name;
    };
    NECRO_NODE_VALUE_TYPE value_type;
} NecroNodeValue;

typedef struct NecroSlot
{
    size_t         slot_num;
    NecroNodeType* necro_node_type;
} NecroSlot;

typedef struct NecroTerminator
{
    union
    {
        struct NecroSwitchTerminator
        {
            struct NecroNodeAST* case_blocks;
            size_t*              case_switch_values;
            size_t               num_cases;
            struct NecroNodeAST* choice_val;
        } switch_terminator;
        struct NecroReturnTerminator
        {
            struct NecroNodeAST* return_value;
        } return_terminator;
        struct NecroBreakTerminator
        {
            struct NecroNodeAST* block_to_jump_to;
        } break_terminator;
        struct NecroCondBreak
        {
            struct NecroNodeAST* true_block;
            struct NecroNodeAST* false_block;
            struct NecroNodeAST* cond_value;
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
// NecroNodeAST
///////////////////////////////////////////////////////
typedef struct NecroNodeStructDef
{
    NecroVar        name;
} NecroNodeStructDef;

typedef struct NecroNodeBlock
{
    NecroSymbol           name;
    struct NecroNodeAST** statements;
    size_t                num_statements;
    size_t                statements_size;
    NecroTerminator*      terminator;
    struct NecroNodeAST*  next_block;
} NecroNodeBlock;

typedef struct NecroNodeDef
{
    NecroVar             bind_name;
    NecroVar             node_name;

    struct NecroNodeAST* mk_fn;
    struct NecroNodeAST* init_fn;
    struct NecroNodeAST* update_fn;
    NecroNodeBlock*      update_error_block;
    uint32_t             initial_tag;
    NECRO_STATE_TYPE     state_type;
    struct NecroNodeAST* outer;
    NecroNodeType*       value_type;
    NecroNodeType*       fn_type;
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

    struct NecroNodeAST* global_value; // If global
    // cache if and where slots have been loaded!?
} NecroNodeDef;

typedef enum
{
    NECRO_FN_FN,
    NECRO_FN_RUNTIME,
    // NECRO_FN_PRIM_OP_CMPI,
    // NECRO_FN_PRIM_OP_ZEXT,
    // NECRO_FN_PRIM_OP_BIT_OR,
    // NECRO_FN_PRIM_OP_BIT_AND,
} NECRO_FN_TYPE;

typedef struct NecroNodeFnDef
{
    NecroVar             name;
    struct NecroNodeAST* call_body;
    NECRO_FN_TYPE        fn_type;
    struct NecroNodeAST* fn_value;
    //-------------------
    // compile time data
    struct NecroNodeAST* _curr_block;
} NecroNodeFnDef;

typedef struct NecroNodeCall
{
    struct NecroNodeAST*  fn_value;
    struct NecroNodeAST** parameters;
    size_t                num_parameters;
    struct NecroNodeAST*  result_reg;
} NecroNodeCall;

typedef struct NecroNodeLoad
{
    union
    {
        struct NecroNodeAST* source_ptr;
        struct
        {
            struct NecroNodeAST* source_ptr;
            size_t               source_slot;
        } load_slot;
    };
    enum
    {
        NECRO_LOAD_TAG,
        NECRO_LOAD_PTR,
        NECRO_LOAD_SLOT,
    } load_type;
    struct NecroNodeAST* dest_value;
} NecroNodeLoad;

typedef struct NecroNodeStore
{
    struct NecroNodeAST* source_value;
    union
    {
        struct NecroNodeAST* dest_ptr;
        struct
        {
            struct NecroNodeAST* dest_ptr;
            size_t               dest_slot;
        } store_slot;
    };
    enum
    {
        NECRO_STORE_TAG,
        NECRO_STORE_PTR,
        NECRO_STORE_SLOT,
    } store_type;
} NecroNodeStore;

typedef struct NecroNodeConstantDef
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
} NecroNodeConstantDef;

// TODO/NOTE: Do we need this if we use load slot / store tag, etc?
typedef struct NecroNodeGetElementPtr
{
    struct NecroNodeAST* source_value;
    uint32_t*            indices;
    size_t               num_indices;
    struct NecroNodeAST* dest_value;
} NecroNodeGetElementPtr;

typedef struct NecroNodeBitCast
{
    struct NecroNodeAST* from_value;
    struct NecroNodeAST* to_value;
} NecroNodeBitCast;

typedef struct NecroNodeNAlloc
{
    NecroNodeType*       type_to_alloc;
    uint16_t             slots_used;
    struct NecroNodeAST* result_reg;
} NecroNodeNAlloc;

typedef enum
{
    NECRO_NODE_BINOP_IADD,
    NECRO_NODE_BINOP_ISUB,
    NECRO_NODE_BINOP_IMUL,
    NECRO_NODE_BINOP_IDIV,
    NECRO_NODE_BINOP_FADD,
    NECRO_NODE_BINOP_FSUB,
    NECRO_NODE_BINOP_FMUL,
    NECRO_NODE_BINOP_FDIV,
} NECRO_NODE_BINOP_TYPE;

typedef struct NecroNodeBinOp
{
    NECRO_NODE_BINOP_TYPE binop_tytpe;
    struct NecroNodeAST*  left;
    struct NecroNodeAST*  right;
    struct NecroNodeAST*  result;
} NecroNodeBinOp;

/*
    Notes:
        * Nodes retain state
        * Constants never change (constructed at compile time?). Can be: Literal or Aggregate types only. hoisted and burned into data section.
        * Closures are a specific kind of node
        * Env are a specific kind of data structure which are projected via case (cleanup work on projection only case statements!)
*/
typedef enum
{
    NECRO_NODE_VALUE,
    NECRO_NODE_BLOCK,

    // ops
    NECRO_NODE_CALL,
    NECRO_NODE_LOAD,
    NECRO_NODE_STORE,
    NECRO_NODE_NALLOC,
    NECRO_NODE_BIT_CAST,
    NECRO_NODE_GEP,
    NECRO_NODE_BINOP,

    // Defs
    NECRO_NODE_STRUCT_DEF,
    NECRO_NODE_FN_DEF,
    NECRO_NODE_DEF,
    // NECRO_NODE_CONSTANT_DEF,
} NECRO_NODE_AST_TYPE;

typedef struct NecroNodeAST
{
    union
    {
        NecroNodeValue         value;
        NecroNodeCall          call;
        NecroNodeLoad          load;
        NecroNodeStore         store;
        NecroNodeBlock         block;
        NecroNodeDef           node_def;
        NecroNodeFnDef         fn_def;
        NecroNodeStructDef     struct_def;
        NecroNodeConstantDef   constant;
        NecroNodeGetElementPtr gep;
        NecroNodeBitCast       bit_cast;
        NecroNodeNAlloc        nalloc;
        NecroNodeBinOp         binop;
    };
    NECRO_NODE_AST_TYPE type;
    NecroNodeType*      necro_node_type;
} NecroNodeAST;

NECRO_DECLARE_VECTOR(NecroNodeAST*, NecroNodeAST, necro_node_ast);

typedef struct NecroNodeProgram
{
    // Program info
    NecroNodeASTVector structs;
    NecroNodeASTVector functions;
    NecroNodeASTVector node_defs;
    NecroNodeASTVector globals;
    NecroNodeAST*      main;

    // Useful structs
    NecroPagedArena      arena;
    NecroSnapshotArena   snapshot_arena;
    NecroIntern*         intern;
    NecroSymTable*       symtable;
    NecroScopedSymTable* scoped_symtable;
    NecroPrimTypes*      prim_types;

    // Cached data
    size_t             gen_vars;
    NecroNodeType*     necro_poly_type;
    NecroNodeType*     necro_poly_ptr_type;
    NecroNodeType*     necro_data_type;
    NecroNodeAST*      mkIntFnValue;
    NecroNodeAST*      mkFloatFnValue;
} NecroNodeProgram;

// TODO: necro_verify_node_program

NecroNodeProgram necro_core_to_node(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types);
void             necro_destroy_node_program(NecroNodeProgram* program);

///////////////////////////////////////////////////////
// create / build API
///////////////////////////////////////////////////////
NecroVar      necro_gen_var(NecroNodeProgram* program, NecroNodeAST* necro_node_ast, const char* var_header, NECRO_NAME_UNIQUENESS uniqueness);
NecroNodeAST* necro_create_node_struct_def(NecroNodeProgram* program, NecroVar name, NecroNodeType** members, size_t num_members);
NecroNodeAST* necro_create_node_block(NecroNodeProgram* program, NecroSymbol name, NecroNodeAST* next_block);
NecroNodeAST* necro_create_node_fn(NecroNodeProgram* program, NecroVar name, NecroNodeAST* call_body, NecroNodeType* necro_node_type);
NecroNodeAST* necro_build_nalloc(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeType* type, uint16_t a_slots_used);
void          necro_build_store_into_tag(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, NecroNodeAST* dest_ptr);
void          necro_build_store_into_slot(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, NecroNodeAST* dest_ptr, size_t dest_slot);
NecroNodeAST* necro_create_param_reg(NecroNodeProgram* program, NecroNodeAST* fn_def, size_t param_num);
void          necro_build_return(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* return_value);
NecroNodeAST* necro_create_uint32_necro_node_value(NecroNodeProgram* program, uint32_t uint_literal);
NecroNodeAST* necro_create_null_necro_node_value(NecroNodeProgram* program, NecroNodeType* ptr_type);
NecroNodeAST* necro_build_binop(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* left, NecroNodeAST* right, NECRO_NODE_BINOP_TYPE op_type);
NecroNodeAST* necro_build_load_from_slot(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_ptr_ast, size_t source_slot, const char* dest_name_header);
NecroNodeAST* necro_build_call(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* fn_to_call_value, NecroNodeAST** a_parameters, size_t num_parameters, const char* dest_name_header);

#endif // NECRO_NODE_H
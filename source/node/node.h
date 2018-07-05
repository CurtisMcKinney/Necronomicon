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
        * case
        * dynamic nodes / dynamic application (requires a load) (recursion and closure require dynamic nodes / applications)
        * recursion
        * prim ops
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
    NECRO_NODE_VALUE_REG,
    NECRO_NODE_VALUE_PARAM,
    NECRO_NODE_VALUE_GLOBAL,
    NECRO_NODE_VALUE_UINT16_LITERAL,
    NECRO_NODE_VALUE_UINT32_LITERAL,
    NECRO_NODE_VALUE_INT_LITERAL,
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
    struct NecroNodeAST* ini_fn;
    struct NecroNodeAST* update_fn;
    NecroNodeBlock*      update_error_block;
    uint32_t             initial_tag;
    NECRO_STATE_TYPE     state_type;
    struct NecroNodeAST* outer;
    NecroNodeType*       value_type;
    NecroNodeType*       fn_type;
    bool                 default_mk;
    bool                 default_init;
    struct NodeAST*      decl_group;

    // args
    NecroVar*            arg_names;
    NecroType**          arg_types;
    size_t               num_arg_names;

    // members
    NecroSlot*           members;
    size_t               num_members;
    size_t               members_size;

    struct NecroNodeAST* global_value; // If global
    // cache if and where slots have been loaded!
} NecroNodeDef;

typedef enum
{
    NECRO_FN_FN,
    NECRO_FN_RUNTIME,
    // NECRO_FN_PRIM_OP_ADDI,
    // NECRO_FN_PRIM_OP_SUBI,
    // NECRO_FN_PRIM_OP_MULI,
    // NECRO_FN_PRIM_OP_DIVI,
    // NECRO_FN_PRIM_OP_CMPI,
    // NECRO_FN_PRIM_OP_ZEXT,
    // NECRO_FN_PRIM_OP_BIT_OR,
    // NECRO_FN_PRIM_OP_BIT_AND,
    // NECRO_FN_PRIM_OP_ADDF,
    // NECRO_FN_PRIM_OP_SUBF,
    // NECRO_FN_PRIM_OP_MULF,
    // NECRO_FN_PRIM_OP_DIVF,
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

typedef enum
{
    NECRO_NODE_LIT_DOUBLE,
    NECRO_NODE_LIT_UINT,
    NECRO_NODE_LIT_INT,
    NECRO_NODE_LIT_CHAR,
    NECRO_NODE_LIT_STRING,
    NECRO_NODE_LIT_NULL,
} NECRO_NODE_LIT_TYPE;

typedef struct NecroNodeLit
{
    union
    {
        double      double_literal;
        int64_t     int_literal;
        char        char_literal;
        NecroSymbol string_literal;
        uint32_t    uint_literal;
    };
    NECRO_NODE_LIT_TYPE type;
} NecroNodeLit;

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
        struct NecroNodeAST* source_global;
    };
    enum
    {
        NECRO_LOAD_TAG,
        NECRO_LOAD_PTR,
        NECRO_LOAD_SLOT,
        NECRO_LOAD_GLOBAL,
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
        struct NecroNodeAST* dest_global;
    };
    enum
    {
        NECRO_STORE_TAG,
        NECRO_STORE_PTR,
        NECRO_STORE_SLOT,
        NECRO_STORE_GLOBAL,
    } store_type;
} NecroNodeStore;

typedef struct NecroNodeConstantDef
{
    union
    {
        NecroNodeLit constant_lit;
        struct NecroConstantStruct
        {
            NecroVar name;
        } constant_struct;
    };
    enum
    {
        NECRO_CONSTANT_LIT,
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

/*
    Notes:
        * Nodes retain state
        * Constants never change (constructed at compile time?). Can be: Literal or Aggregate types only. hoisted and burned into data section.
        * Closures are a specific kind of node
        * Env are a specific kind of data structure which are projected via case (cleanup work on projection only case statements!)
*/
typedef enum
{
    NECRO_NODE_LIT,
    NECRO_NODE_VALUE,
    NECRO_NODE_BLOCK,

    // ops
    NECRO_NODE_CALL,
    NECRO_NODE_LOAD,
    NECRO_NODE_STORE,
    NECRO_NODE_NALLOC,
    NECRO_NODE_BIT_CAST,
    NECRO_NODE_GEP,

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
        NecroNodeLit           lit;
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
    NecroPagedArena    arena;
    NecroSnapshotArena snapshot_arena;
    NecroIntern*       intern;
    NecroSymTable*     symtable;
    NecroPrimTypes*    prim_types;

    // Cached data
    size_t             gen_vars;
    NecroNodeType*     necro_poly_type;
    NecroNodeType*     necro_poly_ptr_type;
    NecroNodeType*     necro_data_type;
} NecroNodeProgram;

// TODO: necro_verify_node_program

NecroNodeProgram necro_core_to_node(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroPrimTypes* prim_types);
void             necro_destroy_node_program(NecroNodeProgram* program);

#endif // NECRO_NODE_H
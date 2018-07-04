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
// * Infinite registers
// * Heap defined by a graph of nodes containing nodes
// * Nodes retain state across multiple calls to main
// * LLVM-IR like, but simpler and necrolang specific
///////////////////////////////////////////////////////

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
    NECRO_STATE_STATIC,
    NECRO_STATE_CONSTANT,
    NECRO_STATE_STATELESS,
    NECRO_STATE_STATEFUL,
} NECRO_STATE_TYPE;

typedef enum
{
    NECRO_NODE_VALUE_REG,
    NECRO_NODE_VALUE_PARAM,
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
    };
    NECRO_NODE_VALUE_TYPE value_type;
    NecroNodeType*        necro_node_type;
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
            NecroNodeValue       choice_val;
        } switch_terminator;
        struct NecroReturnTerminator
        {
            NecroNodeValue return_value;
        } return_terminator;
        struct NecroBreakTerminator
        {
            struct NecroNodeAST* block_to_jump_to;
        } break_terminator;
        struct NecroCondBreak
        {
            struct NecroNodeAST* true_block;
            struct NecroNodeAST* false_block;
            NecroNodeValue       cond_value;
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
    NecroVar*            arg_names;
    size_t               num_arg_names;
    struct NecroNodeAST* mk_fn;
    struct NecroNodeAST* ini_fn;
    struct NecroNodeAST* update_fn;
    NecroNodeBlock*      update_error_block;
    uint32_t             initial_tag;
    NECRO_STATE_TYPE     state_type;
    struct NecroNodeDef* outer;
    NecroNodeType*       value_type;

    // inner_nodes
    NecroSlot*           inner_nodes;
    size_t               num_inner_nodes;
    size_t               inner_node_size;
} NecroNodeDef;

typedef enum
{
    NECRO_FN_FN,
    NECRO_FN_RUNTIME,
    NECRO_FN_ALLOC,
    NECRO_FN_PRIM_OP_ADDI,
    NECRO_FN_PRIM_OP_SUBI,
    NECRO_FN_PRIM_OP_MULI,
    NECRO_FN_PRIM_OP_DIVI,
    NECRO_FN_PRIM_OP_CMPI,
    NECRO_FN_PRIM_OP_ZEXT,
    NECRO_FN_PRIM_OP_BIT_OR,
    NECRO_FN_PRIM_OP_BIT_AND,
    NECRO_FN_PRIM_OP_ADDF,
    NECRO_FN_PRIM_OP_SUBF,
    NECRO_FN_PRIM_OP_MULF,
    NECRO_FN_PRIM_OP_DIVF,
} NECRO_FN_TYPE;

typedef struct NecroNodeFnDef
{
    NecroVar             name;
    struct NecroNodeAST* call_body;
    NECRO_FN_TYPE        fn_type;
} NecroNodeFnDef;

typedef struct NecroNodeCall
{
    NecroVar        name;
    NecroNodeValue* parameters;
    size_t          num_parameters;
    NecroNodeValue  result_reg;
    NECRO_FN_TYPE   fn_type;
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
        NecroNodeValue source_ptr;
        struct
        {
            NecroNodeValue source_ptr;
            NecroSlot      source_slot;
        } load_slot;
        NecroVar source_global_name;
    };
    enum
    {
        NECRO_LOAD_TAG,
        NECRO_LOAD_PTR,
        NECRO_LOAD_SLOT,
        NECRO_LOAD_GLOBAL,
    } load_type;
    NecroNodeValue dest_value;
} NecroNodeLoad;

typedef struct NecroNodeStore
{
    NecroNodeValue source_value;
    union
    {
        NecroNodeValue dest_ptr;
        struct
        {
            NecroNodeValue dest_ptr;
            NecroSlot      dest_slot;
        } store_slot;
        NecroVar dest_global_name;
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
        NecroNodeLit*       constant_lit;
        NecroNodeStructDef* constant_struct;
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
    NecroNodeValue  source_value;
    uint32_t*       indices;
    size_t          num_indices;
    NecroNodeValue  dest_value;
} NecroNodeGetElementPtr;

typedef struct NecroNodeBitCast
{
    NecroNodeValue from_value;
    NecroNodeValue to_value;
} NecroNodeBitCast;

typedef struct NecroNodeNAlloc
{
    NecroNodeType* type_to_alloc;
    uint16_t       slots_used;
    NecroNodeValue result_reg;
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
    NECRO_NODE_CALL,
    NECRO_NODE_BLOCK,

    NECRO_NODE_LOAD,
    NECRO_NODE_STORE,
    NECRO_NODE_NALLOC,
    NECRO_NODE_BIT_CAST, // Maybe remove?
    NECRO_NODE_GEP,      // Maybe remove?

    // Defs
    NECRO_NODE_STRUCT_DEF,
    NECRO_NODE_FN_DEF,
    NECRO_NODE_DEF,
    NECRO_NODE_CONSTANT_DEF,
} NECRO_NODE_AST_TYPE;

typedef struct NecroNodeAST
{
    union
    {
        NecroNodeCall          call;
        NecroNodeLit           lit;
        NecroNodeLoad          load;
        NecroNodeStore         store;
        NecroNodeBlock         block;
        NecroNodeDef           node_def;
        NecroNodeFnDef         fn_def;
        NecroNodeStructDef     struct_def;
        NecroNodeConstantDef   constant;
        NecroNodeGetElementPtr gep; // Maybe remove?
        NecroNodeBitCast       bit_cast; // Maybe remove?
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
    NecroNodeASTVector globals;
    NecroNodeASTVector functions;
    NecroNodeAST*      main;

    // Useful structs
    NecroPagedArena    arena;
    NecroSnapshotArena snapshot_arena;
    NecroIntern*       intern;
    NecroSymTable*     symtable;
    NecroPrimTypes*    prim_types;

    // Cached data
    size_t             gen_vars;
    NecroNodeType*     necro_poly_ptr_type;
    // NecroNodeType*     necro_val_type;
    NecroNodeType*     necro_data_type;
} NecroNodeProgram;

// TODO: necro_verify_node_program

NecroNodeProgram necro_core_to_node(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroPrimTypes* prim_types);
void             necro_destroy_node_program(NecroNodeProgram* program);
void             necro_node_print_ast(NecroNodeProgram* program, NecroNodeAST* ast);
void             necro_print_node_program(NecroNodeProgram* program);

#endif // NECRO_NODE_H
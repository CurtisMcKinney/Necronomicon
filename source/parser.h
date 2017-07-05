/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef PARSER_H
#define PARSER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "arena.h"

// Local offset into AST arena
typedef uint32_t NecroAST_LocalPtr;

//=====================================================
// AST Undefined
//=====================================================

typedef struct
{
    uint8_t _pad;
} NecroAST_Undefined;

//=====================================================
// AST Constant
//=====================================================

typedef enum
{
    NECRO_AST_CONSTANT_FLOAT,
    NECRO_AST_CONSTANT_INTEGER,
    NECRO_AST_CONSTANT_STRING,
    NECRO_AST_CONSTANT_BOOL,
} NecroAST_ConstantType;

typedef struct
{
    union
    {
        double double_literal;
        int64_t int_literal;
        NecroStringSlice str;
		bool boolean_literal;
    };

    NecroAST_ConstantType type;
} NecroAST_Constant;

//=====================================================
// AST Binary Operation
//=====================================================

typedef enum
{
    NECRO_BIN_OP_GT,
    NECRO_BIN_OP_LT,
    NECRO_BIN_OP_GTE,
    NECRO_BIN_OP_LTE,
    NECRO_BIN_OP_ADD,
    NECRO_BIN_OP_SUBTRACT,
    NECRO_BIN_OP_MUL,
    NECRO_BIN_OP_DIV,
    NECRO_BIN_OP_MOD,
} NecroAST_BinOpType;

typedef struct
{
    NecroAST_LocalPtr lhs;
    NecroAST_LocalPtr rhs;
    NecroAST_BinOpType type;
} NecroAST_BinOp;

typedef enum
{
    NECRO_AST_UNDEFINED,
    NECRO_AST_CONSTANT,
    NECRO_AST_BIN_OP
} NecroAST_NodeType;

//=====================================================
// AST Node
//=====================================================

typedef struct
{
    union
    {
        NecroAST_Undefined undefined;
        NecroAST_Constant constant;
        NecroAST_BinOp bin_op;
    };

    NecroAST_NodeType type;
} NecroAST_Node;

//=====================================================
// AST
//=====================================================

typedef struct
{
    NecroArena arena;
} NecroAST;

static inline NecroAST_Node* ast_get_node(NecroAST* ast, NecroAST_LocalPtr local_ptr)
{
    return ((NecroAST_Node*) ast->arena.region) + local_ptr;
}

static inline NecroAST_Node* ast_get_root_node(NecroAST* ast)
{
    return (NecroAST_Node*) ast->arena.region;
}

static inline void ast_prealloc(NecroAST* ast, size_t num_nodes)
{
    arena_alloc(&ast->arena, sizeof(NecroAST_Node) * num_nodes, arena_allow_realloc);
}

void print_ast(NecroAST* ast);

//=====================================================
// Parsing
//=====================================================

typedef enum
{
    ParseSuccessful,
    ParseError
} NecroParse_Result;

NecroParse_Result parse_ast(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast);

#endif // PARSER_H

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
static const NecroAST_LocalPtr null_local_ptr = -1;

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
    NECRO_AST_CONSTANT_CHAR
} NecroAST_ConstantType;

typedef struct
{
    union
    {
        double double_literal;
        int64_t int_literal;
        NecroSymbol symbol;
		bool boolean_literal;
        char char_literal;
    };

    NecroAST_ConstantType type;
} NecroAST_Constant;

//=====================================================
// AST Binary Operation
//=====================================================

typedef enum
{
    NECRO_BIN_OP_ADD,
    NECRO_BIN_OP_SUB,
    NECRO_BIN_OP_MUL,
    NECRO_BIN_OP_DIV,
    NECRO_BIN_OP_MOD,
    NECRO_BIN_OP_GT,
    NECRO_BIN_OP_LT,
    NECRO_BIN_OP_GTE,
    NECRO_BIN_OP_LTE,
	NECRO_BIN_OP_DOUBLE_COLON,
	NECRO_BIN_OP_LEFT_SHIFT,
	NECRO_BIN_OP_RIGHT_SHIFT,
	NECRO_BIN_OP_PIPE,
	NECRO_BIN_OP_FORWARD_PIPE,
	NECRO_BIN_OP_BACK_PIPE,
    NECRO_BIN_OP_EQUALS,
	NECRO_BIN_OP_AND,
	NECRO_BIN_OP_OR
} NecroAST_BinOpType;

typedef struct
{
    NecroAST_LocalPtr lhs;
    NecroAST_LocalPtr rhs;
    NecroAST_BinOpType type;
} NecroAST_BinOp;

typedef struct
{
    NecroAST_LocalPtr if_expr;
    NecroAST_LocalPtr then_expr;
    NecroAST_LocalPtr else_expr;
} NecroAST_IfThenElse;

typedef enum
{
    NECRO_AST_UNDEFINED,
    NECRO_AST_CONSTANT,
    NECRO_AST_BIN_OP,
    NECRO_AST_IF_THEN_ELSE
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
        NecroAST_IfThenElse if_then_else;
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

void print_ast(NecroAST* ast, NecroIntern* intern);

//=====================================================
// Parsing
//=====================================================

typedef enum
{
    ParseSuccessful,
    ParseError
} NecroParse_Result;

NecroParse_Result parse_ast(NecroLexToken** tokens, NecroAST* ast);

#endif // PARSER_H

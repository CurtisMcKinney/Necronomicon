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
static const NecroAST_LocalPtr null_local_ptr = (uint32_t) -1;

typedef enum
{
    NECRO_AST_UNDEFINED,
    NECRO_AST_CONSTANT,
    NECRO_AST_UN_OP,
    NECRO_AST_BIN_OP,
    NECRO_AST_IF_THEN_ELSE,
    NECRO_AST_TOP_DECL,
    NECRO_AST_DECL,
    NECRO_AST_SIMPLE_ASSIGNMENT,
    NECRO_AST_APATS_ASSIGNMENT,
    NECRO_AST_RIGHT_HAND_SIDE,
    NECRO_AST_FUNCTION_EXPRESSION,
    NECRO_AST_VARIABLE,
    NECRO_AST_APATS,
    NECRO_AST_WILDCARD,
    NECRO_AST_LAMBDA,
    // NECRO_AST_MODULE,
} NecroAST_NodeType;

//=====================================================
// AST Module
//=====================================================



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
// AST Unary Operation
//=====================================================

// typedef enum
// {
//     NECRO_UN_OP_NEG = 0,
//     NECRO_UN_OP_COUNT,
//     NECRO_UN_OP_UNDEFINED = NECRO_UN_OP_COUNT
// } NecroAST_UnaryOpType;
//
// typedef struct
// {
//     NecroAST_LocalPtr rhs;
//     NecroAST_UnaryOpType type;
// } NecroAST_UnaryOp;

//=====================================================
// AST Binary Operation
//=====================================================

typedef enum
{
    NECRO_BIN_OP_ADD = 0,
    NECRO_BIN_OP_SUB,
    NECRO_BIN_OP_MUL,
    NECRO_BIN_OP_DIV,
    NECRO_BIN_OP_MOD,
    NECRO_BIN_OP_GT,
    NECRO_BIN_OP_LT,
    NECRO_BIN_OP_GTE,
    NECRO_BIN_OP_LTE,
    NECRO_BIN_OP_COLON,
	NECRO_BIN_OP_DOUBLE_COLON,
	NECRO_BIN_OP_LEFT_SHIFT,
	NECRO_BIN_OP_RIGHT_SHIFT,
	NECRO_BIN_OP_PIPE,
	NECRO_BIN_OP_FORWARD_PIPE,
	NECRO_BIN_OP_BACK_PIPE,
    NECRO_BIN_OP_EQUALS,
    NECRO_BIN_OP_NOT_EQUALS,
	NECRO_BIN_OP_AND,
	NECRO_BIN_OP_OR,
    NECRO_BIN_OP_DOT,
    NECRO_BIN_OP_DOLLAR,
    NECRO_BIN_OP_BIND_RIGHT,
    NECRO_BIN_OP_BIND_LEFT,
    NECRO_BIN_OP_DOUBLE_EXCLAMATION,
    NECRO_BIN_OP_APPEND,
    NECRO_BIN_OP_COUNT,
    NECRO_BIN_OP_UNDEFINED = NECRO_BIN_OP_COUNT
} NecroAST_BinOpType;

typedef struct
{
    NecroAST_LocalPtr lhs;
    NecroAST_LocalPtr rhs;
    NecroAST_BinOpType type;
} NecroAST_BinOp;

//=====================================================
// AST if then else
//=====================================================

typedef struct
{
    NecroAST_LocalPtr if_expr;
    NecroAST_LocalPtr then_expr;
    NecroAST_LocalPtr else_expr;
} NecroAST_IfThenElse;

//=====================================================
// AST Right Hand Side
//=====================================================

typedef struct
{
    NecroAST_LocalPtr expression;
    NecroAST_LocalPtr declarations;
} NecroAST_RightHandSide;

//=====================================================
// AST Simple Assignment
//=====================================================

typedef struct
{
    NecroSymbol variable_name;
    NecroAST_LocalPtr rhs;
} NecroAST_SimpleAssignment;

//=====================================================
// AST apats
//=====================================================

typedef struct
{
    NecroAST_LocalPtr apat;
    NecroAST_LocalPtr next_apat;
} NecroAST_Apats;

//=====================================================
// AST Apats Assignment
//=====================================================

typedef struct
{
    NecroSymbol variable_name;
    NecroAST_LocalPtr apats;
    NecroAST_LocalPtr rhs;
} NecroAST_ApatsAssignment;

//=====================================================
// AST Lambda
//=====================================================

typedef struct
{
    NecroAST_LocalPtr apats;
    NecroAST_LocalPtr expression;
} NecroAST_Lambda;

//=====================================================
// AST Variable
//=====================================================

typedef enum
{
    NECRO_AST_VARIABLE_ID,
    NECRO_AST_VARIABLE_SYMBOL
} NecroAST_VariableType;

typedef struct
{
    union
    {
        NecroSymbol variable_id;
        NECRO_LEX_TOKEN_TYPE variable_symbol;
    };

    NecroAST_VariableType variable_type;
} NecroAST_Variable;

//=====================================================
// AST Function Expression
//=====================================================

typedef struct
{
    NecroAST_LocalPtr aexp;
    NecroAST_LocalPtr next_fexpression; // Points to the next in the list, null_local_ptr if the end
} NecroAST_FunctionExpression;

//=====================================================
// AST Declarations
//=====================================================

typedef struct
{
    NecroAST_LocalPtr declaration_impl;
    NecroAST_LocalPtr next_declaration; // Points to the next in the list, null_local_ptr if the end
} NecroAST_Declaration;

//=====================================================
// AST Top Declarations
//=====================================================

typedef struct
{
    NecroAST_LocalPtr declaration;
    NecroAST_LocalPtr next_top_decl; // Points to the next in the list, null_local_ptr if the end
} NecroAST_TopDeclaration;


//=====================================================
// AST Module
//=====================================================

// To Do: Define these!
// typedef struct
// {
//     NecroAST_LocalPtr body;
// } NecroAST_SimpleModule;
//
// typedef struct
// {
//     NecroAST_LocalPtr body;
// } NecroAST_ModuleWithExports;
//
// typedef union
// {
//     NecroAST_Body body;
//     NecroAST_SimpleModule simple_module;
//     NecroAST_ModuleWithExports module_with_exports;
// } NecroAST_Module;

//=====================================================
// AST Node
//=====================================================

typedef struct
{
    union
    {
        NecroAST_Undefined undefined;
        NecroAST_Constant constant;
        // NecroAST_UnaryOp unary_op; // Do we need this?
        NecroAST_BinOp bin_op;
        NecroAST_IfThenElse if_then_else;
        NecroAST_TopDeclaration top_declaration;
        NecroAST_Declaration declaration;
        NecroAST_SimpleAssignment simple_assignment;
        NecroAST_Apats apats;
        NecroAST_ApatsAssignment apats_assignment;
        NecroAST_RightHandSide right_hand_side;
        NecroAST_FunctionExpression fexpression;
        NecroAST_Variable variable;
        NecroAST_Lambda lambda;
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
    assert(ast != NULL);
    assert(local_ptr != null_local_ptr);
    return ((NecroAST_Node*) ast->arena.region) + local_ptr;
}

static inline NecroAST_Node* ast_get_root_node(NecroAST* ast)
{
    assert(ast != NULL);
    return (NecroAST_Node*) ast->arena.region;
}

void print_ast(NecroAST* ast, NecroIntern* intern, NecroAST_LocalPtr root_node_ptr);

//=====================================================
// Parsing
//=====================================================


typedef enum
{
    NECRO_BIN_OP_ASSOC_LEFT,
    NECRO_BIN_OP_ASSOC_NONE,
    NECRO_BIN_OP_ASSOC_RIGHT
} NecroParse_BinOpAssociativity;

typedef struct
{
    int precedence;
    NecroParse_BinOpAssociativity associativity;
} NecroParse_BinOpBehavior;

static const NecroParse_BinOpBehavior bin_op_behaviors[NECRO_BIN_OP_COUNT + 1] = {
    { 6, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_ADD
    { 6, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_SUB
    { 7, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_MUL
    { 7, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_DIV
    { 7, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_MOD
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_GT
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_LT
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_GTE
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_LTE
    { 5, NECRO_BIN_OP_ASSOC_RIGHT },  // NECRO_BIN_OP_COLON
	{ 9, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_DOUBLE_COLON
	{ 1, NECRO_BIN_OP_ASSOC_RIGHT },  // NECRO_BIN_OP_LEFT_SHIFT
	{ 1, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_RIGHT_SHIFT
	{ 2, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_PIPE
	{ 0, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_FORWARD_PIPE
	{ 0, NECRO_BIN_OP_ASSOC_LEFT }, // NECRO_BIN_OP_BACK_PIPE
    { 4, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_EQUALS
    { 4, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_NOT_EQUALS
	{ 3, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_AND
	{ 2, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_OR
    { 9, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_DOT
    { 0, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_DOLLAR
    { 1, NECRO_BIN_OP_ASSOC_LEFT }, // NECRO_BIN_OP_BIND_RIGHT
    { 1, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_BIND_LEFT
    { 9, NECRO_BIN_OP_ASSOC_LEFT }, // NECRO_BIN_OP_DOUBLE_EXCLAMATION
    { 5, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_APPEND
    { 0, NECRO_BIN_OP_ASSOC_NONE }   // NECRO_BIN_OP_UNDEFINED
};


typedef enum
{
    ParseSuccessful,
    ParseError
} NecroParse_Result;

typedef enum
{
    NECRO_DESCENT_PARSING,
    NECRO_DESCENT_PARSE_ERROR,
    NECRO_DESCENT_PARSE_DONE
} NecroParse_DescentState;

typedef struct
{
    char* error_message;
    NecroAST* ast;
    NecroLexToken* tokens;
    size_t current_token;
    NecroParse_DescentState descent_state;
} NecroParser;

static const size_t MAX_ERROR_MESSAGE_SIZE = 512;

static inline void construct_parser(NecroParser* parser, NecroAST* ast, NecroLexToken* tokens)
{
    parser->error_message = malloc(MAX_ERROR_MESSAGE_SIZE * sizeof(char));
    parser->error_message[0] = '\0';
    parser->current_token = 0;
    parser->ast = ast;
    parser->tokens = tokens;
    parser->descent_state = NECRO_DESCENT_PARSING;
}

static inline void destruct_parser(NecroParser* parser)
{
    free(parser->error_message);
    *parser = (NecroParser) { 0, NULL, NULL, 0, NECRO_DESCENT_PARSE_DONE };
}

NecroParse_Result parse_ast(NecroParser* parser, NecroAST_LocalPtr* out_root_node_ptr);
void compute_ast_math(NecroAST* ast, NecroAST_LocalPtr root_node_ptr);

#endif // PARSER_H

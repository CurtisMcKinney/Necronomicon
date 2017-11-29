/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef AST_H
#define AST_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "lexer.h"
#include "intern.h"
#include "arena.h"
#include "parser.h"
#include "symtable.h"

struct NecroAST_Node_Reified;

//=====================================================
// Types Enums
//=====================================================
// typedef enum
// {
//     NECRO_AST_UNDEFINED,
//     NECRO_AST_CONSTANT,
//     NECRO_AST_UN_OP,
//     NECRO_AST_BIN_OP,
//     NECRO_AST_IF_THEN_ELSE,
//     NECRO_AST_TOP_DECL,
//     NECRO_AST_DECL,
//     NECRO_AST_SIMPLE_ASSIGNMENT,
//     NECRO_AST_APATS_ASSIGNMENT,
//     NECRO_AST_RIGHT_HAND_SIDE,
//     NECRO_AST_LET_EXPRESSION,
//     NECRO_AST_FUNCTION_EXPRESSION,
//     NECRO_AST_VARIABLE,
//     NECRO_AST_APATS,
//     NECRO_AST_WILDCARD,
//     NECRO_AST_LAMBDA,
//     NECRO_AST_DO,
//     NECRO_AST_LIST_NODE,
//     NECRO_AST_EXPRESSION_LIST,
//     NECRO_AST_TUPLE,
//     NECRO_BIND_ASSIGNMENT,
//     NECRO_AST_ARITHMETIC_SEQUENCE
//     // NECRO_AST_MODULE,
// } NecroAST_NodeType;

// typedef enum
// {
//     NECRO_AST_CONSTANT_FLOAT,
//     NECRO_AST_CONSTANT_INTEGER,
//     NECRO_AST_CONSTANT_STRING,
//     NECRO_AST_CONSTANT_BOOL,
//     NECRO_AST_CONSTANT_CHAR
// } NecroAST_ConstantType;

// typedef enum
// {
//     NECRO_BIN_OP_ADD = 0,
//     NECRO_BIN_OP_SUB,
//     NECRO_BIN_OP_MUL,
//     NECRO_BIN_OP_DIV,
//     NECRO_BIN_OP_MOD,
//     NECRO_BIN_OP_GT,
//     NECRO_BIN_OP_LT,
//     NECRO_BIN_OP_GTE,
//     NECRO_BIN_OP_LTE,
//     NECRO_BIN_OP_COLON,
// 	NECRO_BIN_OP_DOUBLE_COLON,
// 	NECRO_BIN_OP_LEFT_SHIFT,
// 	NECRO_BIN_OP_RIGHT_SHIFT,
// 	NECRO_BIN_OP_PIPE,
// 	NECRO_BIN_OP_FORWARD_PIPE,
// 	NECRO_BIN_OP_BACK_PIPE,
//     NECRO_BIN_OP_EQUALS,
//     NECRO_BIN_OP_NOT_EQUALS,
// 	NECRO_BIN_OP_AND,
// 	NECRO_BIN_OP_OR,
//     NECRO_BIN_OP_DOT,
//     NECRO_BIN_OP_DOLLAR,
//     NECRO_BIN_OP_BIND_RIGHT,
//     NECRO_BIN_OP_BIND_LEFT,
//     NECRO_BIN_OP_DOUBLE_EXCLAMATION,
//     NECRO_BIN_OP_APPEND,
//     NECRO_BIN_OP_COUNT,
//     NECRO_BIN_OP_UNDEFINED = NECRO_BIN_OP_COUNT
// } NecroAST_BinOpType;

// typedef enum
// {
//     NECRO_AST_VARIABLE_ID,
//     NECRO_AST_VARIABLE_SYMBOL
// } NecroAST_VariableType;

// typedef enum
// {
//     NECRO_ARITHMETIC_ENUM_FROM,
//     NECRO_ARITHMETIC_ENUM_FROM_TO,
//     NECRO_ARITHMETIC_ENUM_FROM_THEN_TO,
// } NecroAST_ArithmeticSeqType;

//=====================================================
// AST Module
//=====================================================


//=====================================================
// AST Undefined
//=====================================================
typedef struct
{
    uint8_t _pad;
} NecroAST_Undefined_Reified;

//=====================================================
// AST Constant
//=====================================================

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
} NecroAST_Constant_Reified;

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
//     struct NecroAST_Node_Reified* rhs;
//     NecroAST_UnaryOpType type;
// } NecroAST_UnaryOp;

//=====================================================
// AST Binary Operation
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* lhs;
    struct NecroAST_Node_Reified* rhs;
    NecroAST_BinOpType type;
} NecroAST_BinOp_Reified;

//=====================================================
// AST if then else
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* if_expr;
    struct NecroAST_Node_Reified* then_expr;
    struct NecroAST_Node_Reified* else_expr;
} NecroAST_IfThenElse_Reified;

//=====================================================
// AST Right Hand Side
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* expression;
    struct NecroAST_Node_Reified* declarations;
} NecroAST_RightHandSide_Reified;

//=====================================================
// AST Let Expression
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* expression;
    struct NecroAST_Node_Reified* declarations;
} NecroAST_LetExpression_Reified;

//=====================================================
// AST Simple Assignment
//=====================================================

typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAST_Node_Reified* rhs;
    NecroID                       id;
} NecroAST_SimpleAssignment_Reified;

//=====================================================
// AST Bind Assignment
//=====================================================

typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAST_Node_Reified* expression;
    NecroID                       id;
} NecroAST_BindAssignment_Reified;

//=====================================================
// AST apats
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* apat;
    struct NecroAST_Node_Reified* next_apat;
} NecroAST_Apats_Reified;

//=====================================================
// AST Apats Assignment
//=====================================================

typedef struct
{
    NecroSymbol                   variable_name;
    struct NecroAST_Node_Reified* apats;
    struct NecroAST_Node_Reified* rhs;
    NecroID                       id;
} NecroAST_ApatsAssignment_Reified;

//=====================================================
// AST Lambda
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* apats;
    struct NecroAST_Node_Reified* expression;
} NecroAST_Lambda_Reified;

//=====================================================
// AST List Node
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* item;
    struct NecroAST_Node_Reified* next_item;
} NecroAST_ListNode_Reified;

//=====================================================
// AST Expression List
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* expressions; // NecroAST_ListNode of expressions
} NecroAST_ExpressionList_Reified;

//=====================================================
// AST Tuple
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* expressions; // NecroAST_ListNode of expressions
} NecroAST_Tuple_Reified;

//=====================================================
// AST Do
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* statement_list; // NecroAST_ListNode of do statement items
} NecroAST_Do_Reified;

//=====================================================
// AST Variable
//=====================================================

typedef struct
{
    union
    {
        NecroSymbol          variable_id;
        NECRO_LEX_TOKEN_TYPE variable_symbol;
    };
    NecroAST_VariableType variable_type;
    NecroID               id;
} NecroAST_Variable_Reified;

//=====================================================
// AST Function Expression
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* aexp;
    struct NecroAST_Node_Reified* next_fexpression; // Points to the next in the list, null_local_ptr if the end
} NecroAST_FunctionExpression_Reified;

//=====================================================
// AST Declarations
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* declaration_impl;
    struct NecroAST_Node_Reified* next_declaration; // Points to the next in the list, null_local_ptr if the end
} NecroAST_Declaration_Reified;

//=====================================================
// AST Top Declarations
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* declaration;
    struct NecroAST_Node_Reified* next_top_decl; // Points to the next in the list, null_local_ptr if the end
} NecroAST_TopDeclaration_Reified;


//=====================================================
// AST Module
//=====================================================

// To Do: Define these!
// typedef struct
// {
//     struct NecroAST_Node_Reified* body;
// } NecroAST_SimpleModule;
//
// typedef struct
// {
//     struct NecroAST_Node_Reified* body;
// } NecroAST_ModuleWithExports;
//
// typedef union
// {
//     NecroAST_Body body;
//     NecroAST_SimpleModule simple_module;
//     NecroAST_ModuleWithExports module_with_exports;
// } NecroAST_Module;

//=====================================================
// AST Arithmetic Sequence
//=====================================================

typedef struct
{
    struct NecroAST_Node_Reified* from;
    struct NecroAST_Node_Reified* then;
    struct NecroAST_Node_Reified* to;
    NecroAST_ArithmeticSeqType type;
} NecroAST_ArithmeticSequence_Reified;

//=====================================================
// AST Node
//=====================================================

typedef struct NecroAST_Node_Reified
{
    union
    {
        NecroAST_Undefined_Reified          undefined;
        NecroAST_Constant_Reified           constant;
        // NecroAST_UnaryOp unary_op; // Do we need this?
        NecroAST_BinOp_Reified              bin_op;
        NecroAST_IfThenElse_Reified         if_then_else;
        NecroAST_TopDeclaration_Reified     top_declaration;
        NecroAST_Declaration_Reified        declaration;
        NecroAST_SimpleAssignment_Reified   simple_assignment;
        NecroAST_Apats_Reified              apats;
        NecroAST_ApatsAssignment_Reified    apats_assignment;
        NecroAST_RightHandSide_Reified      right_hand_side;
        NecroAST_LetExpression_Reified      let_expression;
        NecroAST_FunctionExpression_Reified fexpression;
        NecroAST_Variable_Reified           variable;
        NecroAST_Lambda_Reified             lambda;
        NecroAST_Do_Reified                 do_statement;
        NecroAST_ListNode_Reified           list;
        NecroAST_ExpressionList_Reified     expression_list;
        NecroAST_Tuple_Reified              tuple;
        NecroAST_BindAssignment_Reified     bind_assignment;
        NecroAST_ArithmeticSequence_Reified arithmetic_sequence;
    };
    NecroAST_NodeType type;
} NecroAST_Node_Reified;

typedef struct
{
    NecroPagedArena        arena;
    NecroAST_Node_Reified* root;
} NecroAST_Reified;

NecroAST_Reified necro_create_reified_ast();
NecroAST_Reified necro_reify_ast(NecroAST* a_ast, NecroAST_LocalPtr a_root);
void             necro_destroy_reified_ast(NecroAST_Reified* ast);
void             necro_test_reify(const char* input_string);
void             necro_print_reified_ast(NecroAST_Reified* ast, NecroIntern* intern);

#endif // AST_H

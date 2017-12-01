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
// AST Case
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* expression;
    struct NecroAST_Node_Reified* alternatives;
    struct NecroAST_Node_Reified* declarations;
} NecroAST_Case_Reified;

//=====================================================
// AST CaseAlternative
//=====================================================
typedef struct
{
    struct NecroAST_Node_Reified* pat;
    struct NecroAST_Node_Reified* body;
} NecroAST_CaseAlternative_Reified;

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
        double      double_literal;
        int64_t     int_literal;
        NecroSymbol symbol;
		bool        boolean_literal;
        char        char_literal;
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
        NecroAST_Case_Reified               case_expression;
        NecroAST_CaseAlternative_Reified    case_alternative;
    };
    NecroAST_NodeType type;
    NecroSourceLoc    source_loc;
} NecroAST_Node_Reified;

typedef struct
{
    NecroPagedArena        arena;
    NecroAST_Node_Reified* root;
} NecroAST_Reified;

NecroAST_Reified necro_create_reified_ast();
NecroAST_Reified necro_reify_ast(NecroAST* a_ast, NecroAST_LocalPtr a_root);
void             necro_destroy_reified_ast(NecroAST_Reified* ast);
void             necro_print_reified_ast(NecroAST_Reified* ast, NecroIntern* intern);

#endif // AST_H

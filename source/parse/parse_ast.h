/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef PARSE_AST_TEST_H
#define PARSE_AST_TEST_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

/*
    NECRO_AST_CONSTANT,
    NECRO_AST_UN_OP,
    NECRO_AST_BIN_OP,
    NECRO_AST_IF_THEN_ELSE,
    NECRO_AST_TOP_DECL,
    NECRO_AST_DECL,
    NECRO_AST_SIMPLE_ASSIGNMENT,
    NECRO_AST_APATS_ASSIGNMENT,
    NECRO_AST_PAT_ASSIGNMENT,
    NECRO_AST_RIGHT_HAND_SIDE,
    NECRO_AST_LET_EXPRESSION,
    NECRO_AST_FUNCTION_EXPRESSION,
    NECRO_AST_INFIX_EXPRESSION,
    NECRO_AST_VARIABLE,
    NECRO_AST_APATS,
    NECRO_AST_WILDCARD,
    NECRO_AST_LAMBDA,
    NECRO_AST_DO,
    NECRO_AST_PAT_EXPRESSION,
    NECRO_AST_LIST_NODE,
    NECRO_AST_EXPRESSION_LIST,
    NECRO_AST_EXPRESSION_ARRAY,
    NECRO_AST_TUPLE,
    NECRO_BIND_ASSIGNMENT,
    NECRO_PAT_BIND_ASSIGNMENT,
    NECRO_AST_ARITHMETIC_SEQUENCE,
    NECRO_AST_CASE,
    NECRO_AST_CASE_ALTERNATIVE,
    NECRO_AST_CONID,
    NECRO_AST_TYPE_APP,
    NECRO_AST_BIN_OP_SYM,
    NECRO_AST_OP_LEFT_SECTION,
    NECRO_AST_OP_RIGHT_SECTION,
    NECRO_AST_CONSTRUCTOR,
    NECRO_AST_SIMPLE_TYPE,

    NECRO_AST_DATA_DECLARATION,
    NECRO_AST_TYPE_CLASS_CONTEXT,
    NECRO_AST_TYPE_CLASS_DECLARATION,
    NECRO_AST_TYPE_CLASS_INSTANCE,
    NECRO_AST_TYPE_SIGNATURE,
    NECRO_AST_FUNCTION_TYPE,
*/

NecroAST_LocalPtr necro_create_parse_ast_bin_op(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr lhs, NecroAST_LocalPtr rhs, NecroAST_BinOpType type, NecroSymbol symbol);
NecroAST_LocalPtr necro_create_parse_ast_op_left_section(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_BinOpType type, NecroSymbol symbol);
NecroAST_LocalPtr necro_create_parse_ast_op_right_section(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr right_ast, NecroAST_BinOpType type, NecroSymbol symbol);
NecroAST_LocalPtr necro_create_parse_ast_if_then_else(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr if_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr else_ast);
NecroAST_LocalPtr necro_create_parse_ast_top_decl(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_create_parse_ast_decl(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_create_parse_ast_simple_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr rhs_ast, NecroAST_LocalPtr initializer_ast);
NecroAST_LocalPtr necro_create_parse_ast_apats_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr rhs_ast);
NecroAST_LocalPtr necro_create_parse_ast_pat_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr rhs_ast);
NecroAST_LocalPtr necro_create_parse_ast_rhs(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast);
NecroAST_LocalPtr necro_create_parse_ast_let(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast);
NecroAST_LocalPtr necro_create_parse_ast_fexpr(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr aexpr_ast, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_create_parse_ast_var(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_VAR_TYPE type, NecroAST_LocalPtr initializer_ast);
NecroAST_LocalPtr necro_create_parse_ast_apats(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apat_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_create_parse_ast_wildcard(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroAST_LocalPtr necro_create_parse_ast_lambda(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_create_parse_ast_do(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr statement_list_ast);
NecroAST_LocalPtr necro_create_parse_ast_pat_expr(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_create_parse_ast_list(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_create_parse_ast_expression_list(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_create_parse_ast_expression_array(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_create_parse_ast_tuple(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_create_parse_ast_bind_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_create_parse_ast_pat_bind_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_create_parse_ast_arithmetic_sequence(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr from_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr to_ast, NecroAST_ArithmeticSeqType type);
NecroAST_LocalPtr necro_create_parse_ast_case(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expr_ast, NecroAST_LocalPtr alts_ast);
NecroAST_LocalPtr necro_create_parse_ast_case_alternative(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr body_ast);
NecroAST_LocalPtr necro_create_parse_ast_conid(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_CON_TYPE type);
NecroAST_LocalPtr necro_create_parse_ast_type_app(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr atype_ast, NecroAST_LocalPtr type_ast);
NecroAST_LocalPtr necro_create_parse_ast_bin_op_sym(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_LocalPtr op_ast, NecroAST_LocalPtr right_ast);
NecroAST_LocalPtr necro_create_parse_ast_constructor(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr conid_ast, NecroAST_LocalPtr arg_list_ast);
NecroAST_LocalPtr necro_create_parse_ast_simple_type(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr type_con_ast, NecroAST_LocalPtr type_var_list_ast);

#endif // PARSE_AST_TEST

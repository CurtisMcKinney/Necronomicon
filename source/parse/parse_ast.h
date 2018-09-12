/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef PARSE_AST_TEST_H
#define PARSE_AST_TEST_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

NecroAST_LocalPtr necro_parse_ast_create_constant(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_Constant constant);
NecroAST_LocalPtr necro_parse_ast_create_bin_op(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr lhs, NecroAST_LocalPtr rhs, NecroAST_BinOpType type, NecroSymbol symbol);
NecroAST_LocalPtr necro_parse_ast_create_op_left_section(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_BinOpType type, NecroSymbol symbol);
NecroAST_LocalPtr necro_parse_ast_create_op_right_section(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr right_ast, NecroAST_BinOpType type, NecroSymbol symbol);
NecroAST_LocalPtr necro_parse_ast_create_if_then_else(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr if_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr else_ast);
NecroAST_LocalPtr necro_parse_ast_create_top_decl(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_parse_ast_create_decl(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_parse_ast_create_simple_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr rhs_ast, NecroAST_LocalPtr initializer_ast);
NecroAST_LocalPtr necro_parse_ast_create_apats_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr rhs_ast);
NecroAST_LocalPtr necro_parse_ast_create_pat_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr rhs_ast);
NecroAST_LocalPtr necro_parse_ast_create_rhs(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast);
NecroAST_LocalPtr necro_parse_ast_create_let(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast);
NecroAST_LocalPtr necro_parse_ast_create_fexpr(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr aexpr_ast, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_parse_ast_create_var(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_VAR_TYPE type, NecroAST_LocalPtr initializer_ast);
NecroAST_LocalPtr necro_parse_ast_create_apats(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apat_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_parse_ast_create_wildcard(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroAST_LocalPtr necro_parse_ast_create_lambda(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_parse_ast_create_do(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr statement_list_ast);
NecroAST_LocalPtr necro_parse_ast_create_pat_expr(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_parse_ast_create_list(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast);
NecroAST_LocalPtr necro_parse_ast_create_expression_list(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_parse_ast_create_expression_array(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_parse_ast_create_tuple(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast);
NecroAST_LocalPtr necro_parse_ast_create_bind_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_parse_ast_create_pat_bind_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr expr_ast);
NecroAST_LocalPtr necro_parse_ast_create_arithmetic_sequence(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr from_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr to_ast, NecroAST_ArithmeticSeqType type);
NecroAST_LocalPtr necro_parse_ast_create_case(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expr_ast, NecroAST_LocalPtr alts_ast);
NecroAST_LocalPtr necro_parse_ast_create_case_alternative(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr body_ast);
NecroAST_LocalPtr necro_parse_ast_create_conid(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_CON_TYPE type);
NecroAST_LocalPtr necro_parse_ast_create_type_app(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr atype_ast, NecroAST_LocalPtr type_ast);
NecroAST_LocalPtr necro_parse_ast_create_bin_op_sym(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_LocalPtr op_ast, NecroAST_LocalPtr right_ast);
NecroAST_LocalPtr necro_parse_ast_create_constructor(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr conid_ast, NecroAST_LocalPtr arg_list_ast);
NecroAST_LocalPtr necro_parse_ast_create_simple_type(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr type_con_ast, NecroAST_LocalPtr type_var_list_ast);
NecroAST_LocalPtr necro_parse_ast_create_data_declaration(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr simple_type_ast, NecroAST_LocalPtr constructor_list_ast);
NecroAST_LocalPtr necro_parse_ast_create_class_context(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr conid_ast, NecroAST_LocalPtr varid_ast);
NecroAST_LocalPtr necro_parse_ast_create_class_declaration(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr context_ast, NecroAST_LocalPtr tycls_ast, NecroAST_LocalPtr tyvar_ast, NecroAST_LocalPtr declarations_ast);
NecroAST_LocalPtr necro_parse_ast_create_instance(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr context_ast, NecroAST_LocalPtr tycls_ast, NecroAST_LocalPtr inst_ast, NecroAST_LocalPtr declarations_ast);
NecroAST_LocalPtr necro_parse_ast_create_type_signature(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr var_ast, NecroAST_LocalPtr context_ast, NecroAST_LocalPtr type_ast, NECRO_SIG_TYPE sig_type);
NecroAST_LocalPtr necro_parse_ast_create_function_type(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr lhs_ast, NecroAST_LocalPtr rhs_ast);

void              necro_parse_ast_assert_eq(NecroIntern* intern, NecroAST* ast1, NecroAST* ast2);

#endif // PARSE_AST_TEST

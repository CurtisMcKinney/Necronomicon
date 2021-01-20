/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef PARSE_AST_TEST_H
#define PARSE_AST_TEST_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "parser.h"

NecroParseAstLocalPtr necro_parse_ast_create_constant(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstConstant constant);
NecroParseAstLocalPtr necro_parse_ast_create_bin_op(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr lhs, NecroParseAstLocalPtr rhs, NECRO_BIN_OP_TYPE type, NecroSymbol symbol);
NecroParseAstLocalPtr necro_parse_ast_create_op_left_section(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr left_ast, NECRO_BIN_OP_TYPE type, NecroSymbol symbol);
NecroParseAstLocalPtr necro_parse_ast_create_op_right_section(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr right_ast, NECRO_BIN_OP_TYPE type, NecroSymbol symbol);
NecroParseAstLocalPtr necro_parse_ast_create_if_then_else(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr if_ast, NecroParseAstLocalPtr then_ast, NecroParseAstLocalPtr else_ast);
NecroParseAstLocalPtr necro_parse_ast_create_top_decl(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr item_ast, NecroParseAstLocalPtr next_ast);
NecroParseAstLocalPtr necro_parse_ast_create_decl(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr item_ast, NecroParseAstLocalPtr next_ast);
NecroParseAstLocalPtr necro_parse_ast_create_simple_assignment(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroParseAstLocalPtr rhs_ast, NecroParseAstLocalPtr initializer_ast);
NecroParseAstLocalPtr necro_parse_ast_create_apats_assignment(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroParseAstLocalPtr apats_ast, NecroParseAstLocalPtr rhs_ast);
NecroParseAstLocalPtr necro_parse_ast_create_pat_assignment(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr pat_ast, NecroParseAstLocalPtr rhs_ast);
NecroParseAstLocalPtr necro_parse_ast_create_rhs(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expression_ast, NecroParseAstLocalPtr declarations_ast);
NecroParseAstLocalPtr necro_parse_ast_create_let(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expression_ast, NecroParseAstLocalPtr declarations_ast);
NecroParseAstLocalPtr necro_parse_ast_create_fexpr(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr aexpr_ast, NecroParseAstLocalPtr expr_ast);
NecroParseAstLocalPtr necro_parse_ast_create_var(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_VAR_TYPE type, NecroParseAstLocalPtr initializer_ast, NECRO_TYPE_ORDER order);
NecroParseAstLocalPtr necro_parse_ast_create_apats(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr apat_ast, NecroParseAstLocalPtr next_ast);
NecroParseAstLocalPtr necro_parse_ast_create_wildcard(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroParseAstLocalPtr necro_parse_ast_create_lambda(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr apats_ast, NecroParseAstLocalPtr expr_ast);
NecroParseAstLocalPtr necro_parse_ast_create_do(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr statement_list_ast);
NecroParseAstLocalPtr necro_parse_ast_create_seq_expr(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast, NECRO_SEQUENCE_TYPE sequence_type);
NecroParseAstLocalPtr necro_parse_ast_create_list(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr item_ast, NecroParseAstLocalPtr next_ast);
NecroParseAstLocalPtr necro_parse_ast_create_expression_list(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast);
NecroParseAstLocalPtr necro_parse_ast_create_expression_array(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast);
NecroParseAstLocalPtr necro_parse_ast_create_tuple(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast);
NecroParseAstLocalPtr necro_parse_ast_create_unboxed_tuple(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast);
NecroParseAstLocalPtr necro_parse_ast_create_bind_assignment(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NecroParseAstLocalPtr expr_ast);
NecroParseAstLocalPtr necro_parse_ast_create_pat_bind_assignment(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr pat_ast, NecroParseAstLocalPtr expr_ast);
NecroParseAstLocalPtr necro_parse_ast_create_arithmetic_sequence(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr from_ast, NecroParseAstLocalPtr then_ast, NecroParseAstLocalPtr to_ast, NECRO_ARITHMETIC_SEQUENCE_TYPE type);
NecroParseAstLocalPtr necro_parse_ast_create_case(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expr_ast, NecroParseAstLocalPtr alts_ast);
NecroParseAstLocalPtr necro_parse_ast_create_case_alternative(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr pat_ast, NecroParseAstLocalPtr body_ast);
NecroParseAstLocalPtr necro_parse_ast_create_conid(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_CON_TYPE type);
NecroParseAstLocalPtr necro_parse_ast_create_type_app(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr atype_ast, NecroParseAstLocalPtr type_ast);
NecroParseAstLocalPtr necro_parse_ast_create_bin_op_sym(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr left_ast, NecroParseAstLocalPtr op_ast, NecroParseAstLocalPtr right_ast);
NecroParseAstLocalPtr necro_parse_ast_create_constructor(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr conid_ast, NecroParseAstLocalPtr arg_list_ast);
NecroParseAstLocalPtr necro_parse_ast_create_simple_type(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr type_con_ast, NecroParseAstLocalPtr type_var_list_ast);
NecroParseAstLocalPtr necro_parse_ast_create_data_declaration(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr simple_type_ast, NecroParseAstLocalPtr constructor_list_ast, NecroParseAstLocalPtr deriving_list);
NecroParseAstLocalPtr necro_parse_ast_create_class_context(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr conid_ast, NecroParseAstLocalPtr varid_ast);
NecroParseAstLocalPtr necro_parse_ast_create_class_declaration(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr tycls_ast, NecroParseAstLocalPtr tyvar_ast, NecroParseAstLocalPtr declarations_ast);
NecroParseAstLocalPtr necro_parse_ast_create_instance(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr tycls_ast, NecroParseAstLocalPtr inst_ast, NecroParseAstLocalPtr declarations_ast);
NecroParseAstLocalPtr necro_parse_ast_create_type_signature(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr var_ast, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr type_ast, NECRO_SIG_TYPE sig_type);
NecroParseAstLocalPtr necro_parse_ast_create_expr_type_signature(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expression_ast, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr type_ast);
NecroParseAstLocalPtr necro_parse_ast_create_function_type(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr lhs_ast, NecroParseAstLocalPtr rhs_ast);
NecroParseAstLocalPtr necro_parse_ast_create_type_attribute(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr attributed_type, NECRO_TYPE_ATTRIBUTE_TYPE type);
NecroParseAstLocalPtr necro_parse_ast_create_for_loop(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr range_init, NecroParseAstLocalPtr value_init, NecroParseAstLocalPtr index_apat, NecroParseAstLocalPtr value_apat, NecroParseAstLocalPtr expression);
NecroParseAstLocalPtr necro_parse_ast_create_while_loop(NecroParseArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr value_init, NecroParseAstLocalPtr value_apat, NecroParseAstLocalPtr while_expression, NecroParseAstLocalPtr do_expression);

void                  necro_parse_ast_assert_eq(NecroParseAstArena* ast1, NecroParseAstArena* ast2);

#endif // PARSE_AST_TEST

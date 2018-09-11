/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include "parse_ast.h"

NecroAST_LocalPtr necro_create_parse_ast_bin_op(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr lhs, NecroAST_LocalPtr rhs, NecroAST_BinOpType type, NecroSymbol symbol)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_BIN_OP;
    node->bin_op.lhs       = lhs;
    node->bin_op.rhs       = rhs;
    node->bin_op.type      = type;
    node->bin_op.symbol    = symbol;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_op_left_section(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_BinOpType type, NecroSymbol symbol)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node       = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                   = NECRO_AST_OP_LEFT_SECTION;
    node->op_left_section.left   = left_ast;
    node->op_left_section.type   = type;
    node->op_left_section.symbol = symbol;
    node->source_loc             = source_loc;
    node->end_loc                = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_op_right_section(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr right_ast, NecroAST_BinOpType type, NecroSymbol symbol)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node        = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                    = NECRO_AST_OP_RIGHT_SECTION;
    node->op_right_section.right  = right_ast;
    node->op_right_section.type   = type;
    node->op_right_section.symbol = symbol;
    node->source_loc              = source_loc;
    node->end_loc                 = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_if_then_else(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr if_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr else_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node       = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                   = NECRO_AST_IF_THEN_ELSE;
    node->if_then_else.if_expr   = if_ast;
    node->if_then_else.then_expr = then_ast;
    node->if_then_else.else_expr = else_ast;
    node->source_loc             = source_loc;
    node->end_loc                = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_top_decl(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node              = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                          = NECRO_AST_TOP_DECL;
    node->top_declaration.declaration   = item_ast;
    node->top_declaration.next_top_decl = next_ast;
    node->source_loc                    = source_loc;
    node->end_loc                       = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_decl(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                         = NECRO_AST_DECL;
    node->declaration.declaration_impl = item_ast;
    node->declaration.next_declaration = next_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_simple_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr rhs_ast, NecroAST_LocalPtr initializer_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node                = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                            = NECRO_AST_SIMPLE_ASSIGNMENT;
    node->simple_assignment.variable_name = var_name;
    node->simple_assignment.rhs           = rhs_ast;
    node->simple_assignment.initializer   = initializer_ast;
    node->source_loc                      = source_loc;
    node->end_loc                         = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_apats_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr rhs_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node               = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                           = NECRO_AST_APATS_ASSIGNMENT;
    node->apats_assignment.variable_name = var_name;
    node->apats_assignment.apats         = apats_ast;
    node->apats_assignment.rhs           = rhs_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_pat_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr rhs_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node   = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type               = NECRO_AST_PAT_ASSIGNMENT;
    node->pat_assignment.pat = pat_ast;
    node->pat_assignment.rhs = rhs_ast;
    node->source_loc         = source_loc;
    node->end_loc            = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_rhs(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                         = NECRO_AST_RIGHT_HAND_SIDE;
    node->right_hand_side.expression   = expression_ast;
    node->right_hand_side.declarations = declarations_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_let(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                        = NECRO_AST_LET_EXPRESSION;
    node->let_expression.expression   = expression_ast;
    node->let_expression.declarations = declarations_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_fexpr(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr aexpr_ast, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                         = NECRO_AST_FUNCTION_EXPRESSION;
    node->fexpression.aexp             = aexpr_ast;
    node->fexpression.next_fexpression = expr_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_var(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_VAR_TYPE type, NecroAST_LocalPtr initializer_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node     = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                 = NECRO_AST_VARIABLE;
    node->variable.symbol      = symbol;
    node->variable.var_type    = type;
    node->variable.initializer = initializer_ast;
    node->source_loc           = source_loc;
    node->end_loc              = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_apats(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apat_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_APATS;
    node->apats.apat       = apat_ast;
    node->apats.next_apat  = next_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_wildcard(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_WILDCARD;
    node->undefined._pad   = 0;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_lambda(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node  = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type              = NECRO_AST_APATS;
    node->lambda.apats      = apats_ast;
    node->lambda.expression = expr_ast;
    node->source_loc        = source_loc;
    node->end_loc           = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_do(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr statement_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                        = NECRO_AST_DO;
    node->do_statement.statement_list = statement_list_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_pat_expr(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node               = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                           = NECRO_AST_PAT_EXPRESSION;
    node->pattern_expression.expressions = expressions_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_list(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_LIST_NODE;
    node->list.item        = item_ast;
    node->list.next_item   = next_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_expression_list(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                        = NECRO_AST_EXPRESSION_LIST;
    node->expression_list.expressions = expressions_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_expression_array(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                         = NECRO_AST_EXPRESSION_ARRAY;
    node->expression_array.expressions = expressions_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_tuple(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node  = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type              = NECRO_AST_TUPLE;
    node->tuple.expressions = expressions_ast;
    node->source_loc        = source_loc;
    node->end_loc           = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_bind_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node              = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                          = NECRO_BIND_ASSIGNMENT;
    node->bind_assignment.variable_name = symbol;
    node->bind_assignment.expression    = expr_ast;
    node->source_loc                    = source_loc;
    node->end_loc                       = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_pat_bind_assignment(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node               = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                           = NECRO_PAT_BIND_ASSIGNMENT;
    node->pat_bind_assignment.pat        = pat_ast;
    node->pat_bind_assignment.expression = expr_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_arithmetic_sequence(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr from_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr to_ast, NecroAST_ArithmeticSeqType type)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node         = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                     = NECRO_AST_ARITHMETIC_SEQUENCE;
    node->arithmetic_sequence.from = from_ast;
    node->arithmetic_sequence.then = then_ast;
    node->arithmetic_sequence.to   = to_ast;
    node->arithmetic_sequence.type = type;
    node->source_loc               = source_loc;
    node->end_loc                  = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_case(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expr_ast, NecroAST_LocalPtr alts_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                         = NECRO_AST_CASE;
    node->case_expression.expression   = expr_ast;
    node->case_expression.alternatives = alts_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_case_alternative(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr body_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node      = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                  = NECRO_AST_CASE_ALTERNATIVE;
    node->case_alternative.pat  = pat_ast;
    node->case_alternative.body = body_ast;
    node->source_loc            = source_loc;
    node->end_loc               = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_conid(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_CON_TYPE type)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_CONID;
    node->conid.symbol     = symbol;
    node->conid.con_type   = type;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_type_app(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr atype_ast, NecroAST_LocalPtr type_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_TYPE_APP;
    node->type_app.ty      = atype_ast;
    node->type_app.next_ty = type_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_bin_op_sym(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_LocalPtr op_ast, NecroAST_LocalPtr right_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type             = NECRO_AST_BIN_OP_SYM;
    node->bin_op_sym.left  = left_ast;
    node->bin_op_sym.op    = op_ast;
    node->bin_op_sym.right = right_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_constructor(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr conid_ast, NecroAST_LocalPtr arg_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node     = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                 = NECRO_AST_CONSTRUCTOR;
    node->constructor.conid    = conid_ast;
    node->constructor.arg_list = arg_list_ast;
    node->source_loc           = source_loc;
    node->end_loc              = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_create_parse_ast_simple_type(struct NecroParser* parser, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr type_con_ast, NecroAST_LocalPtr type_var_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node          = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                      = NECRO_AST_SIMPLE_TYPE;
    node->simple_type.type_con      = type_con_ast;
    node->simple_type.type_var_list = type_var_list_ast;
    node->source_loc                = source_loc;
    node->end_loc                   = end_loc;
    return local_ptr;
}

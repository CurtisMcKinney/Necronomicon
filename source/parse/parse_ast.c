/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include "parse_ast.h"

///////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////
NecroAST_LocalPtr necro_parse_ast_create_constant(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_Constant constant)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_CONSTANT;
    node->constant         = constant;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_bin_op(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr lhs, NecroAST_LocalPtr rhs, NecroAST_BinOpType type, NecroSymbol symbol)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_BIN_OP;
    node->bin_op.lhs       = lhs;
    node->bin_op.rhs       = rhs;
    node->bin_op.type      = type;
    node->bin_op.symbol    = symbol;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_op_left_section(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_BinOpType type, NecroSymbol symbol)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node       = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                   = NECRO_AST_OP_LEFT_SECTION;
    node->op_left_section.left   = left_ast;
    node->op_left_section.type   = type;
    node->op_left_section.symbol = symbol;
    node->source_loc             = source_loc;
    node->end_loc                = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_op_right_section(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr right_ast, NecroAST_BinOpType type, NecroSymbol symbol)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node        = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                    = NECRO_AST_OP_RIGHT_SECTION;
    node->op_right_section.right  = right_ast;
    node->op_right_section.type   = type;
    node->op_right_section.symbol = symbol;
    node->source_loc              = source_loc;
    node->end_loc                 = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_if_then_else(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr if_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr else_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node       = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                   = NECRO_AST_IF_THEN_ELSE;
    node->if_then_else.if_expr   = if_ast;
    node->if_then_else.then_expr = then_ast;
    node->if_then_else.else_expr = else_ast;
    node->source_loc             = source_loc;
    node->end_loc                = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_top_decl(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node              = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                          = NECRO_AST_TOP_DECL;
    node->top_declaration.declaration   = item_ast;
    node->top_declaration.next_top_decl = next_ast;
    node->source_loc                    = source_loc;
    node->end_loc                       = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_decl(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_DECL;
    node->declaration.declaration_impl = item_ast;
    node->declaration.next_declaration = next_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_simple_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr rhs_ast, NecroAST_LocalPtr initializer_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node                = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                            = NECRO_AST_SIMPLE_ASSIGNMENT;
    node->simple_assignment.variable_name = var_name;
    node->simple_assignment.rhs           = rhs_ast;
    node->simple_assignment.initializer   = initializer_ast;
    node->source_loc                      = source_loc;
    node->end_loc                         = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_apats_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr rhs_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node               = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                           = NECRO_AST_APATS_ASSIGNMENT;
    node->apats_assignment.variable_name = var_name;
    node->apats_assignment.apats         = apats_ast;
    node->apats_assignment.rhs           = rhs_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_pat_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr rhs_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node   = necro_parse_ast_alloc(arena, &local_ptr);
    node->type               = NECRO_AST_PAT_ASSIGNMENT;
    node->pat_assignment.pat = pat_ast;
    node->pat_assignment.rhs = rhs_ast;
    node->source_loc         = source_loc;
    node->end_loc            = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_rhs(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_RIGHT_HAND_SIDE;
    node->right_hand_side.expression   = expression_ast;
    node->right_hand_side.declarations = declarations_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_let(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expression_ast, NecroAST_LocalPtr declarations_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_LET_EXPRESSION;
    node->let_expression.expression   = expression_ast;
    node->let_expression.declarations = declarations_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_fexpr(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr aexpr_ast, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_FUNCTION_EXPRESSION;
    node->fexpression.aexp             = aexpr_ast;
    node->fexpression.next_fexpression = expr_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_var(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_VAR_TYPE type, NecroAST_LocalPtr initializer_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node     = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                 = NECRO_AST_VARIABLE;
    node->variable.symbol      = symbol;
    node->variable.var_type    = type;
    node->variable.initializer = initializer_ast;
    node->source_loc           = source_loc;
    node->end_loc              = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_apats(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apat_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_APATS;
    node->apats.apat       = apat_ast;
    node->apats.next_apat  = next_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_wildcard(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_WILDCARD;
    node->undefined._pad   = 0;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_lambda(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr apats_ast, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node  = necro_parse_ast_alloc(arena, &local_ptr);
    node->type              = NECRO_AST_APATS;
    node->lambda.apats      = apats_ast;
    node->lambda.expression = expr_ast;
    node->source_loc        = source_loc;
    node->end_loc           = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_do(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr statement_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_DO;
    node->do_statement.statement_list = statement_list_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_pat_expr(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node               = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                           = NECRO_AST_PAT_EXPRESSION;
    node->pattern_expression.expressions = expressions_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_list(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr item_ast, NecroAST_LocalPtr next_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_LIST_NODE;
    node->list.item        = item_ast;
    node->list.next_item   = next_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_expression_list(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_EXPRESSION_LIST;
    node->expression_list.expressions = expressions_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_expression_array(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_EXPRESSION_ARRAY;
    node->expression_array.expressions = expressions_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_tuple(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expressions_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node  = necro_parse_ast_alloc(arena, &local_ptr);
    node->type              = NECRO_AST_TUPLE;
    node->tuple.expressions = expressions_ast;
    node->source_loc        = source_loc;
    node->end_loc           = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_bind_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node              = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                          = NECRO_BIND_ASSIGNMENT;
    node->bind_assignment.variable_name = symbol;
    node->bind_assignment.expression    = expr_ast;
    node->source_loc                    = source_loc;
    node->end_loc                       = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_pat_bind_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr expr_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node               = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                           = NECRO_PAT_BIND_ASSIGNMENT;
    node->pat_bind_assignment.pat        = pat_ast;
    node->pat_bind_assignment.expression = expr_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_arithmetic_sequence(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr from_ast, NecroAST_LocalPtr then_ast, NecroAST_LocalPtr to_ast, NecroAST_ArithmeticSeqType type)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node         = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                     = NECRO_AST_ARITHMETIC_SEQUENCE;
    node->arithmetic_sequence.from = from_ast;
    node->arithmetic_sequence.then = then_ast;
    node->arithmetic_sequence.to   = to_ast;
    node->arithmetic_sequence.type = type;
    node->source_loc               = source_loc;
    node->end_loc                  = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_case(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr expr_ast, NecroAST_LocalPtr alts_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_CASE;
    node->case_expression.expression   = expr_ast;
    node->case_expression.alternatives = alts_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_case_alternative(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr pat_ast, NecroAST_LocalPtr body_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node      = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                  = NECRO_AST_CASE_ALTERNATIVE;
    node->case_alternative.pat  = pat_ast;
    node->case_alternative.body = body_ast;
    node->source_loc            = source_loc;
    node->end_loc               = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_conid(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_CON_TYPE type)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_CONID;
    node->conid.symbol     = symbol;
    node->conid.con_type   = type;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_type_app(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr atype_ast, NecroAST_LocalPtr type_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_TYPE_APP;
    node->type_app.ty      = atype_ast;
    node->type_app.next_ty = type_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_bin_op_sym(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr left_ast, NecroAST_LocalPtr op_ast, NecroAST_LocalPtr right_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_BIN_OP_SYM;
    node->bin_op_sym.left  = left_ast;
    node->bin_op_sym.op    = op_ast;
    node->bin_op_sym.right = right_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_constructor(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr conid_ast, NecroAST_LocalPtr arg_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node     = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                 = NECRO_AST_CONSTRUCTOR;
    node->constructor.conid    = conid_ast;
    node->constructor.arg_list = arg_list_ast;
    node->source_loc           = source_loc;
    node->end_loc              = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_simple_type(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr type_con_ast, NecroAST_LocalPtr type_var_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node          = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                      = NECRO_AST_SIMPLE_TYPE;
    node->simple_type.type_con      = type_con_ast;
    node->simple_type.type_var_list = type_var_list_ast;
    node->source_loc                = source_loc;
    node->end_loc                   = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_data_declaration(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr simple_type_ast, NecroAST_LocalPtr constructor_list_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node                  = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                              = NECRO_AST_DATA_DECLARATION;
    node->data_declaration.simpletype       = simple_type_ast;
    node->data_declaration.constructor_list = constructor_list_ast;
    node->source_loc                        = source_loc;
    node->end_loc                           = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_class_context(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr conid_ast, NecroAST_LocalPtr varid_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node         = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                     = NECRO_AST_TYPE_CLASS_CONTEXT;
    node->type_class_context.conid = conid_ast;
    node->type_class_context.varid = varid_ast;
    node->source_loc               = source_loc;
    node->end_loc                  = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_class_declaration(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr context_ast, NecroAST_LocalPtr tycls_ast, NecroAST_LocalPtr tyvar_ast, NecroAST_LocalPtr declarations_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node                    = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                                = NECRO_AST_TYPE_CLASS_DECLARATION;
    node->type_class_declaration.context      = context_ast;
    node->type_class_declaration.tycls        = tycls_ast;
    node->type_class_declaration.tyvar        = tyvar_ast;
    node->type_class_declaration.declarations = declarations_ast;
    node->source_loc                          = source_loc;
    node->end_loc                             = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_instance(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr context_ast, NecroAST_LocalPtr tycls_ast, NecroAST_LocalPtr inst_ast, NecroAST_LocalPtr declarations_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node                 = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                             = NECRO_AST_TYPE_CLASS_INSTANCE;
    node->type_class_instance.context      = context_ast;
    node->type_class_instance.qtycls       = tycls_ast;
    node->type_class_instance.inst         = inst_ast;
    node->type_class_instance.declarations = declarations_ast;
    node->source_loc                       = source_loc;
    node->end_loc                          = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_type_signature(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr var_ast, NecroAST_LocalPtr context_ast, NecroAST_LocalPtr type_ast, NECRO_SIG_TYPE sig_type)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node        = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                    = NECRO_AST_TYPE_SIGNATURE;
    node->type_signature.var      = var_ast;
    node->type_signature.context  = context_ast;
    node->type_signature.type     = type_ast;
    node->type_signature.sig_type = sig_type;
    node->source_loc              = source_loc;
    node->end_loc                 = end_loc;
    return local_ptr;
}

NecroAST_LocalPtr necro_parse_ast_create_function_type(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAST_LocalPtr lhs_ast, NecroAST_LocalPtr rhs_ast)
{
    NecroAST_LocalPtr local_ptr;
    NecroAST_Node*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_FUNCTION_TYPE;
    node->function_type.type          = lhs_ast;
    node->function_type.next_on_arrow = rhs_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

///////////////////////////////////////////////////////
// Assert Eq
///////////////////////////////////////////////////////
void necro_parse_ast_assert_eq_go(NecroIntern* intern, NecroAST* ast1, NecroAST_LocalPtr ptr1, NecroAST* ast2, NecroAST_LocalPtr ptr2);

void necro_parse_ast_assert_eq_constant(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_CONSTANT);
    assert(node2->type == NECRO_AST_CONSTANT);
    assert(node1->constant.type == node2->constant.type);
    switch (node1->constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
    case NECRO_AST_CONSTANT_FLOAT:
        assert(node1->constant.double_literal == node2->constant.double_literal);
        break;
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
    case NECRO_AST_CONSTANT_INTEGER:
        assert(node1->constant.int_literal == node2->constant.int_literal);
        break;
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
    case NECRO_AST_CONSTANT_CHAR:
        assert(node1->constant.char_literal == node2->constant.char_literal);
        break;
    case NECRO_AST_CONSTANT_STRING:
        assert(strcmp(necro_intern_get_string(intern, node1->constant.symbol), necro_intern_get_string(intern, node2->constant.symbol)) == 0);
        break;
    }
    UNUSED(ast1);
    UNUSED(ast2);
}

void necro_parse_ast_assert_eq_bin_op(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_BIN_OP);
    assert(node2->type == NECRO_AST_BIN_OP);
    assert(node1->bin_op.type == node2->bin_op.type);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->bin_op.lhs, ast2, node2->bin_op.lhs);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->bin_op.rhs, ast2, node2->bin_op.rhs);
}

void necro_parse_ast_assert_eq_if_then_else(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_IF_THEN_ELSE);
    assert(node2->type == NECRO_AST_IF_THEN_ELSE);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->if_then_else.if_expr, ast2, node2->if_then_else.if_expr);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->if_then_else.then_expr, ast2, node2->if_then_else.then_expr);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->if_then_else.else_expr, ast2, node2->if_then_else.else_expr);
}

void necro_parse_ast_assert_eq_top_decl(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_TOP_DECL);
    assert(node2->type == NECRO_AST_TOP_DECL);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->top_declaration.declaration, ast2, node2->top_declaration.declaration);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->top_declaration.next_top_decl, ast2, node2->top_declaration.next_top_decl);
}

void necro_parse_ast_assert_eq_decl(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_DECL);
    assert(node2->type == NECRO_AST_DECL);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->declaration.declaration_impl, ast2, node2->declaration.declaration_impl);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->declaration.next_declaration, ast2, node2->declaration.next_declaration);
}

void necro_parse_ast_assert_eq_simple_assignment(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    assert(node2->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    assert(strcmp(necro_intern_get_string(intern, node1->simple_assignment.variable_name), necro_intern_get_string(intern, node2->simple_assignment.variable_name)) == 0);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->simple_assignment.initializer, ast2, node2->simple_assignment.initializer);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->simple_assignment.rhs, ast2, node2->simple_assignment.rhs);
}

void necro_parse_ast_assert_eq_apats_assignment(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_APATS_ASSIGNMENT);
    assert(node2->type == NECRO_AST_APATS_ASSIGNMENT);
    assert(strcmp(necro_intern_get_string(intern, node1->apats_assignment.variable_name), necro_intern_get_string(intern, node2->apats_assignment.variable_name)) == 0);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->apats_assignment.apats, ast2, node2->apats_assignment.apats);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->apats_assignment.rhs, ast2, node2->apats_assignment.rhs);
}

void necro_parse_ast_assert_eq_pat_assignment(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_PAT_ASSIGNMENT);
    assert(node2->type == NECRO_AST_PAT_ASSIGNMENT);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->pat_assignment.pat, ast2, node2->pat_assignment.pat);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->pat_assignment.rhs, ast2, node2->pat_assignment.rhs);
}

void necro_parse_ast_assert_eq_var(NecroIntern* intern, NecroAST* ast1, NecroAST_Node* node1, NecroAST* ast2, NecroAST_Node* node2)
{
    assert(node1->type == NECRO_AST_VARIABLE);
    assert(node2->type == NECRO_AST_VARIABLE);
    assert(strcmp(necro_intern_get_string(intern, node1->variable.symbol), necro_intern_get_string(intern, node2->variable.symbol)) == 0);
    necro_parse_ast_assert_eq_go(intern, ast1, node1->variable.initializer, ast2, node2->variable.initializer);
    assert(node2->variable.var_type == node2->variable.var_type);
}

void necro_parse_ast_assert_eq_go(NecroIntern* intern, NecroAST* ast1, NecroAST_LocalPtr ptr1, NecroAST* ast2, NecroAST_LocalPtr ptr2)
{
    if (ptr1 == null_local_ptr && ptr2 == null_local_ptr)
        return;
    assert(ptr1 != null_local_ptr);
    assert(ptr2 != null_local_ptr);
    NecroAST_Node* node1 = ast_get_node(ast1, ptr1);
    NecroAST_Node* node2 = ast_get_node(ast2, ptr2);
    assert(node1->type == node2->type);
    switch (node1->type)
    {
    // case NECRO_AST_UN_OP:                  necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_CONSTANT:               necro_parse_ast_assert_eq_constant(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_BIN_OP:                 necro_parse_ast_assert_eq_bin_op(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_IF_THEN_ELSE:           necro_parse_ast_assert_eq_if_then_else(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TOP_DECL:               necro_parse_ast_assert_eq_top_decl(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_DECL:                   necro_parse_ast_assert_eq_decl(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:      necro_parse_ast_assert_eq_simple_assignment(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_APATS_ASSIGNMENT:       necro_parse_ast_assert_eq_apats_assignment(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_PAT_ASSIGNMENT:         necro_parse_ast_assert_eq_pat_assignment(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_RIGHT_HAND_SIDE:        necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_LET_EXPRESSION:         necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_FUNCTION_EXPRESSION:    necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_VARIABLE:               necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_APATS:                  necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_WILDCARD:               necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_LAMBDA:                 necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_DO:                     necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_PAT_EXPRESSION:         necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_LIST_NODE:              necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_EXPRESSION_LIST:        necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_EXPRESSION_ARRAY:       necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TUPLE:                  necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_BIND_ASSIGNMENT:            necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_PAT_BIND_ASSIGNMENT:        necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:    necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_CASE:                   necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_CASE_ALTERNATIVE:       necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_CONID:                  necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_APP:               necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_BIN_OP_SYM:             necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_OP_LEFT_SECTION:        necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_OP_RIGHT_SECTION:       necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_CONSTRUCTOR:            necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_SIMPLE_TYPE:            necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_DATA_DECLARATION:       necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:     necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_CLASS_DECLARATION: necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:    necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_SIGNATURE:         necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    case NECRO_AST_FUNCTION_TYPE:          necro_parse_ast_assert_eq_var(intern, ast1, node1, ast2, node2); break;
    default:
        assert(false);
        return;
    }
}

void necro_parse_ast_assert_eq(NecroIntern* intern, NecroAST* ast1, NecroAST* ast2)
{
    necro_parse_ast_assert_eq_go(intern, ast1, ast1->root, ast2, ast2->root);
}

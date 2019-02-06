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
NecroParseAstLocalPtr necro_parse_ast_create_constant(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstConstant constant)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_CONSTANT;
    node->constant         = constant;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_bin_op(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr lhs, NecroParseAstLocalPtr rhs, NECRO_BIN_OP_TYPE type, NecroSymbol symbol)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_BIN_OP;
    node->bin_op.lhs       = lhs;
    node->bin_op.rhs       = rhs;
    node->bin_op.type      = type;
    node->bin_op.symbol    = symbol;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_op_left_section(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr left_ast, NECRO_BIN_OP_TYPE type, NecroSymbol symbol)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node       = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                   = NECRO_AST_OP_LEFT_SECTION;
    node->op_left_section.left   = left_ast;
    node->op_left_section.type   = type;
    node->op_left_section.symbol = symbol;
    node->source_loc             = source_loc;
    node->end_loc                = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_op_right_section(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr right_ast, NECRO_BIN_OP_TYPE type, NecroSymbol symbol)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node        = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                    = NECRO_AST_OP_RIGHT_SECTION;
    node->op_right_section.right  = right_ast;
    node->op_right_section.type   = type;
    node->op_right_section.symbol = symbol;
    node->source_loc              = source_loc;
    node->end_loc                 = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_if_then_else(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr if_ast, NecroParseAstLocalPtr then_ast, NecroParseAstLocalPtr else_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node       = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                   = NECRO_AST_IF_THEN_ELSE;
    node->if_then_else.if_expr   = if_ast;
    node->if_then_else.then_expr = then_ast;
    node->if_then_else.else_expr = else_ast;
    node->source_loc             = source_loc;
    node->end_loc                = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_top_decl(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr item_ast, NecroParseAstLocalPtr next_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node              = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                          = NECRO_AST_TOP_DECL;
    node->top_declaration.declaration   = item_ast;
    node->top_declaration.next_top_decl = next_ast;
    node->source_loc                    = source_loc;
    node->end_loc                       = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_decl(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr item_ast, NecroParseAstLocalPtr next_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_DECL;
    node->declaration.declaration_impl = item_ast;
    node->declaration.next_declaration = next_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_simple_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroParseAstLocalPtr rhs_ast, NecroParseAstLocalPtr initializer_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node                = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                            = NECRO_AST_SIMPLE_ASSIGNMENT;
    node->simple_assignment.variable_name = var_name;
    node->simple_assignment.rhs           = rhs_ast;
    node->simple_assignment.initializer   = initializer_ast;
    node->source_loc                      = source_loc;
    node->end_loc                         = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_apats_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol var_name, NecroParseAstLocalPtr apats_ast, NecroParseAstLocalPtr rhs_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node               = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                           = NECRO_AST_APATS_ASSIGNMENT;
    node->apats_assignment.variable_name = var_name;
    node->apats_assignment.apats         = apats_ast;
    node->apats_assignment.rhs           = rhs_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_pat_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr pat_ast, NecroParseAstLocalPtr rhs_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node   = necro_parse_ast_alloc(arena, &local_ptr);
    node->type               = NECRO_AST_PAT_ASSIGNMENT;
    node->pat_assignment.pat = pat_ast;
    node->pat_assignment.rhs = rhs_ast;
    node->source_loc         = source_loc;
    node->end_loc            = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_rhs(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expression_ast, NecroParseAstLocalPtr declarations_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_RIGHT_HAND_SIDE;
    node->right_hand_side.expression   = expression_ast;
    node->right_hand_side.declarations = declarations_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_let(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expression_ast, NecroParseAstLocalPtr declarations_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_LET_EXPRESSION;
    node->let_expression.expression   = expression_ast;
    node->let_expression.declarations = declarations_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_fexpr(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr aexpr_ast, NecroParseAstLocalPtr expr_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_FUNCTION_EXPRESSION;
    node->fexpression.aexp             = aexpr_ast;
    node->fexpression.next_fexpression = expr_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_var(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_VAR_TYPE type, NecroParseAstLocalPtr initializer_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node     = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                 = NECRO_AST_VARIABLE;
    node->variable.symbol      = symbol;
    node->variable.var_type    = type;
    node->variable.initializer = initializer_ast;
    node->source_loc           = source_loc;
    node->end_loc              = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_apats(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr apat_ast, NecroParseAstLocalPtr next_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_APATS;
    node->apats.apat       = apat_ast;
    node->apats.next_apat  = next_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_wildcard(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_WILDCARD;
    node->undefined._pad   = 0;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_lambda(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr apats_ast, NecroParseAstLocalPtr expr_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node  = necro_parse_ast_alloc(arena, &local_ptr);
    node->type              = NECRO_AST_LAMBDA;
    node->lambda.apats      = apats_ast;
    node->lambda.expression = expr_ast;
    node->source_loc        = source_loc;
    node->end_loc           = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_do(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr statement_list_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_DO;
    node->do_statement.statement_list = statement_list_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_pat_expr(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node               = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                           = NECRO_AST_PAT_EXPRESSION;
    node->pattern_expression.expressions = expressions_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_list(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr item_ast, NecroParseAstLocalPtr next_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_LIST_NODE;
    node->list.item        = item_ast;
    node->list.next_item   = next_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_expression_list(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node            = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                        = NECRO_AST_EXPRESSION_LIST;
    node->expression_list.expressions = expressions_ast;
    node->source_loc                  = source_loc;
    node->end_loc                     = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_expression_array(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_EXPRESSION_ARRAY;
    node->expression_array.expressions = expressions_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_tuple(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expressions_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node  = necro_parse_ast_alloc(arena, &local_ptr);
    node->type              = NECRO_AST_TUPLE;
    node->tuple.expressions = expressions_ast;
    node->source_loc        = source_loc;
    node->end_loc           = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_bind_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NecroParseAstLocalPtr expr_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node              = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                          = NECRO_BIND_ASSIGNMENT;
    node->bind_assignment.variable_name = symbol;
    node->bind_assignment.expression    = expr_ast;
    node->source_loc                    = source_loc;
    node->end_loc                       = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_pat_bind_assignment(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr pat_ast, NecroParseAstLocalPtr expr_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node               = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                           = NECRO_PAT_BIND_ASSIGNMENT;
    node->pat_bind_assignment.pat        = pat_ast;
    node->pat_bind_assignment.expression = expr_ast;
    node->source_loc                     = source_loc;
    node->end_loc                        = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_arithmetic_sequence(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr from_ast, NecroParseAstLocalPtr then_ast, NecroParseAstLocalPtr to_ast, NECRO_ARITHMETIC_SEQUENCE_TYPE type)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node         = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                     = NECRO_AST_ARITHMETIC_SEQUENCE;
    node->arithmetic_sequence.from = from_ast;
    node->arithmetic_sequence.then = then_ast;
    node->arithmetic_sequence.to   = to_ast;
    node->arithmetic_sequence.type = type;
    node->source_loc               = source_loc;
    node->end_loc                  = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_case(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr expr_ast, NecroParseAstLocalPtr alts_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node             = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                         = NECRO_AST_CASE;
    node->case_expression.expression   = expr_ast;
    node->case_expression.alternatives = alts_ast;
    node->source_loc                   = source_loc;
    node->end_loc                      = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_case_alternative(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr pat_ast, NecroParseAstLocalPtr body_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node      = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                  = NECRO_AST_CASE_ALTERNATIVE;
    node->case_alternative.pat  = pat_ast;
    node->case_alternative.body = body_ast;
    node->source_loc            = source_loc;
    node->end_loc               = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_conid(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroSymbol symbol, NECRO_CON_TYPE type)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_CONID;
    node->conid.symbol     = symbol;
    node->conid.con_type   = type;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_type_app(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr atype_ast, NecroParseAstLocalPtr type_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_TYPE_APP;
    node->type_app.ty      = atype_ast;
    node->type_app.next_ty = type_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_bin_op_sym(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr left_ast, NecroParseAstLocalPtr op_ast, NecroParseAstLocalPtr right_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node = necro_parse_ast_alloc(arena, &local_ptr);
    node->type             = NECRO_AST_BIN_OP_SYM;
    node->bin_op_sym.left  = left_ast;
    node->bin_op_sym.op    = op_ast;
    node->bin_op_sym.right = right_ast;
    node->source_loc       = source_loc;
    node->end_loc          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_constructor(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr conid_ast, NecroParseAstLocalPtr arg_list_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node     = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                 = NECRO_AST_CONSTRUCTOR;
    node->constructor.conid    = conid_ast;
    node->constructor.arg_list = arg_list_ast;
    node->source_loc           = source_loc;
    node->end_loc              = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_simple_type(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr type_con_ast, NecroParseAstLocalPtr type_var_list_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node          = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                      = NECRO_AST_SIMPLE_TYPE;
    node->simple_type.type_con      = type_con_ast;
    node->simple_type.type_var_list = type_var_list_ast;
    node->source_loc                = source_loc;
    node->end_loc                   = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_data_declaration(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr simple_type_ast, NecroParseAstLocalPtr constructor_list_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node                  = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                              = NECRO_AST_DATA_DECLARATION;
    node->data_declaration.simpletype       = simple_type_ast;
    node->data_declaration.constructor_list = constructor_list_ast;
    node->source_loc                        = source_loc;
    node->end_loc                           = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_class_context(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr conid_ast, NecroParseAstLocalPtr varid_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node         = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                     = NECRO_AST_TYPE_CLASS_CONTEXT;
    node->type_class_context.conid = conid_ast;
    node->type_class_context.varid = varid_ast;
    node->source_loc               = source_loc;
    node->end_loc                  = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_class_declaration(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr tycls_ast, NecroParseAstLocalPtr tyvar_ast, NecroParseAstLocalPtr declarations_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node                    = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                                = NECRO_AST_TYPE_CLASS_DECLARATION;
    node->type_class_declaration.context      = context_ast;
    node->type_class_declaration.tycls        = tycls_ast;
    node->type_class_declaration.tyvar        = tyvar_ast;
    node->type_class_declaration.declarations = declarations_ast;
    node->source_loc                          = source_loc;
    node->end_loc                             = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_instance(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr tycls_ast, NecroParseAstLocalPtr inst_ast, NecroParseAstLocalPtr declarations_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node                 = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                             = NECRO_AST_TYPE_CLASS_INSTANCE;
    node->type_class_instance.context      = context_ast;
    node->type_class_instance.qtycls       = tycls_ast;
    node->type_class_instance.inst         = inst_ast;
    node->type_class_instance.declarations = declarations_ast;
    node->source_loc                       = source_loc;
    node->end_loc                          = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_type_signature(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr var_ast, NecroParseAstLocalPtr context_ast, NecroParseAstLocalPtr type_ast, NECRO_SIG_TYPE sig_type)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node        = necro_parse_ast_alloc(arena, &local_ptr);
    node->type                    = NECRO_AST_TYPE_SIGNATURE;
    node->type_signature.var      = var_ast;
    node->type_signature.context  = context_ast;
    node->type_signature.type     = type_ast;
    node->type_signature.sig_type = sig_type;
    node->source_loc              = source_loc;
    node->end_loc                 = end_loc;
    return local_ptr;
}

NecroParseAstLocalPtr necro_parse_ast_create_function_type(NecroArena* arena, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroParseAstLocalPtr lhs_ast, NecroParseAstLocalPtr rhs_ast)
{
    NecroParseAstLocalPtr local_ptr;
    NecroParseAst*    node            = necro_parse_ast_alloc(arena, &local_ptr);
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
void necro_parse_ast_assert_eq_go(NecroParseAstArena* ast1, NecroParseAstLocalPtr ptr1, NecroParseAstArena* ast2, NecroParseAstLocalPtr ptr2);

void necro_parse_ast_assert_eq_constant(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
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
    {
        const char* str1 = node1->constant.symbol->str;
        const char* str2 = node2->constant.symbol->str;
        assert(strcmp(str1, str2) == 0);
        break;
    }
    }
    UNUSED(ast1);
    UNUSED(ast2);
}

void necro_parse_ast_assert_eq_bin_op(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_BIN_OP);
    assert(node2->type == NECRO_AST_BIN_OP);
    assert(node1->bin_op.type == node2->bin_op.type);
    necro_parse_ast_assert_eq_go(ast1, node1->bin_op.lhs, ast2, node2->bin_op.lhs);
    necro_parse_ast_assert_eq_go(ast1, node1->bin_op.rhs, ast2, node2->bin_op.rhs);
    assert(strcmp(node1->bin_op.symbol->str, node2->bin_op.symbol->str) == 0);
}

void necro_parse_ast_assert_eq_if_then_else(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_IF_THEN_ELSE);
    assert(node2->type == NECRO_AST_IF_THEN_ELSE);
    necro_parse_ast_assert_eq_go(ast1, node1->if_then_else.if_expr, ast2, node2->if_then_else.if_expr);
    necro_parse_ast_assert_eq_go(ast1, node1->if_then_else.then_expr, ast2, node2->if_then_else.then_expr);
    necro_parse_ast_assert_eq_go(ast1, node1->if_then_else.else_expr, ast2, node2->if_then_else.else_expr);
}

void necro_parse_ast_assert_eq_top_decl(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TOP_DECL);
    assert(node2->type == NECRO_AST_TOP_DECL);
    necro_parse_ast_assert_eq_go(ast1, node1->top_declaration.declaration, ast2, node2->top_declaration.declaration);
    necro_parse_ast_assert_eq_go(ast1, node1->top_declaration.next_top_decl, ast2, node2->top_declaration.next_top_decl);
}

void necro_parse_ast_assert_eq_decl(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_DECL);
    assert(node2->type == NECRO_AST_DECL);
    necro_parse_ast_assert_eq_go(ast1, node1->declaration.declaration_impl, ast2, node2->declaration.declaration_impl);
    necro_parse_ast_assert_eq_go(ast1, node1->declaration.next_declaration, ast2, node2->declaration.next_declaration);
}

void necro_parse_ast_assert_eq_simple_assignment(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    assert(node2->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    ASSERT_BREAK(strcmp(node1->simple_assignment.variable_name->str, node2->simple_assignment.variable_name->str) == 0);
    necro_parse_ast_assert_eq_go(ast1, node1->simple_assignment.initializer, ast2, node2->simple_assignment.initializer);
    necro_parse_ast_assert_eq_go(ast1, node1->simple_assignment.rhs, ast2, node2->simple_assignment.rhs);
}

void necro_parse_ast_assert_eq_apats_assignment(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_APATS_ASSIGNMENT);
    assert(node2->type == NECRO_AST_APATS_ASSIGNMENT);
    assert(strcmp(node1->apats_assignment.variable_name->str, node2->apats_assignment.variable_name->str) == 0);
    necro_parse_ast_assert_eq_go(ast1, node1->apats_assignment.apats, ast2, node2->apats_assignment.apats);
    necro_parse_ast_assert_eq_go(ast1, node1->apats_assignment.rhs, ast2, node2->apats_assignment.rhs);
}

void necro_parse_ast_assert_eq_pat_assignment(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_PAT_ASSIGNMENT);
    assert(node2->type == NECRO_AST_PAT_ASSIGNMENT);
    necro_parse_ast_assert_eq_go(ast1, node1->pat_assignment.pat, ast2, node2->pat_assignment.pat);
    necro_parse_ast_assert_eq_go(ast1, node1->pat_assignment.rhs, ast2, node2->pat_assignment.rhs);
}

void necro_parse_ast_assert_eq_rhs(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_RIGHT_HAND_SIDE);
    assert(node2->type == NECRO_AST_RIGHT_HAND_SIDE);
    necro_parse_ast_assert_eq_go(ast1, node1->right_hand_side.expression, ast2, node2->right_hand_side.expression);
    necro_parse_ast_assert_eq_go(ast1, node1->right_hand_side.declarations, ast2, node2->right_hand_side.declarations);
}

void necro_parse_ast_assert_eq_let(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_LET_EXPRESSION);
    assert(node2->type == NECRO_AST_LET_EXPRESSION);
    necro_parse_ast_assert_eq_go(ast1, node1->let_expression.expression, ast2, node2->let_expression.expression);
    necro_parse_ast_assert_eq_go(ast1, node1->let_expression.declarations, ast2, node2->let_expression.declarations);
}

void necro_parse_ast_assert_eq_fexpr(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_FUNCTION_EXPRESSION);
    assert(node2->type == NECRO_AST_FUNCTION_EXPRESSION);
    necro_parse_ast_assert_eq_go(ast1, node1->fexpression.aexp, ast2, node2->fexpression.aexp);
    necro_parse_ast_assert_eq_go(ast1, node1->fexpression.next_fexpression, ast2, node2->fexpression.next_fexpression);
}

void necro_parse_ast_assert_eq_var(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_VARIABLE);
    assert(node2->type == NECRO_AST_VARIABLE);
    //ASSERT_BREAK(strcmp(node1->variable.symbol->str, node2->variable.symbol->str) == 0);
    necro_parse_ast_assert_eq_go(ast1, node1->variable.initializer, ast2, node2->variable.initializer);
    assert(node2->variable.var_type == node2->variable.var_type);
}

void necro_parse_ast_assert_eq_apats(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_APATS);
    assert(node2->type == NECRO_AST_APATS);
    necro_parse_ast_assert_eq_go(ast1, node1->apats.apat, ast2, node2->apats.apat);
    necro_parse_ast_assert_eq_go(ast1, node1->apats.next_apat, ast2, node2->apats.next_apat);
}

void necro_parse_ast_assert_eq_lambda(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_LAMBDA);
    assert(node2->type == NECRO_AST_LAMBDA);
    necro_parse_ast_assert_eq_go(ast1, node1->lambda.apats, ast2, node2->lambda.apats);
    necro_parse_ast_assert_eq_go(ast1, node1->lambda.expression, ast2, node2->lambda.expression);
}

void necro_parse_ast_assert_eq_do(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_DO);
    assert(node2->type == NECRO_AST_DO);
    necro_parse_ast_assert_eq_go(ast1, node1->do_statement.statement_list, ast2, node2->do_statement.statement_list);
}

void necro_parse_ast_assert_eq_pat_expression(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_PAT_EXPRESSION);
    assert(node2->type == NECRO_AST_PAT_EXPRESSION);
    necro_parse_ast_assert_eq_go(ast1, node1->pattern_expression.expressions, ast2, node2->pattern_expression.expressions);
}

void necro_parse_ast_assert_eq_list(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_LIST_NODE);
    assert(node2->type == NECRO_AST_LIST_NODE);
    necro_parse_ast_assert_eq_go(ast1, node1->list.item, ast2, node2->list.item);
    necro_parse_ast_assert_eq_go(ast1, node1->list.next_item, ast2, node2->list.next_item);
}

void necro_parse_ast_assert_eq_expression_list(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_EXPRESSION_LIST);
    assert(node2->type == NECRO_AST_EXPRESSION_LIST);
    necro_parse_ast_assert_eq_go(ast1, node1->expression_list.expressions, ast2, node2->expression_list.expressions);
}

void necro_parse_ast_assert_eq_expression_array(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_EXPRESSION_ARRAY);
    assert(node2->type == NECRO_AST_EXPRESSION_ARRAY);
    necro_parse_ast_assert_eq_go(ast1, node1->expression_array.expressions, ast2, node2->expression_array.expressions);
}

void necro_parse_ast_assert_eq_tuple(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TUPLE);
    assert(node2->type == NECRO_AST_TUPLE);
    necro_parse_ast_assert_eq_go(ast1, node1->tuple.expressions, ast2, node2->tuple.expressions);
}

void necro_parse_ast_assert_eq_bind_assignment(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_BIND_ASSIGNMENT);
    assert(node2->type == NECRO_BIND_ASSIGNMENT);
    assert(strcmp(node1->bind_assignment.variable_name->str, node2->bind_assignment.variable_name->str) == 0);
    necro_parse_ast_assert_eq_go(ast1, node1->bind_assignment.expression, ast2, node2->bind_assignment.expression);
}

void necro_parse_ast_assert_eq_pat_bind_assignment(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_PAT_BIND_ASSIGNMENT);
    assert(node2->type == NECRO_PAT_BIND_ASSIGNMENT);
    necro_parse_ast_assert_eq_go(ast1, node1->pat_bind_assignment.pat, ast2, node2->pat_bind_assignment.pat);
    necro_parse_ast_assert_eq_go(ast1, node1->pat_bind_assignment.expression, ast2, node2->pat_bind_assignment.expression);
}

void necro_parse_ast_assert_eq_arithmetic_sequence(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    assert(node2->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    necro_parse_ast_assert_eq_go(ast1, node1->arithmetic_sequence.from, ast2, node2->arithmetic_sequence.from);
    necro_parse_ast_assert_eq_go(ast1, node1->arithmetic_sequence.then, ast2, node2->arithmetic_sequence.then);
    necro_parse_ast_assert_eq_go(ast1, node1->arithmetic_sequence.to, ast2, node2->arithmetic_sequence.to);
}

void necro_parse_ast_assert_eq_case(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_CASE);
    assert(node2->type == NECRO_AST_CASE);
    necro_parse_ast_assert_eq_go(ast1, node1->case_expression.expression, ast2, node2->case_expression.expression);
    necro_parse_ast_assert_eq_go(ast1, node1->case_expression.alternatives, ast2, node2->case_expression.alternatives);
}

void necro_parse_ast_assert_eq_case_alternative(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_CASE_ALTERNATIVE);
    assert(node2->type == NECRO_AST_CASE_ALTERNATIVE);
    necro_parse_ast_assert_eq_go(ast1, node1->case_alternative.pat, ast2, node2->case_alternative.pat);
    necro_parse_ast_assert_eq_go(ast1, node1->case_alternative.body, ast2, node2->case_alternative.body);
}

void necro_parse_ast_assert_eq_conid(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_CONID);
    assert(node2->type == NECRO_AST_CONID);
    assert(strcmp(node1->conid.symbol->str, node2->conid.symbol->str) == 0);
    assert(node1->conid.con_type == node2->conid.con_type);
    UNUSED(ast1);
    UNUSED(ast2);
}

void necro_parse_ast_assert_eq_type_app(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TYPE_APP);
    assert(node2->type == NECRO_AST_TYPE_APP);
    necro_parse_ast_assert_eq_go(ast1, node1->type_app.ty, ast2, node2->type_app.ty);
    necro_parse_ast_assert_eq_go(ast1, node1->type_app.next_ty, ast2, node2->type_app.next_ty);
}

void necro_parse_ast_assert_eq_bin_op_sym(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_BIN_OP_SYM);
    assert(node2->type == NECRO_AST_BIN_OP_SYM);
    necro_parse_ast_assert_eq_go(ast1, node1->bin_op_sym.left, ast2, node2->bin_op_sym.left);
    necro_parse_ast_assert_eq_go(ast1, node1->bin_op_sym.op, ast2, node2->bin_op_sym.op);
    necro_parse_ast_assert_eq_go(ast1, node1->bin_op_sym.right, ast2, node2->bin_op_sym.right);
}

void necro_parse_ast_assert_eq_left_section(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_OP_LEFT_SECTION);
    assert(node2->type == NECRO_AST_OP_LEFT_SECTION);
    assert(strcmp(node1->op_left_section.symbol->str, node2->op_left_section.symbol->str) == 0);
    necro_parse_ast_assert_eq_go(ast1, node1->op_left_section.left, ast2, node2->op_left_section.left);
}

void necro_parse_ast_assert_eq_right_section(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_OP_RIGHT_SECTION);
    assert(node2->type == NECRO_AST_OP_RIGHT_SECTION);
    assert(strcmp(node1->op_right_section.symbol->str, node2->op_right_section.symbol->str) == 0);
    necro_parse_ast_assert_eq_go(ast1, node1->op_right_section.right, ast2, node2->op_right_section.right);
}

void necro_parse_ast_assert_eq_constructor(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_CONSTRUCTOR);
    assert(node2->type == NECRO_AST_CONSTRUCTOR);
    necro_parse_ast_assert_eq_go(ast1, node1->constructor.conid, ast2, node2->constructor.conid);
    necro_parse_ast_assert_eq_go(ast1, node1->constructor.arg_list, ast2, node2->constructor.arg_list);
}

void necro_parse_ast_assert_eq_simple_type(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_SIMPLE_TYPE);
    assert(node2->type == NECRO_AST_SIMPLE_TYPE);
    necro_parse_ast_assert_eq_go(ast1, node1->simple_type.type_con, ast2, node2->simple_type.type_con);
    necro_parse_ast_assert_eq_go(ast1, node1->simple_type.type_var_list, ast2, node2->simple_type.type_var_list);
}

void necro_parse_ast_assert_eq_data_declaration(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_DATA_DECLARATION);
    assert(node2->type == NECRO_AST_DATA_DECLARATION);
    necro_parse_ast_assert_eq_go(ast1, node1->data_declaration.simpletype, ast2, node2->data_declaration.simpletype);
    necro_parse_ast_assert_eq_go(ast1, node1->data_declaration.constructor_list, ast2, node2->data_declaration.constructor_list);
}

void necro_parse_ast_assert_eq_type_class_context(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TYPE_CLASS_CONTEXT);
    assert(node2->type == NECRO_AST_TYPE_CLASS_CONTEXT);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_context.conid, ast2, node2->type_class_context.conid);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_context.varid, ast2, node2->type_class_context.varid);
}

void necro_parse_ast_assert_eq_type_class_declaration(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    assert(node2->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_declaration.context, ast2, node2->type_class_declaration.context);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_declaration.tycls, ast2, node2->type_class_declaration.tycls);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_declaration.tyvar, ast2, node2->type_class_declaration.tyvar);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_declaration.declarations, ast2, node2->type_class_declaration.declarations);
}

void necro_parse_ast_assert_eq_type_class_instance(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    assert(node2->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_instance.context, ast2, node2->type_class_instance.context);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_instance.qtycls, ast2, node2->type_class_instance.qtycls);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_instance.inst, ast2, node2->type_class_instance.inst);
    necro_parse_ast_assert_eq_go(ast1, node1->type_class_instance.declarations, ast2, node2->type_class_instance.declarations);
}

void necro_parse_ast_assert_eq_type_signature(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_TYPE_SIGNATURE);
    assert(node2->type == NECRO_AST_TYPE_SIGNATURE);
    necro_parse_ast_assert_eq_go(ast1, node1->type_signature.var, ast2, node2->type_signature.var);
    necro_parse_ast_assert_eq_go(ast1, node1->type_signature.context, ast2, node2->type_signature.context);
    necro_parse_ast_assert_eq_go(ast1, node1->type_signature.type, ast2, node2->type_signature.type);
    assert(node1->type_signature.sig_type == node2->type_signature.sig_type);
}

void necro_parse_ast_assert_eq_function_type(NecroParseAstArena* ast1, NecroParseAst* node1, NecroParseAstArena* ast2, NecroParseAst* node2)
{
    assert(node1->type == NECRO_AST_FUNCTION_TYPE);
    assert(node2->type == NECRO_AST_FUNCTION_TYPE);
    necro_parse_ast_assert_eq_go(ast1, node1->function_type.type, ast2, node2->function_type.type);
    necro_parse_ast_assert_eq_go(ast1, node1->function_type.next_on_arrow, ast2, node2->function_type.next_on_arrow);
}

void necro_parse_ast_assert_eq_go(NecroParseAstArena* ast1, NecroParseAstLocalPtr ptr1, NecroParseAstArena* ast2, NecroParseAstLocalPtr ptr2)
{
    if (ptr1 == null_local_ptr && ptr2 == null_local_ptr)
        return;
    assert(ptr1 != null_local_ptr);
    assert(ptr2 != null_local_ptr);
    NecroParseAst* node1 = necro_parse_ast_get_node(ast1, ptr1);
    NecroParseAst* node2 = necro_parse_ast_get_node(ast2, ptr2);
    assert(node1->type == node2->type);
    switch (node1->type)
    {
    // case NECRO_AST_UN_OP:                  necro_parse_ast_assert_eq_var(intern1, ast1, node1, ast2, node2); break;
    case NECRO_AST_CONSTANT:               necro_parse_ast_assert_eq_constant(ast1, node1, ast2, node2); break;
    case NECRO_AST_BIN_OP:                 necro_parse_ast_assert_eq_bin_op(ast1, node1, ast2, node2); break;
    case NECRO_AST_IF_THEN_ELSE:           necro_parse_ast_assert_eq_if_then_else(ast1, node1, ast2, node2); break;
    case NECRO_AST_TOP_DECL:               necro_parse_ast_assert_eq_top_decl(ast1, node1, ast2, node2); break;
    case NECRO_AST_DECL:                   necro_parse_ast_assert_eq_decl(ast1, node1, ast2, node2); break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:      necro_parse_ast_assert_eq_simple_assignment(ast1, node1, ast2, node2); break;
    case NECRO_AST_APATS_ASSIGNMENT:       necro_parse_ast_assert_eq_apats_assignment(ast1, node1, ast2, node2); break;
    case NECRO_AST_PAT_ASSIGNMENT:         necro_parse_ast_assert_eq_pat_assignment(ast1, node1, ast2, node2); break;
    case NECRO_AST_RIGHT_HAND_SIDE:        necro_parse_ast_assert_eq_rhs(ast1, node1, ast2, node2); break;
    case NECRO_AST_LET_EXPRESSION:         necro_parse_ast_assert_eq_let(ast1, node1, ast2, node2); break;
    case NECRO_AST_FUNCTION_EXPRESSION:    necro_parse_ast_assert_eq_fexpr(ast1, node1, ast2, node2); break;
    case NECRO_AST_VARIABLE:               necro_parse_ast_assert_eq_var(ast1, node1, ast2, node2); break;
    case NECRO_AST_APATS:                  necro_parse_ast_assert_eq_apats(ast1, node1, ast2, node2); break;
    case NECRO_AST_WILDCARD:               break;
    case NECRO_AST_LAMBDA:                 necro_parse_ast_assert_eq_lambda(ast1, node1, ast2, node2); break;
    case NECRO_AST_DO:                     necro_parse_ast_assert_eq_do(ast1, node1, ast2, node2); break;
    case NECRO_AST_PAT_EXPRESSION:         necro_parse_ast_assert_eq_pat_expression(ast1, node1, ast2, node2); break;
    case NECRO_AST_LIST_NODE:              necro_parse_ast_assert_eq_list(ast1, node1, ast2, node2); break;
    case NECRO_AST_EXPRESSION_LIST:        necro_parse_ast_assert_eq_expression_list(ast1, node1, ast2, node2); break;
    case NECRO_AST_EXPRESSION_ARRAY:       necro_parse_ast_assert_eq_expression_array(ast1, node1, ast2, node2); break;
    case NECRO_AST_TUPLE:                  necro_parse_ast_assert_eq_tuple(ast1, node1, ast2, node2); break;
    case NECRO_BIND_ASSIGNMENT:            necro_parse_ast_assert_eq_bind_assignment(ast1, node1, ast2, node2); break;
    case NECRO_PAT_BIND_ASSIGNMENT:        necro_parse_ast_assert_eq_pat_bind_assignment(ast1, node1, ast2, node2); break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:    necro_parse_ast_assert_eq_arithmetic_sequence(ast1, node1, ast2, node2); break;
    case NECRO_AST_CASE:                   necro_parse_ast_assert_eq_case(ast1, node1, ast2, node2); break;
    case NECRO_AST_CASE_ALTERNATIVE:       necro_parse_ast_assert_eq_case_alternative(ast1, node1, ast2, node2); break;
    case NECRO_AST_CONID:                  necro_parse_ast_assert_eq_conid(ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_APP:               necro_parse_ast_assert_eq_type_app(ast1, node1, ast2, node2); break;
    case NECRO_AST_BIN_OP_SYM:             necro_parse_ast_assert_eq_bin_op_sym(ast1, node1, ast2, node2); break;
    case NECRO_AST_OP_LEFT_SECTION:        necro_parse_ast_assert_eq_left_section(ast1, node1, ast2, node2); break;
    case NECRO_AST_OP_RIGHT_SECTION:       necro_parse_ast_assert_eq_right_section(ast1, node1, ast2, node2); break;
    case NECRO_AST_CONSTRUCTOR:            necro_parse_ast_assert_eq_constructor(ast1, node1, ast2, node2); break;
    case NECRO_AST_SIMPLE_TYPE:            necro_parse_ast_assert_eq_simple_type(ast1, node1, ast2, node2); break;
    case NECRO_AST_DATA_DECLARATION:       necro_parse_ast_assert_eq_data_declaration(ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:     necro_parse_ast_assert_eq_type_class_context(ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_CLASS_DECLARATION: necro_parse_ast_assert_eq_type_class_declaration(ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:    necro_parse_ast_assert_eq_type_class_instance(ast1, node1, ast2, node2); break;
    case NECRO_AST_TYPE_SIGNATURE:         necro_parse_ast_assert_eq_type_signature(ast1, node1, ast2, node2); break;
    case NECRO_AST_FUNCTION_TYPE:          necro_parse_ast_assert_eq_function_type(ast1, node1, ast2, node2); break;
    default:
        assert(false);
        return;
    }
}

void necro_parse_ast_assert_eq(NecroParseAstArena* ast1, NecroParseAstArena* ast2)
{
    necro_parse_ast_assert_eq_go(ast1, ast1->root, ast2, ast2->root);
}

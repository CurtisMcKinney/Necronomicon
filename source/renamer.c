/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "parser.h"
#include "renamer.h"

// TODO: Don't do error in checking, be more naive and kick the can to type checking?
// Or 1 pass to insert names, 1 pass to lookup names?

//=====================================================
// Logic
//=====================================================
NecroAST_Node_Reified* rename_ast_go(NecroAST_Node_Reified* input_node, NecroRenamer* renamer)
{
    if (input_node == NULL || renamer->error.return_code != NECRO_SUCCESS)
        return NULL;
    NecroAST_Node_Reified* result = necro_paged_arena_alloc(&renamer->ast.arena, sizeof(NecroAST_Node_Reified));
    *result = *input_node;
    switch (result->type)
    {

    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;

    case NECRO_AST_BIN_OP:
    {
        result->bin_op.lhs = rename_ast_go(input_node->bin_op.lhs, renamer);
        result->bin_op.rhs = rename_ast_go(input_node->bin_op.rhs, renamer);
        break;
    }

    case NECRO_AST_IF_THEN_ELSE:
    {
        result->if_then_else.if_expr   = rename_ast_go(input_node->if_then_else.if_expr, renamer);
        result->if_then_else.then_expr = rename_ast_go(input_node->if_then_else.then_expr, renamer);
        result->if_then_else.else_expr = rename_ast_go(input_node->if_then_else.else_expr, renamer);
        break;
    }

    case NECRO_AST_TOP_DECL:
    {
        result->top_declaration.declaration   = rename_ast_go(input_node->top_declaration.declaration, renamer);
        result->top_declaration.next_top_decl = rename_ast_go(input_node->top_declaration.next_top_decl, renamer);
        break;
    }

    case NECRO_AST_DECL:
    {
        result->declaration.declaration_impl = rename_ast_go(input_node->declaration.declaration_impl, renamer);
        result->declaration.next_declaration = rename_ast_go(input_node->declaration.next_declaration, renamer);
        break;
    }

    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        if (necro_scoped_symtable_current_scope_find(&renamer->scoped_symtable, input_node->simple_assignment.variable_name).id != 0)
        {
            necro_error(&renamer->error, input_node->source_loc, "Conflicting definitions for \'%s\' in simple assignment", necro_intern_get_string(renamer->scoped_symtable.global_table->intern, input_node->simple_assignment.variable_name));
            return result;
        }
        result->simple_assignment.variable_name = input_node->simple_assignment.variable_name;
        result->simple_assignment.id            = necro_scoped_symtable_new_symbol_info(&renamer->scoped_symtable, necro_create_initial_symbol_info(result->simple_assignment.variable_name, result->source_loc));
        necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
        result->simple_assignment.rhs = rename_ast_go(input_node->simple_assignment.rhs, renamer);
        necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        if (necro_scoped_symtable_current_scope_find(&renamer->scoped_symtable, input_node->apats_assignment.variable_name).id != 0)
        {
            necro_error(&renamer->error, input_node->source_loc, "Conflicting definitions for \'%s\' in apats assignment", necro_intern_get_string(renamer->scoped_symtable.global_table->intern, input_node->apats_assignment.variable_name));
            return result;
        }
        result->apats_assignment.variable_name = input_node->apats_assignment.variable_name;
        result->apats_assignment.id            = necro_scoped_symtable_new_symbol_info(&renamer->scoped_symtable, necro_create_initial_symbol_info(result->apats_assignment.variable_name, result->source_loc));
        necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
        renamer->mode                          = NECRO_RENAME_PATTERN;
        result->apats_assignment.apats         = rename_ast_go(input_node->apats_assignment.apats, renamer);
        renamer->mode                          = NECRO_RENAME_NORMAL;
        result->apats_assignment.rhs           = rename_ast_go(input_node->apats_assignment.rhs, renamer);
        necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;
    }

    case NECRO_AST_RIGHT_HAND_SIDE:
    {
        result->right_hand_side.declarations = rename_ast_go(input_node->right_hand_side.declarations, renamer);
        result->right_hand_side.expression   = rename_ast_go(input_node->right_hand_side.expression, renamer);
        break;
    }

    case NECRO_AST_LET_EXPRESSION:
    {
        necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
        result->let_expression.declarations = rename_ast_go(input_node->let_expression.declarations, renamer);
        result->let_expression.expression   = rename_ast_go(input_node->let_expression.expression, renamer);
        necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;
    }

    case NECRO_AST_FUNCTION_EXPRESSION:
    {
        result->fexpression.aexp             = rename_ast_go(input_node->fexpression.aexp, renamer);
        result->fexpression.next_fexpression = rename_ast_go(input_node->fexpression.next_fexpression, renamer);
        break;
    }

    case NECRO_AST_VARIABLE:
    {
        result->variable.variable_id   = input_node->variable.variable_id;
        result->variable.variable_type = input_node->variable.variable_type;
        if (result->variable.variable_type == NECRO_AST_VARIABLE_ID)
        {
            if (renamer->mode == NECRO_RENAME_NORMAL)
            {
                result->variable.id = necro_scoped_symtable_find(&renamer->scoped_symtable, result->variable.variable_id);
                if (result->variable.id.id == 0)
                {
                    necro_error(&renamer->error, result->source_loc, "Variable not in scope: %s", necro_intern_get_string(renamer->scoped_symtable.global_table->intern, result->variable.variable_id));
                }
            }
            else if (renamer->mode == NECRO_RENAME_PATTERN)
            {
                result->variable.id = necro_scoped_symtable_new_symbol_info(&renamer->scoped_symtable, necro_create_initial_symbol_info(result->variable.variable_id, result->source_loc));
            }
        }
        break;
    }

    case NECRO_AST_APATS:
    {
        result->apats.apat      = rename_ast_go(input_node->apats.apat, renamer);
        result->apats.next_apat = rename_ast_go(input_node->apats.next_apat, renamer);
        break;
    }

    case NECRO_AST_WILDCARD:
        break;

    case NECRO_AST_LAMBDA:
    {
        necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
        result->lambda.apats      = rename_ast_go(input_node->lambda.apats, renamer);
        result->lambda.expression = rename_ast_go(input_node->lambda.expression, renamer);
        necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;
    }

    case NECRO_AST_DO:
    {
        necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
        result->do_statement.statement_list = rename_ast_go(input_node->do_statement.statement_list, renamer);
        necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;
    }

    case NECRO_AST_LIST_NODE:
    {
        result->list.item      = rename_ast_go(input_node->list.item, renamer);
        result->list.next_item = rename_ast_go(input_node->list.next_item, renamer);
        break;
    }

    case NECRO_AST_EXPRESSION_LIST:
    {
        result->expression_list.expressions = rename_ast_go(input_node->expression_list.expressions, renamer);
        break;
    }

    case NECRO_AST_TUPLE:
    {
        result->tuple.expressions = rename_ast_go(input_node->tuple.expressions, renamer);
        break;
    }

    case NECRO_BIND_ASSIGNMENT:
    {
        if (necro_scoped_symtable_current_scope_find(&renamer->scoped_symtable, input_node->bind_assignment.variable_name).id != 0)
        {
            necro_error(&renamer->error, input_node->source_loc, "Conflicting definitions for \'%s\' in bind assignment", necro_intern_get_string(renamer->scoped_symtable.global_table->intern, input_node->bind_assignment.variable_name));
            return result;
        }
        result->bind_assignment.variable_name = input_node->bind_assignment.variable_name;
        result->bind_assignment.id            = necro_scoped_symtable_new_symbol_info(&renamer->scoped_symtable, necro_create_initial_symbol_info(result->bind_assignment.variable_name, result->source_loc));
        result->bind_assignment.expression    = rename_ast_go(input_node->bind_assignment.expression, renamer);
        break;
    }

    case NECRO_AST_ARITHMETIC_SEQUENCE:
    {
        result->arithmetic_sequence.from = rename_ast_go(input_node->arithmetic_sequence.from, renamer);
        result->arithmetic_sequence.then = rename_ast_go(input_node->arithmetic_sequence.then, renamer);
        result->arithmetic_sequence.to   = rename_ast_go(input_node->arithmetic_sequence.to, renamer);
        result->arithmetic_sequence.type = input_node->arithmetic_sequence.type;
        break;
    }

    case NECRO_AST_CASE:
        if (input_node->case_expression.declarations != NULL)
        {
            necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
            result->case_expression.declarations = rename_ast_go(input_node->case_expression.declarations, renamer);
        }
        result->case_expression.expression   = rename_ast_go(input_node->case_expression.expression, renamer);
        result->case_expression.alternatives = rename_ast_go(input_node->case_expression.alternatives, renamer);
        if (result->case_expression.declarations != NULL)
            necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        necro_scoped_symtable_new_scope(&renamer->scoped_symtable);
        renamer->mode                 = NECRO_RENAME_PATTERN;
        result->case_alternative.pat  = rename_ast_go(input_node->case_alternative.pat, renamer);
        renamer->mode                 = NECRO_RENAME_NORMAL;
        result->case_alternative.body = rename_ast_go(input_node->case_alternative.body, renamer);
        necro_scoped_symtable_pop_scope(&renamer->scoped_symtable);
        break;

    default:
        necro_error(&renamer->error, input_node->source_loc, "Unrecognized AST Node type found: %d", input_node->type);
        break;
    }
    return result;
}

NecroRenamer necro_create_renamer(NecroSymTable* symtable)
{
    NecroScopedSymTable   scoped_symtable = necro_create_scoped_symtable(symtable);
    NecroAST_Reified      ast             = necro_create_reified_ast();
    return (NecroRenamer)
    {
        .ast             = ast,
        .scoped_symtable = scoped_symtable,
        .mode            = NECRO_RENAME_NORMAL,
    };
}

void necro_destroy_renamer(NecroRenamer* renamer)
{
    necro_destroy_reified_ast(&renamer->ast);
    necro_destroy_scoped_symtable(&renamer->scoped_symtable);
}

NECRO_RETURN_CODE necro_rename_ast(NecroRenamer* renamer, NecroAST_Reified* input_ast)
{
    renamer->ast.root = rename_ast_go(input_ast->root, renamer);
    return renamer->error.return_code;
}
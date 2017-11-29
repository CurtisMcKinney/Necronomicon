/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "renamer.h"

typedef struct
{
    union
    {
        NecroError             error;
        NecroAST_Node_Reified* node;
    };
    NECRO_RENAME_RESULT_TYPE type;
} NecroRenameNodeResult;

// TODO: Name already exists in scope error!!!!!!
NecroRenameNodeResult rename_ast_go(NecroAST_Node_Reified* input_node, NecroPagedArena* arena, NecroScopedSymTable* scoped_symtable)
{
    // if (input_node == NULL)
        // return (NecroRenameNodeResult) { .error = (NecroError) { .error_message = "Found NULL node during renaming phase.", .line = 0, .character = 0 }, .type = NECRO_RENAME_ERROR };
    NecroRenameNodeResult result = { .node = NULL, .type = NECRO_RENAME_SUCCESSFUL};
    if (input_node == NULL)
        return result;
    result.node = necro_paged_arena_alloc(arena, sizeof(NecroAST_Node_Reified));
    *result.node = *input_node;
    switch (result.node->type)
    {

    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;

    case NECRO_AST_BIN_OP:
    {
        NecroRenameNodeResult left_result = rename_ast_go(input_node->bin_op.lhs, arena, scoped_symtable);
        if (left_result.type == NECRO_RENAME_ERROR)
            return left_result;
        NecroRenameNodeResult right_result = rename_ast_go(input_node->bin_op.rhs, arena, scoped_symtable);
        if (right_result.type == NECRO_RENAME_ERROR)
            return right_result;
        result.node->bin_op.lhs = left_result.node;
        result.node->bin_op.rhs = right_result.node;
        break;
    }

    case NECRO_AST_IF_THEN_ELSE:
    {
        NecroRenameNodeResult if_result = rename_ast_go(input_node->if_then_else.if_expr, arena, scoped_symtable);
        if (if_result.type == NECRO_RENAME_ERROR)
            return if_result;
        NecroRenameNodeResult then_result = rename_ast_go(input_node->if_then_else.then_expr, arena, scoped_symtable);
        if (then_result.type == NECRO_RENAME_ERROR)
            return then_result;
        NecroRenameNodeResult else_result = rename_ast_go(input_node->if_then_else.else_expr, arena, scoped_symtable);
        if (else_result.type == NECRO_RENAME_ERROR)
            return else_result;
        result.node->if_then_else.if_expr   = if_result.node;
        result.node->if_then_else.then_expr = then_result.node;
        result.node->if_then_else.else_expr = else_result.node;
        break;
    }

    case NECRO_AST_TOP_DECL:
    {
        NecroRenameNodeResult declaration_result = rename_ast_go(input_node->top_declaration.declaration, arena, scoped_symtable);
        if (declaration_result.type == NECRO_RENAME_ERROR)
            return declaration_result;
        NecroRenameNodeResult next_result = rename_ast_go(input_node->top_declaration.next_top_decl, arena, scoped_symtable);
        if (next_result.type == NECRO_RENAME_ERROR)
            return next_result;
        result.node->top_declaration.declaration   = declaration_result.node;
        result.node->top_declaration.next_top_decl = next_result.node;
        break;
    }

    case NECRO_AST_DECL:
    {
        NecroRenameNodeResult declaration_result = rename_ast_go(input_node->declaration.declaration_impl, arena, scoped_symtable);
        if (declaration_result.type == NECRO_RENAME_ERROR)
            return declaration_result;
        NecroRenameNodeResult next_result = rename_ast_go(input_node->declaration.next_declaration, arena, scoped_symtable);
        if (next_result.type == NECRO_RENAME_ERROR)
            return next_result;
        result.node->declaration.declaration_impl = declaration_result.node;
        result.node->declaration.next_declaration = next_result.node;
        break;
    }

    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        result.node->simple_assignment.variable_name = input_node->simple_assignment.variable_name;
        result.node->simple_assignment.id            = necro_scoped_symtable_new_symbol_info(scoped_symtable, necro_create_initial_symbol_info(result.node->simple_assignment.variable_name));
        necro_scoped_symtable_new_scope(scoped_symtable);
        NecroRenameNodeResult rhs_result = rename_ast_go(input_node->simple_assignment.rhs, arena, scoped_symtable);
        if (rhs_result.type == NECRO_RENAME_ERROR)
            return rhs_result;
        result.node->simple_assignment.rhs = rhs_result.node;
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        result.node->apats_assignment.variable_name = input_node->apats_assignment.variable_name;
        result.node->apats_assignment.id            = necro_scoped_symtable_new_symbol_info(scoped_symtable, necro_create_initial_symbol_info(result.node->apats_assignment.variable_name));
        necro_scoped_symtable_new_scope(scoped_symtable);
        NecroRenameNodeResult apats_result = rename_ast_go(input_node->apats_assignment.apats, arena, scoped_symtable);
        if (apats_result.type == NECRO_RENAME_ERROR)
            return apats_result;
        result.node->apats_assignment.apats = apats_result.node;
        NecroRenameNodeResult rhs_result = rename_ast_go(input_node->apats_assignment.rhs, arena, scoped_symtable);
        if (rhs_result.type == NECRO_RENAME_ERROR)
            return rhs_result;
        result.node->apats_assignment.rhs = rhs_result.node;
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    }

    case NECRO_AST_RIGHT_HAND_SIDE:
    {
        NecroRenameNodeResult exp_result = rename_ast_go(input_node->right_hand_side.expression, arena, scoped_symtable);
        if (exp_result.type == NECRO_RENAME_ERROR)
            return exp_result;
        result.node->right_hand_side.expression = exp_result.node;
        NecroRenameNodeResult decl_result = rename_ast_go(input_node->right_hand_side.declarations, arena, scoped_symtable);
        if (decl_result.type == NECRO_RENAME_ERROR)
            return decl_result;
        result.node->right_hand_side.declarations = decl_result.node;
        break;
    }

    case NECRO_AST_LET_EXPRESSION:
    {
        necro_scoped_symtable_new_scope(scoped_symtable);
        NecroRenameNodeResult exp_result = rename_ast_go(input_node->let_expression.expression, arena, scoped_symtable);
        if (exp_result.type == NECRO_RENAME_ERROR)
            return exp_result;
        result.node->let_expression.expression = exp_result.node;
        NecroRenameNodeResult rhs_result = rename_ast_go(input_node->let_expression.declarations, arena, scoped_symtable);
        if (rhs_result.type == NECRO_RENAME_ERROR)
            return rhs_result;
        result.node->let_expression.declarations = rhs_result.node;
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    }

    case NECRO_AST_FUNCTION_EXPRESSION:
    {
        NecroRenameNodeResult exp_result = rename_ast_go(input_node->fexpression.aexp, arena, scoped_symtable);
        if (exp_result.type == NECRO_RENAME_ERROR)
            return exp_result;
        result.node->fexpression.aexp = exp_result.node;
        NecroRenameNodeResult next_result = rename_ast_go(input_node->fexpression.next_fexpression, arena, scoped_symtable);
        if (next_result.type == NECRO_RENAME_ERROR)
            return next_result;
        result.node->fexpression.next_fexpression = next_result.node;
        break;
    }

    case NECRO_AST_VARIABLE:
    {
        result.node->variable.variable_id   = input_node->variable.variable_id;
        result.node->variable.variable_type = input_node->variable.variable_type;
        if (result.node->variable.variable_type == NECRO_AST_VARIABLE_ID)
            result.node->variable.id = necro_scoped_symtable_find(scoped_symtable, result.node->variable.variable_id);
        break;
    }

    case NECRO_AST_APATS:
    {
        NecroRenameNodeResult apat_result = rename_ast_go(input_node->apats.apat, arena, scoped_symtable);
        if (apat_result.type == NECRO_RENAME_ERROR)
            return apat_result;
        result.node->apats.apat = apat_result.node;
        NecroRenameNodeResult next_result = rename_ast_go(input_node->apats.next_apat, arena, scoped_symtable);
        if (next_result.type == NECRO_RENAME_ERROR)
            return next_result;
        result.node->apats.next_apat = next_result.node;
        break;
    }

    case NECRO_AST_WILDCARD:
        break;

    case NECRO_AST_LAMBDA:
    {
        necro_scoped_symtable_new_scope(scoped_symtable);
        NecroRenameNodeResult apats_result = rename_ast_go(input_node->lambda.apats, arena, scoped_symtable);
        if (apats_result.type == NECRO_RENAME_ERROR)
            return apats_result;
        result.node->lambda.apats = apats_result.node;
        NecroRenameNodeResult expr_result = rename_ast_go(input_node->lambda.expression, arena, scoped_symtable);
        if (expr_result.type == NECRO_RENAME_ERROR)
            return expr_result;
        result.node->lambda.expression = expr_result.node;
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    }

    case NECRO_AST_DO:
    {
        necro_scoped_symtable_new_scope(scoped_symtable);
        NecroRenameNodeResult do_result = rename_ast_go(input_node->do_statement.statement_list, arena, scoped_symtable);
        if (do_result.type == NECRO_RENAME_ERROR)
            return do_result;
        result.node->do_statement.statement_list = do_result.node;
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    }

    case NECRO_AST_LIST_NODE:
    {
        NecroRenameNodeResult item_result = rename_ast_go(input_node->list.item, arena, scoped_symtable);
        if (item_result.type == NECRO_RENAME_ERROR)
            return item_result;
        result.node->list.item = item_result.node;
        NecroRenameNodeResult next_result = rename_ast_go(input_node->list.next_item, arena, scoped_symtable);
        if (next_result.type == NECRO_RENAME_ERROR)
            return next_result;
        result.node->list.next_item = next_result.node;
        break;
    }

    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroRenameNodeResult list_result = rename_ast_go(input_node->expression_list.expressions, arena, scoped_symtable);
        if (list_result.type == NECRO_RENAME_ERROR)
            return list_result;
        result.node->expression_list.expressions = list_result.node;
        break;
    }

    case NECRO_AST_TUPLE:
    {
        NecroRenameNodeResult tuple_result = rename_ast_go(input_node->tuple.expressions, arena, scoped_symtable);
        if (tuple_result.type == NECRO_RENAME_ERROR)
            return tuple_result;
        result.node->tuple.expressions = tuple_result.node;
        break;
    }

    case NECRO_BIND_ASSIGNMENT:
    {
        result.node->bind_assignment.variable_name = input_node->bind_assignment.variable_name;
        result.node->bind_assignment.id            = necro_scoped_symtable_new_symbol_info(scoped_symtable, necro_create_initial_symbol_info(result.node->bind_assignment.variable_name));
        NecroRenameNodeResult expr_result = rename_ast_go(input_node->bind_assignment.expression, arena, scoped_symtable);
        if (expr_result.type == NECRO_RENAME_ERROR)
            return expr_result;
        result.node->bind_assignment.expression = expr_result.node;
        break;
    }

    case NECRO_AST_ARITHMETIC_SEQUENCE:
    {
        NecroRenameNodeResult from_result = rename_ast_go(input_node->arithmetic_sequence.from, arena, scoped_symtable);
        if (from_result.type == NECRO_RENAME_ERROR)
            return from_result;
        result.node->arithmetic_sequence.from = from_result.node;
        NecroRenameNodeResult then_result = rename_ast_go(input_node->arithmetic_sequence.then, arena, scoped_symtable);
        if (then_result.type == NECRO_RENAME_ERROR)
            return then_result;
        result.node->arithmetic_sequence.then = then_result.node;
        NecroRenameNodeResult to_result = rename_ast_go(input_node->arithmetic_sequence.to, arena, scoped_symtable);
        if (to_result.type == NECRO_RENAME_ERROR)
            return to_result;
        result.node->arithmetic_sequence.to   = to_result.node;
        result.node->arithmetic_sequence.type = input_node->arithmetic_sequence.type;
        break;
    }

    default:
        return (NecroRenameNodeResult) { .error = (NecroError) { .error_message = "Unrecognized type found during renaming.", .line = 0, .character = 0 }, .type = NECRO_RENAME_ERROR };
    }
    return result;
}

NecroRenameResult necro_rename_ast(NecroAST_Reified* input_ast, NecroSymTable* symtable)
{
    NecroScopedSymTable   scoped_symtable = necro_create_scoped_symtable(symtable);
    NecroAST_Reified      ast             = necro_create_reified_ast();
    NecroRenameNodeResult result          = rename_ast_go(input_ast->root, &ast.arena, &scoped_symtable);
    necro_destroy_scoped_symtable(&scoped_symtable);
    if (result.type == NECRO_RENAME_SUCCESSFUL)
    {
        ast.root = result.node;
        return (NecroRenameResult) { .type = NECRO_RENAME_SUCCESSFUL, .ast = ast };
    }
    else
    {
        return (NecroRenameResult) { .type = NECRO_RENAME_ERROR, .error = result.error };
    }
}

void necro_test_rename(const char* input_string)
{
    // printf("input_string:\n%s\n\n", input_string);
    puts("");
    puts("--------------------------------");
    puts("-- Lexing");
    puts("--------------------------------");

    NecroLexer       lexer      = necro_create_lexer(input_string);
    NECRO_LEX_RESULT lex_result = necro_lex(&lexer);
    necro_print_lexer(&lexer);

    NecroAST ast = { construct_arena(lexer.tokens.length * sizeof(NecroAST_Node)) };
    if (lex_result != NECRO_LEX_RESULT_SUCCESSFUL || lexer.tokens.length <= 0)
        return;

    puts("");
    puts("--------------------------------");
    puts("-- Parsing");
    puts("--------------------------------");

    NecroParser parser;
    construct_parser(&parser, &ast, lexer.tokens.data);
    NecroAST_LocalPtr root_node_ptr = null_local_ptr;

    NecroParse_Result parse_result = parse_ast(&parser, &root_node_ptr);
    if (parse_result != ParseSuccessful)
    {
        if (parser.error_message && parser.error_message[0])
        {
            puts(parser.error_message);
        }
        else
        {
            puts("Parsing failed for unknown reason.");
        }
        return;
    }

    puts("");
    puts("Parse succeeded");
    print_ast(&ast, &lexer.intern, root_node_ptr);

    puts("");
    puts("--------------------------------");
    puts("-- Reifying");
    puts("--------------------------------");
    NecroAST_Reified ast_r = necro_reify_ast(&ast, root_node_ptr);
    // necro_print_reified_ast(&ast_r, &lexer.intern);

    puts("");
    puts("--------------------------------");
    puts("-- Rename");
    puts("--------------------------------");
    NecroSymTable     symtable      = necro_create_symtable(&lexer.intern);
    NecroRenameResult rename_result = necro_rename_ast(&ast_r, &symtable);
    if (rename_result.type == NECRO_RENAME_ERROR)
    {
        if (rename_result.error.error_message && rename_result.error.error_message[0])
        {
            puts(rename_result.error.error_message);
        }
        else
        {
            puts("Parsing failed for unknown reason.");
        }
        return;
    }
    NecroAST_Reified renamed_ast = rename_result.ast;
    necro_print_reified_ast(&renamed_ast, &lexer.intern);

    puts("");
    necro_symtable_print(&symtable);

    puts("");
    puts("--------------------------------");
    puts("-- Cleaning Up");
    puts("--------------------------------");

    destruct_parser(&parser);
    necro_destroy_lexer(&lexer);
    destruct_arena(&ast.arena);
}
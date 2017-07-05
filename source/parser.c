/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "parser.h"

// =====================================================
// Abstract Syntax Tree
// =====================================================

NecroAST_Node* ast_alloc_node(NecroAST* ast)
{
    return (NecroAST_Node*) arena_alloc(&ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

NecroAST_Node* ast_alloc_node_local_ptr(NecroAST* ast, NecroAST_LocalPtr* local_ptr)
{
    *local_ptr = ast->arena.size / sizeof(NecroAST_Node);
    return (NecroAST_Node*) arena_alloc(&ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

// used to back track AST modifications during parsing
void ast_pop_node(NecroAST* ast)
{
    assert(ast->arena.size >= sizeof(NecroAST_Node));
    ast->arena.size -= sizeof(NecroAST_Node);
}

void print_ast_impl(NecroAST* ast, NecroAST_Node* ast_node, uint32_t depth)
{
    for (uint32_t i = 0;  i < depth; ++i)
    {
        printf("\t");
    }

    switch(ast_node->type)
    {
    case NECRO_AST_BIN_OP:
        switch(ast_node->bin_op.type)
        {
        case NECRO_BIN_OP_ADD:
            puts("(+)");
            break;
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.lhs), depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.rhs), depth + 1);
        break;

    default:
        puts("(Undefined)");
        break;
    }
}

void print_ast(NecroAST* ast)
{
    print_ast_impl(ast, ast_get_root_node(ast), 0);
}

// =====================================================
// Recursive Descent Parser
// =====================================================

bool parse_expression(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast);
bool parse_constant(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast);
bool parse_unary_operation(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast);
bool parse_binary_operation(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast);
bool parse_function_composition(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast);

NecroParse_Result parse_ast(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    return ParseError;
}

bool match_token(NecroLexToken** tokens, NECRO_LEX_TOKEN_TYPE token_type)
{
    if ((*tokens)->token == token_type)
    {
        ++(*tokens);
        return true;
    }

    return false;
}

bool parse_expression(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    if (parse_constant(tokens, num_tokens, ast) ||
        parse_unary_operation(tokens, num_tokens, ast) ||
        parse_binary_operation(tokens, num_tokens, ast) ||
        parse_function_composition(tokens, num_tokens, ast))

    {
        return true;
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return false;
}

bool parse_constant(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    switch(tokens[0]->token)
    {
    case NECRO_LEX_FLOAT_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node(ast);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_FLOAT;
            constant.double_literal = tokens[0]->double_literal;
            ast_node->constant = constant;
            ++(*tokens);
            return true;
        }
    case NECRO_LEX_INTEGER_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node(ast);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_INTEGER;
            constant.int_literal = tokens[0]->int_literal;
            ast_node->constant = constant;
            ++(*tokens);
            return true;
        }
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return false;
}

bool parse_unary_operation(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return false;
}

bool parse_binary_operation(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return false;
}

bool parse_function_composition(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return false;
}

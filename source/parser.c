/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "parser.h"

// =====================================================
// Abstract Syntax Tree
// =====================================================

NecroAST_Node* ast_alloc_node(NecroAST* ast, NecroAST_LocalPtr* p_local_ptr)
{
    *p_local_ptr = ast->arena.size / sizeof(NecroAST_Node);
    return (NecroAST_Node*) arena_alloc(&ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

void ast_pop_node(NecroAST* ast)
{
    assert(ast->arena.size >= sizeof(NecroAST_Node));
    ast->arena.size - sizeof(NecroAST_Node);
}

void print_ast_impl(NecroAST* ast, NecroAST_Node* ast_node, uint32_t depth)
{
    uint32_t i;
    for (i = 0;  i < depth; ++i)
    {
        printf("\t");
    }

    switch(ast_node->type)
    {
    case NECRO_AST_ADD:
        puts("(+)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->add.lhs), depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->add.rhs), depth + 1);
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

bool parse_expression(NecroLexToken** tokens, size_t num_tokens);
bool parse_constant(NecroLexToken** tokens, size_t num_tokens);
bool parse_unary_operation(NecroLexToken** tokens, size_t num_tokens);
bool parse_binary_operation(NecroLexToken** tokens, size_t num_tokens);
bool parse_function_composition(NecroLexToken** tokens, size_t num_tokens);

NecroParse_Result parse_ast(NecroLexToken** tokens, size_t num_tokens, NecroAST* ast)
{
    return parse_error;
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

bool parse_expression(NecroLexToken** tokens, size_t num_tokens)
{
    NecroLexToken* originalTokens = *tokens;
    if (parse_constant(tokens, num_tokens) ||
        parse_unary_operation(tokens, num_tokens) ||
        parse_binary_operation(tokens, num_tokens) ||
        parse_function_composition(tokens, num_tokens))

    {
        return true;
    }

    *tokens = originalTokens;
    return false;
}

bool parse_constant(NecroLexToken** tokens, size_t num_tokens)
{
    NecroLexToken* originalTokens = *tokens;
    switch(tokens[0]->token)
    {
    case NECRO_LEX_INTEGER_LITERAL:
        ++(*tokens);
        return true;
	case NECRO_LEX_FLOAT_LITERAL:
		++(*tokens);
		return true;
    }

    *tokens = originalTokens;
    return false;
}

bool parse_unary_operation(NecroLexToken** tokens, size_t num_tokens)
{
    NecroLexToken* originalTokens = *tokens;
    *tokens = originalTokens;
    return false;
}

bool parse_binary_operation(NecroLexToken** tokens, size_t num_tokens)
{
    NecroLexToken* originalTokens = *tokens;
    *tokens = originalTokens;
    return false;
}

bool parse_function_composition(NecroLexToken** tokens, size_t num_tokens)
{
    NecroLexToken* originalTokens = *tokens;
    *tokens = originalTokens;
    return false;
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "parser.h"

// =====================================================
// Abstract Syntax Tree
// =====================================================

NecroAST_Node* ast_alloc_node(NecroAST ast, NecroAST_LocalPtr* pLocalPtr)
{
    *pLocalPtr = ast.arena.size / sizeof(NecroAST_Node);
    return (NecroAST_Node*) arena_alloc(&ast.arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

void print_ast_impl(NecroAST ast, NecroAST_Node* ast_node, uint32_t depth)
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

void print_ast(NecroAST ast)
{
    print_ast_impl(ast, ast_get_root_node(ast), 0);
}

// =====================================================
// Recursive Descent Parser
// =====================================================

bool parse_expression(NecroLexToken** pTokens, size_t num_tokens);
bool parse_constant(NecroLexToken** pTokens, size_t num_tokens);
bool parse_unary_operation(NecroLexToken** pTokens, size_t num_tokens);
bool parse_binary_operation(NecroLexToken** pTokens, size_t num_tokens);
bool parse_function_composition(NecroLexToken** pTokens, size_t num_tokens);

NecroParse_Result parse_ast(NecroLexToken** pTokens, size_t num_tokens, NecroAST* pAst)
{
    return parse_error;
}

bool match_token(NecroLexToken** pTokens, NECRO_LEX_TOKEN_TYPE token_type)
{
    if ((*pTokens)->token == token_type)
    {
        ++(*pTokens);
        return true;
    }

    return false;
}

bool parse_expression(NecroLexToken** pTokens, size_t num_tokens)
{
    NecroLexToken* pOriginalTokens = *pTokens;
    if (parse_constant(pTokens, num_tokens) ||
        parse_unary_operation(pTokens, num_tokens) ||
        parse_binary_operation(pTokens, num_tokens) ||
        parse_function_composition(pTokens, num_tokens))

    {
        return true;
    }

    *pTokens = pOriginalTokens;
    return false;
}

bool parse_constant(NecroLexToken** pTokens, size_t num_tokens)
{
    NecroLexToken* pOriginalTokens = *pTokens;
    switch((*pTokens)->token)
    {
    case NECRO_LEX_NUMERIC_LITERAL:
        ++(*pTokens);
        return true;
    }

    *pTokens = pOriginalTokens;
    return false;
}

bool parse_unary_operation(NecroLexToken** pTokens, size_t num_tokens)
{
    NecroLexToken* pOriginalTokens = *pTokens;
    *pTokens = pOriginalTokens;
    return false;
}

bool parse_binary_operation(NecroLexToken** pTokens, size_t num_tokens)
{
    NecroLexToken* pOriginalTokens = *pTokens;
    *pTokens = pOriginalTokens;
    return false;
}

bool parse_function_composition(NecroLexToken** pTokens, size_t num_tokens)
{
    NecroLexToken* pOriginalTokens = *pTokens;
    *pTokens = pOriginalTokens;
    return false;
}

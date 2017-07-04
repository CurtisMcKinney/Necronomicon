/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "parser.h"
#include "necro.h"

bool parse_expression(struct NecroLexToken** pTokens, size_t num_tokens);
bool parse_constant(struct NecroLexToken** pTokens, size_t num_tokens);
bool parse_unary_operation(struct NecroLexToken** pTokens, size_t num_tokens);
bool parse_binary_operation(struct NecroLexToken** pTokens, size_t num_tokens);
bool parse_function_composition(struct NecroLexToken** pTokens, size_t num_tokens);

bool match_token(struct NecroLexToken** pTokens, NECRO_LEX_TOKEN_TYPE token_type)
{
    if ((*pTokens)->token == token_type)
    {
        ++(*pToken);
        return true;
    }

    return false;
}

bool parse_expression(struct NecroLexToken** pTokens, size_t num_tokens)
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

bool parse_constant(struct NecroLexToken** pTokens, size_t num_tokens)
{
    NecroLexToken* originalTokens = *tokens;

    *tokens = originalTokens;
    return false;
}

bool parse_unary_operation(struct NecroLexToken** pTokens, size_t num_tokens);
bool parse_binary_operation(struct NecroLexToken** pTokens, size_t num_tokens);
bool parse_function_composition(struct NecroLexToken** pTokens, size_t num_tokens);

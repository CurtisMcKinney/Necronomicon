/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "parser.h"

bool parse_expression(NecroLexToken** pTokens, size_t num_tokens);
bool parse_constant(NecroLexToken** pTokens, size_t num_tokens);
bool parse_unary_operation(NecroLexToken** pTokens, size_t num_tokens);
bool parse_binary_operation(NecroLexToken** pTokens, size_t num_tokens);
bool parse_function_composition(NecroLexToken** pTokens, size_t num_tokens);

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

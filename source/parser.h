/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdlib.h>
#include <stdbool.h>

struct NecroLexToken;

bool parse_expression(struct NecroLexToken** pTokens, size_t num_tokens);

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_AST_SYMBOL_H
#define NECRO_AST_SYMBOL_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "intern.h"

#define NECRO_DECLARE_NECRO_AST_SYMBOL(AST_TYPE) \
struct AST_TYPE; \
typedef struct \
{ \
    NecroSymbol        name; \
    NecroSymbol        source_name; \
    NecroSymbol        module_name; \
    struct AST_TYPE*   ast; \
} NecroAstSymbol##AST_TYPE; \
inline const char* necro_ast_symbol_##AST_TYPE##_most_qualified_name(NecroAstSymbol##AST_TYPE symbol) \
{ \
    if (symbol.name != NULL && symbol.name->str != NULL) \
        return symbol.name->str; \
    else if (symbol.source_name != NULL && symbol.source_name->str != NULL) \
        return symbol.source_name->str; \
    else \
        return "NULL AST_SYMBOL"; \
}

#define NecroAstSymbol(AST_TYPE) NecroAstSymbol##AST_TYPE

NECRO_DECLARE_NECRO_AST_SYMBOL(NecroAst);

#endif // NECRO_AST_SYMBOL_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core.h"

#define NECRO_CORE_DEBUG 0
#if NECRO_CORE_DEBUG
#define TRACE_CORE(...) printf(__VA_ARGS__)
#else
#define TRACE_CORE(...)
#endif

void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern)
{
    assert(ast_node != NULL);
    switch (ast_node->expr_type)
    {
    default:
        printf("necro_print_core_node printing expression type unimplemented!: %d", ast_node->expr_type);
        assert(false);
        break;
    }
}
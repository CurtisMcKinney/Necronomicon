/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_NODE_PRINT_H
#define NECRO_NODE_PRINT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"
#include "node_type.h"

struct NecroNodeAST;
struct NecroNodeProgram;

void necro_node_print_ast_go(struct NecroNodeProgram* program, struct NecroNodeAST* ast, size_t depth);
void necro_node_print_ast(struct NecroNodeProgram* program, struct NecroNodeAST* ast);
void necro_print_node_program(struct NecroNodeProgram* program);

#endif // NECRO_NODE_PRINT_H
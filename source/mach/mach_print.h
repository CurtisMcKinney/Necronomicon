/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACH_PRINT_H
#define NECRO_MACH_PRINT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "type.h"
#include "mach_type.h"

struct NecroMachAst;
struct NecroMachProgram;

void necro_mach_print_ast(struct NecroMachAst* ast);
void necro_mach_print_ast_go(struct NecroMachAst* ast, size_t depth);
void necro_mach_print_program(struct NecroMachProgram* program);

#endif // NECRO_MACH_PRINT_H
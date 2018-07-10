/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACHINE_PRINT_H
#define NECRO_MACHINE_PRINT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "core/core.h"
#include "type.h"
#include "machine_type.h"

struct NecroMachineAST;
struct NecroMachineProgram;

void necro_machine_print_ast_go(struct NecroMachineProgram* program, struct NecroMachineAST* ast, size_t depth);
void necro_machine_print_ast(struct NecroMachineProgram* program, struct NecroMachineAST* ast);
void necro_print_machine_program(struct NecroMachineProgram* program);

#endif // NECRO_MACHINE_PRINT_H
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef MACHINE_CASE_H
#define MACHINE_CASE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#include "machine.h"
#include "core/core.h"

void             necro_core_to_machine_1_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer);
void             necro_core_to_machine_2_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer);
NecroMachineAST* necro_core_to_machine_3_case(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer);

#endif // MACHINE_CASE_H
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

NecroMachineAST* necro_core_to_machine_case_1(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer);
NecroMachineAST* necro_core_to_machine_case_2(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer);
NecroMachineAST* necro_core_to_machine_case_3(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer);

#endif // MACHINE_CASE_H
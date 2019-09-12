/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef MACH_CASE_H
#define MACH_CASE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#include "mach_ast.h"
#include "mach_transform.h"
#include "core/core_ast.h"

void          necro_core_transform_to_mach_1_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer);
void          necro_core_transform_to_mach_2_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer);
NecroMachAst* necro_core_transform_to_mach_3_case(NecroMachProgram* program, NecroCoreAst* ast, NecroMachAst* outer);

#endif // MACH_CASE_H
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACHINE_CLOSURE_H
#define NECRO_MACHINE_CLOSURE_H 1

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>

#include "core/closure_conversion.h"
#include "machine_type.h"

NECRO_DECLARE_VECTOR(struct NecroMachineAST*, NecroClosureCon, closure_con);
struct NecroMachineAST* necro_get_closure_con(struct NecroMachineProgram* program, size_t closure_arity, bool is_constant);

#endif // NECRO_MACHINE_CLOSURE_H
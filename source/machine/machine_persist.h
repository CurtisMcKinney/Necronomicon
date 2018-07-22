/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_MACHINE_PERSIST_H
#define NECRO_MACHINE_PERSIST_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include "type.h"
#include "prim.h"
#include "machine_type.h"

typedef struct
{
    size_t           hash;
    NecroType*       type;
    NecroCon         type_con;
    NecroCon         increment_con;
    NecroCon         decrement_con;
    bool             is_primitive;
    // NecroMachineAST* primitive_ast;
} NecroMachinePersistData;

typedef struct
{
    NecroSymTable*           symtable;
    NecroMachinePersistData* data;
    size_t                   capacity;
    size_t                   count;
} NecroMachinePersistTable;
NecroMachinePersistTable necro_create_machine_persist_table(NecroSymTable* symtable, NecroPrimTypes* prim_types);
void                     necro_destroy_machine_persist_table(NecroMachinePersistTable* table);
struct NecroMachineAST*  necro_gen_specialized_increment(struct NecroMachineProgram* program, NecroVar var);

#endif // NECRO_MACHINE_PERSIST_H
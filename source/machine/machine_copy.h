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

struct NecroMachineProgram;
struct NecroMachineAST;

#define NECRO_NULL_DATA_ID    0
#define NECRO_UNBOXED_DATA_ID 1
#define NECRO_APPLY_DATA_ID 2

typedef struct NecroConstructorInfo
{
    size_t  members_offset; // offset into members map which contains ids into data map
    size_t  num_members;
    size_t  size_in_bytes;
    bool    is_tagged_union;
    bool    is_machine_def;
} NecroConstructorInfo;

typedef struct
{
    size_t     hash;
    NecroType* type;
    size_t     data_id;
} NecroMachineCopyData;

NECRO_DECLARE_VECTOR(size_t, NecroMember, member);
NECRO_DECLARE_VECTOR(NecroConstructorInfo, NecroDataMap, data_map);

typedef struct
{
    NecroSymTable*        symtable;
    NecroMachineCopyData* data;
    size_t                capacity;
    size_t                count;
    NecroMemberVector     member_map;
    NecroDataMapVector    data_map;
} NecroMachineCopyTable;
NecroMachineCopyTable necro_create_machine_copy_table(NecroSymTable* symtable, NecroPrimTypes* prim_types);
void                  necro_destroy_machine_copy_table(NecroMachineCopyTable* table);
size_t                necro_create_data_info(struct NecroMachineProgram* program, NecroType* type);
size_t                necro_create_machine_def_data_info(struct NecroMachineProgram* program, struct NecroMachineAST* ast);
void                  necro_print_data_info(struct NecroMachineProgram* program);
NecroConstructorInfo  necro_get_data_info(struct NecroMachineProgram* program, size_t data_id, size_t tag);

#endif // NECRO_MACHINE_PERSIST_H
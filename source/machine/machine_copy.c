/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_copy.h"
#include <ctype.h>
#include "machine.h"
#include "machine_prim.h"
#include "machine_case.h"
#include "machine_print.h"

#define NECRO_MACHINE_PERSIST_TABLE_INITIAL_SIZE 512

///////////////////////////////////////////////////////
// NecroMachineCopyTable
///////////////////////////////////////////////////////
NecroMachineCopyData* necro_machine_copy_table_insert(NecroMachineCopyTable* table, NecroType* type);
NecroMachineCopyTable necro_create_machine_copy_table(NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    NecroMachineCopyTable table;
    table.symtable   = symtable;
    table.data       = malloc(NECRO_MACHINE_PERSIST_TABLE_INITIAL_SIZE * sizeof(NecroMachineCopyData));
    table.capacity   = NECRO_MACHINE_PERSIST_TABLE_INITIAL_SIZE;
    table.count      = NECRO_APPLY_DATA_ID + 1;
    table.member_map = necro_create_member_vector();
    table.data_map   = necro_create_data_map_vector();
    for (size_t i = 0; i < table.capacity; ++i)
        table.data[i] = (NecroMachineCopyData) { .hash = 0, .type = NULL };
    NecroMachineCopyData* int_data   = necro_machine_copy_table_insert(&table, necro_symtable_get(symtable, prim_types->int_type.id)->type);
    int_data->data_id                = NECRO_UNBOXED_DATA_ID;
    NecroMachineCopyData* float_data = necro_machine_copy_table_insert(&table, necro_symtable_get(symtable, prim_types->float_type.id)->type);
    float_data->data_id              = NECRO_UNBOXED_DATA_ID;

    // NULL / Unboxed
    NecroConstructorInfo prim_info   = (NecroConstructorInfo) { .members_offset = 0, .num_members = 0, .size_in_bytes = sizeof(char*), .is_tagged_union = false, .is_machine_def = false }; // TODO: Proper word sizing for target
    necro_push_data_map_vector(&table.data_map, &prim_info); // NULL
    necro_push_data_map_vector(&table.data_map, &prim_info); // SELF
    necro_push_data_map_vector(&table.data_map, &prim_info); // Unboxed

    // Apply
    NecroConstructorInfo apply_info  = (NecroConstructorInfo) { .members_offset = 0, .num_members = 3, .size_in_bytes = 4 * sizeof(char*), .is_tagged_union = false, .is_machine_def = true }; // TODO: Proper word sizing for target
    necro_push_data_map_vector(&table.data_map, &apply_info); // Apply
    size_t unboxed_id = NECRO_UNBOXED_DATA_ID;
    necro_push_member_vector(&table.member_map, &unboxed_id);
    size_t null_id = NECRO_NULL_DATA_ID;
    necro_push_member_vector(&table.member_map, &null_id);
    size_t apply_id = NECRO_APPLY_DATA_ID;
    necro_push_member_vector(&table.member_map, &apply_id);

    // Need special handling for audio / arrays!
    // NecroMachineCopyData* audio_data = necro_machine_copy_table_insert(&table, prim_types->audio_type);
    // audio_data->data_id = ???
    return table;
}

void necro_destroy_machine_copy_table(NecroMachineCopyTable* table)
{
    if (table->data == NULL)
        return;
    free(table->data);
    table->data = NULL;
}

void necro_machine_copy_table_grow(NecroMachineCopyTable* table)
{
    NecroMachineCopyData* prev_data    = table->data;
    size_t               prev_capacity = table->capacity;
    size_t               prev_count    = table->count;
    table->count                       = 0;
    table->capacity                   *= 2;
    table->data                        = malloc(table->capacity * sizeof(NecroMachineCopyData));
    for (size_t i = 0; i < table->capacity; ++i)
        table->data[i] = (NecroMachineCopyData) { .hash = 0, .type = NULL };
    for (size_t i = 0; i < prev_capacity; ++i)
    {
        if (prev_data[i].type == NULL)
            continue;
        size_t hash   = prev_data[i].hash;
        size_t bucket = hash & (table->capacity - 1);
        while (table->data[bucket].type != NULL)
        {
            bucket = (bucket + 1) & (table->capacity - 1);
        }
        table->count++;
        table->data[bucket] = prev_data[i];
    }
    assert(table->count == prev_count);
    free(prev_data);
}

NecroMachineCopyData* necro_machine_copy_table_insert(NecroMachineCopyTable* table, NecroType* type)
{
    if ((table->count + 1) >= (table->capacity / 2))
        necro_machine_copy_table_grow(table);
    size_t               hash      = necro_type_hash(type);
    NecroMachineCopyData copy_data = (NecroMachineCopyData)
    {
       .hash    = hash,
       .type    = type,
       .data_id = NECRO_NULL_DATA_ID,
    };
    size_t bucket  = hash & (table->capacity - 1);
    copy_data.hash = hash;
    while (table->data[bucket].type != NULL)
    {
        if (table->data[bucket].hash == hash && necro_exact_unify(table->data[bucket].type, copy_data.type))
            return table->data + bucket; // Already in table
        bucket = (bucket + 1) & (table->capacity - 1);
    }
    table->count++;
    table->data[bucket] = copy_data;
    return table->data + bucket;
}

///////////////////////////////////////////////////////
// Data Info
///////////////////////////////////////////////////////
size_t necro_create_data_info(NecroMachineProgram* program, NecroType* type)
{
    type = necro_find(type);
    assert(type->type == NECRO_TYPE_CON);
    NecroMachineCopyData* type_data = necro_machine_copy_table_insert(&program->copy_table, type);
    if (type_data->data_id != NECRO_NULL_DATA_ID)
        return type_data->data_id;
    NecroASTNode* data_declaraction_ast = necro_symtable_get(program->symtable, type->con.con.id)->ast;
    NecroASTNode* constructor_ast_list  = data_declaraction_ast->data_declaration.constructor_list;
    assert(constructor_ast_list != NULL);
    bool          is_tagged_union       = constructor_ast_list->list.next_item != NULL;
    while (constructor_ast_list != NULL)
    {
        NecroASTNode*        arg_list     = constructor_ast_list->list.item->constructor.arg_list;
        NecroConstructorInfo info         = (NecroConstructorInfo) { .members_offset = program->copy_table.member_map.length, .num_members = 0, .size_in_bytes = sizeof(char*), .is_tagged_union = is_tagged_union, .is_machine_def = false };
        bool                 first_member = true;
        NecroType*           con_inst     = necro_inst(program->infer, constructor_ast_list->list.item->necro_type, NULL);
        NecroType*           con_spec     = type;
        NecroType*           con_inst_go  = con_inst;
        while (con_inst_go->type == NECRO_TYPE_FUN)
        {
            con_spec    = necro_create_type_fun(program->infer, necro_new_name(program->infer, (NecroSourceLoc) { 0, 0, 0 }), con_spec);
            con_inst_go = con_inst_go->fun.type2;
        }
        necro_unify(program->infer, con_inst, con_spec, NULL, type, "Compiler bug in machine_copy.c");
        assert(program->infer->error.return_code == NECRO_SUCCESS);
        // Tag
        size_t unboxed_id = NECRO_UNBOXED_DATA_ID;
        necro_push_member_vector(&program->copy_table.member_map, &unboxed_id);
        info.size_in_bytes += sizeof(char*);
        info.num_members++;
        // Members
        while (con_spec->type == NECRO_TYPE_FUN)
        {
            NecroType* member_type    = necro_find(con_spec->fun.type1);
            size_t     member_data_id = necro_create_data_info(program, member_type);
            if (first_member)
            {
                info.members_offset = program->copy_table.member_map.length;
                first_member        = false;
            }
            necro_push_member_vector(&program->copy_table.member_map, &member_data_id);
            info.size_in_bytes += sizeof(char*);
            info.num_members++;
            con_spec = con_spec->fun.type2;
        }
        assert(necro_exact_unify(con_spec, type));
        size_t constructor_data_id = program->copy_table.data_map.length;
        necro_push_data_map_vector(&program->copy_table.data_map, &info);
        if (type_data->data_id == NECRO_NULL_DATA_ID)
        {
            type_data->data_id = constructor_data_id;
        }
        constructor_ast_list = constructor_ast_list->list.next_item;
    }
    return type_data->data_id;
}

void necro_add_machine_def_members(NecroMachineProgram* program, NecroConstructorInfo machine_def_info)
{
    for (size_t i = 0; i < machine_def_info.num_members; ++i)
    {
        size_t member_data_id = program->copy_table.member_map.data[machine_def_info.members_offset + i];
        // NecroConstructorInfo member_info    = program->copy_table.data_map.data[member_data_id];
        // Or should it!?
        // if (member_info.is_machine_def)
        // {
        //     // A machine_def data_info should never directly contain another machine_def data_info!
        //     assert(false);
        // }
        // else
        // {
            necro_push_member_vector(&program->copy_table.member_map, &member_data_id);
        // }
    }
}

size_t necro_create_machine_def_data_info(NecroMachineProgram* program, NecroMachineAST* ast)
{
    assert(ast->type == NECRO_MACHINE_DEF);
    NecroMachineDef* machine_def = &ast->machine_def;
    if (machine_def->data_id != NECRO_NULL_DATA_ID)
        return machine_def->data_id;
    machine_def->data_id = program->copy_table.data_map.length;
    NecroConstructorInfo info = (NecroConstructorInfo) { .members_offset = program->copy_table.member_map.length, .num_members = 0, .size_in_bytes = sizeof(char*), .is_tagged_union = false, .is_machine_def = true };
    for (size_t i = 0; i < machine_def->num_members; ++i)
    {
        NecroSlot            slot           = machine_def->members[i];
        size_t               member_data_id = slot.data_id;
        if (member_data_id == NECRO_SELF_DATA_ID)
            member_data_id = machine_def->data_id;
        if (slot.is_dynamic && slot.necro_machine_type->type == NECRO_MACHINE_TYPE_PTR && slot.necro_machine_type->ptr_type.element_type == ast->necro_machine_type)
            member_data_id = machine_def->data_id;
        NecroConstructorInfo member_info    = program->copy_table.data_map.data[member_data_id];
        if (member_info.is_machine_def && !slot.is_dynamic)
        {
            necro_add_machine_def_members(program, member_info);
            info.num_members   += member_info.num_members;
            info.size_in_bytes += member_info.size_in_bytes;
        }
        else
        {
            necro_push_member_vector(&program->copy_table.member_map, &member_data_id);
            info.num_members++;
            info.size_in_bytes += sizeof(char*);
        }
    }
    assert((program->copy_table.member_map.length - info.members_offset) == info.num_members);
    necro_push_data_map_vector(&program->copy_table.data_map, &info);
    return machine_def->data_id;
}

NecroConstructorInfo necro_get_data_info(struct NecroMachineProgram* program, size_t data_id, size_t tag)
{
    NecroConstructorInfo info = program->copy_table.data_map.data[data_id];
    if (info.is_tagged_union)
    {
        info = program->copy_table.data_map.data[data_id + tag];
    }
    return info;
}

void necro_print_data_info_go(struct NecroMachineProgram* program, size_t id, bool is_top)
{
    NecroConstructorInfo info = program->copy_table.data_map.data[id];
    if (!is_top)
    {
        printf("    * { id: %d, ", id);
    }
    else
    {
        printf("{ id: %d, .member_offset: %d, .num_members: %d, .size_in_bytes: %d, ", id, info.members_offset, info.num_members, info.size_in_bytes);
    }
    if (id == NECRO_NULL_DATA_ID)
        printf("Null }\n");
    else if (id == NECRO_UNBOXED_DATA_ID)
        printf("Unboxed }\n");
    else if (id == NECRO_APPLY_DATA_ID)
        printf("Apply }\n");
    else if (info.is_tagged_union)
        printf("TaggedUnion }\n");
    else if (info.is_machine_def)
        printf("MachineDef }\n");
    else
        printf("Constructor }\n");
    if (is_top)
    {
        // Print out members
        for (size_t i = 0; i < info.num_members; ++i)
        {
            necro_print_data_info_go(program, program->copy_table.member_map.data[info.members_offset + i], false);
        }
    }
}

void necro_print_data_info(struct NecroMachineProgram* program)
{
    printf("//-------------------\n");
    printf("// Necro Data Info\n");
    printf("//-------------------\n");
    for (size_t i = 0; i < program->copy_table.data_map.length; ++i)
    {
        necro_print_data_info_go(program, i, true);
    }
}

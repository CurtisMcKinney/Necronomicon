/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_persist.h"
#include <ctype.h>
#include "machine.h"
#include "machine_prim.h"
#include "machine_case.h"
#include "machine_print.h"

#define NECRO_MACHINE_PERSIST_TABLE_INITIAL_SIZE 512

NecroMachinePersistData* necro_machine_persist_table_insert(NecroMachinePersistTable* table, NecroCon type_con);
NecroMachineAST*         necro_gen_persist_method(NecroMachineProgram* program, NecroASTNode* data_declaration_ast);

NecroSymbol necro_create_persist_method_name(NecroMachineProgram* program, NecroType* data_type)
{
    NecroType*         principal_type        = necro_symtable_get(program->symtable, data_type->con.con.id)->type;
    size_t             principal_type_arity  = principal_type->con.arity;
    size_t             num_persist_method_strs = 2 + principal_type->con.arity * 2;
    const char**       persist_method_strs     = necro_snapshot_arena_alloc(&program->snapshot_arena, num_persist_method_strs * sizeof(const char*));
    persist_method_strs[0]                     = "_persist@";
    persist_method_strs[1]                     = necro_intern_get_string(program->intern, data_type->con.con.symbol);
    NecroType*         con_args              = data_type->con.args;
    for (size_t i = 2; i < num_persist_method_strs; i+=2)
    {
        assert(con_args != NULL);
        NecroType* arg_type = necro_find(con_args->list.item);
        assert(arg_type->type == NECRO_TYPE_CON);
        persist_method_strs[i]     = "$";
        persist_method_strs[i + 1] = necro_intern_get_string(program->intern, arg_type->con.con.symbol);
        con_args = con_args->list.next;
    }
    const char* persist_method_name = necro_concat_strings(&program->snapshot_arena, num_persist_method_strs, persist_method_strs);
    return necro_intern_string(program->intern, persist_method_name);
}

NecroMachineAST* necro_gen_specialized_increment(NecroMachineProgram* program, NecroVar var)
{
    NecroMachinePersistData* type_data = necro_machine_persist_table_insert(&program->persist_table, necro_var_to_con(var));
    if (type_data != NULL)
        return necro_symtable_get(program->symtable, type_data->increment_con.id)->necro_machine_ast;
    NecroArenaSnapshot snapshot              = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroType*         data_type             = necro_find(necro_symtable_get(program->symtable, var.id)->type);
    assert(data_type->type == NECRO_TYPE_CON);
    NecroSymbol        persist_method_symbol = necro_create_persist_method_name(program, data_type);
    NecroID            persist_method_id     = necro_this_scope_find(program->scoped_symtable->top_scope, persist_method_symbol);
    assert(persist_method_id.id == 0);
    // if (persist_method_id.id != 0)
    //     return necro_symtable_get(program->symtable, persist_method_id)->necro_machine_ast;
    // else
        persist_method_id = necro_scoped_symtable_new_symbol_info(program->scoped_symtable, program->scoped_symtable->top_scope, necro_create_initial_symbol_info(persist_method_symbol, (NecroSourceLoc) { 0, 0, 0 }, program->scoped_symtable->top_scope, program->intern));
    NecroSymbolInfo*   persist_method_info   = necro_symtable_get(program->symtable, persist_method_id);
    persist_method_info->necro_machine_ast   = necro_gen_persist_method(program, necro_symtable_get(program->symtable, data_type->con.con.id)->ast);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return persist_method_info->necro_machine_ast;
}

NecroMachineAST* necro_gen_persist_method(NecroMachineProgram* program, NecroASTNode* data_declaration_ast)
{
    // NecroSymbol persist_method_name = necro_create
    return NULL;
}

///////////////////////////////////////////////////////
// Persist Table
///////////////////////////////////////////////////////
NecroMachinePersistTable necro_create_machine_persist_table(NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    NecroMachinePersistTable table;
    table.symtable = symtable;
    table.data     = malloc(NECRO_MACHINE_PERSIST_TABLE_INITIAL_SIZE * sizeof(NecroMachinePersistData));
    table.capacity = NECRO_MACHINE_PERSIST_TABLE_INITIAL_SIZE;
    table.count    = 0;
    for (size_t i = 0; i < table.capacity; ++i)
        table.data[i] = (NecroMachinePersistData) { .hash = 0, .type = NULL };
    NecroMachinePersistData* int_data   = necro_machine_persist_table_insert(&table, prim_types->int_type);
    int_data->is_primitive              = true;
    NecroMachinePersistData* float_data = necro_machine_persist_table_insert(&table, prim_types->float_type);
    float_data->is_primitive            = true;
    NecroMachinePersistData* audio_data = necro_machine_persist_table_insert(&table, prim_types->audio_type);
    audio_data->is_primitive            = true;
    return table;
}

void necro_destroy_machine_persist_table(NecroMachinePersistTable* table)
{
    if (table->data == NULL)
        return;
    free(table->data);
    table->data = NULL;
}

void necro_machine_persist_table_grow(NecroMachinePersistTable* table)
{
    NecroMachinePersistData* prev_data     = table->data;
    size_t                   prev_capacity = table->capacity;
    size_t                   prev_count    = table->count;
    table->count                           = 0;
    table->capacity                       *= 2;
    table->data                            = malloc(table->capacity * sizeof(NecroMachinePersistData));
    for (size_t i = 0; i < table->capacity; ++i)
        table->data[i] = (NecroMachinePersistData) { .hash = 0, .type = NULL };
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

NecroMachinePersistData* necro_machine_persist_table_insert(NecroMachinePersistTable* table, NecroCon type_con)
{
    if ((table->count + 1) >= (table->capacity / 2))
        necro_machine_persist_table_grow(table);
    NecroType*              type         = necro_symtable_get(table->symtable, type_con.id)->type;
    size_t                  hash         = necro_type_hash(type);
    NecroMachinePersistData persist_data = (NecroMachinePersistData)
    {
       .hash          = hash,
       .type          = type,
       .type_con      = type_con,
       .increment_con = (NecroCon) { { 0, 0 }, { 0 } },
       .decrement_con = (NecroCon) { { 0, 0 }, { 0 } },
       .is_primitive  = false
    };
    size_t bucket = hash & (table->capacity - 1);
    persist_data.hash = hash;
    while (table->data[bucket].type != NULL)
    {
        if (table->data[bucket].hash == hash && necro_exact_unify(table->data[bucket].type, persist_data.type))
            return table->data + bucket; // Already in table
        bucket = (bucket + 1) & (table->capacity - 1);
    }
    table->count++;
    table->data[bucket] = persist_data;
    return table->data + bucket;
}

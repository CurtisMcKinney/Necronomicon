/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine.h"
#include <ctype.h>
#include "machine_prim.h"
#include "machine_case.h"
#include "machine_print.h"

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroVar necro_gen_var(NecroMachineProgram* program, NecroMachineAST* necro_machine_ast, const char* var_header, NECRO_NAME_UNIQUENESS uniqueness)
{
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
    const char* var_name = NULL;
    if (uniqueness == NECRO_NAME_NOT_UNIQUE)
    {
        char buf[10];
        itoa(program->gen_vars++, buf, 10);
        var_name = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "_", var_header, buf });
    }
    else
    {
        var_name = var_header;
    }
    NecroSymbol var_sym  = necro_intern_string(program->intern, var_name);
    NecroID     var_id   = necro_symtable_manual_new_symbol(program->symtable, var_sym);
    // necro_symtable_get(program->symtable, var_id)->type           = type;
    necro_symtable_get(program->symtable, var_id)->necro_machine_ast = necro_machine_ast;
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return (NecroVar) { .id = var_id, .symbol = var_sym };
}

void necro_program_add_struct(NecroMachineProgram* program, NecroMachineAST* struct_ast)
{
    assert(struct_ast->type == NECRO_MACHINE_STRUCT_DEF);
    necro_push_necro_machine_ast_vector(&program->structs, &struct_ast);
}

void necro_program_add_global(NecroMachineProgram* program, NecroMachineAST* global)
{
    assert(global->type == NECRO_MACHINE_VALUE);
    assert(global->value.value_type == NECRO_MACHINE_VALUE_GLOBAL);
    necro_push_necro_machine_ast_vector(&program->globals, &global);
}

void necro_program_add_function(NecroMachineProgram* program, NecroMachineAST* function)
{
    assert(function->type == NECRO_MACHINE_FN_DEF);
    necro_push_necro_machine_ast_vector(&program->functions, &function);
}

void necro_program_add_machine_def(NecroMachineProgram* program, NecroMachineAST* machine_def)
{
    assert(machine_def->type == NECRO_MACHINE_DEF);
    necro_push_necro_machine_ast_vector(&program->machine_defs, &machine_def);
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
void necro_add_statement_to_block(NecroMachineProgram* program, NecroMachineAST* block, NecroMachineAST* statement);

///////////////////////////////////////////////////////
// AST construction
///////////////////////////////////////////////////////
NecroMachineAST* necro_create_machine_value_ast(NecroMachineProgram* program, NecroMachineValue value, NecroMachineType* necro_machine_type)
{
    NecroMachineAST* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type            = NECRO_MACHINE_VALUE;
    ast->value           = value;
    ast->necro_machine_type = necro_machine_type;
    switch (value.value_type)
    {
    case NECRO_MACHINE_VALUE_REG:
        necro_symtable_get(program->symtable, value.reg_name.id)->necro_machine_ast = ast;
        break;
    default:
        break;
    }
    return ast;
}

// NecroMachineAST* necro_create_machine_char_lit(NecroMachineProgram* program, char c)
// {
//     NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
//     ast->type             = NECRO_MACHINE_LIT;
//     ast->lit.type         = NECRO_MACHINE_LIT_CHAR;
//     ast->lit.char_literal = c;
//     ast->necro_machine_type  = necro_create_machine_char_type(&program->arena);
//     return ast;
// }

NecroMachineAST* necro_create_reg(NecroMachineProgram* program, NecroMachineType* necro_machine_type, const char* reg_name_head)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .reg_name        = necro_gen_var(program, NULL, reg_name_head, NECRO_NAME_NOT_UNIQUE),
        .value_type      = NECRO_MACHINE_VALUE_REG,
    }, necro_machine_type);
}

NecroMachineAST* necro_create_global_value(NecroMachineProgram* program, NecroVar global_name, NecroMachineType* necro_machine_type)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .global_name = global_name,
        .value_type  = NECRO_MACHINE_VALUE_GLOBAL,
    }, necro_machine_type);
}

NecroMachineAST* necro_create_param_reg(NecroMachineProgram* program, NecroMachineAST* fn_def, size_t param_num)
{
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->necro_machine_type->type == NECRO_MACHINE_TYPE_FN);
    assert(fn_def->necro_machine_type->fn_type.num_parameters > param_num);
    // necro_type_check(program, fn_ast->necro_machine_type->fn_type.parameters[param_num], necro_machine_type);
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .param_reg       = { .fn_name = fn_def->fn_def.name, .param_num = param_num },
        .value_type      = NECRO_MACHINE_VALUE_PARAM,
    }, fn_def->necro_machine_type->fn_type.parameters[param_num]);
}

NecroMachineAST* necro_create_uint1_necro_machine_value(NecroMachineProgram* program, bool uint1_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .uint1_literal = uint1_literal,
        .value_type    = NECRO_MACHINE_VALUE_UINT1_LITERAL,
    }, necro_create_machine_uint1_type(&program->arena));
}

NecroMachineAST* necro_create_uint8_necro_machine_value(NecroMachineProgram* program, uint8_t uint8_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .uint8_literal = uint8_literal,
        .value_type    = NECRO_MACHINE_VALUE_UINT8_LITERAL,
    }, necro_create_machine_uint8_type(&program->arena));
}

NecroMachineAST* necro_create_uint16_necro_machine_value(NecroMachineProgram* program, uint16_t uint16_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .uint16_literal  = uint16_literal,
        .value_type      = NECRO_MACHINE_VALUE_UINT16_LITERAL,
    }, necro_create_machine_uint16_type(&program->arena));
}

NecroMachineAST* necro_create_uint32_necro_machine_value(NecroMachineProgram* program, uint32_t uint32_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .uint32_literal  = uint32_literal,
        .value_type      = NECRO_MACHINE_VALUE_UINT32_LITERAL,
    }, necro_create_machine_uint32_type(&program->arena));
}

NecroMachineAST* necro_create_uint64_necro_machine_value(NecroMachineProgram* program, uint64_t uint64_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .uint64_literal  = uint64_literal,
        .value_type      = NECRO_MACHINE_VALUE_UINT64_LITERAL,
    }, necro_create_machine_uint32_type(&program->arena));
}

NecroMachineAST* necro_create_i32_necro_machine_value(NecroMachineProgram* program, int32_t int32_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .int64_literal   = int32_literal,
        .value_type      = NECRO_MACHINE_VALUE_INT32_LITERAL,
    }, necro_create_machine_int32_type(&program->arena));
}

NecroMachineAST* necro_create_i64_necro_machine_value(NecroMachineProgram* program, int64_t int64_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .int64_literal   = int64_literal,
        .value_type      = NECRO_MACHINE_VALUE_INT64_LITERAL,
    }, necro_create_machine_int64_type(&program->arena));
}

NecroMachineAST* necro_create_f32_necro_machine_value(NecroMachineProgram* program, float f32_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .f32_literal = f32_literal,
        .value_type  = NECRO_MACHINE_VALUE_F32_LITERAL,
    }, necro_create_machine_f32_type(&program->arena));
}

NecroMachineAST* necro_create_f64_necro_machine_value(NecroMachineProgram* program, double f64_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .f64_literal = f64_literal,
        .value_type  = NECRO_MACHINE_VALUE_F64_LITERAL,
    }, necro_create_machine_f64_type(&program->arena));
}

NecroMachineAST* necro_create_null_necro_machine_value(NecroMachineProgram* program, NecroMachineType* ptr_type)
{
    assert(ptr_type->type == NECRO_MACHINE_TYPE_PTR);
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .value_type      = NECRO_MACHINE_VALUE_NULL_PTR_LITERAL,
    }, ptr_type);
}

NecroMachineAST* necro_create_word_uint_value(NecroMachineProgram* program, uint64_t int_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_create_uint32_necro_machine_value(program, (uint32_t) int_literal);
    else
        return necro_create_uint64_necro_machine_value(program, int_literal);
}

NecroMachineAST* necro_create_word_int_value(NecroMachineProgram* program, int64_t int_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_create_i32_necro_machine_value(program, (int32_t) int_literal);
    else
        return necro_create_i64_necro_machine_value(program, int_literal);
}

NecroMachineAST* necro_create_word_float_value(NecroMachineProgram* program, double float_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_create_f32_necro_machine_value(program, (float) float_literal);
    else
        return necro_create_f64_necro_machine_value(program, float_literal);
}

NecroMachineAST* necro_create_machine_load_from_ptr(NecroMachineProgram* program, NecroMachineAST* source_ptr_ast, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_MACHINE_VALUE);
    NecroMachineValue source_ptr = source_ptr_ast->value;
    assert(source_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_MACHINE_VALUE_REG || source_ptr.value_type == NECRO_MACHINE_VALUE_PARAM || source_ptr.value_type == NECRO_MACHINE_VALUE_GLOBAL);
    NecroMachineAST* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type            = NECRO_MACHINE_LOAD;
    ast->load.load_type  = NECRO_LOAD_PTR;
    ast->load.source_ptr = source_ptr_ast;
    ast->load.dest_value = necro_create_reg(program, source_ptr_ast->necro_machine_type->ptr_type.element_type, dest_name);
    ast->necro_machine_type = source_ptr_ast->necro_machine_type->ptr_type.element_type;
    return ast;
}

NecroMachineAST* necro_create_machine_load_from_slot(NecroMachineProgram* program, NecroMachineAST* source_ptr_ast, size_t source_slot, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_MACHINE_VALUE);
    assert(source_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(source_ptr_ast->necro_machine_type->ptr_type.element_type->type == NECRO_MACHINE_TYPE_STRUCT);
    assert(source_ptr_ast->necro_machine_type->ptr_type.element_type->struct_type.num_members > source_slot);
    NecroMachineAST* ast               = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                       = NECRO_MACHINE_LOAD;
    ast->load.load_type             = NECRO_LOAD_SLOT;
    ast->load.load_slot.source_ptr  = source_ptr_ast;
    ast->load.load_slot.source_slot = source_slot;
    ast->necro_machine_type         = source_ptr_ast->necro_machine_type->ptr_type.element_type->struct_type.members[source_slot];
    ast->load.dest_value            = necro_create_reg(program, ast->necro_machine_type, dest_name);
    return ast;
}

NecroMachineAST* necro_create_machine_store_into_ptr(NecroMachineProgram* program, NecroMachineAST* source_value_ast, NecroMachineAST* dest_ptr_ast)
{
    assert(source_value_ast->type == NECRO_MACHINE_VALUE);
    assert(dest_ptr_ast->type == NECRO_MACHINE_VALUE);
    // NecroMachineValue dest_ptr     = dest_ptr_ast->value;
    assert(dest_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    // assert(dest_ptr.value_type == NECRO_MACHINE_VALUE_REG || dest_ptr.value_type == NECRO_MACHINE_VALUE_PARAM);
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type               = NECRO_MACHINE_STORE;
    ast->store.store_type   = NECRO_STORE_PTR;
    ast->store.source_value = source_value_ast;
    ast->store.dest_ptr     = dest_ptr_ast;
    ast->necro_machine_type    = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_store_into_slot(NecroMachineProgram* program, NecroMachineAST* source_value_ast, NecroMachineAST* dest_ptr_ast, size_t dest_slot)
{
    assert(source_value_ast->type == NECRO_MACHINE_VALUE);
    assert(dest_ptr_ast->type == NECRO_MACHINE_VALUE);
    assert(dest_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(dest_ptr_ast->necro_machine_type->ptr_type.element_type->type == NECRO_MACHINE_TYPE_STRUCT);
    assert(dest_ptr_ast->necro_machine_type->ptr_type.element_type->struct_type.num_members > dest_slot);
    necro_type_check(program, source_value_ast->necro_machine_type, dest_ptr_ast->necro_machine_type->ptr_type.element_type->struct_type.members[dest_slot]);
    NecroMachineAST* ast               = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                       = NECRO_MACHINE_STORE;
    ast->store.store_type           = NECRO_STORE_SLOT;
    ast->store.source_value         = source_value_ast;
    ast->store.store_slot.dest_ptr  = dest_ptr_ast;
    ast->store.store_slot.dest_slot = dest_slot;
    ast->necro_machine_type            = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_block(NecroMachineProgram* program, const char* name, NecroMachineAST* next_block)
{
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                  = NECRO_MACHINE_BLOCK;
    ast->block.name            = necro_gen_var(program, NULL, name, NECRO_NAME_NOT_UNIQUE);
    ast->block.statements      = necro_paged_arena_alloc(&program->arena, 4 * sizeof(NecroMachineAST*));
    ast->block.num_statements  = 0;
    ast->block.statements_size = 4;
    ast->block.terminator      = NULL;
    ast->block.next_block      = next_block;
    ast->necro_machine_type    = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_struct_def(NecroMachineProgram* program, NecroVar name, NecroMachineType** members, size_t num_members)
{
    NecroMachineAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type            = NECRO_MACHINE_STRUCT_DEF;
    ast->struct_def.name = name;
    ast->necro_machine_type = necro_create_machine_struct_type(&program->arena, name, members, num_members);
    necro_symtable_get(program->symtable, name.id)->necro_machine_ast = ast;
    necro_program_add_struct(program, ast);
    return ast;
}

NecroMachineAST* necro_create_machine_fn(NecroMachineProgram* program, NecroVar name, NecroMachineAST* call_body, NecroMachineType* necro_machine_type)
{
    NecroMachineAST* ast                = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                           = NECRO_MACHINE_FN_DEF;
    ast->fn_def.name                    = name;
    ast->fn_def.call_body               = call_body;
    ast->fn_def.fn_type                 = NECRO_FN_FN;
    ast->fn_def.fn_value                = necro_create_global_value(program, name, necro_machine_type);
    ast->necro_machine_type             = necro_machine_type;
    ast->fn_def.state_type              = NECRO_STATE_CONSTANT;
    assert(call_body->type == NECRO_MACHINE_BLOCK);
    necro_symtable_get(program->symtable, name.id)->necro_machine_ast = ast;
    necro_program_add_function(program, ast);
    ast->fn_def._curr_block             = call_body;
    ast->fn_def._init_block             = NULL;
    ast->fn_def._cont_block             = NULL;
    ast->fn_def._err_block              = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_runtime_fn(NecroMachineProgram* program, NecroVar name, NecroMachineType* necro_machine_type, void* runtime_fn_addr, NECRO_STATE_TYPE state_type)
{
    NecroMachineAST* ast                = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                           = NECRO_MACHINE_FN_DEF;
    ast->fn_def.name                    = name;
    ast->fn_def.fn_type                 = NECRO_FN_RUNTIME;
    ast->fn_def.call_body               = NULL;
    ast->fn_def.fn_value                = necro_create_global_value(program, name, necro_machine_type);
    ast->necro_machine_type             = necro_machine_type;
    ast->fn_def.runtime_fn_addr         = runtime_fn_addr;
    ast->fn_def.state_type              = state_type;
    necro_symtable_get(program->symtable, name.id)->necro_machine_ast = ast;
    necro_program_add_function(program, ast);
    ast->fn_def._curr_block             = NULL;;
    ast->fn_def._init_block             = NULL;
    ast->fn_def._cont_block             = NULL;
    ast->fn_def._err_block              = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_initial_machine_def(NecroMachineProgram* program, NecroVar bind_name, NecroMachineAST* outer, NecroMachineType* value_type, NecroType* necro_value_type)
{
    NecroArenaSnapshot snapshot                    = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineAST* ast                           = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                                      = NECRO_MACHINE_DEF;
    ast->machine_def.bind_name                     = bind_name;
    char itoabuf[10];
    char* machine_name                             = necro_concat_strings(&program->snapshot_arena, 4, (const char*[]) { "_", necro_intern_get_string(program->intern, bind_name.symbol), "Machine", itoa(bind_name.id.id, itoabuf, 10) });
    machine_name[1]                                = toupper(machine_name[1]);
    char* state_name                               = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { machine_name, "State" });
    ast->machine_def.machine_name                  = necro_gen_var(program, ast, machine_name, NECRO_NAME_UNIQUE);
    ast->machine_def.state_name                    = necro_gen_var(program, ast, state_name, NECRO_NAME_UNIQUE);
    ast->machine_def.arg_names                     = NULL;
    ast->machine_def.num_arg_names                 = 0;
    ast->machine_def.mk_fn                         = NULL;
    ast->machine_def.init_fn                       = NULL;
    ast->machine_def.update_fn                     = NULL;
    ast->machine_def.global_value                  = NULL;
    ast->machine_def.global_state                  = NULL;
    ast->machine_def.update_error_block            = NULL;
    ast->machine_def.state_type                    = necro_symtable_get(program->symtable, bind_name.id)->state_type;
    ast->machine_def.is_recursive                  = false;
    ast->machine_def.is_pushed                     = false;
    ast->machine_def.is_persistent_slot_set        = false;
    // ast->machine_def.most_stateful_type_referenced = NECRO_STATE_CONSTANT;
    ast->machine_def.outer                         = outer;
    ast->machine_def.data_id                       = NECRO_NULL_DATA_ID;
    ast->necro_machine_type                        = NULL;
    if (value_type->type == NECRO_MACHINE_TYPE_FN)
    {
        ast->machine_def.fn_type = value_type;
        if (value_type->fn_type.return_type->type == NECRO_MACHINE_TYPE_PTR)
            ast->machine_def.value_type = value_type->fn_type.return_type->ptr_type.element_type;
        else
            ast->machine_def.value_type = value_type->fn_type.return_type;
    }
    else
    {
        // ast->machine_def.value_type = necro_create_machine_ptr_type(&program->arena, value_type);
        ast->machine_def.value_type       = value_type;
        ast->machine_def.fn_type          = NULL;
        ast->machine_def.necro_value_type = necro_value_type;
    }
    const size_t initial_members_size  = 4;
    ast->machine_def.members           = necro_paged_arena_alloc(&program->arena, initial_members_size * sizeof(NecroSlot));
    ast->machine_def.num_members       = 0;
    ast->machine_def.members_size      = initial_members_size;
    ast->machine_def._first_dynamic    = -1;
    necro_symtable_get(program->symtable, bind_name.id)->necro_machine_ast = ast;
    necro_symtable_get(program->symtable, ast->machine_def.machine_name.id)->necro_machine_ast = ast;
    if (outer == 0)
        necro_program_add_machine_def(program, ast);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return ast;
}

NecroMachineAST* necro_build_non_const_gep(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST** a_indices, size_t num_indices, const char* dest_name, NecroMachineType* result_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(source_value->type == NECRO_MACHINE_VALUE);
    NecroMachineAST* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_GEP;
    ast->gep.source_value = source_value;
    NecroMachineAST** indices = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(NecroMachineAST*));
    for (size_t i = 0; i < num_indices; ++i)
        indices[i] = a_indices[i];
    ast->gep.indices        = indices;
    ast->gep.num_indices    = num_indices;
    // necro_machine_type      = necro_create_machine_ptr_type(&program->arena, necro_machine_type);
    ast->gep.dest_value     = necro_create_reg(program, result_type, dest_name);
    ast->necro_machine_type = result_type;
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->gep.dest_value;
}

NecroMachineAST* necro_build_gep(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(source_value->type == NECRO_MACHINE_VALUE);
    NecroMachineAST** indices = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(NecroMachineAST*));
    for (size_t i = 0; i < num_indices; ++i)
        indices[i] = necro_create_uint32_necro_machine_value(program, a_indices[i]);
    // type check gep
    NecroMachineType* necro_machine_type = source_value->necro_machine_type;
    for (size_t i = 0; i < num_indices; ++i)
    {
        if (necro_machine_type->type == NECRO_MACHINE_TYPE_STRUCT)
        {
            assert(a_indices[i] < (uint32_t) necro_machine_type->struct_type.num_members);
            necro_machine_type = necro_machine_type->struct_type.members[a_indices[i]];
        }
        else if (necro_machine_type->type == NECRO_MACHINE_TYPE_PTR)
        {
            assert(i == 0); // NOTE: Can only deref the first type!
            necro_machine_type = necro_machine_type->ptr_type.element_type;
        }
        else
        {
            assert(necro_machine_type->type == NECRO_MACHINE_TYPE_STRUCT || necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
        }
    }
    return necro_build_non_const_gep(program, fn_def, source_value, indices, num_indices, dest_name, necro_create_machine_ptr_type(&program->arena, necro_machine_type));
}

///////////////////////////////////////////////////////
// Build
///////////////////////////////////////////////////////
void necro_add_statement_to_block(NecroMachineProgram* program, NecroMachineAST* block, NecroMachineAST* statement)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_MACHINE_BLOCK);
    if (block->block.num_statements + 1 >= block->block.statements_size)
    {
        NecroMachineAST** old_statements = block->block.statements;
        block->block.statements       = necro_paged_arena_alloc(&program->arena, block->block.statements_size * 2 * sizeof(NecroMachineAST*));
        memcpy(block->block.statements, old_statements, block->block.statements_size * sizeof(NecroMachineAST*));
        block->block.statements_size *= 2;
    }
    block->block.statements[block->block.num_statements] = statement;
    block->block.num_statements++;
}

NecroMachineAST* necro_append_block(NecroMachineProgram* program, NecroMachineAST* fn_def, const char* block_name)
{
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def.call_body != NULL);
    NecroMachineAST* block    = necro_create_machine_block(program, block_name, NULL);
    NecroMachineAST* fn_block = fn_def->fn_def.call_body;
    while (fn_block->block.next_block != NULL)
    {
        fn_block = fn_block->block.next_block;
    }
    fn_block->block.next_block = block;
    return block;
}

NecroMachineAST* necro_insert_block_before(NecroMachineProgram* program, NecroMachineAST* fn_def, const char* block_name, NecroMachineAST* block_to_precede)
{
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(block_to_precede->type == NECRO_MACHINE_BLOCK);
    NecroMachineAST* block    = necro_create_machine_block(program, block_name, block_to_precede);
    NecroMachineAST* fn_block = fn_def->fn_def.call_body;
    if (fn_block == block_to_precede)
    {
        fn_def->fn_def.call_body = block;
        return block;
    }
    while (fn_block->block.next_block != NULL)
    {
        if (fn_block->block.next_block == block_to_precede)
        {
            fn_block->block.next_block = block;
            return block;
        }
        fn_block = fn_block->block.next_block;
    }
    assert(false && "Could not find block to insert before in function body!");
    return NULL;
}

void necro_move_to_block(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* block)
{
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(block->type == NECRO_MACHINE_BLOCK);
    NecroMachineAST* fn_block = fn_def->fn_def.call_body;
    while (fn_block != NULL)
    {
        if (fn_block->block.name.id.id == block->block.name.id.id)
        {
            fn_def->fn_def._curr_block = block;
            return;
        }
        fn_block = fn_block->block.next_block;
    }
    assert(false);
}

NecroMachineAST* necro_build_nalloc(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineType* type, uint32_t a_slots_used)
{
    assert(program != NULL);
    assert(type != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(a_slots_used > 0);
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                  = NECRO_MACHINE_NALLOC;
    ast->nalloc.type_to_alloc  = type;
    ast->nalloc.slots_used     = a_slots_used;
    NecroMachineType* type_ptr = necro_create_machine_ptr_type(&program->arena, type);
    ast->nalloc.result_reg     = necro_create_reg(program, type_ptr, "data_ptr");
    ast->necro_machine_type    = type_ptr;
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->nalloc.result_reg->type == NECRO_MACHINE_VALUE);
    assert(ast->nalloc.result_reg->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->nalloc.result_reg;
}

NecroMachineAST* necro_build_alloca(NecroMachineProgram* program, NecroMachineAST* fn_def, size_t num_slots)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(num_slots > 0);
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                  = NECRO_MACHINE_ALLOCA;
    ast->alloca.num_slots      = num_slots;
    ast->alloca.result         = necro_create_reg(program, necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type), "data_ptr");
    ast->necro_machine_type    = necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->alloca.result->type == NECRO_MACHINE_VALUE);
    assert(ast->alloca.result->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->alloca.result;
}

void necro_build_store_into_ptr(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, necro_create_machine_store_into_ptr(program, source_value, dest_ptr));
}

void necro_build_store_into_slot(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr, size_t dest_slot)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, necro_create_machine_store_into_slot(program, source_value, dest_ptr, dest_slot));
}

NecroMachineAST* necro_build_bit_cast(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* value, NecroMachineType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                = NECRO_MACHINE_BIT_CAST;
    ast->bit_cast.from_value = value;
    ast->bit_cast.to_value   = necro_create_reg(program, to_type, "cst");
    ast->necro_machine_type  = to_type;
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->bit_cast.to_value->type == NECRO_MACHINE_VALUE);
    assert(ast->bit_cast.to_value->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->bit_cast.to_value;
}

NecroMachineAST* necro_build_zext(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* value, NecroMachineType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(
        value->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT1  ||
        value->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT8  ||
        value->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT16 ||
        value->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT32 ||
        value->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT64 ||

        value->necro_machine_type->type == NECRO_MACHINE_TYPE_INT32 ||
        value->necro_machine_type->type == NECRO_MACHINE_TYPE_INT64
    );
    assert(
        to_type->type == NECRO_MACHINE_TYPE_UINT1  ||
        to_type->type == NECRO_MACHINE_TYPE_UINT8  ||
        to_type->type == NECRO_MACHINE_TYPE_UINT16 ||
        to_type->type == NECRO_MACHINE_TYPE_UINT32 ||
        to_type->type == NECRO_MACHINE_TYPE_UINT64 ||

        to_type->type == NECRO_MACHINE_TYPE_INT32 ||
        to_type->type == NECRO_MACHINE_TYPE_INT64
    );
    NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                = NECRO_MACHINE_ZEXT;
    ast->zext.from_value     = value;
    ast->zext.to_value       = necro_create_reg(program, to_type, "zxt");
    ast->necro_machine_type  = to_type;
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->zext.to_value;
}

NecroMachineAST* necro_build_load_from_ptr(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_ptr_ast, const char* dest_name_header)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast = necro_create_machine_load_from_ptr(program, source_ptr_ast, dest_name_header);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->load.dest_value->type == NECRO_MACHINE_VALUE);
    assert(ast->load.dest_value->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->load.dest_value;
}

NecroMachineAST* necro_build_load_from_slot(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_ptr_ast, size_t source_slot, const char* dest_name_header)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast = necro_create_machine_load_from_slot(program, source_ptr_ast, source_slot, dest_name_header);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->load.dest_value->type == NECRO_MACHINE_VALUE);
    assert(ast->load.dest_value->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->load.dest_value;
}

NecroMachineAST* necro_build_call(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* fn_value_ast, NecroMachineAST** a_parameters, size_t num_parameters, NECRO_MACHINE_CALL_TYPE call_type, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);

    // type_check
    NecroMachineType* fn_value_type = fn_value_ast->necro_machine_type;
    if (fn_value_type->type == NECRO_MACHINE_TYPE_PTR && fn_value_type->ptr_type.element_type->type == NECRO_MACHINE_TYPE_FN)
        fn_value_type = fn_value_type->ptr_type.element_type;
    assert(fn_value_ast->type == NECRO_MACHINE_VALUE);
    assert(fn_value_type->type == NECRO_MACHINE_TYPE_FN);
    assert(fn_value_type->fn_type.num_parameters == num_parameters);
    for (size_t i = 0; i < num_parameters; i++)
    {
        necro_type_check(program, fn_value_type->fn_type.parameters[i], a_parameters[i]->necro_machine_type);
    }
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                  = NECRO_MACHINE_CALL;
    ast->call.fn_value         = fn_value_ast;
    ast->call.call_type        = call_type;
    NecroMachineAST** parameters = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroMachineAST*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachineAST*));
    ast->call.parameters       = parameters;
    ast->call.num_parameters   = num_parameters;
    if (fn_value_type->fn_type.return_type->type == NECRO_MACHINE_TYPE_VOID)
        ast->call.result_reg = necro_create_machine_value_ast(program, (NecroMachineValue) { .value_type = NECRO_MACHINE_VALUE_VOID }, fn_value_type->fn_type.return_type);
    else
        ast->call.result_reg = necro_create_reg(program, fn_value_type->fn_type.return_type, dest_name);
    ast->necro_machine_type    = fn_value_type->fn_type.return_type;

    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->call.result_reg->type == NECRO_MACHINE_VALUE);
    assert(ast->call.result_reg->value.value_type == NECRO_MACHINE_VALUE_REG || ast->call.result_reg->value.value_type == NECRO_MACHINE_VALUE_VOID);
    return ast->call.result_reg;
}

NecroMachineAST* necro_build_binop(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* left, NecroMachineAST* right, NECRO_MACHINE_BINOP_TYPE op_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);

    NecroMachineAST* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type            = NECRO_MACHINE_BINOP;
    assert(left->type == NECRO_MACHINE_VALUE);
    assert(right->type == NECRO_MACHINE_VALUE);
    necro_type_check(program, left->necro_machine_type, right->necro_machine_type);
    // typecheck
    switch (op_type)
    {
    case NECRO_MACHINE_BINOP_IADD: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_ISUB: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_IMUL: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_IDIV: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_OR:
    case NECRO_MACHINE_BINOP_AND:
    case NECRO_MACHINE_BINOP_SHL:
    case NECRO_MACHINE_BINOP_SHR:
    {
        // Type check that it's an int type
        // necro_type_check(program, left->necro_machine_type, program->necro_int_type);
        // necro_type_check(program, right->necro_machine_type, program->necro_int_type);
        // ast->necro_machine_type = program->necro_int_type;
        ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_create_reg(program, ast->necro_machine_type, "iop");
        break;
    }
    case NECRO_MACHINE_BINOP_FADD: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_FSUB: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_FMUL: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_FDIV: /* FALL THROUGH */
    {
        // Type check that it's a float type
        // necro_type_check(program, left->necro_machine_type, program->necro_float_type);
        // necro_type_check(program, right->necro_machine_type, program->necro_float_type);
        // ast->necro_machine_type = program->necro_float_type;
        ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
    default:
        assert(false);
    }
    ast->binop.binop_type = op_type;
    ast->binop.left       = left;
    ast->binop.right      = right;
    // return ast;

    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->binop.result->type == NECRO_MACHINE_VALUE);
    assert(ast->binop.result->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->binop.result;
}

void necro_build_return(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* return_value)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACHINE_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(return_value != NULL);
    assert(return_value->type == NECRO_MACHINE_VALUE);
    necro_type_check(program, fn_def->fn_def.fn_value->necro_machine_type->fn_type.return_type, return_value->necro_machine_type);
    fn_def->fn_def._curr_block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                           = NECRO_TERM_RETURN;
    fn_def->fn_def._curr_block->block.terminator->return_terminator.return_value = return_value;
}

void necro_build_return_void(NecroMachineProgram* program, NecroMachineAST* fn_def)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACHINE_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(fn_def->fn_def.fn_value->necro_machine_type->fn_type.return_type->type == NECRO_MACHINE_TYPE_VOID);
    fn_def->fn_def._curr_block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                           = NECRO_TERM_RETURN_VOID;
}

void necro_build_break(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* block_to_jump_to)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACHINE_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(block_to_jump_to != NULL);
    assert(block_to_jump_to->type == NECRO_MACHINE_BLOCK);
    fn_def->fn_def._curr_block->block.terminator                                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                              = NECRO_TERM_BREAK;
    fn_def->fn_def._curr_block->block.terminator->break_terminator.block_to_jump_to = block_to_jump_to;
}

void necro_build_cond_break(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* cond, NecroMachineAST* true_block, NecroMachineAST* false_block)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACHINE_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(cond != NULL);
    assert(cond->type == NECRO_MACHINE_VALUE);
    assert(cond->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT1);
    assert(true_block != NULL);
    assert(true_block->type == NECRO_MACHINE_BLOCK);
    assert(false_block != NULL);
    assert(false_block->type == NECRO_MACHINE_BLOCK);
    fn_def->fn_def._curr_block->block.terminator                                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                              = NECRO_TERM_COND_BREAK;
    fn_def->fn_def._curr_block->block.terminator->cond_break_terminator.cond_value  = cond;
    fn_def->fn_def._curr_block->block.terminator->cond_break_terminator.true_block  = true_block;
    fn_def->fn_def._curr_block->block.terminator->cond_break_terminator.false_block = false_block;
}

NecroMachineAST* necro_build_cmp(NecroMachineProgram* program, NecroMachineAST* fn_def, NECRO_MACHINE_CMP_TYPE cmp_type, NecroMachineAST* left, NecroMachineAST* right)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACHINE_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(left != NULL);
    assert(left->type == NECRO_MACHINE_VALUE);
    assert(right != NULL);
    assert(right->type == NECRO_MACHINE_VALUE);
    assert(left->type == NECRO_MACHINE_TYPE_UINT1  ||
           left->type == NECRO_MACHINE_TYPE_UINT8  ||
           left->type == NECRO_MACHINE_TYPE_UINT16 ||
           left->type == NECRO_MACHINE_TYPE_UINT32 ||
           left->type == NECRO_MACHINE_TYPE_UINT64 ||
           left->type == NECRO_MACHINE_TYPE_INT32  ||
           left->type == NECRO_MACHINE_TYPE_INT64  ||
           left->type == NECRO_MACHINE_TYPE_F32    ||
           left->type == NECRO_MACHINE_TYPE_F64    ||
           left->type == NECRO_MACHINE_TYPE_PTR);
    assert(left->type == right->type);
    NecroMachineAST* cmp = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    cmp->type         = NECRO_MACHINE_CMP;
    cmp->cmp.cmp_type = cmp_type;
    cmp->cmp.left     = left;
    cmp->cmp.right    = right;
    cmp->cmp.result   = necro_create_reg(program, necro_create_machine_uint1_type(&program->arena), "cmp");
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, cmp);
    assert(cmp->cmp.result->type == NECRO_MACHINE_VALUE);
    assert(cmp->cmp.result->value.value_type == NECRO_MACHINE_VALUE_REG);
    return cmp->cmp.result;
}

NecroMachineAST* necro_build_phi(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineType* type, NecroMachinePhiList* values)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block->block.num_statements == 0 ||
           fn_def->fn_def._curr_block->block.statements[fn_def->fn_def._curr_block->block.num_statements - 1]->type == NECRO_MACHINE_PHI); // Verify at beginning of block or preceded by a phi machine
    NecroMachineAST* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_PHI;
    ast->phi.values       = values;
    ast->phi.result       = necro_create_reg(program, type, "phi");
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->phi.result;
}

NecroMachineAST* necro_build_maybe_cast(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* ast, NecroMachineType* type_to_match)
{
    necro_type_check(program, ast->necro_machine_type, type_to_match);
    if (is_poly_ptr(program, ast->necro_machine_type) ^ is_poly_ptr(program, type_to_match)) // XOR
        return necro_build_bit_cast(program, fn_def, ast, type_to_match);
    else
        return ast;
}

void necro_add_incoming_to_phi(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* phi, NecroMachineAST* block, NecroMachineAST* value)
{
    assert(program != NULL);
    assert(phi != NULL);
    assert(phi->type == NECRO_MACHINE_PHI);
    assert(block != NULL);
    assert(block->type == NECRO_MACHINE_BLOCK);
    assert(value != NULL);
    necro_type_check(program, phi->phi.result->necro_machine_type, value->necro_machine_type);
    value = necro_build_maybe_cast(program, fn_def, value, phi->phi.result->necro_machine_type);
    phi->phi.values = necro_cons_machine_phi_list(&program->arena, (NecroMachinePhiData) { .block = block, .value = value }, phi->phi.values);
}

struct NecroSwitchTerminator* necro_build_switch(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* choice_val, NecroMachineSwitchList* values, NecroMachineAST* else_block)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(choice_val != NULL);
    assert(choice_val->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT32);
    fn_def->fn_def._curr_block->block.terminator                               = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                         = NECRO_TERM_SWITCH;
    fn_def->fn_def._curr_block->block.terminator->switch_terminator.choice_val = choice_val;
    fn_def->fn_def._curr_block->block.terminator->switch_terminator.values     = values;
    fn_def->fn_def._curr_block->block.terminator->switch_terminator.else_block = else_block;
    return &fn_def->fn_def._curr_block->block.terminator->switch_terminator;
}

void necro_add_case_to_switch(NecroMachineProgram* program, struct NecroSwitchTerminator* switch_term, NecroMachineAST* block, size_t value)
{
    switch_term->values = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = block, .value = value }, switch_term->values);
}

NecroMachineAST* necro_build_select(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* cmp_value, NecroMachineAST* left, NecroMachineAST* right)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(cmp_value->type == NECRO_MACHINE_TYPE_UINT1);
    necro_type_check(program, left->necro_machine_type, right->necro_machine_type);
    NecroMachineAST* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_SELECT;
    ast->select.cmp_value = cmp_value;
    ast->select.left      = left;
    ast->select.right     = right;
    ast->select.result    = necro_create_reg(program, left->necro_machine_type, "sel_result");
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->select.result;
}

void necro_build_unreachable(NecroMachineProgram* program, NecroMachineAST* fn_def)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    fn_def->fn_def._curr_block->block.terminator       = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type = NECRO_TERM_UNREACHABLE;
}

void necro_build_memcpy(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* dest, NecroMachineAST* source, NecroMachineAST* num_bytes)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(dest->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(source->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    necro_type_check(program, dest->necro_machine_type, source->necro_machine_type);
    assert(necro_is_word_uint_type(program, num_bytes->necro_machine_type));
    NecroMachineAST* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_MEMCPY;
    ast->memcpy.dest      = dest;
    ast->memcpy.source    = source;
    ast->memcpy.num_bytes = num_bytes;
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
}

void necro_build_memset(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* ptr, NecroMachineAST* value, NecroMachineAST* num_bytes)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(ptr->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(value->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT8);
    assert(necro_is_word_uint_type(program, num_bytes->necro_machine_type));
    NecroMachineAST* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_MEMSET;
    ast->memset.ptr       = ptr;
    ast->memset.value     = value;
    ast->memset.num_bytes = num_bytes;
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
}


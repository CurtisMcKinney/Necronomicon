/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_ast.h"
#include "mach_type.h"
#include "runtime/runtime.h"
#include <ctype.h>
#include <math.h>

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroMachSlot necro_mach_slot_empty()
{
    return (NecroMachSlot) { .slot_num = 0, .necro_machine_type = NULL, .machine_def = NULL, .slot_ast = NULL, .const_init_value = NULL };
}

NecroMachAstSymbol* necro_mach_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name)
{
    NecroMachAstSymbol* symbol   = necro_paged_arena_alloc(arena, sizeof(NecroMachAstSymbol));
    symbol->name                 = name;
    symbol->ast                  = NULL;
    symbol->mach_type            = NULL;
    symbol->necro_type           = NULL;
    symbol->state_type           = NECRO_STATE_CONSTANT;
    symbol->con_num              = 0;
    symbol->is_enum              = false;
    symbol->is_constructor       = false;
    symbol->is_primitive         = false;
    symbol->is_unboxed           = false;
    symbol->is_deep_copy_fn      = false;
    symbol->primop_type          = NECRO_PRIMOP_NONE;
    symbol->codegen_symbol       = NULL;
    symbol->global_string_symbol = NULL;
    return symbol;
}

NecroMachAstSymbol* necro_mach_ast_symbol_create_from_core_ast_symbol(NecroPagedArena* arena, NecroCoreAstSymbol* core_ast_symbol)
{
    if (core_ast_symbol->mach_symbol != NULL)
        return core_ast_symbol->mach_symbol;
    NecroMachAstSymbol* symbol   = necro_paged_arena_alloc(arena, sizeof(NecroMachAstSymbol));
    symbol->name                 = core_ast_symbol->name;
    symbol->ast                  = NULL;
    symbol->mach_type            = NULL;
    symbol->necro_type           = core_ast_symbol->type;
    symbol->state_type           = core_ast_symbol->state_type;
    symbol->con_num              = core_ast_symbol->con_num;
    // symbol->is_enum              = core_ast_symbol->is_enum;
    symbol->is_enum              = false; // HACK: CoreAstSymbol is_enum seems to be coming in wrong for some reason, track this down.
    symbol->is_constructor       = core_ast_symbol->is_constructor;
    symbol->is_primitive         = core_ast_symbol->is_primitive;
    symbol->is_unboxed           = core_ast_symbol->is_unboxed;
    symbol->is_deep_copy_fn      = core_ast_symbol->is_deep_copy_fn;
    symbol->primop_type          = core_ast_symbol->primop_type;
    symbol->codegen_symbol       = NULL;
    symbol->global_string_symbol = NULL;
    core_ast_symbol->mach_symbol = symbol;
    return symbol;
}

NecroMachAstSymbol* necro_mach_ast_symbol_gen(NecroMachProgram* program, NecroMachAst* ast, const char* str, NECRO_MANGLE_TYPE mangle_type)
{
    NecroSymbol name = NULL;
    if (mangle_type == NECRO_MANGLE_NAME)
    {
        name = necro_intern_unique_string(program->intern, str);
    }
    else
    {
        name = necro_intern_string(program->intern, str);
    }

    NecroMachAstSymbol* symbol = necro_mach_ast_symbol_create(&program->arena, name);
    symbol->ast                = ast;
    return symbol;
}

void necro_mach_program_add_struct(NecroMachProgram* program, NecroMachAst* struct_ast)
{
    assert(struct_ast->type == NECRO_MACH_STRUCT_DEF);
    necro_push_necro_mach_ast_vector(&program->structs, &struct_ast);
}

void necro_mach_program_add_global(NecroMachProgram* program, NecroMachAst* global)
{
    assert(global->type == NECRO_MACH_VALUE);
    assert(global->value.value_type == NECRO_MACH_VALUE_GLOBAL);
    necro_push_necro_mach_ast_vector(&program->globals, &global);
}

void necro_mach_program_add_function(NecroMachProgram* program, NecroMachAst* function)
{
    assert(function->type == NECRO_MACH_FN_DEF);
    necro_push_necro_mach_ast_vector(&program->functions, &function);
}

void necro_mach_program_add_machine_def(NecroMachProgram* program, NecroMachAst* machine_def)
{
    assert(machine_def->type == NECRO_MACH_DEF);
    necro_push_necro_mach_ast_vector(&program->machine_defs, &machine_def);
}

///////////////////////////////////////////////////////
// Ast construction
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_value_create(NecroMachProgram* program, NecroMachValue value, NecroMachType* necro_machine_type)
{
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_VALUE;
    ast->value              = value;
    ast->necro_machine_type = necro_machine_type;
    switch (value.value_type)
    {
    case NECRO_MACH_VALUE_REG:
        value.reg_symbol->ast = ast;
        break;
    default:
        break;
    }
    return ast;
}

// NecroMachAst* necro_mach_value_create_tuple(NecroMachProgram* program, NecroMachAst** values, size_t num_values)
// {
//     NecroMachAst* ast           = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
//     ast->type                   = NECRO_MACH_VALUE;
//     ast->value.value_type       = NECRO_MACH_VALUE_TUPLE;
//     ast->necro_machine_type     = NULL;
//     ast->value.tuple.values     = values;
//     ast->value.tuple.num_values = num_values;
//     return ast;
// }

NecroMachAst* necro_mach_value_create_undefined(NecroMachProgram* program, struct NecroMachType* necro_machine_type)
{
    return necro_mach_value_create(program, (NecroMachValue) { .value_type = NECRO_MACH_VALUE_UNDEFINED }, necro_machine_type);
}

NecroMachAst* necro_mach_value_create_reg(NecroMachProgram* program, NecroMachType* necro_machine_type, const char* reg_name)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .reg_symbol      = necro_mach_ast_symbol_gen(program, NULL, reg_name, NECRO_MANGLE_NAME),
        .value_type      = NECRO_MACH_VALUE_REG,
    }, necro_machine_type);
}

NecroMachAst* necro_mach_value_create_global(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachType* necro_machine_type)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .global_symbol = symbol,
        .value_type    = NECRO_MACH_VALUE_GLOBAL,
    }, necro_machine_type);
}

NecroMachAst* necro_mach_value_create_param_reg(NecroMachProgram* program, NecroMachAst* fn_def, size_t param_num)
{
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->necro_machine_type->type == NECRO_MACH_TYPE_FN);
    assert(fn_def->necro_machine_type->fn_type.num_parameters > param_num);
    // necro_mach_type_check(program, fn_ast->necro_machine_type->fn_type.parameters[param_num], necro_machine_type);
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .param_reg       = { .fn_symbol = fn_def->fn_def.symbol, .param_num = param_num },
        .value_type      = NECRO_MACH_VALUE_PARAM,
    }, fn_def->necro_machine_type->fn_type.parameters[param_num]);
}


NecroMachAst* necro_mach_value_create_uint1(NecroMachProgram* program, bool uint1_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint1_literal = uint1_literal,
        .value_type    = NECRO_MACH_VALUE_UINT1_LITERAL,
    }, necro_mach_type_create_uint1(program));
}

NecroMachAst* necro_mach_value_create_uint8(NecroMachProgram* program, uint8_t uint8_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint8_literal = uint8_literal,
        .value_type    = NECRO_MACH_VALUE_UINT8_LITERAL,
    }, necro_mach_type_create_uint8(program));
}

NecroMachAst* necro_mach_value_create_uint16(NecroMachProgram* program, uint16_t uint16_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint16_literal  = uint16_literal,
        .value_type      = NECRO_MACH_VALUE_UINT16_LITERAL,
    }, necro_mach_type_create_uint16(program));
}

NecroMachAst* necro_mach_value_create_uint32(NecroMachProgram* program, uint32_t uint32_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint32_literal  = uint32_literal,
        .value_type      = NECRO_MACH_VALUE_UINT32_LITERAL,
    }, necro_mach_type_create_uint32(program));
}

NecroMachAst* necro_mach_value_create_uint64(NecroMachProgram* program, uint64_t uint64_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint64_literal  = uint64_literal,
        .value_type      = NECRO_MACH_VALUE_UINT64_LITERAL,
    }, necro_mach_type_create_uint64(program));
}

NecroMachAst* necro_mach_value_create_i32(NecroMachProgram* program, int32_t int32_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .int64_literal   = int32_literal,
        .value_type      = NECRO_MACH_VALUE_INT32_LITERAL,
    }, necro_mach_type_create_int32(program));
}

NecroMachAst* necro_mach_value_create_i64(NecroMachProgram* program, int64_t int64_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .int64_literal   = int64_literal,
        .value_type      = NECRO_MACH_VALUE_INT64_LITERAL,
    }, necro_mach_type_create_int64(program));
}

NecroMachAst* necro_mach_value_create_f32(NecroMachProgram* program, float f32_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .f32_literal = f32_literal,
        .value_type  = NECRO_MACH_VALUE_F32_LITERAL,
    }, necro_mach_type_create_f32(program));
}

NecroMachAst* necro_mach_value_create_f64(NecroMachProgram* program, double f64_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .f64_literal = f64_literal,
        .value_type  = NECRO_MACH_VALUE_F64_LITERAL,
    }, necro_mach_type_create_f64(program));
}

NecroMachAst* necro_mach_value_create_null(NecroMachProgram* program, NecroMachType* ptr_type)
{
    assert(ptr_type->type == NECRO_MACH_TYPE_PTR);
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .value_type = NECRO_MACH_VALUE_NULL_PTR_LITERAL,
    }, ptr_type);
}

NecroMachAst* necro_mach_value_create_word_uint(NecroMachProgram* program, uint64_t int_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_mach_value_create_uint32(program, (uint32_t) int_literal);
    else
        return necro_mach_value_create_uint64(program, int_literal);
}

NecroMachAst* necro_mach_value_create_word_int(NecroMachProgram* program, int64_t int_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_mach_value_create_i32(program, (int32_t) int_literal);
    else
        return necro_mach_value_create_i64(program, int_literal);
}

NecroMachAst* necro_mach_value_create_word_float(NecroMachProgram* program, double float_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_mach_value_create_f32(program, (float) float_literal);
    else
        return necro_mach_value_create_f64(program, float_literal);
}

///////////////////////////////////////////////////////
// Blocks
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_block_create(NecroMachProgram* program, const char* name, NecroMachAst* next_block)
{
    NecroMachAst* ast          = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                  = NECRO_MACH_BLOCK;
    ast->block.symbol          = necro_mach_ast_symbol_gen(program, NULL, name, NECRO_MANGLE_NAME);
    ast->block.statements      = necro_paged_arena_alloc(&program->arena, 4 * sizeof(NecroMachAst*));
    ast->block.num_statements  = 0;
    ast->block.statements_size = 4;
    ast->block.terminator      = NULL;
    ast->block.next_block      = next_block;
    ast->necro_machine_type    = NULL;
    return ast;
}

void necro_mach_block_add_statement(NecroMachProgram* program, NecroMachAst* block, NecroMachAst* statement)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_MACH_BLOCK);
    if (block->block.num_statements + 1 >= block->block.statements_size)
    {
        NecroMachAst** old_statements = block->block.statements;
        block->block.statements       = necro_paged_arena_alloc(&program->arena, block->block.statements_size * 2 * sizeof(NecroMachAst*));
        memcpy(block->block.statements, old_statements, block->block.statements_size * sizeof(NecroMachAst*));
        block->block.statements_size *= 2;
    }
    block->block.statements[block->block.num_statements] = statement;
    block->block.num_statements++;
}

NecroMachAst* necro_mach_block_append(NecroMachProgram* program, NecroMachAst* fn_def, const char* block_name)
{
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def.call_body != NULL);
    NecroMachAst* block    = necro_mach_block_create(program, block_name, NULL);
    NecroMachAst* fn_block = fn_def->fn_def.call_body;
    while (fn_block->block.next_block != NULL)
    {
        fn_block = fn_block->block.next_block;
    }
    fn_block->block.next_block = block;
    return block;
}

NecroMachAst* necro_mach_block_insert_before(NecroMachProgram* program, NecroMachAst* fn_def, const char* block_name, NecroMachAst* block_to_precede)
{
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(block_to_precede->type == NECRO_MACH_BLOCK);
    NecroMachAst* block    = necro_mach_block_create(program, block_name, block_to_precede);
    NecroMachAst* fn_block = fn_def->fn_def.call_body;
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

void necro_mach_block_move_to(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* block)
{
    UNUSED(program);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(block->type == NECRO_MACH_BLOCK);
    NecroMachAst* fn_block = fn_def->fn_def.call_body;
    while (fn_block != NULL)
    {
        if (fn_block->block.symbol == block->block.symbol)
        {
            fn_def->fn_def._curr_block = block;
            return;
        }
        fn_block = fn_block->block.next_block;
    }
    assert(false);
}

NecroMachAst* necro_mach_block_get_current(NecroMachAst* fn_def)
{
    return fn_def->fn_def._curr_block;
}

///////////////////////////////////////////////////////
// Memory
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_build_nalloc(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachType* type)
{
    assert(program != NULL);
    assert(type != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    NecroMachAst* alloc_size = necro_mach_value_create_word_uint(program, necro_mach_type_calculate_size_in_bytes(program, type));
    NecroMachAst* void_ptr   = necro_mach_build_call(program, fn_def, program->runtime.necro_runtime_alloc->ast->fn_def.fn_value, (NecroMachAst*[]) { alloc_size }, 1, NECRO_MACH_CALL_C, "void_ptr");
    NecroMachAst* data_ptr   = necro_mach_build_bit_cast(program, fn_def, void_ptr, necro_mach_type_create_ptr(&program->arena, type));
    return data_ptr;
}

NecroMachAst* necro_mach_build_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, size_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(source_value->type == NECRO_MACH_VALUE);
    NecroMachAst** indices = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(NecroMachAst*));
    for (size_t i = 0; i < num_indices; ++i)
        indices[i] = necro_mach_value_create_uint32(program, (uint32_t) a_indices[i]);
        // indices[i] = necro_mach_value_create_word_uint(program, a_indices[i]);
    // type check gep
    NecroMachType* necro_machine_type = source_value->necro_machine_type;
    for (size_t i = 0; i < num_indices; ++i)
    {
        if (necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
        {
            const size_t index = a_indices[i];
            assert(index < necro_machine_type->struct_type.num_members);
            necro_machine_type = necro_machine_type->struct_type.members[index];
        }
        else if (necro_machine_type->type == NECRO_MACH_TYPE_ARRAY)
        {
            const size_t index = a_indices[i];
            assert(index < necro_machine_type->array_type.element_count);
            necro_machine_type = necro_machine_type->array_type.element_type;
        }
        else if (necro_machine_type->type == NECRO_MACH_TYPE_PTR)
        {
            assert(i == 0); // NOTE: Can only deref the first type!
            necro_machine_type = necro_machine_type->ptr_type.element_type;
        }
        else
        {
            assert(false && "gep type error");
        }
    }
    return necro_mach_build_non_const_gep(program, fn_def, source_value, indices, num_indices, dest_name, necro_mach_type_create_ptr(&program->arena, necro_machine_type));
}

NecroMachAst* necro_mach_build_non_const_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, NecroMachAst** a_indices, size_t num_indices, const char* dest_name, NecroMachType* result_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(source_value->type == NECRO_MACH_VALUE);
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_GEP;
    ast->gep.source_value   = source_value;
    NecroMachAst** indices  = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(NecroMachAst*));
    for (size_t i = 0; i < num_indices; ++i)
        indices[i] = a_indices[i];
    ast->gep.indices        = indices;
    ast->gep.num_indices    = num_indices;
    // necro_machine_type      = necro_create_machine_ptr_type(&program->arena, necro_machine_type);
    ast->gep.dest_value     = necro_mach_value_create_reg(program, result_type, dest_name);
    ast->necro_machine_type = result_type;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    return ast->gep.dest_value;
}

NecroMachAst* necro_mach_build_insert_value(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* aggregate_value, NecroMachAst* inserted_value, size_t index, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(aggregate_value->type == NECRO_MACH_VALUE);
    assert(aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT ||
           aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_ARRAY);
    assert(inserted_value->type == NECRO_MACH_VALUE);
    if (aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
    {
        assert(index < aggregate_value->necro_machine_type->struct_type.num_members);
        necro_mach_type_check(program, aggregate_value->necro_machine_type->struct_type.members[index], inserted_value->necro_machine_type);
    }
    else
    {
        assert(index < aggregate_value->necro_machine_type->array_type.element_count);
        necro_mach_type_check(program, aggregate_value->necro_machine_type->array_type.element_type, inserted_value->necro_machine_type);
    }
    NecroMachAst*  ast                = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                         = NECRO_MACH_INSERT_VALUE;
    ast->insert_value.aggregate_value = aggregate_value;
    ast->insert_value.index           = index;
    ast->insert_value.inserted_value  = inserted_value;
    ast->insert_value.dest_value      = necro_mach_value_create_reg(program, aggregate_value->necro_machine_type, dest_name);
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    return ast->insert_value.dest_value;
}

NecroMachAst* necro_mach_build_extract_value(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* aggregate_value, size_t index, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(aggregate_value->type == NECRO_MACH_VALUE);
    assert(aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT ||
           aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_ARRAY);
    NecroMachAst*  ast            = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                     = NECRO_MACH_EXTRACT_VALUE;
    NecroMachType* extracted_type = NULL;
    if (aggregate_value->necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
    {
        assert(index < aggregate_value->necro_machine_type->struct_type.num_members);
        extracted_type = aggregate_value->necro_machine_type->struct_type.members[index];
    }
    else
    {
        assert(index < aggregate_value->necro_machine_type->array_type.element_count);
        extracted_type = aggregate_value->necro_machine_type->array_type.element_type;
    }
    ast->extract_value.aggregate_value = aggregate_value;
    ast->extract_value.index           = index;
    ast->extract_value.dest_value      = necro_mach_value_create_reg(program, extracted_type, dest_name);
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    return ast->extract_value.dest_value;
}

NecroMachAst* necro_mach_build_bit_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, NecroMachType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    NecroMachAst* ast        = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                = NECRO_MACH_BIT_CAST;
    ast->bit_cast.from_value = value;
    ast->bit_cast.to_value   = necro_mach_value_create_reg(program, to_type, "cst");
    ast->necro_machine_type  = to_type;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    assert(ast->bit_cast.to_value->type == NECRO_MACH_VALUE);
    assert(ast->bit_cast.to_value->value.value_type == NECRO_MACH_VALUE_REG);
    return ast->bit_cast.to_value;
}

NecroMachAst* necro_mach_build_up_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, NecroMachType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    if (necro_mach_type_is_eq(value->necro_machine_type, to_type))
        return value;
    NecroMachType* from_type = value->necro_machine_type;
    assert(from_type->type == NECRO_MACH_TYPE_PTR);
    assert(to_type->type == NECRO_MACH_TYPE_PTR);
    NecroMachType* from_type_struct = from_type->ptr_type.element_type;
    NecroMachType* to_type_struct   = to_type->ptr_type.element_type;
    assert(from_type_struct->type == NECRO_MACH_TYPE_STRUCT);
    assert(to_type_struct->type == NECRO_MACH_TYPE_STRUCT);
    assert(from_type_struct->struct_type.sum_type_symbol == to_type_struct->struct_type.symbol);
    return necro_mach_build_bit_cast(program, fn_def, value, to_type);
}

NecroMachAst* necro_mach_build_down_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, NecroMachType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    if (necro_mach_type_is_eq(value->necro_machine_type, to_type))
        return value;
    NecroMachType* from_type = value->necro_machine_type;
    assert(from_type->type == NECRO_MACH_TYPE_PTR);
    assert(to_type->type == NECRO_MACH_TYPE_PTR);
    NecroMachType* from_type_struct = from_type->ptr_type.element_type;
    NecroMachType* to_type_struct   = to_type->ptr_type.element_type;
    assert(from_type_struct->type == NECRO_MACH_TYPE_STRUCT);
    assert(to_type_struct->type == NECRO_MACH_TYPE_STRUCT);
    assert(from_type_struct->struct_type.symbol == to_type_struct->struct_type.sum_type_symbol);
    return necro_mach_build_bit_cast(program, fn_def, value, to_type);
}

NecroMachAst* necro_mach_build_maybe_bit_cast(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, NecroMachType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    if (necro_mach_type_is_eq(value->necro_machine_type, to_type))
        return value;
    else
        return necro_mach_build_bit_cast(program, fn_def, value, to_type);
}

NecroMachAst* necro_mach_build_zext(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* value, NecroMachType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT1  ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT8  ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT16 ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT32 ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_UINT64 ||

        value->necro_machine_type->type == NECRO_MACH_TYPE_INT32 ||
        value->necro_machine_type->type == NECRO_MACH_TYPE_INT64
    );
    assert(
        to_type->type == NECRO_MACH_TYPE_UINT1  ||
        to_type->type == NECRO_MACH_TYPE_UINT8  ||
        to_type->type == NECRO_MACH_TYPE_UINT16 ||
        to_type->type == NECRO_MACH_TYPE_UINT32 ||
        to_type->type == NECRO_MACH_TYPE_UINT64 ||

        to_type->type == NECRO_MACH_TYPE_INT32 ||
        to_type->type == NECRO_MACH_TYPE_INT64
    );
    NecroMachAst* ast        = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                = NECRO_MACH_ZEXT;
    ast->zext.from_value     = value;
    ast->zext.to_value       = necro_mach_value_create_reg(program, to_type, "zxt");
    ast->necro_machine_type  = to_type;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    return ast->zext.to_value;
}

void necro_mach_build_memcpy(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* dest, NecroMachAst* source, NecroMachAst* num_bytes)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(dest->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(source->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(necro_mach_type_is_word_uint(program, num_bytes->necro_machine_type));
    necro_mach_type_check(program, dest->necro_machine_type, source->necro_machine_type);
    NecroMachAst* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type             = NECRO_MACH_MEMCPY;
    ast->memcpy.dest      = dest;
    ast->memcpy.source    = source;
    ast->memcpy.num_bytes = num_bytes;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
}

void necro_mach_build_memset(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* ptr, NecroMachAst* value, NecroMachAst* num_bytes)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(ptr->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(value->necro_machine_type->type == NECRO_MACH_TYPE_UINT8);
    assert(necro_mach_type_is_word_uint(program, num_bytes->necro_machine_type));
    NecroMachAst* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type             = NECRO_MACH_MEMSET;
    ast->memset.ptr       = ptr;
    ast->memset.value     = value;
    ast->memset.num_bytes = num_bytes;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
}

void necro_mach_build_store(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, NecroMachAst* dest_ptr)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(source_value->type == NECRO_MACH_VALUE);
    assert(dest_ptr->type == NECRO_MACH_VALUE);
    assert(dest_ptr->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    necro_mach_type_check(program, source_value->necro_machine_type, dest_ptr->necro_machine_type->ptr_type.element_type);
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_STORE;
    ast->store.source_value = source_value;
    ast->store.dest_ptr     = dest_ptr;
    ast->necro_machine_type = NULL;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
}

NecroMachAst* necro_mach_build_load(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_ptr_ast, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(source_ptr_ast->type == NECRO_MACH_VALUE);
    assert(source_ptr_ast->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(source_ptr_ast->value.value_type == NECRO_MACH_VALUE_REG || source_ptr_ast->value.value_type == NECRO_MACH_VALUE_PARAM || source_ptr_ast->value.value_type == NECRO_MACH_VALUE_GLOBAL);
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_LOAD;
    ast->load.source_ptr    = source_ptr_ast;
    ast->load.dest_value    = necro_mach_value_create_reg(program, source_ptr_ast->necro_machine_type->ptr_type.element_type, dest_name);
    ast->necro_machine_type = source_ptr_ast->necro_machine_type->ptr_type.element_type;
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    return ast->load.dest_value;
}

///////////////////////////////////////////////////////
// Branching
///////////////////////////////////////////////////////
void necro_mach_build_return(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* return_value)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACH_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(return_value != NULL);
    assert(return_value->type == NECRO_MACH_VALUE);
    necro_mach_type_check(program, fn_def->fn_def.fn_value->necro_machine_type->fn_type.return_type, return_value->necro_machine_type);
    fn_def->fn_def._curr_block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                           = NECRO_MACH_TERM_RETURN;
    fn_def->fn_def._curr_block->block.terminator->return_terminator.return_value = return_value;
}

void necro_mach_build_return_void(NecroMachProgram* program, NecroMachAst* fn_def)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACH_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(fn_def->fn_def.fn_value->necro_machine_type->fn_type.return_type->type == NECRO_MACH_TYPE_VOID);
    fn_def->fn_def._curr_block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                           = NECRO_MACH_TERM_RETURN_VOID;
}

void necro_mach_build_break(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* block_to_jump_to)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACH_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(block_to_jump_to != NULL);
    assert(block_to_jump_to->type == NECRO_MACH_BLOCK);
    fn_def->fn_def._curr_block->block.terminator                                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                              = NECRO_MACH_TERM_BREAK;
    fn_def->fn_def._curr_block->block.terminator->break_terminator.block_to_jump_to = block_to_jump_to;
}

void necro_mach_build_cond_break(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* cond, NecroMachAst* true_block, NecroMachAst* false_block)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACH_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(cond != NULL);
    assert(cond->type == NECRO_MACH_VALUE);
    assert(cond->necro_machine_type->type == NECRO_MACH_TYPE_UINT1);
    assert(true_block != NULL);
    assert(true_block->type == NECRO_MACH_BLOCK);
    assert(false_block != NULL);
    assert(false_block->type == NECRO_MACH_BLOCK);
    fn_def->fn_def._curr_block->block.terminator                                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                              = NECRO_MACH_TERM_COND_BREAK;
    fn_def->fn_def._curr_block->block.terminator->cond_break_terminator.cond_value  = cond;
    fn_def->fn_def._curr_block->block.terminator->cond_break_terminator.true_block  = true_block;
    fn_def->fn_def._curr_block->block.terminator->cond_break_terminator.false_block = false_block;
}

NecroMachAst* necro_mach_build_cmp(NecroMachProgram* program, NecroMachAst* fn_def, NECRO_PRIMOP_TYPE cmp_type, NecroMachAst* left, NecroMachAst* right)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def._curr_block != NULL);
    assert(fn_def->fn_def._curr_block->type == NECRO_MACH_BLOCK);
    assert(fn_def->fn_def._curr_block->block.terminator == NULL);
    assert(left != NULL);
    assert(left->type == NECRO_MACH_VALUE);
    assert(right != NULL);
    assert(right->type == NECRO_MACH_VALUE);
    assert(
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT1  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT8  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT16 ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT32 ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_UINT64 ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_INT32  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_INT64  ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_F32    ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_F64    ||
        left->necro_machine_type->type == NECRO_MACH_TYPE_PTR);
    assert(left->necro_machine_type->type == right->necro_machine_type->type);
    NecroMachAst* cmp = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    cmp->type         = NECRO_MACH_CMP;
    cmp->cmp.cmp_type = cmp_type;
    cmp->cmp.left     = left;
    cmp->cmp.right    = right;
    cmp->cmp.result   = necro_mach_value_create_reg(program, necro_mach_type_create_uint1(program), "cmp");
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, cmp);
    assert(cmp->cmp.result->type == NECRO_MACH_VALUE);
    assert(cmp->cmp.result->value.value_type == NECRO_MACH_VALUE_REG);
    return cmp->cmp.result;
}

NecroMachAst* necro_mach_build_phi(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachType* type, NecroMachPhiList* values)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->fn_def._curr_block->block.num_statements == 0 ||
           fn_def->fn_def._curr_block->block.statements[fn_def->fn_def._curr_block->block.num_statements - 1]->type == NECRO_MACH_PHI); // Verify at beginning of block or preceded by a phi machine
    NecroMachAst* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type             = NECRO_MACH_PHI;
    ast->phi.values       = values;
    ast->phi.result       = necro_mach_value_create_reg(program, type, "phi");
    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    necro_mach_ast_type_check(program, ast);
    return ast->phi.result;
}

void necro_mach_add_incoming_to_phi(NecroMachProgram* program, NecroMachAst* phi, NecroMachAst* block, NecroMachAst* value)
{
    assert(program != NULL);
    assert(phi != NULL);
    assert(phi->type == NECRO_MACH_PHI);
    assert(block != NULL);
    assert(block->type == NECRO_MACH_BLOCK);
    assert(value != NULL);
    necro_mach_type_check(program, phi->phi.result->necro_machine_type, value->necro_machine_type);
    phi->phi.values = necro_cons_mach_phi_list(&program->arena, (NecroMachPhiData) { .block = block, .value = value }, phi->phi.values);
}

struct NecroMachSwitchTerminator* necro_mach_build_switch(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* choice_val, NecroMachSwitchList* values, NecroMachAst* else_block)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(choice_val != NULL);
    if (program->word_size == NECRO_WORD_4_BYTES)
        assert(choice_val->necro_machine_type->type == NECRO_MACH_TYPE_UINT32);
    else
        assert(choice_val->necro_machine_type->type == NECRO_MACH_TYPE_UINT64);
    fn_def->fn_def._curr_block->block.terminator                               = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                         = NECRO_MACH_TERM_SWITCH;
    fn_def->fn_def._curr_block->block.terminator->switch_terminator.choice_val = choice_val;
    fn_def->fn_def._curr_block->block.terminator->switch_terminator.values     = values;
    fn_def->fn_def._curr_block->block.terminator->switch_terminator.else_block = else_block;
    return &fn_def->fn_def._curr_block->block.terminator->switch_terminator;
}

void necro_mach_build_unreachable(NecroMachProgram* program, NecroMachAst* fn_def)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    fn_def->fn_def._curr_block->block.terminator       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachTerminator));
    fn_def->fn_def._curr_block->block.terminator->type = NECRO_MACH_TERM_UNREACHABLE;
}

void necro_mach_add_case_to_switch(NecroMachProgram* program, NecroMachSwitchTerminator* switch_term, NecroMachAst* block, size_t value)
{
    switch_term->values = necro_cons_mach_switch_list(&program->arena, (NecroMachSwitchData) { .block = block, .value = value }, switch_term->values);
}

// NecroMachAst* necro_mach_build_select(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* cmp_value, NecroMachAst* left, NecroMachAst* right)
// {
//     assert(program != NULL);
//     assert(fn_def != NULL);
//     assert(fn_def->type == NECRO_MACH_FN_DEF);
//     assert(cmp_value->type == NECRO_MACH_TYPE_UINT1);
//     necro_type_check(program, left->necro_machine_type, right->necro_machine_type);
//     NecroMachAst* ast  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
//     ast->type             = NECRO_MACH_SELECT;
//     ast->select.cmp_value = cmp_value;
//     ast->select.left      = left;
//     ast->select.right     = right;
//     ast->select.result    = necro_create_reg(program, left->necro_machine_type, "sel_result");
//     necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
//     return ast->select.result;
// }

///////////////////////////////////////////////////////
// Call
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_build_call(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* fn_value_ast, NecroMachAst** a_parameters, size_t num_parameters, NECRO_MACH_CALL_TYPE call_type, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    NecroMachType* fn_value_type = fn_value_ast->necro_machine_type;
    assert(fn_value_ast->type == NECRO_MACH_VALUE);
    assert(fn_value_type->type == NECRO_MACH_TYPE_FN);
    assert(fn_value_type->fn_type.num_parameters == num_parameters);
    for (size_t i = 0; i < num_parameters; i++)
    {
        necro_mach_type_check(program, fn_value_type->fn_type.parameters[i], a_parameters[i]->necro_machine_type);
    }
    NecroMachAst* ast         = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                 = NECRO_MACH_CALL;
    ast->call.fn_value        = fn_value_ast;
    ast->call.call_type       = call_type;
    NecroMachAst** parameters = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroMachAst*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachAst*));
    ast->call.parameters      = parameters;
    ast->call.num_parameters  = num_parameters;
    if (fn_value_type->fn_type.return_type->type == NECRO_MACH_TYPE_VOID)
        ast->call.result_reg = necro_mach_value_create(program, (NecroMachValue) { .value_type = NECRO_MACH_VALUE_VOID }, fn_value_type->fn_type.return_type);
    else
        ast->call.result_reg = necro_mach_value_create_reg(program, fn_value_type->fn_type.return_type, dest_name);
    ast->necro_machine_type = fn_value_type->fn_type.return_type;

    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    assert(ast->call.result_reg->type == NECRO_MACH_VALUE);
    assert(ast->call.result_reg->value.value_type == NECRO_MACH_VALUE_REG || ast->call.result_reg->value.value_type == NECRO_MACH_VALUE_VOID);
    return ast->call.result_reg;
}

NecroMachAst* necro_mach_build_call_intrinsic(NecroMachProgram* program, NecroMachAst* fn_def, NECRO_PRIMOP_TYPE intrinsic, NecroMachType* intrinsic_type, NecroMachAst** a_parameters, size_t num_parameters, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(intrinsic_type->type == NECRO_MACH_TYPE_FN);
    assert(intrinsic_type->fn_type.num_parameters == num_parameters);
    for (size_t i = 0; i < num_parameters; i++)
    {
        necro_mach_type_check(program, intrinsic_type->fn_type.parameters[i], a_parameters[i]->necro_machine_type);
    }
    NecroMachAst* ast                  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                          = NECRO_MACH_CALLI;
    ast->call_intrinsic.intrinsic      = intrinsic;
    ast->call_intrinsic.intrinsic_type = intrinsic_type;
    NecroMachAst** parameters          = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroMachAst*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachAst*));
    ast->call_intrinsic.parameters     = parameters;
    ast->call_intrinsic.num_parameters = num_parameters;
    if (intrinsic_type->fn_type.return_type->type == NECRO_MACH_TYPE_VOID)
        ast->call_intrinsic.result_reg = necro_mach_value_create(program, (NecroMachValue) { .value_type = NECRO_MACH_VALUE_VOID }, intrinsic_type->fn_type.return_type);
    else
        ast->call_intrinsic.result_reg = necro_mach_value_create_reg(program, intrinsic_type->fn_type.return_type, dest_name);
    ast->necro_machine_type = intrinsic_type->fn_type.return_type;

    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    assert(ast->call_intrinsic.result_reg->type == NECRO_MACH_VALUE);
    assert(ast->call_intrinsic.result_reg->value.value_type == NECRO_MACH_VALUE_REG || ast->call_intrinsic.result_reg->value.value_type == NECRO_MACH_VALUE_VOID);
    return ast->call_intrinsic.result_reg;
}

NecroMachAst* necro_mach_build_binop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* left, NecroMachAst* right, NECRO_PRIMOP_TYPE op_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);

    NecroMachAst* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type         = NECRO_MACH_BINOP;
    assert(left->type == NECRO_MACH_VALUE);
    assert(right->type == NECRO_MACH_VALUE);

	switch (op_type)
	{
    case NECRO_PRIMOP_BINOP_FSHL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSHR: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSHRA:
		break;
	default:
		necro_mach_type_check(program, left->necro_machine_type, right->necro_machine_type);
		break;
	}

    // typecheck
    switch (op_type)
    {
    case NECRO_PRIMOP_BINOP_IADD: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_ISUB: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_IMUL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_IDIV: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_IREM:
    {
        // Type check that it's an int type
        necro_mach_type_check_is_int_type(left->necro_machine_type);
        necro_mach_type_check_is_int_type(right->necro_machine_type);
        ast->necro_machine_type = left->necro_machine_type;
        // ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_mach_value_create_reg(program, ast->necro_machine_type, "iop");
        break;
    }
    case NECRO_PRIMOP_BINOP_UADD: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_USUB: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_UMUL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_UDIV: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_UREM: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_OR:   /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_AND:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_SHL:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_SHR:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_SHRA: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_XOR:
    {
        // Type check that it's an uint type
        necro_mach_type_check_is_uint_type(left->necro_machine_type);
        necro_mach_type_check_is_uint_type(right->necro_machine_type);
        ast->necro_machine_type = left->necro_machine_type;
        // ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_BINOP_FADD: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSUB: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FMUL: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FDIV: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FAND: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FOR:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FXOR: /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FREM:
    {
        // Type check that it's a float type
        necro_mach_type_check_is_float_type(left->necro_machine_type);
        necro_mach_type_check_is_float_type(right->necro_machine_type);
        ast->necro_machine_type = left->necro_machine_type;
        // ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_mach_value_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
    case NECRO_PRIMOP_BINOP_FSHL:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSHR:  /* FALL THROUGH */
    case NECRO_PRIMOP_BINOP_FSHRA:
    {
        necro_mach_type_check_is_float_type(left->necro_machine_type);
        necro_mach_type_check_is_uint_type(right->necro_machine_type);
        ast->necro_machine_type = left->necro_machine_type;
        // ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_mach_value_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
    default:
        assert(false);
    }
    ast->binop.binop_type = op_type;
    ast->binop.left       = left;
    ast->binop.right      = right;
    // return ast;

    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    assert(ast->binop.result->type == NECRO_MACH_VALUE);
    assert(ast->binop.result->value.value_type == NECRO_MACH_VALUE_REG);
    return ast->binop.result;
}

NecroMachAst* necro_mach_build_uop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* param, NecroMachType* result_type, NECRO_PRIMOP_TYPE op_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);

    NecroMachAst* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type         = NECRO_MACH_UOP;
    assert(param->type == NECRO_MACH_VALUE);
    // typecheck
    switch (op_type)
    {
    case NECRO_PRIMOP_UOP_ITOI:
    {
        if (necro_mach_type_is_eq(param->necro_machine_type, result_type))
            return param;
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        ast->necro_machine_type = result_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "iop");
        break;
    }
    case NECRO_PRIMOP_UOP_IABS: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_ISGN:
    {
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "iop");
        break;
    }
    case NECRO_PRIMOP_UOP_UABS: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_NOT:  /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_USGN:
    {
        necro_mach_type_check_is_uint_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_UOP_FSGN: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_FNOT: /* FALL THROUGH */
    case NECRO_PRIMOP_UOP_FBREV:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
	case NECRO_PRIMOP_UOP_FTOB:
	{
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type->type == NECRO_MACH_TYPE_F64 ? program->type_cache.uint64_type : program->type_cache.word_uint_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "fop");
		break;
	}
	case NECRO_PRIMOP_UOP_FFRB:
	{
        necro_mach_type_check_is_uint_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type->type == NECRO_MACH_TYPE_UINT64 ? program->type_cache.f64_type : program->type_cache.word_float_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
		break;
	}
    case NECRO_PRIMOP_UOP_ITOU:
    {
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        ast->necro_machine_type = program->type_cache.word_uint_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_UOP_ITOF:
    {
        necro_mach_type_check_is_int_type(param->necro_machine_type);
        ast->necro_machine_type = result_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_UOP_UTOI:
    {
        necro_mach_type_check_is_uint_type(param->necro_machine_type);
        ast->necro_machine_type = program->type_cache.word_int_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_UOP_FTRI:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type->type == NECRO_MACH_TYPE_F64 ? program->type_cache.int64_type : program->type_cache.word_int_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_UOP_FRNI:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        ast->necro_machine_type = param->necro_machine_type->type == NECRO_MACH_TYPE_F64 ? program->type_cache.int64_type : program->type_cache.word_int_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "uop");
        break;
    }
    case NECRO_PRIMOP_UOP_FTOF:
    {
        if (necro_mach_type_is_eq(param->necro_machine_type, result_type))
            return param;
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        ast->necro_machine_type = result_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
    case NECRO_PRIMOP_UOP_FFLR:
    {
        necro_mach_type_check_is_float_type(param->necro_machine_type);
        necro_mach_type_check_is_float_type(result_type);
        ast->necro_machine_type = result_type;
        ast->uop.result         = necro_mach_value_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
    default:
        assert(false);
    }
    ast->uop.uop_type = op_type;
    ast->uop.param    = param;

    necro_mach_block_add_statement(program, fn_def->fn_def._curr_block, ast);
    assert(ast->uop.result->type == NECRO_MACH_VALUE);
    assert(ast->uop.result->value.value_type == NECRO_MACH_VALUE_REG);
    return ast->uop.result;
}

///////////////////////////////////////////////////////
// Defs
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_create_struct_def_with_sum_type(NecroMachProgram* program, NecroMachAstSymbol* symbol, struct NecroMachType** members, size_t num_members, NecroMachAstSymbol* sum_type_symbol)
{
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_STRUCT_DEF;
    ast->struct_def.symbol  = symbol;
    ast->necro_machine_type = necro_mach_type_create_struct_with_sum_type(&program->arena, symbol, members, num_members, sum_type_symbol);
    symbol->mach_type       = ast->necro_machine_type;
    symbol->ast             = ast;
    necro_mach_program_add_struct(program, ast);
    return ast;
}

NecroMachAst* necro_mach_create_struct_def(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachType** members, size_t num_members)
{
    return necro_mach_create_struct_def_with_sum_type(program, symbol, members, num_members, NULL);
}

NecroMachAst* necro_mach_create_fn(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachAst* call_body, NecroMachType* necro_machine_type)
{
    NecroMachAst* ast                   = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                           = NECRO_MACH_FN_DEF;
    ast->fn_def.symbol                  = symbol;
    ast->fn_def.call_body               = call_body;
    ast->fn_def.fn_type                 = NECRO_MACH_FN_FN;
    ast->fn_def.fn_value                = necro_mach_value_create_global(program, symbol, necro_machine_type);
    ast->necro_machine_type             = necro_machine_type;
    // ast->fn_def.state_type              = NECRO_STATE_CONSTANT; // TODO / NOTE: Is this correct1?!?!?!!?!?!?!?
    ast->fn_def.state_type              = symbol->state_type; // TODO / NOTE: Is this correct1?!?!?!!?!?!?!?
    ast->fn_def.state_ptr               = (necro_machine_type->fn_type.num_parameters > 0) ? necro_mach_value_create_param_reg(program, ast, 0) : NULL;
    ast->fn_def._curr_block             = call_body;
    ast->fn_def._init_block             = NULL;
    ast->fn_def._cont_block             = NULL;
    ast->fn_def._err_block              = NULL;
    assert(call_body->type == NECRO_MACH_BLOCK);
    symbol->ast                         = ast;
    necro_mach_program_add_function(program, ast);
    return ast;
}

NecroMachAst* necro_mach_create_runtime_fn(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachType* necro_machine_type, NecroMachFnPtr runtime_fn_addr, NECRO_STATE_TYPE state_type)
{
    NecroMachAst* ast           = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                   = NECRO_MACH_FN_DEF;
    ast->fn_def.symbol          = symbol;
    ast->fn_def.fn_type         = NECRO_MACH_FN_RUNTIME;
    ast->fn_def.call_body       = NULL;
    ast->fn_def.fn_value        = necro_mach_value_create_global(program, symbol, necro_machine_type);
    ast->necro_machine_type     = necro_machine_type;
    ast->fn_def.runtime_fn_addr = runtime_fn_addr;
    ast->fn_def.state_type      = state_type;
    ast->fn_def.state_ptr       = NULL;
    ast->fn_def._curr_block     = NULL;;
    ast->fn_def._init_block     = NULL;
    ast->fn_def._cont_block     = NULL;
    ast->fn_def._err_block      = NULL;
    symbol->ast                 = ast;
    symbol->is_primitive        = true;
    necro_mach_program_add_function(program, ast);
    return ast;
}

NecroMachAst* necro_mach_create_initial_machine_def(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachAst* outer, struct NecroMachType* value_type, NecroType* necro_value_type)
{
    NecroArenaSnapshot snapshot                    = necro_snapshot_arena_get(&program->snapshot_arena);
    NecroMachAst* ast                              = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                                      = NECRO_MACH_DEF;
    ast->machine_def.symbol                        = symbol;
    char* machine_name                             = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "_", symbol->name->str, "Machine" });
    machine_name[1]                                = (char) toupper(machine_name[1]);
    char* state_name                               = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { machine_name, "State" });
    ast->machine_def.machine_name                  = necro_mach_ast_symbol_gen(program, ast, machine_name, NECRO_DONT_MANGLE);
    ast->machine_def.state_name                    = necro_mach_ast_symbol_gen(program, ast, state_name, NECRO_DONT_MANGLE);
    ast->machine_def.arg_names                     = NULL;
    ast->machine_def.num_arg_names                 = 0;
    ast->machine_def.mk_fn                         = NULL;
    ast->machine_def.init_fn                       = NULL;
    ast->machine_def.update_fn                     = NULL;
    ast->machine_def.global_value                  = NULL;
    ast->machine_def.global_state                  = NULL;
    ast->machine_def.update_error_block            = NULL;
    ast->machine_def.state_type                    = symbol->state_type;
    ast->machine_def.machine_name->state_type      = symbol->state_type;
    ast->machine_def.state_name->state_type        = symbol->state_type;
    ast->machine_def.outer                         = outer;
    ast->necro_machine_type                        = NULL;
    if (value_type->type == NECRO_MACH_TYPE_FN)
    {
        ast->machine_def.fn_type = value_type;
        // TODO: Why unbox result pointer types?
        // if (value_type->fn_type.return_type->type == NECRO_MACH_TYPE_PTR)
        //     ast->machine_def.value_type = value_type->fn_type.return_type->ptr_type.element_type;
        // else
            ast->machine_def.value_type = value_type->fn_type.return_type;
    }
    else
    {
        // ast->machine_def.value_type = necro_create_machine_ptr_type(&program->arena, value_type);
        ast->machine_def.value_type       = value_type;
        ast->machine_def.fn_type          = NULL;
        ast->machine_def.necro_value_type = necro_value_type;
    }
    const size_t initial_members_size    = 4;
    ast->machine_def.members             = necro_paged_arena_alloc(&program->arena, initial_members_size * sizeof(NecroMachSlot));
    ast->machine_def.num_members         = 0;
    ast->machine_def.members_size        = initial_members_size;
    symbol->ast                          = ast;
    ast->machine_def.machine_name->ast   = ast;
    if (outer == 0)
        necro_mach_program_add_machine_def(program, ast);
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return ast;
}

///////////////////////////////////////////////////////
// NecroMachRuntime
///////////////////////////////////////////////////////
NecroMachRuntime necro_mach_runtime_empty()
{
    return (NecroMachRuntime)
    {
        .necro_init_runtime        = NULL,
        .necro_update_runtime      = NULL,
        .necro_error_exit          = NULL,
        .necro_sleep               = NULL,
        .necro_print               = NULL,
        // .necro_debug_print         = NULL,
        // .necro_print_int           = NULL,
        .necro_runtime_get_mouse_x = NULL,
        .necro_runtime_get_mouse_y = NULL,
        .necro_runtime_is_done     = NULL,
        .necro_runtime_alloc       = NULL,
        .necro_runtime_free        = NULL,
    };
}

///////////////////////////////////////////////////////
// NecroMachProgram
///////////////////////////////////////////////////////
NecroMachProgram necro_mach_program_empty()
{
    return (NecroMachProgram)
    {
        .structs            = necro_empty_necro_mach_ast_vector(),
        .functions          = necro_empty_necro_mach_ast_vector(),
        .machine_defs       = necro_empty_necro_mach_ast_vector(),
        .globals            = necro_empty_necro_mach_ast_vector(),
        .necro_init         = NULL,
        .necro_main         = NULL,
        .necro_shutdown     = NULL,
        .word_size          = NECRO_WORD_4_BYTES,

        .arena              = necro_paged_arena_empty(),
        .snapshot_arena     = necro_snapshot_arena_empty(),
        .base               = NULL,
        .intern             = NULL,

        .type_cache         = necro_mach_type_cache_empty(),
        .runtime            = necro_mach_runtime_empty(),
        .program_main       = NULL,
        .clash_suffix       = 0,
    };
}

void necro_mach_program_init_base_and_runtime(NecroMachProgram* program);

NecroMachProgram necro_mach_program_create(NecroIntern* intern, NecroBase* base)
{
    NecroMachProgram program =
    {
        .structs            = necro_create_necro_mach_ast_vector(),
        .functions          = necro_create_necro_mach_ast_vector(),
        .machine_defs       = necro_create_necro_mach_ast_vector(),
        .globals            = necro_create_necro_mach_ast_vector(),
        .necro_init         = NULL,
        .necro_main         = NULL,
        .necro_shutdown     = NULL,
        .word_size          = (sizeof(char*) == 4) ? NECRO_WORD_4_BYTES : NECRO_WORD_8_BYTES,

        .arena              = necro_paged_arena_create(),
        .snapshot_arena     = necro_snapshot_arena_create(),
        .base               = base,
        .intern             = intern,

        .type_cache         = necro_mach_type_cache_empty(),
        .runtime            = necro_mach_runtime_empty(),
        .program_main       = NULL,
        .clash_suffix       = 0,
    };
    program.type_cache = necro_mach_type_cache_create(&program),
    necro_mach_program_init_base_and_runtime(&program);
    return program;
}

void necro_mach_program_destroy(NecroMachProgram* program)
{
    assert(program != NULL);
    necro_paged_arena_destroy(&program->arena);
    necro_snapshot_arena_destroy(&program->snapshot_arena);
    necro_destroy_necro_mach_ast_vector(&program->structs);
    necro_destroy_necro_mach_ast_vector(&program->globals);
    necro_destroy_necro_mach_ast_vector(&program->functions);
    necro_destroy_necro_mach_ast_vector(&program->machine_defs);
    necro_mach_type_cache_destroy(&program->type_cache);
    *program = necro_mach_program_empty();
}

void necro_mach_program_init_base_and_runtime(NecroMachProgram* program)
{
    assert(program != NULL);

    //--------------------
    // Prim Types and Ops
    //--------------------

    // Int
    {
        NecroAstSymbol*     int_type_ast_symbol  = program->base->int_type;
        int_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* int_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, int_type_ast_symbol->core_ast_symbol);
        int_type_mach_symbol->mach_type          = necro_mach_type_create_word_sized_int(program);
        int_type_mach_symbol->is_primitive       = true;
    }

    // I64
    {
        NecroAstSymbol*     int_type_ast_symbol  = program->base->int64_type;
        int_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* int_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, int_type_ast_symbol->core_ast_symbol);
        int_type_mach_symbol->mach_type          = necro_mach_type_create_int64(program);
        int_type_mach_symbol->is_primitive       = true;
    }

    // UInt
    {
        // TODO / NOTE: Do binops need special handling due to llvm not supporting an int/uint distinction?
        NecroAstSymbol*     uint_type_ast_symbol  = program->base->uint_type;
        uint_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* uint_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, uint_type_ast_symbol->core_ast_symbol);
        uint_type_mach_symbol->mach_type          = necro_mach_type_create_word_sized_uint(program);
        uint_type_mach_symbol->is_primitive       = true;
    }

    // Float
    {
        NecroAstSymbol*     float_type_ast_symbol  = program->base->float_type;
        float_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* float_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, float_type_ast_symbol->core_ast_symbol);
        float_type_mach_symbol->mach_type          = necro_mach_type_create_word_sized_float(program);
        float_type_mach_symbol->is_primitive       = true;
    }

    // F64
    {
        NecroAstSymbol*     float_type_ast_symbol  = program->base->float64_type;
        float_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* float_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, float_type_ast_symbol->core_ast_symbol);
        float_type_mach_symbol->mach_type          = necro_mach_type_create_f64(program);
        float_type_mach_symbol->is_primitive       = true;
    }

    // Char
    {
        NecroAstSymbol*     char_type_ast_symbol  = program->base->char_type;
        char_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* char_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, char_type_ast_symbol->core_ast_symbol);
        char_type_mach_symbol->mach_type          = necro_mach_type_create_word_sized_uint(program);
        char_type_mach_symbol->is_primitive       = true;
    }

    // ()
    {
        // TODO / NOTE: Do binops need special handling due to llvm not supporting an int/uint distinction?
        NecroAstSymbol*     unit_type_ast_symbol  = program->base->unit_type;
        unit_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* unit_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, unit_type_ast_symbol->core_ast_symbol);
        unit_type_mach_symbol->mach_type          = necro_mach_type_create_word_sized_uint(program);
        unit_type_mach_symbol->is_primitive       = true;
        unit_type_mach_symbol->is_enum            = true;
        NecroAstSymbol*     unit_con_ast_symbol   = program->base->unit_con;
        unit_con_ast_symbol->is_primitive         = true;
        NecroMachAstSymbol* unit_con_mach_symbol  = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, unit_con_ast_symbol->core_ast_symbol);
        unit_con_mach_symbol->is_primitive        = true;
        unit_con_mach_symbol->is_enum             = true;
    }

    // Bool
    {
        // TODO / NOTE: Do binops need special handling due to llvm not supporting an int/uint distinction?
        NecroAstSymbol*     bool_type_ast_symbol  = program->base->bool_type;
        bool_type_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* bool_type_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, bool_type_ast_symbol->core_ast_symbol);
        bool_type_mach_symbol->mach_type          = necro_mach_type_create_word_sized_uint(program);
        bool_type_mach_symbol->is_primitive       = true;
        bool_type_mach_symbol->is_enum            = true;
        NecroAstSymbol*     true_con_ast_symbol   = program->base->true_con;
        true_con_ast_symbol->is_primitive         = true;
        NecroMachAstSymbol* true_con_mach_symbol  = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, true_con_ast_symbol->core_ast_symbol);
        true_con_mach_symbol->is_primitive        = true;
        true_con_mach_symbol->is_enum             = true;
        NecroAstSymbol*     false_con_ast_symbol  = program->base->false_con;
        false_con_ast_symbol->is_primitive        = true;
        NecroMachAstSymbol* false_con_mach_symbol = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, false_con_ast_symbol->core_ast_symbol);
        false_con_mach_symbol->is_primitive       = true;
        false_con_mach_symbol->is_enum            = true;
    }

    //--------------------
    // Runtime
    //--------------------

    // getMouseX
    {
        NecroAstSymbol*     ast_symbol             = program->base->mouse_x_fn;
        ast_symbol->is_primitive                   = true;
        ast_symbol->core_ast_symbol->is_primitive  = true;
        NecroMachAstSymbol* mach_symbol            = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                  = true;
        NecroMachType*      fn_type                = necro_mach_type_create_fn(&program->arena, program->type_cache.word_int_type, (NecroMachType*[]){program->type_cache.word_uint_type}, 1);
        program->runtime.necro_runtime_get_mouse_x = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_get_mouse_x, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // getMouseY
    {
        NecroAstSymbol*     ast_symbol             = program->base->mouse_y_fn;
        ast_symbol->is_primitive                   = true;
        ast_symbol->core_ast_symbol->is_primitive  = true;
        NecroMachAstSymbol* mach_symbol            = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                  = true;
        NecroMachType*      fn_type                = necro_mach_type_create_fn(&program->arena, program->type_cache.word_int_type, (NecroMachType*[]){program->type_cache.word_uint_type}, 1);
        program->runtime.necro_runtime_get_mouse_y = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_get_mouse_y, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_init
    {
        NecroMachAstSymbol* mach_symbol     = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_init", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive           = true;
        NecroMachType*      fn_type         = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), NULL, 0);
        program->runtime.necro_init_runtime = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_init, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_update
    {
        NecroMachAstSymbol* mach_symbol       = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_update", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive             = true;
        NecroMachType*      fn_type           = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), NULL, 0);
        program->runtime.necro_update_runtime = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_update, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_test_assertion
    {
        NecroAstSymbol*     ast_symbol                = program->base->test_assertion;
        ast_symbol->is_primitive                      = true;
        ast_symbol->core_ast_symbol->is_primitive     = true;
        NecroMachAstSymbol* mach_symbol               = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                     = true;
        NecroMachType*      fn_type                   = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_uint_type, program->type_cache.word_uint_type }, 2);
        program->runtime.necro_runtime_test_assertion = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_test_assertion, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_print_int
    {
        NecroAstSymbol*     ast_symbol            = program->base->print_int;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_int_type, program->type_cache.word_uint_type }, 2);
        if (program->word_size == NECRO_WORD_4_BYTES)
            necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr)necro_runtime_print_i32, NECRO_STATE_POINTWISE);
        else
            necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr)necro_runtime_print_i64, NECRO_STATE_POINTWISE);
    }

    // necro_runtime_print_i64
    {
        NecroAstSymbol*     ast_symbol            = program->base->print_i64;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.int64_type, program->type_cache.word_uint_type }, 2);
        program->runtime.necro_print_i64          = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_print_i64, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_print_uint
    {
        NecroAstSymbol*     ast_symbol            = program->base->print_uint;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_uint_type, program->type_cache.word_uint_type }, 2);
        if (program->word_size == NECRO_WORD_4_BYTES)
            necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr)necro_runtime_print_u32, NECRO_STATE_POINTWISE);
        else
            necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr)necro_runtime_print_u64, NECRO_STATE_POINTWISE);
    }

    // necro_runtime_print_float
    {
        NecroAstSymbol*     ast_symbol            = program->base->print_float;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_float_type, program->type_cache.word_uint_type }, 2);
        if (program->word_size == NECRO_WORD_4_BYTES)
            necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr)necro_runtime_print_f32, NECRO_STATE_POINTWISE);
        else
            necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr)necro_runtime_print_f64, NECRO_STATE_POINTWISE);
    }

    // necro_runtime_print_f64
    {
        NecroAstSymbol*     ast_symbol            = program->base->print_f64;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.f64_type, program->type_cache.word_uint_type }, 2);
        necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_print_f64, NECRO_STATE_POINTWISE);
    }

    // necro_runtime_print_char
    {
        NecroAstSymbol*     ast_symbol            = program->base->print_char;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_uint_type, program->type_cache.word_uint_type }, 2);
        program->runtime.necro_print_char         = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_print_char, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_error_exit
    {
        NecroMachAstSymbol* mach_symbol   = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_error_exit", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive         = true;
        NecroMachType*      fn_type       = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_void(program), (NecroMachType*[]) { necro_mach_type_create_uint32(program) }, 1);
        program->runtime.necro_error_exit = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_error_exit, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_is_done
    {
        NecroMachAstSymbol* mach_symbol        = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_is_done", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive              = true;
        NecroMachType*      fn_type            = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_word_sized_uint(program), NULL, 0);
        program->runtime.necro_runtime_is_done = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_is_done, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_alloc
    {
        NecroMachAstSymbol* mach_symbol      = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_alloc", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive            = true;
        NecroMachType*      fn_type          = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_uint8(program)), (NecroMachType*[]) { necro_mach_type_create_word_sized_uint(program) }, 1);
        program->runtime.necro_runtime_alloc = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_alloc, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_realloc
    {
        NecroMachAstSymbol* mach_symbol        = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_realloc", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive              = true;
        NecroMachType*      fn_type            = necro_mach_type_create_fn(&program->arena, necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_uint8(program)), (NecroMachType*[]) { necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_uint8(program)), necro_mach_type_create_word_sized_uint(program) }, 2);
        program->runtime.necro_runtime_realloc = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_realloc, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // necro_runtime_free
    {
        NecroMachAstSymbol* mach_symbol     = necro_mach_ast_symbol_gen(program, NULL, "necro_runtime_free", NECRO_DONT_MANGLE);
        mach_symbol->is_primitive           = true;
        NecroMachType*      fn_type         = necro_mach_type_create_fn(&program->arena, program->type_cache.void_type, (NecroMachType*[]) { necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_uint8(program)) }, 1);
        program->runtime.necro_runtime_free = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_free, NECRO_STATE_POINTWISE)->fn_def.symbol;
    }

    // outAudioBlock
    {
        NecroAstSymbol*     ast_symbol                 = program->base->out_audio_block;
        ast_symbol->is_primitive                       = true;
        ast_symbol->core_ast_symbol->is_primitive      = true;
        NecroMachAstSymbol* mach_symbol                = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                      = true;
        NecroMachType*      audio_block_type           = necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_array(&program->arena, program->type_cache.f64_type, necro_runtime_get_block_size()));
        NecroMachType*      fn_type                    = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_uint_type, audio_block_type, program->type_cache.word_uint_type }, 3);
        program->runtime.necro_runtime_out_audio_block = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_out_audio_block, NECRO_STATE_STATEFUL)->fn_def.symbol;
    }

    // // printAudioBlock
    // {
    //     NecroAstSymbol*     ast_symbol                   = program->base->print_audio_block;
    //     ast_symbol->is_primitive                         = true;
    //     ast_symbol->core_ast_symbol->is_primitive        = true;
    //     NecroMachAstSymbol* mach_symbol                  = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
    //     mach_symbol->is_primitive                        = true;
    //     NecroMachType*      audio_block_type             = necro_mach_type_create_ptr(&program->arena, necro_mach_type_create_array(&program->arena, program->type_cache.f64_type, necro_runtime_get_block_size()));
    //     NecroMachType*      fn_type                      = necro_mach_type_create_fn(&program->arena, program->type_cache.word_uint_type, (NecroMachType*[]) { program->type_cache.word_uint_type, audio_block_type, program->type_cache.word_uint_type }, 3);
    //     program->runtime.necro_runtime_print_audio_block = necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) necro_runtime_print_audio_block, NECRO_STATE_STATEFUL)->fn_def.symbol;
    // }

    // fast_floor
    {
        NecroAstSymbol*     ast_symbol            = program->base->fast_floor;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
    }

    // fma
    {
        NecroAstSymbol*     ast_symbol            = program->base->fma;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
    }

    // floor
    {
        NecroAstSymbol*     ast_symbol            = program->base->floor;
        ast_symbol->is_primitive                  = true;
        ast_symbol->core_ast_symbol->is_primitive = true;
        NecroMachAstSymbol* mach_symbol           = necro_mach_ast_symbol_create_from_core_ast_symbol(&program->arena, ast_symbol->core_ast_symbol);
        mach_symbol->is_primitive                 = true;
        NecroMachType*      fn_type               = necro_mach_type_create_fn(&program->arena, program->type_cache.f64_type, (NecroMachType*[]) { program->type_cache.f64_type }, 1);
        necro_mach_create_runtime_fn(program, mach_symbol, fn_type, (NecroMachFnPtr) floor, NECRO_STATE_POINTWISE);
    }

}

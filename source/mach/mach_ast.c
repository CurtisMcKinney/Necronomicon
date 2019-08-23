/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_ast.h"
#include "mach_type.h"
#include <ctype.h>

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroMachAstSymbol* necro_mach_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name)
{
    NecroMachAstSymbol* symbol = necro_paged_arena_alloc(arena, sizeof(NecroMachAstSymbol));
    symbol->name               = name;
    symbol->ast                = NULL;
    symbol->mach_type          = NULL;
    symbol->necro_type         = NULL;
    symbol->state_type         = NECRO_STATE_POINTWISE;
    return symbol;
}

NecroMachAstSymbol* necro_mach_ast_symbol_gen(NecroMachProgram* program, NecroMachAst* ast, const char* str, NECRO_MANGLE_TYPE mangle_type)
{
    NecroSymbol name = NULL;
    if (mangle_type == NECRO_MANGLE_NAME)
    {
        // TODO: API Change, fix
        // name = necro_intern_unique_string(program->intern, str);
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
// Forward Declarations
///////////////////////////////////////////////////////


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
    }, necro_mach_type_create_uint1(&program->arena));
}

NecroMachAst* necro_mach_value_create_uint8(NecroMachProgram* program, uint8_t uint8_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint8_literal = uint8_literal,
        .value_type    = NECRO_MACH_VALUE_UINT8_LITERAL,
    }, necro_mach_type_create_uint8(&program->arena));
}

NecroMachAst* necro_mach_value_create_uint16(NecroMachProgram* program, uint16_t uint16_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint16_literal  = uint16_literal,
        .value_type      = NECRO_MACH_VALUE_UINT16_LITERAL,
    }, necro_mach_type_create_uint16(&program->arena));
}

NecroMachAst* necro_mach_value_create_uint32(NecroMachProgram* program, uint32_t uint32_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint32_literal  = uint32_literal,
        .value_type      = NECRO_MACH_VALUE_UINT32_LITERAL,
    }, necro_mach_type_create_uint32(&program->arena));
}

NecroMachAst* necro_mach_value_create_uint64(NecroMachProgram* program, uint64_t uint64_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .uint64_literal  = uint64_literal,
        .value_type      = NECRO_MACH_VALUE_UINT64_LITERAL,
    }, necro_mach_type_create_uint64(&program->arena));
}

NecroMachAst* necro_mach_value_create_i32(NecroMachProgram* program, int32_t int32_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .int64_literal   = int32_literal,
        .value_type      = NECRO_MACH_VALUE_INT32_LITERAL,
    }, necro_mach_type_create_int32(&program->arena));
}

NecroMachAst* necro_mach_value_create_i64(NecroMachProgram* program, int64_t int64_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .int64_literal   = int64_literal,
        .value_type      = NECRO_MACH_VALUE_INT64_LITERAL,
    }, necro_mach_type_create_int64(&program->arena));
}

NecroMachAst* necro_mach_value_create_f32(NecroMachProgram* program, float f32_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .f32_literal = f32_literal,
        .value_type  = NECRO_MACH_VALUE_F32_LITERAL,
    }, necro_mach_type_create_f32(&program->arena));
}

NecroMachAst* necro_mach_value_create_f64(NecroMachProgram* program, double f64_literal)
{
    return necro_mach_value_create(program, (NecroMachValue)
    {
        .f64_literal = f64_literal,
        .value_type  = NECRO_MACH_VALUE_F64_LITERAL,
    }, necro_mach_type_create_f64(&program->arena));
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
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
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

///////////////////////////////////////////////////////
// Memory
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_build_gep(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(source_value->type == NECRO_MACH_VALUE);
    NecroMachAst** indices = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(NecroMachAst*));
    for (size_t i = 0; i < num_indices; ++i)
        indices[i] = necro_mach_value_create_uint32(program, a_indices[i]);
    // type check gep
    NecroMachType* necro_machine_type = source_value->necro_machine_type;
    for (size_t i = 0; i < num_indices; ++i)
    {
        if (necro_machine_type->type == NECRO_MACH_TYPE_STRUCT)
        {
            assert(a_indices[i] < (uint32_t) necro_machine_type->struct_type.num_members);
            necro_machine_type = necro_machine_type->struct_type.members[a_indices[i]];
        }
        else if (necro_machine_type->type == NECRO_MACH_TYPE_PTR)
        {
            assert(i == 0); // NOTE: Can only deref the first type!
            necro_machine_type = necro_machine_type->ptr_type.element_type;
        }
        else
        {
            assert(necro_machine_type->type == NECRO_MACH_TYPE_STRUCT || necro_machine_type->type == NECRO_MACH_TYPE_PTR);
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
    // TODO: Put back once refactored
    // necro_type_check(program, dest->necro_machine_type, source->necro_machine_type);
    assert(necro_mach_type_is_word_uint(program, num_bytes->necro_machine_type));
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
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_STORE;
    ast->store.store_type   = NECRO_MACH_STORE_PTR;
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
    ast->load.load_type     = NECRO_MACH_LOAD_PTR;
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

NecroMachAst* necro_mach_build_cmp(NecroMachProgram* program, NecroMachAst* fn_def, NECRO_MACH_CMP_TYPE cmp_type, NecroMachAst* left, NecroMachAst* right)
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
    // @curtis: these aren't the same type
    /* assert(left->type == NECRO_MACH_TYPE_UINT1  || */
    /*        left->type == NECRO_MACH_TYPE_UINT8  || */
    /*        left->type == NECRO_MACH_TYPE_UINT16 || */
    /*        left->type == NECRO_MACH_TYPE_UINT32 || */
    /*        left->type == NECRO_MACH_TYPE_UINT64 || */
    /*        left->type == NECRO_MACH_TYPE_INT32  || */
    /*        left->type == NECRO_MACH_TYPE_INT64  || */
    /*        left->type == NECRO_MACH_TYPE_F32    || */
    /*        left->type == NECRO_MACH_TYPE_F64    || */
    /*        left->type == NECRO_MACH_TYPE_PTR); */
    assert(left->type == right->type);
    NecroMachAst* cmp = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    cmp->type         = NECRO_MACH_CMP;
    cmp->cmp.cmp_type = cmp_type;
    cmp->cmp.left     = left;
    cmp->cmp.right    = right;
    cmp->cmp.result   = necro_mach_value_create_reg(program, necro_mach_type_create_uint1(&program->arena), "cmp");
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
    // value = necro_mach_build_maybe_cast(program, fn_def, value, phi->phi.result->necro_machine_type);
    phi->phi.values = necro_cons_mach_phi_list(&program->arena, (NecroMachPhiData) { .block = block, .value = value }, phi->phi.values);
}

struct NecroMachSwitchTerminator* necro_mach_build_switch(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* choice_val, NecroMachSwitchList* values, NecroMachAst* else_block)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(choice_val != NULL);
    assert(choice_val->necro_machine_type->type == NECRO_MACH_TYPE_UINT32);
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

    // type_check
    NecroMachType* fn_value_type = fn_value_ast->necro_machine_type;
    // if (fn_value_type->type == NECRO_MACH_TYPE_PTR && fn_value_type->ptr_type.element_type->type == NECRO_MACH_TYPE_FN)
    //     fn_value_type = fn_value_type->ptr_type.element_type;
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

NecroMachAst* necro_mach_build_binop(NecroMachProgram* program, NecroMachAst* fn_def, NecroMachAst* left, NecroMachAst* right, NECRO_MACH_BINOP_TYPE op_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);

    NecroMachAst* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type         = NECRO_MACH_BINOP;
    assert(left->type == NECRO_MACH_VALUE);
    assert(right->type == NECRO_MACH_VALUE);
    necro_mach_type_check(program, left->necro_machine_type, right->necro_machine_type);
    // typecheck
    switch (op_type)
    {
    case NECRO_MACH_BINOP_IADD: /* FALL THROUGH */
    case NECRO_MACH_BINOP_ISUB: /* FALL THROUGH */
    case NECRO_MACH_BINOP_IMUL: /* FALL THROUGH */
    case NECRO_MACH_BINOP_IDIV: /* FALL THROUGH */
    case NECRO_MACH_BINOP_OR:
    case NECRO_MACH_BINOP_AND:
    case NECRO_MACH_BINOP_SHL:
    case NECRO_MACH_BINOP_SHR:
    {
        // Type check that it's an int type
        necro_mach_type_check(program, left->necro_machine_type, program->necro_int_type);
        necro_mach_type_check(program, right->necro_machine_type, program->necro_int_type);
        ast->necro_machine_type = program->necro_int_type;
        // ast->necro_machine_type = left->necro_machine_type;
        ast->binop.result       = necro_mach_value_create_reg(program, ast->necro_machine_type, "iop");
        break;
    }
    case NECRO_MACH_BINOP_FADD: /* FALL THROUGH */
    case NECRO_MACH_BINOP_FSUB: /* FALL THROUGH */
    case NECRO_MACH_BINOP_FMUL: /* FALL THROUGH */
    case NECRO_MACH_BINOP_FDIV: /* FALL THROUGH */
    {
        // Type check that it's a float type
        necro_mach_type_check(program, left->necro_machine_type, program->necro_float_type);
        necro_mach_type_check(program, right->necro_machine_type, program->necro_float_type);
        ast->necro_machine_type = program->necro_float_type;
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

///////////////////////////////////////////////////////
// Defs
///////////////////////////////////////////////////////
NecroMachAst* necro_mach_create_struct_def(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachType** members, size_t num_members)
{
    NecroMachAst* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type               = NECRO_MACH_STRUCT_DEF;
    ast->struct_def.symbol  = symbol;
    ast->necro_machine_type = necro_mach_type_create_struct(&program->arena, symbol, members, num_members);
    symbol->ast             = ast;
    necro_mach_program_add_struct(program, ast);
    return ast;
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
    ast->fn_def.state_type              = NECRO_STATE_CONSTANT;
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
    NecroMachAst* ast                   = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachAst));
    ast->type                           = NECRO_MACH_FN_DEF;
    ast->fn_def.symbol                  = symbol;
    ast->fn_def.fn_type                 = NECRO_MACH_FN_RUNTIME;
    ast->fn_def.call_body               = NULL;
    ast->fn_def.fn_value                = necro_mach_value_create_global(program, symbol, necro_machine_type);
    ast->necro_machine_type             = necro_machine_type;
    ast->fn_def.runtime_fn_addr         = runtime_fn_addr;
    ast->fn_def.state_type              = state_type;
    ast->fn_def._curr_block             = NULL;;
    ast->fn_def._init_block             = NULL;
    ast->fn_def._cont_block             = NULL;
    ast->fn_def._err_block              = NULL;
    symbol->ast                         = ast;
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
    ast->machine_def.is_persistent_slot_set        = false;
    ast->machine_def.outer                         = outer;
    ast->necro_machine_type                        = NULL;
    if (value_type->type == NECRO_MACH_TYPE_FN)
    {
        ast->machine_def.fn_type = value_type;
        // TODO: Why unbox result pointer types?
        if (value_type->fn_type.return_type->type == NECRO_MACH_TYPE_PTR)
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
    symbol->ast                        = ast;
    ast->machine_def.machine_name->ast = ast;
    if (outer == 0)
        necro_mach_program_add_machine_def(program, ast);
    necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
    return ast;
}


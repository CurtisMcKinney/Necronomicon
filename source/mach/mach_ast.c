/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_ast.h"
#include "mach_type.h"

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
    return symbol;
}

NecroMachAstSymbol* necro_mach_ast_symbol_gen(NecroMachProgram* program, NecroMachAst* ast, const char* str, NECRO_MANGLE_TYPE mangle_type)
{
    NecroSymbol name = NULL;
    if (mangle_type == NECRO_MANGLE_NAME)
    {
        name = necro_intern_unique_string(program->intern, str, &program->clash_suffix);
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
void necro_mach_add_statement_to_block(NecroMachProgram* program, NecroMachAst* block, NecroMachAst* statement);


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

//--------------------
// Blocks
//--------------------
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

//--------------------
// Memory
//--------------------
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

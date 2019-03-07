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
NecroMachAstSymbol* necro_mach_create_ast_symbol(NecroPagedArena* arena, NecroSymbol name)
{
    NecroMachAstSymbol* symbol = necro_paged_arena_alloc(arena, sizeof(NecroMachAstSymbol));
    symbol->name               = name;
    symbol->ast                = NULL;
    symbol->mach_type          = NULL;
    symbol->necro_type         = NULL;
    return symbol;
}

NecroMachAstSymbol* necro_mach_gen_ast_symbol(NecroMachProgram* program, NecroMachAst* ast, const char* str, NECRO_MANGLE_TYPE mangle_type)
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

    NecroMachAstSymbol* symbol = necro_mach_create_ast_symbol(&program->arena, name);
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
NecroMachAst* necro_mach_create_machine_value_ast(NecroMachProgram* program, NecroMachValue value, NecroMachType* necro_machine_type)
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

NecroMachAst* necro_mach_create_global_value(NecroMachProgram* program, NecroMachAstSymbol* symbol, NecroMachType* necro_machine_type)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .global_symbol = symbol,
        .value_type    = NECRO_MACH_VALUE_GLOBAL,
    }, necro_machine_type);
}

NecroMachAst* necro_mach_create_param_reg(NecroMachProgram* program, NecroMachAst* fn_def, size_t param_num)
{
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACH_FN_DEF);
    assert(fn_def->necro_machine_type->type == NECRO_MACH_TYPE_FN);
    assert(fn_def->necro_machine_type->fn_type.num_parameters > param_num);
    // necro_mach_type_check(program, fn_ast->necro_machine_type->fn_type.parameters[param_num], necro_machine_type);
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .param_reg       = { .fn_symbol = fn_def->fn_def.symbol, .param_num = param_num },
        .value_type      = NECRO_MACH_VALUE_PARAM,
    }, fn_def->necro_machine_type->fn_type.parameters[param_num]);
}


NecroMachAst* necro_mach_create_uint1_value(NecroMachProgram* program, bool uint1_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .uint1_literal = uint1_literal,
        .value_type    = NECRO_MACH_VALUE_UINT1_LITERAL,
    }, necro_mach_type_create_uint1(&program->arena));
}

NecroMachAst* necro_mach_create_uint8_value(NecroMachProgram* program, uint8_t uint8_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .uint8_literal = uint8_literal,
        .value_type    = NECRO_MACH_VALUE_UINT8_LITERAL,
    }, necro_mach_type_create_uint8(&program->arena));
}

NecroMachAst* necro_mach_create_uint16_value(NecroMachProgram* program, uint16_t uint16_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .uint16_literal  = uint16_literal,
        .value_type      = NECRO_MACH_VALUE_UINT16_LITERAL,
    }, necro_mach_type_create_uint16(&program->arena));
}

NecroMachAst* necro_mach_create_uint32_value(NecroMachProgram* program, uint32_t uint32_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .uint32_literal  = uint32_literal,
        .value_type      = NECRO_MACH_VALUE_UINT32_LITERAL,
    }, necro_mach_type_create_uint32(&program->arena));
}

NecroMachAst* necro_mach_create_uint64_value(NecroMachProgram* program, uint64_t uint64_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .uint64_literal  = uint64_literal,
        .value_type      = NECRO_MACH_VALUE_UINT64_LITERAL,
    }, necro_mach_type_create_uint64(&program->arena));
}

NecroMachAst* necro_mach_create_i32_value(NecroMachProgram* program, int32_t int32_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .int64_literal   = int32_literal,
        .value_type      = NECRO_MACH_VALUE_INT32_LITERAL,
    }, necro_mach_type_create_int32(&program->arena));
}

NecroMachAst* necro_mach_create_i64_value(NecroMachProgram* program, int64_t int64_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .int64_literal   = int64_literal,
        .value_type      = NECRO_MACH_VALUE_INT64_LITERAL,
    }, necro_mach_type_create_int64(&program->arena));
}

NecroMachAst* necro_mach_create_f32_value(NecroMachProgram* program, float f32_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .f32_literal = f32_literal,
        .value_type  = NECRO_MACH_VALUE_F32_LITERAL,
    }, necro_mach_type_create_f32(&program->arena));
}

NecroMachAst* necro_mach_create_f64_value(NecroMachProgram* program, double f64_literal)
{
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .f64_literal = f64_literal,
        .value_type  = NECRO_MACH_VALUE_F64_LITERAL,
    }, necro_mach_type_create_f64(&program->arena));
}

NecroMachAst* necro_mach_create_null_value(NecroMachProgram* program, NecroMachType* ptr_type)
{
    assert(ptr_type->type == NECRO_MACH_TYPE_PTR);
    return necro_mach_create_machine_value_ast(program, (NecroMachValue)
    {
        .value_type = NECRO_MACH_VALUE_NULL_PTR_LITERAL,
    }, ptr_type);
}

NecroMachAst* necro_mach_create_word_uint_value(NecroMachProgram* program, uint64_t int_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_mach_create_uint32_value(program, (uint32_t) int_literal);
    else
        return necro_mach_create_uint64_value(program, int_literal);
}

NecroMachAst* necro_mach_create_word_int_value(NecroMachProgram* program, int64_t int_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_mach_create_i32_value(program, (int32_t) int_literal);
    else
        return necro_mach_create_i64_value(program, int_literal);
}

NecroMachAst* necro_mach_create_word_float_value(NecroMachProgram* program, double float_literal)
{
    if (program->word_size == NECRO_WORD_4_BYTES)
        return necro_mach_create_f32_value(program, (float) float_literal);
    else
        return necro_mach_create_f64_value(program, float_literal);
}


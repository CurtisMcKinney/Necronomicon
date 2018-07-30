/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_print.h"
#include "machine.h"
#include "machine_copy.h"

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_machine_print_fn(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_FN_DEF);
    print_white_space(depth);
    if (ast->fn_def.fn_type == NECRO_FN_RUNTIME)
        printf("foreign ");
    printf("fn %s(", necro_intern_get_string(program->intern, ast->fn_def.name.symbol));
    for (size_t i = 0; i < ast->necro_machine_type->fn_type.num_parameters; ++i)
    {
        necro_machine_print_machine_type_go(program->intern, ast->necro_machine_type->fn_type.parameters[i], false);
        if (i < ast->necro_machine_type->fn_type.num_parameters - 1)
            printf(", ");
    }
    printf(") -> ");
    necro_machine_print_machine_type_go(program->intern, ast->necro_machine_type->fn_type.return_type, false);
    if (ast->fn_def.fn_type == NECRO_FN_RUNTIME)
    {
        printf("\n");
        return;
    }
    printf("\n");
    print_white_space(depth);
    printf("{\n");
    necro_machine_print_ast_go(program, ast->fn_def.call_body, depth + 4);
    print_white_space(depth);
    printf("}\n");
}

typedef enum
{
     NECRO_PRINT_VALUE_TYPE,
     NECRO_DONT_PRINT_VALUE_TYPE
} NECRO_SHOULD_PRINT_VALUE_TYPE;

void necro_print_machine_value(NecroMachineProgram* program, NecroMachineAST* ast, NECRO_SHOULD_PRINT_VALUE_TYPE should_print_value_type)
{
    NecroMachineValue value = ast->value;
    switch (value.value_type)
    {
    case NECRO_MACHINE_VALUE_VOID:
        break;
    case NECRO_MACHINE_VALUE_GLOBAL:
        printf("@%s", necro_intern_get_string(program->intern, value.global_name.symbol));
        break;
    case NECRO_MACHINE_VALUE_REG:
        printf("%%%s", necro_intern_get_string(program->intern, value.reg_name.symbol));
        break;
    case NECRO_MACHINE_VALUE_PARAM:
        printf("%%%d", value.param_reg.param_num);
        break;
    case NECRO_MACHINE_VALUE_UINT1_LITERAL:
        printf("%du1", value.uint1_literal);
        return;
    case NECRO_MACHINE_VALUE_UINT8_LITERAL:
        printf("%du8", value.uint8_literal);
        return;
    case NECRO_MACHINE_VALUE_UINT16_LITERAL:
        printf("%du16", value.uint16_literal);
        return;
    case NECRO_MACHINE_VALUE_UINT32_LITERAL:
        printf("%du32", value.uint32_literal);
        return;
    case NECRO_MACHINE_VALUE_UINT64_LITERAL:
        printf("%lldu64", value.uint64_literal);
        return;
    case NECRO_MACHINE_VALUE_INT32_LITERAL:
        printf("%di32", value.int32_literal);
        return;
    case NECRO_MACHINE_VALUE_INT64_LITERAL:
        printf("%lldi64", value.int64_literal);
        return;
    case NECRO_MACHINE_VALUE_F32_LITERAL:
        printf("%ff32", value.f32_literal);
        return;
    case NECRO_MACHINE_VALUE_F64_LITERAL:
        printf("%ff64", value.f64_literal);
        return;
    case NECRO_MACHINE_VALUE_NULL_PTR_LITERAL:
        printf("null");
        break;
    default:
        assert(false);
        break;
    }
    if (should_print_value_type == NECRO_PRINT_VALUE_TYPE)
    // Turning this off for now to clear some clutter
    // if (false)
    {
        printf(" (");
        necro_machine_print_machine_type_go(program->intern, ast->necro_machine_type, false);
        printf(")");
    }
}

void necro_machine_print_block(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_BLOCK);
    print_white_space(depth - 4);
    printf("%s:\n", necro_intern_get_string(program->intern, ast->block.name.symbol));
    for (size_t i = 0; i < ast->block.num_statements; ++i)
    {
        necro_machine_print_ast_go(program, ast->block.statements[i], depth);
        printf("\n");
    }
    assert(ast->block.terminator != NULL);
    switch (ast->block.terminator->type)
    {
    case NECRO_TERM_RETURN:
        print_white_space(depth);
        printf("return ");
        necro_print_machine_value(program, ast->block.terminator->return_terminator.return_value, NECRO_PRINT_VALUE_TYPE);
        printf("\n");
        break;
    case NECRO_TERM_SWITCH:
    {
        print_white_space(depth);
        printf("switch ");
        necro_print_machine_value(program, ast->block.terminator->switch_terminator.choice_val, NECRO_DONT_PRINT_VALUE_TYPE);
        printf(" [");
        NecroMachineSwitchList* values = ast->block.terminator->switch_terminator.values;
        assert(values != NULL);
        while (values != NULL)
        {
            printf("%d: %s, ", values->data.value, necro_intern_get_string(program->intern, values->data.block->block.name.symbol));
            values = values->next;
        }
        printf("else: %s]\n", necro_intern_get_string(program->intern, ast->block.terminator->switch_terminator.else_block->block.name.symbol));
        break;
    }
    case NECRO_TERM_BREAK:
        print_white_space(depth);
        printf("break %s\n", necro_intern_get_string(program->intern, ast->block.terminator->break_terminator.block_to_jump_to->block.name.symbol));
        break;
    case NECRO_TERM_COND_BREAK:
        print_white_space(depth);
        printf("condbreak %%%s [true: %s, false: %s]\n",
            necro_intern_get_string(program->intern, ast->block.terminator->cond_break_terminator.cond_value->value.reg_name.symbol),
            necro_intern_get_string(program->intern, ast->block.terminator->cond_break_terminator.true_block->block.name.symbol),
            necro_intern_get_string(program->intern, ast->block.terminator->cond_break_terminator.false_block->block.name.symbol)
            );
        break;
    case NECRO_TERM_UNREACHABLE:
        print_white_space(depth);
        printf("unreachable\n");
        break;
    }
    if (ast->block.next_block != NULL)
    {
        printf("\n");
        necro_machine_print_block(program, ast->block.next_block, depth);
    }
}

void necro_machine_print_call(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_CALL);
    if (ast->call.result_reg->value.value_type == NECRO_MACHINE_VALUE_VOID)
        printf("call ");
    else
        printf("%%%s = call ", necro_intern_get_string(program->intern, ast->call.result_reg->value.reg_name.symbol));
    necro_print_machine_value(program, ast->call.fn_value, NECRO_DONT_PRINT_VALUE_TYPE);
    printf("(");
    for (size_t i = 0; i < ast->call.num_parameters; ++i)
    {
        necro_print_machine_value(program, ast->call.parameters[i], NECRO_DONT_PRINT_VALUE_TYPE);
        if (i < ast->call.num_parameters - 1)
            printf(", ");
    }
    printf(")");
}

void necro_machine_print_store(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_STORE);
    printf("store ");
    switch (ast->store.store_type)
    {
    case NECRO_STORE_TAG:
        necro_print_machine_value(program, ast->store.source_value, NECRO_PRINT_VALUE_TYPE);
        printf(" ");
        necro_print_machine_value(program, ast->store.dest_ptr, NECRO_PRINT_VALUE_TYPE);
        printf(" (tag)");
        return;
    case NECRO_STORE_PTR:
        necro_print_machine_value(program, ast->store.source_value, NECRO_PRINT_VALUE_TYPE);
        printf(" ");
        necro_print_machine_value(program, ast->store.dest_ptr, NECRO_PRINT_VALUE_TYPE);
        return;
    case NECRO_STORE_SLOT:
        necro_print_machine_value(program, ast->store.source_value, NECRO_PRINT_VALUE_TYPE);
        printf(" ");
        necro_print_machine_value(program, ast->store.store_slot.dest_ptr, NECRO_PRINT_VALUE_TYPE);
        printf(" (slot %d)", ast->store.store_slot.dest_slot);
        return;
    }
}

void necro_machine_print_load(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_LOAD);
    printf("%%%s = load ", necro_intern_get_string(program->intern, ast->load.dest_value->value.reg_name.symbol));
    switch (ast->load.load_type)
    {
    case NECRO_LOAD_TAG:
        necro_print_machine_value(program, ast->load.source_ptr, NECRO_PRINT_VALUE_TYPE);
        printf(" (tag)");
        return;
    case NECRO_LOAD_PTR:
        necro_print_machine_value(program, ast->load.source_ptr, NECRO_PRINT_VALUE_TYPE);
        return;
    case NECRO_LOAD_SLOT:
        necro_print_machine_value(program, ast->load.load_slot.source_ptr, NECRO_PRINT_VALUE_TYPE);
        printf(" (slot %d)", ast->load.load_slot.source_slot);
        return;
    // case NECRO_LOAD_GLOBAL:
    //     necro_print_machine_value(program, ast->load.source_global, NECRO_PRINT_VALUE_TYPE);
    //     // printf(" ");
    //     // printf(" (global))", ast->load.load_slot.source_slot);
    //     return;
    }
}

void necro_machine_print_bit_cast(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_BIT_CAST);
    printf("%%%s = bitcast ", necro_intern_get_string(program->intern, ast->bit_cast.to_value->value.reg_name.symbol));
    necro_print_machine_value(program, ast->bit_cast.from_value, NECRO_PRINT_VALUE_TYPE);
    printf(" => (");
    necro_machine_print_machine_type_go(program->intern, ast->bit_cast.to_value->necro_machine_type, false);
    printf(")");
}

void necro_machine_print_gep(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_GEP);
    printf("%%%s = gep", necro_intern_get_string(program->intern, ast->gep.dest_value->value.reg_name.symbol));
    for (size_t i = 0; i < ast->gep.num_indices; ++i)
    {
        printf(" %du32", ast->gep.indices[i]);
        if (i < ast->gep.num_indices - 1)
            printf(",");
    }
    printf(", ");
    necro_print_machine_value(program, ast->gep.source_value, depth);
}

void necro_machine_print_nalloc(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_NALLOC);
    if (ast->nalloc.is_constant)
        printf("%%%s = alloc_const (", necro_intern_get_string(program->intern, ast->nalloc.result_reg->value.reg_name.symbol));
    else
        printf("%%%s = alloc_from (", necro_intern_get_string(program->intern, ast->nalloc.result_reg->value.reg_name.symbol));
    necro_machine_print_machine_type_go(program->intern, ast->nalloc.type_to_alloc, false);
    printf(") %du16", ast->nalloc.slots_used);
}

void necro_machine_print_memcpy(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_MEMCPY);
    print_white_space(depth);
    printf("memcpy ");
    necro_print_machine_value(program, ast->memcpy.dest, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_print_machine_value(program, ast->memcpy.source, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_print_machine_value(program, ast->memcpy.num_bytes, NECRO_DONT_PRINT_VALUE_TYPE);
}

void necro_machine_print_state_type(NecroMachineProgram* program, NECRO_STATE_TYPE state_type)
{
    switch (state_type)
    {
    // case NECRO_STATE_STATIC:
    //     printf("static ");
    case NECRO_STATE_CONSTANT:
        printf("constant ");
        break;
    case NECRO_STATE_POINTWISE:
        printf("pointwise");
        break;
    case NECRO_STATE_STATEFUL:
        printf("stateful");
        break;
    }
}

void necro_machine_print_machine_def(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_DEF);
    print_white_space(depth);
    printf("machine %s\n{\n", necro_intern_get_string(program->intern, ast->machine_def.machine_name.symbol));
    print_white_space(depth + 4);
    necro_machine_print_state_type(program, ast->machine_def.state_type);
    printf("\n");
    print_white_space(depth + 4);
    printf("type: ");
    necro_machine_print_machine_type_go(program->intern, ast->necro_machine_type, true);
    printf("\n");
    if (ast->machine_def.mk_fn != NULL && (ast->machine_def.state_type == NECRO_STATE_STATEFUL || ast->machine_def.state_type == NECRO_STATE_CONSTANT))
    {
        // print_white_space(depth + 4);
        printf("\n");
        necro_machine_print_fn(program, ast->machine_def.mk_fn, depth + 4);
    }
    // if (ast->machine_def.default_init && (ast->machine_def.state_type == NECRO_STATE_STATEFUL || ast->machine_def.state_type == NECRO_STATE_CONSTANT))
    // {
    //     // print_white_space(depth + 4);
    //     // printf("init:\n");
    //     // necro_machine_print_fn(program, ast->machine_def.init_fn, depth + 4);
    // }
    if (ast->machine_def.update_fn != NULL)
    {
        printf("\n");
        necro_machine_print_fn(program, ast->machine_def.update_fn, depth + 4);
    }

    printf("}\n");
    puts("");
}

void necro_machine_print_binop(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_BINOP);
    print_white_space(depth);
    printf("%%%s = ", necro_intern_get_string(program->intern, ast->binop.result->value.reg_name.symbol));
    switch (ast->binop.binop_type)
    {
    case NECRO_MACHINE_BINOP_IADD: printf("iadd "); break;
    case NECRO_MACHINE_BINOP_ISUB: printf("isub "); break;
    case NECRO_MACHINE_BINOP_IMUL: printf("imul "); break;
    case NECRO_MACHINE_BINOP_IDIV: printf("idiv "); break;
    case NECRO_MACHINE_BINOP_FADD: printf("fadd "); break;
    case NECRO_MACHINE_BINOP_FSUB: printf("fsub "); break;
    case NECRO_MACHINE_BINOP_FMUL: printf("fmul "); break;
    case NECRO_MACHINE_BINOP_FDIV: printf("fdiv "); break;
    }
    necro_print_machine_value(program, ast->binop.left, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_print_machine_value(program, ast->binop.right, NECRO_PRINT_VALUE_TYPE);
}

void necro_machine_print_phi(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_PHI);
    print_white_space(depth);
    printf("%%%s = phi [", necro_intern_get_string(program->intern, ast->phi.result->value.reg_name.symbol));
    NecroMachinePhiList* values = ast->phi.values;
    while (values != NULL)
    {
        printf("%s: %%%s", necro_intern_get_string(program->intern, values->data.block->block.name.symbol), necro_intern_get_string(program->intern, values->data.value->value.reg_name.symbol));
        if (values->next == NULL)
            printf("]");
        else
            printf(", ");
        values = values->next;
    }
}

void necro_machine_print_cmp(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    assert(ast->type == NECRO_MACHINE_CMP);
    print_white_space(depth);
    printf("%%%s = ", necro_intern_get_string(program->intern, ast->cmp.result->value.reg_name.symbol));
    switch (ast->cmp.cmp_type)
    {
    case NECRO_MACHINE_CMP_EQ: printf("eq "); break;
    case NECRO_MACHINE_CMP_NE: printf("ne "); break;
    case NECRO_MACHINE_CMP_GT: printf("gt "); break;
    case NECRO_MACHINE_CMP_GE: printf("ge "); break;
    case NECRO_MACHINE_CMP_LT: printf("lt "); break;
    case NECRO_MACHINE_CMP_LE: printf("le "); break;
    }
    necro_print_machine_value(program, ast->cmp.left, NECRO_PRINT_VALUE_TYPE);
    printf(" ");
    necro_print_machine_value(program, ast->cmp.right, NECRO_PRINT_VALUE_TYPE);
}

void necro_machine_print_ast_go(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth)
{
    switch (ast->type)
    {
    case NECRO_MACHINE_VALUE:
        print_white_space(depth);
        necro_print_machine_value(program, ast, NECRO_PRINT_VALUE_TYPE);
        return;
    case NECRO_MACHINE_LOAD:
        print_white_space(depth);
        necro_machine_print_load(program, ast, depth);
        return;
    case NECRO_MACHINE_STORE:
        print_white_space(depth);
        necro_machine_print_store(program, ast, depth);
        return;
    case NECRO_MACHINE_BIT_CAST:
        print_white_space(depth);
        necro_machine_print_bit_cast(program, ast, depth);
        return;
    // case NECRO_MACHINE_CONSTANT_DEF:
    //     return;
    case NECRO_MACHINE_NALLOC:
        print_white_space(depth);
        necro_machine_print_nalloc(program, ast, depth);
        return;
    case NECRO_MACHINE_CALL:
        print_white_space(depth);
        necro_machine_print_call(program, ast, depth);
        return;
    case NECRO_MACHINE_STRUCT_DEF:
        print_white_space(depth);
        printf("struct ");
        necro_machine_print_machine_type(program->intern, ast->necro_machine_type);
        return;
    case NECRO_MACHINE_BLOCK:
        necro_machine_print_block(program, ast, depth);
        return;
    case NECRO_MACHINE_FN_DEF:
        print_white_space(depth);
        necro_machine_print_fn(program, ast, depth);
        return;
    case NECRO_MACHINE_DEF:
        necro_machine_print_machine_def(program, ast, depth);
        return;
    case NECRO_MACHINE_GEP:
        print_white_space(depth);
        necro_machine_print_gep(program, ast, depth);
        return;
    case NECRO_MACHINE_BINOP:
        necro_machine_print_binop(program, ast, depth);
        return;
    case NECRO_MACHINE_PHI:
        necro_machine_print_phi(program, ast, depth);
        return;
    case NECRO_MACHINE_CMP:
        necro_machine_print_cmp(program, ast, depth);
        return;
    case NECRO_MACHINE_MEMCPY:
        necro_machine_print_memcpy(program, ast, depth);
        return;
    }
}

void necro_machine_print_ast(NecroMachineProgram* program, NecroMachineAST* ast)
{
    necro_machine_print_ast_go(program, ast, 0);
}

void necro_print_machine_program(NecroMachineProgram* program)
{
    printf("///////////////////////////////////////////////////////\n");
    printf("// NecroMachineProgram\n");
    printf("///////////////////////////////////////////////////////\n");
    puts("");
    for (size_t i = 0; i < program->structs.length; ++i)
    {
        necro_machine_print_ast_go(program, program->structs.data[i], 0);
        puts("");
    }
    if (program->structs.length > 0)
        puts("");
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_machine_print_ast_go(program, program->globals.data[i], 0);
        puts("");
    }
    if (program->globals.length > 0)
        puts("");
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_machine_print_ast_go(program, program->functions.data[i], 0);
        puts("");
    }
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        necro_machine_print_ast_go(program, program->machine_defs.data[i], 0);
    }
    necro_machine_print_ast_go(program, program->necro_main, 0);
    // necro_print_data_info(program);
}
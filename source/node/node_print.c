/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "node_print.h"
#include "node.h"

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_node_print_fn(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_FN_DEF);
    if (ast->fn_def.fn_type == NECRO_FN_FN)
    {
        print_white_space(depth);
        printf("fn ");
        necro_node_print_node_type_go(program->intern, ast->necro_node_type->fn_type.return_type, false);
        printf(" %s(", necro_intern_get_string(program->intern, ast->fn_def.name.symbol));
        for (size_t i = 0; i < ast->necro_node_type->fn_type.num_parameters; ++i)
        {
            necro_node_print_node_type_go(program->intern, ast->necro_node_type->fn_type.parameters[i], false);
            if (i < ast->necro_node_type->fn_type.num_parameters - 1)
                printf(", ");
        }
        printf(")\n");
        print_white_space(depth);
        printf("{\n");
        necro_node_print_ast_go(program, ast->fn_def.call_body, depth + 4);
        print_white_space(depth);
        printf("}\n");
    }
}

void necro_print_node_value(NecroNodeProgram* program, NecroNodeAST* ast)
{
    NecroNodeValue value = ast->value;
    switch (value.value_type)
    {
    case NECRO_NODE_VALUE_GLOBAL:
        printf("%%%s", necro_intern_get_string(program->intern, value.global_name.symbol));
        break;
    case NECRO_NODE_VALUE_REG:
        printf("%%%s", necro_intern_get_string(program->intern, value.reg_name.symbol));
        break;
    case NECRO_NODE_VALUE_PARAM:
        printf("%%%d", value.param_reg.param_num);
        break;
    case NECRO_NODE_VALUE_UINT32_LITERAL:
        printf("%du32", value.uint_literal);
        return;
    case NECRO_NODE_VALUE_UINT16_LITERAL:
        printf("%du16", value.uint_literal);
        return;
    case NECRO_NODE_VALUE_INT_LITERAL:
        printf("%lldi64", value.int_literal);
        return;
    case NECRO_NODE_VALUE_NULL_PTR_LITERAL:
        printf("null");
        break;
    default:
        assert(false);
        break;
    }
    printf(" (");
    necro_node_print_node_type_go(program->intern, value.necro_node_type, false);
    printf(")");
}

void necro_node_print_block(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_BLOCK);
    print_white_space(depth - 4);
    printf("%s:\n", necro_intern_get_string(program->intern, ast->block.name));
    for (size_t i = 0; i < ast->block.num_statements; ++i)
    {
        necro_node_print_ast_go(program, ast->block.statements[i], depth);
        printf("\n");
    }
    assert(ast->block.terminator != NULL);
    switch (ast->block.terminator->type)
    {
    case NECRO_TERM_RETURN:
        print_white_space(depth);
        printf("return ");
        necro_print_node_value(program, ast->block.terminator->return_terminator.return_value);
        printf("\n");
        return;
    case NECRO_TERM_SWITCH:
        return;
    case NECRO_TERM_BREAK:
        return;
    case NECRO_TERM_COND_BREAK:
        return;
    case NECRO_TERM_UNREACHABLE:
        return;
    }
}

void necro_node_print_call(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_CALL);
    // printf("%%%s = %s(", necro_intern_get_string(program->intern, ast->call.result_reg.reg_name.symbol), necro_intern_get_string(program->intern, ast->call.name.symbol));
    printf("%%%s = call ", necro_intern_get_string(program->intern, ast->call.result_reg->value.reg_name.symbol));
    necro_print_node_value(program, ast->call.fn_value);
    printf("(");
    for (size_t i = 0; i < ast->call.num_parameters; ++i)
    {
        necro_print_node_value(program, ast->call.parameters[i]);
        if (i < ast->call.num_parameters - 1)
            printf(", ");
    }
    printf(")");
}

void necro_node_print_store(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_STORE);
    switch (ast->store.store_type)
    {
    case NECRO_STORE_TAG:
        printf("store ");
        necro_print_node_value(program, ast->store.source_value);
        printf(" ");
        necro_print_node_value(program, ast->store.dest_ptr);
        printf(" (tag)");
        return;
    case NECRO_STORE_PTR:
        printf("store ");
        necro_print_node_value(program, ast->store.source_value);
        printf(" ");
        necro_print_node_value(program, ast->store.dest_ptr);
        return;
    case NECRO_STORE_SLOT:
        printf("store ");
        necro_print_node_value(program, ast->store.source_value);
        printf(" ");
        necro_print_node_value(program, ast->store.store_slot.dest_ptr);
        printf(" (slot %d)", ast->store.store_slot.dest_slot.slot_num);
        return;
    case NECRO_STORE_GLOBAL:
        printf("store");
        necro_print_node_value(program, ast->store.source_value);
        printf("%s", necro_intern_get_string(program->intern, ast->store.dest_global->value.global_name.symbol));
        printf(" (global)");
        return;
    }
}

void necro_node_print_bit_cast(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_BIT_CAST);
    printf("%%%s = bit_cast ", necro_intern_get_string(program->intern, ast->bit_cast.to_value->value.reg_name.symbol));
    necro_print_node_value(program, ast->bit_cast.from_value);
    printf(" => (");
    necro_node_print_node_type_go(program->intern, ast->bit_cast.to_value->necro_node_type, false);
    printf(")");
}

void necro_node_print_nalloc(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_NALLOC);
    printf("%%%s = nalloc (", necro_intern_get_string(program->intern, ast->nalloc.result_reg->value.reg_name.symbol));
    necro_node_print_node_type_go(program->intern, ast->nalloc.type_to_alloc, false);
    printf(") %du16", ast->nalloc.slots_used);
}

void necro_node_print_state_type(NecroNodeProgram* program, NECRO_STATE_TYPE state_type)
{
    switch (state_type)
    {
    case NECRO_STATE_STATIC:
        printf("static ");
    case NECRO_STATE_CONSTANT:
        printf("constant ");
        break;
    case NECRO_STATE_STATELESS:
        printf("statless");
        break;
    case NECRO_STATE_STATEFUL:
        printf("stateful");
        break;
    }
}

void necro_node_print_node_def(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_DEF);
    print_white_space(depth);
    printf("node %s\n{\n", necro_intern_get_string(program->intern, ast->node_def.node_name.symbol));
    print_white_space(depth + 4);
    necro_node_print_state_type(program, ast->node_def.state_type);
    printf("\n");
    // print_white_space(depth + 4);
    // printf("value: ");
    // necro_node_print_node_type_go(program->intern, ast->node_def.value_type, false);
    // printf("\n");
    // if (ast->node_def.num_members > 0)
    // {
    //     print_white_space(depth + 4);
    //     printf("members:\n");
    //     for (size_t i = 0; i < ast->node_def.num_members; ++i)
    //     {
    //         print_white_space(depth + 8);
    //         necro_node_print_node_type_go(program->intern, ast->node_def.members[i].necro_node_type, false);
    //         printf("\n");
    //     }
    // }
    print_white_space(depth + 4);
    printf("type: ");
    necro_node_print_node_type_go(program->intern, ast->necro_node_type, true);
    printf("\n");
    if (ast->node_def.default_mk && ast->node_def.state_type == NECRO_STATE_STATEFUL)
    {
        print_white_space(depth + 4);
        printf("mk:   default\n");
    }
    if (ast->node_def.default_init && ast->node_def.state_type == NECRO_STATE_STATEFUL)
    {
        print_white_space(depth + 4);
        printf("init: default\n");
    }
    if (ast->node_def.update_fn != NULL)
    {
        printf("\n");
        necro_node_print_fn(program, ast->node_def.update_fn, depth + 4);
    }

    printf("}\n");
    puts("");
}

void necro_node_print_ast_go(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    switch (ast->type)
    {
    case NECRO_NODE_LIT:
        return;
    case NECRO_NODE_LOAD:
        return;
    case NECRO_NODE_STORE:
        print_white_space(depth);
        necro_node_print_store(program, ast, depth);
        return;
    case NECRO_NODE_BIT_CAST:
        print_white_space(depth);
        necro_node_print_bit_cast(program, ast, depth);
        return;
    case NECRO_NODE_CONSTANT_DEF:
        return;
    case NECRO_NODE_NALLOC:
        print_white_space(depth);
        necro_node_print_nalloc(program, ast, depth);
        return;
    case NECRO_NODE_CALL:
        print_white_space(depth);
        necro_node_print_call(program, ast, depth);
        return;
    case NECRO_NODE_STRUCT_DEF:
        print_white_space(depth);
        printf("struct ");
        necro_node_print_node_type(program->intern, ast->necro_node_type);
        return;
    case NECRO_NODE_BLOCK:
        necro_node_print_block(program, ast, depth);
        return;
    case NECRO_NODE_FN_DEF:
        print_white_space(depth);
        necro_node_print_fn(program, ast, depth);
        return;
    case NECRO_NODE_DEF:
        necro_node_print_node_def(program, ast, depth);
        return;
    }
}

void necro_node_print_ast(NecroNodeProgram* program, NecroNodeAST* ast)
{
    necro_node_print_ast_go(program, ast, 0);
}

void necro_print_node_program(NecroNodeProgram* program)
{
    printf("///////////////////////////////////////////////////////\n");
    printf("// NecroNodeProgram\n");
    printf("///////////////////////////////////////////////////////\n");
    puts("");

    for (size_t i = 0; i < program->structs.length; ++i)
    {
        necro_node_print_ast_go(program, program->structs.data[i], 0);
        puts("");
    }
    if (program->structs.length > 0)
        puts("");
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_node_print_ast_go(program, program->functions.data[i], 0);
        puts("");
    }
    for (size_t i = 0; i < program->node_defs.length; ++i)
    {
        necro_node_print_ast_go(program, program->node_defs.data[i], 0);
    }
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_node_print_ast_go(program, program->globals.data[i], 0);
        puts("");
    }
}


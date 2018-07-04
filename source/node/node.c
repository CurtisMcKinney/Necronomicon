/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "node.h"

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
NecroNodeAST* necro_core_to_node_1_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast);
void          necro_program_add_global(NecroNodeProgram* program, NecroNodeAST* global);
void          necro_program_add_struct(NecroNodeProgram* program, NecroNodeAST* struct_ast);
void          necro_program_add_function(NecroNodeProgram* program, NecroNodeAST* function);
void          necro_node_print_ast_go(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth);

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_NAME_UNIQUE,
    NECRO_NAME_NOT_UNIQUE
} NECRO_NAME_UNIQUENESS;

NecroVar necro_gen_var(NecroNodeProgram* program, NecroNodeAST* necro_node_ast, const char* var_header, NECRO_NAME_UNIQUENESS uniqueness)
{
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
    const char* var_name = NULL;
    if (uniqueness == NECRO_NAME_NOT_UNIQUE)
    {
        char buf[10];
        itoa(program->gen_vars++, buf, 10);
        var_name = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { var_header, "#", buf });
    }
    else
    {
        var_name = var_header;
    }
    NecroSymbol var_sym  = necro_intern_string(program->intern, var_name);
    NecroID     var_id   = necro_symtable_manual_new_symbol(program->symtable, var_sym);
    // necro_symtable_get(program->symtable, var_id)->type           = type;
    necro_symtable_get(program->symtable, var_id)->necro_node_ast = necro_node_ast;
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return (NecroVar) { .id = var_id, .symbol = var_sym };
}

static size_t necro_data_con_count(NecroCoreAST_DataCon* con)
{
    if (con == NULL) return 0;
    size_t count = 0;
    while (con != NULL)
    {
        count++;
        con = con->next;
    }
    return count;
}

size_t necro_count_data_con_args(NecroCoreAST_DataCon* con)
{
    assert(con != NULL);
    size_t count = 0;
    NecroCoreAST_Expression* args = con->arg_list;
    while (args != NULL)
    {
        assert(args->expr_type == NECRO_CORE_EXPR_LIST);
        count++;
        args = args->list.next;
    }
    return count;
}

///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
void necro_node_print_fn(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_FN_DEF);
    if (ast->fn_def.fn_type == NECRO_FN_FN)
    {
        necro_node_print_node_type_go(program->intern, ast->necro_node_type->fn_type.return_type, false);
        printf(" %s(", necro_intern_get_string(program->intern, ast->fn_def.name.symbol));
        for (size_t i = 0; i < ast->necro_node_type->fn_type.num_parameters; ++i)
        {
            necro_node_print_node_type_go(program->intern, ast->necro_node_type->fn_type.parameters[i], false);
            if (i < ast->necro_node_type->fn_type.num_parameters - 1)
                printf(", ");
        }
        printf(")\n");
        printf("{\n");
        necro_node_print_ast_go(program, ast->fn_def.call_body, depth + 4);
        printf("}\n");
    }
}

void necro_print_node_value(NecroNodeProgram* program, NecroNodeValue value)
{
    switch (value.value_type)
    {
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
    printf("%%%s = %s(", necro_intern_get_string(program->intern, ast->call.result_reg.reg_name.symbol), necro_intern_get_string(program->intern, ast->call.name.symbol));
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
        printf("%s", necro_intern_get_string(program->intern, ast->store.dest_global_name.symbol));
        printf(" (global)");
        return;
    }
}

void necro_node_print_bit_cast(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_BIT_CAST);
    printf("%%%s = bit_cast ", necro_intern_get_string(program->intern, ast->bit_cast.to_value.reg_name.symbol));
    necro_print_node_value(program, ast->bit_cast.from_value);
    printf(" => (");
    necro_node_print_node_type_go(program->intern, ast->bit_cast.to_value.necro_node_type, false);
    printf(")");
}

void necro_node_print_nalloc(NecroNodeProgram* program, NecroNodeAST* ast, size_t depth)
{
    assert(ast->type == NECRO_NODE_NALLOC);
    printf("%%%s = nalloc (", necro_intern_get_string(program->intern, ast->nalloc.result_reg.reg_name.symbol));
    necro_node_print_node_type_go(program->intern, ast->nalloc.type_to_alloc, false);
    printf(") %du16", ast->nalloc.slots_used);
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
    case NECRO_NODE_DEF:
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
        necro_node_print_node_type(program->intern, ast->necro_node_type);
        return;
    case NECRO_NODE_BLOCK:
        necro_node_print_block(program, ast, depth);
        return;
    case NECRO_NODE_FN_DEF:
        print_white_space(depth);
        necro_node_print_fn(program, ast, depth);
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
    if (program->structs.length > 0 && program->globals.length > 0 || program->functions.length > 0)
        puts("");
    for (size_t i = 0; i < program->globals.length; ++i)
    {
        necro_node_print_ast_go(program, program->globals.data[i], 0);
        puts("");
    }
    if (program->globals.length > 0 && program->functions.length > 0)
        puts("");
    for (size_t i = 0; i < program->functions.length; ++i)
    {
        necro_node_print_ast_go(program, program->functions.data[i], 0);
        puts("");
    }

    // puts("");
}

///////////////////////////////////////////////////////
// AST construction
///////////////////////////////////////////////////////
NecroNodeAST* necro_create_node_int_lit(NecroNodeProgram* program, int64_t i)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LIT;
    ast->lit.type        = NECRO_NODE_LIT_INT;
    ast->lit.int_literal = i;
    ast->necro_node_type = necro_create_node_int_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_double_lit(NecroNodeProgram* program, double d)
{
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_LIT;
    ast->lit.type           = NECRO_NODE_LIT_DOUBLE;
    ast->lit.double_literal = d;
    ast->necro_node_type    = necro_create_node_double_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_char_lit(NecroNodeProgram* program, char c)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_LIT;
    ast->lit.type         = NECRO_NODE_LIT_CHAR;
    ast->lit.char_literal = c;
    ast->necro_node_type  = necro_create_node_char_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_null_lit(NecroNodeProgram* program, NecroNodeType* node_type)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LIT;
    ast->lit.type        = NECRO_NODE_LIT_NULL;
    ast->necro_node_type = node_type;
    assert(node_type->type == NECRO_NODE_TYPE_PTR);
    return ast;
}

NecroNodeValue necro_create_reg(NecroNodeProgram* program, NecroNodeAST* ast, NecroNodeType* necro_node_type, const char* reg_name_head)
{
    return (NecroNodeValue)
    {
        .reg_name        = necro_gen_var(program, ast, reg_name_head, NECRO_NAME_NOT_UNIQUE),
        .value_type      = NECRO_NODE_VALUE_REG,
        .necro_node_type = necro_node_type
    };
}

NecroNodeValue necro_create_param_reg(NecroNodeProgram* program, NecroVar fn_name, size_t param_num, NecroNodeType* necro_node_type)
{
    NecroNodeAST* fn_ast = necro_symtable_get(program->symtable, fn_name.id)->necro_node_ast;
    assert(fn_ast != NULL);
    assert(fn_ast->necro_node_type->type == NECRO_NODE_TYPE_FN);
    assert(fn_ast->necro_node_type->fn_type.num_parameters > param_num);
    necro_type_check(fn_ast->necro_node_type->fn_type.parameters[param_num], necro_node_type);
    return (NecroNodeValue)
    {
        .param_reg       = { .fn_name = fn_name, .param_num = param_num },
        .value_type      = NECRO_NODE_VALUE_PARAM,
        .necro_node_type = necro_node_type
    };
}

NecroNodeValue necro_create_uint32_necro_node_value(NecroNodeProgram* program, uint32_t uint_literal)
{
    return (NecroNodeValue)
    {
        .uint_literal    = uint_literal,
        .value_type      = NECRO_NODE_VALUE_UINT32_LITERAL,
        .necro_node_type = necro_create_node_uint32_type(&program->arena),
    };
}

NecroNodeValue necro_create_uint16_necro_node_value(NecroNodeProgram* program, uint16_t uint16_literal)
{
    return (NecroNodeValue)
    {
        .uint16_literal  = uint16_literal,
        .value_type      = NECRO_NODE_VALUE_UINT16_LITERAL,
        .necro_node_type = necro_create_node_uint16_type(&program->arena),
    };
}

NecroNodeValue necro_create_int_necro_node_value(NecroNodeProgram* program, int64_t int_literal)
{
    return (NecroNodeValue)
    {
        .int_literal     = int_literal,
        .value_type      = NECRO_NODE_VALUE_INT_LITERAL,
        .necro_node_type = necro_create_node_int_type(&program->arena),
    };
}

NecroNodeValue necro_create_null_necro_node_value(NecroNodeProgram* program, NecroNodeType* ptr_type)
{
    return (NecroNodeValue)
    {
        .value_type      = NECRO_NODE_VALUE_NULL_PTR_LITERAL,
        .necro_node_type = ptr_type,
    };
}

NecroNodeAST* necro_create_node_load_from_ptr(NecroNodeProgram* program, NecroNodeValue source_ptr, const char* dest_name)
{
    assert(source_ptr.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_NODE_VALUE_REG || source_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LOAD;
    ast->load.load_type  = NECRO_LOAD_PTR;
    ast->load.source_ptr = source_ptr;
    ast->load.dest_value = necro_create_reg(program, ast, source_ptr.necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type = source_ptr.necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_tag(NecroNodeProgram* program, NecroNodeValue source_ptr, const char* dest_name)
{
    assert(source_ptr.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_NODE_VALUE_REG || source_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LOAD;
    ast->load.load_type  = NECRO_LOAD_TAG;
    ast->load.source_ptr = source_ptr;
    ast->load.dest_value = necro_create_reg(program, ast, necro_create_node_uint32_type(&program->arena), dest_name);
    ast->necro_node_type = ast->load.dest_value.necro_node_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_from_slot(NecroNodeProgram* program, NecroNodeValue source_ptr, NecroSlot source_slot, const char* dest_name)
{
    assert(source_slot.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    NecroNodeAST* ast               = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                       = NECRO_NODE_LOAD;
    ast->load.load_type             = NECRO_LOAD_SLOT;
    ast->load.load_slot.source_ptr  = source_ptr;
    ast->load.load_slot.source_slot = source_slot;
    ast->load.dest_value            = necro_create_reg(program, ast, source_slot.necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type            = source_slot.necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_from_global(NecroNodeProgram* program, NecroVar source_global_name, NecroNodeType* necro_node_type, const char* dest_name)
{
    assert(necro_node_type->type == NECRO_NODE_TYPE_PTR);
    NecroNodeAST* ast            = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                    = NECRO_NODE_LOAD;
    ast->load.load_type          = NECRO_LOAD_GLOBAL;
    ast->load.source_global_name = source_global_name;
    ast->load.dest_value         = necro_create_reg(program, ast, necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type         = necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_ptr(NecroNodeProgram* program, NecroNodeValue source_value, NecroNodeValue dest_ptr)
{
    assert(dest_ptr.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_NODE_VALUE_REG || dest_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_PTR;
    ast->store.source_value = source_value;
    ast->store.dest_ptr     = dest_ptr;
    ast->necro_node_type    = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_tag(NecroNodeProgram* program, NecroNodeValue source_value, NecroNodeValue dest_ptr)
{
    assert(dest_ptr.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_NODE_VALUE_REG || dest_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    assert(source_value.necro_node_type->type == NECRO_NODE_TYPE_UINT32);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_TAG;
    ast->store.source_value = source_value;
    ast->store.dest_ptr     = dest_ptr;
    ast->necro_node_type    = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_slot(NecroNodeProgram* program, NecroNodeValue source_value, NecroNodeValue dest_ptr, NecroSlot dest_slot)
{
    necro_type_check(source_value.necro_node_type, dest_slot.necro_node_type);
    assert(dest_slot.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr.necro_node_type->ptr_type.element_type->type == NECRO_NODE_TYPE_STRUCT);
    assert(dest_ptr.necro_node_type->ptr_type.element_type->struct_type.num_members > dest_slot.slot_num + 1);
    necro_type_check(dest_ptr.necro_node_type->ptr_type.element_type->struct_type.members[dest_slot.slot_num + 1], dest_slot.necro_node_type);
    NecroNodeAST* ast               = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                       = NECRO_NODE_STORE;
    ast->store.store_type           = NECRO_STORE_SLOT;
    ast->store.source_value         = source_value;
    ast->store.store_slot.dest_ptr  = dest_ptr;
    ast->store.store_slot.dest_slot = dest_slot;
    ast->necro_node_type            = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_global(NecroNodeProgram* program, NecroNodeValue source_value, NecroVar dest_global_name, NecroNodeType* necro_node_type)
{
    assert(necro_node_type->type == NECRO_NODE_TYPE_PTR);
    necro_type_check(source_value.necro_node_type, necro_node_type->ptr_type.element_type);
    NecroNodeAST* ast           = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                   = NECRO_NODE_STORE;
    ast->store.store_type       = NECRO_STORE_GLOBAL;
    ast->store.source_value     = source_value;
    ast->store.dest_global_name = dest_global_name;
    ast->necro_node_type        = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_block(NecroNodeProgram* program, NecroSymbol name, NecroNodeAST* next_block)
{
    NecroNodeAST* ast          = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                  = NECRO_NODE_BLOCK;
    ast->block.name            = name;
    ast->block.statements      = necro_paged_arena_alloc(&program->arena, 4 * sizeof(NecroNodeAST*));
    ast->block.num_statements  = 0;
    ast->block.statements_size = 4;
    ast->block.terminator      = NULL;
    ast->block.next_block      = next_block;
    ast->necro_node_type       = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_call(NecroNodeProgram* program, NecroVar name, NecroNodeValue* a_parameters, size_t num_parameters, NECRO_FN_TYPE fn_type, NecroNodeType* necro_node_type, const char* dest_name)
{
    NecroNodeAST* ast        = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                = NECRO_NODE_CALL;
    ast->call.name           = name;
    NecroNodeValue* parameters     = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroNodeValue));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroNodeValue));
    ast->call.parameters     = parameters;
    ast->call.num_parameters = num_parameters;
    ast->call.fn_type        = fn_type;
    ast->call.result_reg     = necro_create_reg(program, ast, necro_node_type, dest_name);
    ast->necro_node_type     = necro_node_type;
    switch (fn_type)
    {
    case NECRO_FN_FN:      /* FALLTHROUGH */
    case NECRO_FN_RUNTIME:
        necro_type_check(necro_node_type, necro_symtable_get(program->symtable, name.id)->necro_node_ast->necro_node_type);
        break;
    case NECRO_FN_PRIM_OP_ADDI: /* FALLTHROUGH */
    case NECRO_FN_PRIM_OP_SUBI: /* FALLTHROUGH */
    case NECRO_FN_PRIM_OP_MULI: /* FALLTHROUGH */
    case NECRO_FN_PRIM_OP_DIVI:
        // necro_type_check(necro_node_type, necro_symtable_get(program->symtable, name.id)->necro_node_ast->necro_node_type);
        break;
    };
    return ast;
}

NecroNodeAST* necro_create_node_struct_def(NecroNodeProgram* program, NecroVar name, NecroNodeType** members, size_t num_members)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_STRUCT_DEF;
    ast->struct_def.name = name;
    ast->necro_node_type = necro_create_node_struct_type(&program->arena, name, members, num_members);
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    necro_program_add_struct(program, ast);
    return ast;
}

NecroNodeAST* necro_create_node_fn(NecroNodeProgram* program, NecroVar name, NecroNodeAST* call_body, NecroNodeType* necro_node_type)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_FN_DEF;
    ast->fn_def.name      = name;
    ast->fn_def.call_body = call_body;
    ast->fn_def.fn_type   = NECRO_FN_FN;
    ast->necro_node_type  = necro_node_type;
    assert(call_body->type == NECRO_NODE_BLOCK);
    // while (call_body != NULL)
    // {
    //     assert(call_body->block.terminator != NULL);
    //     call_body = call_body->block.next_block;
    // }
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    necro_program_add_function(program, ast);
    return ast;
}

NecroNodeAST* necro_create_node_runtime_fn(NecroNodeProgram* program, NecroVar name, NecroNodeType* necro_node_type)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_FN_DEF;
    ast->fn_def.name      = name;
    ast->fn_def.fn_type   = NECRO_FN_RUNTIME;
    ast->fn_def.call_body = NULL;
    ast->necro_node_type  = necro_node_type;
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    return ast;
}

NecroNodeAST* necro_create_node_initial_node_def(NecroNodeProgram* program, NecroVar bind_name, NecroNodeDef* outer, NecroNodeType* value_type)
{
    NecroArenaSnapshot snapshot          = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroNodeAST* ast                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                            = NECRO_NODE_DEF;
    ast->node_def.bind_name              = bind_name;
    ast->node_def.node_name              = necro_gen_var(program, ast, necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(program->intern, bind_name.symbol), "_Node" }), NECRO_NAME_UNIQUE);
    ast->node_def.arg_names              = NULL;
    ast->node_def.num_arg_names          = 0;
    ast->node_def.mk_fn                  = NULL;
    ast->node_def.ini_fn                 = NULL;
    ast->node_def.update_fn              = NULL;
    ast->node_def.update_error_block     = NULL;
    ast->node_def.initial_tag            = 0;
    ast->node_def.state_type             = NECRO_STATE_STATEFUL;
    ast->node_def.outer                  = outer;
    ast->necro_node_type                 = NULL; // TODO: Node type will be constructed after analysis!
    // ast->necro_node_type                 = necro_create_node_struct_type(&program->arena, ast->node_def.node_name);
    ast->node_def.value_type             = value_type;
    const size_t initial_inner_node_size = 8;
    ast->node_def.inner_nodes            = necro_paged_arena_alloc(&program->arena, initial_inner_node_size * sizeof(NecroSlot));
    ast->node_def.num_inner_nodes        = 0;
    ast->node_def.inner_node_size        = initial_inner_node_size;
    necro_symtable_get(program->symtable, bind_name.id)->necro_node_ast = ast;
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return ast;
}

NecroNodeAST* necro_create_node_gep(NecroNodeProgram* program, NecroNodeValue source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_GEP;
    ast->gep.source_value = source_value;
    int32_t* indices      = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(int32_t));
    memcpy(indices, a_indices, num_indices * sizeof(int32_t));
    ast->gep.indices      = indices;
    ast->gep.num_indices  = num_indices;
    // type check gep
    NecroNodeType* necro_node_type = source_value.necro_node_type;
    for (size_t i = 0; i < num_indices; ++i)
    {
        if (necro_node_type->type == NECRO_NODE_TYPE_STRUCT)
        {
            assert(a_indices[i] < (uint32_t) necro_node_type->struct_type.num_members);
            necro_node_type = necro_node_type->struct_type.members[a_indices[i]];
        }
        else if (necro_node_type->type == NECRO_NODE_TYPE_PTR)
        {
            assert(a_indices[i] == 0); // NOTE: The node abstract machine never directly works with contiguous arrays of data. Thus all pointers should only ever be indexed from 0!
            assert(i == 0);            // NOTE:Can only deref the first type!
            necro_node_type = necro_node_type->ptr_type.element_type;
        }
        else
        {
            assert(necro_node_type->type == NECRO_NODE_TYPE_STRUCT || necro_node_type->type == NECRO_NODE_TYPE_PTR);
        }
    }
    necro_node_type      = necro_create_node_ptr_type(&program->arena, necro_node_type);
    ast->gep.dest_value  = necro_create_reg(program, ast, necro_node_type, dest_name);
    ast->necro_node_type = necro_node_type;
    return ast;
}

NecroNodeAST* necro_create_bit_cast(NecroNodeProgram* program, NecroNodeValue from_value, NecroNodeType* to_type)
{
    NecroNodeAST* ast        = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                = NECRO_NODE_BIT_CAST;
    ast->bit_cast.from_value = from_value;
    switch (from_value.value_type)
    {
    case NECRO_NODE_VALUE_REG:
    case NECRO_NODE_VALUE_PARAM:
    case NECRO_NODE_VALUE_NULL_PTR_LITERAL:
        ast->bit_cast.to_value = necro_create_reg(program, ast, to_type, "tmp");
        break;
    case NECRO_NODE_VALUE_UINT16_LITERAL:
    case NECRO_NODE_VALUE_UINT32_LITERAL:
    case NECRO_NODE_VALUE_INT_LITERAL:
        assert(false && "Cannot bit cast int literal values!");
        break;
    }
    ast->necro_node_type = to_type;
    return ast;
}

NecroNodeAST* necro_create_nalloc(NecroNodeProgram* program, NecroNodeType* type, uint16_t slots_used)
{
    NecroNodeAST* ast         = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                 = NECRO_NODE_NALLOC;
    ast->nalloc.type_to_alloc = type;
    ast->nalloc.slots_used    = slots_used;
    NecroNodeType* type_ptr   = necro_create_node_ptr_type(&program->arena, type);
    ast->nalloc.result_reg    = necro_create_reg(program, ast, type_ptr, "data_ptr");
    ast->necro_node_type      = type_ptr;
    return ast;
}

///////////////////////////////////////////////////////
// Build
///////////////////////////////////////////////////////
void necro_add_statement_to_block(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeAST* statement)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    if (block->block.num_statements + 1 >= block->block.statements_size)
    {
        NecroNodeAST** old_statements = block->block.statements;
        block->block.statements       = necro_paged_arena_alloc(&program->arena, block->block.statements_size * 2 * sizeof(NecroNodeAST*));
        memcpy(block->block.statements, old_statements, block->block.statements_size * sizeof(NecroNodeAST*));
        block->block.statements_size *= 2;
    }
    block->block.statements[block->block.num_statements] = statement;
    block->block.num_statements++;
}

NecroNodeValue necro_build_nalloc(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeType* type, uint16_t a_slots_used)
{
    assert(program != NULL);
    assert(type != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    NecroNodeAST* data_ptr = necro_create_nalloc(program, type, a_slots_used);
    necro_add_statement_to_block(program, block, data_ptr);
    return data_ptr->nalloc.result_reg;
}

void necro_build_store_into_tag(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeValue source_value, NecroNodeValue dest_ptr)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    NecroNodeAST* store_ast = necro_create_node_store_into_tag(program, source_value, dest_ptr);
    necro_add_statement_to_block(program, block, store_ast);
}

void necro_build_store_into_ptr(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeValue source_value, NecroNodeValue dest_ptr)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    necro_add_statement_to_block(program, block, necro_create_node_store_into_ptr(program, source_value, dest_ptr));
}

void necro_build_store_into_slot(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeValue source_value, NecroNodeValue dest_ptr, NecroSlot dest_slot)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    necro_add_statement_to_block(program, block, necro_create_node_store_into_slot(program, source_value, dest_ptr, dest_slot));
}

void necro_build_store_into_global(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeValue source_value, NecroVar dest_global_name, NecroNodeType* necro_node_type)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    necro_add_statement_to_block(program, block, necro_create_node_store_into_global(program, source_value, dest_global_name, necro_node_type));
}

NecroNodeValue necro_build_bit_cast(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeValue value, NecroNodeType* to_type)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    NecroNodeAST* ast = necro_create_bit_cast(program, value, to_type);
    necro_add_statement_to_block(program, block, ast);
    return ast->bit_cast.to_value;
}

void necro_build_return(NecroNodeProgram* program, NecroNodeAST* block, NecroNodeValue return_value)
{
    assert(program != NULL);
    assert(block != NULL);
    assert(block->type == NECRO_NODE_BLOCK);
    block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    block->block.terminator->type                           = NECRO_TERM_RETURN;
    block->block.terminator->return_terminator.return_value = return_value;
}

///////////////////////////////////////////////////////
// NecroNodeProgram
///////////////////////////////////////////////////////
NecroNodeProgram necro_create_initial_node_program(NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    NecroNodeProgram program =
    {
        .arena          = necro_create_paged_arena(),
        .snapshot_arena = necro_create_snapshot_arena(),
        .structs        = necro_create_necro_node_ast_vector(),
        .globals        = necro_create_necro_node_ast_vector(),
        .functions      = necro_create_necro_node_ast_vector(),
        .main           = NULL,
        .intern         = intern,
        .symtable       = symtable,
        .prim_types     = prim_types,
        .gen_vars       = 0,
    };
    NecroNodeAST* necro_data_struct = necro_create_node_struct_def(&program, necro_con_to_var(prim_types->necro_data_type), (NecroNodeType*[]) { necro_create_node_uint32_type(&program.arena), necro_create_node_uint32_type(&program.arena) }, 2);
    program.necro_data_type = necro_data_struct->necro_node_type;
    // NecroNodeAST* necro_val_struct  = necro_create_node_struct_def(&program, necro_con_to_var(prim_types->necro_val_type),  (NecroNodeType*[]) { program.necro_data_type }, 1);
    // program.necro_val_type  = necro_val_struct->necro_node_type;
    program.necro_poly_ptr_type = necro_create_node_poly_ptr_type(&program.arena);
    return program;
}

void necro_destroy_node_program(NecroNodeProgram* program)
{
    necro_destroy_paged_arena(&program->arena);
    necro_destroy_snapshot_arena(&program->snapshot_arena);
    necro_destroy_necro_node_ast_vector(&program->structs);
    necro_destroy_necro_node_ast_vector(&program->globals);
    necro_destroy_necro_node_ast_vector(&program->functions);
}

void necro_program_add_struct(NecroNodeProgram* program, NecroNodeAST* struct_ast)
{
    assert(struct_ast->type == NECRO_NODE_STRUCT_DEF);
    necro_push_necro_node_ast_vector(&program->structs, &struct_ast);
}

void necro_program_add_global(NecroNodeProgram* program, NecroNodeAST* global)
{
    assert(global->type == NECRO_NODE_CONSTANT_DEF);
    necro_push_necro_node_ast_vector(&program->globals, &global);
}

void necro_program_add_function(NecroNodeProgram* program, NecroNodeAST* function)
{
    assert(function->type == NECRO_NODE_FN_DEF);
    necro_push_necro_node_ast_vector(&program->functions, &function);
}

NecroNodeProgram necro_core_to_node(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    NecroNodeProgram program = necro_create_initial_node_program(symtable->intern, symtable, prim_types);

    // Pass 1
    NecroCoreAST_Expression* top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_node_1_go(&program, top_level_list->list.expr);
        // if (necro_is_codegen_error(codegen)) return codegen->error.return_code;
        top_level_list = top_level_list->list.next;
    }

    return program;
}

///////////////////////////////////////////////////////
// Core to Node Pass 1
///////////////////////////////////////////////////////
// NecroFCore necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// {
//     NecroClosureConversion closure_conversion = necro_create_closure_conversion(intern, symtable, prim_types);
//     return closure_conversion.fcore;
// }

// // NecroCoreAST necro_core_final_pass(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// // {
// //     NecroCoreFinalPass final_pass = necro_create_final_pass(intern, symtable, prim_types);
// //     necro_core_final_pass_go(&final_pass, in_ast->root);
// //     return final_pass.core_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_lit(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIT);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_lit(final_pass, in_ast->lit);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_var(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_var(final_pass, in_ast->var.id, in_ast->var.symbol);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_data_con(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* args = necro_core_final_pass_go(final_pass, in_ast->data_con.arg_list);
// //     NecroCoreAST_DataCon*    next = NULL;
// //     if (in_ast->data_con.next != NULL)
// //     {
// //         NecroCoreAST_Expression dummy_ast;
// //         dummy_ast.expr_type = NECRO_CORE_EXPR_DATA_CON;
// //         dummy_ast.data_con  = *in_ast->data_con.next;
// //         next                = &necro_core_final_pass_data_con(final_pass, &dummy_ast)->data_con;
// //         if (necro_is_final_pass_error(final_pass)) return NULL;
// //     }
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_data_con(final_pass, args, in_ast->data_con.condid, next);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_data_decl(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression dummy_ast;
// //     dummy_ast.expr_type = NECRO_CORE_EXPR_DATA_CON;
// //     dummy_ast.data_con  = *in_ast->data_decl.con_list;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_data_decl(final_pass, in_ast->data_decl.data_id, &necro_core_final_pass_go(final_pass, &dummy_ast)->data_con);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_list(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     if (in_ast == NULL) return NULL;
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast_head = necro_create_fcore_ast_list(final_pass, necro_core_final_pass_go(final_pass, in_ast->list.expr), NULL);
// //     NecroCoreAST_Expression* out_ast_curr = out_ast_head;
// //     in_ast                                = in_ast->list.next;
// //     while (in_ast != NULL)
// //     {
// //         out_ast_curr->list.next = necro_create_fcore_ast_list(final_pass, necro_core_final_pass_go(final_pass, in_ast->list.expr), NULL);
// //         if (necro_is_final_pass_error(final_pass)) return NULL;
// //         in_ast                  = in_ast->list.next;
// //     }
// //     return out_ast_head;
// // }

// // NecroCoreAST_CaseAlt* necro_core_final_pass_alt(NecroCoreFinalPass* final_pass, NecroCoreAST_CaseAlt* in_alt)
// // {
// //     assert(final_pass != NULL);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     if (in_alt == NULL) return NULL;
// //     NecroCoreAST_CaseAlt* out_alt = necro_paged_arena_alloc(&final_pass->core_ast.arena, sizeof(NecroCoreAST_CaseAlt));
// //     if (in_alt->altCon == NULL)
// //         out_alt->altCon = NULL;
// //     else
// //         out_alt->altCon = necro_core_final_pass_go(final_pass, in_alt->altCon);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     out_alt->expr = necro_core_final_pass_go(final_pass, in_alt->expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     out_alt->next = necro_core_final_pass_alt(final_pass, in_alt->next);
// //     return out_alt;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_case(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_CaseAlt*    alts    = necro_core_final_pass_alt(final_pass, in_ast->case_expr.alts);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->case_expr.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_case(final_pass, alts, expr, in_ast->case_expr.type);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_lambda(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* arg     = necro_core_final_pass_go(final_pass, in_ast->lambda.arg);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->lambda.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_lambda(final_pass, arg, expr);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_app(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* exprA   = necro_core_final_pass_go(final_pass, in_ast->app.exprA);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* exprB   = necro_core_final_pass_go(final_pass, in_ast->app.exprB);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_app(final_pass, exprA, exprB);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_let(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* bind    = necro_core_final_pass_go(final_pass, in_ast->let.bind);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->let.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_let(final_pass, bind, expr);
// //     return out_ast;
// // }

// // // TODO: Finish!
// // NecroCoreAST_Expression* necro_core_final_pass_bind(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->bind.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_bind(final_pass, in_ast->bind.var, in_ast->bind.expr, in_ast->bind.is_recursive);
// //     // //--------------------
// //     // // Make non-pointfree
// //     // //--------------------
// //     // NecroCoreAST_Expression* ast      = out_ast;
// //     // NecroType*               for_alls = ((NecroFCoreAST*)out_ast)->necro_type;
// //     // NecroType*               arrows   = NULL;
// //     // while (for_alls->type == NECRO_TYPE_FOR)
// //     // {
// //     //     NecroTypeClassContext* for_all_context = for_alls->for_all.context;
// //     //     while (for_all_context != NULL)
// //     //     {
// //     //         if (ast->expr_type == NECRO_CORE_EXPR_LAM)
// //     //         {
// //     //             ast = ast->lambda.expr;
// //     //         }
// //     //         else
// //     //         {
// //     //             // NecroVar gen_var = necro_gen_var(final_pass, for_alls)
// //     //         }
// //     //         for_all_context = for_all_context->next;
// //     //     }
// //     //     for_alls = for_alls->for_all.type;
// //     //     for_alls = necro_find(for_alls);
// //     // }
// //     // arrows = for_alls;
// //     // arrows = necro_find(arrows);
// //     // while (arrows->type == NECRO_TYPE_FUN)
// //     // {
// //     //     if (ast->expr_type == NECRO_CORE_EXPR_LAM)
// //     //     {
// //     //         ast = ast->lambda.expr;
// //     //     }
// //     //     else
// //     //     {
// //     //         NecroVar                 gen_var = necro_gen_var(final_pass, arrows->fun.type1);
// //     //         NecroCoreAST_Expression* var_ast = necro_create_fcore_ast_var(final_pass, gen_var.id, gen_var.symbol);
// //     //         out_ast                          = necro_create_fcore_ast_app(final_pass, out_ast, var_ast);
// //     //         NecroCoreAST_Expression* arg_ast = necro_create_fcore_ast_var(final_pass, gen_var.id, gen_var.symbol);
// //     //         // TODO: Add new var and lambda to lambdas ast!
// //     //     }
// //     //     arrows = arrows->fun.type2;
// //     //     arrows = necro_find(arrows);
// //     // }
// //     return out_ast;
// // }

// void necro_codegen_data_con(NecroCodeGen* codegen, NecroCoreAST_DataCon* con, LLVMTypeRef data_type_ref, size_t max_arg_count, uint32_t con_number)
// {
//     if (necro_is_codegen_error(codegen)) return;
//     NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&codegen->snapshot_arena);
//     necro_symtable_get(codegen->symtable, con->condid.id)->llvm_type = data_type_ref;
//     char*  con_name  = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(codegen->intern, con->condid.symbol) });
//     size_t arg_count = necro_codegen_count_num_args(codegen, con);
//     assert(arg_count <= max_arg_count);
//     NecroLLVMTypeRefArray    args      = necro_create_llvm_type_ref_array(arg_count);
//     NecroCoreAST_Expression* arg       = con->arg_list;
//     for (size_t i = 0; i < arg_count; ++i)
//     {
//         assert(arg->expr_type == NECRO_CORE_EXPR_LIST);
//         LLVMTypeRef arg_type = necro_get_ast_llvm_type(codegen, arg->list.expr);
//         if (necro_is_codegen_error(codegen))
//             goto necro_codegen_data_con_cleanup;
//         *necro_llvm_type_ref_array_get(&args, i) = LLVMPointerType(arg_type, 0);
//         arg = arg->list.next;
//     }
//     LLVMTypeRef       ret_type = LLVMFunctionType(LLVMPointerType(data_type_ref, 0), necro_llvm_type_ref_array_get(&args, 0), arg_count, 0);
//     LLVMValueRef      make_con = LLVMAddFunction(codegen->mod, con_name, ret_type);
//     LLVMSetFunctionCallConv(make_con, LLVMFastCallConv);
//     LLVMBasicBlockRef entry    = LLVMAppendBasicBlock(make_con, "entry");
//     LLVMPositionBuilderAtEnd(codegen->builder, entry);
//     LLVMValueRef      void_ptr = necro_alloc_codegen(codegen, data_type_ref, (uint8_t)arg_count);
//     LLVMValueRef      data_ptr = LLVMBuildBitCast(codegen->builder, void_ptr, LLVMPointerType(data_type_ref, 0), "data_ptr");
//     necro_init_necro_data(codegen, data_ptr, 0, con_number);
//     //--------------
//     // Parameters
//     for (size_t i = 0; i < max_arg_count; ++i)
//     {
//         char itoa_buff[6];
//         char* location_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "slot_", itoa(i, itoa_buff, 10) });
//         LLVMValueRef slot = necro_snapshot_gep(codegen, location_name, data_ptr, 2, (uint32_t[]) { 0, i + 1 });
//         if (i < arg_count)
//         {
//             char itoa_buff_2[6];
//             char* value_name = necro_concat_strings(&codegen->snapshot_arena, 2, (const char*[]) { "param_", itoa(i, itoa_buff_2, 10) });
//             LLVMValueRef value = LLVMBuildBitCast(codegen->builder, LLVMGetParam(make_con, i), necro_get_value_ptr(codegen), value_name);
//             LLVMBuildStore(codegen->builder, value, slot);
//         }
//         else
//         {
//             LLVMBuildStore(codegen->builder, LLVMConstPointerNull(necro_get_value_ptr(codegen)), slot);
//         }
//     }
//     LLVMBuildRet(codegen->builder, data_ptr);
//     // Create empty NecroNodePrototype that is stateless!
//     // struct NecroNodePrototype* data_con_prototype = necro_declare_node_prototype(codegen, (NecroVar) { .id = con->condid.id, .symbol = con->condid.symbol });
//     // data_con_prototype->type = NECRO_NODE_STATELESS;
//     NecroNodePrototype* prototype = necro_create_prim_node_prototype(codegen, con->condid, data_type_ref, make_con, NECRO_NODE_STATELESS);
//     prototype->call_function = make_con;
// necro_codegen_data_con_cleanup:
//     necro_destroy_llvm_type_ref_array(&args);
//     necro_rewind_arena(&codegen->snapshot_arena, snapshot);
// }

void necro_core_to_node_1_data_con(NecroNodeProgram* program, NecroCoreAST_DataCon* con, NecroNodeAST* struct_type, size_t max_arg_count, size_t con_number)
{
    assert(program != NULL);
    assert(con != NULL);
    assert(struct_type != NULL);
    assert(struct_type->type = NECRO_NODE_STRUCT_DEF);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
    necro_symtable_get(program->symtable, con->condid.id)->necro_node_ast = struct_type;
    char*  con_name  = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "mk", necro_intern_get_string(program->intern, con->condid.symbol), "#" });
    NecroVar con_var = necro_gen_var(program, NULL, con_name, NECRO_NAME_UNIQUE);
    size_t arg_count = necro_count_data_con_args(con);
    assert(arg_count <= max_arg_count);
    NecroNodeType**          parameters = necro_snapshot_arena_alloc(&program->snapshot_arena, arg_count * sizeof(NecroType*));
    NecroCoreAST_Expression* arg        = con->arg_list;
    for (size_t i = 0; i < arg_count; ++i)
    {
        assert(arg->expr_type == NECRO_CORE_EXPR_LIST);
        NecroNodeType* arg_type = necro_core_ast_to_node_type(program, arg->list.expr);
        parameters[i] = necro_create_node_ptr_type(&program->arena, arg_type);
        arg = arg->list.next;
    }
    NecroNodeType*  struct_ptr_type = necro_create_node_ptr_type(&program->arena, struct_type->necro_node_type);
    NecroNodeType*  mk_fn_type      = necro_create_node_fn_type(&program->arena, con_var, struct_ptr_type, parameters, arg_count);
    NecroNodeAST*   mk_fn_body      = necro_create_node_block(program, necro_intern_string(program->intern, "enter"), NULL);
    NecroNodeAST*   mk_fn_def       = necro_create_node_fn(program, con_var, mk_fn_body, mk_fn_type);
    NecroNodeValue  data_ptr        = necro_build_nalloc(program, mk_fn_body, struct_type->necro_node_type, (uint16_t) arg_count);
    necro_build_store_into_tag(program, mk_fn_body, necro_create_uint32_necro_node_value(program, con_number), data_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < max_arg_count; ++i)
    {
        if (i < arg_count)
        {
            char itoa_buff_2[6];
            char* value_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "param_", itoa(i, itoa_buff_2, 10) });
            // NecroNodeValue parameter_value = necro_build_bit_cast(program, mk_fn_body, necro_create_param_reg(program, con_var, i, parameters[i]), program->necro_poly_ptr_type);
            necro_build_store_into_slot(program, mk_fn_body, necro_create_param_reg(program, con_var, i, parameters[i]), data_ptr, (NecroSlot) { .slot_num = i, .necro_node_type = program->necro_poly_ptr_type });
            // LLVMValueRef value = LLVMBuildBitCast(codegen->builder, LLVMGetParam(make_con, i), necro_get_value_ptr(codegen), value_name);
            // LLVMBuildStore(codegen->builder, value, slot);
        }
        else
        {
            // LLVMBuildStore(codegen->builder, LLVMConstPointerNull(necro_get_value_ptr(codegen)), slot);
            necro_build_store_into_slot(program, mk_fn_body, necro_create_null_necro_node_value(program, program->necro_poly_ptr_type), data_ptr, (NecroSlot) { .slot_num = i, .necro_node_type = program->necro_poly_ptr_type });
        }
    }

    necro_build_return(program, mk_fn_body, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

NecroNodeAST* necro_core_to_node_1_data_decl(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);

    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);

    size_t max_arg_count = 0;
    NecroCoreAST_DataCon* con = core_ast->data_decl.con_list;
    while (con != NULL)
    {
        size_t arg_count = necro_count_data_con_args(con);
        max_arg_count    = (arg_count > max_arg_count) ? arg_count : max_arg_count;
        con = con->next;
    }

    // Members
    NecroNodeType** members = necro_snapshot_arena_alloc(&program->snapshot_arena, max_arg_count * sizeof(NecroNodeType*));
    members[0] = program->necro_data_type;
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    {
        members[i] = program->necro_poly_ptr_type;
    }

    // Struct
    NecroNodeAST* struct_type = necro_create_node_struct_def(program, core_ast->data_decl.data_id, members, max_arg_count + 1);

    con = core_ast->data_decl.con_list;
    size_t con_number = 0;
    while (con != NULL)
    {
        necro_core_to_node_1_data_con(program, con, struct_type, max_arg_count, con_number);
        con = con->next;
        con_number++;
    }

    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return NULL;
}

///////////////////////////////////////////////////////
// Go
///////////////////////////////////////////////////////
NecroNodeAST* necro_core_to_node_1_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_DATA_DECL: return necro_core_to_node_1_data_decl(program, core_ast);
    // case NECRO_CORE_EXPR_DATA_CON:  return necro_core_to_node_1_data_decl(program, core_ast);

    // case NECRO_CORE_EXPR_VAR:       return necro_core_final_pass_var(final_pass, in_ast);
    // case NECRO_CORE_EXPR_LIT:       return necro_core_final_pass_lit(final_pass, in_ast);
    // case NECRO_CORE_EXPR_CASE:      return necro_core_final_pass_case(final_pass, in_ast);
    // case NECRO_CORE_EXPR_LAM:       return necro_core_final_pass_lambda(final_pass, in_ast);
    // case NECRO_CORE_EXPR_APP:       return necro_core_final_pass_app(final_pass, in_ast);
    // case NECRO_CORE_EXPR_BIND:      return necro_core_final_pass_bind(final_pass, in_ast);
    // case NECRO_CORE_EXPR_LET:       return necro_core_final_pass_let(final_pass, in_ast);
    // case NECRO_CORE_EXPR_LIST:      return necro_core_final_pass_list(final_pass, in_ast); // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_node_1_go"); return NULL;
    }
    return NULL;
}
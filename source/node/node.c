/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "node.h"
#include <ctype.h>

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
void          necro_core_to_node_1_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer);
void          necro_core_to_node_2_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer);
NecroNodeAST* necro_core_to_node_3_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer);
void          necro_program_add_global(NecroNodeProgram* program, NecroNodeAST* global);
void          necro_program_add_struct(NecroNodeProgram* program, NecroNodeAST* struct_ast);
void          necro_program_add_function(NecroNodeProgram* program, NecroNodeAST* function);
void          necro_program_add_node_def(NecroNodeProgram* program, NecroNodeAST* node_def);
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
// AST construction
///////////////////////////////////////////////////////
NecroNodeAST* necro_create_node_value_ast(NecroNodeProgram* program, NecroNodeValue value, NecroNodeType* necro_node_type)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_VALUE;
    ast->value           = value;
    ast->necro_node_type = necro_node_type;
    switch (value.value_type)
    {
    case NECRO_NODE_VALUE_REG:
        necro_symtable_get(program->symtable, value.reg_name.id)->necro_node_ast = ast;
        break;
    // NECRO_NODE_VALUE_GLOBAL,
    // case NECRO_NODE_VALUE_PARAM:
    //     break;
    default:
        break;
    }
    return ast;
}

NecroNodeAST* necro_create_node_int_lit(NecroNodeProgram* program, int64_t i)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LIT;
    ast->lit.type        = NECRO_NODE_LIT_INT;
    ast->lit.int_literal = i;
    ast->necro_node_type = necro_create_node_int_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_float_lit(NecroNodeProgram* program, double d)
{
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_LIT;
    ast->lit.type           = NECRO_NODE_LIT_DOUBLE;
    ast->lit.double_literal = d;
    ast->necro_node_type    = necro_create_node_float_type(&program->arena);
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

NecroNodeAST* necro_create_reg(NecroNodeProgram* program, NecroNodeType* necro_node_type, const char* reg_name_head)
{
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .reg_name        = necro_gen_var(program, NULL, reg_name_head, NECRO_NAME_NOT_UNIQUE),
        .value_type      = NECRO_NODE_VALUE_REG,
    }, necro_node_type);
}

NecroNodeAST* necro_create_global_value(NecroNodeProgram* program, NecroVar global_name, NecroNodeType* necro_node_type)
{
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .global_name     = global_name,
        .value_type      = NECRO_NODE_VALUE_GLOBAL,
    }, necro_node_type);
}

NecroNodeAST* necro_create_param_reg(NecroNodeProgram* program, NecroNodeAST* fn_value, size_t param_num)
{
    assert(fn_value != NULL);
    assert(fn_value->necro_node_type->type == NECRO_NODE_TYPE_FN);
    assert(fn_value->necro_node_type->fn_type.num_parameters > param_num);
    // necro_type_check(program, fn_ast->necro_node_type->fn_type.parameters[param_num], necro_node_type);
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .param_reg       = { .fn_name = fn_value->fn_def.name, .param_num = param_num },
        .value_type      = NECRO_NODE_VALUE_PARAM,
    }, fn_value->necro_node_type->fn_type.parameters[param_num]);
}

NecroNodeAST* necro_create_uint32_necro_node_value(NecroNodeProgram* program, uint32_t uint_literal)
{
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .uint_literal    = uint_literal,
        .value_type      = NECRO_NODE_VALUE_UINT32_LITERAL,
    }, necro_create_node_uint32_type(&program->arena));
}

NecroNodeAST* necro_create_uint16_necro_node_value(NecroNodeProgram* program, uint16_t uint16_literal)
{
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .uint16_literal  = uint16_literal,
        .value_type      = NECRO_NODE_VALUE_UINT16_LITERAL,
    }, necro_create_node_uint16_type(&program->arena));
}

NecroNodeAST* necro_create_int_necro_node_value(NecroNodeProgram* program, int64_t int_literal)
{
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .int_literal     = int_literal,
        .value_type      = NECRO_NODE_VALUE_INT_LITERAL,
    }, necro_create_node_int_type(&program->arena));
}

NecroNodeAST* necro_create_null_necro_node_value(NecroNodeProgram* program, NecroNodeType* ptr_type)
{
    return necro_create_node_value_ast(program, (NecroNodeValue)
    {
        .value_type      = NECRO_NODE_VALUE_NULL_PTR_LITERAL,
    }, ptr_type);
}

NecroNodeAST* necro_create_node_load_from_ptr(NecroNodeProgram* program, NecroNodeAST* source_ptr_ast, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_NODE_VALUE);
    NecroNodeValue source_ptr = source_ptr_ast->value;
    assert(source_ptr_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_NODE_VALUE_REG || source_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LOAD;
    ast->load.load_type  = NECRO_LOAD_PTR;
    ast->load.source_ptr = source_ptr_ast;
    ast->load.dest_value = necro_create_reg(program, source_ptr_ast->necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type = source_ptr_ast->necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_tag(NecroNodeProgram* program, NecroNodeAST* source_ptr_ast, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_NODE_VALUE);
    NecroNodeValue source_ptr = source_ptr_ast->value;
    assert(source_ptr_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_NODE_VALUE_REG || source_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LOAD;
    ast->load.load_type  = NECRO_LOAD_TAG;
    ast->load.source_ptr = source_ptr_ast;
    ast->load.dest_value = necro_create_reg(program, necro_create_node_uint32_type(&program->arena), dest_name);
    ast->necro_node_type = ast->load.dest_value->necro_node_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_from_slot(NecroNodeProgram* program, NecroNodeAST* source_ptr_ast, size_t source_slot, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_NODE_VALUE);
    assert(source_ptr_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(source_ptr_ast->necro_node_type->ptr_type.element_type->type == NECRO_NODE_TYPE_STRUCT);
    assert(source_ptr_ast->necro_node_type->ptr_type.element_type->struct_type.num_members > source_slot);
    NecroNodeAST* ast               = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                       = NECRO_NODE_LOAD;
    ast->load.load_type             = NECRO_LOAD_SLOT;
    ast->load.load_slot.source_ptr  = source_ptr_ast;
    ast->load.load_slot.source_slot = source_slot;
    ast->necro_node_type            = source_ptr_ast->necro_node_type->ptr_type.element_type->struct_type.members[source_slot];
    ast->load.dest_value            = necro_create_reg(program, ast->necro_node_type, dest_name);
    return ast;
}

NecroNodeAST* necro_create_node_load_from_global(NecroNodeProgram* program, NecroNodeAST* source_global_ast, NecroNodeType* necro_node_type, const char* dest_name)
{
    assert(necro_node_type->type == NECRO_NODE_TYPE_PTR);
    // assert(source_ptr.value_type == NECRO_NODE_VALUE_REG || source_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_LOAD;
    ast->load.load_type     = NECRO_LOAD_GLOBAL;
    ast->load.source_global = source_global_ast;
    ast->load.dest_value    = necro_create_reg(program, necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type    = necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_ptr(NecroNodeProgram* program, NecroNodeAST* source_value_ast, NecroNodeAST* dest_ptr_ast)
{
    assert(source_value_ast->type == NECRO_NODE_VALUE);
    assert(dest_ptr_ast->type == NECRO_NODE_VALUE);
    NecroNodeValue dest_ptr     = dest_ptr_ast->value;
    assert(dest_ptr_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_NODE_VALUE_REG || dest_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_PTR;
    ast->store.source_value = source_value_ast;
    ast->store.dest_ptr     = dest_ptr_ast;
    ast->necro_node_type    = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_tag(NecroNodeProgram* program, NecroNodeAST* source_value_ast, NecroNodeAST* dest_ptr_ast)
{
    assert(source_value_ast->type == NECRO_NODE_VALUE);
    assert(dest_ptr_ast->type == NECRO_NODE_VALUE);
    NecroNodeValue dest_ptr     = dest_ptr_ast->value;
    assert(dest_ptr_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_NODE_VALUE_REG || dest_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    assert(source_value_ast->necro_node_type->type == NECRO_NODE_TYPE_UINT32);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_TAG;
    ast->store.source_value = source_value_ast;
    ast->store.dest_ptr     = dest_ptr_ast;
    ast->necro_node_type    = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_slot(NecroNodeProgram* program, NecroNodeAST* source_value_ast, NecroNodeAST* dest_ptr_ast, size_t dest_slot)
{
    assert(source_value_ast->type == NECRO_NODE_VALUE);
    assert(dest_ptr_ast->type == NECRO_NODE_VALUE);
    assert(dest_ptr_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr_ast->necro_node_type->ptr_type.element_type->type == NECRO_NODE_TYPE_STRUCT);
    assert(dest_ptr_ast->necro_node_type->ptr_type.element_type->struct_type.num_members > dest_slot);
    necro_type_check(program, source_value_ast->necro_node_type, dest_ptr_ast->necro_node_type->ptr_type.element_type->struct_type.members[dest_slot]);

    NecroNodeAST* ast               = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                       = NECRO_NODE_STORE;
    ast->store.store_type           = NECRO_STORE_SLOT;
    ast->store.source_value         = source_value_ast;
    ast->store.store_slot.dest_ptr  = dest_ptr_ast;
    ast->store.store_slot.dest_slot = dest_slot;
    ast->necro_node_type            = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_global(NecroNodeProgram* program, NecroNodeAST* source_value_ast, NecroNodeAST* dest_global_ast, NecroNodeType* necro_node_type)
{
    assert(source_value_ast->type == NECRO_NODE_VALUE);
    assert(necro_node_type->type == NECRO_NODE_TYPE_PTR);
    necro_type_check(program, source_value_ast->necro_node_type, necro_node_type->ptr_type.element_type);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_GLOBAL;
    ast->store.source_value = source_value_ast;
    ast->store.dest_global  = dest_global_ast;
    ast->necro_node_type    = NULL;
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

NecroNodeAST* necro_create_node_call(NecroNodeProgram* program, NecroNodeAST* fn_value_ast, NecroNodeAST** a_parameters, size_t num_parameters, const char* dest_name)
{
    // type_check
    assert(fn_value_ast->type == NECRO_NODE_VALUE);
    assert(fn_value_ast->necro_node_type->type == NECRO_NODE_TYPE_FN);
    assert(fn_value_ast->necro_node_type->fn_type.num_parameters == num_parameters);
    for (size_t i = 0; i < num_parameters; i++)
    {
        necro_type_check(program, fn_value_ast->necro_node_type->fn_type.parameters[i], a_parameters[i]->necro_node_type);
    }
    NecroNodeAST* ast          = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                  = NECRO_NODE_CALL;
    ast->call.fn_value         = fn_value_ast;
    NecroNodeAST** parameters = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroNodeAST*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroNodeAST*));
    ast->call.parameters       = parameters;
    ast->call.num_parameters   = num_parameters;
    ast->call.result_reg       = necro_create_reg(program, fn_value_ast->necro_node_type->fn_type.return_type, dest_name);
    ast->necro_node_type       = fn_value_ast->necro_node_type->fn_type.return_type;
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
    ast->fn_def.fn_value  = necro_create_global_value(program, name, necro_node_type);
    ast->necro_node_type  = necro_node_type;
    assert(call_body->type == NECRO_NODE_BLOCK);
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    necro_program_add_function(program, ast);
    ast->fn_def._curr_block = call_body;
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

NecroNodeAST* necro_create_node_initial_node_def(NecroNodeProgram* program, NecroVar bind_name, NecroNodeAST* outer, NecroNodeType* value_type)
{
    NecroArenaSnapshot snapshot          = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroNodeAST* ast                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                            = NECRO_NODE_DEF;
    ast->node_def.bind_name              = bind_name;
    char itoabuf[10];
    char* node_name                      = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { necro_intern_get_string(program->intern, bind_name.symbol), "Node#", itoa(bind_name.id.id, itoabuf, 10) });
    node_name[0]                         = toupper(node_name[0]);
    ast->node_def.node_name              = necro_gen_var(program, ast, node_name, NECRO_NAME_UNIQUE);
    ast->node_def.arg_names              = NULL;
    ast->node_def.num_arg_names          = 0;
    ast->node_def.mk_fn                  = NULL;
    ast->node_def.ini_fn                 = NULL;
    ast->node_def.update_fn              = NULL;
    ast->node_def.global_value           = NULL;
    ast->node_def.update_error_block     = NULL;
    ast->node_def.initial_tag            = 0;
    ast->node_def.state_type             = NECRO_STATE_STATEFUL;
    ast->node_def.outer                  = outer;
    ast->node_def.default_init           = true;
    ast->node_def.default_mk             = true;
    ast->necro_node_type                 = NULL; // TODO: Node type will be constructed after analysis!
    if (value_type->type == NECRO_NODE_TYPE_FN)
    {
        ast->node_def.value_type = value_type->fn_type.return_type;
        ast->node_def.fn_type    = value_type;
    }
    else
    {
        ast->node_def.value_type = necro_create_node_ptr_type(&program->arena, value_type);
        ast->node_def.fn_type    = NULL;
    }
    const size_t initial_members_size    = 4;
    ast->node_def.members                = necro_paged_arena_alloc(&program->arena, initial_members_size * sizeof(NecroSlot));
    ast->node_def.num_members            = 0;
    ast->node_def.members_size           = initial_members_size;
    necro_symtable_get(program->symtable, bind_name.id)->necro_node_ast = ast;
    necro_symtable_get(program->symtable, ast->node_def.node_name.id)->necro_node_ast = ast;
    if (outer == 0)
        necro_program_add_node_def(program, ast);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return ast;
}

// // Assumes zero-initialized
// NecroNodeAST* necro_create_node_constant_struct(NecroNodeProgram* program, NecroNodeType* necro_node_type)
// {
//     assert(necro_node_type->type == NECRO_NODE_TYPE_STRUCT);
//     NecroNodeAST* ast                  = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
//     ast->type                          = NECRO_NODE_CONSTANT_DEF;
//     ast->necro_node_type               = necro_node_type;
//     ast->constant.constant_type        = NECRO_CONSTANT_STRUCT;
//     ast->constant.constant_struct.name = necro_node_type->struct_type.name;
//     return ast;
// }

NecroNodeAST* necro_create_node_gep(NecroNodeProgram* program, NecroNodeAST* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(source_value->type == NECRO_NODE_VALUE);
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_GEP;
    ast->gep.source_value = source_value;
    int32_t* indices      = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(int32_t));
    memcpy(indices, a_indices, num_indices * sizeof(int32_t));
    ast->gep.indices      = indices;
    ast->gep.num_indices  = num_indices;
    // type check gep
    NecroNodeType* necro_node_type = source_value->necro_node_type;
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
    ast->gep.dest_value  = necro_create_reg(program, necro_node_type, dest_name);
    ast->necro_node_type = necro_node_type;
    return ast;
}

NecroNodeAST* necro_create_bit_cast(NecroNodeProgram* program, NecroNodeAST* from_value_ast, NecroNodeType* to_type)
{
    assert(from_value_ast->necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(to_type->type == NECRO_NODE_TYPE_PTR);
    NecroNodeValue from_value = from_value_ast->value;
    NecroNodeAST* ast         = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                 = NECRO_NODE_BIT_CAST;
    ast->bit_cast.from_value  = from_value_ast;
    switch (from_value.value_type)
    {
    case NECRO_NODE_VALUE_REG:
    case NECRO_NODE_VALUE_PARAM:
    case NECRO_NODE_VALUE_GLOBAL:
    case NECRO_NODE_VALUE_NULL_PTR_LITERAL:
        ast->bit_cast.to_value = necro_create_reg(program, to_type, "cst");
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
    ast->nalloc.result_reg    = necro_create_reg(program, type_ptr, "data_ptr");
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

void necro_append_block(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroSymbol block_name)
{
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* block = necro_create_node_block(program, block_name, NULL);
    if (fn_def->fn_def.call_body == NULL)
    {
        fn_def->fn_def.call_body   = block;
        fn_def->fn_def._curr_block = block;
        return;
    }
    NecroNodeAST* fn_block = fn_def->fn_def.call_body;
    while (block->block.next_block != NULL)
    {
        fn_block = fn_block->block.next_block;
    }
    fn_block->block.next_block = block;
    fn_def->fn_def._curr_block = block;
}

void necro_move_to_block(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* block)
{
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    assert(block->type == NECRO_NODE_BLOCK);
    NecroNodeAST* fn_block = fn_def->fn_def.call_body;
    while (fn_block != NULL)
    {
        if (fn_block == block)
        {
            fn_def->fn_def._curr_block = block;
            return;
        }
        fn_block = fn_block->block.next_block;
    }
    assert(false);
}

NecroNodeAST* necro_build_nalloc(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeType* type, uint16_t a_slots_used)
{
    assert(program != NULL);
    assert(type != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* data_ptr = necro_create_nalloc(program, type, a_slots_used);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, data_ptr);
    return data_ptr->nalloc.result_reg;
}

void necro_build_store_into_tag(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, NecroNodeAST* dest_ptr)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* store_ast = necro_create_node_store_into_tag(program, source_value, dest_ptr);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, store_ast);
}

void necro_build_store_into_ptr(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, NecroNodeAST* dest_ptr)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, necro_create_node_store_into_ptr(program, source_value, dest_ptr));
}

void necro_build_store_into_slot(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, NecroNodeAST* dest_ptr, size_t dest_slot)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, necro_create_node_store_into_slot(program, source_value, dest_ptr, dest_slot));
}

void necro_build_store_into_global(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, NecroNodeAST* dest_global, NecroNodeType* necro_node_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, necro_create_node_store_into_global(program, source_value, dest_global, necro_node_type));
}

NecroNodeAST* necro_build_bit_cast(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* value, NecroNodeType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* ast = necro_create_bit_cast(program, value, to_type);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->bit_cast.to_value;
}

NecroNodeAST* necro_build_gep(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* ast = necro_create_node_gep(program, source_value, a_indices, num_indices, dest_name);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->gep.dest_value;
}

NecroNodeAST* necro_build_load_from_slot(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* source_ptr_ast, size_t source_slot, const char* dest_name_header)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* ast = necro_create_node_load_from_slot(program, source_ptr_ast, source_slot, dest_name_header);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->load.dest_value;
}

static NecroNodeAST* necro_build_call(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* fn_to_call_value, NecroNodeAST** a_parameters, size_t num_parameters, const char* dest_name_header)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    NecroNodeAST* ast = necro_create_node_call(program, fn_to_call_value, a_parameters, num_parameters, dest_name_header);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->call.result_reg;
}

void necro_build_return(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* return_value)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_NODE_FN_DEF);
    fn_def->fn_def._curr_block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                           = NECRO_TERM_RETURN;
    fn_def->fn_def._curr_block->block.terminator->return_terminator.return_value = return_value;
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
        .node_defs      = necro_create_necro_node_ast_vector(),
        .main           = NULL,
        .intern         = intern,
        .symtable       = symtable,
        .prim_types     = prim_types,
        .gen_vars       = 0,
    };
    NecroNodeAST* necro_data_struct = necro_create_node_struct_def(&program, necro_con_to_var(prim_types->necro_data_type), (NecroNodeType*[]) { necro_create_node_uint32_type(&program.arena), necro_create_node_uint32_type(&program.arena) }, 2);
    program.necro_data_type         = necro_data_struct->necro_node_type;
    NecroVar      poly_var          = necro_gen_var(&program, NULL, "Poly#", NECRO_NAME_UNIQUE);
    NecroNodeAST* necro_poly_struct = necro_create_node_struct_def(&program, poly_var,  (NecroNodeType*[]) { program.necro_data_type }, 1);
    program.necro_poly_type         = necro_poly_struct->necro_node_type;
    program.necro_poly_ptr_type     = necro_create_node_ptr_type(&program.arena, program.necro_poly_type);
    return program;
}

void necro_destroy_node_program(NecroNodeProgram* program)
{
    necro_destroy_paged_arena(&program->arena);
    necro_destroy_snapshot_arena(&program->snapshot_arena);
    necro_destroy_necro_node_ast_vector(&program->structs);
    necro_destroy_necro_node_ast_vector(&program->globals);
    necro_destroy_necro_node_ast_vector(&program->functions);
    necro_destroy_necro_node_ast_vector(&program->node_defs);
}

void necro_program_add_struct(NecroNodeProgram* program, NecroNodeAST* struct_ast)
{
    assert(struct_ast->type == NECRO_NODE_STRUCT_DEF);
    necro_push_necro_node_ast_vector(&program->structs, &struct_ast);
}

void necro_program_add_global(NecroNodeProgram* program, NecroNodeAST* global)
{
    // assert(global->type == NECRO_NODE_CONSTANT_DEF);
    assert(global->type == NECRO_NODE_VALUE);
    assert(global->value.value_type == NECRO_NODE_VALUE_GLOBAL);
    necro_push_necro_node_ast_vector(&program->globals, &global);
}

void necro_program_add_function(NecroNodeProgram* program, NecroNodeAST* function)
{
    assert(function->type == NECRO_NODE_FN_DEF);
    necro_push_necro_node_ast_vector(&program->functions, &function);
}

void necro_program_add_node_def(NecroNodeProgram* program, NecroNodeAST* node_def)
{
    assert(node_def->type == NECRO_NODE_DEF);
    necro_push_necro_node_ast_vector(&program->node_defs, &node_def);
}

///////////////////////////////////////////////////////
// Core to Node Pass 1
///////////////////////////////////////////////////////
void necro_core_to_node_1_data_con(NecroNodeProgram* program, NecroCoreAST_DataCon* con, NecroNodeAST* struct_type, size_t max_arg_count, size_t con_number)
{
    assert(program != NULL);
    assert(con != NULL);
    assert(struct_type != NULL);
    assert(struct_type->type = NECRO_NODE_STRUCT_DEF);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
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
    NecroNodeType*  mk_fn_type      = necro_create_node_fn_type(&program->arena, struct_ptr_type, parameters, arg_count);
    NecroNodeAST*   mk_fn_body      = necro_create_node_block(program, necro_intern_string(program->intern, "enter"), NULL);
    NecroNodeAST*   mk_fn_def       = necro_create_node_fn(program, con_var, mk_fn_body, mk_fn_type);
    NecroNodeAST*   data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type->necro_node_type, (uint16_t) arg_count);
    necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_node_value(program, con_number), data_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < max_arg_count; ++i)
    {
        if (i < arg_count)
        {
            char itoa_buff_2[6];
            char* value_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "param_", itoa(i, itoa_buff_2, 10) });
            necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
        }
        else
        {
            necro_build_store_into_slot(program, mk_fn_def, necro_create_null_necro_node_value(program, program->necro_poly_ptr_type), data_ptr, i + 1);
        }
    }

    necro_symtable_get(program->symtable, con->condid.id)->necro_node_ast = mk_fn_def->fn_def.fn_value;
    necro_build_return(program, mk_fn_def, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

void necro_core_to_node_1_data_decl(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast)
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
}

void necro_core_to_node_1_bind(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);

    //---------------
    // Create initial NodeDef
    NecroNodeAST* node_def = necro_create_node_initial_node_def(program, core_ast->bind.var, outer, necro_core_ast_to_node_type(program, core_ast));

    //---------------
    // Count and assign arg names
    size_t                   num_args = 0;
    NecroCoreAST_Expression* lambdas  = core_ast->bind.expr;
    while (lambdas->expr_type == NECRO_CORE_EXPR_LAM)
    {
        num_args++;
        lambdas = lambdas->lambda.expr;
    }
    if (num_args > 0)
    {
        node_def->node_def.num_arg_names = num_args;
        node_def->node_def.arg_names     = necro_paged_arena_alloc(&program->arena, num_args * sizeof(NecroVar));
        num_args                         = 0;
        NecroCoreAST_Expression* lambdas = core_ast->bind.expr;
        while (lambdas->expr_type == NECRO_CORE_EXPR_LAM)
        {
            node_def->node_def.arg_names[num_args] = lambdas->lambda.arg->var;
            num_args++;
            lambdas = lambdas->lambda.expr;
        }
    }

    //---------------
    // go deeper
    necro_core_to_node_1_go(program, core_ast->bind.expr, node_def);
}

void necro_core_to_node_1_let(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_1_go(program, core_ast->let.bind, outer);
    necro_core_to_node_1_go(program, core_ast->let.expr, outer);
}

void necro_core_to_node_1_lambda(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_1_go(program, core_ast->lambda.arg, outer);
    necro_core_to_node_1_go(program, core_ast->lambda.expr, outer);
}

void necro_core_to_node_1_app(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_1_go(program, core_ast->app.exprA, outer);
    necro_core_to_node_1_go(program, core_ast->app.exprB, outer);
}

void necro_core_to_node_1_case(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_1_go(program, core_ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = core_ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_node_1_go(program, alts->expr, outer);
        alts = alts->next;
    }
}

///////////////////////////////////////////////////////
// Core to Node Pass 1 Go
///////////////////////////////////////////////////////
void necro_core_to_node_1_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_DATA_DECL: necro_core_to_node_1_data_decl(program, core_ast); return;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "found NECRO_CORE_EXPR_DATA_CON in necro_core_to_node_1_go"); return;
    case NECRO_CORE_EXPR_BIND:      necro_core_to_node_1_bind(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LET:       necro_core_to_node_1_let(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LAM:       necro_core_to_node_1_lambda(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_APP:       necro_core_to_node_1_app(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_CASE:      necro_core_to_node_1_case(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_VAR:       return;
    case NECRO_CORE_EXPR_LIT:       return;
    case NECRO_CORE_EXPR_LIST:      assert(false); return; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_node_1_go"); return;
    }
}

///////////////////////////////////////////////////////
// Core to Node Pass 2
///////////////////////////////////////////////////////
static bool necro_is_var_node_arg(NecroNodeProgram* program, NecroNodeDef* node_def, NecroVar var)
{
    for (size_t i = 0; i < node_def->num_arg_names; i++)
    {
        if (node_def->arg_names[i].id.id == var.id.id)
            return true;
    }
    return false;
}

NecroSlot necro_add_member(NecroNodeProgram* program, NecroNodeDef* node_def, NecroNodeType* new_member)
{
    assert(program != NULL);
    assert(node_def != NULL);
    if (node_def->num_members + 1 >= node_def->members_size)
    {
        NecroSlot* old_members   = node_def->members;
        node_def->members        = necro_paged_arena_alloc(&program->arena, node_def->members_size * 2 * sizeof(NecroSlot));
        memcpy(node_def->members, old_members, node_def->members_size * sizeof(NecroSlot));
        node_def->members_size *= 2;
    }
    NecroSlot slot = (NecroSlot) { .necro_node_type = new_member, .slot_num = node_def->num_members + 2 };
    node_def->members[node_def->num_members] = slot;
    node_def->num_members++;
    return slot;
}

void necro_calculate_statefulness(NecroNodeProgram* program, NecroNodeAST* ast)
{
    assert(program != NULL);
    assert(ast->type = NECRO_NODE_DEF);
    if (ast->node_def.num_members == 0 && ast->node_def.num_arg_names == 0)
        ast->node_def.state_type = NECRO_STATE_CONSTANT;
    else if (ast->node_def.num_members == 0 && ast->node_def.num_arg_names > 0)
        ast->node_def.state_type = NECRO_STATE_POINTWISE;
    else if (ast->node_def.num_members > 0)
        ast->node_def.state_type = NECRO_STATE_STATEFUL;
}

void necro_core_to_node_2_bind(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);

    // Retrieve node_def
    NecroNodeAST* node_def = necro_symtable_get(program->symtable, core_ast->bind.var.id)->necro_node_ast;
    assert(node_def != NULL);
    assert(node_def->type == NECRO_NODE_DEF);

    // Go deeper
    if (outer != NULL)
    {
        necro_core_to_node_2_go(program, core_ast->bind.expr, outer);
        necro_calculate_statefulness(program, node_def);
        return;
    }
    node_def->node_def.pushed = true;
    necro_core_to_node_2_go(program, core_ast->bind.expr, node_def);
    necro_calculate_statefulness(program, node_def);
    node_def->node_def.pushed = false;

    // Calculate node type
    NecroArenaSnapshot snapshot          = necro_get_arena_snapshot(&program->snapshot_arena);
    const size_t       num_node_members  = (2 + node_def->node_def.num_members);
    NecroNodeType**    node_type_members = necro_snapshot_arena_alloc(&program->snapshot_arena, num_node_members * sizeof(NecroNodeType*));
    node_type_members[0] = program->necro_data_type;
    node_type_members[1] = node_def->node_def.value_type;
    for (size_t i = 0; i < node_def->node_def.num_members; ++i)
        node_type_members[i + 2] = node_def->node_def.members[i].necro_node_type;
    node_def->necro_node_type = necro_create_node_struct_type(&program->arena, node_def->node_def.node_name, node_type_members, num_node_members);

    // add_global
    if (outer == NULL && (node_def->node_def.state_type == NECRO_STATE_CONSTANT || node_def->node_def.state_type == NECRO_STATE_STATEFUL) && node_def->node_def.num_arg_names == 0)
    {
        NecroNodeAST* global_value      = necro_create_global_value(program, node_def->node_def.bind_name, necro_create_node_ptr_type(&program->arena, node_def->necro_node_type));
        node_def->node_def.global_value = global_value;
        necro_program_add_global(program, global_value);
    }

    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

void necro_core_to_node_2_let(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_2_go(program, core_ast->let.bind, outer);
    necro_core_to_node_2_go(program, core_ast->let.expr, outer);
}

void necro_core_to_node_2_lambda(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_2_go(program, core_ast->lambda.arg, outer);
    necro_core_to_node_2_go(program, core_ast->lambda.expr, outer);
}

void necro_core_to_node_2_case(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_2_go(program, core_ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = core_ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_node_2_go(program, alts->expr, outer);
        alts = alts->next;
    }
}

void necro_core_to_node_2_app(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);

    NecroCoreAST_Expression*  function        = core_ast;
    size_t                    arg_count       = 0;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        necro_core_to_node_2_go(program, function->app.exprB, outer);
        arg_count++;
        function = function->app.exprA;
    }
    assert(function->expr_type == NECRO_CORE_EXPR_VAR);

    NecroNodeAST*  fn_value      = necro_symtable_get(program->symtable, function->var.id)->necro_node_ast;
    bool           is_persistent = false;
    NecroNodeAST*  fn_def        = NULL;
    NecroNodeType* fn_type       = NULL;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_NODE_DEF)
    {
        fn_def        = fn_value;
        is_persistent = fn_value->node_def.state_type == NECRO_STATE_STATEFUL;
        fn_type       = fn_value->node_def.fn_type;
    }
    else
    {
        fn_type = fn_value->necro_node_type;
    }
    assert(fn_type->type == NECRO_NODE_TYPE_FN);
    assert(fn_type->fn_type.num_parameters == arg_count);
    if (!is_persistent)
        return;
    bool is_dynamic = false;
    if (!is_dynamic)
    {
        NecroSlot slot = necro_add_member(program, &outer->node_def, program->necro_data_type);
        for (size_t i = 1; i < fn_def->necro_node_type->struct_type.num_members; ++i)
        {
            necro_add_member(program, &outer->node_def, fn_def->necro_node_type->struct_type.members[i]);
        }
        core_ast->app.persistent_slot = slot.slot_num;
    }
    else
    {
    }
}

void necro_core_to_node_2_var(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    NecroNodeAST* node_ast = necro_symtable_get(program->symtable, core_ast->var.id)->necro_node_ast;
    if (node_ast == NULL)
    {
        // Lambda args are NULL at first...better way to differentiate!?
        // assert(false);
        return;
    }
    if (node_ast->type != NECRO_NODE_DEF)
        return;
    if (node_ast->node_def.state_type != NECRO_STATE_STATEFUL)
        return;
    if (necro_is_var_node_arg(program, &outer->node_def, core_ast->var))
        return;
    if (node_ast->node_def.outer == NULL)
        return;
    if (necro_symtable_get(program->symtable, core_ast->var.id)->persistent_slot != 0)
        return;
    NecroSlot slot = necro_add_member(program, &outer->node_def, node_ast->node_def.value_type);
    necro_symtable_get(program->symtable, core_ast->var.id)->persistent_slot = slot.slot_num;
}

///////////////////////////////////////////////////////
// Core to Node Pass 2 Go
///////////////////////////////////////////////////////
void necro_core_to_node_2_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_BIND:      necro_core_to_node_2_bind(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LET:       necro_core_to_node_2_let(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LAM:       necro_core_to_node_2_lambda(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_APP:       necro_core_to_node_2_app(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_CASE:      necro_core_to_node_2_case(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_VAR:       necro_core_to_node_2_var(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_DATA_DECL: return;
    case NECRO_CORE_EXPR_DATA_CON:  return;
    case NECRO_CORE_EXPR_LIT:       return;
    case NECRO_CORE_EXPR_LIST:      assert(false); return; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_node_2_go"); return;
    }
}

///////////////////////////////////////////////////////
// Core to Node Pass 3
///////////////////////////////////////////////////////
NecroNodeAST* necro_core_to_node_3_bind(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);

    // Retrieve node_def
    NecroSymbolInfo* info     = necro_symtable_get(program->symtable, core_ast->bind.var.id);
    NecroNodeAST*    node_def = info->necro_node_ast;
    assert(node_def != NULL);
    assert(node_def->type == NECRO_NODE_DEF);

    if (outer != NULL)
    {
        NecroNodeAST* return_value = necro_core_to_node_3_go(program, core_ast->bind.expr, outer);
        if (info->persistent_slot != 0)
        {
            NecroNodeAST* param0 = necro_create_param_reg(program, outer->node_def.update_fn, 0);
            necro_build_store_into_slot(program, node_def->node_def.outer->node_def.update_fn, return_value, param0, info->persistent_slot);
        }
        else
        {
            info->necro_node_ast = return_value;
        }
        return return_value;
    }
    if (node_def->node_def.state_type == NECRO_STATE_STATIC)
    {
        return necro_core_to_node_3_go(program, core_ast->bind.expr, outer);
    }

    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);

    // Start function
    bool            is_stateful       = node_def->node_def.state_type == NECRO_STATE_STATEFUL;
    const char*     update_name       = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "update", necro_intern_get_string(program->intern, node_def->node_def.node_name.symbol) });
    NecroVar        update_var        = necro_gen_var(program, NULL, update_name, NECRO_NAME_UNIQUE);
    size_t          num_update_params = node_def->node_def.num_arg_names;
    if (num_update_params > 0)
        assert(node_def->node_def.num_arg_names == node_def->node_def.fn_type->fn_type.num_parameters);
    if (is_stateful)
        num_update_params++;
    NecroNodeType** update_params     = necro_snapshot_arena_alloc(&program->snapshot_arena, num_update_params * sizeof(NecroNodeType*));
    if (is_stateful)
    {
        update_params[0] = necro_create_node_ptr_type(&program->arena, node_def->necro_node_type);
    }
    for (size_t i = 0; i < node_def->node_def.num_arg_names; ++i)
    {
        update_params[i + (is_stateful ? 1 : 0)] = node_def->node_def.fn_type->fn_type.parameters[i];
    }
    NecroNodeType*  update_fn_type  = necro_create_node_fn_type(&program->arena, node_def->node_def.value_type, update_params, num_update_params);
    NecroNodeAST*   update_fn_body  = necro_create_node_block(program, necro_intern_string(program->intern, "enter"), NULL);
    NecroNodeAST*   update_fn_def   = necro_create_node_fn(program, update_var, update_fn_body, update_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the node
    node_def->node_def.update_fn = update_fn_def;

    for (size_t i = 0; i < node_def->node_def.num_arg_names; ++i)
    {
        NecroNodeAST* param_ast = necro_create_param_reg(program, update_fn_def, i + (is_stateful ? 1 : 0));
        necro_symtable_get(program->symtable, node_def->node_def.arg_names[i].id)->necro_node_ast = param_ast;
    }

    // Go deeper
    NecroNodeAST* result = necro_core_to_node_3_go(program, core_ast->bind.expr, node_def);

    // Finish function
    necro_build_return(program, update_fn_def, result);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return NULL;
}

NecroNodeAST* necro_core_to_node_3_let(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_3_go(program, core_ast->let.bind, outer);
    return necro_core_to_node_3_go(program, core_ast->let.expr, outer);
}

NecroNodeAST* necro_core_to_node_3_lambda(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    // necro_core_to_node_3_go(program, core_ast->lambda.arg, outer);
    return necro_core_to_node_3_go(program, core_ast->lambda.expr, outer);
}

NecroNodeAST* necro_core_to_node_3_case(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    necro_core_to_node_3_go(program, core_ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = core_ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_node_3_go(program, alts->expr, outer);
        alts = alts->next;
    }
    return NULL;
}

NecroNodeAST* necro_core_to_node_3_app(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);

    NecroCoreAST_Expression*  function        = core_ast;
    size_t                    arg_count       = 0;
    size_t                    persistent_slot = core_ast->app.persistent_slot;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    assert(function->expr_type == NECRO_CORE_EXPR_VAR);

    NecroNodeAST* fn_value = necro_symtable_get(program->symtable, function->var.id)->necro_node_ast;
    NecroNodeAST* node_def = NULL;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_NODE_DEF)
    {
        node_def = fn_value;
        fn_value = fn_value->node_def.update_fn->fn_def.fn_value;
    }
    else if (fn_value->type == NECRO_NODE_FN_DEF)
    {
        fn_value = fn_value->fn_def.fn_value;
    }
    assert(fn_value->type == NECRO_NODE_VALUE);
    assert(fn_value->necro_node_type->type == NECRO_NODE_TYPE_FN);

    if (persistent_slot != 0)
        arg_count++;

    assert(fn_value->necro_node_type->fn_type.num_parameters == arg_count);

    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroNodeAST**     args      = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroNodeAST*));
    size_t             arg_index = arg_count - 1;
    function                     = core_ast;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_core_to_node_3_go(program, function->app.exprB, outer);
        // args[arg_index] = necro_maybe_cast(codegen, args[arg_index], LLVMTypeOf(params[arg_index]));
        arg_index--;
        function        = function->app.exprA;
    }

    necro_rewind_arena(&program->snapshot_arena, snapshot);
    // Pass in persistent var
    if (persistent_slot != 0)
    {
        assert(node_def != NULL);
        bool is_dynamic = false;
        if (!is_dynamic)
        {
            NecroNodeAST* gep_value  = necro_build_gep(program, outer->node_def.update_fn, necro_create_param_reg(program, outer->node_def.update_fn->fn_def.fn_value, 0), (uint32_t[]) { 0, persistent_slot }, 2, "gep");
            NecroNodeAST* cast_value = necro_build_bit_cast(program, outer->node_def.update_fn, gep_value, necro_create_node_ptr_type(&program->arena, node_def->necro_node_type));
            args[0]                  = cast_value;
        }
        else
        {
            // TODO: dynamic application
        }
    }
    return necro_build_call(program, outer->node_def.update_fn, fn_value, args, arg_count, "app");
}

NecroNodeAST* necro_core_to_node_3_var(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_NODE_DEF);
    NecroSymbolInfo* info = necro_symtable_get(program->symtable, core_ast->var.id);
    // It's a constructor
    if (info->is_constructor)
    {
        return necro_build_call(program, outer->node_def.update_fn, info->necro_node_ast, NULL, 0, "con");
    }
    // // It's a dynamic value
    // {
    // }
    // It's a persistent value
    else if (info->persistent_slot != 0)
    {
        NecroNodeAST* param0 = necro_create_param_reg(program, outer->node_def.update_fn, 0);
        NecroNodeAST* loaded = necro_build_load_from_slot(program, outer->node_def.update_fn, param0, info->persistent_slot, "var");
        // How do we safely cache loaded values!?
        return loaded;
    }
    // It's a local
    else if (info->necro_node_ast->type == NECRO_NODE_VALUE)
    {
        return info->necro_node_ast;
    }
    // Else it's a global
    else if (info->necro_node_ast->type == NECRO_NODE_DEF)
    {
        assert(info->necro_node_ast->node_def.global_value != NULL);
        return necro_build_load_from_slot(program, outer->node_def.update_fn, info->necro_node_ast->node_def.global_value, 1, "glb");
    }
    else
    {
        assert(false);
        return NULL;
    }
}

///////////////////////////////////////////////////////
// Core to Node Pass 3 Go
///////////////////////////////////////////////////////
NecroNodeAST* necro_core_to_node_3_go(NecroNodeProgram* program, NecroCoreAST_Expression* core_ast, NecroNodeAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_BIND:      return necro_core_to_node_3_bind(program, core_ast, outer);
    case NECRO_CORE_EXPR_LET:       return necro_core_to_node_3_let(program, core_ast, outer);
    case NECRO_CORE_EXPR_LAM:       return necro_core_to_node_3_lambda(program, core_ast, outer);
    case NECRO_CORE_EXPR_APP:       return necro_core_to_node_3_app(program, core_ast, outer);
    case NECRO_CORE_EXPR_CASE:      return necro_core_to_node_3_case(program, core_ast, outer);
    case NECRO_CORE_EXPR_VAR:       return necro_core_to_node_3_var(program, core_ast, outer);
    case NECRO_CORE_EXPR_DATA_DECL: return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
    case NECRO_CORE_EXPR_LIT:       return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_node_3_go"); return NULL;
    }
}

///////////////////////////////////////////////////////
// Core to Node
///////////////////////////////////////////////////////
NecroNodeProgram necro_core_to_node(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    NecroNodeProgram program = necro_create_initial_node_program(symtable->intern, symtable, prim_types);

    // Pass 1
    NecroCoreAST_Expression* top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_node_1_go(&program, top_level_list->list.expr, NULL);
        top_level_list = top_level_list->list.next;
    }

    // Pass 2
    top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_node_2_go(&program, top_level_list->list.expr, NULL);
        top_level_list = top_level_list->list.next;
    }

    // return program;

    // Pass 3
    top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_node_3_go(&program, top_level_list->list.expr, NULL);
        top_level_list = top_level_list->list.next;
    }

    return program;
}

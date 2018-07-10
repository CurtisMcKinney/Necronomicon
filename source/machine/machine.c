/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine.h"
#include <ctype.h>
#include "machine_prim.h"

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
void             necro_core_to_machine_1_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
void             necro_core_to_machine_2_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
NecroMachineAST* necro_core_to_machine_3_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer);
void             necro_program_add_global(NecroMachineProgram* program, NecroMachineAST* global);
void             necro_program_add_struct(NecroMachineProgram* program, NecroMachineAST* struct_ast);
void             necro_program_add_function(NecroMachineProgram* program, NecroMachineAST* function);
void             necro_program_add_machine_def(NecroMachineProgram* program, NecroMachineAST* machine_def);
void             necro_machine_print_ast_go(NecroMachineProgram* program, NecroMachineAST* ast, size_t depth);

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
        var_name = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { var_header, "$", buf });
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
NecroMachineAST* necro_create_machine_value_ast(NecroMachineProgram* program, NecroMachineValue value, NecroMachineType* necro_machine_type)
{
    NecroMachineAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
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
    }, necro_create_machine_uint8_type(&program->arena));
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

NecroMachineAST* necro_create_i64_necro_machine_value(NecroMachineProgram* program, int64_t int64_literal)
{
    return necro_create_machine_value_ast(program, (NecroMachineValue)
    {
        .int64_literal   = int64_literal,
        .value_type      = NECRO_MACHINE_VALUE_INT64_LITERAL,
    }, necro_create_machine_int64_type(&program->arena));
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

NecroMachineAST* necro_create_machine_load_from_ptr(NecroMachineProgram* program, NecroMachineAST* source_ptr_ast, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_MACHINE_VALUE);
    NecroMachineValue source_ptr = source_ptr_ast->value;
    assert(source_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_MACHINE_VALUE_REG || source_ptr.value_type == NECRO_MACHINE_VALUE_PARAM);
    NecroMachineAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type            = NECRO_MACHINE_LOAD;
    ast->load.load_type  = NECRO_LOAD_PTR;
    ast->load.source_ptr = source_ptr_ast;
    ast->load.dest_value = necro_create_reg(program, source_ptr_ast->necro_machine_type->ptr_type.element_type, dest_name);
    ast->necro_machine_type = source_ptr_ast->necro_machine_type->ptr_type.element_type;
    return ast;
}

NecroMachineAST* necro_create_machine_load_tag(NecroMachineProgram* program, NecroMachineAST* source_ptr_ast, const char* dest_name)
{
    assert(source_ptr_ast->type == NECRO_MACHINE_VALUE);
    NecroMachineValue source_ptr = source_ptr_ast->value;
    assert(source_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_MACHINE_VALUE_REG || source_ptr.value_type == NECRO_MACHINE_VALUE_PARAM);
    NecroMachineAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type            = NECRO_MACHINE_LOAD;
    ast->load.load_type  = NECRO_LOAD_TAG;
    ast->load.source_ptr = source_ptr_ast;
    ast->load.dest_value = necro_create_reg(program, necro_create_machine_uint32_type(&program->arena), dest_name);
    ast->necro_machine_type = ast->load.dest_value->necro_machine_type;
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
    ast->necro_machine_type            = source_ptr_ast->necro_machine_type->ptr_type.element_type->struct_type.members[source_slot];
    ast->load.dest_value            = necro_create_reg(program, ast->necro_machine_type, dest_name);
    return ast;
}

// NecroMachineAST* necro_create_machine_load_from_global(NecroMachineProgram* program, NecroMachineAST* source_global_ast, NecroMachineType* necro_machine_type, const char* dest_name)
// {
//     assert(necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
//     // assert(source_ptr.value_type == NECRO_MACHINE_VALUE_REG || source_ptr.value_type == NECRO_MACHINE_VALUE_PARAM);
//     NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
//     ast->type               = NECRO_MACHINE_LOAD;
//     ast->load.load_type     = NECRO_LOAD_GLOBAL;
//     ast->load.source_global = source_global_ast;
//     ast->load.dest_value    = necro_create_reg(program, necro_machine_type->ptr_type.element_type, dest_name);
//     ast->necro_machine_type    = necro_machine_type->ptr_type.element_type;
//     return ast;
// }

NecroMachineAST* necro_create_machine_store_into_ptr(NecroMachineProgram* program, NecroMachineAST* source_value_ast, NecroMachineAST* dest_ptr_ast)
{
    assert(source_value_ast->type == NECRO_MACHINE_VALUE);
    assert(dest_ptr_ast->type == NECRO_MACHINE_VALUE);
    NecroMachineValue dest_ptr     = dest_ptr_ast->value;
    assert(dest_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_MACHINE_VALUE_REG || dest_ptr.value_type == NECRO_MACHINE_VALUE_PARAM);
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type               = NECRO_MACHINE_STORE;
    ast->store.store_type   = NECRO_STORE_PTR;
    ast->store.source_value = source_value_ast;
    ast->store.dest_ptr     = dest_ptr_ast;
    ast->necro_machine_type    = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_store_into_tag(NecroMachineProgram* program, NecroMachineAST* source_value_ast, NecroMachineAST* dest_ptr_ast)
{
    assert(source_value_ast->type == NECRO_MACHINE_VALUE);
    assert(dest_ptr_ast->type == NECRO_MACHINE_VALUE);
    NecroMachineValue dest_ptr     = dest_ptr_ast->value;
    assert(dest_ptr_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_MACHINE_VALUE_REG || dest_ptr.value_type == NECRO_MACHINE_VALUE_PARAM);
    assert(source_value_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_UINT32);
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type               = NECRO_MACHINE_STORE;
    ast->store.store_type   = NECRO_STORE_TAG;
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

// NecroMachineAST* necro_create_machine_store_into_global(NecroMachineProgram* program, NecroMachineAST* source_value_ast, NecroMachineAST* dest_global_ast, NecroMachineType* necro_machine_type)
// {
//     assert(source_value_ast->type == NECRO_MACHINE_VALUE);
//     assert(necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
//     necro_type_check(program, source_value_ast->necro_machine_type, necro_machine_type->ptr_type.element_type);
//     NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
//     ast->type               = NECRO_MACHINE_STORE;
//     ast->store.store_type   = NECRO_STORE_GLOBAL;
//     ast->store.source_value = source_value_ast;
//     ast->store.dest_global  = dest_global_ast;
//     ast->necro_machine_type    = NULL;
//     return ast;
// }

NecroMachineAST* necro_create_machine_block(NecroMachineProgram* program, const char* name, NecroMachineAST* next_block)
{
    NecroMachineAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                  = NECRO_MACHINE_BLOCK;
    ast->block.name            = necro_gen_var(program, NULL, name, NECRO_NAME_UNIQUE);
    ast->block.statements      = necro_paged_arena_alloc(&program->arena, 4 * sizeof(NecroMachineAST*));
    ast->block.num_statements  = 0;
    ast->block.statements_size = 4;
    ast->block.terminator      = NULL;
    ast->block.next_block      = next_block;
    ast->necro_machine_type    = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_call(NecroMachineProgram* program, NecroMachineAST* fn_value_ast, NecroMachineAST** a_parameters, size_t num_parameters, const char* dest_name)
{
    // type_check
    assert(fn_value_ast->type == NECRO_MACHINE_VALUE);
    assert(fn_value_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_FN);
    assert(fn_value_ast->necro_machine_type->fn_type.num_parameters == num_parameters);
    for (size_t i = 0; i < num_parameters; i++)
    {
        necro_type_check(program, fn_value_ast->necro_machine_type->fn_type.parameters[i], a_parameters[i]->necro_machine_type);
    }
    NecroMachineAST* ast          = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                  = NECRO_MACHINE_CALL;
    ast->call.fn_value         = fn_value_ast;
    NecroMachineAST** parameters = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroMachineAST*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachineAST*));
    ast->call.parameters       = parameters;
    ast->call.num_parameters   = num_parameters;
    ast->call.result_reg       = necro_create_reg(program, fn_value_ast->necro_machine_type->fn_type.return_type, dest_name);
    ast->necro_machine_type       = fn_value_ast->necro_machine_type->fn_type.return_type;
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
    NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_FN_DEF;
    ast->fn_def.name      = name;
    ast->fn_def.call_body = call_body;
    ast->fn_def.fn_type   = NECRO_FN_FN;
    ast->fn_def.fn_value  = necro_create_global_value(program, name, necro_machine_type);
    ast->necro_machine_type  = necro_machine_type;
    assert(call_body->type == NECRO_MACHINE_BLOCK);
    necro_symtable_get(program->symtable, name.id)->necro_machine_ast = ast;
    necro_program_add_function(program, ast);
    ast->fn_def._curr_block = call_body;
    ast->fn_def._init_block = NULL;
    ast->fn_def._cont_block = NULL;
    return ast;
}

NecroMachineAST* necro_create_machine_runtime_fn(NecroMachineProgram* program, NecroVar name, NecroMachineType* necro_machine_type)
{
    NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_FN_DEF;
    ast->fn_def.name      = name;
    ast->fn_def.fn_type   = NECRO_FN_RUNTIME;
    ast->fn_def.call_body = NULL;
    ast->necro_machine_type  = necro_machine_type;
    necro_symtable_get(program->symtable, name.id)->necro_machine_ast = ast;
    return ast;
}

NecroMachineAST* necro_create_machine_initial_machine_def(NecroMachineProgram* program, NecroVar bind_name, NecroMachineAST* outer, NecroMachineType* value_type)
{
    NecroArenaSnapshot snapshot          = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineAST* ast                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                            = NECRO_MACHINE_DEF;
    ast->machine_def.bind_name              = bind_name;
    char itoabuf[10];
    char* machine_name                      = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { necro_intern_get_string(program->intern, bind_name.symbol), "Machine#", itoa(bind_name.id.id, itoabuf, 10) });
    machine_name[0]                         = toupper(machine_name[0]);
    ast->machine_def.machine_name              = necro_gen_var(program, ast, machine_name, NECRO_NAME_UNIQUE);
    ast->machine_def.arg_names              = NULL;
    ast->machine_def.num_arg_names          = 0;
    ast->machine_def.mk_fn                  = NULL;
    ast->machine_def.init_fn                = NULL;
    ast->machine_def.update_fn              = NULL;
    ast->machine_def.global_value           = NULL;
    ast->machine_def.update_error_block     = NULL;
    ast->machine_def.initial_tag            = 0;
    ast->machine_def.state_type             = NECRO_STATE_STATEFUL;
    ast->machine_def.is_recursive           = false;
    ast->machine_def.is_pushed              = false;
    ast->machine_def.is_persistent_slot_set = false;
    ast->machine_def.outer                  = outer;
    ast->machine_def.default_init           = true;
    ast->machine_def.default_mk             = true;
    ast->necro_machine_type                 = NULL; // TODO: Machine type will be constructed after analysis!
    if (value_type->type == NECRO_MACHINE_TYPE_FN)
    {
        ast->machine_def.value_type = value_type->fn_type.return_type;
        ast->machine_def.fn_type    = value_type;
    }
    else
    {
        ast->machine_def.value_type = necro_create_machine_ptr_type(&program->arena, value_type);
        ast->machine_def.fn_type    = NULL;
    }
    const size_t initial_members_size    = 4;
    ast->machine_def.members                = necro_paged_arena_alloc(&program->arena, initial_members_size * sizeof(NecroSlot));
    ast->machine_def.num_members            = 0;
    ast->machine_def.members_size           = initial_members_size;
    necro_symtable_get(program->symtable, bind_name.id)->necro_machine_ast = ast;
    necro_symtable_get(program->symtable, ast->machine_def.machine_name.id)->necro_machine_ast = ast;
    if (outer == 0)
        necro_program_add_machine_def(program, ast);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return ast;
}

// // Assumes zero-initialized
// NecroMachineAST* necro_create_machine_constant_struct(NecroMachineProgram* program, NecroMachineType* necro_machine_type)
// {
//     assert(necro_machine_type->type == NECRO_MACHINE_TYPE_STRUCT);
//     NecroMachineAST* ast                  = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
//     ast->type                          = NECRO_MACHINE_CONSTANT_DEF;
//     ast->necro_machine_type               = necro_machine_type;
//     ast->constant.constant_type        = NECRO_CONSTANT_STRUCT;
//     ast->constant.constant_struct.name = necro_machine_type->struct_type.name;
//     return ast;
// }

NecroMachineAST* necro_create_machine_gep(NecroMachineProgram* program, NecroMachineAST* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(source_value->type == NECRO_MACHINE_VALUE);
    NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_GEP;
    ast->gep.source_value = source_value;
    int32_t* indices      = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(int32_t));
    memcpy(indices, a_indices, num_indices * sizeof(int32_t));
    ast->gep.indices      = indices;
    ast->gep.num_indices  = num_indices;
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
            assert(a_indices[i] == 0); // NOTE: The machine abstract machine never directly works with contiguous arrays of data. Thus all pointers should only ever be indexed from 0!
            assert(i == 0);            // NOTE:Can only deref the first type!
            necro_machine_type = necro_machine_type->ptr_type.element_type;
        }
        else
        {
            assert(necro_machine_type->type == NECRO_MACHINE_TYPE_STRUCT || necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
        }
    }
    necro_machine_type      = necro_create_machine_ptr_type(&program->arena, necro_machine_type);
    ast->gep.dest_value  = necro_create_reg(program, necro_machine_type, dest_name);
    ast->necro_machine_type = necro_machine_type;
    return ast;
}

NecroMachineAST* necro_create_bit_cast(NecroMachineProgram* program, NecroMachineAST* from_value_ast, NecroMachineType* to_type)
{
    assert(from_value_ast->necro_machine_type->type == NECRO_MACHINE_TYPE_PTR);
    assert(to_type->type == NECRO_MACHINE_TYPE_PTR);
    NecroMachineValue from_value = from_value_ast->value;
    NecroMachineAST* ast         = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                 = NECRO_MACHINE_BIT_CAST;
    ast->bit_cast.from_value  = from_value_ast;
    switch (from_value.value_type)
    {
    case NECRO_MACHINE_VALUE_REG:
    case NECRO_MACHINE_VALUE_PARAM:
    case NECRO_MACHINE_VALUE_GLOBAL:
    case NECRO_MACHINE_VALUE_NULL_PTR_LITERAL:
        ast->bit_cast.to_value = necro_create_reg(program, to_type, "cst");
        break;
    case NECRO_MACHINE_VALUE_UINT1_LITERAL:
    case NECRO_MACHINE_VALUE_UINT8_LITERAL:
    case NECRO_MACHINE_VALUE_UINT16_LITERAL:
    case NECRO_MACHINE_VALUE_UINT32_LITERAL:
    case NECRO_MACHINE_VALUE_UINT64_LITERAL:
    case NECRO_MACHINE_VALUE_INT64_LITERAL:
        assert(false && "Cannot bit cast int literal values!");
        break;
    }
    ast->necro_machine_type = to_type;
    return ast;
}

NecroMachineAST* necro_create_nalloc(NecroMachineProgram* program, NecroMachineType* type, uint16_t slots_used)
{
    NecroMachineAST* ast         = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type                 = NECRO_MACHINE_NALLOC;
    ast->nalloc.type_to_alloc = type;
    ast->nalloc.slots_used    = slots_used;
    NecroMachineType* type_ptr   = necro_create_machine_ptr_type(&program->arena, type);
    ast->nalloc.result_reg    = necro_create_reg(program, type_ptr, "data_ptr");
    ast->necro_machine_type      = type_ptr;
    return ast;
}

NecroMachineAST* necro_create_binop(NecroMachineProgram* program, NecroMachineAST* left, NecroMachineAST* right, NECRO_MACHINE_BINOP_TYPE op_type)
{
    NecroMachineAST* ast = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type         = NECRO_MACHINE_BINOP;
    assert(left->type == NECRO_MACHINE_VALUE);
    assert(right->type == NECRO_MACHINE_VALUE);
    // typecheck
    switch (op_type)
    {
    case NECRO_MACHINE_BINOP_IADD: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_ISUB: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_IMUL: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_IDIV: /* FALL THROUGH */
    {
        NecroMachineType* int_type = necro_create_machine_int64_type(&program->arena);
        necro_type_check(program, left->necro_machine_type, int_type);
        necro_type_check(program, right->necro_machine_type, int_type);
        ast->necro_machine_type = int_type;
        ast->binop.result      = necro_create_reg(program, ast->necro_machine_type, "iop");
        break;
    }
    case NECRO_MACHINE_BINOP_FADD: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_FSUB: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_FMUL: /* FALL THROUGH */
    case NECRO_MACHINE_BINOP_FDIV: /* FALL THROUGH */
    {
        NecroMachineType* float_type = necro_create_machine_f64_type(&program->arena);
        necro_type_check(program, left->necro_machine_type, float_type);
        necro_type_check(program, right->necro_machine_type, float_type);
        ast->necro_machine_type = float_type;
        ast->binop.result      = necro_create_reg(program, ast->necro_machine_type, "fop");
        break;
    }
    }
    ast->binop.binop_type = op_type;
    ast->binop.left       = left;
    ast->binop.right      = right;
    return ast;
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
    NecroMachineAST* block = necro_create_machine_block(program, block_name, NULL);
    // if (fn_def->fn_def.call_body == NULL)
    // {
    //     fn_def->fn_def.call_body   = block;
    //     fn_def->fn_def._curr_block = block;
    //     return block;
    // }
    NecroMachineAST* fn_block = fn_def->fn_def.call_body;
    while (fn_block->block.next_block != NULL)
    {
        fn_block = fn_block->block.next_block;
    }
    fn_block->block.next_block = block;
    // fn_def->fn_def._curr_block = block;
    return block;
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

NecroMachineAST* necro_build_nalloc(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineType* type, uint16_t a_slots_used)
{
    assert(program != NULL);
    assert(type != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* data_ptr = necro_create_nalloc(program, type, a_slots_used);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, data_ptr);
    assert(data_ptr->nalloc.result_reg->type == NECRO_MACHINE_VALUE);
    assert(data_ptr->nalloc.result_reg->value.value_type == NECRO_MACHINE_VALUE_REG);
    return data_ptr->nalloc.result_reg;
}

void necro_build_store_into_tag(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_ptr)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* store_ast = necro_create_machine_store_into_tag(program, source_value, dest_ptr);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, store_ast);
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

// void necro_build_store_into_global(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, NecroMachineAST* dest_global, NecroMachineType* necro_machine_type)
// {
//     assert(program != NULL);
//     assert(fn_def != NULL);
//     assert(fn_def->type == NECRO_MACHINE_FN_DEF);
//     necro_add_statement_to_block(program, fn_def->fn_def._curr_block, necro_create_machine_store_into_global(program, source_value, dest_global, necro_machine_type));
// }

NecroMachineAST* necro_build_bit_cast(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* value, NecroMachineType* to_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast = necro_create_bit_cast(program, value, to_type);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->bit_cast.to_value->type == NECRO_MACHINE_VALUE);
    assert(ast->bit_cast.to_value->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->bit_cast.to_value;
}

NecroMachineAST* necro_build_gep(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast = necro_create_machine_gep(program, source_value, a_indices, num_indices, dest_name);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->gep.dest_value->type == NECRO_MACHINE_VALUE);
    assert(ast->gep.dest_value->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->gep.dest_value;
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

NecroMachineAST* necro_build_call(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* fn_to_call_value, NecroMachineAST** a_parameters, size_t num_parameters, const char* dest_name_header)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast = necro_create_machine_call(program, fn_to_call_value, a_parameters, num_parameters, dest_name_header);
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    assert(ast->call.result_reg->type == NECRO_MACHINE_VALUE);
    assert(ast->call.result_reg->value.value_type == NECRO_MACHINE_VALUE_REG);
    return ast->call.result_reg;
}

NecroMachineAST* necro_build_binop(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* left, NecroMachineAST* right, NECRO_MACHINE_BINOP_TYPE op_type)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    NecroMachineAST* ast = necro_create_binop(program, left, right, op_type);
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
    fn_def->fn_def._curr_block->block.terminator                                 = necro_paged_arena_alloc(&program->arena, sizeof(NecroTerminator));
    fn_def->fn_def._curr_block->block.terminator->type                           = NECRO_TERM_RETURN;
    fn_def->fn_def._curr_block->block.terminator->return_terminator.return_value = return_value;
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
           left->type == NECRO_MACHINE_TYPE_INT64  ||
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

NecroMachineAST* necro_build_phi(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST** a_blocks, NecroMachineAST** a_values, size_t num_values)
{
    assert(program != NULL);
    assert(fn_def != NULL);
    assert(fn_def->type == NECRO_MACHINE_FN_DEF);
    assert(fn_def->fn_def._curr_block->block.num_statements == 0 ||
           fn_def->fn_def._curr_block->block.statements[fn_def->fn_def._curr_block->block.num_statements - 1]->type == NECRO_MACHINE_PHI); // Verify at beginning of block or preceded by a phi machine
    assert(a_blocks != NULL);
    assert(a_values != NULL);
    assert(num_values > 0);
    NecroMachineAST** blocks          = necro_paged_arena_alloc(&program->arena, num_values * sizeof(NecroMachineAST*));
    NecroMachineAST** values          = necro_paged_arena_alloc(&program->arena, num_values * sizeof(NecroMachineAST*));
    NecroMachineType* necro_machine_type = NULL;
    for (size_t i = 0; i < num_values; ++i)
    {
        assert(a_blocks[i] != NULL);
        assert(a_blocks[i]->type == NECRO_MACHINE_BLOCK);
        assert(a_values[i] != NULL);
        assert(a_values[i]->type == NECRO_MACHINE_VALUE);
        assert(a_values[i]->value.value_type == NECRO_MACHINE_VALUE_REG);
        if (i == 0)
        {
            necro_machine_type = a_values[i]->necro_machine_type;
        }
        else
        {
            necro_type_check(program, necro_machine_type, a_values[i]->necro_machine_type);
        }
        blocks[i] = a_blocks[i];
        values[i] = a_values[i];
    }
    NecroMachineAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
    ast->type             = NECRO_MACHINE_PHI;
    ast->phi.blocks       = blocks;
    ast->phi.values       = values;
    ast->phi.num_values   = num_values;
    ast->phi.result       = necro_create_reg(program, necro_machine_type, "phi");
    necro_add_statement_to_block(program, fn_def->fn_def._curr_block, ast);
    return ast->phi.result;
}

///////////////////////////////////////////////////////
// NecroMachineProgram
///////////////////////////////////////////////////////
NecroMachineProgram necro_create_initial_machine_program(NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types)
{
    NecroMachineProgram program =
    {
        .arena           = necro_create_paged_arena(),
        .snapshot_arena  = necro_create_snapshot_arena(),
        .structs         = necro_create_necro_machine_ast_vector(),
        .globals         = necro_create_necro_machine_ast_vector(),
        .functions       = necro_create_necro_machine_ast_vector(),
        .machine_defs       = necro_create_necro_machine_ast_vector(),
        .main            = NULL,
        .intern          = intern,
        .symtable        = symtable,
        .scoped_symtable = scoped_symtable,
        .prim_types      = prim_types,
        .gen_vars        = 0,
    };
    necro_init_machine_prim(&program);
    return program;
}

void necro_destroy_machine_program(NecroMachineProgram* program)
{
    necro_destroy_paged_arena(&program->arena);
    necro_destroy_snapshot_arena(&program->snapshot_arena);
    necro_destroy_necro_machine_ast_vector(&program->structs);
    necro_destroy_necro_machine_ast_vector(&program->globals);
    necro_destroy_necro_machine_ast_vector(&program->functions);
    necro_destroy_necro_machine_ast_vector(&program->machine_defs);
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
// Core to Machine Pass 1
///////////////////////////////////////////////////////
void necro_core_to_machine_1_data_con(NecroMachineProgram* program, NecroCoreAST_DataCon* con, NecroMachineAST* struct_type, size_t max_arg_count, size_t con_number)
{
    assert(program != NULL);
    assert(con != NULL);
    assert(struct_type != NULL);
    assert(struct_type->type = NECRO_MACHINE_STRUCT_DEF);
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
    char*  con_name  = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "mk", necro_intern_get_string(program->intern, con->condid.symbol), "#" });
    NecroVar con_var = necro_gen_var(program, NULL, con_name, NECRO_NAME_UNIQUE);
    size_t arg_count = necro_count_data_con_args(con);
    assert(arg_count <= max_arg_count);
    NecroMachineType**          parameters = necro_snapshot_arena_alloc(&program->snapshot_arena, arg_count * sizeof(NecroType*));
    NecroCoreAST_Expression* arg        = con->arg_list;
    for (size_t i = 0; i < arg_count; ++i)
    {
        assert(arg->expr_type == NECRO_CORE_EXPR_LIST);
        NecroMachineType* arg_type = necro_core_ast_to_machine_type(program, arg->list.expr);
        parameters[i] = necro_create_machine_ptr_type(&program->arena, arg_type);
        arg = arg->list.next;
    }
    NecroMachineType*  struct_ptr_type = necro_create_machine_ptr_type(&program->arena, struct_type->necro_machine_type);
    NecroMachineType*  mk_fn_type      = necro_create_machine_fn_type(&program->arena, struct_ptr_type, parameters, arg_count);
    NecroMachineAST*   mk_fn_body      = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*   mk_fn_def       = necro_create_machine_fn(program, con_var, mk_fn_body, mk_fn_type);
    NecroMachineAST*   data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type->necro_machine_type, (uint16_t) arg_count);
    necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_machine_value(program, con_number), data_ptr);

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
            necro_build_store_into_slot(program, mk_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), data_ptr, i + 1);
        }
    }

    necro_symtable_get(program->symtable, con->condid.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
    necro_build_return(program, mk_fn_def, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

void necro_core_to_machine_1_data_decl(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast)
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
    bool            is_sum_type = core_ast->data_decl.con_list->next != NULL;
    NecroMachineType** members     = necro_snapshot_arena_alloc(&program->snapshot_arena, max_arg_count * sizeof(NecroMachineType*));
    members[0] = program->necro_data_type;
    NecroCoreAST_Expression* args = core_ast->data_decl.con_list->arg_list;
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    {
        if (is_sum_type)
        {
            members[i] = program->necro_poly_ptr_type;
        }
        else
        {
            members[i] = necro_create_machine_ptr_type(&program->arena, necro_core_ast_to_machine_type(program, args->list.expr));
            args = args->list.next;
        }
    }

    // Struct
    NecroMachineAST* struct_type = necro_create_machine_struct_def(program, core_ast->data_decl.data_id, members, max_arg_count + 1);

    con = core_ast->data_decl.con_list;
    size_t con_number = 0;
    while (con != NULL)
    {
        necro_core_to_machine_1_data_con(program, con, struct_type, max_arg_count, con_number);
        con = con->next;
        con_number++;
    }

    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

void necro_core_to_machine_1_bind(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);

    //---------------
    // Create initial MachineDef
    NecroMachineAST* machine_def = necro_create_machine_initial_machine_def(program, core_ast->bind.var, outer, necro_core_ast_to_machine_type(program, core_ast));
    machine_def->machine_def.is_recursive = core_ast->bind.is_recursive;

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
        machine_def->machine_def.num_arg_names = num_args;
        machine_def->machine_def.arg_names     = necro_paged_arena_alloc(&program->arena, num_args * sizeof(NecroVar));
        num_args                         = 0;
        NecroCoreAST_Expression* lambdas = core_ast->bind.expr;
        while (lambdas->expr_type == NECRO_CORE_EXPR_LAM)
        {
            machine_def->machine_def.arg_names[num_args] = lambdas->lambda.arg->var;
            num_args++;
            lambdas = lambdas->lambda.expr;
        }
    }

    //---------------
    // go deeper
    necro_core_to_machine_1_go(program, core_ast->bind.expr, machine_def);

    //---------------
    // declare mk_fn
    if (outer == NULL)
    {
        const char*       mk_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "mk", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) });
        NecroVar          mk_fn_var  = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
        NecroMachineType* mk_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
        NecroMachineAST*  mk_fn_body = necro_create_machine_block(program, "entry", NULL);
        NecroMachineAST*  mk_fn_def  = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
        program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
        machine_def->machine_def.mk_fn  = mk_fn_def;
    }

    // //---------------
    // // declare init_fn
    // {
    //     const char*    init_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "init", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) });
    //     NecroVar       init_fn_var  = necro_gen_var(program, NULL, init_fn_name, NECRO_NAME_UNIQUE);
    //     NecroMachineType* init_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
    //     NecroMachineAST*  init_fn_body = necro_create_machine_block(program, necro_intern_string(program->intern, "entry"), NULL);
    //     NecroMachineAST*  init_fn_def  = necro_create_machine_fn(program, init_fn_var, init_fn_body, init_fn_type);
    //     machine_def->machine_def.init_fn  = init_fn_def;
    // }

}

void necro_core_to_machine_1_let(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_1_go(program, core_ast->let.bind, outer);
    necro_core_to_machine_1_go(program, core_ast->let.expr, outer);
}

void necro_core_to_machine_1_lambda(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_1_go(program, core_ast->lambda.arg, outer);
    necro_core_to_machine_1_go(program, core_ast->lambda.expr, outer);
}

void necro_core_to_machine_1_app(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_1_go(program, core_ast->app.exprA, outer);
    necro_core_to_machine_1_go(program, core_ast->app.exprB, outer);
}

void necro_core_to_machine_1_case(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_1_go(program, core_ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = core_ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_machine_1_go(program, alts->expr, outer);
        alts = alts->next;
    }
}

///////////////////////////////////////////////////////
// Core to Machine Pass 1 Go
///////////////////////////////////////////////////////
void necro_core_to_machine_1_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_DATA_DECL: necro_core_to_machine_1_data_decl(program, core_ast); return;
    case NECRO_CORE_EXPR_DATA_CON:  assert(false && "found NECRO_CORE_EXPR_DATA_CON in necro_core_to_machine_1_go"); return;
    case NECRO_CORE_EXPR_BIND:      necro_core_to_machine_1_bind(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LET:       necro_core_to_machine_1_let(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LAM:       necro_core_to_machine_1_lambda(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_APP:       necro_core_to_machine_1_app(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_CASE:      necro_core_to_machine_1_case(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_VAR:       return;
    case NECRO_CORE_EXPR_LIT:       return;
    case NECRO_CORE_EXPR_LIST:      assert(false); return; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_machine_1_go"); return;
    }
}

///////////////////////////////////////////////////////
// Core to Machine Pass 2
///////////////////////////////////////////////////////
static bool necro_is_var_machine_arg(NecroMachineProgram* program, NecroMachineDef* machine_def, NecroVar var)
{
    for (size_t i = 0; i < machine_def->num_arg_names; i++)
    {
        if (machine_def->arg_names[i].id.id == var.id.id)
            return true;
    }
    return false;
}

NecroSlot necro_add_member(NecroMachineProgram* program, NecroMachineDef* machine_def, NecroMachineType* new_member)
{
    assert(program != NULL);
    assert(machine_def != NULL);
    if (machine_def->num_members + 1 >= machine_def->members_size)
    {
        NecroSlot* old_members   = machine_def->members;
        machine_def->members        = necro_paged_arena_alloc(&program->arena, machine_def->members_size * 2 * sizeof(NecroSlot));
        memcpy(machine_def->members, old_members, machine_def->members_size * sizeof(NecroSlot));
        machine_def->members_size *= 2;
    }
    NecroSlot slot = (NecroSlot) { .necro_machine_type = new_member, .slot_num = machine_def->num_members + ((machine_def->num_arg_names > 0) ? 0 : 2) };
    machine_def->members[machine_def->num_members] = slot;
    machine_def->num_members++;
    return slot;
}

void necro_remove_only_self_recursive_member(NecroMachineProgram* program, NecroMachineAST* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACHINE_DEF);
    if (ast->machine_def.num_arg_names == 0 || ast->machine_def.num_members != 1 || ast->machine_def.members[0].necro_machine_type->ptr_type.element_type != ast->necro_machine_type)
        return;
    // else if (ast->machine_def.num_arg_names > 0 && ast->machine_def.num_members == 1 && ast->machine_def.members[0].necro_machine_type->ptr_type.element_type == ast->necro_machine_type)
        // ast->machine_def.state_type = NECRO_STATE_POINTWISE;
    ast->machine_def.num_members = 0;
}

void necro_calculate_statefulness(NecroMachineProgram* program, NecroMachineAST* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACHINE_DEF);
    if (ast->machine_def.is_recursive && ast->machine_def.num_arg_names == 0)
        ast->machine_def.state_type = NECRO_STATE_STATEFUL;
    else if (ast->machine_def.num_members == 0 && ast->machine_def.num_arg_names == 0)
        ast->machine_def.state_type = NECRO_STATE_CONSTANT;
    else if (ast->machine_def.num_members == 0 && ast->machine_def.num_arg_names > 0)
        ast->machine_def.state_type = NECRO_STATE_POINTWISE;
    else if (ast->machine_def.num_members > 0)
        ast->machine_def.state_type = NECRO_STATE_STATEFUL;
    else
        assert(false);
}

void necro_core_to_machine_2_bind(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);

    // Retrieve machine_def
    NecroMachineAST* machine_def = necro_symtable_get(program->symtable, core_ast->bind.var.id)->necro_machine_ast;
    assert(machine_def != NULL);
    assert(machine_def->type == NECRO_MACHINE_DEF);

    // Go deeper
    if (outer != NULL)
    {
        necro_core_to_machine_2_go(program, core_ast->bind.expr, outer);
        necro_calculate_statefulness(program, machine_def);
        return;
    }

    machine_def->necro_machine_type = necro_create_machine_struct_type(&program->arena, machine_def->machine_def.machine_name, NULL, 0);

    machine_def->machine_def.is_pushed = true;
    necro_core_to_machine_2_go(program, core_ast->bind.expr, machine_def);

    necro_remove_only_self_recursive_member(program, machine_def);
    necro_calculate_statefulness(program, machine_def);
    machine_def->machine_def.is_pushed = false;

    const bool   is_machine_fn    = machine_def->machine_def.num_arg_names > 0;
    const size_t member_offset = is_machine_fn ? 0 : 2;

    // Calculate machine type
    const size_t    num_machine_members  = (member_offset + machine_def->machine_def.num_members);
    NecroMachineType** machine_type_members = necro_paged_arena_alloc(&program->arena, num_machine_members * sizeof(NecroMachineType*));
    if (!is_machine_fn)
    {
        machine_type_members[0] = program->necro_data_type;
        machine_type_members[1] = machine_def->machine_def.value_type;
    }
    for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
        machine_type_members[i + member_offset] = machine_def->machine_def.members[i].necro_machine_type;
    machine_def->necro_machine_type->struct_type.members     = machine_type_members;
    machine_def->necro_machine_type->struct_type.num_members = num_machine_members;

    // add_global
    if (outer == NULL && !is_machine_fn && (machine_def->machine_def.state_type == NECRO_STATE_CONSTANT || machine_def->machine_def.state_type == NECRO_STATE_STATEFUL))
    {
        NecroMachineAST* global_value      = necro_create_global_value(program, machine_def->machine_def.bind_name, necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type));
        machine_def->machine_def.global_value = global_value;
        necro_program_add_global(program, global_value);
    }

    if (machine_def->machine_def.state_type == NECRO_STATE_STATIC || machine_def->machine_def.state_type == NECRO_STATE_POINTWISE || (is_machine_fn && !machine_def->machine_def.is_recursive))
    {
        machine_def->machine_def.mk_fn = NULL;
    }

    //--------------------
    // Define mk_fn
    if (machine_def->machine_def.mk_fn != NULL)
    {
        machine_def->machine_def.mk_fn->necro_machine_type->fn_type.return_type = necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type);
        NecroMachineAST* data_ptr = necro_build_nalloc(program, machine_def->machine_def.mk_fn, machine_def->necro_machine_type, (uint16_t) (machine_def->machine_def.num_members + member_offset));
        if (!is_machine_fn)
        {
            necro_build_store_into_tag(program, machine_def->machine_def.mk_fn, necro_create_uint32_necro_machine_value(program, 0), data_ptr);
            // necro_build_store_into_slot(program, machine_def->machine_def.mk_fn, necro_create_null_necro_machine_value(program, machine_def->machine_def.value_type), data_ptr, 1);
        }
        // NULL out members via nalloc and something smarter!!!
        // for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
        // {
        //     if (machine_def->machine_def.members[i].necro_machine_type->type == NECRO_MACHINE_TYPE_PTR)
        //         necro_build_store_into_slot(program, machine_def->machine_def.mk_fn, necro_create_null_necro_machine_value(program, machine_def->machine_def.members[i].necro_machine_type), data_ptr, i + member_offset);
        // }
        necro_build_return(program, machine_def->machine_def.mk_fn, data_ptr);
    }

}

void necro_core_to_machine_2_let(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_2_go(program, core_ast->let.bind, outer);
    necro_core_to_machine_2_go(program, core_ast->let.expr, outer);
}

void necro_core_to_machine_2_lambda(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_2_go(program, core_ast->lambda.arg, outer);
    necro_core_to_machine_2_go(program, core_ast->lambda.expr, outer);
}

void necro_core_to_machine_2_case(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_2_go(program, core_ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = core_ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_machine_2_go(program, alts->expr, outer);
        alts = alts->next;
    }
}

void necro_core_to_machine_2_app(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);

    NecroCoreAST_Expression*  function        = core_ast;
    size_t                    arg_count       = 0;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        necro_core_to_machine_2_go(program, function->app.exprB, outer);
        arg_count++;
        function = function->app.exprA;
    }
    assert(function->expr_type == NECRO_CORE_EXPR_VAR);

    NecroMachineAST*  fn_value      = necro_symtable_get(program->symtable, function->var.id)->necro_machine_ast;
    bool           is_persistent = false;
    NecroMachineAST*  fn_def        = NULL;
    NecroMachineType* fn_type       = NULL;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_MACHINE_DEF)
    {
        fn_def        = fn_value;
        is_persistent = fn_value->machine_def.state_type == NECRO_STATE_STATEFUL;
        fn_type       = fn_value->machine_def.fn_type;
    }
    else
    {
        fn_type = fn_value->necro_machine_type;
    }
    assert(fn_type->type == NECRO_MACHINE_TYPE_FN);
    assert(fn_type->fn_type.num_parameters == arg_count);
    if (!is_persistent)
        return;
    bool is_dynamic = fn_def->machine_def.is_pushed;
    if (!is_dynamic)
    {
        // // NecroSlot slot = necro_add_member(program, &outer->machine_def, program->necro_data_type);
        // // for (size_t i = 1; i < fn_def->necro_machine_type->struct_type.num_members; ++i)
        // for (size_t i = 0; i < fn_def->necro_machine_type->struct_type.num_members; ++i)
        // {
        //     if (i == 0)
        //         core_ast->app.persistent_slot = necro_add_member(program, &outer->machine_def, fn_def->necro_machine_type->struct_type.members[i]).slot_num;
        //     else
        //         necro_add_member(program, &outer->machine_def, fn_def->necro_machine_type->struct_type.members[i]);
        // }
        // // slot.slot_num;
        core_ast->app.persistent_slot = necro_add_member(program, &outer->machine_def, fn_def->necro_machine_type).slot_num;
    }
    else
    {
        fn_def->machine_def.is_recursive = true;
        core_ast->app.persistent_slot = necro_add_member(program, &outer->machine_def, necro_create_machine_ptr_type(&program->arena, fn_def->necro_machine_type)).slot_num;
    }
}

void necro_core_to_machine_2_var(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    NecroMachineAST* machine_ast = necro_symtable_get(program->symtable, core_ast->var.id)->necro_machine_ast;
    if (machine_ast == NULL)
    {
        // Lambda args are NULL at first...better way to differentiate!?
        // assert(false);
        return;
    }
    if (machine_ast->type != NECRO_MACHINE_DEF)
        return;
    if (machine_ast->machine_def.state_type != NECRO_STATE_STATEFUL)
        return;
    if (necro_is_var_machine_arg(program, &outer->machine_def, core_ast->var))
        return;
    if (machine_ast->machine_def.outer == NULL)
        return;
    // if (necro_symtable_get(program->symtable, core_ast->var.id)->persistent_slot != 0)
    //     return;
    if (machine_ast->machine_def.is_persistent_slot_set)
        return;
    machine_ast->machine_def.is_persistent_slot_set = true;
    NecroSlot slot = necro_add_member(program, &outer->machine_def, machine_ast->machine_def.value_type);
    necro_symtable_get(program->symtable, core_ast->var.id)->persistent_slot = slot.slot_num;
}

///////////////////////////////////////////////////////
// Core to Machine Pass 2 Go
///////////////////////////////////////////////////////
void necro_core_to_machine_2_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_BIND:      necro_core_to_machine_2_bind(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LET:       necro_core_to_machine_2_let(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_LAM:       necro_core_to_machine_2_lambda(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_APP:       necro_core_to_machine_2_app(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_CASE:      necro_core_to_machine_2_case(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_VAR:       necro_core_to_machine_2_var(program, core_ast, outer); return;
    case NECRO_CORE_EXPR_DATA_DECL: return;
    case NECRO_CORE_EXPR_DATA_CON:  return;
    case NECRO_CORE_EXPR_LIT:       return;
    case NECRO_CORE_EXPR_LIST:      assert(false); return; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_machine_2_go"); return;
    }
}

///////////////////////////////////////////////////////
// Core to Machine Pass 3
///////////////////////////////////////////////////////
NecroMachineAST* necro_core_to_machine_3_bind(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);

    // Retrieve machine_def
    NecroSymbolInfo* info     = necro_symtable_get(program->symtable, core_ast->bind.var.id);
    NecroMachineAST*    machine_def = info->necro_machine_ast;
    assert(machine_def != NULL);
    assert(machine_def->type == NECRO_MACHINE_DEF);

    if (outer != NULL)
    {
        NecroMachineAST* return_value = necro_core_to_machine_3_go(program, core_ast->bind.expr, outer);
        // if (info->persistent_slot != 0)
        if (machine_def->machine_def.is_persistent_slot_set)
        {
            NecroMachineAST* param0 = necro_create_param_reg(program, outer->machine_def.update_fn, 0);
            necro_build_store_into_slot(program, machine_def->machine_def.outer->machine_def.update_fn, return_value, param0, info->persistent_slot);
        }
        else
        {
            info->necro_machine_ast = return_value;
        }
        return return_value;
    }
    if (machine_def->machine_def.state_type == NECRO_STATE_STATIC)
    {
        return necro_core_to_machine_3_go(program, core_ast->bind.expr, outer);
    }

    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);

    // Start function
    bool            is_stateful       = machine_def->machine_def.state_type == NECRO_STATE_STATEFUL;
    const char*     update_name       = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "update", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) });
    NecroVar        update_var        = necro_gen_var(program, NULL, update_name, NECRO_NAME_UNIQUE);
    size_t          num_update_params = machine_def->machine_def.num_arg_names;
    if (num_update_params > 0)
        assert(machine_def->machine_def.num_arg_names == machine_def->machine_def.fn_type->fn_type.num_parameters);
    if (is_stateful)
        num_update_params++;
    NecroMachineType** update_params     = necro_snapshot_arena_alloc(&program->snapshot_arena, num_update_params * sizeof(NecroMachineType*));
    if (is_stateful)
    {
        update_params[0] = necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type);
    }
    for (size_t i = 0; i < machine_def->machine_def.num_arg_names; ++i)
    {
        update_params[i + (is_stateful ? 1 : 0)] = machine_def->machine_def.fn_type->fn_type.parameters[i];
    }
    NecroMachineType*  update_fn_type  = necro_create_machine_fn_type(&program->arena, machine_def->machine_def.value_type, update_params, num_update_params);
    NecroMachineAST*   update_fn_body  = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*   update_fn_def   = necro_create_machine_fn(program, update_var, update_fn_body, update_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
    machine_def->machine_def.update_fn = update_fn_def;

    for (size_t i = 0; i < machine_def->machine_def.num_arg_names; ++i)
    {
        NecroMachineAST* param_ast = necro_create_param_reg(program, update_fn_def, i + (is_stateful ? 1 : 0));
        necro_symtable_get(program->symtable, machine_def->machine_def.arg_names[i].id)->necro_machine_ast = param_ast;
    }

    // Go deeper
    NecroMachineAST* result = necro_core_to_machine_3_go(program, core_ast->bind.expr, machine_def);

    // store if stateful
    if (machine_def->machine_def.state_type == NECRO_STATE_STATEFUL && machine_def->machine_def.global_value != NULL && machine_def->machine_def.num_arg_names == 0)
        necro_build_store_into_slot(program, update_fn_def, result, necro_create_param_reg(program, update_fn_def, 0), 1);

    // Finish function
    necro_build_return(program, update_fn_def, result);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return NULL;
}

NecroMachineAST* necro_core_to_machine_3_let(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LET);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_3_go(program, core_ast->let.bind, outer);
    return necro_core_to_machine_3_go(program, core_ast->let.expr, outer);
}

NecroMachineAST* necro_core_to_machine_3_lambda(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LAM);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    // necro_core_to_machine_3_go(program, core_ast->lambda.arg, outer);
    return necro_core_to_machine_3_go(program, core_ast->lambda.expr, outer);
}

NecroMachineAST* necro_core_to_machine_3_case(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_CASE);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    necro_core_to_machine_3_go(program, core_ast->case_expr.expr, outer);
    NecroCoreAST_CaseAlt* alts = core_ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_to_machine_3_go(program, alts->expr, outer);
        alts = alts->next;
    }
    return NULL;
}

NecroMachineAST* necro_core_to_machine_3_app(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);

    NecroCoreAST_Expression*  function        = core_ast;
    size_t                    arg_count       = 0;
    size_t                    persistent_slot = core_ast->app.persistent_slot;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    assert(function->expr_type == NECRO_CORE_EXPR_VAR);

    NecroMachineAST* fn_value    = necro_symtable_get(program->symtable, function->var.id)->necro_machine_ast;
    NecroMachineAST* machine_def    = NULL;
    bool          is_stateful = false;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_MACHINE_DEF)
    {
        machine_def    = fn_value;
        fn_value    = fn_value->machine_def.update_fn->fn_def.fn_value;
        is_stateful = machine_def->machine_def.state_type == NECRO_STATE_STATEFUL;
    }
    else if (fn_value->type == NECRO_MACHINE_FN_DEF)
    {
        fn_value = fn_value->fn_def.fn_value;
    }
    assert(fn_value->type == NECRO_MACHINE_VALUE);
    assert(fn_value->necro_machine_type->type == NECRO_MACHINE_TYPE_FN);

    if (is_stateful)
        arg_count++;

    assert(fn_value->necro_machine_type->fn_type.num_parameters == arg_count);

    NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineAST**     args      = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroMachineAST*));
    size_t             arg_index = arg_count - 1;
    function                     = core_ast;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_core_to_machine_3_go(program, function->app.exprB, outer);
        // args[arg_index] = necro_maybe_cast(codegen, args[arg_index], LLVMTypeOf(params[arg_index]));
        arg_index--;
        function        = function->app.exprA;
    }

    necro_rewind_arena(&program->snapshot_arena, snapshot);
    // Pass in persistent var
    if (is_stateful)
    {
        assert(machine_def != NULL);
        bool is_dynamic = machine_def->machine_def.is_recursive && outer == machine_def;
        if (!is_dynamic)
        {
            NecroMachineAST* gep_value  = necro_build_gep(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), (uint32_t[]) { 0, persistent_slot }, 2, "gep");
            //NecroMachineAST* cast_value = necro_build_bit_cast(program, outer->machine_def.update_fn, gep_value, necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type));
            // args[0]                     = cast_value;
            args[0]                     = gep_value;
        }
        else
        {
            // TODO: break out into seperate function
            // dynamic application
            if (outer->machine_def.update_fn->fn_def._init_block == NULL)
            {
                NecroMachineAST* prev_block   = outer->machine_def.update_fn->fn_def._curr_block;
                NecroMachineAST* init_block   = necro_append_block(program, outer->machine_def.update_fn, "init");
                NecroMachineAST* cont_block   = necro_append_block(program, outer->machine_def.update_fn, "cont");

                // entry block
                NecroMachineAST* machine_value_1 = necro_build_load_from_slot(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), persistent_slot, "dyn");
                NecroMachineAST* cmp_result   = necro_build_cmp(program, outer->machine_def.update_fn, NECRO_MACHINE_CMP_EQ, machine_value_1, necro_create_null_necro_machine_value(program, machine_value_1->necro_machine_type));
                necro_build_cond_break(program, outer->machine_def.update_fn, cmp_result, init_block, cont_block);

                // init block
                necro_move_to_block(program, outer->machine_def.update_fn, init_block);
                NecroMachineAST* machine_value_2 = necro_build_call(program, outer->machine_def.update_fn, machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, "mkn");
                necro_build_store_into_slot(program, outer->machine_def.update_fn, machine_value_2, necro_create_param_reg(program, outer->machine_def.update_fn, 0), persistent_slot);
                necro_build_break(program, outer->machine_def.update_fn, cont_block);

                // cont block
                necro_move_to_block(program, outer->machine_def.update_fn, cont_block);
                NecroMachineAST* machine_value_phi = necro_build_phi(program, outer->machine_def.update_fn, (NecroMachineAST*[]) { prev_block, init_block }, (NecroMachineAST*[]) { machine_value_1, machine_value_2 }, 2);
                args[0]                      = machine_value_phi;
                outer->machine_def.update_fn->fn_def._init_block = init_block;
                outer->machine_def.update_fn->fn_def._cont_block = cont_block;
            }
            else
            {
                // Write into existing init and cont blocks
                NecroMachineAST* prev_block  = outer->machine_def.update_fn->fn_def._curr_block;
                NecroMachineAST* init_block  = outer->machine_def.update_fn->fn_def._init_block;
                // NecroMachineAST* cont_block  = outer->machine_def.update_fn->fn_def._cont_block;

                //!!!!!!!!!!!!!!!!!!!!!!!!!!
                // TODO: insert statement!
                //!!!!!!!!!!!!!!!!!!!!!!!!!!

                // init block
                necro_move_to_block(program, outer->machine_def.update_fn, init_block);
                // init_block->block.num_statements--;
                NecroMachineAST* machine_value_2 = necro_build_call(program, outer->machine_def.update_fn, machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, "mkn");
                necro_build_store_into_slot(program, outer->machine_def.update_fn, machine_value_2, necro_create_param_reg(program, outer->machine_def.update_fn, 0), persistent_slot);
                // necro_build_break(program, outer->machine_def.update_fn, cont_block);

                // resume writing in prev block
                necro_move_to_block(program, outer->machine_def.update_fn, prev_block);
                NecroMachineAST* machine_value = necro_build_load_from_slot(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), persistent_slot, "dyn");
                args[0]                  = machine_value;
            }
        }
    }
    return necro_build_call(program, outer->machine_def.update_fn, fn_value, args, arg_count, "app");
}

NecroMachineAST* necro_core_to_machine_3_var(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    NecroSymbolInfo* info = necro_symtable_get(program->symtable, core_ast->var.id);
    // It's a constructor
    if (info->is_constructor)
    {
        return necro_build_call(program, outer->machine_def.update_fn, info->necro_machine_ast, NULL, 0, "con");
    }
    // It's a persistent value
    else if (info->necro_machine_ast->type == NECRO_MACHINE_DEF && info->necro_machine_ast->machine_def.state_type == NECRO_STATE_STATEFUL)
    {
        NecroMachineAST* param0 = necro_create_param_reg(program, outer->machine_def.update_fn, 0);
        NecroMachineAST* loaded = necro_build_load_from_slot(program, outer->machine_def.update_fn, param0, info->persistent_slot, "var");
        // How do we safely cache loaded values!?
        return loaded;
    }
    // It's a local
    else if (info->necro_machine_ast->type == NECRO_MACHINE_VALUE)
    {
        return info->necro_machine_ast;
    }
    // Else it's a global
    else if (info->necro_machine_ast->type == NECRO_MACHINE_DEF)
    {
        assert(info->necro_machine_ast->machine_def.global_value != NULL);
        return necro_build_load_from_slot(program, outer->machine_def.update_fn, info->necro_machine_ast->machine_def.global_value, 1, "glb");
    }
    else
    {
        assert(false);
        return NULL;
    }
}

NecroMachineAST* necro_core_to_machine_3_lit(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_LIT);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);
    switch (core_ast->lit.type)
    {
    case NECRO_AST_CONSTANT_INTEGER:
    {
        NecroMachineAST* lit = necro_create_i64_necro_machine_value(program, core_ast->lit.int_literal);
        NecroMachineAST* val = necro_build_call(program, outer->machine_def.update_fn, program->mkIntFnValue, (NecroMachineAST*[]) { lit }, 1, "int");
        return val;
    }
    case NECRO_AST_CONSTANT_FLOAT:
    {
        NecroMachineAST* lit = necro_create_f64_necro_machine_value(program, core_ast->lit.double_literal);
        NecroMachineAST* val = necro_build_call(program, outer->machine_def.update_fn, program->mkFloatFnValue, (NecroMachineAST*[]) { lit }, 1, "flt");
        return val;
    }
    case NECRO_AST_CONSTANT_CHAR:
        assert(false);
        return NULL;
    default:
        return NULL;
    }
}


///////////////////////////////////////////////////////
// Core to Machine Pass 3 Go
///////////////////////////////////////////////////////
NecroMachineAST* necro_core_to_machine_3_go(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    switch (core_ast->expr_type)
    {
    case NECRO_CORE_EXPR_BIND:      return necro_core_to_machine_3_bind(program, core_ast, outer);
    case NECRO_CORE_EXPR_LET:       return necro_core_to_machine_3_let(program, core_ast, outer);
    case NECRO_CORE_EXPR_LAM:       return necro_core_to_machine_3_lambda(program, core_ast, outer);
    case NECRO_CORE_EXPR_APP:       return necro_core_to_machine_3_app(program, core_ast, outer);
    case NECRO_CORE_EXPR_CASE:      return necro_core_to_machine_3_case(program, core_ast, outer);
    case NECRO_CORE_EXPR_VAR:       return necro_core_to_machine_3_var(program, core_ast, outer);
    case NECRO_CORE_EXPR_LIT:       return necro_core_to_machine_3_lit(program, core_ast, outer);
    case NECRO_CORE_EXPR_DATA_DECL: return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    default:                        assert(false && "Unimplemented AST type in necro_core_to_machine_3_go"); return NULL;
    }
}

///////////////////////////////////////////////////////
// Core to Machine
///////////////////////////////////////////////////////
NecroMachineProgram necro_core_to_machine(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types)
{
    NecroMachineProgram program = necro_create_initial_machine_program(symtable->intern, symtable, scoped_symtable, prim_types);

    // Pass 1
    NecroCoreAST_Expression* top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_machine_1_go(&program, top_level_list->list.expr, NULL);
        top_level_list = top_level_list->list.next;
    }

    // Pass 2
    top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_machine_2_go(&program, top_level_list->list.expr, NULL);
        top_level_list = top_level_list->list.next;
    }

    // Pass 3
    top_level_list = core_ast->root;
    while (top_level_list != NULL)
    {
        if (top_level_list->list.expr != NULL)
            necro_core_to_machine_3_go(&program, top_level_list->list.expr, NULL);
        top_level_list = top_level_list->list.next;
    }

    return program;
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine.h"
#include <ctype.h>
#include "machine_prim.h"
#include "machine_case.h"
#include "machine_print.h"
#include "machine_build.h"
#include "machine_copy.h"

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
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

void necro_build_debug_print(NecroMachineProgram* program, NecroMachineAST* fn_def, int print_value)
{
    necro_build_call(program, fn_def, necro_symtable_get(program->symtable, program->runtime._necro_debug_print.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_word_int_value(program, print_value) }, 1, NECRO_C_CALL, "");
}

///////////////////////////////////////////////////////
// NecroMachineProgram
///////////////////////////////////////////////////////
NECRO_WORD_SIZE necro_get_word_size()
{
    if (sizeof(char*) == 4)
        return NECRO_WORD_4_BYTES;
    else if (sizeof(char*) == 8)
        return NECRO_WORD_8_BYTES;
    else
        assert(false);
}

NecroMachineProgram necro_create_initial_machine_program(NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer, NecroClosureDefVector closure_defs)
{
    NecroMachineProgram program =
    {
        .arena              = necro_create_paged_arena(),
        .snapshot_arena     = necro_create_snapshot_arena(),
        .structs            = necro_create_necro_machine_ast_vector(),
        .globals            = necro_create_necro_machine_ast_vector(),
        .functions          = necro_create_necro_machine_ast_vector(),
        .machine_defs       = necro_create_necro_machine_ast_vector(),
        .word_size          = necro_get_word_size(),
        .intern             = intern,
        .symtable           = symtable,
        .scoped_symtable    = scoped_symtable,
        .prim_types         = prim_types,
        .infer              = infer,
        .gen_vars           = 0,
        .necro_main         = NULL,
        .main_symbol        = necro_intern_string(symtable->intern, "main"),
        .copy_table         = necro_create_machine_copy_table(symtable, prim_types),
        .closure_con        = necro_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_Closure")),
        .closure_cons       = necro_create_closure_con_vector(),
        .closure_types      = necro_create_closure_type_vector(),
        .closure_defs       = closure_defs,
        .apply_fns          = necro_create_apply_fn_vector(),
        .dyn_state_con      = necro_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_DynState")),
        .null_con           = necro_get_data_con_from_symbol(prim_types, necro_intern_string(intern, "_NullPoly")),
        .get_apply_state_fn = NULL,
    };
    program.necro_uint_type  = necro_create_word_sized_uint_type(&program);
    program.necro_int_type   = necro_create_word_sized_int_type(&program);
    program.necro_float_type = necro_create_word_sized_float_type(&program);
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
    necro_destroy_machine_copy_table(&program->copy_table);
    necro_destroy_closure_con_vector(&program->closure_cons);
    necro_destroy_closure_def_vector(&program->closure_defs);
    necro_destroy_closure_type_vector(&program->closure_types);
    necro_destroy_apply_fn_vector(&program->apply_fns);
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

    NecroArenaSnapshot snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineType*  struct_ptr_type = necro_create_machine_ptr_type(&program->arena, struct_type->necro_machine_type);
    char*              con_name        = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(program->intern, con->condid.symbol) });
    // char*              const_con_name  = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkConst", necro_intern_get_string(program->intern, con->condid.symbol) });
    NecroVar           con_var         = necro_gen_var(program, NULL, con_name, NECRO_NAME_UNIQUE);
    // NecroVar           const_con_var   = necro_gen_var(program, NULL, const_con_name, NECRO_NAME_UNIQUE);
    size_t             arg_count       = necro_count_data_con_args(con);
    assert(arg_count <= max_arg_count);

    // for (size_t c = 0; c < 2; ++c)
    // {
        // con_var                             = (c == 0) ? con_var : const_con_var;
        con_var                             = con_var;
        NecroMachineType**       parameters = necro_snapshot_arena_alloc(&program->snapshot_arena, arg_count * sizeof(NecroType*));
        NecroCoreAST_Expression* arg        = con->arg_list;
        for (size_t i = 0; i < arg_count; ++i)
        {
            assert(arg->expr_type == NECRO_CORE_EXPR_LIST);
            NecroMachineType* arg_type = necro_core_ast_to_machine_type(program, arg->list.expr);
            parameters[i] = necro_make_ptr_if_boxed(program, arg_type);
            arg = arg->list.next;
        }
        NecroMachineType*  mk_fn_type      = necro_create_machine_fn_type(&program->arena, struct_ptr_type, parameters, arg_count);
        NecroMachineAST*   mk_fn_body      = necro_create_machine_block(program, "entry", NULL);
        NecroMachineAST*   mk_fn_def       = necro_create_machine_fn(program, con_var, mk_fn_body, mk_fn_type);
        NecroMachineAST*   data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type->necro_machine_type, arg_count + 1);
        necro_build_store_into_slot(program, mk_fn_def, necro_create_word_uint_value(program, con_number), data_ptr, 0);

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
        }

        necro_build_return(program, mk_fn_def, data_ptr);
        // if (c == 0)
            necro_symtable_get(program->symtable, con->condid.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
        // else
        //     necro_symtable_get(program->symtable, con->condid.id)->const_necro_machine_ast = mk_fn_def->fn_def.fn_value;
    // }

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

    // Enum type
    if (max_arg_count == 0)
    {
        NecroMachineAST* int_ast = necro_symtable_get(program->symtable, program->prim_types->int_type.id)->necro_machine_ast;
        necro_symtable_get(program->symtable, core_ast->data_decl.data_id.id)->is_enum = true;
        necro_symtable_get(program->symtable, core_ast->data_decl.data_id.id)->necro_machine_ast = int_ast;
        con = core_ast->data_decl.con_list;
        while (con != NULL)
        {
            necro_symtable_get(program->symtable, con->condid.id)->is_enum           = true;
            necro_symtable_get(program->symtable, con->condid.id)->necro_machine_ast = int_ast;
            con = con->next;
        }
        return;
    }

    // Members
    bool               is_sum_type = core_ast->data_decl.con_list->next != NULL;
    NecroMachineType** members     = necro_snapshot_arena_alloc(&program->snapshot_arena, max_arg_count * sizeof(NecroMachineType*));
    members[0] = program->necro_uint_type;
    NecroCoreAST_Expression* args = core_ast->data_decl.con_list->arg_list;
    for (size_t i = 1; i < max_arg_count + 1; ++i)
    // for (size_t i = 0; i < max_arg_count; ++i)
    {
        if (is_sum_type)
        {
            members[i] = program->necro_poly_ptr_type;
        }
        else
        {
            // members[i] = necro_create_machine_ptr_type(&program->arena, necro_core_ast_to_machine_type(program, args->list.expr));
            members[i] = necro_make_ptr_if_boxed(program, necro_core_ast_to_machine_type(program, args->list.expr));
            args = args->list.next;
        }
    }

    // Struct
    NecroMachineAST* struct_type = necro_create_machine_struct_def(program, core_ast->data_decl.data_id, members, max_arg_count + 1);
    // NecroMachineAST* struct_type = necro_create_machine_struct_def(program, core_ast->data_decl.data_id, members, max_arg_count);

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
    NecroMachineAST* machine_def = necro_create_machine_initial_machine_def(program, core_ast->bind.var, outer, necro_core_ast_to_machine_type(program, core_ast), necro_core_ast_to_necro_type(program, core_ast));
    machine_def->machine_def.is_recursive = core_ast->bind.is_recursive;

    if (outer == NULL && necro_symtable_get(program->symtable, core_ast->bind.var.id)->name.id == program->main_symbol.id)
    {
        assert(program->program_main == NULL);
        program->program_main = machine_def;
    }

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
    // declare mk_fn and init_fn
    if (outer == NULL)
    {
        // mk_fn
        {
            const char*       mk_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) + 1 });
            NecroVar          mk_fn_var  = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
            NecroMachineType* mk_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
            NecroMachineAST*  mk_fn_body = necro_create_machine_block(program, "entry", NULL);
            NecroMachineAST*  mk_fn_def  = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
            program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
            machine_def->machine_def.mk_fn  = mk_fn_def;
        }

        // init_fn
        {
            const char*       init_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_init", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) + 1 });
            NecroVar          init_fn_var  = necro_gen_var(program, NULL, init_fn_name, NECRO_NAME_UNIQUE);
            NecroMachineType* init_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { program->necro_poly_ptr_type }, 1);
            NecroMachineAST*  init_fn_body = necro_create_machine_block(program, "entry", NULL);
            NecroMachineAST*  init_fn_def  = necro_create_machine_fn(program, init_fn_var, init_fn_body, init_fn_type);
            program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
            machine_def->machine_def.init_fn = init_fn_def;
        }
    }

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

NecroSlot necro_add_member(NecroMachineProgram* program, NecroMachineDef* machine_def, NecroMachineType* new_member, NecroMachineAST* member_ast, size_t data_id, bool is_dynamic)
{
    assert(program != NULL);
    assert(machine_def != NULL);
    // if (machine_def->num_members + 1 >= machine_def->members_size)
    if (machine_def->num_members >= machine_def->members_size)
    {
        NecroSlot* old_members = machine_def->members;
        machine_def->members   = necro_paged_arena_alloc(&program->arena, machine_def->members_size * 2 * sizeof(NecroSlot));
        memcpy(machine_def->members, old_members, machine_def->members_size * sizeof(NecroSlot));
        machine_def->members_size *= 2;
    }
    NecroSlot slot = (NecroSlot) { .necro_machine_type = new_member, .slot_num = machine_def->num_members, .slot_ast = member_ast, .machine_def = machine_def, .data_id = data_id, .is_dynamic = is_dynamic };
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
    ast->machine_def.num_members    = 0;
    ast->machine_def._first_dynamic = -1;
}

void necro_calculate_statefulness(NecroMachineProgram* program, NecroMachineAST* ast)
{
    assert(program != NULL);
    assert(ast->type == NECRO_MACHINE_DEF);
    ast->machine_def.state_type = necro_symtable_get(program->symtable, ast->machine_def.bind_name.id)->state_type;
    if (ast->machine_def.state_type == NECRO_STATE_POLY)
    {
        if (ast->machine_def.num_arg_names > 0)
            ast->machine_def.state_type = NECRO_STATE_POINTWISE;
        else
            ast->machine_def.state_type = NECRO_STATE_CONSTANT;
    }
    // if (ast->machine_def.is_recursive && ast->machine_def.num_arg_names > 0 && ast->machine_def.num_members == 0 && ast->machine_def.most_stateful_type_referenced == NECRO_STATE_CONSTANT)
    //     ast->machine_def.state_type = NECRO_STATE_CONSTANT;
    // else if (ast->machine_def.is_recursive && ast->machine_def.num_arg_names == 0 && ast->machine_def.num_members == 0 && ast->machine_def.outer == NULL)
    //     ast->machine_def.state_type = NECRO_STATE_POINTWISE;
    // else if (ast->machine_def.is_recursive && ast->machine_def.num_arg_names == 0 && ast->machine_def.num_members == 0)
    //     ast->machine_def.state_type = NECRO_STATE_STATEFUL;
    // else if (ast->machine_def.num_members == 0 && ast->machine_def.num_arg_names == 0 && ast->machine_def.most_stateful_type_referenced == NECRO_STATE_CONSTANT)
    //     ast->machine_def.state_type = NECRO_STATE_CONSTANT;
    // else if (ast->machine_def.num_members == 0 && ast->machine_def.num_arg_names == 0)
    //     ast->machine_def.state_type = NECRO_STATE_POINTWISE;
    // else if (ast->machine_def.num_members == 0 && ast->machine_def.num_arg_names > 0)
    //     ast->machine_def.state_type = NECRO_STATE_POINTWISE;
    // else if (ast->machine_def.num_members > 0)
    //     ast->machine_def.state_type = NECRO_STATE_STATEFUL;
    // else
    //     assert(false);
    if (ast->machine_def.update_fn != NULL)
    {
        ast->machine_def.update_fn->fn_def.state_type = ast->machine_def.state_type;
    }
}

void necro_core_to_machine_2_bind(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_BIND);
    if (outer != NULL)
        assert(outer->type == NECRO_MACHINE_DEF);

    //--------------------
    // Retrieve machine_def
    NecroMachineAST* machine_def = necro_symtable_get(program->symtable, core_ast->bind.var.id)->necro_machine_ast;
    assert(machine_def != NULL);
    assert(machine_def->type == NECRO_MACHINE_DEF);

    //--------------------
    // Go deeper
    if (outer != NULL)
    {
        necro_core_to_machine_2_go(program, core_ast->bind.expr, outer);
        necro_calculate_statefulness(program, machine_def);
        return;
    }
    machine_def->necro_machine_type    = necro_create_machine_struct_type(&program->arena, machine_def->machine_def.machine_name, NULL, 0);
    machine_def->machine_def.is_pushed = true;
    necro_core_to_machine_2_go(program, core_ast->bind.expr, machine_def);

    //--------------------
    // Calculate statefulness
    necro_remove_only_self_recursive_member(program, machine_def);
    necro_calculate_statefulness(program, machine_def);
    machine_def->machine_def.is_pushed = false;

    //--------------------
    // Calculate machine type
    const bool         is_machine_fn        = machine_def->machine_def.num_arg_names > 0;
    const size_t       num_machine_members  = machine_def->machine_def.num_members;
    NecroMachineType** machine_type_members = necro_paged_arena_alloc(&program->arena, num_machine_members * sizeof(NecroMachineType*));
    for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
        machine_type_members[i] = machine_def->machine_def.members[i].necro_machine_type;
    machine_def->necro_machine_type->struct_type.members     = machine_type_members;
    machine_def->necro_machine_type->struct_type.num_members = num_machine_members;

    //--------------------
    // Add global state
    if (outer == NULL && !is_machine_fn && machine_def->machine_def.state_type == NECRO_STATE_STATEFUL)
    {
        NecroMachineAST* global_state = necro_create_global_value(program, machine_def->machine_def.state_name, necro_create_machine_ptr_type(&program->arena, necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type)));
        machine_def->machine_def.global_state = global_state;
        necro_program_add_global(program, global_state);
    }
    //--------------------
    // Add global value
    if (outer == NULL && !is_machine_fn)
    {
        NecroMachineAST* global_value = NULL;
        if (necro_is_unboxed_type(program, machine_def->machine_def.value_type))
            global_value = necro_create_global_value(program, machine_def->machine_def.bind_name, necro_create_machine_ptr_type(&program->arena, machine_def->machine_def.value_type));
        else
            global_value = necro_create_global_value(program, machine_def->machine_def.bind_name, necro_create_machine_ptr_type(&program->arena, necro_create_machine_ptr_type(&program->arena, machine_def->machine_def.value_type)));
        machine_def->machine_def.global_value = global_value;
        necro_program_add_global(program, global_value);
    }

    if (machine_def->machine_def.state_type < NECRO_STATE_STATEFUL)
    {
        machine_def->machine_def.mk_fn   = NULL;
        machine_def->machine_def.init_fn = NULL;
    }

    //--------------------
    // Define mk_fn and init_fn
    // TODO: Initializers
    if (machine_def->machine_def.mk_fn != NULL)
    {
        NecroMachineType* machine_ptr_type = necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type);
        //--------------------
        // init_fn
        {
            machine_def->machine_def.init_fn->necro_machine_type->fn_type.parameters[0] = machine_ptr_type;
            NecroMachineAST* data_ptr = necro_create_param_reg(program, machine_def->machine_def.init_fn, 0);
            // gep and init
            for (size_t i = 0; i < machine_def->machine_def.num_members; ++i)
            {
                NecroSlot            slot        = machine_def->machine_def.members[i];
                NecroConstructorInfo member_info = program->copy_table.data_map.data[slot.data_id];
                if (member_info.is_machine_def && !slot.is_dynamic && slot.slot_ast->machine_def.init_fn != NULL)
                {
                    NecroMachineAST* member = necro_build_gep(program, machine_def->machine_def.init_fn, data_ptr, (uint32_t[]) { 0, i }, 2, "member");
                    necro_build_call(program, machine_def->machine_def.init_fn, slot.slot_ast->machine_def.init_fn->fn_def.fn_value, (NecroMachineAST*[]) { member }, 1, NECRO_LANG_CALL, "");
                }
                else if (slot.is_dynamic)
                {
                    necro_build_store_into_slot(program, machine_def->machine_def.init_fn, necro_create_null_necro_machine_value(program, slot.necro_machine_type), data_ptr, slot.slot_num);
                }
            }
            necro_build_return_void(program, machine_def->machine_def.init_fn);
        }

        //--------------------
        // mk_fn
        {
            machine_def->machine_def.mk_fn->necro_machine_type->fn_type.return_type = machine_ptr_type;
            size_t               data_id          = necro_create_machine_def_data_info(program, machine_def);
            NecroConstructorInfo info             = necro_get_data_info(program, data_id, 0);
            NecroMachineAST*     data_ptr         = necro_build_nalloc(program, machine_def->machine_def.mk_fn, machine_def->necro_machine_type, info.num_members);
            necro_build_call(program, machine_def->machine_def.mk_fn, machine_def->machine_def.init_fn->fn_def.fn_value, (NecroMachineAST*[]) { data_ptr }, 1, NECRO_LANG_CALL, "");
            necro_build_return(program, machine_def->machine_def.mk_fn, data_ptr);
        }
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

NECRO_STATE_TYPE necro_most_stateful_type(NECRO_STATE_TYPE state_type1, NECRO_STATE_TYPE state_type2)
{
    if (state_type1 > state_type2)
        return state_type1;
    else
        return state_type2;
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
    NecroSymbolInfo* info     = necro_symtable_get(program->symtable, function->var.id);
    NecroMachineAST* fn_value = info->necro_machine_ast;
    if (function->var.id.id == program->closure_con.id.id)
        fn_value = necro_get_closure_con(program, arg_count);
    else if (function->var.id.id == program->prim_types->apply_fn.id.id)
        fn_value = necro_get_apply_fn(program, arg_count);
    else if (function->var.id.id == program->dyn_state_con.id.id)
        return;
    bool              is_persistent = false;
    NecroMachineAST*  fn_def        = NULL;
    NecroMachineType* fn_type       = NULL;
    assert(fn_value != NULL);
    // Statefulness affected by arguments!
    // A pointwise function applied to all constant arguments is actually constant!
    if (fn_value->type == NECRO_MACHINE_DEF)
    {
        fn_def        = fn_value;
        is_persistent = fn_value->machine_def.state_type == NECRO_STATE_STATEFUL;
        fn_type       = fn_value->machine_def.fn_type;
        // if (fn_value != outer)
        //     outer->machine_def.most_stateful_type_referenced = necro_most_stateful_type(outer->machine_def.most_stateful_type_referenced, fn_value->machine_def.state_type);
    }
    else
    {
        // if (fn_value != outer)
        //     outer->machine_def.most_stateful_type_referenced = necro_most_stateful_type(outer->machine_def.most_stateful_type_referenced, fn_value->fn_def.state_type);
        fn_type = fn_value->necro_machine_type;
    }
    assert(fn_type->type == NECRO_MACHINE_TYPE_FN);
    assert(fn_type->fn_type.num_parameters == arg_count);
    if (!is_persistent)
        return;
    bool is_dynamic = fn_def->machine_def.is_pushed || (fn_def->machine_def.is_recursive && fn_def  == outer);
    if (!is_dynamic)
    {
        size_t data_id = necro_create_machine_def_data_info(program, fn_def);
        core_ast->app.persistent_slot = necro_add_member(program, &outer->machine_def, fn_def->necro_machine_type, fn_def, data_id, false).slot_num;
    }
    else
    {
        fn_def->machine_def.is_recursive = true;
        core_ast->app.persistent_slot    = necro_add_member(program, &outer->machine_def, necro_create_machine_ptr_type(&program->arena, fn_def->necro_machine_type), fn_def, 0, true).slot_num;
        if (fn_def == outer || fn_def->machine_def.bind_name.id.id == outer->machine_def.bind_name.id.id)
        {
            outer->machine_def.members[core_ast->app.persistent_slot].data_id = NECRO_SELF_DATA_ID;
        }
        else
        {
            size_t data_id = necro_create_machine_def_data_info(program, fn_def);
            outer->machine_def.members[core_ast->app.persistent_slot].data_id = data_id;
        }
        if (outer->machine_def._first_dynamic == -1)
            outer->machine_def._first_dynamic = core_ast->app.persistent_slot;
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
    // outer->machine_def.most_stateful_type_referenced = necro_most_stateful_type(outer->machine_def.most_stateful_type_referenced, machine_ast->machine_def.state_type);
    if (machine_ast->machine_def.state_type != NECRO_STATE_STATEFUL)
        return;
    if (necro_is_var_machine_arg(program, &outer->machine_def, core_ast->var))
        return;
    if (machine_ast->machine_def.outer == NULL)
    {
        // outer->machine_def.most_stateful_type_referenced = NECRO_STATE_STATEFUL;
        return;
    }
    if (machine_ast->machine_def.is_persistent_slot_set)
        return;
    size_t data_id = necro_create_data_info(program, necro_core_ast_to_necro_type(program, core_ast)); //necro_symtable_get(program->symtable, core_ast->var.id)->type);
    machine_ast->machine_def.is_persistent_slot_set = true;
    NecroSlot slot = necro_is_unboxed_type(program, machine_ast->machine_def.value_type)
        ? necro_add_member(program, &outer->machine_def, machine_ast->machine_def.value_type, machine_ast, data_id, false)
        : necro_add_member(program, &outer->machine_def, necro_create_machine_ptr_type(&program->arena, machine_ast->machine_def.value_type), machine_ast, data_id, false);
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
        if (machine_def->machine_def.is_persistent_slot_set)
        {
            // Outer update_fn is NULL somehow?!?!?!
            NecroMachineAST* param0 = necro_create_param_reg(program, outer->machine_def.update_fn, 0);
            necro_build_store_into_slot(program, machine_def->machine_def.outer->machine_def.update_fn, return_value, param0, info->persistent_slot);
        }
        else
        {
            info->necro_machine_ast = return_value;
        }
        return return_value;
    }

    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);

    // Start function
    bool            is_stateful       = machine_def->machine_def.state_type == NECRO_STATE_STATEFUL;
    const char*     update_name       = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_update", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) + 1 });
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
    NecroMachineType*  update_fn_type  = necro_create_machine_fn_type(&program->arena, necro_make_ptr_if_boxed(program, machine_def->machine_def.value_type), update_params, num_update_params);
    NecroMachineAST*   update_fn_body  = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*   update_fn_def   = necro_create_machine_fn(program, update_var, update_fn_body, update_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
    machine_def->machine_def.update_fn = update_fn_def;

    for (size_t i = 0; i < machine_def->machine_def.num_arg_names; ++i)
    {
        NecroMachineAST* param_ast = necro_create_param_reg(program, update_fn_def, i + (is_stateful ? 1 : 0));
        necro_symtable_get(program->symtable, machine_def->machine_def.arg_names[i].id)->necro_machine_ast = param_ast;
    }

    // Init dynamic nodes
    if (machine_def->machine_def._first_dynamic != -1)
    {
        // Create blocks
        size_t           persistent_slot = machine_def->machine_def._first_dynamic;
        NecroMachineAST* prev_block      = update_fn_def->fn_def._curr_block;
        NecroMachineAST* init_block      = necro_append_block(program, update_fn_def, "init");
        NecroMachineAST* cont_block      = necro_append_block(program, update_fn_def, "cont");
        // entry block
        NecroMachineAST* machine_value_1 = necro_build_load_from_slot(program, machine_def->machine_def.update_fn, necro_create_param_reg(program, machine_def->machine_def.update_fn, 0), persistent_slot, "dyn");
        NecroMachineAST* cmp_result      = necro_build_cmp(program, machine_def->machine_def.update_fn, NECRO_MACHINE_CMP_EQ, machine_value_1, necro_create_null_necro_machine_value(program, machine_value_1->necro_machine_type));
        necro_build_cond_break(program, machine_def->machine_def.update_fn, cmp_result, init_block, cont_block);
        // init block
        necro_move_to_block(program, update_fn_def, init_block);
        necro_build_break(program, update_fn_def, cont_block);
        // cont block
        necro_move_to_block(program, update_fn_def, cont_block);
        update_fn_def->fn_def._init_block = init_block;
        update_fn_def->fn_def._cont_block = cont_block;
    }

    // Go deeper
    NecroMachineAST* result = necro_core_to_machine_3_go(program, core_ast->bind.expr, machine_def);

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

NecroMachineAST* necro_core_to_machine_3_app(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_APP);
    assert(outer != NULL);
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
    NecroSymbolInfo* info     = necro_symtable_get(program->symtable, function->var.id);
    NecroMachineAST* fn_value = // (outer->machine_def.state_type == NECRO_STATE_CONSTANT && info->is_constructor) ?
        // info->const_necro_machine_ast :
        info->necro_machine_ast;
    if (function->var.id.id == program->closure_con.id.id)
        fn_value = necro_get_closure_con(program, arg_count);
    else if (function->var.id.id == program->prim_types->apply_fn.id.id)
        fn_value = necro_get_apply_fn(program, arg_count);
    else if (function->var.id.id == program->dyn_state_con.id.id)
        return necro_core_to_machine_dyn_state_3(program, core_ast, outer);
    NecroMachineAST* machine_def = NULL;
    bool             is_stateful = false;
    assert(fn_value != NULL);
    if (fn_value->type == NECRO_MACHINE_DEF)
    {
        machine_def = fn_value;
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
    NecroMachineAST**  args      = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroMachineAST*));
    size_t             arg_index = arg_count - 1;
    function                     = core_ast;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        args[arg_index] = necro_core_to_machine_3_go(program, function->app.exprB, outer);
        arg_index--;
        function        = function->app.exprA;
    }

    necro_rewind_arena(&program->snapshot_arena, snapshot);
    // Pass in state struct
    if (is_stateful)
    {
        assert(machine_def != NULL);
        bool is_dynamic = machine_def->machine_def.is_recursive && machine_def == outer;
        if (!is_dynamic)
        {
            NecroMachineAST* gep_value = necro_build_gep(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), (uint32_t[]) { 0, persistent_slot }, 2, "gep");
            args[0]                    = gep_value;
        }
        else
        {
            assert(outer->machine_def.update_fn->fn_def._init_block != NULL);
            NecroMachineAST* prev_block  = outer->machine_def.update_fn->fn_def._curr_block;
            NecroMachineAST* init_block  = outer->machine_def.update_fn->fn_def._init_block;
            // Write into existing init block
            necro_move_to_block(program, outer->machine_def.update_fn, init_block);
            NecroMachineAST* machine_value_2 = necro_build_call(program, outer->machine_def.update_fn, machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "mkn");
            necro_build_store_into_slot(program, outer->machine_def.update_fn, machine_value_2, necro_create_param_reg(program, outer->machine_def.update_fn, 0), persistent_slot);
            // resume writing in prev block
            necro_move_to_block(program, outer->machine_def.update_fn, prev_block);
            NecroMachineAST* machine_value = necro_build_load_from_slot(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), persistent_slot, "dyn");
            args[0]                        = machine_value;
        }
    }
    return necro_build_call(program, outer->machine_def.update_fn, fn_value, args, arg_count, NECRO_LANG_CALL, "app");
}

NecroMachineAST* necro_core_to_machine_3_var(NecroMachineProgram* program, NecroCoreAST_Expression* core_ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(core_ast != NULL);
    assert(core_ast->expr_type == NECRO_CORE_EXPR_VAR);
    assert(outer != NULL);
    assert(outer->type == NECRO_MACHINE_DEF);
    assert(core_ast->var.id.id != program->closure_con.id.id); // Should never encounter the closure constructor here
    NecroSymbolInfo* info = necro_symtable_get(program->symtable, core_ast->var.id);
    // It's NULL
    if (core_ast->var.id.id == program->null_con.id.id)
    {
        return necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type);
    }
    // It's an enum
    else if (info->is_constructor && info->arity == 0 && info->is_enum)
    {
        return necro_create_word_int_value(program, info->con_num);
    }
    // It's a constructor
    else if (info->is_constructor && info->arity == 0)
    {
        // if (outer->machine_def.state_type == NECRO_STATE_CONSTANT)
        //     return necro_build_call(program, outer->machine_def.update_fn, info->const_necro_machine_ast, NULL, 0, NECRO_LANG_CALL, "const_con");
        // else
            return necro_build_call(program, outer->machine_def.update_fn, info->necro_machine_ast, NULL, 0, NECRO_LANG_CALL, "con");
    }
    // It's a function pointer
    else if (info->arity > 0)
    {
        NecroMachineAST* fn_value = NULL;
        if (info->is_constructor)
        {
            // Can never be const because _apply is always stateful
            fn_value = info->necro_machine_ast;
        }
        else if (info->necro_machine_ast->type == NECRO_MACHINE_DEF)
        {
            fn_value = info->necro_machine_ast->machine_def.update_fn->fn_def.fn_value;
        }
        else if (info->necro_machine_ast->type == NECRO_MACHINE_FN_DEF)
        {
            fn_value = info->necro_machine_ast->fn_def.fn_value;
        }
        else if (info->necro_machine_ast->type == NECRO_MACHINE_VALUE)
        {
            fn_value = info->necro_machine_ast;
        }
        else
        {
            assert(false);
        }
        NecroMachineAST* fn_ptr    = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineAST));
        *fn_ptr                    = *fn_value;
        fn_ptr->necro_machine_type = necro_create_machine_ptr_type(&program->arena, fn_value->necro_machine_type);
        return fn_ptr;
    }
    // It's a global
    else if (info->necro_machine_ast->type == NECRO_MACHINE_VALUE && info->necro_machine_ast->value.value_type == NECRO_MACHINE_VALUE_GLOBAL)
    {
        NecroMachineType* type = info->necro_machine_ast->necro_machine_type;
        return necro_build_load_from_ptr(program, outer->machine_def.update_fn, info->necro_machine_ast, "glb");
    }
    // It's a persistent value
    else if (info->necro_machine_ast->type == NECRO_MACHINE_DEF && info->necro_machine_ast->machine_def.state_type == NECRO_STATE_STATEFUL && info->necro_machine_ast->machine_def.outer != NULL)
    {
        NecroMachineType* slot_type = outer->necro_machine_type->struct_type.members[info->persistent_slot];
        if (necro_is_unboxed_type(program, slot_type) || slot_type->type == NECRO_MACHINE_TYPE_PTR)
            return necro_build_load_from_slot(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), info->persistent_slot, "per");
        else
            return necro_build_gep(program, outer->machine_def.update_fn, necro_create_param_reg(program, outer->machine_def.update_fn, 0), (uint32_t[]) { 0, info->persistent_slot }, 2, "per");
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
        NecroMachineType* type = info->necro_machine_ast->machine_def.global_value->necro_machine_type;
        return necro_build_load_from_ptr(program, outer->machine_def.update_fn, info->necro_machine_ast->machine_def.global_value, "glb");
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
        // NecroMachineAST* lit = necro_create_i64_necro_machine_value(program, core_ast->lit.int_literal);
        // NecroMachineAST* val = necro_build_call(program, outer->machine_def.update_fn, program->mkIntFnValue, (NecroMachineAST*[]) { lit }, 1, "int");
        // return val;
        return necro_create_word_int_value(program, core_ast->lit.int_literal);
    }
    case NECRO_AST_CONSTANT_FLOAT:
    {
        // NecroMachineAST* lit = necro_create_f64_necro_machine_value(program, core_ast->lit.double_literal);
        // NecroMachineAST* val = necro_build_call(program, outer->machine_def.update_fn, program->mkFloatFnValue, (NecroMachineAST*[]) { lit }, 1, "flt");
        // return val;
        return necro_create_word_float_value(program, core_ast->lit.double_literal);
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
// Construct main
///////////////////////////////////////////////////////
void necro_construct_main(NecroMachineProgram* program)
{
    assert(program->program_main != NULL);
    // TODO: Main requirement and type checking in front end!

    NecroVar          necro_main_var   = necro_gen_var(program, NULL, "_necro_main", NECRO_NAME_UNIQUE);
    NecroMachineType* necro_main_type  = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
    NecroMachineAST*  necro_main_entry = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*  necro_main_fn    = necro_create_machine_fn(program, necro_main_var, necro_main_entry, necro_main_type);
    program->functions.length--; // Hack...
    NecroMachineAST*  necro_main_loop  = necro_append_block(program, necro_main_fn, "loop");

    //---------
    // entry
    necro_build_break(program, necro_main_fn, necro_main_loop);
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_init_runtime.id)->necro_machine_ast->fn_def.fn_value, NULL, 0, NECRO_C_CALL, "");

    // Create world
    NecroMachineAST* world_value = necro_build_nalloc(program, necro_main_fn, program->world_type, 1);
    necro_build_store_into_ptr(program, necro_main_fn, world_value, program->world_value);

    // Call constants
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.state_type > NECRO_STATE_CONSTANT || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        NecroMachineAST* const_value = necro_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "const_result");
        necro_build_store_into_ptr(program, necro_main_fn, const_value, program->machine_defs.data[i]->machine_def.global_value);
    }

    // Flip from and const
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_flip_const.id)->necro_machine_ast->fn_def.fn_value, NULL, 0, NECRO_C_CALL, "");

    // Create states
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.state_type != NECRO_STATE_STATEFUL || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        NecroMachineAST* mk_result = necro_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "mk_state_result");
        necro_build_store_into_ptr(program, necro_main_fn, mk_result, program->machine_defs.data[i]->machine_def.global_state);
    }

    // Only set stateful roots and global state
    size_t stateful_root_count = 0;
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        // if ((program->machine_defs.data[i]->machine_def.state_type != NECRO_STATE_STATEFUL && program->machine_defs.data[i]->machine_def.is_recursive) || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
        if (program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        if (program->machine_defs.data[i]->machine_def.state_type == NECRO_STATE_STATEFUL)
            stateful_root_count++; // State objects are roots
        if (program->machine_defs.data[i]->machine_def.is_recursive && !necro_is_unboxed_type(program, program->machine_defs.data[i]->machine_def.value_type))
            stateful_root_count++; // Boxed recursive values are roots
    }
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_copy_gc_initialize_root_set.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_word_uint_value(program, stateful_root_count) }, 1, NECRO_C_CALL, "");

    // set roots
    stateful_root_count = 0;
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        // if ((program->machine_defs.data[i]->machine_def.state_type != NECRO_STATE_STATEFUL && program->machine_defs.data[i]->machine_def.is_recursive) || program->machine_defs.data[i]->machine_def.num_arg_names != 0)
        if (program->machine_defs.data[i]->machine_def.num_arg_names != 0)
            continue;
        // global state
        if (program->machine_defs.data[i]->machine_def.state_type == NECRO_STATE_STATEFUL)
        {
            NecroMachineAST* bit_cast_global_state = necro_build_bit_cast(program, necro_main_fn, program->machine_defs.data[i]->machine_def.global_state, necro_create_machine_ptr_type(&program->arena, necro_create_machine_ptr_type(&program->arena, necro_create_word_sized_int_type(program))));
            size_t           data_id               = necro_create_machine_def_data_info(program, program->machine_defs.data[i]);
            necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_copy_gc_set_root.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[])
            {
                bit_cast_global_state,
                necro_create_word_uint_value(program, stateful_root_count),
                necro_create_word_uint_value(program, data_id),
            }, 3, NECRO_C_CALL, "");
            stateful_root_count++;
        }
        // global value
        if (program->machine_defs.data[i]->machine_def.is_recursive && !necro_is_unboxed_type(program, program->machine_defs.data[i]->machine_def.value_type))
        {
            NecroMachineAST* bit_cast_global_value = necro_build_bit_cast(program, necro_main_fn, program->machine_defs.data[i]->machine_def.global_value, necro_create_machine_ptr_type(&program->arena, necro_create_machine_ptr_type(&program->arena, necro_create_word_sized_int_type(program))));
            size_t           data_id               = necro_create_data_info(program, program->machine_defs.data[i]->machine_def.necro_value_type);
            necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_copy_gc_set_root.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[])
            {
                bit_cast_global_value,
                necro_create_word_uint_value(program, stateful_root_count),
                necro_create_word_uint_value(program, data_id),
            }, 3, NECRO_C_CALL, "");
            stateful_root_count++;
        }
    }

    //---------
    // loop
    necro_move_to_block(program, necro_main_fn, necro_main_loop);
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_update_runtime.id)->necro_machine_ast->fn_def.fn_value, NULL, 0, NECRO_C_CALL, "");
    for (size_t i = 0; i < program->machine_defs.length; ++i)
    {
        if (program->machine_defs.data[i]->machine_def.state_type == NECRO_STATE_STATEFUL && program->machine_defs.data[i]->machine_def.num_arg_names == 0)
        {
            NecroMachineAST* state  = necro_build_load_from_ptr(program, necro_main_fn, program->machine_defs.data[i]->machine_def.global_state, "state");
            NecroMachineAST* result = necro_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, (NecroMachineAST*[]) { state }, 1, NECRO_LANG_CALL, "stateful_result");
            necro_build_store_into_ptr(program, necro_main_fn, result, program->machine_defs.data[i]->machine_def.global_value);
        }
        else if (program->machine_defs.data[i]->machine_def.state_type == NECRO_STATE_POINTWISE && program->machine_defs.data[i]->machine_def.num_arg_names == 0)
        {
            NecroMachineAST* result = necro_build_call(program, necro_main_fn, program->machine_defs.data[i]->machine_def.update_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "pointwise_result");
            necro_build_store_into_ptr(program, necro_main_fn, result, program->machine_defs.data[i]->machine_def.global_value);
        }
    }
    NecroMachineAST* result = necro_build_load_from_ptr(program, necro_main_fn, program->program_main->machine_def.global_value, "main_result");
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_print.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { result }, 1, NECRO_C_CALL, "");
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_copy_gc_collect.id)->necro_machine_ast->fn_def.fn_value, NULL, 0, NECRO_C_CALL, "");
    necro_build_call(program, necro_main_fn, necro_symtable_get(program->symtable, program->runtime._necro_sleep.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_uint32_necro_machine_value(program, 10) }, 1, NECRO_C_CALL, "");
    necro_build_break(program, necro_main_fn, necro_main_loop);
    // necro_build_return_void(program, necro_main_fn);
    program->necro_main = necro_main_fn;
}

///////////////////////////////////////////////////////
// Core to Machine
///////////////////////////////////////////////////////
NecroMachineProgram necro_core_to_machine(NecroCoreAST* core_ast, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer, NecroClosureDefVector closure_defs)
{
    NecroMachineProgram program = necro_create_initial_machine_program(symtable->intern, symtable, scoped_symtable, prim_types, infer, closure_defs);

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

    necro_construct_main(&program);
    return program;
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_closure.h"
#include "machine.h"
#include "machine_build.h"
#include "symtable.h"
#include "machine_copy.h"

/*
    TODO:
*/

#define NECRO_NUM_CLOSURE_PRE_ARGS 4
#define NECRO_DEBUG_PRINT_CLOSURES 0

// TODO: Finish
NecroMachineAST* necro_core_to_machine_dyn_state_3(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    NecroCoreAST_Expression*  function        = ast;
    size_t                    arg_count       = 0;
    while (function->expr_type == NECRO_CORE_EXPR_APP)
    {
        arg_count++;
        function = function->app.exprA;
    }
    assert(function->expr_type == NECRO_CORE_EXPR_VAR);
    assert(function->var.id.id == program->dyn_state_con.id.id);
    assert(arg_count == 2);
    NecroCoreAST_Expression* fn     = ast->app.exprA->app.exprB;
    assert(fn->expr_type == NECRO_CORE_EXPR_VAR);
    NecroMachineAST* machine_def = necro_symtable_get(program->symtable, fn->var.id)->necro_machine_ast;
    NecroMachineAST* mk_fn       = machine_def->machine_def.mk_fn->fn_def.fn_value;
    size_t           data_id     = machine_def->machine_def.data_id;
    assert(mk_fn != NULL);
    assert(data_id != 0);
    NecroMachineAST* mk_fn_ptr  = necro_create_global_value(program, mk_fn->value.global_name, necro_create_machine_ptr_type(&program->arena, mk_fn->necro_machine_type));
    NecroMachineAST* dyn_state  = necro_build_call(program, outer->machine_def.update_fn, necro_symtable_get(program->symtable, program->dyn_state_con.id)->necro_machine_ast, (NecroMachineAST*[]) { mk_fn_ptr, necro_create_word_uint_value(program, data_id) }, 2, NECRO_LANG_CALL, "dyn_state");
    return dyn_state;
}

NecroMachineAST* necro_get_closure_con(NecroMachineProgram* program, size_t closure_arity)
{
    assert(program != NULL);
    assert(closure_arity >= NECRO_NUM_CLOSURE_PRE_ARGS);
    size_t adjusted_closure_arity = closure_arity - NECRO_NUM_CLOSURE_PRE_ARGS;
    while (program->closure_cons.length < (adjusted_closure_arity + 1))
    {
        NecroMachineAST* null_ast = NULL;
        necro_push_closure_con_vector(&program->closure_cons, &null_ast);

    }
    while (program->closure_types.length < (closure_arity + 1))
    {
        NecroMachineType* null_type = NULL;
        necro_push_closure_type_vector(&program->closure_types, &null_type);
    }
    if (program->closure_cons.data[adjusted_closure_arity] != NULL)
        return program->closure_cons.data[adjusted_closure_arity];
    assert(program->closure_type != NULL);
    assert(program->closure_type->type == NECRO_MACHINE_TYPE_STRUCT);

    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineType*  struct_ptr_type  = necro_create_machine_ptr_type(&program->arena, program->closure_type);
    assert(adjusted_closure_arity < INT32_MAX);
    const char*        num_string       = itoa((int32_t) adjusted_closure_arity, necro_snapshot_arena_alloc(&program->snapshot_arena, 10), 10);
    const char*        struct_name      = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_Closure", num_string });
    const char*        mk_fn_name       = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkClosure", num_string });
    // const char*        const_mk_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkConstClosure", num_string });
    NecroVar           struct_var       = necro_gen_var(program, NULL, struct_name, NECRO_NAME_UNIQUE);
    NecroVar           mk_fn_var        = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    // NecroVar           const_mk_fn_var  = necro_gen_var(program, NULL, const_mk_fn_name, NECRO_NAME_UNIQUE);

    NecroMachineType** elems            = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_arity * sizeof(NecroMachineType*));
    elems[0]                            = program->necro_int_type;
    elems[1]                            = program->necro_int_type;
    elems[2]                            = necro_create_machine_ptr_type(&program->arena, program->dyn_state_type);
    elems[3]                            = program->necro_poly_ptr_type;
    for (size_t parg = 4; parg < closure_arity; ++parg)
    {
        elems[parg] = program->necro_poly_ptr_type;
    }
    NecroMachineAST* actual_type               = necro_create_machine_struct_def(program, struct_var, elems, closure_arity);
    program->closure_types.data[closure_arity] = actual_type->necro_machine_type;

    // for (size_t c = 0; c < 2; ++c)
    // {
        // mk_fn_var                     = (c == 0) ? mk_fn_var : const_mk_fn_var;
        mk_fn_var                     = mk_fn_var;

        NecroMachineType*  mk_fn_type = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems, closure_arity);
        NecroMachineAST*   mk_fn_body = necro_create_machine_block(program, "entry", NULL);
        NecroMachineAST*   mk_fn_def  = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
        assert(closure_arity < UINT32_MAX);
        NecroMachineAST*   data_ptr   = necro_build_nalloc(program, mk_fn_def, actual_type->necro_machine_type, (uint32_t) closure_arity);
        // if (c == 0)
            mk_fn_def->fn_def.state_type = NECRO_STATE_CONSTANT;
        // else
        //     mk_fn_def->fn_def.state_type = NECRO_STATE_POINTWISE;

        //--------------
        // Parameters
        for (size_t i = 0; i < closure_arity; ++i)
        {
            necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i);
        }

        NecroMachineAST* bit_cast_value = necro_build_bit_cast(program, mk_fn_def, data_ptr, struct_ptr_type);
        necro_build_return(program, mk_fn_def, bit_cast_value);

        // TODO: Un-Fuck-up-ify closure storage from constant closure mess
        // if (c == 0)
            program->closure_cons.data[adjusted_closure_arity] = mk_fn_def->fn_def.fn_value;
        // else
        //     program->closure_cons.data[(adjusted_closure_arity * 2) + 1] = mk_fn_def->fn_def.fn_value;
    // }
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return program->closure_cons.data[adjusted_closure_arity];
}

NecroMachineAST* necro_create_get_apply_state_fn(NecroMachineProgram* program)
{
    if (program->get_apply_state_fn != NULL)
        return program->get_apply_state_fn;
    NecroMachineType* fn_type       = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, (NecroMachineType*[]) { necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type), necro_create_machine_ptr_type(&program->arena, program->necro_uint_type), program->necro_poly_ptr_type, program->necro_uint_type }, 4);
    NecroMachineAST*  mk_state      = necro_create_machine_block(program, "mk_state", NULL);
    NecroMachineAST*  load_state    = necro_create_machine_block(program, "load_state", mk_state);
    NecroMachineAST*  entry         = necro_create_machine_block(program, "entry", load_state);
    NecroMachineAST*  fn            = necro_create_machine_fn(program, necro_gen_var(program, NULL, "_getApplyState", NECRO_NAME_UNIQUE), entry, fn_type);

    // Entry
    necro_build_debug_print(program, fn, 1000, NECRO_DEBUG_PRINT_CLOSURES);
    NecroMachineAST*  apply_data_id = necro_build_load_from_ptr(program, fn, necro_create_param_reg(program, fn, 1), "apply_data_id");
    NecroMachineAST*  should_load   = necro_build_cmp(program, fn, NECRO_MACHINE_CMP_EQ, apply_data_id, necro_create_param_reg(program, fn, 3));
    necro_build_cond_break(program, fn, should_load, load_state, mk_state);

    // Load State
    necro_move_to_block(program, fn, load_state);
    necro_build_debug_print(program, fn, 1010, NECRO_DEBUG_PRINT_CLOSURES);
    NecroMachineAST*  loaded_state  = necro_build_load_from_ptr(program, fn, necro_create_param_reg(program, fn, 0), "loaded_state");
    necro_build_debug_print(program, fn, 1011, NECRO_DEBUG_PRINT_CLOSURES);
    necro_build_return(program, fn, loaded_state);

    // Mk State
    necro_move_to_block(program, fn, mk_state);
    necro_build_debug_print(program, fn, 1020, NECRO_DEBUG_PRINT_CLOSURES);
    NecroMachineType* mk_fn_type     = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, NULL, 0);
    NecroMachineAST*  bit_cast_mk_fn = necro_build_bit_cast(program, fn, necro_create_param_reg(program, fn, 2), necro_create_machine_ptr_type(&program->arena, mk_fn_type));
    necro_build_debug_print(program, fn, 1021, NECRO_DEBUG_PRINT_CLOSURES);
    NecroMachineAST*  new_state      = necro_build_call(program, fn, bit_cast_mk_fn, NULL, 0, NECRO_LANG_CALL, "new_state");
    necro_build_debug_print(program, fn, 1022, NECRO_DEBUG_PRINT_CLOSURES);
    necro_build_store_into_ptr(program, fn, new_state, necro_create_param_reg(program, fn, 0));
    necro_build_debug_print(program, fn, 1023, NECRO_DEBUG_PRINT_CLOSURES);
    necro_build_store_into_ptr(program, fn, necro_create_param_reg(program, fn, 3), necro_create_param_reg(program, fn, 1));
    necro_build_debug_print(program, fn, 1024, NECRO_DEBUG_PRINT_CLOSURES);
    necro_build_return(program, fn, new_state);

    // Finish
    program->get_apply_state_fn = fn->fn_def.fn_value;
    return program->get_apply_state_fn;
}

void necro_declare_apply_fn(NecroMachineProgram* program, size_t apply_arity)
{
    assert(apply_arity > 0);
    assert(program != NULL);
    size_t adjusted_apply_arity = apply_arity - 1;
    size_t arity_with_state     = apply_arity + 1;
    while (program->apply_fns.length < adjusted_apply_arity + 1)
    {
        NecroMachineAST* null_ast = NULL;
        necro_push_apply_fn_vector(&program->apply_fns, &null_ast);
    }
    if (program->apply_fns.data[adjusted_apply_arity] != NULL)
        return;
    assert(program->closure_type != NULL);
    assert(program->closure_type->type == NECRO_MACHINE_TYPE_STRUCT);
    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&program->snapshot_arena);

    //--------------------
    // apply non-state update_fn type
    NecroMachineType*  closure_ptr_type = necro_create_machine_ptr_type(&program->arena, program->closure_type);
    NecroMachineType** elems_non_state  = necro_snapshot_arena_alloc(&program->snapshot_arena, apply_arity * sizeof(NecroMachineType*));
    elems_non_state[0]                  = closure_ptr_type;
    for (size_t arg = 1; arg < apply_arity; ++arg)
    {
        elems_non_state[arg] = program->necro_poly_ptr_type;
    }
    NecroMachineType* non_state_apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems_non_state, apply_arity);

    //--------------------
    // apply MachineDef
    assert(adjusted_apply_arity <= INT32_MAX);
    const char*       num_string                 = itoa((int32_t) adjusted_apply_arity, necro_snapshot_arena_alloc(&program->snapshot_arena, 10), 10);
    const char*       apply_machine_name         = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "apply", num_string });
    NecroVar          apply_machine_var          = necro_gen_var(program, NULL, apply_machine_name, NECRO_NAME_UNIQUE);
    NecroMachineAST*  apply_machine_def          = necro_create_machine_initial_machine_def(program, apply_machine_var, NULL, non_state_apply_fn_type, NULL);
    apply_machine_def->machine_def.num_arg_names = apply_arity;

    //--------------------
    // apply MachineDef state
    const char*       apply_machine_state_name  = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_ApplyMachineState", num_string });
    NecroVar          apply_machine_state_var   = necro_gen_var(program, NULL, apply_machine_state_name, NECRO_NAME_UNIQUE);
    NecroMachineType* apply_machine_def_type    = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
    apply_machine_def_type->struct_type.members[2] = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
    apply_machine_def->necro_machine_type       = apply_machine_def_type;
    apply_machine_def->machine_def.data_id      = NECRO_APPLY_DATA_ID;
    apply_machine_def->machine_def.state_type   = NECRO_STATE_STATEFUL;
    apply_machine_def->machine_def.is_recursive = true;
    apply_machine_def->machine_def.num_members  = 3;

    NecroMachineType* machine_ptr_type = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
    //--------------------
    // Apply init_fn
    {
        NecroMachineType*  init_fn_type    = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { machine_ptr_type }, 1);
        NecroMachineAST*   init_fn_body    = necro_create_machine_block(program, "entry", NULL);
        const char*        init_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_initApply", num_string });
        NecroVar           init_fn_var     = necro_gen_var(program, NULL, init_fn_name, NECRO_NAME_UNIQUE);
        NecroMachineAST*   init_fn_def     = necro_create_machine_fn(program, init_fn_var, init_fn_body, init_fn_type);
        program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
        apply_machine_def->machine_def.init_fn = init_fn_def;
        necro_build_store_into_slot(program, init_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), necro_create_param_reg(program, init_fn_def, 0), 1);
        necro_build_store_into_slot(program, init_fn_def, necro_create_null_necro_machine_value(program, machine_ptr_type), necro_create_param_reg(program, init_fn_def, 0), 2);
        necro_build_return_void(program, init_fn_def);
        necro_symtable_get(program->symtable, init_fn_var.id)->necro_machine_ast = init_fn_def->fn_def.fn_value;
    }

    //--------------------
    // Apply mk_fn
    {
        NecroMachineType*  mk_fn_type        = necro_create_machine_fn_type(&program->arena, machine_ptr_type, NULL, 0);
        NecroMachineAST*   mk_fn_body        = necro_create_machine_block(program, "entry", NULL);
        const char*        mk_fn_name        = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkApply", num_string });
        NecroVar           mk_fn_var         = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
        NecroMachineAST*   mk_fn_def         = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
        program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
        apply_machine_def->machine_def.mk_fn = mk_fn_def;
        NecroMachineAST*   data_ptr          = necro_build_nalloc(program, mk_fn_def, apply_machine_def_type, 3);
        necro_build_call(program, mk_fn_def, apply_machine_def->machine_def.init_fn->fn_def.fn_value, (NecroMachineAST*[]) { data_ptr }, 1, NECRO_LANG_CALL, "");
        necro_build_return(program, mk_fn_def, data_ptr);
        necro_symtable_get(program->symtable, mk_fn_var.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
    }

    //--------------------
    // apply update_fn
    const char*        apply_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_updateApply", num_string });
    NecroVar           apply_fn_var     = necro_gen_var(program, NULL, apply_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType** elems            = necro_snapshot_arena_alloc(&program->snapshot_arena, arity_with_state * sizeof(NecroMachineType*));
    elems[0]                            = necro_create_machine_ptr_type(&program->arena, apply_machine_def->necro_machine_type);
    elems[1]                            = closure_ptr_type;
    for (size_t arg = 2; arg < arity_with_state; ++arg)
    {
        elems[arg] = program->necro_poly_ptr_type;
    }
    NecroMachineType* apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems, arity_with_state);
    NecroMachineAST*  apply_fn_body = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*  apply_fn_def  = necro_create_machine_fn(program, apply_fn_var, apply_fn_body, apply_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
    apply_fn_def->fn_def.state_type = NECRO_STATE_STATEFUL;

    //--------------------
    // Apply Fn Body

    necro_build_debug_print(program, apply_fn_def, 100, NECRO_DEBUG_PRINT_CLOSURES);
    NecroMachineAST* init_block     = necro_append_block(program, apply_fn_def, "init");
    NecroMachineAST* cont_block     = necro_append_block(program, apply_fn_def, "cont");
    NecroMachineAST* child_ptr      = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 2, "child");
    NecroMachineAST* cmp_result     = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_EQ, child_ptr, necro_create_null_necro_machine_value(program, child_ptr->necro_machine_type));
    necro_build_cond_break(program, apply_fn_def, cmp_result, init_block, cont_block);

    //--------------------
    // Apply init
    necro_move_to_block(program, apply_fn_def, init_block);
    necro_build_debug_print(program, apply_fn_def, -100, NECRO_DEBUG_PRINT_CLOSURES);
    NecroMachineAST* data_ptr = necro_build_call(program, apply_fn_def, apply_machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "data_ptr");
    necro_build_store_into_slot(program, apply_fn_def, data_ptr, necro_create_param_reg(program, apply_fn_def, 0), 2);
    necro_build_break(program, apply_fn_def, cont_block);

    //--------------------
    // Closure Arity Blocks
    necro_move_to_block(program, apply_fn_def, cont_block);
    necro_build_debug_print(program, apply_fn_def, 200, NECRO_DEBUG_PRINT_CLOSURES);
    size_t                  num_closure_types    = program->closure_defs.length * 2;
    NecroMachineAST**       closure_arity_blocks = necro_snapshot_arena_alloc(&program->snapshot_arena, num_closure_types * sizeof(NecroMachineAST*));
    NecroMachineSwitchList* fn_arity_switch_list = NULL;
    assert(program->closure_defs.length > 0);
    for (size_t i = 0; i < program->closure_defs.length; ++i)
    {
        char        itoa_buf1[10];
        char        itoa_buf2[10];
        size_t      closure_def_fn_arity = program->closure_defs.data[i].fn_arity;
        size_t      closure_def_pargs    = program->closure_defs.data[i].num_pargs;
        // Non-stateful
        {
            assert(closure_def_fn_arity <= INT32_MAX);
            assert(closure_def_pargs <= INT32_MAX);
            const char* block_name           = necro_concat_strings(&program->snapshot_arena, 5, (const char*[]) { "closure_arity_", itoa((int32_t) closure_def_fn_arity, itoa_buf1, 10), "_pargs_", itoa((int32_t) closure_def_pargs, itoa_buf2, 10), "_" });
            closure_arity_blocks[i]          = necro_append_block(program, apply_fn_def, block_name);
            size_t      closure_switch_val   = (closure_def_fn_arity << 16) | closure_def_pargs;
            fn_arity_switch_list             = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = closure_arity_blocks[i], .value = closure_switch_val }, fn_arity_switch_list);
        }
        // Stateful
        {
            assert(closure_def_fn_arity <= INT32_MAX);
            assert(closure_def_pargs <= INT32_MAX);
            const char* block_name           = necro_concat_strings(&program->snapshot_arena, 5, (const char*[]) { "closure_arity_", itoa((int32_t) closure_def_fn_arity, itoa_buf1, 10), "_pargs_", itoa((int32_t) closure_def_pargs, itoa_buf2, 10), "_Stateful" });
            closure_arity_blocks[i + program->closure_defs.length] = necro_append_block(program, apply_fn_def, block_name);
            size_t      closure_switch_val   = (1 << 31) | (closure_def_fn_arity << 16) | closure_def_pargs;
            fn_arity_switch_list             = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = closure_arity_blocks[i + program->closure_defs.length], .value = closure_switch_val }, fn_arity_switch_list);
        }
    }
    NecroMachineAST* error               = necro_append_block(program, apply_fn_def, "error");
    NecroMachineAST* closure_fn_arity    = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 0, "closure_fn_arity");
    NecroMachineAST* u_closure_fn_arity  = necro_build_bit_cast(program, apply_fn_def, closure_fn_arity, program->necro_uint_type);
    NecroMachineAST* closure_num_pargs   = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 1, "closure_num_pargs");
    NecroMachineAST* u_closure_num_pargs = necro_build_bit_cast(program, apply_fn_def, closure_num_pargs, program->necro_uint_type);
    NecroMachineAST* shifted_fn_arity    = necro_build_binop(program, apply_fn_def, u_closure_fn_arity, necro_create_word_uint_value(program, 16), NECRO_MACHINE_BINOP_SHL);
    NecroMachineAST* dyn_state           = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 2, "dyn_state");
    NecroMachineAST* is_stateful         = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_NE, dyn_state, necro_create_null_necro_machine_value(program, dyn_state->necro_machine_type));
    NecroMachineAST* shifted_is_stateful = necro_build_binop(program, apply_fn_def, necro_build_zext(program, apply_fn_def, is_stateful, program->necro_uint_type), necro_create_word_uint_value(program, 31), NECRO_MACHINE_BINOP_SHL);
    NecroMachineAST* closure_type        = necro_build_binop(program, apply_fn_def, shifted_fn_arity, u_closure_num_pargs, NECRO_MACHINE_BINOP_OR);
    closure_type                         = necro_build_binop(program, apply_fn_def, closure_type, shifted_is_stateful, NECRO_MACHINE_BINOP_OR);
    NecroMachineAST* closure_fn_poly     = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 3, "closure_fn_poly");
    // necro_build_debug_print_value(program, apply_fn_def, closure_type);
    necro_build_switch(program, apply_fn_def, closure_type, fn_arity_switch_list, error);

    for (size_t c_stateful = 0; c_stateful < (program->closure_defs.length * 2); ++c_stateful)
    {
        const size_t     c                    = c_stateful % program->closure_defs.length;
        const bool       closure_is_stateful  = c_stateful >= program->closure_defs.length;
        NecroArenaSnapshot closure_snapshot   = necro_get_arena_snapshot(&program->snapshot_arena);
        necro_move_to_block(program, apply_fn_def, closure_arity_blocks[c_stateful]);
        const size_t     closure_def_fn_arity = program->closure_defs.data[c].fn_arity;
        const size_t     closure_def_pargs    = program->closure_defs.data[c].num_pargs;
        const size_t     num_apply_args       = adjusted_apply_arity;
        const size_t     num_total_args       = closure_def_pargs + num_apply_args;
        NecroMachineAST* closure_con          = necro_get_closure_con(program, NECRO_NUM_CLOSURE_PRE_ARGS + closure_def_pargs);
        UNUSED(closure_con);
        NecroMachineAST* closure_ptr          = necro_build_bit_cast(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), necro_create_machine_ptr_type(&program->arena, program->closure_types.data[closure_def_pargs + NECRO_NUM_CLOSURE_PRE_ARGS]));
        // TODO: Statefuleness!
        // TODO: Correct GC!

        // x - N cases
        // Under Saturated, Non-Stateful and Stateful
        if (num_total_args < closure_def_fn_arity)
        {
            necro_build_debug_print(program, apply_fn_def, 300, NECRO_DEBUG_PRINT_CLOSURES);
            if (!closure_is_stateful)
            {
                necro_build_store_into_slot(program, apply_fn_def, necro_create_word_uint_value(program, 0), necro_create_param_reg(program, apply_fn_def, 0), 0);
                necro_build_store_into_slot(program, apply_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), necro_create_param_reg(program, apply_fn_def, 0), 1);
            }
            necro_build_debug_print(program, apply_fn_def, 301, NECRO_DEBUG_PRINT_CLOSURES);
            NecroMachineAST*  new_closure_con = necro_get_closure_con(program, NECRO_NUM_CLOSURE_PRE_ARGS + num_total_args);
            NecroMachineAST** call_args       = necro_snapshot_arena_alloc(&program->snapshot_arena, (NECRO_NUM_CLOSURE_PRE_ARGS + num_total_args) * sizeof(NecroMachineAST*));
            call_args[0] = closure_fn_arity;
            call_args[1] = necro_create_word_int_value(program, num_total_args);
            call_args[2] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, 2, "closure_dyn_state");
            call_args[3] = closure_fn_poly;
            for (size_t i = NECRO_NUM_CLOSURE_PRE_ARGS; i < (NECRO_NUM_CLOSURE_PRE_ARGS + closure_def_pargs); ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[NECRO_NUM_CLOSURE_PRE_ARGS + i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineAST*  closure_result    = necro_build_call(program, apply_fn_def, new_closure_con, call_args, NECRO_NUM_CLOSURE_PRE_ARGS + num_total_args, NECRO_LANG_CALL, "result");
            necro_build_return(program, apply_fn_def, closure_result);
        }

        // Saturated, Non-Stateful
        else if (num_total_args == closure_def_fn_arity && !closure_is_stateful)
        {
            necro_build_debug_print(program, apply_fn_def, 400, NECRO_DEBUG_PRINT_CLOSURES);
            necro_build_store_into_slot(program, apply_fn_def, necro_create_word_uint_value(program, 0), necro_create_param_reg(program, apply_fn_def, 0), 0);
            necro_build_store_into_slot(program, apply_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), necro_create_param_reg(program, apply_fn_def, 0), 1);
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, NECRO_NUM_CLOSURE_PRE_ARGS + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType*  closure_fn_type   = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
            NecroMachineAST*   closure_fn        = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            NecroMachineAST*   closure_result    = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, NECRO_LANG_CALL, "result");
            necro_build_debug_print(program, apply_fn_def, 401, NECRO_DEBUG_PRINT_CLOSURES);
            necro_build_return(program, apply_fn_def, closure_result);
        }

        // TODO: NULL out state when we go to Non-Stateful version!
        // TODO: TEST this!

        // Saturated, Stateful
        else if (num_total_args == closure_def_fn_arity && closure_is_stateful)
        {
            necro_build_debug_print(program, apply_fn_def, 500, NECRO_DEBUG_PRINT_CLOSURES);
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (closure_def_fn_arity + 1) * sizeof(NecroMachineAST*));

            // Get State
            NecroMachineAST* get_apply_state_fn = necro_create_get_apply_state_fn(program);
            NecroMachineAST* apply_state_ptr    = necro_build_gep(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), (uint32_t[]) { 0, 1 }, 2, "apply_state_ptr");
            necro_build_debug_print(program, apply_fn_def, 501, NECRO_DEBUG_PRINT_CLOSURES);
            NecroMachineAST* apply_id_ptr       = necro_build_gep(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), (uint32_t[]) { 0, 0 }, 2, "apply_id_ptr");
            necro_build_debug_print(program, apply_fn_def, 502, NECRO_DEBUG_PRINT_CLOSURES);
            NecroMachineAST* closure_dyn_state  = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 2, "closure_dyn_state");
            necro_build_debug_print(program, apply_fn_def, 503, NECRO_DEBUG_PRINT_CLOSURES);
            NecroMachineAST* closure_mk_fn      = necro_build_load_from_slot(program, apply_fn_def, closure_dyn_state, 1, "closure_mk_dyn_state");
            necro_build_debug_print(program, apply_fn_def, 504, NECRO_DEBUG_PRINT_CLOSURES);
            NecroMachineAST* closure_id         = necro_build_load_from_slot(program, apply_fn_def, closure_dyn_state, 2, "closure_id");
            necro_build_debug_print(program, apply_fn_def, 505, NECRO_DEBUG_PRINT_CLOSURES);
            call_args[0]                        = necro_build_call(program, apply_fn_def, get_apply_state_fn, (NecroMachineAST*[]) { apply_state_ptr, apply_id_ptr, closure_mk_fn, closure_id }, 4, NECRO_LANG_CALL, "dyn_state");

            necro_build_debug_print(program, apply_fn_def, 506, NECRO_DEBUG_PRINT_CLOSURES);
            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i + 1] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, NECRO_NUM_CLOSURE_PRE_ARGS + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + closure_def_pargs + 1] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, (closure_def_fn_arity + 1) * sizeof(NecroMachineType*));
            for (size_t i = 0; i < (closure_def_fn_arity + 1); ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType*  closure_fn_type   = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity + 1);
            NecroMachineAST*   closure_fn        = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            NecroMachineAST*   closure_result    = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity + 1, NECRO_LANG_CALL, "result");
            necro_build_debug_print(program, apply_fn_def, 507, NECRO_DEBUG_PRINT_CLOSURES);
            necro_build_return(program, apply_fn_def, closure_result);
        }

        // Over Saturated, Not-Stateful
        else if (!closure_is_stateful)
        {
            necro_build_debug_print(program, apply_fn_def, 600, NECRO_DEBUG_PRINT_CLOSURES);
            necro_build_store_into_slot(program, apply_fn_def, necro_create_word_uint_value(program, 0), necro_create_param_reg(program, apply_fn_def, 0), 0);
            necro_build_store_into_slot(program, apply_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), necro_create_param_reg(program, apply_fn_def, 0), 1);
            //-------------------
            // Call closure fn
            const size_t num_extra_args = num_total_args - closure_def_fn_arity;
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, NECRO_NUM_CLOSURE_PRE_ARGS + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType* closure_fn_type    = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
            NecroMachineAST*  closure_fn         = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            NecroMachineAST*  closure_result     = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, NECRO_LANG_CALL, "result");
            necro_build_debug_print(program, apply_fn_def, 601, NECRO_DEBUG_PRINT_CLOSURES);
            //-------------------
            // Call overapply fn
            NecroMachineAST*  over_apply_fn   = necro_get_apply_fn(program, num_extra_args + 1);
            NecroMachineAST** over_apply_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (num_extra_args + 2) * sizeof(NecroMachineAST*));
            NecroMachineAST*  nested_data_ptr = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 2, "nested_state");
            over_apply_args[0]                = necro_build_bit_cast(program, apply_fn_def, nested_data_ptr, program->necro_poly_ptr_type);
            over_apply_args[1]                = closure_result;
            for (size_t i = 0; i < num_extra_args; ++i)
                over_apply_args[i + 2] = necro_create_param_reg(program, apply_fn_def, i + (closure_def_fn_arity - closure_def_pargs) + 2);
            NecroMachineAST* over_result      = necro_build_call(program, apply_fn_def, over_apply_fn->machine_def.update_fn->fn_def.fn_value, over_apply_args, num_extra_args + 2, NECRO_LANG_CALL, "over_result");
            necro_build_debug_print(program, apply_fn_def, 602, NECRO_DEBUG_PRINT_CLOSURES);
            necro_build_return(program, apply_fn_def, over_result);
        }

        // Over Saturated, Stateful
        else
        {
            necro_build_debug_print(program, apply_fn_def, 700, NECRO_DEBUG_PRINT_CLOSURES);
            //-------------------
            // Call closure fn
            const size_t num_extra_args = num_total_args - closure_def_fn_arity;
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (closure_def_fn_arity + 1) * sizeof(NecroMachineAST*));

            // Get State
            NecroMachineAST* get_apply_state_fn = necro_create_get_apply_state_fn(program);
            NecroMachineAST* apply_state_ptr    = necro_build_gep(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), (uint32_t[]) { 0, 1 }, 2, "apply_state_ptr");
            NecroMachineAST* apply_id_ptr       = necro_build_gep(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), (uint32_t[]) { 0, 0 }, 2, "apply_id_ptr");
            NecroMachineAST* closure_dyn_state  = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 2, "closure_dyn_state");
            NecroMachineAST* closure_mk_fn      = necro_build_load_from_slot(program, apply_fn_def, closure_dyn_state, 1, "closure_mk_dyn_state");
            NecroMachineAST* closure_id         = necro_build_load_from_slot(program, apply_fn_def, closure_dyn_state, 2, "closure_id");
            call_args[0]                        = necro_build_call(program, apply_fn_def, get_apply_state_fn, (NecroMachineAST*[]) { apply_state_ptr, apply_id_ptr, closure_mk_fn, closure_id }, 4, NECRO_LANG_CALL, "dyn_state");
            necro_build_debug_print(program, apply_fn_def, 701, NECRO_DEBUG_PRINT_CLOSURES);

            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i + 1] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, NECRO_NUM_CLOSURE_PRE_ARGS + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + 1 + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, (closure_def_fn_arity + 1) * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity + 1; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType* closure_fn_type    = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity + 1);
            NecroMachineAST*  closure_fn         = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            NecroMachineAST*  closure_result     = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity + 1, NECRO_LANG_CALL, "result");
            necro_build_debug_print(program, apply_fn_def, 702, NECRO_DEBUG_PRINT_CLOSURES);
            //-------------------
            // Call overapply fn
            NecroMachineAST*  over_apply_fn   = necro_get_apply_fn(program, num_extra_args + 1);
            NecroMachineAST** over_apply_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (num_extra_args + 2) * sizeof(NecroMachineAST*));
            NecroMachineAST*  nested_data_ptr = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 2, "nested_state");
            over_apply_args[0]                = necro_build_bit_cast(program, apply_fn_def, nested_data_ptr, program->necro_poly_ptr_type);
            over_apply_args[1]                = closure_result;
            for (size_t i = 0; i < num_extra_args; ++i)
                over_apply_args[i + 2] = necro_create_param_reg(program, apply_fn_def, i + (closure_def_fn_arity - closure_def_pargs) + 2);
            NecroMachineAST* over_result      = necro_build_call(program, apply_fn_def, over_apply_fn->machine_def.update_fn->fn_def.fn_value, over_apply_args, num_extra_args + 2, NECRO_LANG_CALL, "over_result");
            necro_build_debug_print(program, apply_fn_def, 703, NECRO_DEBUG_PRINT_CLOSURES);
            necro_build_return(program, apply_fn_def, over_result);
        }

        necro_rewind_arena(&program->snapshot_arena, closure_snapshot);
    }

    //--------------------
    // Error Block
    necro_move_to_block(program, apply_fn_def, error);
    necro_build_call(program, apply_fn_def, necro_symtable_get(program->symtable, program->runtime._necro_error_exit.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_uint32_necro_machine_value(program, 2) }, 1, NECRO_C_CALL, "");
    necro_build_unreachable(program, apply_fn_def);

    program->apply_fns.data[adjusted_apply_arity] = apply_machine_def;
    apply_machine_def->machine_def.update_fn = apply_fn_def;

    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

NecroMachineAST* necro_get_apply_fn(NecroMachineProgram* program, size_t apply_arity)
{
    assert(apply_arity > 0);
    size_t adjusted_apply_arity = apply_arity - 1;
    if (program->apply_fns.length > adjusted_apply_arity && program->apply_fns.data[adjusted_apply_arity] != NULL)
        return program->apply_fns.data[adjusted_apply_arity];
    necro_declare_apply_fn(program, apply_arity);
    assert(program->apply_fns.length > adjusted_apply_arity);
    assert(program->apply_fns.data[adjusted_apply_arity] != NULL);
    return program->apply_fns.data[adjusted_apply_arity];
}

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
        * declare apply_fn in machine_prim.c
        * Static array machine_ast gen
        * Actual apply function body
        * Stateful closures
        * NULL out apply state
        * Check that recursive dynamic states are correctly NULL'ed out (maybe just go ahead and pre-emptively NULL out everything....)
        * Correct GC handling for apply data_ids!
*/

NecroMachineAST* necro_get_closure_con(NecroMachineProgram* program, size_t closure_arity, bool is_constant)
{
    assert(program != NULL);
    assert(closure_arity >= 3);
    size_t adjusted_closure_arity = closure_arity - 3;
    while (program->closure_cons.length < ((adjusted_closure_arity * 2) + 2))
    {
        NecroMachineAST* null_ast = NULL;
        necro_push_closure_con_vector(&program->closure_cons, &null_ast);

    }
    while (program->closure_types.length < (closure_arity + 1))
    {
        NecroMachineType* null_type = NULL;
        necro_push_closure_type_vector(&program->closure_types, &null_type);
    }
    if (program->closure_cons.data[(adjusted_closure_arity * 2) + is_constant] != NULL)
        return program->closure_cons.data[(adjusted_closure_arity * 2) + is_constant];
    assert(program->closure_type != NULL);
    assert(program->closure_type->type == NECRO_MACHINE_TYPE_STRUCT);

    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineType*  struct_ptr_type  = necro_create_machine_ptr_type(&program->arena, program->closure_type);
    const char*        num_string       = itoa(adjusted_closure_arity, necro_snapshot_arena_alloc(&program->snapshot_arena, 10), 10);
    const char*        struct_name      = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_Closure", num_string });
    const char*        mk_fn_name       = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkClosure", num_string });
    const char*        const_mk_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkConstClosure", num_string });
    NecroVar           struct_var       = necro_gen_var(program, NULL, struct_name, NECRO_NAME_UNIQUE);
    NecroVar           mk_fn_var        = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroVar           const_mk_fn_var  = necro_gen_var(program, NULL, const_mk_fn_name, NECRO_NAME_UNIQUE);

    NecroMachineType** elems            = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_arity * sizeof(NecroMachineType*));
    elems[0]                            = program->necro_int_type;
    elems[1]                            = program->necro_int_type;
    elems[2]                            = program->necro_poly_ptr_type;
    for (size_t parg = 3; parg < closure_arity; ++parg)
    {
        elems[parg] = program->necro_poly_ptr_type;
    }
    NecroMachineAST* actual_type               = necro_create_machine_struct_def(program, struct_var, elems, closure_arity);
    program->closure_types.data[closure_arity] = actual_type->necro_machine_type;

    for (size_t c = 0; c < 2; ++c)
    {
        mk_fn_var                     = (c == 0) ? mk_fn_var : const_mk_fn_var;

        NecroMachineType*  mk_fn_type = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems, closure_arity);
        NecroMachineAST*   mk_fn_body = necro_create_machine_block(program, "entry", NULL);
        NecroMachineAST*   mk_fn_def  = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
        NecroMachineAST*   data_ptr   = necro_build_nalloc(program, mk_fn_def, actual_type->necro_machine_type, closure_arity, c == 1);
        if (c == 0)
            mk_fn_def->fn_def.state_type = NECRO_STATE_CONSTANT;
        else
            mk_fn_def->fn_def.state_type = NECRO_STATE_POINTWISE;

        //--------------
        // Parameters
        for (size_t i = 0; i < closure_arity; ++i)
        {
            necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i);
        }

        NecroMachineAST* bit_cast_value = necro_build_bit_cast(program, mk_fn_def, data_ptr, struct_ptr_type);
        necro_build_return(program, mk_fn_def, bit_cast_value);

        if (c == 0)
            program->closure_cons.data[adjusted_closure_arity * 2] = mk_fn_def->fn_def.fn_value;
        else
            program->closure_cons.data[(adjusted_closure_arity * 2) + 1] = mk_fn_def->fn_def.fn_value;
    }
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return program->closure_cons.data[(adjusted_closure_arity * 2) + is_constant];
}

// void necro_declare_apply_fn(NecroMachineProgram* program)
// {
//     assert(program != NULL);
//     assert(program->closure_type != NULL);
//     assert(program->closure_type->type == NECRO_MACHINE_TYPE_STRUCT);
//     NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&program->snapshot_arena);

//     //--------------------
//     // apply non-state update_fn type
//     NecroMachineType* closure_ptr_type        = necro_create_machine_ptr_type(&program->arena, program->closure_type);
//     NecroMachineType* non_state_apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, (NecroMachineType*[]) { closure_ptr_type, program->necro_int_type, necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type) }, 3);

//     //--------------------
//     // apply MachineDef
//     NecroVar          apply_var                  = necro_con_to_var(program->prim_types->apply_fn);
//     NecroMachineAST*  apply_machine_def          = necro_create_machine_initial_machine_def(program, apply_var, NULL, non_state_apply_fn_type, NULL);
//     apply_machine_def->machine_def.num_arg_names = 2;

//     //--------------------
//     // apply MachineDef state
//     NecroVar          apply_machine_state_var  = necro_gen_var(program, NULL, "_ApplyMachineState", NECRO_NAME_UNIQUE);
//     NecroMachineType* apply_machine_def_type   = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, (NecroMachineType*[]) { program->necro_int_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
//     apply_machine_def_type->struct_type.members[2] = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
//     apply_machine_def->necro_machine_type      = apply_machine_def_type;
//     apply_machine_def->machine_def.data_id     = NECRO_APPLY_DATA_ID;
//     apply_machine_def->machine_def.state_type  = NECRO_STATE_STATEFUL;

//     //--------------------
//     // apply update_fn
//     NecroVar           apply_fn_var = necro_gen_var(program, NULL, "_apply_update_fn", NECRO_NAME_UNIQUE);
//     NecroMachineType** elems        = necro_snapshot_arena_alloc(&program->snapshot_arena, 3 * sizeof(NecroMachineType*));
//     elems[0]                        = necro_create_machine_ptr_type(&program->arena, apply_machine_def->necro_machine_type);
//     elems[1]                        = closure_ptr_type;
//     elems[2]                        = program->necro_int_type;
//     elems[3]                        = necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type);
//     NecroMachineType* apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems, 4);
//     NecroMachineAST*  apply_fn_body = necro_create_machine_block(program, "entry", NULL);
//     NecroMachineAST*  apply_fn_def  = necro_create_machine_fn(program, apply_fn_var, apply_fn_body, apply_fn_type);
//     program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
//     apply_fn_def->fn_def.state_type = NECRO_STATE_STATEFUL;
//     NecroMachineAST*  loop_block    = necro_append_block(program, apply_fn_def, "loop");
//     NecroMachineAST*  less_than     = necro_append_block(program, apply_fn_def, "less_than");
//     NecroMachineAST*  saturated     = necro_append_block(program, apply_fn_def, "saturated");
//     NecroMachineAST*  error         = necro_append_block(program, apply_fn_def, "error");
//     NecroMachineAST*  end           = necro_append_block(program, apply_fn_def, "end");
//     apply_fn_def->fn_def._err_block = error;
//     // NecroMachineAST*  greater_than  = necro_append_block(program, apply_fn_def, "greater_than");

//     //--------------------
//     // Entry Block
//     necro_build_break(program, apply_fn_def, loop_block);

//     //--------------------
//     // Loop Block
//     necro_move_to_block(program, apply_fn_def, loop_block);
//     NecroMachineAST*  fn_arity         = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 0, "fn_arity");
//     NecroMachineAST*  num_pargs        = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 1, "num_pargs");
//     NecroMachineAST*  total_args       = necro_build_binop(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 2), num_pargs, NECRO_MACHINE_BINOP_IADD);
//     NecroMachineAST*  is_less_than     = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_LT, total_args, fn_arity);
//     necro_build_cond_break(program, apply_fn_def, is_less_than, less_than, saturated);

//     //--------------------
//     // Less Than Block
//     necro_move_to_block(program, apply_fn_def, less_than);
//     necro_build_break(program, apply_fn_def, loop_block);

//     //--------------------
//     // Saturated Block
//     necro_move_to_block(program, apply_fn_def, saturated);
//     NecroMachineAST**       closure_arity_blocks = necro_snapshot_arena_alloc(&program->snapshot_arena, program->closure_defs.length * sizeof(NecroMachineAST*));
//     NecroMachineSwitchList* fn_arity_switch_list = NULL;
//     assert(program->closure_defs.length > 0);
//     for (size_t i = 0; i < program->closure_defs.length; ++i)
//     {
//         char        itoa_buf1[10];
//         size_t      closure_def_fn_arity = program->closure_defs.data[i].fn_arity;
//         const char* block_name           = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "closure_fn_arity_", itoa(closure_def_fn_arity, itoa_buf1, 10), "_" });
//         closure_arity_blocks[i]          = necro_append_block(program, apply_fn_def, block_name);
//         fn_arity_switch_list             = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = closure_arity_blocks[i], .value = closure_def_fn_arity }, fn_arity_switch_list);
//     }
//     NecroMachineAST* u_fn_arity      = necro_build_bit_cast(program, apply_fn_def, fn_arity, program->necro_uint_type);
//     NecroMachineAST* closure_fn_poly = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 2, "closure_fn_poly");
//     necro_build_switch(program, apply_fn_def, u_fn_arity, fn_arity_switch_list, error);

//     //--------------------
//     // Closure Arity Blocks
//     for (size_t i = 0; i < program->closure_defs.length; ++i)
//     {
//         NecroArenaSnapshot closure_snapshot      = necro_get_arena_snapshot(&program->snapshot_arena);
//         necro_move_to_block(program, apply_fn_def, closure_arity_blocks[i]);
//         size_t             closure_def_fn_arity  = program->closure_defs.data[i].fn_arity;
//         necro_get_closure_con(program, closure_def_fn_arity + 3, false);
//         NecroMachineAST*   closure_ptr           = necro_build_bit_cast(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), necro_create_machine_ptr_type(&program->arena, program->closure_types.data[closure_def_fn_arity + 3]));
//         NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
//         for (size_t i = 0; i < closure_def_fn_arity; ++i)
//             closure_fn_type_elems[i] = program->necro_poly_ptr_type;
//         NecroMachineType*  closure_fn_type       = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
//         NecroMachineAST*   closure_fn            = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
//         NecroMachineAST**  closure_fn_args       = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
//         NecroMachineAST*   parg_ptr              = necro_build_gep(program, apply_fn_def, closure_ptr, (uint32_t[]) { 0, 3 }, 2, "parg_ptr");
//         NecroMachineAST*   aarg_ptr              = necro_create_param_reg(program, apply_fn_def, 3);
//         for (size_t i = 0; i < closure_def_fn_arity; ++i)
//         {
//             NecroMachineAST* is_parg = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_LE, num_pargs, necro_create_i32_necro_machine_value(program, i));
//             NecroMachineAST* is_aarg = necro_build_binop(program, apply_fn_def, necro_create_uint1_necro_machine_value(program, 1), is_parg, NECRO_MACHINE_BINOP_ISUB);
//             NecroMachineAST* arg_ptr = necro_build_select(program, apply_fn_def, is_parg, parg_ptr, aarg_ptr);
//             closure_fn_args[i]       = necro_build_load_from_ptr(program, apply_fn_def, arg_ptr, "arg");
//             // TODO: Zero extend, also type check gep that types are uint32!
//             parg_ptr                 = necro_build_non_const_gep(program, apply_fn_def, parg_ptr, (NecroMachineAST*[]) { is_parg }, 1, "parg_ptr", parg_ptr->necro_machine_type);
//             aarg_ptr                 = necro_build_non_const_gep(program, apply_fn_def, aarg_ptr, (NecroMachineAST*[]) { is_aarg }, 1, "aarg_ptr", aarg_ptr->necro_machine_type);
//         }
//         closure_fn->necro_machine_type    = closure_fn->necro_machine_type->ptr_type.element_type;
//         NecroMachineAST*  closure_result  = necro_build_call(program, apply_fn_def, closure_fn, closure_fn_args, closure_def_fn_arity, "closure_result");
//         closure_fn->necro_machine_type    = necro_create_machine_ptr_type(&program->arena, closure_fn_type);
//         NecroMachineAST*  is_greater_than = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_GT, total_args, fn_arity);
//         // Tail call here instead?
//         necro_build_cond_break(program, apply_fn_def, is_greater_than, loop_block, end);
//         necro_rewind_arena(&program->snapshot_arena, closure_snapshot);
//     }

//     //--------------------
//     // Error Block
//     necro_move_to_block(program, apply_fn_def, error);
//     necro_build_call(program, apply_fn_def, necro_symtable_get(program->symtable, program->runtime._necro_error_exit.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_uint32_necro_machine_value(program, 2) }, 1, "");
//     necro_build_unreachable(program, apply_fn_def);

//     //--------------------
//     // End Block
//     necro_move_to_block(program, apply_fn_def, end);
//     necro_build_return(program, apply_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type));

//     //--------------------
//     // Finish
//     apply_machine_def->machine_def.update_fn = apply_fn_def;
//     necro_push_apply_fn_vector(&program->apply_fns, &apply_machine_def);
//     necro_rewind_arena(&program->snapshot_arena, snapshot);
// }

// void necro_declare_apply_fns(NecroMachineProgram* program)
// {
//     assert(program != NULL);
//     necro_declare_apply_fn(program);
//     // for (size_t i = 2; i < 12; ++i)
//     // {
//     //     necro_declare_apply_fn(program, i);
//     // }
// }

// NecroMachineAST* necro_get_apply_fn(NecroMachineProgram* program, size_t apply_arity)
// {
//     return program->apply_fns.data[0];
//     // assert(apply_arity > 0);
//     // size_t adjusted_apply_arity = apply_arity - 1;
//     // if (program->apply_fns.length > adjusted_apply_arity)
//     //     return program->apply_fns.data[adjusted_apply_arity];
//     // necro_declare_apply_fn(program, apply_arity);
//     // // TODO / HACK: Manually re-ordering by moving the new apply function to the beginning of the machine_defs...
//     // // This is so that dependencies are in a correct order for code generation
//     // // ...Not a great solution, but it works
//     // NecroMachineAST* prev_def = program->machine_defs.data[program->machine_defs.length - 1];
//     // for (size_t i = 0; i < program->machine_defs.length; ++i)
//     // {
//     //     NecroMachineAST* temp_def     = program->machine_defs.data[i];
//     //     program->machine_defs.data[i] = prev_def;
//     //     prev_def                      = temp_def;
//     // }
//     // assert(program->apply_fns.length > adjusted_apply_arity);
//     // assert(program->apply_fns.data[adjusted_apply_arity] != NULL);
//     // return program->apply_fns.data[adjusted_apply_arity];
// }

// void necro_core_to_machine_2_stack_array(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
// {
//     assert(program != NULL);
//     assert(ast != NULL);
//     assert(ast->expr_type == NECRO_CORE_EXPR_APP);
//     necro_core_to_machine_2_go(program, ast->app.exprB, outer);
// }

// NecroMachineAST* necro_core_to_machine_3_stack_array(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer, size_t arg_count)
// {
//     assert(program != NULL);
//     assert(ast != NULL);
//     assert(ast->expr_type == NECRO_CORE_EXPR_APP);
//     NecroMachineAST*   data_ptr  = necro_build_alloca(program, outer->machine_def.update_fn, arg_count);
//     size_t             arg_index = arg_count - 1;
//     while (ast->expr_type == NECRO_CORE_EXPR_APP)
//     {
//         NecroMachineAST* arg     = necro_core_to_machine_3_go(program, ast->app.exprB, outer);
//         NecroMachineAST* arg_ptr = necro_build_gep(program, outer->machine_def.update_fn, data_ptr, (uint32_t[]) { arg_index }, 1, "arg_ptr");
//         necro_build_store_into_ptr(program, outer->machine_def.update_fn, arg, arg_ptr);
//         arg_index--;
//         ast = ast->app.exprA;
//     }
//     return data_ptr;
// }

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
    const char*       num_string                 = itoa(adjusted_apply_arity, necro_snapshot_arena_alloc(&program->snapshot_arena, 10), 10);
    const char*       apply_machine_name         = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "apply", num_string });
    NecroVar          apply_machine_var          = necro_gen_var(program, NULL, apply_machine_name, NECRO_NAME_UNIQUE);
    NecroMachineAST*  apply_machine_def          = necro_create_machine_initial_machine_def(program, apply_machine_var, NULL, non_state_apply_fn_type, NULL);
    apply_machine_def->machine_def.num_arg_names = apply_arity;

    //--------------------
    // apply MachineDef state
    // TODO: Only have ONE Apply state type...
    const char*       apply_machine_state_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_ApplyMachineState", num_string });
    NecroVar          apply_machine_state_var  = necro_gen_var(program, NULL, apply_machine_state_name, NECRO_NAME_UNIQUE);
    NecroMachineType* apply_machine_def_type   = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type }, 2);
    apply_machine_def->necro_machine_type      = apply_machine_def_type;
    apply_machine_def->machine_def.data_id     = NECRO_APPLY_DATA_ID;
    apply_machine_def->machine_def.state_type  = NECRO_STATE_STATEFUL;

    //--------------------
    // apply update_fn
    const char*        apply_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_apply", num_string });
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

    //--------------------
    // Closure Arity Blocks
    NecroMachineAST**       closure_arity_blocks = necro_snapshot_arena_alloc(&program->snapshot_arena, program->closure_defs.length * sizeof(NecroMachineAST*));
    NecroMachineSwitchList* fn_arity_switch_list = NULL;
    assert(program->closure_defs.length > 0);
    for (size_t i = 0; i < program->closure_defs.length; ++i)
    {
        char        itoa_buf1[10];
        char        itoa_buf2[10];
        size_t      closure_def_fn_arity = program->closure_defs.data[i].fn_arity;
        size_t      closure_def_pargs    = program->closure_defs.data[i].num_pargs;
        const char* block_name           = necro_concat_strings(&program->snapshot_arena, 5, (const char*[]) { "closure_arity_", itoa(closure_def_fn_arity, itoa_buf1, 10), "_pargs_" , itoa(closure_def_pargs, itoa_buf2, 10), "_" });
        closure_arity_blocks[i]          = necro_append_block(program, apply_fn_def, block_name);
        fn_arity_switch_list             = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = closure_arity_blocks[i], .value = i }, fn_arity_switch_list);
    }
    NecroMachineAST* error           = necro_append_block(program, apply_fn_def, "error");
    NecroMachineAST* closure_type    = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 0, "closure_type");
    NecroMachineAST* u_closure_type  = necro_build_bit_cast(program, apply_fn_def, closure_type, program->necro_uint_type);
    NecroMachineAST* closure_fn_poly = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 2, "closure_fn_poly");
    necro_build_switch(program, apply_fn_def, u_closure_type, fn_arity_switch_list, error);
    for (size_t c = 0; c < program->closure_defs.length; ++c)
    {
        NecroArenaSnapshot closure_snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
        necro_move_to_block(program, apply_fn_def, closure_arity_blocks[c]);
        const size_t     closure_def_fn_arity = program->closure_defs.data[c].fn_arity;
        const size_t     closure_def_pargs    = program->closure_defs.data[c].num_pargs;
        const size_t     num_apply_args       = adjusted_apply_arity;
        const size_t     num_total_args       = closure_def_pargs + num_apply_args;
        NecroMachineAST* closure_con          = necro_get_closure_con(program, 3 + closure_def_pargs, false);
        NecroMachineAST* closure_ptr          = necro_build_bit_cast(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), necro_create_machine_ptr_type(&program->arena, program->closure_types.data[closure_def_pargs + 3]));
        // TODO: Statefuleness!
        // TODO: Store uid in closure and remove unneeded extra argument!
        // Under Saturated
        if (num_total_args < closure_def_fn_arity)
        {
            NecroMachineAST*  new_closure_con = necro_get_closure_con(program, 3 + num_total_args, false);
            NecroMachineAST** call_args       = necro_snapshot_arena_alloc(&program->snapshot_arena, (3 + num_total_args) * sizeof(NecroMachineAST*));
            for (size_t i = 0; i < 3 + closure_def_pargs; ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, i, "carg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[3 + i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineAST*  closure_result    = necro_build_call(program, apply_fn_def, new_closure_con, call_args, 3 + num_total_args, "result");
            necro_build_return(program, apply_fn_def, closure_result);
        }
        // Saturated
        else if (num_total_args == closure_def_fn_arity)
        {
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, 3 + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType*  closure_fn_type   = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
            NecroMachineAST*   closure_fn        = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            closure_fn->necro_machine_type       = closure_fn->necro_machine_type->ptr_type.element_type;
            NecroMachineAST*   closure_result    = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, "result");
            closure_fn->necro_machine_type       = necro_create_machine_ptr_type(&program->arena, closure_fn_type);
            necro_build_return(program, apply_fn_def, closure_result);
        }
        // Over Saturated
        else
        {
            //-------------------
            // Call closure fn
            const size_t num_extra_args = num_total_args - closure_def_fn_arity;
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, 3 + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType* closure_fn_type    = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
            NecroMachineAST*  closure_fn         = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            closure_fn->necro_machine_type       = closure_fn->necro_machine_type->ptr_type.element_type;
            NecroMachineAST*  closure_result     = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, "result");
            closure_fn->necro_machine_type       = necro_create_machine_ptr_type(&program->arena, closure_fn_type);
            //-------------------
            // Call overapply fn
            NecroMachineAST*  over_apply_fn   = necro_get_apply_fn(program, num_extra_args + 1);
            NecroMachineAST** over_apply_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (num_extra_args + 2) * sizeof(NecroMachineAST*));
            over_apply_args[0]                = necro_create_null_necro_machine_value(program, necro_create_machine_ptr_type(&program->arena, over_apply_fn->necro_machine_type)); // NULL state for now...
            over_apply_args[1]                = closure_result;
            for (size_t i = 0; i < num_extra_args; ++i)
                over_apply_args[i + 2] = necro_create_param_reg(program, apply_fn_def, i + (closure_def_fn_arity - closure_def_pargs) + 2);
            NecroMachineAST* over_result      = necro_build_call(program, apply_fn_def, over_apply_fn->machine_def.update_fn->fn_def.fn_value, over_apply_args, num_extra_args + 2, "over_result");
            necro_build_return(program, apply_fn_def, over_result);
        }
        necro_rewind_arena(&program->snapshot_arena, closure_snapshot);
    }

    //--------------------
    // Error Block
    necro_move_to_block(program, apply_fn_def, error);
    necro_build_call(program, apply_fn_def, necro_symtable_get(program->symtable, program->runtime._necro_error_exit.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_uint32_necro_machine_value(program, 2) }, 1, "");
    necro_build_unreachable(program, apply_fn_def);

    program->apply_fns.data[adjusted_apply_arity] = apply_machine_def;
    apply_machine_def->machine_def.update_fn = apply_fn_def;

    // TODO: Maybe need mk_fn? Maybe not?

    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

void necro_declare_apply_fns(NecroMachineProgram* program)
{
    assert(program != NULL);
    // for (size_t i = 2; i < 12; ++i)
    // {
    //     necro_declare_apply_fn(program, i);
    // }
}

NecroMachineAST* necro_get_apply_fn(NecroMachineProgram* program, size_t apply_arity)
{
    assert(apply_arity > 0);
    size_t adjusted_apply_arity = apply_arity - 1;
    if (program->apply_fns.length > adjusted_apply_arity && program->apply_fns.data[adjusted_apply_arity] != NULL)
        return program->apply_fns.data[adjusted_apply_arity];
    necro_declare_apply_fn(program, apply_arity);
    // // TODO / HACK: Manually re-ordering by moving the new apply function to the beginning of the machine_defs...
    // // This is so that dependencies are in a correct order for code generation
    // // ...Not a great solution, but it works
    // NecroMachineAST* prev_def = program->machine_defs.data[program->machine_defs.length - 1];
    // for (size_t i = 0; i < program->machine_defs.length; ++i)
    // {
    //     NecroMachineAST* temp_def     = program->machine_defs.data[i];
    //     program->machine_defs.data[i] = prev_def;
    //     prev_def                      = temp_def;
    // }
    assert(program->apply_fns.length > adjusted_apply_arity);
    assert(program->apply_fns.data[adjusted_apply_arity] != NULL);
    return program->apply_fns.data[adjusted_apply_arity];
}

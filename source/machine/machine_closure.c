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

// void necro_declare_apply_fn(NecroMachineProgram* program, size_t apply_arity)
// {
//     assert(apply_arity > 0);
//     assert(program != NULL);
//     size_t adjusted_apply_arity = apply_arity - 1;
//     size_t arity_with_state     = apply_arity + 1;
//     while (program->apply_fns.length < adjusted_apply_arity + 1)
//     {
//         NecroMachineAST* null_ast = NULL;
//         necro_push_apply_fn_vector(&program->apply_fns, &null_ast);
//     }
//     if (program->apply_fns.data[adjusted_apply_arity] != NULL)
//         return;
//     assert(program->closure_type != NULL);
//     assert(program->closure_type->type == NECRO_MACHINE_TYPE_STRUCT);
//     NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&program->snapshot_arena);

//     //--------------------
//     // apply non-state update_fn type
//     NecroMachineType*  closure_ptr_type = necro_create_machine_ptr_type(&program->arena, program->closure_type);
//     NecroMachineType** elems_non_state  = necro_snapshot_arena_alloc(&program->snapshot_arena, apply_arity * sizeof(NecroMachineType*));
//     elems_non_state[0]                  = closure_ptr_type;
//     for (size_t arg = 1; arg < apply_arity; ++arg)
//     {
//         elems_non_state[arg] = program->necro_poly_ptr_type;
//     }
//     NecroMachineType* non_state_apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems_non_state, apply_arity);

//     //--------------------
//     // apply MachineDef
//     const char*       num_string                 = itoa(adjusted_apply_arity, necro_snapshot_arena_alloc(&program->snapshot_arena, 10), 10);
//     const char*       apply_machine_name         = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "apply", num_string });
//     NecroVar          apply_machine_var          = necro_gen_var(program, NULL, apply_machine_name, NECRO_NAME_UNIQUE);
//     NecroMachineAST*  apply_machine_def          = necro_create_machine_initial_machine_def(program, apply_machine_var, NULL, non_state_apply_fn_type, NULL);
//     apply_machine_def->machine_def.num_arg_names = apply_arity;

//     //--------------------
//     // apply MachineDef state
//     const char*       apply_machine_state_name  = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_ApplyMachineState", num_string });
//     NecroVar          apply_machine_state_var   = necro_gen_var(program, NULL, apply_machine_state_name, NECRO_NAME_UNIQUE);
//     NecroMachineType* apply_machine_def_type    = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
//     apply_machine_def_type->struct_type.members[2] = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
//     apply_machine_def->necro_machine_type       = apply_machine_def_type;
//     apply_machine_def->machine_def.data_id      = NECRO_APPLY_DATA_ID;
//     apply_machine_def->machine_def.state_type   = NECRO_STATE_STATEFUL;
//     apply_machine_def->machine_def.is_recursive = true;
//     apply_machine_def->machine_def.num_members  = 3;

//     NecroMachineType* machine_ptr_type = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
//     //--------------------
//     // Apply init_fn
//     {
//         NecroMachineType*  init_fn_type    = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { machine_ptr_type }, 1);
//         NecroMachineAST*   init_fn_body    = necro_create_machine_block(program, "entry", NULL);
//         const char*        init_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_initApply", num_string });
//         NecroVar           init_fn_var     = necro_gen_var(program, NULL, init_fn_name, NECRO_NAME_UNIQUE);
//         NecroMachineAST*   init_fn_def     = necro_create_machine_fn(program, init_fn_var, init_fn_body, init_fn_type);
//         program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
//         apply_machine_def->machine_def.init_fn = init_fn_def;
//         necro_build_store_into_slot(program, init_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), necro_create_param_reg(program, init_fn_def, 0), 1);
//         necro_build_store_into_slot(program, init_fn_def, necro_create_null_necro_machine_value(program, machine_ptr_type), necro_create_param_reg(program, init_fn_def, 0), 2);
//         necro_build_return_void(program, init_fn_def);
//         necro_symtable_get(program->symtable, init_fn_var.id)->necro_machine_ast = init_fn_def->fn_def.fn_value;
//     }

//     //--------------------
//     // Apply mk_fn
//     {
//         NecroMachineType*  mk_fn_type        = necro_create_machine_fn_type(&program->arena, machine_ptr_type, NULL, 0);
//         NecroMachineAST*   mk_fn_body        = necro_create_machine_block(program, "entry", NULL);
//         const char*        mk_fn_name        = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkApply", num_string });
//         NecroVar           mk_fn_var         = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
//         NecroMachineAST*   mk_fn_def         = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
//         program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
//         apply_machine_def->machine_def.mk_fn = mk_fn_def;
//         NecroMachineAST*   data_ptr          = necro_build_nalloc(program, mk_fn_def, apply_machine_def_type, 3, false);
//         necro_build_call(program, mk_fn_def, apply_machine_def->machine_def.init_fn->fn_def.fn_value, (NecroMachineAST*[]) { data_ptr }, 1, NECRO_LANG_CALL, "");
//         necro_build_return(program, mk_fn_def, data_ptr);
//         necro_symtable_get(program->symtable, mk_fn_var.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
//     }

//     //--------------------
//     // apply update_fn
//     const char*        apply_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_updateApply", num_string });
//     NecroVar           apply_fn_var     = necro_gen_var(program, NULL, apply_fn_name, NECRO_NAME_UNIQUE);
//     NecroMachineType** elems            = necro_snapshot_arena_alloc(&program->snapshot_arena, arity_with_state * sizeof(NecroMachineType*));
//     elems[0]                            = necro_create_machine_ptr_type(&program->arena, apply_machine_def->necro_machine_type);
//     elems[1]                            = closure_ptr_type;
//     for (size_t arg = 2; arg < arity_with_state; ++arg)
//     {
//         elems[arg] = program->necro_poly_ptr_type;
//     }
//     NecroMachineType* apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems, arity_with_state);
//     NecroMachineAST*  apply_fn_body = necro_create_machine_block(program, "entry", NULL);
//     NecroMachineAST*  apply_fn_def  = necro_create_machine_fn(program, apply_fn_var, apply_fn_body, apply_fn_type);
//     program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
//     apply_fn_def->fn_def.state_type = NECRO_STATE_STATEFUL;

//     //--------------------
//     // Apply Fn Body
//     NecroMachineAST* init_block     = necro_append_block(program, apply_fn_def, "init");
//     NecroMachineAST* cont_block     = necro_append_block(program, apply_fn_def, "cont");
//     NecroMachineAST* child_ptr      = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 2, "child");
//     NecroMachineAST* cmp_result     = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_EQ, child_ptr, necro_create_null_necro_machine_value(program, child_ptr->necro_machine_type));
//     necro_build_cond_break(program, apply_fn_def, cmp_result, init_block, cont_block);

//     //--------------------
//     // Apply init
//     necro_move_to_block(program, apply_fn_def, init_block);
//     NecroMachineAST* data_ptr = necro_build_call(program, apply_fn_def, apply_machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "data_ptr");
//     necro_build_store_into_slot(program, apply_fn_def, data_ptr, necro_create_param_reg(program, apply_fn_def, 0), 2);
//     necro_build_break(program, apply_fn_def, cont_block);

//     //--------------------
//     // Closure Arity Blocks
//     necro_move_to_block(program, apply_fn_def, cont_block);
//     NecroMachineAST**       closure_arity_blocks = necro_snapshot_arena_alloc(&program->snapshot_arena, program->closure_defs.length * sizeof(NecroMachineAST*));
//     NecroMachineSwitchList* fn_arity_switch_list = NULL;
//     assert(program->closure_defs.length > 0);
//     for (size_t i = 0; i < program->closure_defs.length; ++i)
//     {
//         char        itoa_buf1[10];
//         char        itoa_buf2[10];
//         size_t      closure_def_fn_arity = program->closure_defs.data[i].fn_arity;
//         size_t      closure_def_pargs    = program->closure_defs.data[i].num_pargs;
//         const char* block_name           = necro_concat_strings(&program->snapshot_arena, 5, (const char*[]) { "closure_arity_", itoa(closure_def_fn_arity, itoa_buf1, 10), "_pargs_" , itoa(closure_def_pargs, itoa_buf2, 10), "_" });
//         closure_arity_blocks[i]          = necro_append_block(program, apply_fn_def, block_name);
//         size_t      closure_switch_val   = (closure_def_fn_arity << 16) | closure_def_pargs;
//         fn_arity_switch_list             = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = closure_arity_blocks[i], .value = closure_switch_val }, fn_arity_switch_list);
//     }
//     NecroMachineAST* error               = necro_append_block(program, apply_fn_def, "error");
//     NecroMachineAST* closure_fn_arity    = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 0, "closure_fn_arity");
//     NecroMachineAST* u_closure_fn_arity  = necro_build_bit_cast(program, apply_fn_def, closure_fn_arity, program->necro_uint_type);
//     NecroMachineAST* closure_num_pargs   = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 1, "closure_num_pargs");
//     NecroMachineAST* u_closure_num_pargs = necro_build_bit_cast(program, apply_fn_def, closure_num_pargs, program->necro_uint_type);
//     NecroMachineAST* shifted_fn_arity    = necro_build_binop(program, apply_fn_def, u_closure_fn_arity, necro_create_word_uint_value(program, 16), NECRO_MACHINE_BINOP_SHL);
//     NecroMachineAST* closure_type        = necro_build_binop(program, apply_fn_def, shifted_fn_arity, u_closure_num_pargs, NECRO_MACHINE_BINOP_OR);
//     NecroMachineAST* closure_fn_poly     = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), 2, "closure_fn_poly");
//     necro_build_switch(program, apply_fn_def, closure_type, fn_arity_switch_list, error);

//     for (size_t c = 0; c < program->closure_defs.length; ++c)
//     {
//         NecroArenaSnapshot closure_snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
//         necro_move_to_block(program, apply_fn_def, closure_arity_blocks[c]);
//         const size_t     closure_def_fn_arity = program->closure_defs.data[c].fn_arity;
//         const size_t     closure_def_pargs    = program->closure_defs.data[c].num_pargs;
//         const size_t     num_apply_args       = adjusted_apply_arity;
//         const size_t     num_total_args       = closure_def_pargs + num_apply_args;
//         NecroMachineAST* closure_con          = necro_get_closure_con(program, 3 + closure_def_pargs, false);
//         NecroMachineAST* closure_ptr          = necro_build_bit_cast(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 1), necro_create_machine_ptr_type(&program->arena, program->closure_types.data[closure_def_pargs + 3]));
//         // TODO: Statefuleness!
//         // TODO: Correct GC!

//         // x - N cases
//         // Under Saturated
//         if (num_total_args < closure_def_fn_arity)
//         {
//             NecroMachineAST*  new_closure_con = necro_get_closure_con(program, 3 + num_total_args, false);
//             NecroMachineAST** call_args       = necro_snapshot_arena_alloc(&program->snapshot_arena, (3 + num_total_args) * sizeof(NecroMachineAST*));
//             call_args[0] = closure_fn_arity;
//             call_args[1] = necro_create_word_int_value(program, num_total_args);
//             call_args[2] = closure_fn_poly;
//             for (size_t i = 3; i < (3 + closure_def_pargs); ++i)
//                 call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, i, "parg");
//             for (size_t i = 0; i < num_apply_args; ++i)
//                 call_args[3 + i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
//             NecroMachineAST*  closure_result    = necro_build_call(program, apply_fn_def, new_closure_con, call_args, 3 + num_total_args, NECRO_LANG_CALL, "result");
//             necro_build_return(program, apply_fn_def, closure_result);
//         }

//         // Saturated
//         else if (num_total_args == closure_def_fn_arity)
//         {
//             NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
//             for (size_t i = 0; i < closure_def_pargs; ++i)
//                 call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, 3 + i, "parg");
//             for (size_t i = 0; i < num_apply_args; ++i)
//                 call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
//             NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
//             for (size_t i = 0; i < closure_def_fn_arity; ++i)
//                 closure_fn_type_elems[i] = program->necro_poly_ptr_type;
//             NecroMachineType*  closure_fn_type   = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
//             NecroMachineAST*   closure_fn        = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
//             closure_fn->necro_machine_type       = closure_fn->necro_machine_type->ptr_type.element_type;
//             NecroMachineAST*   closure_result    = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, NECRO_LANG_CALL, "result");
//             closure_fn->necro_machine_type       = necro_create_machine_ptr_type(&program->arena, closure_fn_type);
//             necro_build_return(program, apply_fn_def, closure_result);
//         }

//         // Over Saturated
//         else
//         {
//             //-------------------
//             // Call closure fn
//             const size_t num_extra_args = num_total_args - closure_def_fn_arity;
//             NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
//             for (size_t i = 0; i < closure_def_pargs; ++i)
//                 call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, 3 + i, "parg");
//             for (size_t i = 0; i < num_apply_args; ++i)
//                 call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 2 + i);
//             NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
//             for (size_t i = 0; i < closure_def_fn_arity; ++i)
//                 closure_fn_type_elems[i] = program->necro_poly_ptr_type;
//             NecroMachineType* closure_fn_type    = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
//             NecroMachineAST*  closure_fn         = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
//             closure_fn->necro_machine_type       = closure_fn->necro_machine_type->ptr_type.element_type;
//             NecroMachineAST*  closure_result     = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, NECRO_LANG_CALL, "result");
//             closure_fn->necro_machine_type       = necro_create_machine_ptr_type(&program->arena, closure_fn_type);
//             //-------------------
//             // Call overapply fn
//             NecroMachineAST*  over_apply_fn   = necro_get_apply_fn(program, num_extra_args + 1);
//             NecroMachineAST** over_apply_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (num_extra_args + 2) * sizeof(NecroMachineAST*));
//             // over_apply_args[0]                = necro_create_null_necro_machine_value(program, necro_create_machine_ptr_type(&program->arena, over_apply_fn->necro_machine_type)); // NULL state for now...
//             NecroMachineAST*  nested_data_ptr = necro_build_call(program, apply_fn_def, apply_machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "nested_data_ptr");
//             over_apply_args[0]                = necro_build_bit_cast(program, apply_fn_def, nested_data_ptr, program->necro_poly_ptr_type);
//             over_apply_args[1]                = closure_result;
//             for (size_t i = 0; i < num_extra_args; ++i)
//                 over_apply_args[i + 2] = necro_create_param_reg(program, apply_fn_def, i + (closure_def_fn_arity - closure_def_pargs) + 2);
//             NecroMachineAST* over_result      = necro_build_call(program, apply_fn_def, over_apply_fn->machine_def.update_fn->fn_def.fn_value, over_apply_args, num_extra_args + 2, NECRO_LANG_CALL, "over_result");
//             necro_build_return(program, apply_fn_def, over_result);
//         }

//         necro_rewind_arena(&program->snapshot_arena, closure_snapshot);
//     }

//     //--------------------
//     // Error Block
//     necro_move_to_block(program, apply_fn_def, error);
//     necro_build_call(program, apply_fn_def, necro_symtable_get(program->symtable, program->runtime._necro_error_exit.id)->necro_machine_ast->fn_def.fn_value, (NecroMachineAST*[]) { necro_create_uint32_necro_machine_value(program, 2) }, 1, NECRO_C_CALL, "");
//     necro_build_unreachable(program, apply_fn_def);

//     program->apply_fns.data[adjusted_apply_arity] = apply_machine_def;
//     apply_machine_def->machine_def.update_fn = apply_fn_def;

//     necro_rewind_arena(&program->snapshot_arena, snapshot);
// }

void necro_declare_apply_fn(NecroMachineProgram* program, size_t apply_arity)
{
    assert(apply_arity > 0);
    assert(program != NULL);
    size_t adjusted_apply_arity = apply_arity - 1;
    //size_t arity_with_state     = apply_arity + 1;
    size_t arity_with_state     = apply_arity;
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
    const char*       apply_machine_state_name  = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_ApplyMachineState", num_string });
    NecroVar          apply_machine_state_var   = necro_gen_var(program, NULL, apply_machine_state_name, NECRO_NAME_UNIQUE);
    // NecroMachineType* apply_machine_def_type    = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
    NecroMachineType* apply_machine_def_type    = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, NULL, 0);
    // apply_machine_def_type->struct_type.members[2] = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
    apply_machine_def->necro_machine_type       = apply_machine_def_type;
    apply_machine_def->machine_def.data_id      = NECRO_APPLY_DATA_ID;
    apply_machine_def->machine_def.state_type   = NECRO_STATE_POINTWISE;
    apply_machine_def->machine_def.is_recursive = false;
    apply_machine_def->machine_def.num_members  = 0;

    NecroMachineType* machine_ptr_type     = necro_create_machine_ptr_type(&program->arena, apply_machine_def_type);
    apply_machine_def->machine_def.init_fn = NULL;
    apply_machine_def->machine_def.mk_fn   = NULL;

    //--------------------
    // apply update_fn
    const char*        apply_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_updateApply", num_string });
    NecroVar           apply_fn_var     = necro_gen_var(program, NULL, apply_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType** elems            = necro_snapshot_arena_alloc(&program->snapshot_arena, arity_with_state * sizeof(NecroMachineType*));
    // elems[0]                            = necro_create_machine_ptr_type(&program->arena, apply_machine_def->necro_machine_type);
    elems[0]                            = closure_ptr_type;
    for (size_t arg = 1; arg < arity_with_state; ++arg)
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
    // NecroMachineAST* init_block     = necro_append_block(program, apply_fn_def, "init");
    // NecroMachineAST* cont_block     = necro_append_block(program, apply_fn_def, "cont");
    // NecroMachineAST* child_ptr      = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 2, "child");
    // NecroMachineAST* cmp_result     = necro_build_cmp(program, apply_fn_def, NECRO_MACHINE_CMP_EQ, child_ptr, necro_create_null_necro_machine_value(program, child_ptr->necro_machine_type));
    // necro_build_cond_break(program, apply_fn_def, cmp_result, init_block, cont_block);

    // //--------------------
    // // Apply init
    // necro_move_to_block(program, apply_fn_def, init_block);
    // NecroMachineAST* data_ptr = necro_build_call(program, apply_fn_def, apply_machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "data_ptr");
    // necro_build_store_into_slot(program, apply_fn_def, data_ptr, necro_create_param_reg(program, apply_fn_def, 0), 2);
    // necro_build_break(program, apply_fn_def, cont_block);

    //--------------------
    // Closure Arity Blocks
    // necro_move_to_block(program, apply_fn_def, cont_block);
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
        size_t      closure_switch_val   = (closure_def_fn_arity << 16) | closure_def_pargs;
        fn_arity_switch_list             = necro_cons_machine_switch_list(&program->arena, (NecroMachineSwitchData) { .block = closure_arity_blocks[i], .value = closure_switch_val }, fn_arity_switch_list);
    }
    NecroMachineAST* error               = necro_append_block(program, apply_fn_def, "error");
    NecroMachineAST* closure_fn_arity    = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 0, "closure_fn_arity");
    NecroMachineAST* u_closure_fn_arity  = necro_build_bit_cast(program, apply_fn_def, closure_fn_arity, program->necro_uint_type);
    NecroMachineAST* closure_num_pargs   = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 1, "closure_num_pargs");
    NecroMachineAST* u_closure_num_pargs = necro_build_bit_cast(program, apply_fn_def, closure_num_pargs, program->necro_uint_type);
    NecroMachineAST* shifted_fn_arity    = necro_build_binop(program, apply_fn_def, u_closure_fn_arity, necro_create_word_uint_value(program, 16), NECRO_MACHINE_BINOP_SHL);
    NecroMachineAST* closure_type        = necro_build_binop(program, apply_fn_def, shifted_fn_arity, u_closure_num_pargs, NECRO_MACHINE_BINOP_OR);
    NecroMachineAST* closure_fn_poly     = necro_build_load_from_slot(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), 2, "closure_fn_poly");
    necro_build_switch(program, apply_fn_def, closure_type, fn_arity_switch_list, error);

    for (size_t c = 0; c < program->closure_defs.length; ++c)
    {
        NecroArenaSnapshot closure_snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
        necro_move_to_block(program, apply_fn_def, closure_arity_blocks[c]);
        const size_t     closure_def_fn_arity = program->closure_defs.data[c].fn_arity;
        const size_t     closure_def_pargs    = program->closure_defs.data[c].num_pargs;
        const size_t     num_apply_args       = adjusted_apply_arity;
        const size_t     num_total_args       = closure_def_pargs + num_apply_args;
        NecroMachineAST* closure_con          = necro_get_closure_con(program, 3 + closure_def_pargs, false);
        NecroMachineAST* closure_ptr          = necro_build_bit_cast(program, apply_fn_def, necro_create_param_reg(program, apply_fn_def, 0), necro_create_machine_ptr_type(&program->arena, program->closure_types.data[closure_def_pargs + 3]));
        // TODO: Statefuleness!
        // TODO: Correct GC!

        // x - N cases
        // Under Saturated
        if (num_total_args < closure_def_fn_arity)
        {
            NecroMachineAST*  new_closure_con = necro_get_closure_con(program, 3 + num_total_args, false);
            NecroMachineAST** call_args       = necro_snapshot_arena_alloc(&program->snapshot_arena, (3 + num_total_args) * sizeof(NecroMachineAST*));
            call_args[0] = closure_fn_arity;
            call_args[1] = necro_create_word_int_value(program, num_total_args);
            call_args[2] = closure_fn_poly;
            for (size_t i = 3; i < (3 + closure_def_pargs); ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[3 + i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 1 + i);
            NecroMachineAST*  closure_result    = necro_build_call(program, apply_fn_def, new_closure_con, call_args, 3 + num_total_args, NECRO_LANG_CALL, "result");
            necro_build_return(program, apply_fn_def, closure_result);
        }

        // Saturated
        else if (num_total_args == closure_def_fn_arity)
        {
            NecroMachineAST** call_args = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineAST*));
            for (size_t i = 0; i < closure_def_pargs; ++i)
                call_args[i] = necro_build_load_from_slot(program, apply_fn_def, closure_ptr, 3 + i, "parg");
            for (size_t i = 0; i < num_apply_args; ++i)
                call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 1 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType*  closure_fn_type   = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
            NecroMachineAST*   closure_fn        = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            closure_fn->necro_machine_type       = closure_fn->necro_machine_type->ptr_type.element_type;
            NecroMachineAST*   closure_result    = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, NECRO_LANG_CALL, "result");
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
                call_args[i + closure_def_pargs] = necro_create_param_reg(program, apply_fn_def, 1 + i);
            NecroMachineType** closure_fn_type_elems = necro_snapshot_arena_alloc(&program->snapshot_arena, closure_def_fn_arity * sizeof(NecroMachineType*));
            for (size_t i = 0; i < closure_def_fn_arity; ++i)
                closure_fn_type_elems[i] = program->necro_poly_ptr_type;
            NecroMachineType* closure_fn_type    = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, closure_fn_type_elems, closure_def_fn_arity);
            NecroMachineAST*  closure_fn         = necro_build_bit_cast(program, apply_fn_def, closure_fn_poly, necro_create_machine_ptr_type(&program->arena, closure_fn_type));
            closure_fn->necro_machine_type       = closure_fn->necro_machine_type->ptr_type.element_type;
            NecroMachineAST*  closure_result     = necro_build_call(program, apply_fn_def, closure_fn, call_args, closure_def_fn_arity, NECRO_LANG_CALL, "result");
            closure_fn->necro_machine_type       = necro_create_machine_ptr_type(&program->arena, closure_fn_type);
            //-------------------
            // Call overapply fn
            NecroMachineAST*  over_apply_fn   = necro_get_apply_fn(program, num_extra_args + 1);
            NecroMachineAST** over_apply_args = necro_snapshot_arena_alloc(&program->snapshot_arena, (num_extra_args + 1) * sizeof(NecroMachineAST*));
            // over_apply_args[0]                = necro_create_null_necro_machine_value(program, necro_create_machine_ptr_type(&program->arena, over_apply_fn->necro_machine_type)); // NULL state for now...
            // NecroMachineAST*  nested_data_ptr = necro_build_call(program, apply_fn_def, apply_machine_def->machine_def.mk_fn->fn_def.fn_value, NULL, 0, NECRO_LANG_CALL, "nested_data_ptr");
            // over_apply_args[0]                = necro_build_bit_cast(program, apply_fn_def, nested_data_ptr, program->necro_poly_ptr_type);
            over_apply_args[0]                = closure_result;
            for (size_t i = 0; i < num_extra_args; ++i)
                over_apply_args[i + 1] = necro_create_param_reg(program, apply_fn_def, i + (closure_def_fn_arity - closure_def_pargs) + 1);
            NecroMachineAST* over_result      = necro_build_call(program, apply_fn_def, over_apply_fn->machine_def.update_fn->fn_def.fn_value, over_apply_args, num_extra_args + 1, NECRO_LANG_CALL, "over_result");
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

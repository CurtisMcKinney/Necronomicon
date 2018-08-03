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
    while (program->closure_types.length < adjusted_closure_arity + 1)
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

void necro_declare_apply_fn(NecroMachineProgram* program)
{
    // assert(apply_arity > 0);
    assert(program != NULL);
    // size_t adjusted_apply_arity = apply_arity - 1;
    // size_t arity_with_state     = apply_arity + 1;
    // while (program->apply_fns.length < adjusted_apply_arity + 1)
    // {
    //     NecroMachineAST* null_ast = NULL;
    //     necro_push_apply_fn_vector(&program->apply_fns, &null_ast);
    // }
    // if (program->apply_fns.data[adjusted_apply_arity] != NULL)
    //     return;
    assert(program->closure_type != NULL);
    assert(program->closure_type->type == NECRO_MACHINE_TYPE_STRUCT);
    NecroArenaSnapshot snapshot         = necro_get_arena_snapshot(&program->snapshot_arena);

    //--------------------
    // apply non-state update_fn type
    NecroMachineType*  closure_ptr_type = necro_create_machine_ptr_type(&program->arena, program->closure_type);
    // NecroMachineType** elems_non_state  = necro_snapshot_arena_alloc(&program->snapshot_arena, apply_arity * sizeof(NecroMachineType*));
    // NecroMachineType** elems_non_state  = necro_snapshot_arena_alloc(&program->snapshot_arena, 3 * sizeof(NecroMachineType*));
    // elems_non_state[0]                  = closure_ptr_type;
    // elems_non_state[1]                  = program->necro_int_type;
    // elems_non_state[2]                  = necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type);
    // for (size_t arg = 1; arg < apply_arity; ++arg)
    // {
    //     elems_non_state[arg] = program->necro_poly_ptr_type;
    // }
    // NecroMachineType* non_state_apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems_non_state, apply_arity);
    // NecroMachineType* non_state_apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems_non_state, 2);
    NecroMachineType* non_state_apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, (NecroMachineType*[]) { closure_ptr_type, program->necro_int_type, necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type) }, 3);

    //--------------------
    // apply MachineDef
    // const char*       num_string                 = itoa(adjusted_apply_arity, necro_snapshot_arena_alloc(&program->snapshot_arena, 10), 10);
    // const char*       apply_machine_name         = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "apply", num_string });
    // NecroVar          apply_machine_var          = necro_gen_var(program, NULL, apply_machine_name, NECRO_NAME_UNIQUE);
    // NecroVar          apply_machine_var          = necro_gen_var(program, NULL, "_apply_", NECRO_NAME_UNIQUE);
    NecroVar          apply_var                  = necro_con_to_var(program->prim_types->apply_fn);
    NecroMachineAST*  apply_machine_def          = necro_create_machine_initial_machine_def(program, apply_var, NULL, non_state_apply_fn_type, NULL);
    apply_machine_def->machine_def.num_arg_names = 2;
    // apply_machine_def->machine_def.num_arg_names = apply_arity;

    //--------------------
    // apply MachineDef state
    // TODO: Only have ONE Apply state type...
    // const char*       apply_machine_state_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_ApplyMachineState", num_string });
    NecroVar          apply_machine_state_var  = necro_gen_var(program, NULL, "_ApplyMachineState", NECRO_NAME_UNIQUE);
    NecroMachineType* apply_machine_def_type   = necro_create_machine_struct_type(&program->arena, apply_machine_state_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type }, 2);
    apply_machine_def->necro_machine_type      = apply_machine_def_type;
    apply_machine_def->machine_def.data_id     = NECRO_APPLY_DATA_ID;
    apply_machine_def->machine_def.state_type  = NECRO_STATE_STATEFUL;

    //--------------------
    // apply update_fn
    // const char*        apply_fn_name    = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_apply_update_fn", num_string });
    NecroVar           apply_fn_var     = necro_gen_var(program, NULL, "_apply_update_fn", NECRO_NAME_UNIQUE);
    NecroMachineType** elems            = necro_snapshot_arena_alloc(&program->snapshot_arena, 3 * sizeof(NecroMachineType*));
    elems[0]                            = necro_create_machine_ptr_type(&program->arena, apply_machine_def->necro_machine_type);
    elems[1]                            = closure_ptr_type;
    elems[2]                            = program->necro_int_type;
    elems[3]                            = necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type);
    // for (size_t arg = 2; arg < arity_with_state; ++arg)
    // {
    //     elems[arg] = program->necro_poly_ptr_type;
    // }
    NecroMachineType* apply_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, elems, 4);
    NecroMachineAST*  apply_fn_body = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*  apply_fn_def  = necro_create_machine_fn(program, apply_fn_var, apply_fn_body, apply_fn_type);
    program->functions.length--; // HACK: Don't want the update function in the functions list, instead it belongs to the machine
    apply_fn_def->fn_def.state_type = NECRO_STATE_STATEFUL;

    // NecroMachineAST*  data_ptr   = necro_build_nalloc(program, mk_fn_def, actual_type->necro_machine_type, closure_arity, c == 1);
    // TODO: Body of fn
    // Iterate through closure defs
    // When hotswapping simply plan on redefining all apply functions if the closure def vector has changed to reflect all the new possibilities

    necro_build_return(program, apply_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type));
    // program->apply_fns.data[adjusted_apply_arity] = apply_machine_def;
    // program->apply_fns.data[0] = apply_machine_def;
    apply_machine_def->machine_def.update_fn = apply_fn_def;
    necro_push_apply_fn_vector(&program->apply_fns, &apply_machine_def);

    // TODO: Maybe need mk_fn? Maybe not?

    necro_rewind_arena(&program->snapshot_arena, snapshot);
}

void necro_declare_apply_fns(NecroMachineProgram* program)
{
    assert(program != NULL);
    necro_declare_apply_fn(program);
    // for (size_t i = 2; i < 12; ++i)
    // {
    //     necro_declare_apply_fn(program, i);
    // }
}

NecroMachineAST* necro_get_apply_fn(NecroMachineProgram* program, size_t apply_arity)
{
    return program->apply_fns.data[0];
    // assert(apply_arity > 0);
    // size_t adjusted_apply_arity = apply_arity - 1;
    // if (program->apply_fns.length > adjusted_apply_arity)
    //     return program->apply_fns.data[adjusted_apply_arity];
    // necro_declare_apply_fn(program, apply_arity);
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
    // assert(program->apply_fns.length > adjusted_apply_arity);
    // assert(program->apply_fns.data[adjusted_apply_arity] != NULL);
    // return program->apply_fns.data[adjusted_apply_arity];
}

// // TODO: Figure out exactly how to do this
// void necro_core_to_machine_1_stack_array(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
// {
//     assert(program != NULL);
//     assert(ast != NULL);
//     assert(ast->expr_type == NECRO_CORE_EXPR_APP);
//     // assert(ast->app.exprA->expr_type == NECRO_CORE_EXPR_VAR);
//     // assert(ast->app.exprA->var.id.id == program->prim_types->_stack_array_type.id.id);
//     // assert(ast->app.exprB->expr_type == NECRO_CORE_EXPR_APP);
//     necro_core_to_machine_1_go(program, ast->app.exprB, outer);
// }

void necro_core_to_machine_2_stack_array(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    // assert(ast->app.exprA->expr_type == NECRO_CORE_EXPR_VAR);
    // assert(ast->app.exprA->var.id.id == program->prim_types->_stack_array_type.id.id);
    necro_core_to_machine_2_go(program, ast->app.exprB, outer);
}

NecroMachineAST* necro_core_to_machine_3_stack_array(NecroMachineProgram* program, NecroCoreAST_Expression* ast, NecroMachineAST* outer, size_t arg_count)
{
    assert(program != NULL);
    assert(ast != NULL);
    assert(ast->expr_type == NECRO_CORE_EXPR_APP);
    // assert(ast->app.exprA->expr_type == NECRO_CORE_EXPR_VAR);
    // assert(ast->app.exprA->var.id.id == program->prim_types->_stack_array_type.id.id);
    // // NecroMachineAST* data_ptr = necro_build_alloca(program, )
    NecroMachineAST*   data_ptr  = necro_build_alloca(program, outer->machine_def.update_fn, arg_count);
    // NecroArenaSnapshot snapshot  = necro_get_arena_snapshot(&program->snapshot_arena);
    // NecroMachineAST**  args      = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroMachineAST*));
    size_t             arg_index = arg_count - 1;
    while (ast->expr_type == NECRO_CORE_EXPR_APP)
    {
        NecroMachineAST* arg     = necro_core_to_machine_3_go(program, ast->app.exprB, outer);
        NecroMachineAST* arg_ptr = necro_build_gep(program, outer->machine_def.update_fn, data_ptr, (uint32_t[]) { arg_index }, 1, "arg_ptr");
        necro_build_store_into_ptr(program, outer->machine_def.update_fn, arg, arg_ptr);
        arg_index--;
        ast = ast->app.exprA;
    }
    return data_ptr;
}

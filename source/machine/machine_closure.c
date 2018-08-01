/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_closure.h"
#include "machine.h"
#include "machine_build.h"
#include "symtable.h"

NecroMachineAST* necro_get_closure_con(NecroMachineProgram* program, size_t closure_arity, bool is_constant)
{
    assert(program != NULL);
    size_t adjusted_closure_arity = closure_arity - 3;
    while (program->closure_cons.length < ((adjusted_closure_arity * 2) + 2))
    {
        NecroMachineAST* null_ast = NULL;
        necro_push_closure_con_vector(&program->closure_cons, &null_ast);
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
    NecroMachineAST* actual_type        = necro_create_machine_struct_def(program, struct_var, elems, closure_arity);

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

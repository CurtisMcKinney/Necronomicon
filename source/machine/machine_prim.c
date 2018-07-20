/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_prim.h"
#include "machine_type.h"
#include "runtime/runtime.h"
#include "machine_build.h"

///////////////////////////////////////////////////////
// Utility declarations
///////////////////////////////////////////////////////

// Use this for simple all in one package
NecroMachineType* necro_create_prim_type(NecroMachineProgram* program, NecroVar type_name, NecroVar con_var, NecroMachineType** elems, size_t num_elems)
{
    assert(num_elems >= 1);
    assert(type_name.id.id != 0);
    assert(con_var.id.id != 0);
    NecroArenaSnapshot    snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineAST*      struct_type     = necro_create_machine_struct_def(program, type_name, elems, num_elems);
    NecroMachineType*     struct_ptr_type = necro_create_machine_ptr_type(&program->arena, struct_type->necro_machine_type);
    const char*           mk_fn_name      = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(program->intern, con_var.symbol), "#" });
    NecroVar              mk_fn_var       = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType*     mk_fn_type      = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems + 1, num_elems - 1);
    NecroMachineAST*      mk_fn_body      = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*      mk_fn_def       = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    NecroMachineAST*      data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type->necro_machine_type, (uint16_t) num_elems - 1);
    // necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_machine_value(program, 0), data_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < num_elems - 1; ++i)
    {
        necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
    }

    necro_symtable_get(program->symtable, con_var.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
    necro_build_return(program, mk_fn_def, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return struct_type->necro_machine_type;
}

// Use this for when you want something more involved, like sum types
NecroMachineAST* necro_create_prim_con(NecroMachineProgram* program, NecroMachineType* struct_type, NecroVar con_var, NecroMachineType** elems, size_t num_elems, uint16_t slots_used, int32_t tag)
{
    assert(struct_type != NULL);
    assert(struct_type->type == NECRO_MACHINE_TYPE_STRUCT);
    assert(con_var.id.id != 0);
    NecroArenaSnapshot    snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineType*     struct_ptr_type = necro_create_machine_ptr_type(&program->arena, struct_type);
    const char*           mk_fn_name      = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(program->intern, con_var.symbol), "#" });
    NecroVar              mk_fn_var       = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType*     mk_fn_type      = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems, num_elems);
    NecroMachineAST*      mk_fn_body      = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*      mk_fn_def       = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    NecroMachineAST*      data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type, slots_used);
    if (tag > - 1)
        necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_machine_value(program, tag), data_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < num_elems; ++i)
    {
        necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
    }
    for (size_t i = num_elems + 1; i < struct_type->struct_type.num_members; ++i)
    {
        if (i > 0)
            necro_build_store_into_slot(program, mk_fn_def, necro_create_null_necro_machine_value(program, program->necro_poly_ptr_type), data_ptr, i);
    }

    necro_symtable_get(program->symtable, con_var.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
    necro_build_return(program, mk_fn_def, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return mk_fn_def;
}

NecroMachineAST* necro_prim_fn_begin(NecroMachineProgram* program, NecroVar fn_var, NecroMachineType* return_type, NecroMachineType** params, size_t num_params)
{
    NecroMachineType* fn_type = necro_create_machine_fn_type(&program->arena, return_type, params, num_params);
    NecroMachineAST*  fn_body = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*  fn_def  = necro_create_machine_fn(program, fn_var, fn_body, fn_type);
    return fn_def;
}


void necro_prim_fn_end(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* return_value)
{
    // necro_symtable_get(program->symtable, fn_def->fn_def.name.id)->necro_machine_ast = fn_def->fn_def.fn_value;
    necro_symtable_get(program->symtable, fn_def->fn_def.name.id)->necro_machine_ast = fn_def;
    necro_build_return(program, fn_def, return_value);
}

// void necro_create_prim_binop(NecroMachineProgram* program, const char* fn_name, NecroMachineAST* mk_fn_value, NecroMachineType* type, NECRO_MACHINE_BINOP_TYPE op)
// {
//     NecroVar         binop_var      = necro_get_top_level_symbol_var(program->scoped_symtable, fn_name);
//     NecroMachineAST* binop_fn       = necro_prim_fn_begin(program, binop_var, type, (NecroMachineType*[]) { type, type }, 2);
//     NecroMachineAST* binop_l_unbox  = necro_build_load_from_slot(program, binop_fn, necro_create_param_reg(program, binop_fn, 0), 1, "lhs");
//     NecroMachineAST* binop_r_unbox  = necro_build_load_from_slot(program, binop_fn, necro_create_param_reg(program, binop_fn, 1), 1, "rhs");
//     NecroMachineAST* bin_op_result  = necro_build_binop(program, binop_fn, binop_l_unbox, binop_r_unbox, op);
//     NecroMachineAST* binop_box      = necro_build_call(program, binop_fn, mk_fn_value, (NecroMachineAST*[]) { bin_op_result }, 1, "box");
//     necro_prim_fn_end(program, binop_fn, binop_box);
// }

void necro_create_prim_binop(NecroMachineProgram* program, const char* fn_name, NecroMachineAST* mk_fn_value, NecroMachineType* type, NECRO_MACHINE_BINOP_TYPE op)
{
    NecroVar         binop_var      = necro_get_top_level_symbol_var(program->scoped_symtable, fn_name);
    NecroMachineAST* binop_fn       = necro_prim_fn_begin(program, binop_var, type, (NecroMachineType*[]) { type, type }, 2);
    NecroMachineAST* bin_op_result  = necro_build_binop(program, binop_fn, necro_create_param_reg(program, binop_fn, 0), necro_create_param_reg(program, binop_fn, 1), op);
    necro_prim_fn_end(program, binop_fn, bin_op_result);
}

void necro_create_prim_binops(NecroMachineProgram* program, NecroMachineType* type, NecroMachineAST* mk_fn_value, const char** fn_names, NECRO_MACHINE_BINOP_TYPE* ops, size_t num_ops)
{
    for (size_t i = 0; i < num_ops; ++i)
        necro_create_prim_binop(program, fn_names[i], mk_fn_value, type, ops[i]);
}

// Note: assumes default _mk_fn
// Note: assumes that YOU define the update_fn once the machine_def is returned to you
NecroMachineAST* necro_create_stateful_fn_machine_def(NecroMachineProgram* program, NecroVar fn_name, NecroMachineType* value_type, NecroMachineType** members, size_t num_members, size_t num_args, NecroVar* out_update_fn_var)
{
    NecroArenaSnapshot snapshot            = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineAST*   machine_def         = necro_create_machine_initial_machine_def(program, fn_name, NULL, value_type);
    machine_def->necro_machine_type        = necro_create_machine_struct_type(&program->arena, machine_def->machine_def.machine_name, members, num_members);
    machine_def->machine_def.state_type    = NECRO_STATE_STATEFUL;
    machine_def->machine_def.num_arg_names = num_args;
    necro_rewind_arena(&program->snapshot_arena, snapshot);

    // mk_fn
    const char*       mk_fn_name   = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) + 1 });
    NecroVar          mk_fn_var    = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType* mk_fn_type   = necro_create_machine_fn_type(&program->arena, necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type), NULL, 0);
    NecroMachineAST*  mk_fn_body   = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*  mk_fn_def    = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    program->functions.length--; // HACK: Don't want the mk function in the functions list, instead it belongs to the machine
    machine_def->machine_def.mk_fn = mk_fn_def;
    machine_def->machine_def.mk_fn->necro_machine_type->fn_type.return_type = necro_create_machine_ptr_type(&program->arena, machine_def->necro_machine_type);
    NecroMachineAST*  data_ptr     = necro_build_nalloc(program, machine_def->machine_def.mk_fn, machine_def->necro_machine_type, (uint16_t) (num_members - 1));
    necro_build_return(program, machine_def->machine_def.mk_fn, data_ptr);

    const char* update_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_update", necro_intern_get_string(program->intern, machine_def->machine_def.machine_name.symbol) + 1 });
    *out_update_fn_var      = necro_gen_var(program, NULL, update_name, NECRO_NAME_UNIQUE);

    return machine_def;
}

///////////////////////////////////////////////////////
// Init Machine Prim
///////////////////////////////////////////////////////
void necro_init_machine_prim(NecroMachineProgram* program)
{
    //--------------------
    // Runtime functions
    //--------------------
    NecroVar          _necro_init_runtime_var     = necro_gen_var(program, NULL, "_necro_init_runtime", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_init_runtime_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
    NecroMachineAST*  _necro_init_runtime_fn      = necro_create_machine_runtime_fn(program, _necro_init_runtime_var, _necro_init_runtime_fn_type, _necro_init_runtime);
    program->runtime._necro_init_runtime          = _necro_init_runtime_var;

    NecroVar          _necro_update_runtime_var     = necro_gen_var(program, NULL, "_necro_update_runtime", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_update_runtime_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
    NecroMachineAST*  _necro_update_runtime_fn      = necro_create_machine_runtime_fn(program, _necro_update_runtime_var, _necro_update_runtime_fn_type, _necro_update_runtime);
    program->runtime._necro_update_runtime          = _necro_update_runtime_var;

    NecroVar          _necro_error_exit_var     = necro_gen_var(program, NULL, "_necro_error_exit", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_error_exit_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena) }, 1);
    NecroMachineAST*  _necro_error_exit_fn      = necro_create_machine_runtime_fn(program, _necro_error_exit_var, _necro_error_exit_fn_type, _necro_error_exit);
    program->runtime._necro_error_exit          = _necro_error_exit_var;

    NecroVar          _necro_sleep_var     = necro_gen_var(program, NULL, "_necro_sleep", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_sleep_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena) }, 1);
    NecroMachineAST*  _necro_sleep_fn      = necro_create_machine_runtime_fn(program, _necro_sleep_var, _necro_sleep_fn_type, _necro_sleep);
    program->runtime._necro_sleep          = _necro_sleep_var;

    NecroVar          _necro_print_var     = necro_gen_var(program, NULL, "_necro_print", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_print_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_int64_type(&program->arena) }, 1);
    NecroMachineAST*  _necro_print_fn      = necro_create_machine_runtime_fn(program, _necro_print_var, _necro_print_fn_type, _necro_print);
    program->runtime._necro_print          = _necro_print_var;

    NecroVar          _necro_set_root_var     = necro_gen_var(program, NULL, "_necro_set_root", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_set_root_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_ptr_type(&program->arena, necro_create_machine_uint32_type(&program->arena)), necro_create_machine_uint32_type(&program->arena), necro_create_machine_uint32_type(&program->arena) }, 3);
    NecroMachineAST*  _necro_set_root_fn      = necro_create_machine_runtime_fn(program, _necro_set_root_var, _necro_set_root_fn_type, _necro_set_root);
    program->runtime._necro_set_root          = _necro_set_root_var;

    NecroVar          _necro_initialize_root_set_var     = necro_gen_var(program, NULL, "_necro_initialize_root_set", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_initialize_root_set_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena) }, 1);
    NecroMachineAST*  _necro_initialize_root_set_fn      = necro_create_machine_runtime_fn(program, _necro_initialize_root_set_var, _necro_initialize_root_set_fn_type, _necro_initialize_root_set);
    program->runtime._necro_initialize_root_set          = _necro_initialize_root_set_var;

    NecroVar          _necro_collect_var     = necro_gen_var(program, NULL, "_necro_collect", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_collect_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
    NecroMachineAST*  _necro_collect_fn      = necro_create_machine_runtime_fn(program, _necro_collect_var, _necro_collect_fn_type, _necro_collect);
    program->runtime._necro_collect          = _necro_collect_var;

    NecroVar          _necro_alloc_var     = necro_gen_var(program, NULL, "_necro_alloc", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_alloc_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_ptr_type(&program->arena, necro_create_machine_int64_type(&program->arena)), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena), necro_create_machine_uint8_type(&program->arena) }, 2);
    NecroMachineAST*  _necro_alloc_fn      = necro_create_machine_runtime_fn(program, _necro_alloc_var, _necro_alloc_fn_type, _necro_alloc);
    program->runtime._necro_alloc          = _necro_alloc_var;

    NecroVar          _necro_mouse_x_var     = necro_gen_var(program, NULL, "_necro_mouse_x", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_mouse_x_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_int_type, NULL, 0);
    NecroMachineAST*  _necro_mouse_x_fn      = necro_create_machine_runtime_fn(program, _necro_mouse_x_var, _necro_mouse_x_fn_type, _necro_mouse_x);

    NecroVar          _necro_mouse_y_var     = necro_gen_var(program, NULL, "_necro_mouse_y", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_mouse_y_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_int_type, NULL, 0);
    NecroMachineAST*  _necro_mouse_y_fn      = necro_create_machine_runtime_fn(program, _necro_mouse_y_var, _necro_mouse_y_fn_type, _necro_mouse_y);

    //--------------------
    // Prim Types
    //--------------------

    // NecroData
    NecroMachineAST* necro_data_struct = necro_create_machine_struct_def(program, necro_con_to_var(program->prim_types->necro_data_type), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena), necro_create_machine_uint32_type(&program->arena) }, 2);
    program->necro_data_type           = necro_data_struct->necro_machine_type;

    // World
    NecroMachineAST* world_type = necro_create_machine_struct_def(program, necro_con_to_var(program->prim_types->world_type), (NecroMachineType*[]) { program->necro_int_type }, 1);
    program->world_type         = world_type->necro_machine_type;

    // // TheWorldMachine
    // NecroMachineAST* the_world_type = necro_create_machine_struct_def(program, necro_con_to_var(program->prim_types->the_world_type), (NecroMachineType*[]) { necro_data_struct->necro_machine_type, necro_create_machine_ptr_type(&program->arena, world_type->necro_machine_type) }, 2);
    // program->the_world_type         = the_world_type->necro_machine_type;

    // world value
    NecroVar         world_value_var = necro_con_to_var(program->prim_types->world_value);
    NecroMachineAST* world_value     = necro_create_global_value(program, world_value_var, necro_create_machine_ptr_type(&program->arena, world_type->necro_machine_type));
    necro_program_add_global(program, world_value);
    necro_symtable_get(program->symtable, world_value_var.id)->necro_machine_ast = world_value;

    // Poly
    NecroVar         poly_var          = necro_con_to_var(program->prim_types->any_type);
    NecroMachineAST* necro_poly_struct = necro_create_machine_struct_def(program, poly_var,  (NecroMachineType*[]) { program->necro_data_type }, 1);
    program->necro_poly_type           = necro_poly_struct->necro_machine_type;
    program->necro_poly_ptr_type       = necro_create_machine_ptr_type(&program->arena, program->necro_poly_type);

    // Int
    // NecroMachineType* i64_type = necro_create_machine_int64_type(&program->arena);
    NecroVar          int_var  = necro_con_to_var(program->prim_types->int_type);
    // NecroVar          int_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Int").id));
    // NecroMachineAST*  int_type = necro_create_machine_struct_def(program, int_var, (NecroMachineType*[]) { program->necro_data_type, i64_type }, 2);
    // NecroMachineAST*  mk_int   = necro_create_prim_con(program, int_type->necro_machine_type, int_con, (NecroMachineType*[]) { i64_type }, 1, 0, -1);
    // program->mkIntFnValue      = mk_int->fn_def.fn_value;
    necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast = necro_create_word_int_value(program, 0);

    // Float
    // NecroMachineType* f64_type   = necro_create_machine_f64_type(&program->arena);
    NecroVar          float_var  = necro_con_to_var(program->prim_types->float_type);
    // NecroVar          float_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Float").id));
    // NecroMachineAST*  float_type = necro_create_machine_struct_def(program, float_var, (NecroMachineType*[]) { program->necro_data_type, f64_type }, 2);
    // NecroMachineAST*  mk_float   = necro_create_prim_con(program, float_type->necro_machine_type, float_con, (NecroMachineType*[]) { f64_type }, 1, 0, -1);
    // program->mkFloatFnValue      = mk_float->fn_def.fn_value;
    necro_symtable_get(program->symtable, float_var.id)->necro_machine_ast = necro_create_word_float_value(program, 0.f);

    // Rational
    NecroVar          rational_var  = necro_con_to_var(program->prim_types->rational_type);
    NecroVar          rational_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Rational").id));
    NecroMachineType* rational_type = necro_create_prim_type(program, rational_var, rational_con, (NecroMachineType*[]) { program->necro_data_type, program->necro_int_type, program->necro_int_type}, 3);
    // program->mkFloatFnValue      = fromInt_Int_fn->fn_def.fn_value;

    // Audio
    NecroVar          audio_var  = necro_con_to_var(program->prim_types->audio_type);
    NecroVar          audio_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Audio").id));
    NecroMachineType* audio_type = necro_create_prim_type(program, audio_var, audio_con, (NecroMachineType*[]) { program->necro_data_type, necro_create_machine_ptr_type(&program->arena, necro_create_machine_f32_type(&program->arena)) }, 2);

    // ()
    NecroVar unit_var = necro_con_to_var(program->prim_types->unit_type);
    NecroVar unit_con = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, unit_var.symbol.id));
    necro_create_prim_type(program, unit_var, unit_con, (NecroMachineType*[]) { program->necro_data_type }, 1);

    // []
    NecroVar         list_var    = necro_con_to_var(program->prim_types->list_type);
    NecroVar         cons_con    = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, ":").id));
    NecroVar         nil_con     = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "[]").id));
    NecroMachineAST* list_struct = necro_create_machine_struct_def(program, list_var, (NecroMachineType*[]) { program->necro_data_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
    necro_create_prim_con(program, list_struct->necro_machine_type, cons_con, (NecroMachineType*[]) { program->necro_poly_ptr_type, necro_create_machine_ptr_type(&program->arena, list_struct->necro_machine_type) }, 2, 2, 0);
    necro_create_prim_con(program, list_struct->necro_machine_type, nil_con, NULL, 0, 0, 1);

    //--------------------
    // Prim Functions
    //--------------------
    // Int functions
    // NecroMachineType* int_ptr_type    = necro_create_machine_ptr_type(&program->arena, int_type->necro_machine_type);
    NecroVar          fromInt_Int_var = necro_get_top_level_symbol_var(program->scoped_symtable, "fromInt@Int");
    NecroMachineAST*  fromInt_Int_fn  = necro_prim_fn_begin(program, fromInt_Int_var, program->necro_int_type, (NecroMachineType*[]) { program->necro_int_type }, 1);
    necro_prim_fn_end(program, fromInt_Int_fn, necro_create_param_reg(program, fromInt_Int_fn, 0));
    necro_create_prim_binops(program, program->necro_int_type, program->mkIntFnValue, (const char*[]) { "add@Int", "sub@Int", "mul@Int", "div@Int" }, (NECRO_MACHINE_BINOP_TYPE[]) { NECRO_MACHINE_BINOP_IADD, NECRO_MACHINE_BINOP_ISUB, NECRO_MACHINE_BINOP_IMUL, NECRO_MACHINE_BINOP_IDIV }, 3);

    // Float functions
    // NecroMachineType* float_ptr_type         = necro_create_machine_ptr_type(&program->arena, float_type->necro_machine_type);
    NecroVar          fromRational_Float_var = necro_get_top_level_symbol_var(program->scoped_symtable, "fromRational@Float");
    NecroMachineAST*  fromRational_Float_fn  = necro_prim_fn_begin(program, fromRational_Float_var, program->necro_float_type, (NecroMachineType*[]) { program->necro_float_type }, 1);
    necro_prim_fn_end(program, fromRational_Float_fn, necro_create_param_reg(program, fromRational_Float_fn, 0));
    necro_create_prim_binops(program, program->necro_float_type, program->mkFloatFnValue, (const char*[]) { "add@Float", "sub@Float", "mul@Float", "div@Float" }, (NECRO_MACHINE_BINOP_TYPE[]) { NECRO_MACHINE_BINOP_FADD, NECRO_MACHINE_BINOP_FSUB, NECRO_MACHINE_BINOP_FMUL, NECRO_MACHINE_BINOP_FDIV }, 4);

    // getMouseX
    NecroVar          get_mouse_x_var   = necro_con_to_var(program->prim_types->mouse_x_fn);
    NecroMachineAST*  get_mouse_x_fn    = necro_prim_fn_begin(program, get_mouse_x_var, program->necro_int_type, (NecroMachineType*[]) { world_type->necro_machine_type }, 1);
    NecroMachineAST*  mouse_x_value_val = necro_build_call(program, get_mouse_x_fn, _necro_mouse_x_fn->fn_def.fn_value, NULL, 0, "xval");
    // NecroMachineAST*  boxed_mouse_x_val = necro_build_call(program, get_mouse_x_fn, program->mkIntFnValue, (NecroMachineAST*[]) { mouse_x_value_val }, 1, "xbox");
    necro_prim_fn_end(program, get_mouse_x_fn, mouse_x_value_val);
    get_mouse_x_fn->fn_def.is_primitively_stateful = true;

    // getMouseY
    NecroVar          get_mouse_y_var   = necro_con_to_var(program->prim_types->mouse_y_fn);
    NecroMachineAST*  get_mouse_y_fn    = necro_prim_fn_begin(program, get_mouse_y_var, program->necro_int_type, (NecroMachineType*[]) { world_type->necro_machine_type }, 1);
    NecroMachineAST*  mouse_y_value_val = necro_build_call(program, get_mouse_y_fn, _necro_mouse_y_fn->fn_def.fn_value, NULL, 0, "yval");
    // NecroMachineAST*  boxed_mouse_y_val = necro_build_call(program, get_mouse_y_fn, program->mkIntFnValue, (NecroMachineAST*[]) { mouse_y_value_val }, 1, "ybox");
    necro_prim_fn_end(program, get_mouse_y_fn, mouse_y_value_val);
    get_mouse_y_fn->fn_def.is_primitively_stateful = true;

    // TODO: redo for new system
    // delay
    {
        NecroVar         delay_var             = necro_con_to_var(program->prim_types->delay_fn);
        NecroVar         delay_update_name;
        NecroMachineAST* delay_machine_def     = necro_create_stateful_fn_machine_def(program, delay_var, program->necro_poly_type, (NecroMachineType*[]) { program->necro_uint_type }, 1, 3, &delay_update_name);
        NecroMachineAST* delay_update_fn       = necro_prim_fn_begin(program, delay_update_name, program->necro_poly_ptr_type, (NecroMachineType*[]) { necro_create_machine_ptr_type(&program->arena, delay_machine_def->necro_machine_type), program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
        delay_machine_def->machine_def.fn_type = necro_create_machine_fn_type(&program->arena, program->necro_poly_ptr_type, (NecroMachineType*[]) { program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 2);
        NecroMachineAST* delay_buf             = necro_build_load_from_slot(program, delay_update_fn, necro_create_param_reg(program, delay_update_fn, 0), 0, "buf");
        NecroMachineAST* delay_is_init         = necro_build_cmp(program, delay_update_fn, NECRO_MACHINE_CMP_EQ, delay_buf, necro_create_word_uint_value(program, 0));
        NecroMachineAST* delay_init            = necro_append_block(program, delay_update_fn, "init");
        NecroMachineAST* delay_cont            = necro_append_block(program, delay_update_fn, "cont");
        necro_build_cond_break(program, delay_update_fn, delay_is_init, delay_init, delay_cont);
        necro_move_to_block(program, delay_update_fn, delay_init);
        necro_build_store_into_slot(program, delay_update_fn, necro_create_word_uint_value(program, 1), necro_create_param_reg(program, delay_update_fn, 0), 0);
        necro_build_return(program, delay_update_fn, necro_create_param_reg(program, delay_update_fn, 1));
        necro_move_to_block(program, delay_update_fn, delay_cont);
        necro_prim_fn_end(program, delay_update_fn, necro_create_param_reg(program, delay_update_fn, 2));
        delay_machine_def->machine_def.update_fn = delay_update_fn;
    }
}

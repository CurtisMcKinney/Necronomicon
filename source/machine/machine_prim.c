/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_prim.h"
// #include "machine_type.h"
// #include "runtime/runtime.h"
// #include "machine_build.h"
//
// ///////////////////////////////////////////////////////
// // Utility declarations
// ///////////////////////////////////////////////////////
//
// // Use this for simple all in one package
// NecroMachineType* necro_create_prim_type(NecroMachineProgram* program, NecroVar type_name, NecroVar con_var, NecroMachineType** elems, size_t num_elems)
// {
//     assert(num_elems >= 1);
//     assert(type_name.id.id != 0);
//     assert(con_var.id.id != 0);
//     NecroArenaSnapshot snapshot         = necro_snapshot_arena_get(&program->snapshot_arena);
//     NecroMachineAST*   struct_type      = necro_create_machine_struct_def(program, type_name, elems, num_elems);
//     NecroMachineType*  struct_ptr_type  = necro_create_machine_ptr_type(&program->arena, struct_type->necro_machine_type);
//     const char*        mk_fn_name       = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", con_var.symbol->str });
//     // const char*        const_mk_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkConst", necro_intern_get_string(program->intern, con_var.symbol) });
//     NecroVar           mk_fn_var        = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
//     // NecroVar           const_mk_fn_var  = necro_gen_var(program, NULL, const_mk_fn_name, NECRO_NAME_UNIQUE);
//     // for (size_t c = 0; c < 2; ++c)
//     // {
//         // mk_fn_var                    = (c == 0) ? mk_fn_var : const_mk_fn_var;
//         /* mk_fn_var                    = mk_fn_var; @curtis: not sure why you assigned this to itself, commented out for now */
//         NecroMachineType* mk_fn_type = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems + 1, num_elems - 1);
//         NecroMachineAST*  mk_fn_body = necro_create_machine_block(program, "entry", NULL);
//         NecroMachineAST*  mk_fn_def  = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
//         NecroMachineAST*  data_ptr   = necro_build_nalloc(program, mk_fn_def, struct_type->necro_machine_type, (uint16_t)num_elems);
//         // necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_machine_value(program, 0), data_ptr);
//
//         //--------------
//         // Parameters
//         for (size_t i = 0; i < num_elems - 1; ++i)
//         {
//             necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
//         }
//
//         // if (c == 0)
//             necro_symtable_get(program->symtable, con_var.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
//         // else
//         //     necro_symtable_get(program->symtable, con_var.id)->const_necro_machine_ast = mk_fn_def->fn_def.fn_value;
//         necro_build_return(program, mk_fn_def, data_ptr);
//     // }
//     necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
//     return struct_type->necro_machine_type;
// }
//
// // Use this for when you want something more involved, like sum types
// void necro_create_prim_con(NecroMachineProgram* program, NecroMachineType* struct_type, NecroVar con_var, NecroMachineType** elems, size_t num_elems, uint16_t slots_used, uint32_t tag)
// {
//     assert(struct_type != NULL);
//     assert(struct_type->type == NECRO_MACHINE_TYPE_STRUCT);
//     assert(con_var.id.id != 0);
//     NecroArenaSnapshot    snapshot         = necro_snapshot_arena_get(&program->snapshot_arena);
//     NecroMachineType*     struct_ptr_type  = necro_create_machine_ptr_type(&program->arena, struct_type);
//     const char*           mk_fn_name       = necro_snapshot_arena_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mk", con_var.symbol->str });
//     // const char*           const_mk_fn_name = necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { "_mkConst", necro_intern_get_string(program->intern, con_var.symbol) });
//     NecroVar              mk_fn_var        = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
//     // NecroVar              const_mk_fn_var  = necro_gen_var(program, NULL, const_mk_fn_name, NECRO_NAME_UNIQUE);
//     // for (size_t c = 0; c < 2; ++c)
//     // {
//         // mk_fn_var                    = (c == 0) ? mk_fn_var : const_mk_fn_var;
//         /* mk_fn_var                    = mk_fn_var; @curtis: not sure what you wanted, commented out for now */
//         NecroMachineType* mk_fn_type = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems, num_elems);
//         NecroMachineAST*  mk_fn_body = necro_create_machine_block(program, "entry", NULL);
//         NecroMachineAST*  mk_fn_def  = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
//         NecroMachineAST*  data_ptr   = necro_build_nalloc(program, mk_fn_def, struct_type, slots_used + 1);
//         necro_build_store_into_slot(program, mk_fn_def, necro_create_word_uint_value(program, tag), data_ptr, 0);
//
//         //--------------
//         // Parameters
//         for (size_t i = 0; i < num_elems; ++i)
//         {
//             necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
//         }
//
//         // if (c == 0)
//             necro_symtable_get(program->symtable, con_var.id)->necro_machine_ast = mk_fn_def->fn_def.fn_value;
//         // else
//         //     necro_symtable_get(program->symtable, con_var.id)->const_necro_machine_ast = mk_fn_def->fn_def.fn_value;
//         necro_build_return(program, mk_fn_def, data_ptr);
//     // }
//     necro_snapshot_arena_rewind(&program->snapshot_arena, snapshot);
// }
//
// NecroMachineAST* necro_prim_fn_begin(NecroMachineProgram* program, NecroVar fn_var, NecroMachineType* return_type, NecroMachineType** params, size_t num_params)
// {
//     NecroMachineType* fn_type = necro_create_machine_fn_type(&program->arena, return_type, params, num_params);
//     NecroMachineAST*  fn_body = necro_create_machine_block(program, "entry", NULL);
//     NecroMachineAST*  fn_def  = necro_create_machine_fn(program, fn_var, fn_body, fn_type);
//     return fn_def;
// }
//
//
// void necro_prim_fn_end(NecroMachineProgram* program, NecroMachineAST* fn_def, NecroMachineAST* return_value)
// {
//     // necro_symtable_get(program->symtable, fn_def->fn_def.name.id)->necro_machine_ast = fn_def->fn_def.fn_value;
//     necro_symtable_get(program->symtable, fn_def->fn_def.name.id)->necro_machine_ast = fn_def;
//     necro_build_return(program, fn_def, return_value);
// }
//
// void necro_create_prim_binop(NecroMachineProgram* program, const char* fn_name, NecroMachineType* type, NECRO_MACHINE_BINOP_TYPE op)
// {
//     UNUSED(program);
//     UNUSED(fn_name);
//     UNUSED(type);
//     UNUSED(op);
//     // TODO / NOTE commented out because of API change
//     // NecroVar         binop_var      = necro_scoped_symtable_get_top_level_symbol_var(program->scoped_symtable, fn_name);
//     // NecroMachineAST* binop_fn       = necro_prim_fn_begin(program, binop_var, type, (NecroMachineType*[]) { type, type }, 2);
//     // NecroMachineAST* bin_op_result  = necro_build_binop(program, binop_fn, necro_create_param_reg(program, binop_fn, 0), necro_create_param_reg(program, binop_fn, 1), op);
//     // necro_prim_fn_end(program, binop_fn, bin_op_result);
// }
//
// void necro_create_prim_binops(NecroMachineProgram* program, NecroMachineType* type, const char** fn_names, NECRO_MACHINE_BINOP_TYPE* ops, size_t num_ops)
// {
//     for (size_t i = 0; i < num_ops; ++i)
//         necro_create_prim_binop(program, fn_names[i], type, ops[i]);
// }
//
// void necro_create_prim_cmp(NecroMachineProgram* program, const char* fn_name, NecroMachineType* type, NECRO_MACHINE_CMP_TYPE op)
// {
//     UNUSED(program);
//     UNUSED(fn_name);
//     UNUSED(type);
//     UNUSED(op);
//     // TODO / NOTE commented out because of API change
//     // NecroVar         cmp_var    = necro_scoped_symtable_get_top_level_symbol_var(program->scoped_symtable, fn_name);
//     // NecroMachineAST* cmp_fn     = necro_prim_fn_begin(program, cmp_var, program->necro_int_type, (NecroMachineType*[]) { type, type }, 2);
//     // NecroMachineAST* cmp_result = necro_build_cmp(program, cmp_fn, op, necro_create_param_reg(program, cmp_fn, 0), necro_create_param_reg(program, cmp_fn, 1));
//     // cmp_result                  = necro_build_zext(program, cmp_fn, cmp_result, cmp_fn->necro_machine_type->fn_type.return_type);
//     // necro_prim_fn_end(program, cmp_fn, cmp_result);
// }
//
// void necro_create_prim_cmps(NecroMachineProgram* program, NecroMachineType* type, const char** fn_names, NECRO_MACHINE_CMP_TYPE* ops, size_t num_ops)
// {
//     for (size_t i = 0; i < num_ops; ++i)
//         necro_create_prim_cmp(program, fn_names[i], type, ops[i]);
// }
//
// ///////////////////////////////////////////////////////
// // Init Machine Prim
// ///////////////////////////////////////////////////////
// void necro_init_machine_prim(NecroMachineProgram* program)
// {
//     //--------------------
//     // Runtime functions
//     //--------------------
//     NecroVar          _necro_init_runtime_var     = necro_gen_var(program, NULL, "_necro_init_runtime", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_init_runtime_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
//     NecroMachineAST*  _necro_init_runtime_fn      = necro_create_machine_runtime_fn(program, _necro_init_runtime_var, _necro_init_runtime_fn_type, (NecroMachineFnPtr) _necro_init_runtime, NECRO_STATE_POINTWISE);
//     UNUSED(_necro_init_runtime_fn);
//     program->runtime._necro_init_runtime          = _necro_init_runtime_var;
//
//     NecroVar          _necro_update_runtime_var     = necro_gen_var(program, NULL, "_necro_update_runtime", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_update_runtime_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
//     NecroMachineAST*  _necro_update_runtime_fn      = necro_create_machine_runtime_fn(program, _necro_update_runtime_var, _necro_update_runtime_fn_type, (NecroMachineFnPtr) _necro_update_runtime, NECRO_STATE_POINTWISE);
//     UNUSED(_necro_update_runtime_fn);
//     program->runtime._necro_update_runtime          = _necro_update_runtime_var;
//
//     NecroVar          _necro_error_exit_var     = necro_gen_var(program, NULL, "_necro_error_exit", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_error_exit_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena) }, 1);
//     NecroMachineAST*  _necro_error_exit_fn      = necro_create_machine_runtime_fn(program, _necro_error_exit_var, _necro_error_exit_fn_type, (NecroMachineFnPtr) _necro_error_exit, NECRO_STATE_CONSTANT);
//     UNUSED(_necro_error_exit_fn);
//     program->runtime._necro_error_exit          = _necro_error_exit_var;
//
//     NecroVar          _necro_sleep_var     = necro_gen_var(program, NULL, "_necro_sleep", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_sleep_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena) }, 1);
//     NecroMachineAST*  _necro_sleep_fn      = necro_create_machine_runtime_fn(program, _necro_sleep_var, _necro_sleep_fn_type, (NecroMachineFnPtr) _necro_sleep, NECRO_STATE_CONSTANT);
//     UNUSED(_necro_sleep_fn);
//     program->runtime._necro_sleep          = _necro_sleep_var;
//
//     NecroVar          _necro_print_var     = necro_gen_var(program, NULL, "_necro_print", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_print_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_word_sized_int_type(program) }, 1);
//     NecroMachineAST*  _necro_print_fn      = necro_create_machine_runtime_fn(program, _necro_print_var, _necro_print_fn_type, (NecroMachineFnPtr) _necro_print, NECRO_STATE_CONSTANT);
//     UNUSED(_necro_print_fn);
//     program->runtime._necro_print          = _necro_print_var;
//
//     NecroVar          _necro_debug_print_var     = necro_gen_var(program, NULL, "_necro_debug_print", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_debug_print_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_word_sized_int_type(program) }, 1);
//     NecroMachineAST*  _necro_debug_print_fn      = necro_create_machine_runtime_fn(program, _necro_debug_print_var, _necro_debug_print_fn_type, (NecroMachineFnPtr) _necro_debug_print, NECRO_STATE_CONSTANT);
//     UNUSED(_necro_debug_print_fn);
//     program->runtime._necro_debug_print          = _necro_debug_print_var;
//
//     // NecroVar          _necro_mouse_x_var     = necro_gen_var(program, NULL, "_necro_mouse_x", NECRO_NAME_UNIQUE);
//     // NecroMachineType* _necro_mouse_x_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_int_type, NULL, 0);
//     // NecroMachineAST*  _necro_mouse_x_fn      = necro_create_machine_runtime_fn(program, _necro_mouse_x_var, _necro_mouse_x_fn_type, (NecroMachineFnPtr) _necro_mouse_x, NECRO_STATE_POINTWISE);
//
//     // NecroVar          _necro_mouse_y_var     = necro_gen_var(program, NULL, "_necro_mouse_y", NECRO_NAME_UNIQUE);
//     // NecroMachineType* _necro_mouse_y_fn_type = necro_create_machine_fn_type(&program->arena, program->necro_int_type, NULL, 0);
//     // NecroMachineAST*  _necro_mouse_y_fn      = necro_create_machine_runtime_fn(program, _necro_mouse_y_var, _necro_mouse_y_fn_type, (NecroMachineFnPtr) _necro_mouse_y, NECRO_STATE_POINTWISE);
//
//     //--------------------
//     // New Copy GC
//     //--------------------
//     NecroVar          _necro_copy_gc_initialize_root_set_var     = necro_gen_var(program, NULL, "_necro_copy_gc_initialize_root_set", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_copy_gc_initialize_root_set_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_word_sized_uint_type(program) }, 1);
//     NecroMachineAST*  _necro_copy_gc_initialize_root_set_fn      = necro_create_machine_runtime_fn(program, _necro_copy_gc_initialize_root_set_var, _necro_copy_gc_initialize_root_set_fn_type, (NecroMachineFnPtr) _necro_copy_gc_initialize_root_set, NECRO_STATE_POINTWISE);
//     UNUSED(_necro_copy_gc_initialize_root_set_fn);
//     program->runtime._necro_copy_gc_initialize_root_set          = _necro_copy_gc_initialize_root_set_var;
//
//     NecroVar          _necro_copy_gc_set_root_var     = necro_gen_var(program, NULL, "_necro_copy_gc_set_root", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_copy_gc_set_root_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), (NecroMachineType*[]) { necro_create_machine_ptr_type(&program->arena, necro_create_machine_ptr_type(&program->arena, necro_create_word_sized_int_type(program))), necro_create_word_sized_uint_type(program), necro_create_word_sized_uint_type(program) }, 3);
//     NecroMachineAST*  _necro_copy_gc_set_root_fn      = necro_create_machine_runtime_fn(program, _necro_copy_gc_set_root_var, _necro_copy_gc_set_root_fn_type, (NecroMachineFnPtr) _necro_copy_gc_set_root, NECRO_STATE_POINTWISE);
//     UNUSED(_necro_copy_gc_set_root_fn);
//     program->runtime._necro_copy_gc_set_root          = _necro_copy_gc_set_root_var;
//
//     NecroVar          _necro_copy_gc_collect_var     = necro_gen_var(program, NULL, "_necro_copy_gc_collect", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_copy_gc_collect_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
//     NecroMachineAST*  _necro_copy_gc_collect_fn      = necro_create_machine_runtime_fn(program, _necro_copy_gc_collect_var, _necro_copy_gc_collect_fn_type, (NecroMachineFnPtr) _necro_copy_gc_collect, NECRO_STATE_POINTWISE);
//     UNUSED(_necro_copy_gc_collect_fn);
//     program->runtime._necro_copy_gc_collect          = _necro_copy_gc_collect_var;
//
//     // NecroVar          _necro_from_alloc_var     = necro_gen_var(program, NULL, "_necro_from_alloc", NECRO_NAME_UNIQUE);
//     // NecroMachineType* _necro_from_alloc_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_ptr_type(&program->arena, necro_create_word_sized_int_type(program)), (NecroMachineType*[]) { necro_create_word_sized_uint_type(program) }, 1);
//     // NecroMachineAST*  _necro_from_alloc_fn      = necro_create_machine_runtime_fn(program, _necro_from_alloc_var, _necro_from_alloc_fn_type, (NecroMachineFnPtr) _necro_from_alloc, NECRO_STATE_CONSTANT);
//     // program->runtime._necro_from_alloc          = _necro_from_alloc_var;
//
//     NecroVar          _necro_flip_const_var     = necro_gen_var(program, NULL, "_necro_flip_const", NECRO_NAME_UNIQUE);
//     NecroMachineType* _necro_flip_const_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
//     NecroMachineAST*  _necro_flip_const_fn      = necro_create_machine_runtime_fn(program, _necro_flip_const_var, _necro_flip_const_fn_type, (NecroMachineFnPtr) _necro_flip_const, NECRO_STATE_CONSTANT);
//     UNUSED(_necro_flip_const_fn);
//     program->runtime._necro_flip_const          = _necro_flip_const_var;
//
//     //--------------------
//     // Prim Types
//     //--------------------
//
//     // World
//     NecroMachineAST* world_type = necro_create_machine_struct_def(program, necro_con_to_var(program->prim_types->world_type), (NecroMachineType*[]) { program->necro_int_type }, 1);
//     program->world_type         = world_type->necro_machine_type;
//
//     // world value
//     NecroVar         world_value_var = necro_con_to_var(program->prim_types->world_value);
//     NecroMachineAST* world_value     = necro_create_global_value(program, world_value_var, necro_create_machine_ptr_type(&program->arena, necro_create_machine_ptr_type(&program->arena, world_type->necro_machine_type)));
//     necro_program_add_global(program, world_value);
//     necro_symtable_get(program->symtable, world_value_var.id)->necro_machine_ast = world_value;
//     program->world_value             = world_value;
//
//     // Poly
//     NecroVar         poly_var          = necro_con_to_var(program->prim_types->any_type);
//     NecroMachineAST* necro_poly_struct = necro_create_machine_struct_def(program, poly_var,  (NecroMachineType*[]) { program->necro_uint_type }, 1);
//     program->necro_poly_type           = necro_poly_struct->necro_machine_type;
//     program->necro_poly_ptr_type       = necro_create_machine_ptr_type(&program->arena, program->necro_poly_type);
//
//     // Ptr
//     NecroVar         ptr_var    = necro_con_to_var(program->prim_types->ptr_type);
//     NecroMachineAST* ptr_struct = necro_create_null_necro_machine_value(program,  program->necro_poly_ptr_type);
//     necro_symtable_get(program->symtable, ptr_var.id)->necro_machine_ast = ptr_struct;
//
//     // Int
//     NecroVar          int_var  = necro_con_to_var(program->prim_types->int_type);
//     necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast = necro_create_word_int_value(program, 0);
//
//     // Float
//     NecroVar          float_var  = necro_con_to_var(program->prim_types->float_type);
//     necro_symtable_get(program->symtable, float_var.id)->necro_machine_ast = necro_create_word_float_value(program, 0.f);
//
//     // Char
//     NecroVar char_var = necro_con_to_var(program->prim_types->char_type);
//     necro_symtable_get(program->symtable, char_var.id)->necro_machine_ast = necro_create_word_uint_value(program, 0);
//
//     // _DynState
//     NecroVar         dyn_state_var    = necro_con_to_var(program->prim_types->dyn_state_type);
//     NecroMachineAST* dyn_state_struct = necro_create_machine_struct_def(program, dyn_state_var,  (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type, program->necro_uint_type }, 3);
//     program->dyn_state_type           = dyn_state_struct->necro_machine_type;
//     NecroVar         dyn_state_con    = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "_DynState")->symbol_num));
//     necro_create_prim_con(program, dyn_state_struct->necro_machine_type, dyn_state_con, (NecroMachineType*[]) { program->necro_poly_ptr_type, program->necro_uint_type }, 2, 2, 0);
//     necro_symtable_get(program->symtable, dyn_state_con.id)->arity = 2;
//
//     // _Closure
//     NecroVar         closure_var          = necro_con_to_var(program->prim_types->closure_type);
//     NecroMachineAST* necro_closure_struct = necro_create_machine_struct_def(program, closure_var,  (NecroMachineType*[]) { program->necro_int_type, program->necro_int_type, necro_create_machine_ptr_type(&program->arena, dyn_state_struct->necro_machine_type), program->necro_poly_ptr_type }, 4);
//     program->closure_type                 = necro_closure_struct->necro_machine_type;
//
//     // Rational
//     NecroVar          rational_var  = necro_con_to_var(program->prim_types->rational_type);
//     NecroVar          rational_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Rational")->symbol_num));
//     NecroMachineType* rational_type = necro_create_prim_type(program, rational_var, rational_con, (NecroMachineType*[]) { program->necro_uint_type, program->necro_int_type, program->necro_int_type}, 3);
//     UNUSED(rational_type);
//
//     // Audio
//     NecroVar          audio_var  = necro_con_to_var(program->prim_types->audio_type);
//     NecroVar          audio_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Audio")->symbol_num));
//     NecroMachineType* audio_type = necro_create_prim_type(program, audio_var, audio_con, (NecroMachineType*[]) { program->necro_uint_type, necro_create_machine_ptr_type(&program->arena, necro_create_machine_f32_type(&program->arena)) }, 2);
//     UNUSED(audio_type);
//
//     // () (is_enum)
//     NecroVar unit_var = necro_con_to_var(program->prim_types->unit_type);
//     NecroVar unit_con = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, unit_var.symbol->symbol_num));
//     necro_symtable_get(program->symtable, unit_var.id)->is_enum = 0;
//     necro_symtable_get(program->symtable, unit_var.id)->necro_machine_ast = necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast;
//     necro_symtable_get(program->symtable, unit_con.id)->arity   = 0;
//     necro_symtable_get(program->symtable, unit_con.id)->is_enum = true;
//     necro_symtable_get(program->symtable, unit_con.id)->necro_machine_ast = necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast;
//
//     // Bool (is_enum)
//     NecroVar bool_var  = necro_con_to_var(program->prim_types->bool_type);
//     NecroVar true_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "True")->symbol_num));
//     NecroVar false_con = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "False")->symbol_num));
//     necro_symtable_get(program->symtable, bool_var.id)->is_enum = 0;
//     necro_symtable_get(program->symtable, bool_var.id)->necro_machine_ast = necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast;
//     necro_symtable_get(program->symtable, true_con.id)->arity   = 0;
//     necro_symtable_get(program->symtable, true_con.id)->is_enum = true;
//     necro_symtable_get(program->symtable, true_con.id)->necro_machine_ast = necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast;
//     necro_symtable_get(program->symtable, false_con.id)->arity   = 0;
//     necro_symtable_get(program->symtable, false_con.id)->is_enum = true;
//     necro_symtable_get(program->symtable, false_con.id)->necro_machine_ast = necro_symtable_get(program->symtable, int_var.id)->necro_machine_ast;
//
//     // []
//     NecroVar         list_var    = necro_con_to_var(program->prim_types->list_type);
//     NecroVar         cons_con    = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, ":")->symbol_num));
//     NecroVar         nil_con     = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "[]")->symbol_num));
//     NecroMachineAST* list_struct = necro_create_machine_struct_def(program, list_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
//     necro_create_prim_con(program, list_struct->necro_machine_type, cons_con, (NecroMachineType*[]) { program->necro_poly_ptr_type, necro_create_machine_ptr_type(&program->arena, list_struct->necro_machine_type) }, 2, 2, 0);
//     necro_create_prim_con(program, list_struct->necro_machine_type, nil_con, NULL, 0, 0, 1);
//     necro_symtable_get(program->symtable, nil_con.id)->arity  = 0;
//     necro_symtable_get(program->symtable, cons_con.id)->arity = 2;
//
//     // Maybe
//     NecroVar         maybe_var    = necro_con_to_var(program->prim_types->maybe_type);
//     NecroVar         just_con     = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Just")->symbol_num));
//     NecroVar         nothing_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Nothing")->symbol_num));
//     NecroMachineAST* maybe_struct = necro_create_machine_struct_def(program, maybe_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_poly_ptr_type }, 2);
//     necro_create_prim_con(program, maybe_struct->necro_machine_type, just_con, (NecroMachineType*[]) { program->necro_poly_ptr_type }, 1, 1, 0);
//     necro_create_prim_con(program, maybe_struct->necro_machine_type, nothing_con, NULL, 0, 0, 1);
//     necro_symtable_get(program->symtable, nothing_con.id)->arity = 0;
//     necro_symtable_get(program->symtable, just_con.id)->arity    = 1;
//
//     // Array
//     NecroVar         array_var    = necro_con_to_var(program->prim_types->array_type);
//     NecroVar         array_con    = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Array")->symbol_num));
//     NecroMachineAST* array_struct = necro_create_machine_struct_def(program, array_var, (NecroMachineType*[]) { program->necro_uint_type, program->necro_int_type, necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type) }, 3);
//     necro_create_prim_con(program, array_struct->necro_machine_type, array_con, (NecroMachineType*[]) { program->necro_int_type, necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type) }, 2, 2, 0);
//     necro_symtable_get(program->symtable, array_con.id)->arity = 2;
//
//     // TODO / NOTE: Commented out because of API changes
//     /*
//     //--------------------
//     // Prim Functions
//     //--------------------
//     // Int functions
//     NecroVar          fromInt_Int_var = necro_scoped_symtable_get_top_level_symbol_var(program->scoped_symtable, "fromInt@Int");
//     NecroMachineAST*  fromInt_Int_fn  = necro_prim_fn_begin(program, fromInt_Int_var, program->necro_int_type, (NecroMachineType*[]) { program->necro_int_type }, 1);
//     necro_prim_fn_end(program, fromInt_Int_fn, necro_create_param_reg(program, fromInt_Int_fn, 0));
//     necro_create_prim_binops(program, program->necro_int_type, (const char*[]) { "add@Int", "sub@Int", "mul@Int", "div@Int" }, (NECRO_MACHINE_BINOP_TYPE[]) { NECRO_MACHINE_BINOP_IADD, NECRO_MACHINE_BINOP_ISUB, NECRO_MACHINE_BINOP_IMUL, NECRO_MACHINE_BINOP_IDIV }, 3);
//     necro_create_prim_cmps(program, program->necro_int_type, (const char*[]) { "eq@Int", "neq@Int", "gt@Int", "gte@Int", "lt@Int", "lte@Int" }, (NECRO_MACHINE_CMP_TYPE[]) { NECRO_MACHINE_CMP_EQ, NECRO_MACHINE_CMP_NE, NECRO_MACHINE_CMP_GT, NECRO_MACHINE_CMP_GE, NECRO_MACHINE_CMP_LT, NECRO_MACHINE_CMP_LE }, 6);
//
//     // Float functions
//     NecroVar          fromRational_Float_var = necro_scoped_symtable_get_top_level_symbol_var(program->scoped_symtable, "fromRational@Float");
//     NecroMachineAST*  fromRational_Float_fn  = necro_prim_fn_begin(program, fromRational_Float_var, program->necro_float_type, (NecroMachineType*[]) { program->necro_float_type }, 1);
//     necro_prim_fn_end(program, fromRational_Float_fn, necro_create_param_reg(program, fromRational_Float_fn, 0));
//     necro_create_prim_binops(program, program->necro_float_type, (const char*[]) { "add@Float", "sub@Float", "mul@Float", "div@Float" }, (NECRO_MACHINE_BINOP_TYPE[]) { NECRO_MACHINE_BINOP_FADD, NECRO_MACHINE_BINOP_FSUB, NECRO_MACHINE_BINOP_FMUL, NECRO_MACHINE_BINOP_FDIV }, 4);
//     necro_create_prim_cmps(program, program->necro_float_type, (const char*[]) { "eq@Float", "neq@Float", "gt@Float", "gte@Float", "lt@Float", "lte@Float" }, (NECRO_MACHINE_CMP_TYPE[]) { NECRO_MACHINE_CMP_EQ, NECRO_MACHINE_CMP_NE, NECRO_MACHINE_CMP_GT, NECRO_MACHINE_CMP_GE, NECRO_MACHINE_CMP_LT, NECRO_MACHINE_CMP_LE }, 6);
//
//     // Bool functions
//     necro_create_prim_cmps(program, program->necro_int_type, (const char*[]) { "eq@Bool", "neq@Bool", "gt@Bool", "gte@Bool", "lt@Bool", "lte@Bool" }, (NECRO_MACHINE_CMP_TYPE[]) { NECRO_MACHINE_CMP_EQ, NECRO_MACHINE_CMP_NE, NECRO_MACHINE_CMP_GT, NECRO_MACHINE_CMP_GE, NECRO_MACHINE_CMP_LT, NECRO_MACHINE_CMP_LE }, 6);
//     // And
//     NecroVar          and_var  = necro_scoped_symtable_get_top_level_symbol_var(program->scoped_symtable, "&&");
//     NecroMachineAST*  and_fn   = necro_prim_fn_begin(program, and_var, program->necro_int_type, (NecroMachineType*[]) { program->necro_int_type, program->necro_int_type }, 2);
//     NecroMachineAST*  and_val  = necro_build_binop(program, and_fn, necro_create_param_reg(program, and_fn, 0), necro_create_param_reg(program, and_fn, 1), NECRO_MACHINE_BINOP_AND);
//     necro_prim_fn_end(program, and_fn, and_val);
//     and_fn->fn_def.state_type = NECRO_STATE_CONSTANT;
//     // Or
//     NecroVar          or_var  = necro_scoped_symtable_get_top_level_symbol_var(program->scoped_symtable, "||");
//     NecroMachineAST*  or_fn   = necro_prim_fn_begin(program, or_var, program->necro_int_type, (NecroMachineType*[]) { program->necro_int_type, program->necro_int_type }, 2);
//     NecroMachineAST*  or_val  = necro_build_binop(program, or_fn, necro_create_param_reg(program, or_fn, 0), necro_create_param_reg(program, or_fn, 1), NECRO_MACHINE_BINOP_OR);
//     necro_prim_fn_end(program, or_fn, or_val);
//     or_fn->fn_def.state_type = NECRO_STATE_CONSTANT;
//
//     // getMouseX
//     NecroVar          get_mouse_x_var   = necro_con_to_var(program->prim_types->mouse_x_fn);
//     NecroMachineAST*  get_mouse_x_fn    = necro_prim_fn_begin(program, get_mouse_x_var, program->necro_int_type, (NecroMachineType*[]) { necro_create_machine_ptr_type(&program->arena, world_type->necro_machine_type) }, 1);
//     NecroMachineAST*  mouse_x_value_val = necro_build_call(program, get_mouse_x_fn, _necro_mouse_x_fn->fn_def.fn_value, NULL, 0, NECRO_C_CALL, "xval");
//     necro_prim_fn_end(program, get_mouse_x_fn, mouse_x_value_val);
//     get_mouse_x_fn->fn_def.state_type = NECRO_STATE_POINTWISE;
//
//     // getMouseY
//     NecroVar          get_mouse_y_var   = necro_con_to_var(program->prim_types->mouse_y_fn);
//     NecroMachineAST*  get_mouse_y_fn    = necro_prim_fn_begin(program, get_mouse_y_var, program->necro_int_type, (NecroMachineType*[]) { necro_create_machine_ptr_type(&program->arena, world_type->necro_machine_type) }, 1);
//     NecroMachineAST*  mouse_y_value_val = necro_build_call(program, get_mouse_y_fn, _necro_mouse_y_fn->fn_def.fn_value, NULL, 0, NECRO_C_CALL, "yval");
//     necro_prim_fn_end(program, get_mouse_y_fn, mouse_y_value_val);
//     get_mouse_y_fn->fn_def.state_type = NECRO_STATE_POINTWISE;
//
//     NecroMachineType* ptr_type = necro_create_machine_ptr_type(&program->arena, program->necro_poly_ptr_type);
//     // unsafeMalloc
//     {
//         // TODO: Figure out how to alloc into const space
//         NecroVar         unsafe_malloc_var = necro_con_to_var(program->prim_types->unsafe_malloc);
//         NecroMachineAST* unsafe_malloc_fn  = necro_prim_fn_begin(program, unsafe_malloc_var, ptr_type, (NecroMachineType*[]) { program->necro_int_type }, 1);
//         NecroMachineAST* unsafe_slots_used = necro_build_binop(program, unsafe_malloc_fn, necro_create_param_reg(program, unsafe_malloc_fn, 0), necro_create_word_int_value(program, 1), NECRO_MACHINE_BINOP_IADD);
//         if (necro_word_size == NECRO_WORD_4_BYTES)
//             unsafe_slots_used = necro_build_binop(program, unsafe_malloc_fn, unsafe_slots_used, necro_create_word_int_value(program, 4), NECRO_MACHINE_BINOP_IMUL);
//         else
//             unsafe_slots_used = necro_build_binop(program, unsafe_malloc_fn, unsafe_slots_used, necro_create_word_int_value(program, 8), NECRO_MACHINE_BINOP_IMUL);
//         NecroMachineAST* bit_cast_slots    = necro_build_bit_cast(program, unsafe_malloc_fn, unsafe_slots_used, program->necro_uint_type);
//         NecroMachineAST* unsafe_malloc_val = necro_build_call(program, unsafe_malloc_fn, _necro_from_alloc_fn->fn_def.fn_value, (NecroMachineAST*[]) { bit_cast_slots }, 1, NECRO_C_CALL, "data_ptr");
//         NecroMachineAST* bit_cast_val      = necro_build_bit_cast(program, unsafe_malloc_fn, unsafe_malloc_val, ptr_type);
//         necro_prim_fn_end(program, unsafe_malloc_fn, bit_cast_val);
//         unsafe_malloc_fn->fn_def.state_type = NECRO_STATE_CONSTANT;
//     }
//
//     // unsafePeek
//     {
//         // Int -> Ptr a -> a
//         NecroVar         unsafe_peek_var        = necro_con_to_var(program->prim_types->unsafe_peek);
//         NecroMachineAST* unsafe_peek_fn         = necro_prim_fn_begin(program, unsafe_peek_var, program->necro_poly_ptr_type, (NecroMachineType*[]) { program->necro_int_type, ptr_type }, 2);
//         NecroMachineAST* unsafe_peek_ptr_offset = necro_build_non_const_gep(program, unsafe_peek_fn, necro_create_param_reg(program, unsafe_peek_fn, 1), (NecroMachineAST*[]) { necro_create_param_reg(program, unsafe_peek_fn, 0) }, 1, "offset_ptr", ptr_type);
//         NecroMachineAST* unsafe_peek_ptr_val    = necro_build_load_from_ptr(program, unsafe_peek_fn, unsafe_peek_ptr_offset, "val");
//         necro_prim_fn_end(program, unsafe_peek_fn, unsafe_peek_ptr_val);
//         unsafe_peek_fn->fn_def.state_type = NECRO_STATE_CONSTANT;
//     }
//
//     // unsafePoke
//     {
//         // Int -> a -> Ptr a -> Ptr a
//         NecroVar         unsafe_poke_var        = necro_con_to_var(program->prim_types->unsafe_poke);
//         NecroMachineAST* unsafe_poke_fn         = necro_prim_fn_begin(program, unsafe_poke_var, ptr_type, (NecroMachineType*[]) { program->necro_int_type, program->necro_poly_ptr_type, ptr_type }, 3);
//         NecroMachineAST* unsafe_poke_ptr_offset = necro_build_non_const_gep(program, unsafe_poke_fn, necro_create_param_reg(program, unsafe_poke_fn, 2), (NecroMachineAST*[]) { necro_create_param_reg(program, unsafe_poke_fn, 0) }, 1, "offset_ptr", ptr_type);
//         necro_build_store_into_ptr(program, unsafe_poke_fn, necro_create_param_reg(program, unsafe_poke_fn, 1), unsafe_poke_ptr_offset);
//         necro_prim_fn_end(program, unsafe_poke_fn, necro_create_param_reg(program, unsafe_poke_fn, 2));
//         unsafe_poke_fn->fn_def.state_type = NECRO_STATE_CONSTANT;
//     }
//
//     // .
//     {
//         // (_Poly* -> _Poly*) -> (_Poly* -> _Poly*) -> (_Poly* -> _Poly*)
//     }
//     */
//
// }

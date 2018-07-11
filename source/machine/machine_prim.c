/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_prim.h"
#include "machine_type.h"
#include "runtime/runtime.h"

///////////////////////////////////////////////////////
// Utility declarations
///////////////////////////////////////////////////////

// Use this for simple all in one package
NecroMachineType* necro_create_prim_type(NecroMachineProgram* program, NecroVar type_name, NecroVar con_var, NecroMachineType** elems, size_t num_elems)
{
    assert(num_elems >= 1);
    assert(type_name.id.id != 0);
    assert(con_var.id.id != 0);
    NecroArenaSnapshot snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineAST*      struct_type     = necro_create_machine_struct_def(program, type_name, elems, num_elems);
    NecroMachineType*     struct_ptr_type = necro_create_machine_ptr_type(&program->arena, struct_type->necro_machine_type);
    const char*        mk_fn_name      = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "mk", necro_intern_get_string(program->intern, con_var.symbol), "#" });
    NecroVar           mk_fn_var       = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType*     mk_fn_type      = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems + 1, num_elems - 1);
    NecroMachineAST*      mk_fn_body      = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*      mk_fn_def       = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    NecroMachineAST*      data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type->necro_machine_type, (uint16_t) num_elems - 1);
    necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_machine_value(program, 0), data_ptr);

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
NecroMachineAST* necro_create_prim_con(NecroMachineProgram* program, NecroMachineType* struct_type, NecroVar con_var, NecroMachineType** elems, size_t num_elems)
{
    assert(struct_type != NULL);
    assert(struct_type->type == NECRO_MACHINE_TYPE_STRUCT);
    assert(con_var.id.id != 0);
    NecroArenaSnapshot snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroMachineType*     struct_ptr_type = necro_create_machine_ptr_type(&program->arena, struct_type);
    const char*        mk_fn_name      = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "mk", necro_intern_get_string(program->intern, con_var.symbol), "#" });
    NecroVar           mk_fn_var       = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroMachineType*     mk_fn_type      = necro_create_machine_fn_type(&program->arena, struct_ptr_type, elems, num_elems);
    NecroMachineAST*      mk_fn_body      = necro_create_machine_block(program, "entry", NULL);
    NecroMachineAST*      mk_fn_def       = necro_create_machine_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    NecroMachineAST*      data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type, (uint16_t) num_elems);
    necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_machine_value(program, 0), data_ptr);

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
    necro_symtable_get(program->symtable, fn_def->fn_def.name.id)->necro_machine_ast = fn_def->fn_def.fn_value;
    necro_build_return(program, fn_def, return_value);
}

NecroVar necro_get_top_level_symbol_var(NecroMachineProgram* program, const char* name)
{
    NecroSymbol symbol = necro_intern_string(program->intern, name);
    NecroID     id     = necro_scope_find(program->scoped_symtable->top_scope, symbol);
    assert(id.id != 0);
    return (NecroVar) { .id = id, .symbol = symbol };
}

void necro_create_prim_binop(NecroMachineProgram* program, const char* fn_name, NecroMachineAST* mk_fn_value, NecroMachineType* type, NECRO_MACHINE_BINOP_TYPE op)
{
    NecroVar         binop_var      = necro_get_top_level_symbol_var(program, fn_name);
    NecroMachineAST* binop_fn       = necro_prim_fn_begin(program, binop_var, type, (NecroMachineType*[]) { type, type }, 2);
    NecroMachineAST* binop_l_unbox  = necro_build_load_from_slot(program, binop_fn, necro_create_param_reg(program, binop_fn, 0), 1, "lhs");
    NecroMachineAST* binop_r_unbox  = necro_build_load_from_slot(program, binop_fn, necro_create_param_reg(program, binop_fn, 1), 1, "rhs");
    NecroMachineAST* bin_op_result  = necro_build_binop(program, binop_fn, binop_l_unbox, binop_r_unbox, op);
    NecroMachineAST* binop_box      = necro_build_call(program, binop_fn, mk_fn_value, (NecroMachineAST*[]) { bin_op_result }, 1, "box");
    necro_prim_fn_end(program, binop_fn, binop_box);
}

void necro_create_prim_binops(NecroMachineProgram* program, NecroMachineType* type, NecroMachineAST* mk_fn_value, const char** fn_names, NECRO_MACHINE_BINOP_TYPE* ops, size_t num_ops)
{
    for (size_t i = 0; i < num_ops; ++i)
        necro_create_prim_binop(program, fn_names[i], mk_fn_value, type, ops[i]);
    // necro_create_prim_binop(program, fn_names[1], mk_fn_value, type, ops[1]);
    // necro_create_prim_binop(program, fn_names[2], mk_fn_value, type, ops[2]);
    // necro_create_prim_binop(program, fn_names[3], mk_fn_value, type, ops[3]);
}

///////////////////////////////////////////////////////
// Init Machine Prim
///////////////////////////////////////////////////////
void necro_init_machine_prim(NecroMachineProgram* program)
{
    //--------------------
    // Prim Types
    //--------------------

    // NecroData
    NecroMachineAST* necro_data_struct = necro_create_machine_struct_def(program, necro_con_to_var(program->prim_types->necro_data_type), (NecroMachineType*[]) { necro_create_machine_uint32_type(&program->arena), necro_create_machine_uint32_type(&program->arena) }, 2);
    program->necro_data_type        = necro_data_struct->necro_machine_type;

    // Poly
    NecroVar         poly_var          = necro_gen_var(program, NULL, "Poly#", NECRO_NAME_UNIQUE);
    NecroMachineAST* necro_poly_struct = necro_create_machine_struct_def(program, poly_var,  (NecroMachineType*[]) { program->necro_data_type }, 1);
    program->necro_poly_type        = necro_poly_struct->necro_machine_type;
    program->necro_poly_ptr_type    = necro_create_machine_ptr_type(&program->arena, program->necro_poly_type);

    // NecroEnv
    NecroVar         env_var    = necro_gen_var(program, NULL, "Env#", NECRO_NAME_UNIQUE);
    NecroMachineAST* env_struct = necro_create_machine_struct_def(program, env_var, (NecroMachineType*[]) { program->necro_data_type }, 1);

    // Int
    NecroMachineType* i64_type = necro_create_machine_int64_type(&program->arena);
    NecroVar          int_var  = necro_con_to_var(program->prim_types->int_type);
    NecroVar          int_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Int").id));
    NecroMachineAST*  int_type = necro_create_machine_struct_def(program, int_var, (NecroMachineType*[]) { program->necro_data_type, i64_type }, 2);
    NecroMachineAST*  mk_int   = necro_create_prim_con(program, int_type->necro_machine_type, int_con, (NecroMachineType*[]) { i64_type }, 1);
    program->mkIntFnValue   = mk_int->fn_def.fn_value;
    // necro_gen_bin_ops(codegen, necro_con_to_var(codegen->infer->prim_types->int_type), int_type_ref, LLVMInt64TypeInContext(codegen->context), LLVMAdd, LLVMSub, LLVMMul, 0);

    // Float
    NecroMachineType* f64_type   = necro_create_machine_f64_type(&program->arena);
    NecroVar          float_var  = necro_con_to_var(program->prim_types->float_type);
    NecroVar          float_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Float").id));
    NecroMachineAST*  float_type = necro_create_machine_struct_def(program, float_var, (NecroMachineType*[]) { program->necro_data_type, f64_type }, 2);
    NecroMachineAST*  mk_float   = necro_create_prim_con(program, float_type->necro_machine_type, float_con, (NecroMachineType*[]) { f64_type }, 1);
    program->mkFloatFnValue      = mk_float->fn_def.fn_value;
    // necro_gen_bin_ops(codegen, necro_con_to_var(codegen->infer->prim_types->float_type), float_type_ref, LLVMFloatTypeInContext(codegen->context), LLVMFAdd, LLVMFSub, LLVMFMul, LLVMFDiv);
    // necro_gen_from_cast(codegen, "fromRational@Float", float_type_ref, LLVMDoubleTypeInContext(codegen->context), float_type_ref, LLVMDoubleTypeInContext(codegen->context), LLVMSIToFP);

    // Rational
    NecroVar          rational_var  = necro_con_to_var(program->prim_types->rational_type);
    NecroVar          rational_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Rational").id));
    NecroMachineType* rational_type = necro_create_prim_type(program, rational_var, rational_con, (NecroMachineType*[]) { program->necro_data_type, i64_type, i64_type}, 3);
    // program->mkFloatFnValue      = fromInt_Int_fn->fn_def.fn_value;

    // Audio
    NecroVar          audio_var  = necro_con_to_var(program->prim_types->audio_type);
    NecroVar          audio_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Audio").id));
    NecroMachineType* audio_type = necro_create_prim_type(program, audio_var, audio_con, (NecroMachineType*[]) { program->necro_data_type, necro_create_machine_ptr_type(&program->arena, f64_type) }, 2);

    // ()
    NecroVar unit_var = necro_con_to_var(program->prim_types->unit_type);
    NecroVar unit_con = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, unit_var.symbol.id));
    necro_create_prim_type(program, unit_var, unit_con, (NecroMachineType*[]) { program->necro_data_type }, 1);

    // []
    NecroVar         list_var    = necro_con_to_var(program->prim_types->list_type);
    NecroVar         cons_con    = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, ":").id));
    NecroVar         nil_con     = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "[]").id));
    NecroMachineAST* list_struct = necro_create_machine_struct_def(program, list_var, (NecroMachineType*[]) { program->necro_data_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
    necro_create_prim_con(program, list_struct->necro_machine_type, cons_con, (NecroMachineType*[]) { program->necro_poly_ptr_type, necro_create_machine_ptr_type(&program->arena, list_struct->necro_machine_type) }, 2);
    necro_create_prim_con(program, list_struct->necro_machine_type, nil_con, NULL, 0);

    //--------------------
    // Prim Functions
    //--------------------

    // Int functions
    NecroMachineType* int_ptr_type    = necro_create_machine_ptr_type(&program->arena, int_type->necro_machine_type);
    NecroVar          fromInt_Int_var = necro_get_top_level_symbol_var(program, "fromInt@Int");
    NecroMachineAST*  fromInt_Int_fn  = necro_prim_fn_begin(program, fromInt_Int_var, int_ptr_type, (NecroMachineType*[]) { int_ptr_type }, 1);
    necro_prim_fn_end(program, fromInt_Int_fn, necro_create_param_reg(program, fromInt_Int_fn, 0));
    necro_create_prim_binops(program, int_ptr_type, program->mkIntFnValue, (const char*[]) { "add@Int", "sub@Int", "mul@Int", "div@Int" }, (NECRO_MACHINE_BINOP_TYPE[]) { NECRO_MACHINE_BINOP_IADD, NECRO_MACHINE_BINOP_ISUB, NECRO_MACHINE_BINOP_IMUL, NECRO_MACHINE_BINOP_IDIV }, 3);

    // Float functions
    NecroMachineType* float_ptr_type         = necro_create_machine_ptr_type(&program->arena, float_type->necro_machine_type);
    NecroVar          fromRational_Float_var = necro_get_top_level_symbol_var(program, "fromRational@Float");
    NecroMachineAST*  fromRational_Float_fn  = necro_prim_fn_begin(program, fromRational_Float_var, float_ptr_type, (NecroMachineType*[]) { float_ptr_type }, 1);
    necro_prim_fn_end(program, fromRational_Float_fn, necro_create_param_reg(program, fromRational_Float_fn, 0));
    necro_create_prim_binops(program, float_ptr_type, program->mkFloatFnValue, (const char*[]) { "add@Float", "sub@Float", "mul@Float", "div@Float" }, (NECRO_MACHINE_BINOP_TYPE[]) { NECRO_MACHINE_BINOP_FADD, NECRO_MACHINE_BINOP_FSUB, NECRO_MACHINE_BINOP_FMUL, NECRO_MACHINE_BINOP_FDIV }, 4);

    //--------------------
    // Runtime functions
    //--------------------
    NecroVar          _necro_init_runtime_var     = necro_gen_var(program, NULL, "_necro_init_runtime#", NECRO_NAME_UNIQUE);
    NecroMachineType* _necro_init_runtime_fn_type = necro_create_machine_fn_type(&program->arena, necro_create_machine_void_type(&program->arena), NULL, 0);
    NecroMachineAST*  _necro_init_runtime_fn      = necro_create_machine_runtime_fn(program, _necro_init_runtime_var, _necro_init_runtime_fn_type, &_necro_init_runtime);
}

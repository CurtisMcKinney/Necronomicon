/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "node_prim.h"
#include "node_type.h"

///////////////////////////////////////////////////////
// Utility declarations
///////////////////////////////////////////////////////

// Use this for simple all in one package
NecroNodeType* necro_create_prim_type(NecroNodeProgram* program, NecroVar type_name, NecroVar con_var, NecroNodeType** elems, size_t num_elems)
{
    assert(num_elems >= 1);
    assert(type_name.id.id != 0);
    assert(con_var.id.id != 0);
    NecroArenaSnapshot snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroNodeAST*      struct_type     = necro_create_node_struct_def(program, type_name, elems, num_elems);
    NecroNodeType*     struct_ptr_type = necro_create_node_ptr_type(&program->arena, struct_type->necro_node_type);
    const char*        mk_fn_name      = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "mk", necro_intern_get_string(program->intern, con_var.symbol), "#" });
    NecroVar           mk_fn_var       = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroNodeType*     mk_fn_type      = necro_create_node_fn_type(&program->arena, struct_ptr_type, elems + 1, num_elems - 1);
    NecroNodeAST*      mk_fn_body      = necro_create_node_block(program, necro_intern_string(program->intern, "enter"), NULL);
    NecroNodeAST*      mk_fn_def       = necro_create_node_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    NecroNodeAST*      data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type->necro_node_type, (uint16_t) num_elems - 1);
    necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_node_value(program, 0), data_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < num_elems - 1; ++i)
    {
        necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
    }

    necro_symtable_get(program->symtable, con_var.id)->necro_node_ast = mk_fn_def->fn_def.fn_value;
    necro_build_return(program, mk_fn_def, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return struct_type->necro_node_type;
}

// Use this for when you want something more involved, like sum types
NecroNodeAST* necro_create_prim_con(NecroNodeProgram* program, NecroNodeType* struct_type, NecroVar con_var, NecroNodeType** elems, size_t num_elems)
{
    assert(struct_type != NULL);
    assert(struct_type->type == NECRO_NODE_TYPE_STRUCT);
    assert(con_var.id.id != 0);
    NecroArenaSnapshot snapshot        = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroNodeType*     struct_ptr_type = necro_create_node_ptr_type(&program->arena, struct_type);
    const char*        mk_fn_name      = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { "mk", necro_intern_get_string(program->intern, con_var.symbol), "#" });
    NecroVar           mk_fn_var       = necro_gen_var(program, NULL, mk_fn_name, NECRO_NAME_UNIQUE);
    NecroNodeType*     mk_fn_type      = necro_create_node_fn_type(&program->arena, struct_ptr_type, elems, num_elems);
    NecroNodeAST*      mk_fn_body      = necro_create_node_block(program, necro_intern_string(program->intern, "enter"), NULL);
    NecroNodeAST*      mk_fn_def       = necro_create_node_fn(program, mk_fn_var, mk_fn_body, mk_fn_type);
    NecroNodeAST*      data_ptr        = necro_build_nalloc(program, mk_fn_def, struct_type, (uint16_t) num_elems);
    necro_build_store_into_tag(program, mk_fn_def, necro_create_uint32_necro_node_value(program, 0), data_ptr);

    //--------------
    // Parameters
    for (size_t i = 0; i < num_elems; ++i)
    {
        necro_build_store_into_slot(program, mk_fn_def, necro_create_param_reg(program, mk_fn_def, i), data_ptr, i + 1);
    }
    for (size_t i = num_elems + 1; i < struct_type->struct_type.num_members; ++i)
    {
        if (i > 0)
            necro_build_store_into_slot(program, mk_fn_def, necro_create_null_necro_node_value(program, program->necro_poly_ptr_type), data_ptr, i);
    }

    necro_symtable_get(program->symtable, con_var.id)->necro_node_ast = mk_fn_def->fn_def.fn_value;
    necro_build_return(program, mk_fn_def, data_ptr);
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return mk_fn_def;
}

NecroNodeAST* necro_prim_fn_begin(NecroNodeProgram* program, NecroVar fn_var, NecroNodeType* return_type, NecroNodeType** params, size_t num_params)
{
    NecroNodeType* fn_type = necro_create_node_fn_type(&program->arena, return_type, params, num_params);
    NecroNodeAST*  fn_body = necro_create_node_block(program, necro_intern_string(program->intern, "enter"), NULL);
    NecroNodeAST*  fn_def  = necro_create_node_fn(program, fn_var, fn_body, fn_type);
    return fn_def;
}


void necro_prim_fn_end(NecroNodeProgram* program, NecroNodeAST* fn_def, NecroNodeAST* return_value)
{
    necro_symtable_get(program->symtable, fn_def->fn_def.name.id)->necro_node_ast = fn_def->fn_def.fn_value;
    necro_build_return(program, fn_def, return_value);
}

NecroVar necro_get_top_level_symbol_var(NecroNodeProgram* program, const char* name)
{
    NecroSymbol symbol = necro_intern_string(program->intern, name);
    NecroID     id     = necro_scope_find(program->scoped_symtable->top_scope, symbol);
    return (NecroVar) { .id = id, .symbol = symbol };
}

void necro_create_prim_binop(NecroNodeProgram* program, const char* fn_name, NecroNodeAST* mk_fn_value, NecroNodeType* type, NECRO_NODE_BINOP_TYPE op)
{
    NecroVar      binop_var      = necro_get_top_level_symbol_var(program, fn_name);
    NecroNodeAST* binop_fn       = necro_prim_fn_begin(program, binop_var, type, (NecroNodeType*[]) { type, type }, 2);
    NecroNodeAST* binop_l_unbox  = necro_build_load_from_slot(program, binop_fn, necro_create_param_reg(program, binop_fn, 0), 1, "lhs");
    NecroNodeAST* binop_r_unbox  = necro_build_load_from_slot(program, binop_fn, necro_create_param_reg(program, binop_fn, 1), 1, "rhs");
    NecroNodeAST* bin_op_result  = necro_build_binop(program, binop_fn, binop_l_unbox, binop_r_unbox, op);
    NecroNodeAST* binop_box      = necro_build_call(program, binop_fn, mk_fn_value, (NecroNodeAST*[]) { bin_op_result }, 1, "box");
    necro_prim_fn_end(program, binop_fn, binop_box);
}

void necro_create_prim_binops(NecroNodeProgram* program, NecroNodeType* type, NecroNodeAST* mk_fn_value, const char** fn_names, NECRO_NODE_BINOP_TYPE* ops)
{
    necro_create_prim_binop(program, fn_names[0], mk_fn_value, type, ops[0]);
    necro_create_prim_binop(program, fn_names[1], mk_fn_value, type, ops[1]);
    necro_create_prim_binop(program, fn_names[2], mk_fn_value, type, ops[2]);
    necro_create_prim_binop(program, fn_names[3], mk_fn_value, type, ops[3]);
}

///////////////////////////////////////////////////////
// Init Node Prim
///////////////////////////////////////////////////////
void necro_init_node_prim(NecroNodeProgram* program)
{
    //--------------------
    // Prim Types
    //--------------------

    // NecroData
    NecroNodeAST* necro_data_struct = necro_create_node_struct_def(program, necro_con_to_var(program->prim_types->necro_data_type), (NecroNodeType*[]) { necro_create_node_uint32_type(&program->arena), necro_create_node_uint32_type(&program->arena) }, 2);
    program->necro_data_type        = necro_data_struct->necro_node_type;

    // Poly
    NecroVar      poly_var          = necro_gen_var(program, NULL, "Poly#", NECRO_NAME_UNIQUE);
    NecroNodeAST* necro_poly_struct = necro_create_node_struct_def(program, poly_var,  (NecroNodeType*[]) { program->necro_data_type }, 1);
    program->necro_poly_type        = necro_poly_struct->necro_node_type;
    program->necro_poly_ptr_type    = necro_create_node_ptr_type(&program->arena, program->necro_poly_type);

    // NecroEnv
    NecroVar      env_var    = necro_gen_var(program, NULL, "Env#", NECRO_NAME_UNIQUE);
    NecroNodeAST* env_struct = necro_create_node_struct_def(program, env_var, (NecroNodeType*[]) { program->necro_data_type }, 1);

    // Int
    NecroNodeType* i64_type = necro_create_node_int_type(&program->arena);
    NecroVar       int_var  = necro_con_to_var(program->prim_types->int_type);
    NecroVar       int_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Int").id));
    NecroNodeAST*  int_type = necro_create_node_struct_def(program, int_var, (NecroNodeType*[]) { program->necro_data_type, i64_type }, 2);
    NecroNodeAST*  mk_int   = necro_create_prim_con(program, int_type->necro_node_type, int_con, (NecroNodeType*[]) { i64_type }, 1);
    program->mkIntFnValue   = mk_int->fn_def.fn_value;
    // necro_gen_bin_ops(codegen, necro_con_to_var(codegen->infer->prim_types->int_type), int_type_ref, LLVMInt64TypeInContext(codegen->context), LLVMAdd, LLVMSub, LLVMMul, 0);

    // Float
    NecroNodeType* f64_type   = necro_create_node_float_type(&program->arena);
    NecroVar       float_var  = necro_con_to_var(program->prim_types->float_type);
    NecroVar       float_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Float").id));
    NecroNodeAST*  float_type = necro_create_node_struct_def(program, float_var, (NecroNodeType*[]) { program->necro_data_type, f64_type }, 2);
    NecroNodeAST*  mk_float   = necro_create_prim_con(program, float_type->necro_node_type, float_con, (NecroNodeType*[]) { f64_type }, 1);
    program->mkFloatFnValue   = mk_float->fn_def.fn_value;
    // necro_gen_bin_ops(codegen, necro_con_to_var(codegen->infer->prim_types->float_type), float_type_ref, LLVMFloatTypeInContext(codegen->context), LLVMFAdd, LLVMFSub, LLVMFMul, LLVMFDiv);
    // necro_gen_from_cast(codegen, "fromRational@Float", float_type_ref, LLVMDoubleTypeInContext(codegen->context), float_type_ref, LLVMDoubleTypeInContext(codegen->context), LLVMSIToFP);

    // Rational
    NecroVar       rational_var  = necro_con_to_var(program->prim_types->rational_type);
    NecroVar       rational_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Rational").id));
    NecroNodeType* rational_type = necro_create_prim_type(program, rational_var, rational_con, (NecroNodeType*[]) { program->necro_data_type, i64_type, i64_type}, 3);
    // program->mkFloatFnValue      = fromInt_Int_fn->fn_def.fn_value;

    // Audio
    NecroVar       audio_var  = necro_con_to_var(program->prim_types->audio_type);
    NecroVar       audio_con  = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "Audio").id));
    NecroNodeType* audio_type = necro_create_prim_type(program, audio_var, audio_con, (NecroNodeType*[]) { program->necro_data_type, necro_create_node_ptr_type(&program->arena, f64_type) }, 2);

    // ()
    NecroVar unit_var = necro_con_to_var(program->prim_types->unit_type);
    NecroVar unit_con = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, unit_var.symbol.id));
    necro_create_prim_type(program, unit_var, unit_con, (NecroNodeType*[]) { program->necro_data_type }, 1);

    // []
    NecroVar      list_var    = necro_con_to_var(program->prim_types->list_type);
    NecroVar      cons_con    = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, ":").id));
    NecroVar      nil_con     = necro_con_to_var(*necro_con_table_get(&program->prim_types->con_table, necro_intern_string(program->intern, "[]").id));
    NecroNodeAST* list_struct = necro_create_node_struct_def(program, list_var, (NecroNodeType*[]) { program->necro_data_type, program->necro_poly_ptr_type, program->necro_poly_ptr_type }, 3);
    necro_create_prim_con(program, list_struct->necro_node_type, cons_con, (NecroNodeType*[]) { program->necro_poly_ptr_type, necro_create_node_ptr_type(&program->arena, list_struct->necro_node_type) }, 2);
    necro_create_prim_con(program, list_struct->necro_node_type, nil_con, NULL, 0);

    //--------------------
    // Prim Functions
    //--------------------
    // Int functions
    NecroNodeType* int_ptr_type    = necro_create_node_ptr_type(&program->arena, int_type->necro_node_type);
    NecroVar       fromInt_Int_var = necro_get_top_level_symbol_var(program, "fromInt@Int");
    NecroNodeAST*  fromInt_Int_fn  = necro_prim_fn_begin(program, fromInt_Int_var, int_ptr_type, (NecroNodeType*[]) { int_ptr_type }, 1);
    necro_prim_fn_end(program, fromInt_Int_fn, necro_create_param_reg(program, fromInt_Int_fn, 0));
    necro_create_prim_binops(program, int_ptr_type, program->mkIntFnValue, (const char*[]) { "add@Int", "sub@Int", "mul@Int", "div@Int" }, (NECRO_NODE_BINOP_TYPE[]) { NECRO_NODE_BINOP_IADD, NECRO_NODE_BINOP_ISUB, NECRO_NODE_BINOP_IMUL, NECRO_NODE_BINOP_IDIV });

    // Float functions
    NecroNodeType* float_ptr_type         = necro_create_node_ptr_type(&program->arena, float_type->necro_node_type);
    NecroVar       fromRational_Float_var = necro_get_top_level_symbol_var(program, "fromRational@Float");
    NecroNodeAST*  fromRational_Float_fn  = necro_prim_fn_begin(program, fromRational_Float_var, float_ptr_type, (NecroNodeType*[]) { float_ptr_type }, 1);
    necro_prim_fn_end(program, fromRational_Float_fn, necro_create_param_reg(program, fromRational_Float_fn, 0));
    necro_create_prim_binops(program, float_ptr_type, program->mkFloatFnValue, (const char*[]) { "add@Float", "sub@Float", "mul@Float", "div@Float" }, (NECRO_NODE_BINOP_TYPE[]) { NECRO_NODE_BINOP_FADD, NECRO_NODE_BINOP_FSUB, NECRO_NODE_BINOP_FMUL, NECRO_NODE_BINOP_FDIV });
}

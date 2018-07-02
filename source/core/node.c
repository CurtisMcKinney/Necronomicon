/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "node.h"

/*
    * https://www.microsoft.com/en-us/research/wp-content/uploads/1992/01/student.pdf
*/

NecroNodeProgram necro_create_initial_node_program(NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
{
    return (NecroNodeProgram)
    {
        .arena          = necro_create_paged_arena(),
        .snapshot_arena = necro_create_snapshot_arena(),
        .globals        = NULL,
        .num_globals    = 0,
        .main           = NULL,
        .intern         = intern,
        .symtable       = symtable,
        .prim_types     = prim_types,
        .gen_vars       = 0,
    };
}

void necro_destroy_node_program(NecroNodeProgram* program)
{
    necro_destroy_paged_arena(&program->arena);
    necro_destroy_snapshot_arena(&program->snapshot_arena);
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroVar necro_gen_var(NecroNodeProgram* program, NecroType* type, NecroNodeAST* necro_node_ast, const char* var_header)
{
    NecroArenaSnapshot snapshot = necro_get_arena_snapshot(&program->snapshot_arena);
    char buf[10];
    itoa(program->gen_vars++, buf, 10);
    const char* var_name = necro_concat_strings(&program->snapshot_arena, 3, (const char*[]) { var_header, "#", buf });
    NecroSymbol var_sym  = necro_intern_string(program->intern, var_name);
    NecroID     var_id   = necro_symtable_manual_new_symbol(program->symtable, var_sym);
    necro_symtable_get(program->symtable, var_id)->type           = type;
    necro_symtable_get(program->symtable, var_id)->necro_node_ast = necro_node_ast;
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return (NecroVar) { .id = var_id, .symbol = var_sym };
}

///////////////////////////////////////////////////////
// NecroNodeType
///////////////////////////////////////////////////////
NecroNodeType* necro_create_node_int_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_INT;
    return type;
}

NecroNodeType* necro_create_node_double_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_DOUBLE;
    return type;
}

NecroNodeType* necro_create_node_char_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_CHAR;
    return type;
}

NecroNodeType* necro_create_node_struct_type(NecroPagedArena* arena, NecroVar name, NecroNodeType** a_members, size_t num_members)
{
    NecroNodeType* type       = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type                = NECRO_NODE_TYPE_STRUCT;
    type->struct_type.name    = name;
    NecroNodeType** members   = necro_paged_arena_alloc(arena, num_members * sizeof(NecroNodeType*));
    memcpy(members, a_members, num_members * sizeof(NecroNodeType*));
    type->struct_type.members = members;
    type->struct_type.num_members = num_members;
    return type;
}

NecroNodeType* necro_create_node_fn_type(NecroPagedArena* arena, NecroVar name, NecroNodeType* return_type, NecroNodeType** a_parameters, size_t num_parameters)
{
    NecroNodeType* type          = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type                   = NECRO_NODE_TYPE_FN;
    type->fn_type.name           = name;
    type->fn_type.return_type    = return_type;
    NecroNodeType** parameters   = necro_paged_arena_alloc(arena, num_parameters * sizeof(NecroNodeType*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroNodeType*));
    type->fn_type.parameters     = parameters;
    type->fn_type.num_parameters = num_parameters;
    return type;
}

NecroNodeType* necro_create_node_ptr_type(NecroPagedArena* arena, NecroNodeType* element_type)
{
    NecroNodeType* type         = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type                  = NECRO_NODE_TYPE_PTR;
    type->ptr_type.element_type = element_type;
    return type;
}

void necro_type_check(NecroNodeType* type1, NecroNodeType* type2)
{
    assert(type1->type == type2->type);
    switch (type1->type)
    {
    case NECRO_NODE_TYPE_STRUCT:
        assert(type1->struct_type.name.id.id == type2->struct_type.name.id.id);
        return;
    case NECRO_NODE_TYPE_FN:
        assert(type1->fn_type.name.id.id == type2->fn_type.name.id.id);
        assert(type1->fn_type.num_parameters == type2->fn_type.num_parameters);
        for (size_t i = 0; i < type1->fn_type.num_parameters; ++i)
        {
            necro_type_check(type1->fn_type.parameters[i], type2->fn_type.parameters[i]);
        }
        necro_type_check(type1->fn_type.return_type, type2->fn_type.return_type);
        return;
    case NECRO_NODE_TYPE_PTR:
        necro_type_check(type1->ptr_type.element_type, type2->ptr_type.element_type);
        return;
    }
}

///////////////////////////////////////////////////////
// AST construction
///////////////////////////////////////////////////////
NecroNodeAST* necro_create_node_int_lit(NecroNodeProgram* program, int64_t i)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LIT;
    ast->lit.type        = NECRO_NODE_LIT_INT;
    ast->lit.int_literal = i;
    ast->necro_node_type = necro_create_node_int_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_double_lit(NecroNodeProgram* program, double d)
{
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_LIT;
    ast->lit.type           = NECRO_NODE_LIT_DOUBLE;
    ast->lit.double_literal = d;
    ast->necro_node_type    = necro_create_node_double_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_char_lit(NecroNodeProgram* program, char c)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_LIT;
    ast->lit.type         = NECRO_NODE_LIT_CHAR;
    ast->lit.char_literal = c;
    ast->necro_node_type  = necro_create_node_char_type(&program->arena);
    return ast;
}

NecroNodeAST* necro_create_node_null_lit(NecroNodeProgram* program, NecroNodeType* node_type)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LIT;
    ast->lit.type        = NECRO_NODE_LIT_NULL;
    ast->necro_node_type = node_type;
    assert(node_type->type == NECRO_NODE_TYPE_PTR);
    return ast;
}

NecroNodeValue necro_create_reg(NecroNodeProgram* program, NecroNodeAST* ast, NecroNodeType* necro_node_type, const char* reg_name_head)
{
    return (NecroNodeValue)
    {
        .reg_name        = necro_gen_var(program, NULL, ast, reg_name_head),
        .value_type      = NECRO_NODE_VALUE_REG,
        .necro_node_type = necro_node_type
    };
}

NecroNodeValue necro_create_param_reg(NecroNodeProgram* program, NecroNodeAST* ast, NecroVar fn_name, size_t param_num, NecroNodeType* necro_node_type)
{
    NecroNodeAST* fn_ast = necro_symtable_get(program->symtable, fn_name.id)->necro_node_ast;
    assert(fn_ast != NULL);
    assert(fn_ast->necro_node_type->type == NECRO_NODE_TYPE_FN);
    assert(fn_ast->necro_node_type->fn_type.num_parameters > param_num);
    necro_type_check(fn_ast->necro_node_type->fn_type.parameters[param_num], necro_node_type);
    return (NecroNodeValue)
    {
        .param_reg       = { .fn_name = fn_name, .param_num = param_num },
        .value_type      = NECRO_NODE_VALUE_PARAM,
        .necro_node_type = necro_node_type
    };
}

NecroNodeValue necro_create_uint_literal_value(NecroNodeProgram* program, uint32_t uint_literal)
{
    return (NecroNodeValue)
    {
        .uint_literal    = uint_literal,
        .value_type      = NECRO_NODE_VALUE_UINT_LITERAL,
        .necro_node_type = necro_create_node_int_type(&program->arena),
    };
}

NecroNodeAST* necro_create_node_load_from_ptr(NecroNodeProgram* program, NecroNodeValue source_ptr, const char* dest_name)
{
    assert(source_ptr.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(source_ptr.value_type == NECRO_NODE_VALUE_REG || source_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_LOAD;
    ast->load.load_type  = NECRO_LOAD_PTR;
    ast->load.source_ptr = source_ptr;
    ast->load.dest_value = necro_create_reg(program, ast, source_ptr.necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type = source_ptr.necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_from_slot(NecroNodeProgram* program, NecroSlot source_slot, const char* dest_name)
{
    assert(source_slot.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_LOAD;
    ast->load.load_type   = NECRO_LOAD_SLOT;
    ast->load.source_slot = source_slot;
    ast->load.dest_value  = necro_create_reg(program, ast, source_slot.necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type  = source_slot.necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_load_from_global(NecroNodeProgram* program, NecroVar source_global_name, NecroNodeType* necro_node_type, const char* dest_name)
{
    assert(necro_node_type->type == NECRO_NODE_TYPE_PTR);
    NecroNodeAST* ast            = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                    = NECRO_NODE_LOAD;
    ast->load.load_type          = NECRO_LOAD_GLOBAL;
    ast->load.source_global_name = source_global_name;
    ast->load.dest_value         = necro_create_reg(program, ast, necro_node_type->ptr_type.element_type, dest_name);
    ast->necro_node_type         = necro_node_type->ptr_type.element_type;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_ptr(NecroNodeProgram* program, NecroNodeValue source_value, NecroNodeValue dest_ptr)
{
    assert(dest_ptr.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    assert(dest_ptr.value_type == NECRO_NODE_VALUE_REG || dest_ptr.value_type == NECRO_NODE_VALUE_PARAM);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_PTR;
    ast->store.source_value = source_value;
    ast->store.dest_ptr     = dest_ptr;
    ast->necro_node_type    = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_slot(NecroNodeProgram* program, NecroNodeValue source_value, NecroSlot dest_slot)
{
    assert(dest_slot.necro_node_type->type == NECRO_NODE_TYPE_PTR);
    necro_type_check(source_value.necro_node_type, dest_slot.necro_node_type->ptr_type.element_type);
    NecroNodeAST* ast       = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type               = NECRO_NODE_STORE;
    ast->store.store_type   = NECRO_STORE_SLOT;
    ast->store.source_value = source_value;
    ast->store.dest_slot    = dest_slot;
    ast->necro_node_type    = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_store_into_global(NecroNodeProgram* program, NecroNodeValue source_value, NecroVar dest_global_name, NecroNodeType* necro_node_type)
{
    assert(necro_node_type->type == NECRO_NODE_TYPE_PTR);
    necro_type_check(source_value.necro_node_type, necro_node_type->ptr_type.element_type);
    NecroNodeAST* ast           = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                   = NECRO_NODE_STORE;
    ast->store.store_type       = NECRO_STORE_GLOBAL;
    ast->store.source_value     = source_value;
    ast->store.dest_global_name = dest_global_name;
    ast->necro_node_type        = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_block(NecroNodeProgram* program, NecroSymbol name, NecroNodeAST** a_statements, size_t num_statements, NecroTerminator terminator, NecroNodeAST* next_block)
{
    NecroNodeAST* ast         = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                 = NECRO_NODE_BLOCK;
    ast->block.name           = name;
    NecroNodeAST** statements = necro_paged_arena_alloc(&program->arena, num_statements * sizeof(NecroNodeAST*));
    memcpy(statements, a_statements, num_statements * sizeof(NecroNodeAST*));
    ast->block.statements     = statements;
    ast->block.num_statements = num_statements;
    ast->block.terminator     = terminator;
    ast->block.next_block     = next_block;
    ast->necro_node_type      = NULL;
    return ast;
}

NecroNodeAST* necro_create_node_call(NecroNodeProgram* program, NecroVar name, NecroNodeValue* a_parameters, size_t num_parameters, NECRO_FN_TYPE fn_type, NecroNodeType* necro_node_type, const char* dest_name)
{
    NecroNodeAST* ast        = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                = NECRO_NODE_CALL;
    ast->call.name           = name;
    NecroNodeValue* parameters     = necro_paged_arena_alloc(&program->arena, num_parameters * sizeof(NecroNodeValue));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroNodeValue));
    ast->call.parameters     = parameters;
    ast->call.num_parameters = num_parameters;
    ast->call.fn_type        = fn_type;
    ast->call.result_reg     = necro_create_reg(program, ast, necro_node_type, dest_name);
    ast->necro_node_type     = necro_node_type;
    switch (fn_type)
    {
    case NECRO_FN_FN:      /* FALLTHROUGH */
    case NECRO_FN_RUNTIME:
        necro_type_check(necro_node_type, necro_symtable_get(program->symtable, name.id)->necro_node_ast->necro_node_type);
        break;
    case NECRO_FN_PRIM_OP_ADDI: /* FALLTHROUGH */
    case NECRO_FN_PRIM_OP_SUBI: /* FALLTHROUGH */
    case NECRO_FN_PRIM_OP_MULI: /* FALLTHROUGH */
    case NECRO_FN_PRIM_OP_DIVI:
        // necro_type_check(necro_node_type, necro_symtable_get(program->symtable, name.id)->necro_node_ast->necro_node_type);
        break;
    };
    return ast;
}

NecroNodeAST* necro_create_node_struct_def(NecroNodeProgram* program, NecroVar name, NecroNodeType** members, size_t num_members)
{
    NecroNodeAST* ast    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type            = NECRO_NODE_STRUCT_DEF;
    ast->struct_def.name = name;
    ast->necro_node_type = necro_create_node_struct_type(&program->arena, name, members, num_members);
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    return ast;
}

NecroNodeAST* necro_create_node_fn(NecroNodeProgram* program, NecroVar name, NecroNodeAST* call_body, NecroNodeType* necro_node_type)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_FN_DEF;
    ast->fn_def.name      = name;
    ast->fn_def.call_body = call_body;
    ast->fn_def.fn_type   = NECRO_FN_FN;
    ast->necro_node_type  = necro_node_type;
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    return ast;
}

NecroNodeAST* necro_create_node_runtime_fn(NecroNodeProgram* program, NecroVar name, NecroNodeType* necro_node_type)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_FN_DEF;
    ast->fn_def.name      = name;
    ast->fn_def.fn_type   = NECRO_FN_RUNTIME;
    ast->fn_def.call_body = NULL;
    ast->necro_node_type  = necro_node_type;
    necro_symtable_get(program->symtable, name.id)->necro_node_ast = ast;
    return ast;
}

NecroNodeAST* necro_create_node_initial_node_def(NecroNodeProgram* program, NecroVar bind_name, NecroNodeDef* outer, NecroNodeType* value_type)
{
    NecroArenaSnapshot snapshot          = necro_get_arena_snapshot(&program->snapshot_arena);
    NecroNodeAST* ast                    = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type                            = NECRO_NODE_DEF;
    ast->node_def.bind_name              = bind_name;
    ast->node_def.node_name              = necro_gen_var(program, NULL, ast, necro_concat_strings(&program->snapshot_arena, 2, (const char*[]) { necro_intern_get_string(program->intern, bind_name.symbol), "_Node" }));
    ast->node_def.arg_names              = NULL;
    ast->node_def.num_arg_names          = 0;
    ast->node_def.mk_fn                  = NULL;
    ast->node_def.ini_fn                 = NULL;
    ast->node_def.update_fn              = NULL;
    ast->node_def.update_error_block     = NULL;
    ast->node_def.initial_tag            = 0;
    ast->node_def.state_type             = NECRO_STATE_STATEFUL;
    ast->node_def.outer                  = outer;
    ast->necro_node_type                 = NULL; // TODO: Node type will be constructed after analysis!
    // ast->necro_node_type                 = necro_create_node_struct_type(&program->arena, ast->node_def.node_name);
    ast->node_def.value_type             = value_type;
    const size_t initial_inner_node_size = 8;
    ast->node_def.inner_nodes            = necro_paged_arena_alloc(&program->arena, initial_inner_node_size * sizeof(NecroSlot));
    ast->node_def.num_inner_nodes        = 0;
    ast->node_def.inner_node_size        = initial_inner_node_size;
    necro_symtable_get(program->symtable, bind_name.id)->necro_node_ast = ast;
    necro_rewind_arena(&program->snapshot_arena, snapshot);
    return ast;
}

NecroNodeAST* necro_create_node_gep(NecroNodeProgram* program, NecroNodeValue source_value, uint32_t* a_indices, size_t num_indices, const char* dest_name)
{
    NecroNodeAST* ast     = necro_paged_arena_alloc(&program->arena, sizeof(NecroNodeAST));
    ast->type             = NECRO_NODE_GEP;
    ast->gep.source_value = source_value;
    int32_t* indices      = necro_paged_arena_alloc(&program->arena, num_indices * sizeof(int32_t));
    memcpy(indices, a_indices, num_indices * sizeof(int32_t));
    ast->gep.indices      = indices;
    ast->gep.num_indices  = num_indices;
    // type check gep
    NecroNodeType* necro_node_type = source_value.necro_node_type;
    for (size_t i = 0; i < num_indices; ++i)
    {
        if (necro_node_type->type == NECRO_NODE_TYPE_STRUCT)
        {
            assert(a_indices[i] < (uint32_t) necro_node_type->struct_type.num_members);
            necro_node_type = necro_node_type->struct_type.members[a_indices[i]];
        }
        else if (necro_node_type->type == NECRO_NODE_TYPE_PTR)
        {
            assert(a_indices[i] == 0); // NOTE: The node abstract machine never directly works with contiguous arrays of data. Thus all pointers should only ever be indexed from 0!
            assert(i == 0);            // NOTE:Can only deref the first type!
            necro_node_type = necro_node_type->ptr_type.element_type;
        }
        else
        {
            assert(necro_node_type->type == NECRO_NODE_TYPE_STRUCT || necro_node_type->type == NECRO_NODE_TYPE_PTR);
        }
    }
    necro_node_type      = necro_create_node_ptr_type(&program->arena, necro_node_type);
    ast->gep.dest_value  = necro_create_reg(program, ast, necro_node_type, dest_name);
    ast->necro_node_type = necro_node_type;
    return ast;
}

// ///////////////////////////////////////////////////////
// // Forward declarations
// ///////////////////////////////////////////////////////
// NecroFCoreAST* necro_closure_conversion_go(NecroClosureConversion* ccon, NecroCoreAST_Expression* in_ast);

// ///////////////////////////////////////////////////////
// // Closure Conversion
// ///////////////////////////////////////////////////////
// NecroFCore necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// {
//     NecroClosureConversion closure_conversion = necro_create_closure_conversion(intern, symtable, prim_types);
//     return closure_conversion.fcore;
// }

// // NecroCoreAST necro_core_final_pass(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types)
// // {
// //     NecroCoreFinalPass final_pass = necro_create_final_pass(intern, symtable, prim_types);
// //     necro_core_final_pass_go(&final_pass, in_ast->root);
// //     return final_pass.core_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_lit(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIT);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_lit(final_pass, in_ast->lit);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_var(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_var(final_pass, in_ast->var.id, in_ast->var.symbol);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_data_con(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* args = necro_core_final_pass_go(final_pass, in_ast->data_con.arg_list);
// //     NecroCoreAST_DataCon*    next = NULL;
// //     if (in_ast->data_con.next != NULL)
// //     {
// //         NecroCoreAST_Expression dummy_ast;
// //         dummy_ast.expr_type = NECRO_CORE_EXPR_DATA_CON;
// //         dummy_ast.data_con  = *in_ast->data_con.next;
// //         next                = &necro_core_final_pass_data_con(final_pass, &dummy_ast)->data_con;
// //         if (necro_is_final_pass_error(final_pass)) return NULL;
// //     }
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_data_con(final_pass, args, in_ast->data_con.condid, next);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_data_decl(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression dummy_ast;
// //     dummy_ast.expr_type = NECRO_CORE_EXPR_DATA_CON;
// //     dummy_ast.data_con  = *in_ast->data_decl.con_list;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_data_decl(final_pass, in_ast->data_decl.data_id, &necro_core_final_pass_go(final_pass, &dummy_ast)->data_con);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_list(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     if (in_ast == NULL) return NULL;
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast_head = necro_create_fcore_ast_list(final_pass, necro_core_final_pass_go(final_pass, in_ast->list.expr), NULL);
// //     NecroCoreAST_Expression* out_ast_curr = out_ast_head;
// //     in_ast                                = in_ast->list.next;
// //     while (in_ast != NULL)
// //     {
// //         out_ast_curr->list.next = necro_create_fcore_ast_list(final_pass, necro_core_final_pass_go(final_pass, in_ast->list.expr), NULL);
// //         if (necro_is_final_pass_error(final_pass)) return NULL;
// //         in_ast                  = in_ast->list.next;
// //     }
// //     return out_ast_head;
// // }

// // NecroCoreAST_CaseAlt* necro_core_final_pass_alt(NecroCoreFinalPass* final_pass, NecroCoreAST_CaseAlt* in_alt)
// // {
// //     assert(final_pass != NULL);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     if (in_alt == NULL) return NULL;
// //     NecroCoreAST_CaseAlt* out_alt = necro_paged_arena_alloc(&final_pass->core_ast.arena, sizeof(NecroCoreAST_CaseAlt));
// //     if (in_alt->altCon == NULL)
// //         out_alt->altCon = NULL;
// //     else
// //         out_alt->altCon = necro_core_final_pass_go(final_pass, in_alt->altCon);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     out_alt->expr = necro_core_final_pass_go(final_pass, in_alt->expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     out_alt->next = necro_core_final_pass_alt(final_pass, in_alt->next);
// //     return out_alt;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_case(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_CaseAlt*    alts    = necro_core_final_pass_alt(final_pass, in_ast->case_expr.alts);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->case_expr.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_case(final_pass, alts, expr, in_ast->case_expr.type);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_lambda(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* arg     = necro_core_final_pass_go(final_pass, in_ast->lambda.arg);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->lambda.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_lambda(final_pass, arg, expr);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_app(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* exprA   = necro_core_final_pass_go(final_pass, in_ast->app.exprA);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* exprB   = necro_core_final_pass_go(final_pass, in_ast->app.exprB);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_app(final_pass, exprA, exprB);
// //     return out_ast;
// // }

// // NecroCoreAST_Expression* necro_core_final_pass_let(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* bind    = necro_core_final_pass_go(final_pass, in_ast->let.bind);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->let.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_let(final_pass, bind, expr);
// //     return out_ast;
// // }

// // // TODO: Finish!
// // NecroCoreAST_Expression* necro_core_final_pass_bind(NecroCoreFinalPass* final_pass, NecroCoreAST_Expression* in_ast)
// // {
// //     assert(final_pass != NULL);
// //     assert(in_ast != NULL);
// //     assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* expr    = necro_core_final_pass_go(final_pass, in_ast->bind.expr);
// //     if (necro_is_final_pass_error(final_pass)) return NULL;
// //     NecroCoreAST_Expression* out_ast = necro_create_fcore_ast_bind(final_pass, in_ast->bind.var, in_ast->bind.expr, in_ast->bind.is_recursive);
// //     // //--------------------
// //     // // Make non-pointfree
// //     // //--------------------
// //     // NecroCoreAST_Expression* ast      = out_ast;
// //     // NecroType*               for_alls = ((NecroFCoreAST*)out_ast)->necro_type;
// //     // NecroType*               arrows   = NULL;
// //     // while (for_alls->type == NECRO_TYPE_FOR)
// //     // {
// //     //     NecroTypeClassContext* for_all_context = for_alls->for_all.context;
// //     //     while (for_all_context != NULL)
// //     //     {
// //     //         if (ast->expr_type == NECRO_CORE_EXPR_LAM)
// //     //         {
// //     //             ast = ast->lambda.expr;
// //     //         }
// //     //         else
// //     //         {
// //     //             // NecroVar gen_var = necro_gen_var(final_pass, for_alls)
// //     //         }
// //     //         for_all_context = for_all_context->next;
// //     //     }
// //     //     for_alls = for_alls->for_all.type;
// //     //     for_alls = necro_find(for_alls);
// //     // }
// //     // arrows = for_alls;
// //     // arrows = necro_find(arrows);
// //     // while (arrows->type == NECRO_TYPE_FUN)
// //     // {
// //     //     if (ast->expr_type == NECRO_CORE_EXPR_LAM)
// //     //     {
// //     //         ast = ast->lambda.expr;
// //     //     }
// //     //     else
// //     //     {
// //     //         NecroVar                 gen_var = necro_gen_var(final_pass, arrows->fun.type1);
// //     //         NecroCoreAST_Expression* var_ast = necro_create_fcore_ast_var(final_pass, gen_var.id, gen_var.symbol);
// //     //         out_ast                          = necro_create_fcore_ast_app(final_pass, out_ast, var_ast);
// //     //         NecroCoreAST_Expression* arg_ast = necro_create_fcore_ast_var(final_pass, gen_var.id, gen_var.symbol);
// //     //         // TODO: Add new var and lambda to lambdas ast!
// //     //     }
// //     //     arrows = arrows->fun.type2;
// //     //     arrows = necro_find(arrows);
// //     // }
// //     return out_ast;
// // }

// ///////////////////////////////////////////////////////
// // Go
// ///////////////////////////////////////////////////////
// NecroFCoreAST* necro_closure_conversion_go(NecroClosureConversion* ccon, NecroCoreAST_Expression* in_ast)
// {
//     assert(ccon != NULL);
//     assert(in_ast != NULL);
//     // if (necro_is_final_pass_error(final_pass)) return NULL;
//     // switch (in_ast->expr_type)
//     // {
//     // case NECRO_CORE_EXPR_VAR:       return necro_core_final_pass_var(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_DATA_CON:  return necro_core_final_pass_data_con(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_DATA_DECL: return necro_core_final_pass_data_decl(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LIT:       return necro_core_final_pass_lit(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_CASE:      return necro_core_final_pass_case(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LAM:       return necro_core_final_pass_lambda(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_APP:       return necro_core_final_pass_app(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_BIND:      return necro_core_final_pass_bind(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LET:       return necro_core_final_pass_let(final_pass, in_ast);
//     // case NECRO_CORE_EXPR_LIST:      return necro_core_final_pass_list(final_pass, in_ast); // used for top decls not language lists
//     // case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
//     // default:                        assert(false && "Unimplemented AST type in necro_core_final_pass_go"); return NULL;
//     // }
//     return NULL;
// }
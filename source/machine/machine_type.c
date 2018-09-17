/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "machine_type.h"
#include <ctype.h>
#include "machine/machine.h"

///////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////
NecroMachineType* necro_create_machine_uint1_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_UINT1;
    return type;
}

NecroMachineType* necro_create_machine_uint8_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_UINT8;
    return type;
}

NecroMachineType* necro_create_machine_uint16_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_UINT16;
    return type;
}

NecroMachineType* necro_create_machine_uint32_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_UINT32;
    return type;
}

NecroMachineType* necro_create_machine_uint64_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_UINT64;
    return type;
}

NecroMachineType* necro_create_machine_int32_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_INT32;
    return type;
}

NecroMachineType* necro_create_machine_int64_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_INT64;
    return type;
}

NecroMachineType* necro_create_machine_f32_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_F32;
    return type;
}

NecroMachineType* necro_create_machine_f64_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_F64;
    return type;
}

NecroMachineType* necro_create_machine_char_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_CHAR;
    return type;
}

NecroMachineType* necro_create_machine_void_type(NecroPagedArena* arena)
{
    NecroMachineType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type             = NECRO_MACHINE_TYPE_VOID;
    return type;
}

NecroMachineType* necro_create_word_sized_uint_type(NecroMachineProgram* program)
{
    NecroMachineType* type = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineType));
    type->type             = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACHINE_TYPE_UINT32 : NECRO_MACHINE_TYPE_UINT64;
    return type;
}

NecroMachineType* necro_create_word_sized_int_type(NecroMachineProgram* program)
{
    NecroMachineType* type = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineType));
    type->type             = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACHINE_TYPE_INT32 : NECRO_MACHINE_TYPE_INT64;
    return type;
}

NecroMachineType* necro_create_word_sized_float_type(NecroMachineProgram* program)
{
    NecroMachineType* type = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachineType));
    type->type             = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACHINE_TYPE_F32 : NECRO_MACHINE_TYPE_F64;
    return type;
}

NecroMachineType* necro_create_machine_struct_type(NecroPagedArena* arena, NecroVar name, NecroMachineType** a_members, size_t num_members)
{
    NecroMachineType* type        = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type                    = NECRO_MACHINE_TYPE_STRUCT;
    type->struct_type.name        = name;
    NecroMachineType** members    = necro_paged_arena_alloc(arena, num_members * sizeof(NecroMachineType*));
    memcpy(members, a_members, num_members * sizeof(NecroMachineType*));
    type->struct_type.members     = members;
    type->struct_type.num_members = num_members;
    return type;
}

NecroMachineType* necro_create_machine_fn_type(NecroPagedArena* arena, NecroMachineType* return_type, NecroMachineType** a_parameters, size_t num_parameters)
{
    NecroMachineType* type          = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type                   = NECRO_MACHINE_TYPE_FN;
    type->fn_type.return_type    = return_type;
    NecroMachineType** parameters   = necro_paged_arena_alloc(arena, num_parameters * sizeof(NecroMachineType*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachineType*));
    type->fn_type.parameters     = parameters;
    type->fn_type.num_parameters = num_parameters;
    return type;
}

bool is_poly_ptr(NecroMachineProgram* program, NecroMachineType* type)
{
    return type->type == NECRO_MACHINE_TYPE_PTR && type->ptr_type.element_type->type == NECRO_MACHINE_TYPE_STRUCT && type->ptr_type.element_type->struct_type.name.id.id == program->necro_poly_type->struct_type.name.id.id;
}

NecroMachineType* necro_create_machine_ptr_type(NecroPagedArena* arena, NecroMachineType* element_type)
{
    assert(element_type != NULL);
    NecroMachineType* type         = necro_paged_arena_alloc(arena, sizeof(NecroMachineType));
    type->type                  = NECRO_MACHINE_TYPE_PTR;
    type->ptr_type.element_type = element_type;
    return type;
}

void necro_type_check(NecroMachineProgram* program, NecroMachineType* type1, NecroMachineType* type2)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    assert(type1 != program->necro_poly_type);
    assert(type2 != program->necro_poly_type);
    if (is_poly_ptr(program, type1) && (type2->type == NECRO_MACHINE_TYPE_PTR || necro_is_unboxed_type(program, type2)))
        return;
    if (is_poly_ptr(program, type2) && (type1->type == NECRO_MACHINE_TYPE_PTR || necro_is_unboxed_type(program, type1)))
        return;
    assert(type1->type == type2->type);
    if (type1->type)
    switch (type1->type)
    {
    case NECRO_MACHINE_TYPE_STRUCT:
        assert(type1->struct_type.name.id.id == type2->struct_type.name.id.id);
        return;
    case NECRO_MACHINE_TYPE_FN:
        assert(type1->fn_type.num_parameters == type2->fn_type.num_parameters);
        for (size_t i = 0; i < type1->fn_type.num_parameters; ++i)
        {
            necro_type_check(program, type1->fn_type.parameters[i], type2->fn_type.parameters[i]);
        }
        necro_type_check(program, type1->fn_type.return_type, type2->fn_type.return_type);
        return;
    case NECRO_MACHINE_TYPE_PTR:
        if (is_poly_ptr(program, type1) || is_poly_ptr(program, type2))
            return;
        necro_type_check(program, type1->ptr_type.element_type, type2->ptr_type.element_type);
        return;
    }
}

bool necro_is_unboxed_type(struct NecroMachineProgram* program, NecroMachineType* type)
{
    return type->type == program->necro_int_type->type
        || type->type == program->necro_float_type->type
        || type->type == program->necro_uint_type->type
        || (type->type == NECRO_MACHINE_TYPE_STRUCT && type->struct_type.name.id.id == program->world_type->struct_type.name.id.id);
}

bool necro_is_word_uint_type(struct NecroMachineProgram* program, NecroMachineType* type)
{
    return type->type == program->necro_uint_type->type;
}

NecroMachineType* necro_make_ptr_if_boxed(NecroMachineProgram* program, NecroMachineType* type)
{
    if (necro_is_unboxed_type(program, type))
        return type;
    else
        return necro_create_machine_ptr_type(&program->arena, type);
}

///////////////////////////////////////////////////////
// Print
///////////////////////////////////////////////////////
void necro_machine_print_machine_type_go(NecroIntern* intern, NecroMachineType* type, bool is_recursive)
{
    switch (type->type)
    {
    case NECRO_MACHINE_TYPE_VOID:
        printf("void");
        return;
    case NECRO_MACHINE_TYPE_UINT1:
        printf("u1");
        return;
    case NECRO_MACHINE_TYPE_UINT8:
        printf("u8");
        return;
    case NECRO_MACHINE_TYPE_UINT16:
        printf("u16");
        return;
    case NECRO_MACHINE_TYPE_UINT32:
        printf("u32");
        return;
    case NECRO_MACHINE_TYPE_UINT64:
        printf("u64");
        return;
    case NECRO_MACHINE_TYPE_INT32:
        printf("i32");
        return;
    case NECRO_MACHINE_TYPE_INT64:
        printf("i64");
        return;
    case NECRO_MACHINE_TYPE_F32:
        printf("f32");
        return;
    case NECRO_MACHINE_TYPE_F64:
        printf("f64");
        return;
    case NECRO_MACHINE_TYPE_CHAR:
        printf("char");
        return;
    case NECRO_MACHINE_TYPE_PTR:
        necro_machine_print_machine_type_go(intern, type->ptr_type.element_type, false);
        printf("*");
        return;
    case NECRO_MACHINE_TYPE_STRUCT:
        if (is_recursive)
        {
            printf("%s { ", type->struct_type.name.symbol->str);
            for (size_t i = 0; i < type->struct_type.num_members; ++i)
            {
                necro_machine_print_machine_type_go(intern, type->struct_type.members[i], false);
                if (i < type->struct_type.num_members - 1)
                    printf(", ");
            }
            printf(" }");
        }
        else
        {
            printf("%s", type->struct_type.name.symbol->str);
        }
        return;
    case NECRO_MACHINE_TYPE_FN:
        return;
    }
}

void necro_machine_print_machine_type(NecroIntern* intern, NecroMachineType* type)
{
    necro_machine_print_machine_type_go(intern, type, true);
}

///////////////////////////////////////////////////////
// Conversion
///////////////////////////////////////////////////////
NecroType* necro_core_ast_to_necro_type(NecroMachineProgram* program, NecroCoreAST_Expression* ast)
{
    assert(program != NULL);
    assert(ast != NULL);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER: return necro_symtable_get(program->symtable, program->prim_types->int_type.id)->type;
        case NECRO_AST_CONSTANT_FLOAT:   return necro_symtable_get(program->symtable, program->prim_types->float_type.id)->type;
        default:                         assert(false); return NULL;
        }
    case NECRO_CORE_EXPR_VAR:
        if (necro_symtable_get(program->symtable, ast->var.id)->closure_type != NULL)
            return necro_symtable_get(program->symtable, ast->var.id)->closure_type;
        else
            return necro_symtable_get(program->symtable, ast->var.id)->type;
    case NECRO_CORE_EXPR_BIND:
        if (necro_symtable_get(program->symtable, ast->bind.var.id)->closure_type != NULL)
            return necro_symtable_get(program->symtable, ast->bind.var.id)->closure_type;
        else
            return necro_symtable_get(program->symtable, ast->bind.var.id)->type;
    case NECRO_CORE_EXPR_LAM:
        assert(false && "TODO: Finish!");
        return NULL;
    case NECRO_CORE_EXPR_DATA_CON:
        if (necro_symtable_get(program->symtable, ast->data_con.condid.id)->closure_type != NULL)
            return necro_symtable_get(program->symtable, ast->data_con.condid.id)->closure_type;
        else
            return necro_symtable_get(program->symtable, ast->data_con.condid.id)->type;
    case NECRO_CORE_EXPR_TYPE: return ast->type.type;
    case NECRO_CORE_EXPR_APP:
    {
        // We're assuming that we always hit a var at the end...
        NecroCoreAST_Expression* app = ast;
        while (app->expr_type == NECRO_CORE_EXPR_APP)
        {
            app = app->app.exprA;
        }
        if (app->expr_type == NECRO_CORE_EXPR_VAR)
        {
            if (necro_symtable_get(program->symtable, app->var.id)->closure_type != NULL)
                return necro_symtable_get(program->symtable, app->var.id)->closure_type;
            else
                return necro_symtable_get(program->symtable, app->var.id)->type;
        }
        else
        {
            assert(false);
            return NULL;
        }
    }
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_codegen_go"); return NULL;
    }
}

NecroMachineType* necro_function_type_to_machine_function_type(NecroMachineProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(type);
    assert(type->type == NECRO_TYPE_FUN);
    size_t arg_count = 0;
    size_t arg_index = 0;
    // Count args
    NecroType* arrows = necro_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        arg_count++;
        arrows = arrows->fun.type2;
        arrows = necro_find(arrows);
    }
    NecroMachineType** args = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroMachineType*));
    arrows = necro_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        // args[arg_index] = necro_create_machine_ptr_type(&program->arena, necro_type_to_machine_type(program, arrows->fun.type1));
        args[arg_index] = necro_make_ptr_if_boxed(program, necro_type_to_machine_type(program, arrows->fun.type1));
        arg_index++;
        arrows = arrows->fun.type2;
        arrows = necro_find(arrows);
    }
    NecroMachineType* return_type   = necro_make_ptr_if_boxed(program, necro_type_to_machine_type(program, arrows));
    // NecroMachineType* return_type   = necro_type_to_machine_type(program, arrows);
    NecroMachineType* function_type = necro_create_machine_fn_type(&program->arena, return_type, args, arg_count);
    return function_type;
}

NecroMachineType* necro_type_to_machine_type(NecroMachineProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return program->necro_poly_type;
    case NECRO_TYPE_LIST: return necro_symtable_get(program->symtable, program->prim_types->list_type.id)->necro_machine_ast->necro_machine_type;
    case NECRO_TYPE_CON:  return necro_symtable_get(program->symtable, type->con.con.id)->necro_machine_ast->necro_machine_type;
    case NECRO_TYPE_FUN:  return necro_function_type_to_machine_function_type(program, type);
    case NECRO_TYPE_FOR:
        while (type->type == NECRO_TYPE_FOR)
            type = type->for_all.type;
        return necro_type_to_machine_type(program, type);

    case NECRO_TYPE_APP:  assert(false); return NULL;
    default: assert(false);
    }
    return NULL;
}

NecroMachineType* necro_core_pattern_type_to_machine_type(NecroMachineProgram* program, NecroCoreAST_Expression* ast)
{
    NecroType* type = necro_core_ast_to_necro_type(program, ast);
    type            = necro_find(type);
    while (type->type == NECRO_TYPE_FOR)
    {
        type = type->for_all.type;
        type = necro_find(type);
    }
    while (type->type == NECRO_TYPE_FUN)
    {
        type = type->fun.type2;
        type = necro_find(type);
    }
    return necro_type_to_machine_type(program, type);
}

NecroMachineType* necro_core_ast_to_machine_type(NecroMachineProgram* program, NecroCoreAST_Expression* ast)
{
    assert(program != NULL);
    assert(ast != NULL);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER: return program->necro_int_type;
        case NECRO_AST_CONSTANT_FLOAT:   return program->necro_float_type;
        default:                         assert(false); return NULL;
        }
    case NECRO_CORE_EXPR_VAR:            return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_BIND:           return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_LAM:
        assert(false && "TODO: Finish!");
        return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_TYPE:      return necro_type_to_machine_type(program, ast->type.type);
    case NECRO_CORE_EXPR_APP:       return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_core_ast_to_machine_type"); return NULL;
    }
}
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "node_type.h"
#include <ctype.h>
#include "node/node.h"

///////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////
NecroNodeType* necro_create_node_uint32_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_UINT32;
    return type;
}

NecroNodeType* necro_create_node_uint16_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_UINT16;
    return type;
}

NecroNodeType* necro_create_node_int_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_INT;
    return type;
}

NecroNodeType* necro_create_node_float_type(NecroPagedArena* arena)
{
    NecroNodeType* type = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type          = NECRO_NODE_TYPE_FLOAT;
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

NecroNodeType* necro_create_node_fn_type(NecroPagedArena* arena, NecroNodeType* return_type, NecroNodeType** a_parameters, size_t num_parameters)
{
    NecroNodeType* type          = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type                   = NECRO_NODE_TYPE_FN;
    type->fn_type.return_type    = return_type;
    NecroNodeType** parameters   = necro_paged_arena_alloc(arena, num_parameters * sizeof(NecroNodeType*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroNodeType*));
    type->fn_type.parameters     = parameters;
    type->fn_type.num_parameters = num_parameters;
    return type;
}

bool is_poly_ptr(NecroNodeProgram* program, NecroNodeType* type)
{
    return type->type == NECRO_NODE_TYPE_PTR && type->ptr_type.element_type == program->necro_poly_type;
}

NecroNodeType* necro_create_node_ptr_type(NecroPagedArena* arena, NecroNodeType* element_type)
{
    assert(element_type != NULL);
    NecroNodeType* type         = necro_paged_arena_alloc(arena, sizeof(NecroNodeType));
    type->type                  = NECRO_NODE_TYPE_PTR;
    type->ptr_type.element_type = element_type;
    return type;
}

void necro_type_check(NecroNodeProgram* program, NecroNodeType* type1, NecroNodeType* type2)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    assert(type1->type == type2->type);
    assert(type1 != program->necro_poly_type);
    assert(type2 != program->necro_poly_type);
    if (type1->type)
    switch (type1->type)
    {
    case NECRO_NODE_TYPE_STRUCT:
        assert(type1->struct_type.name.id.id == type2->struct_type.name.id.id);
        return;
    case NECRO_NODE_TYPE_FN:
        // assert(type1->fn_type.name.id.id == type2->fn_type.name.id.id);
        assert(type1->fn_type.num_parameters == type2->fn_type.num_parameters);
        for (size_t i = 0; i < type1->fn_type.num_parameters; ++i)
        {
            necro_type_check(program, type1->fn_type.parameters[i], type2->fn_type.parameters[i]);
        }
        necro_type_check(program, type1->fn_type.return_type, type2->fn_type.return_type);
        return;
    case NECRO_NODE_TYPE_PTR:
        if (is_poly_ptr(program, type1) || is_poly_ptr(program, type2))
            return;
        necro_type_check(program, type1->ptr_type.element_type, type2->ptr_type.element_type);
        return;
    }
}

///////////////////////////////////////////////////////
// Print
///////////////////////////////////////////////////////
void necro_node_print_node_type_go(NecroIntern* intern, NecroNodeType* type, bool is_recursive)
{
    switch (type->type)
    {
    case NECRO_NODE_TYPE_UINT32:
        printf("uint32");
        return;
    case NECRO_NODE_TYPE_INT:
        printf("int64");
        return;
    case NECRO_NODE_TYPE_FLOAT:
        printf("f64");
        return;
    case NECRO_NODE_TYPE_CHAR:
        printf("char");
        return;
    case NECRO_NODE_TYPE_PTR:
        necro_node_print_node_type_go(intern, type->ptr_type.element_type, false);
        printf("*");
        return;
    case NECRO_NODE_TYPE_STRUCT:
        if (is_recursive)
        {
            printf("%s { ", necro_intern_get_string(intern, type->struct_type.name.symbol));
            for (size_t i = 0; i < type->struct_type.num_members; ++i)
            {
                necro_node_print_node_type_go(intern, type->struct_type.members[i], false);
                if (i < type->struct_type.num_members - 1)
                    printf(", ");
            }
            printf(" }");
        }
        else
        {
            printf("%s", necro_intern_get_string(intern, type->struct_type.name.symbol));
        }
        return;
    case NECRO_NODE_TYPE_FN:
        return;
    }
}

void necro_node_print_node_type(NecroIntern* intern, NecroNodeType* type)
{
    necro_node_print_node_type_go(intern, type, true);
}

///////////////////////////////////////////////////////
// Conversion
///////////////////////////////////////////////////////
NecroType* necro_core_ast_to_necro_type(NecroNodeProgram* program, NecroCoreAST_Expression* ast)
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
    case NECRO_CORE_EXPR_VAR:  return necro_symtable_get(program->symtable, ast->var.id)->type;
    case NECRO_CORE_EXPR_BIND: return necro_symtable_get(program->symtable, ast->bind.var.id)->type;
    case NECRO_CORE_EXPR_LAM:
        assert(false && "TODO: Finish!");
        return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return necro_symtable_get(program->symtable, ast->data_con.condid.id)->type;
    case NECRO_CORE_EXPR_TYPE:      return ast->type.type;
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

NecroNodeType* necro_function_type_to_node_function_type(NecroNodeProgram* program, NecroType* type)
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
    NecroNodeType** args = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroNodeType*));
    arrows = necro_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        args[arg_index] = necro_create_node_ptr_type(&program->arena, necro_type_to_node_type(program, arrows->fun.type1));
        arg_index++;
        arrows = arrows->fun.type2;
        arrows = necro_find(arrows);
    }
    NecroNodeType* return_type   = necro_create_node_ptr_type(&program->arena, necro_type_to_node_type(program, arrows));
    NecroNodeType* function_type = necro_create_node_fn_type(&program->arena, return_type, args, arg_count);
    return function_type;
}

NecroNodeType* necro_type_to_node_type(NecroNodeProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return program->necro_poly_type;
    case NECRO_TYPE_LIST: return necro_symtable_get(program->symtable, program->prim_types->list_type.id)->necro_node_ast->necro_node_type;
    case NECRO_TYPE_CON:  return necro_symtable_get(program->symtable, type->con.con.id)->necro_node_ast->necro_node_type;
    case NECRO_TYPE_FUN:  return necro_function_type_to_node_function_type(program, type);
    case NECRO_TYPE_FOR:
        while (type->type == NECRO_TYPE_FOR)
            type = type->for_all.type;
        return necro_type_to_node_type(program, type);
    case NECRO_TYPE_APP:  assert(false); return NULL;
    default: assert(false);
    }
    return NULL;
}

NecroNodeType* necro_core_ast_to_node_type(NecroNodeProgram* program, NecroCoreAST_Expression* ast)
{
    assert(program != NULL);
    assert(ast != NULL);
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_LIT:
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER: return necro_create_node_int_type(&program->arena);
        case NECRO_AST_CONSTANT_FLOAT:   return necro_create_node_float_type(&program->arena);
        default:                         assert(false); return NULL;
        }
    case NECRO_CORE_EXPR_VAR:            return necro_type_to_node_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_BIND:           return necro_type_to_node_type(program, necro_symtable_get(program->symtable, ast->bind.var.id)->type);
    case NECRO_CORE_EXPR_LAM:
        assert(false && "TODO: Finish!");
        return NULL;
    case NECRO_CORE_EXPR_DATA_CON:  return necro_type_to_node_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_TYPE:      return necro_type_to_node_type(program, ast->type.type);
    case NECRO_CORE_EXPR_APP:       return necro_type_to_node_type(program, necro_core_ast_to_necro_type(program, ast));
    case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
    case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
    case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
    case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
    default:                        assert(false && "Unimplemented AST type in necro_core_ast_to_node_type"); return NULL;
    }
}
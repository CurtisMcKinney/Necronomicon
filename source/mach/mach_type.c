/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "mach_type.h"
#include <ctype.h>
#include "mach_ast.h"

///////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////

NecroMachType* necro_mach_type_create_word_sized_uint(NecroMachProgram* program)
{
    if (program->necro_uint_type != NULL)
        return program->necro_uint_type;
    NecroMachType* type = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type          = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACH_TYPE_UINT32 : NECRO_MACH_TYPE_UINT64;
    program->necro_uint_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_word_sized_int(NecroMachProgram* program)
{
    if (program->necro_int_type != NULL)
        return program->necro_int_type;
    NecroMachType* type = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type          = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACH_TYPE_INT32 : NECRO_MACH_TYPE_INT64;
    program->necro_int_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_word_sized_float(NecroMachProgram* program)
{
    if (program->necro_float_type != NULL)
        return program->necro_float_type;
    NecroMachType* type = necro_paged_arena_alloc(&program->arena, sizeof(NecroMachType));
    type->type          = (program->word_size == NECRO_WORD_4_BYTES) ? NECRO_MACH_TYPE_F32 : NECRO_MACH_TYPE_F64;
    program->necro_float_type = type;
    return type;
}

NecroMachType* necro_mach_type_create_uint1(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_UINT1;
    return type;
}

NecroMachType* necro_mach_type_create_uint8(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_UINT8;
    return type;
}

NecroMachType* necro_mach_type_create_uint16(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_UINT16;
    return type;
}

NecroMachType* necro_mach_type_create_uint32(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_UINT32;
    return type;
}

NecroMachType* necro_mach_type_create_uint64(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_UINT64;
    return type;
}

NecroMachType* necro_mach_type_create_int32(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_INT32;
    return type;
}

NecroMachType* necro_mach_type_create_int64(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_INT64;
    return type;
}

NecroMachType* necro_mach_type_create_f32(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_F32;
    return type;
}

NecroMachType* necro_mach_type_create_f64(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_F64;
    return type;
}

NecroMachType* necro_mach_type_create_char(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_CHAR;
    return type;
}

NecroMachType* necro_mach_type_create_void(NecroPagedArena* arena)
{
    NecroMachType* type = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type          = NECRO_MACH_TYPE_VOID;
    return type;
}

NecroMachType* necro_mach_type_create_struct(NecroPagedArena* arena, struct NecroMachAstSymbol* symbol, NecroMachType** a_members, size_t num_members)
{
    NecroMachType* type           = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                    = NECRO_MACH_TYPE_STRUCT;
    type->struct_type.symbol      = symbol;
    NecroMachType** members       = necro_paged_arena_alloc(arena, num_members * sizeof(NecroMachType*));
    memcpy(members, a_members, num_members * sizeof(NecroMachType*)); // This isn't a deep copy...is that ok?
    type->struct_type.members     = members;
    type->struct_type.num_members = num_members;
    return type;
}

NecroMachType* necro_mach_type_create_fn(NecroPagedArena* arena, NecroMachType* return_type, NecroMachType** a_parameters, size_t num_parameters)
{
    NecroMachType* type          = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                   = NECRO_MACH_TYPE_FN;
    type->fn_type.return_type    = return_type;
    NecroMachType** parameters   = necro_paged_arena_alloc(arena, num_parameters * sizeof(NecroMachType*));
    memcpy(parameters, a_parameters, num_parameters * sizeof(NecroMachType*));
    type->fn_type.parameters     = parameters;
    type->fn_type.num_parameters = num_parameters;
    return type;
}

NecroMachType* necro_mach_type_create_ptr(NecroPagedArena* arena, NecroMachType* element_type)
{
    assert(element_type != NULL);
    NecroMachType* type         = necro_paged_arena_alloc(arena, sizeof(NecroMachType));
    type->type                  = NECRO_MACH_TYPE_PTR;
    type->ptr_type.element_type = element_type;
    return type;
}

void necro_mach_type_check(NecroMachProgram* program, NecroMachType* type1, NecroMachType* type2)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    assert(type1->type == type2->type);
    switch (type1->type)
    {
    case NECRO_MACH_TYPE_STRUCT:
        assert(type1->struct_type.symbol == type2->struct_type.symbol);
        return;
    case NECRO_MACH_TYPE_FN:
        assert(type1->fn_type.num_parameters == type2->fn_type.num_parameters);
        for (size_t i = 0; i < type1->fn_type.num_parameters; ++i)
        {
            necro_mach_type_check(program, type1->fn_type.parameters[i], type2->fn_type.parameters[i]);
        }
        necro_mach_type_check(program, type1->fn_type.return_type, type2->fn_type.return_type);
        return;
    case NECRO_MACH_TYPE_PTR:
        necro_mach_type_check(program, type1->ptr_type.element_type, type2->ptr_type.element_type);
        return;
    default:
        return;
    }
}

bool necro_mach_type_is_unboxed(struct NecroMachProgram* program, NecroMachType* type)
{
    return type->type == program->necro_int_type->type
        || type->type == program->necro_float_type->type
        || type->type == program->necro_uint_type->type;
}

bool necro_mach_type_is_word_uint(struct NecroMachProgram* program, NecroMachType* type)
{
    return type->type == program->necro_uint_type->type;
}

NecroMachType* necro_mach_type_make_ptr_if_boxed(NecroMachProgram* program, NecroMachType* type)
{
    if (necro_mach_type_is_unboxed(program, type))
        return type;
    else
        return necro_mach_type_create_ptr(&program->arena, type);
}

///////////////////////////////////////////////////////
// Print
///////////////////////////////////////////////////////
void necro_mach_type_print_go(NecroMachType* type, bool is_recursive)
{
    if (type == NULL)
    {
        printf("NULL");
        return;
    }
    switch (type->type)
    {
    case NECRO_MACH_TYPE_VOID:
        printf("void");
        return;
    case NECRO_MACH_TYPE_UINT1:
        printf("u1");
        return;
    case NECRO_MACH_TYPE_UINT8:
        printf("u8");
        return;
    case NECRO_MACH_TYPE_UINT16:
        printf("u16");
        return;
    case NECRO_MACH_TYPE_UINT32:
        printf("u32");
        return;
    case NECRO_MACH_TYPE_UINT64:
        printf("u64");
        return;
    case NECRO_MACH_TYPE_INT32:
        printf("i32");
        return;
    case NECRO_MACH_TYPE_INT64:
        printf("i64");
        return;
    case NECRO_MACH_TYPE_F32:
        printf("f32");
        return;
    case NECRO_MACH_TYPE_F64:
        printf("f64");
        return;
    case NECRO_MACH_TYPE_CHAR:
        printf("char");
        return;
    case NECRO_MACH_TYPE_PTR:
        necro_mach_type_print_go(type->ptr_type.element_type, false);
        printf("*");
        return;
    case NECRO_MACH_TYPE_STRUCT:
        if (is_recursive)
        {
            printf("%s { ", type->struct_type.symbol->name->str);
            for (size_t i = 0; i < type->struct_type.num_members; ++i)
            {
                necro_mach_type_print_go(type->struct_type.members[i], false);
                if (i < type->struct_type.num_members - 1)
                    printf(", ");
            }
            printf(" }");
        }
        else
        {
            printf("%s", type->struct_type.symbol->name->str);
        }
        return;
    case NECRO_MACH_TYPE_FN:
        return;
    }
}

void necro_mach_type_print(NecroMachType* type)
{
    necro_mach_type_print_go(type, true);
}

///////////////////////////////////////////////////////
// From NecroType
///////////////////////////////////////////////////////
NecroMachType* necro_mach_type_fn_from_necro_type(NecroMachProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find(type);
    assert(type->type == NECRO_TYPE_FUN);
    // Count args
    size_t     arg_count = 0;
    size_t     arg_index = 0;
    NecroType* arrows    = necro_type_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        arg_count++;
        arrows = arrows->fun.type2;
        arrows = necro_type_find(arrows);
    }
    NecroMachType** args = necro_paged_arena_alloc(&program->arena, arg_count * sizeof(NecroMachType*));
    arrows               = necro_type_find(type);
    while (arrows->type == NECRO_TYPE_FUN)
    {
        args[arg_index] = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, arrows->fun.type1));
        arg_index++;
        arrows          = arrows->fun.type2;
        arrows          = necro_type_find(arrows);
    }
    NecroMachType* return_type   = necro_mach_type_make_ptr_if_boxed(program, necro_mach_type_from_necro_type(program, arrows));
    NecroMachType* function_type = necro_mach_type_create_fn(&program->arena, return_type, args, arg_count);
    return function_type;
}

// Notes:
// Should always be monomorphic
// Constructors should always be completely applied.
NecroMachType* necro_mach_type_from_necro_type(NecroMachProgram* program, NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_FUN:
        return necro_mach_type_fn_from_necro_type(program, type);
    case NECRO_TYPE_CON:
        assert(type->con.args == NULL);
        assert(type->con.con_symbol != NULL);
        assert(type->con.con_symbol->core_ast_symbol->mach_symbol != NULL);
        assert(type->con.con_symbol->core_ast_symbol->mach_symbol->mach_type != NULL);
        return type->con.con_symbol->core_ast_symbol->mach_symbol->mach_type;
    default:
        assert(false);
        return NULL;
    }
}

// ///////////////////////////////////////////////////////
// // Conversion
// ///////////////////////////////////////////////////////
// NecroType* necro_core_ast_to_necro_type(NecroMachProgram* program, NecroCoreAST_Expression* ast)
// {
//     assert(program != NULL);
//     assert(ast != NULL);
//     switch (ast->expr_type)
//     {
//     case NECRO_CORE_EXPR_LIT:
//         switch (ast->lit.type)
//         {
//         case NECRO_AST_CONSTANT_INTEGER: return necro_symtable_get(program->symtable, program->prim_types->int_type.id)->type;
//         case NECRO_AST_CONSTANT_FLOAT:   return necro_symtable_get(program->symtable, program->prim_types->float_type.id)->type;
//         default:                         assert(false); return NULL;
//         }
//     case NECRO_CORE_EXPR_VAR:
//         if (necro_symtable_get(program->symtable, ast->var.id)->closure_type != NULL)
//             return necro_symtable_get(program->symtable, ast->var.id)->closure_type;
//         else
//             return necro_symtable_get(program->symtable, ast->var.id)->type;
//     case NECRO_CORE_EXPR_BIND:
//         if (necro_symtable_get(program->symtable, ast->bind.var.id)->closure_type != NULL)
//             return necro_symtable_get(program->symtable, ast->bind.var.id)->closure_type;
//         else
//             return necro_symtable_get(program->symtable, ast->bind.var.id)->type;
//     case NECRO_CORE_EXPR_LAM:
//         assert(false && "TODO: Finish!");
//         return NULL;
//     case NECRO_CORE_EXPR_DATA_CON:
//         if (necro_symtable_get(program->symtable, ast->data_con.condid.id)->closure_type != NULL)
//             return necro_symtable_get(program->symtable, ast->data_con.condid.id)->closure_type;
//         else
//             return necro_symtable_get(program->symtable, ast->data_con.condid.id)->type;
//     case NECRO_CORE_EXPR_TYPE: return ast->type.type;
//     case NECRO_CORE_EXPR_APP:
//     {
//         // We're assuming that we always hit a var at the end...
//         NecroCoreAST_Expression* app = ast;
//         while (app->expr_type == NECRO_CORE_EXPR_APP)
//         {
//             app = app->app.exprA;
//         }
//         if (app->expr_type == NECRO_CORE_EXPR_VAR)
//         {
//             if (necro_symtable_get(program->symtable, app->var.id)->closure_type != NULL)
//                 return necro_symtable_get(program->symtable, app->var.id)->closure_type;
//             else
//                 return necro_symtable_get(program->symtable, app->var.id)->type;
//         }
//         else
//         {
//             assert(false);
//             return NULL;
//         }
//     }
//     case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
//     case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
//     case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
//     case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
//     default:                        assert(false && "Unimplemented AST type in necro_codegen_go"); return NULL;
//     }
// }
//
// NecroMachType* necro_core_pattern_type_to_machine_type(NecroMachProgram* program, NecroCoreAST_Expression* ast)
// {
//     NecroType* type = necro_core_ast_to_necro_type(program, ast);
//     type            = necro_type_find(type);
//     while (type->type == NECRO_TYPE_FOR)
//     {
//         type = type->for_all.type;
//         type = necro_type_find(type);
//     }
//     while (type->type == NECRO_TYPE_FUN)
//     {
//         type = type->fun.type2;
//         type = necro_type_find(type);
//     }
//     return necro_type_to_machine_type(program, type);
// }
//
// NecroMachType* necro_core_ast_to_machine_type(NecroMachProgram* program, NecroCoreAST_Expression* ast)
// {
//     assert(program != NULL);
//     assert(ast != NULL);
//     switch (ast->expr_type)
//     {
//     case NECRO_CORE_EXPR_LIT:
//         switch (ast->lit.type)
//         {
//         case NECRO_AST_CONSTANT_INTEGER: return program->necro_int_type;
//         case NECRO_AST_CONSTANT_FLOAT:   return program->necro_float_type;
//         default:                         assert(false); return NULL;
//         }
//     case NECRO_CORE_EXPR_VAR:            return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
//     case NECRO_CORE_EXPR_BIND:           return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
//     case NECRO_CORE_EXPR_LAM:
//         assert(false && "TODO: Finish!");
//         return NULL;
//     case NECRO_CORE_EXPR_DATA_CON:  return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
//     case NECRO_CORE_EXPR_TYPE:      return necro_type_to_machine_type(program, ast->type.type);
//     case NECRO_CORE_EXPR_APP:       return necro_type_to_machine_type(program, necro_core_ast_to_necro_type(program, ast));
//     case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
//     case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
//     case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
//     case NECRO_CORE_EXPR_LIST:      assert(false); return NULL; // used for top decls not language lists
//     default:                        assert(false && "Unimplemented AST type in necro_core_ast_to_machine_type"); return NULL;
//     }
// }
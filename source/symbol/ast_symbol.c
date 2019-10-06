/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "ast_symbol.h"
#include "type.h"
#include "utility/math.h"

NecroAstSymbol* necro_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, struct NecroAst* ast)
{
    NecroAstSymbol* ast_symbol = necro_paged_arena_alloc(arena, sizeof(NecroAstSymbol));
    *ast_symbol = (NecroAstSymbol)
    {
        .name                    = name,
        .source_name             = source_name,
        .module_name             = module_name,
        .ast                     = ast,
        .optional_type_signature = NULL,
        .declaration_group       = NULL,
        .type                    = NULL,
        .con_num                 = 0,
        .is_enum                 = true,
        .is_constructor          = false,
        .is_primitive            = false,
        .type_status             = NECRO_TYPE_UNCHECKED,
        .is_recursive            = false,
        .is_unboxed              = false,
        .is_wrapper              = false,
        .instance_list           = NULL,
        .method_type_class       = NULL,
        .type_class              = NULL,
        .type_class_instance     = NULL,
        .necro_machine_ast       = NULL,
        .usage                   = NULL,
        .primop_type             = NECRO_PRIMOP_NONE,
    };
    return ast_symbol;
}

NecroAstSymbol* necro_ast_symbol_deep_copy(NecroPagedArena* arena, NecroAstSymbol* ast_symbol)
{
    NecroAstSymbol* new_symbol = necro_paged_arena_alloc(arena, sizeof(NecroAstSymbol));
    *new_symbol                = (NecroAstSymbol)
    {
        .name                    = ast_symbol->name,
        .source_name             = ast_symbol->source_name,
        .module_name             = ast_symbol->module_name,
        .ast                     = NULL,
        .optional_type_signature = NULL,
        .declaration_group       = NULL,
        .type                    = necro_type_deep_copy(arena, ast_symbol->type),
        .con_num                 = ast_symbol->con_num,
        .is_constructor          = ast_symbol->is_constructor,
        .is_primitive            = ast_symbol->is_primitive,
        .type_status             = ast_symbol->type_status,
        .is_recursive            = ast_symbol->is_recursive,
        .is_unboxed              = ast_symbol->is_unboxed,
        .is_wrapper              = false,
        .instance_list           = NULL,
        .method_type_class       = NULL,
        .type_class              = NULL,
        .type_class_instance     = NULL,
        .necro_machine_ast       = NULL,
        .usage                   = ast_symbol->usage,
        .primop_type             = NECRO_PRIMOP_NONE,
    };
    return new_symbol;
}

const char* necro_ast_symbol_most_qualified_name(NecroAstSymbol* ast_symbol)
{
    if (ast_symbol->name != NULL && ast_symbol->name->str != NULL)
        return ast_symbol->name->str;
    else if (ast_symbol->source_name != NULL && ast_symbol->source_name->str != NULL)
        return ast_symbol->source_name->str;
    else
        return "NULL AST_SYMBOL";
}

void necro_ast_symbol_print_type_and_kind(NecroAstSymbol* ast_symbol, size_t num_white_spaces)
{
    assert(ast_symbol != NULL);
    print_white_space(num_white_spaces);
    printf("%s ", ast_symbol->name->str);
    size_t length = strlen(ast_symbol->name->str);
    size_t offset = MAX(25, length) - length;
    for (size_t i = 0; i < offset; i++)
        printf(" ");
    printf(" :: ");
    if (ast_symbol->type == NULL)
    {
        printf(" NULL_TYPE!");
        return;
    }
    necro_type_print(ast_symbol->type);
    printf(" : ");
    if (ast_symbol->type->kind == NULL)
    {
        printf(" NULL_KIND!");
    }
    necro_type_print(ast_symbol->type->kind);
}

NecroCoreAstSymbol* necro_core_ast_symbol_create(NecroPagedArena* core_ast_arena, NecroSymbol name, NecroType* type)
{
    NecroCoreAstSymbol* core_ast_symbol = necro_paged_arena_alloc(core_ast_arena, sizeof(NecroCoreAstSymbol));
    core_ast_symbol->name               = name;
    core_ast_symbol->source_name        = name;
    core_ast_symbol->module_name        = NULL;
    core_ast_symbol->ast                = NULL;
    core_ast_symbol->inline_ast         = NULL;
    core_ast_symbol->type               = necro_type_deep_copy(core_ast_arena, type);
    core_ast_symbol->is_constructor     = false;
    core_ast_symbol->is_primitive       = false;
    core_ast_symbol->con_num            = 0;
    core_ast_symbol->is_enum            = false;
    core_ast_symbol->is_recursive       = false;
    core_ast_symbol->is_unboxed         = false;
    core_ast_symbol->is_wrapper         = false;
    core_ast_symbol->free_vars          = NULL;
    core_ast_symbol->static_value       = NULL;
    core_ast_symbol->mach_symbol        = NULL;
    core_ast_symbol->arity              = 0;
    core_ast_symbol->state_type         = NECRO_STATE_CONSTANT;
    core_ast_symbol->arity              = 0;
    core_ast_symbol->deep_copy_fn       = NULL;
    core_ast_symbol->is_deep_copy_fn    = false;
    core_ast_symbol->is_wildcard        = false;
    core_ast_symbol->primop_type        = NECRO_PRIMOP_NONE;
    return core_ast_symbol;
}

NecroCoreAstSymbol* necro_core_ast_symbol_create_from_ast_symbol(NecroPagedArena* core_ast_arena, NecroAstSymbol* ast_symbol)
{
    if (ast_symbol->core_ast_symbol != NULL)
        return ast_symbol->core_ast_symbol;
    NecroCoreAstSymbol* core_ast_symbol = necro_paged_arena_alloc(core_ast_arena, sizeof(NecroCoreAstSymbol));
    core_ast_symbol->name               = ast_symbol->name;
    core_ast_symbol->source_name        = ast_symbol->source_name;
    core_ast_symbol->module_name        = ast_symbol->module_name;
    core_ast_symbol->ast                = NULL;
    core_ast_symbol->inline_ast         = NULL;
    core_ast_symbol->type               = necro_type_deep_copy(core_ast_arena, ast_symbol->type);
    core_ast_symbol->is_constructor     = ast_symbol->is_constructor;
    core_ast_symbol->is_primitive       = ast_symbol->is_primitive;
    core_ast_symbol->con_num            = ast_symbol->con_num;
    core_ast_symbol->is_enum            = ast_symbol->is_enum;
    core_ast_symbol->is_recursive       = ast_symbol->is_recursive;
    core_ast_symbol->is_unboxed         = ast_symbol->is_unboxed;
    core_ast_symbol->is_wrapper         = ast_symbol->is_wrapper;
    core_ast_symbol->free_vars          = NULL;
    core_ast_symbol->mach_symbol        = NULL;
    core_ast_symbol->static_value       = NULL;
    core_ast_symbol->arity              = 0;
    ast_symbol->core_ast_symbol         = core_ast_symbol;
    core_ast_symbol->state_type         = NECRO_STATE_CONSTANT;
    core_ast_symbol->outer              = NULL;
    core_ast_symbol->deep_copy_fn       = NULL;
    core_ast_symbol->is_deep_copy_fn    = false;
    core_ast_symbol->is_wildcard        = false;
    core_ast_symbol->primop_type        = ast_symbol->primop_type = ast_symbol->primop_type;
    return core_ast_symbol;
}

NecroCoreAstSymbol* necro_core_ast_symbol_create_by_renaming(NecroPagedArena* core_ast_arena, NecroSymbol new_name, NecroCoreAstSymbol* ast_symbol)
{
    NecroCoreAstSymbol* core_ast_symbol = necro_paged_arena_alloc(core_ast_arena, sizeof(NecroCoreAstSymbol));
    core_ast_symbol->name               = new_name;
    core_ast_symbol->source_name        = ast_symbol->source_name;
    core_ast_symbol->module_name        = ast_symbol->module_name;
    core_ast_symbol->ast                = NULL;
    core_ast_symbol->inline_ast         = NULL;
    core_ast_symbol->type               = necro_type_deep_copy(core_ast_arena, ast_symbol->type);
    core_ast_symbol->is_constructor     = ast_symbol->is_constructor;
    core_ast_symbol->is_primitive       = ast_symbol->is_primitive;
    core_ast_symbol->con_num            = ast_symbol->con_num;
    core_ast_symbol->is_enum            = ast_symbol->is_enum;
    core_ast_symbol->is_recursive       = ast_symbol->is_recursive;
    core_ast_symbol->is_unboxed         = ast_symbol->is_unboxed;
    core_ast_symbol->is_wrapper         = ast_symbol->is_wrapper;
    core_ast_symbol->free_vars          = NULL;
    core_ast_symbol->static_value       = NULL;
    core_ast_symbol->mach_symbol        = NULL;
    core_ast_symbol->arity              = ast_symbol->arity;
    core_ast_symbol->state_type         = ast_symbol->state_type;
    core_ast_symbol->arity              = ast_symbol->arity;
    core_ast_symbol->deep_copy_fn       = NULL;
    core_ast_symbol->is_deep_copy_fn    = false;
    core_ast_symbol->is_wildcard        = ast_symbol->is_wildcard;
    core_ast_symbol->primop_type        = ast_symbol->primop_type;
    return core_ast_symbol;
}

const char* necro_core_ast_symbol_most_qualified_name(NecroCoreAstSymbol* core_ast_symbol)
{
    if (core_ast_symbol->name != NULL && core_ast_symbol->name->str != NULL)
        return core_ast_symbol->name->str;
    else if (core_ast_symbol->source_name != NULL && core_ast_symbol->source_name->str != NULL)
        return core_ast_symbol->source_name->str;
    else
        return "NULL AST_SYMBOL";
}

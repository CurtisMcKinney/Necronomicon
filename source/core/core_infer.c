/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core_infer.h"
#include <stdio.h>
#include "type.h"
#include "kind.h"

///////////////////////////////////////////////////////
// NecroCoreInfer
///////////////////////////////////////////////////////
typedef struct NecroCoreInfer
{
    NecroPagedArena*   arena;
    NecroIntern*       intern;
    NecroBase*         base;
    NecroCoreAstArena* core_ast_arena;
    // NecroConstraintEnv con_env;
} NecroCoreInfer;

NecroCoreInfer necro_core_infer_create(NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    return (NecroCoreInfer)
    {
        .arena          = &core_ast_arena->arena,
        .intern         = intern,
        .base           = base,
        .core_ast_arena = core_ast_arena,
    };
}

///////////////////////////////////////////////////////
// Infer Go
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_core_infer_go(NecroCoreInfer* infer, NecroCoreAst* ast);

NecroResult(NecroType) necro_core_infer_lit(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LIT);
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:   return ok(NecroType, infer->base->float_type->type);
    case NECRO_AST_CONSTANT_INTEGER:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN: return ok(NecroType, infer->base->int_type->type);
    case NECRO_AST_CONSTANT_CHAR:
    case NECRO_AST_CONSTANT_CHAR_PATTERN:    return ok(NecroType, infer->base->char_type->type);
    case NECRO_AST_CONSTANT_STRING:
    {
        NecroType* arity_type = necro_type_nat_create(infer->arena, strlen(ast->lit.string_literal->str) + 1);
        arity_type->kind      = infer->base->nat_kind->type;
        NecroType* array_type = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, infer->base->char_type->type);
        ast->necro_type       = array_type;
        return ok(NecroType, array_type);
    }
    case NECRO_AST_CONSTANT_ARRAY:
    {
        size_t            count        = 0;
        NecroCoreAstList* elements     = ast->lit.array_literal_elements;
        NecroType*        element_type = necro_type_fresh_var(infer->arena, NULL);
        element_type->kind             = infer->base->star_kind->type;
        while (elements != NULL)
        {
            count++;
            NecroType* element_expr_type = necro_try_result(NecroType, necro_core_infer_go(infer, elements->data));
            necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, element_type, element_expr_type, NULL, zero_loc, zero_loc));
            elements = elements->next;
        }
        NecroType* arity_type = necro_type_nat_create(infer->arena, count);
        arity_type->kind      = infer->base->nat_kind->type;
        NecroType* array_type = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, element_type);
        ast->necro_type       = array_type;
        return ok(NecroType, array_type);
    }
    case NECRO_AST_CONSTANT_TYPE_INT:
    default:
        assert(false);
        return ok(NecroType, NULL);
    }
}

NecroResult(NecroType) necro_core_infer_var(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    if (ast->var.ast_symbol == infer->base->prim_undefined->core_ast_symbol)
    {
        NecroType* type_var = necro_type_fresh_var(infer->arena, NULL);
        type_var->kind      = infer->base->star_kind->type;
        type_var->ownership = infer->base->ownership_share->type;
        if (ast->necro_type == NULL)
        {
            ast->necro_type = type_var;
        }
        else
        {
            unwrap(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, ast->necro_type, type_var, NULL, zero_loc, zero_loc));
        }
        return ok(NecroType, type_var);
    }
    else if (ast->var.ast_symbol->type->type == NECRO_TYPE_FOR)
    {
        NecroType* type = necro_try_result(NecroType, necro_type_instantiate(infer->arena, NULL, infer->base, ast->var.ast_symbol->type, NULL));
        if (type->ownership == NULL)
            type->ownership = infer->base->ownership_share->type;
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->var.ast_symbol->type, zero_loc, zero_loc));
        unwrap(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, ast->necro_type, type, NULL, zero_loc, zero_loc));
        return ok(NecroType, type);
    }
    else
    {
        if (ast->var.ast_symbol->type->ownership == NULL)
            ast->var.ast_symbol->type->ownership = infer->base->ownership_share->type;
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->var.ast_symbol->type, zero_loc, zero_loc));
        return ok(NecroType, ast->var.ast_symbol->type);
    }
}

NecroResult(NecroType) necro_core_infer_for(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_FOR);
    NecroType* range_init_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->for_loop.range_init));
    NecroType* value_init_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->for_loop.value_init));
    NecroType* index_arg_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->for_loop.index_arg));
    NecroType* value_arg_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->for_loop.value_arg));
    NecroType* expression_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->for_loop.expression));
    NecroType* n_type          = necro_type_fresh_var(infer->arena, NULL);
    NecroType* inner_type      = necro_type_fresh_var(infer->arena, NULL);
    NecroType* range_inner     = necro_type_con2_create(infer->arena, infer->base->range_type, n_type, inner_type);
    unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, range_inner, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, range_inner, range_init_type, NULL, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, inner_type, index_arg_type, NULL, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, value_init_type, value_arg_type, NULL, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, value_arg_type, expression_type, NULL, zero_loc, zero_loc));
    return ok(NecroType, expression_type);
}

NecroResult(NecroType) necro_core_infer_app(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    NecroType* expr1_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->app.expr1));
    NecroType* expr2_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->app.expr2));
    NecroType* result_type = necro_type_fresh_var(infer->arena, NULL);
    NecroType* fn_type     = necro_type_fn_create(infer->arena, expr2_type, result_type);
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, expr1_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, fn_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify(infer->arena, NULL, infer->base, expr1_type, fn_type, NULL));
    return ok(NecroType, result_type);
}

NecroResult(NecroType) necro_core_infer_lam(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    NecroType* arg_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->lambda.arg));
    NecroType* expr_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->lambda.expr));
    return ok(NecroType, necro_type_fn_create(infer->arena, arg_type, expr_type));
}

NecroResult(NecroType) necro_core_infer_bind(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    NecroType* expr_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->bind.expr));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, expr_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->bind.ast_symbol->type, zero_loc, zero_loc));
    return necro_type_unify(infer->arena, NULL, infer->base, ast->bind.ast_symbol->type, expr_type, NULL);
}

// TODO: Array Literals
NecroResult(NecroType) necro_core_infer_let(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LET);
    while (ast != NULL)
    {
        if (ast->ast_type != NECRO_CORE_AST_LET)
            return necro_core_infer_go(infer, ast);
        necro_try(NecroType, necro_core_infer_go(infer, ast->let.bind));
        ast = ast->let.expr;
    }
    return ok(NecroType, NULL);
}

NecroResult(NecroType) necro_core_infer_case(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    NecroType* expression_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->case_expr.expr));
    NecroType* result_type     = necro_type_fresh_var(infer->arena, NULL);
    result_type->kind          = infer->base->star_kind->type;
    NecroCoreAstList* alts     = ast->case_expr.alts;
    while (alts != NULL)
    {
        NecroCoreAst* alt       = alts->data;
        NecroType*    pat_type  = (alt->case_alt.pat == NULL) ? necro_type_fresh_var(infer->arena, NULL) : necro_try_result(NecroType, necro_core_infer_go(infer, alt->case_alt.pat));
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, pat_type, zero_loc, zero_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, expression_type, pat_type, NULL, zero_loc, zero_loc));
        NecroType*    body_type = necro_try_result(NecroType, necro_core_infer_go(infer, alt->case_alt.expr));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, result_type, body_type, NULL, zero_loc, zero_loc));
        alts = alts->next;
    }
    return ok(NecroType, result_type);
}

NecroResult(NecroType) necro_core_infer_go(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    if (ast == NULL)
        return ok(NecroType, NULL);
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_LIT:       return necro_core_infer_lit(infer, ast);
    case NECRO_CORE_AST_VAR:       return necro_core_infer_var(infer, ast);
    case NECRO_CORE_AST_APP:       return necro_core_infer_app(infer, ast);
    case NECRO_CORE_AST_LAM:       return necro_core_infer_lam(infer, ast);
    case NECRO_CORE_AST_CASE:      return necro_core_infer_case(infer, ast);
    case NECRO_CORE_AST_FOR:       return necro_core_infer_for(infer, ast);
    case NECRO_CORE_AST_LET:       return necro_core_infer_let(infer, ast);
    case NECRO_CORE_AST_BIND:      return necro_core_infer_bind(infer, ast);
    // // case NECRO_CORE_AST_BIND_REC:
    case NECRO_CORE_AST_DATA_DECL: return ok(NecroType, NULL);
    case NECRO_CORE_AST_DATA_CON:  return ok(NecroType, NULL);
    default:                       assert(false && "Unimplemented Ast in necro_core_infer_go"); return ok(NecroType, NULL);
    }
}

NecroResult(void) necro_core_infer(NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    NecroCoreInfer infer = necro_core_infer_create(intern, base, core_ast_arena);
    return necro_error_map(NecroType, void, necro_core_infer_go(&infer, core_ast_arena->root));
}


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

// TODO: With uvars in numbers, isn't this wrong now?
NecroResult(NecroType) necro_core_infer_lit(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LIT);
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
        ast->necro_type = infer->base->float_type->type;
        return ok(NecroType, ast->necro_type);
    case NECRO_AST_CONSTANT_INTEGER:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
        ast->necro_type = infer->base->int_type->type;
        return ok(NecroType, ast->necro_type);
    case NECRO_AST_CONSTANT_UNSIGNED_INTEGER:
    case NECRO_AST_CONSTANT_UNSIGNED_INTEGER_PATTERN:
        ast->necro_type = infer->base->uint_type->type;
        return ok(NecroType, ast->necro_type);
    case NECRO_AST_CONSTANT_CHAR:
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
        ast->necro_type = infer->base->char_type->type;
        return ok(NecroType, ast->necro_type);
    case NECRO_AST_CONSTANT_STRING:
    {
        NecroType* arity_type = necro_type_nat_create(infer->arena, strlen(ast->lit.string_literal->str));
        arity_type->kind      = infer->base->nat_kind->type;
        NecroType* array_type = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, infer->base->char_type->type);
        ast->necro_type       = array_type;
        assert(ast->necro_type != NULL);
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
        assert(ast->necro_type != NULL);
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
        assert(ast->necro_type != NULL);
        return ok(NecroType, type_var);
    }
    else
    {
        assert(ast->necro_type != NULL);
        NecroType* type = NULL;
        if (ast->var.ast_symbol->type->type == NECRO_TYPE_FOR)
        {
            type = necro_type_instantiate(infer->arena, NULL, infer->base, ast->var.ast_symbol->type, NULL, zero_loc, zero_loc);
        }
        else
        {
            type = ast->var.ast_symbol->type;
        }
        // For now we're simply turning off ownership inference in core...
        ast->necro_type->ownership = infer->base->ownership_share->type;
        type->ownership            = infer->base->ownership_share->type;
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type, zero_loc, zero_loc));
        necro_type_unify_con_uninhabited_args(infer->arena, infer->base, ast->necro_type, type);
        // unwrap(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, ast->necro_type, type, NULL, zero_loc, zero_loc));
        ast->necro_type = type; // For now we're simply forcing the ast to take the type instead of unifying...
        necro_type_assert_no_rigid_variables(type);
        return ok(NecroType, type);
    }
}

NecroResult(NecroType) necro_core_infer_loop(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LOOP);
    NecroType* value_pat_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->loop.value_pat));
    NecroType* value_init_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->loop.value_init));
    if (ast->loop.loop_type == NECRO_LOOP_FOR)
    {
        NecroType* index_pat_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->loop.for_loop.index_pat));
        NecroType* range_init_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->loop.for_loop.range_init));
        index_pat_type             = necro_type_strip_for_all(necro_type_find(index_pat_type));
        range_init_type            = necro_type_strip_for_all(necro_type_find(range_init_type));
        if (index_pat_type->type == NECRO_TYPE_CON && index_pat_type->con.args != NULL)
        {
            // Pre Type Monomorphization
            NecroType* n_type     = necro_type_fresh_var(infer->arena, NULL);
            NecroType* range_type = necro_type_con1_create(infer->arena, infer->base->range_type, n_type);
            NecroType* index_type = necro_type_con1_create(infer->arena, infer->base->index_type, n_type);
            unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, range_init_type, zero_loc, zero_loc));
            unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, range_type, zero_loc, zero_loc));
            unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, index_type, zero_loc, zero_loc));
            necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, range_type, range_init_type, NULL, zero_loc, zero_loc));
            necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, index_type, index_pat_type, NULL, zero_loc, zero_loc));
        }
    }
    else
    {
        NecroType* while_expression_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->loop.while_loop.while_expression));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, infer->base->bool_type->type, while_expression_type, NULL, zero_loc, zero_loc));
    }
    NecroType* do_expression_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->loop.do_expression));
    unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, value_init_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, value_init_type, value_pat_type, NULL, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, value_init_type, do_expression_type, NULL, zero_loc, zero_loc));
    ast->necro_type = necro_type_find(do_expression_type);
    assert(ast->necro_type != NULL);
    return ok(NecroType, do_expression_type);
}

NecroResult(NecroType) necro_core_infer_app(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    NecroType* expr1_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->app.expr1));
    NecroType* expr2_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->app.expr2));
    assert(expr1_type != NULL);
    assert(expr2_type != NULL);
    NecroType* result_type = necro_type_fresh_var(infer->arena, NULL);
    NecroType* fn_type     = necro_type_fn_create(infer->arena, expr2_type, result_type);
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, expr1_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, fn_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_type_unify(infer->arena, NULL, infer->base, expr1_type, fn_type, NULL));
    ast->necro_type       = result_type;
    assert(ast->necro_type != NULL);
    return ok(NecroType, result_type);
}

NecroResult(NecroType) necro_core_infer_lam(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    NecroType* arg_type  = necro_try_result(NecroType, necro_core_infer_go(infer, ast->lambda.arg));
    NecroType* expr_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->lambda.expr));
    ast->necro_type      = necro_type_fn_create(infer->arena, arg_type, expr_type);
    assert(ast->necro_type != NULL);
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_core_infer_bind(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    if (ast->bind.initializer != NULL)
    {
        ast->bind.initializer->necro_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->bind.initializer));
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->bind.initializer->necro_type, zero_loc, zero_loc));
    }
    NecroType* expr_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->bind.expr));
    assert(ast->bind.expr->necro_type != NULL);
    assert(expr_type != NULL);
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, expr_type, zero_loc, zero_loc));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->bind.ast_symbol->type, zero_loc, zero_loc));
    if (ast->bind.initializer != NULL)
    {
        necro_try(NecroType, necro_type_unify(infer->arena, NULL, infer->base, expr_type, ast->bind.initializer->necro_type, NULL));
    }
    assert(ast->bind.ast_symbol->type != NULL);
    necro_try_result(NecroType, necro_type_unify(infer->arena, NULL, infer->base, ast->bind.ast_symbol->type, expr_type, NULL));
    necro_type_unify_con_uninhabited_args(infer->arena, infer->base, ast->necro_type, ast->bind.ast_symbol->type);
    ast->necro_type = ast->bind.ast_symbol->type;
    assert(ast->necro_type != NULL);
    return ok(NecroType, ast->necro_type);
}

// TODO: Array Literals
// TODO: Non-Recursive style
NecroResult(NecroType) necro_core_infer_let(NecroCoreInfer* infer, NecroCoreAst* ast)
{
    if (ast == NULL)
        return ok(NecroType, NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LET);
    necro_try(NecroType, necro_core_infer_go(infer, ast->let.bind));
    ast->necro_type = necro_try_result(NecroType, necro_core_infer_go(infer, ast->let.expr));
    // if (ast->let.expr != NULL)
    //     assert(ast->necro_type != NULL);
    return ok(NecroType, ast->necro_type);
}

// NecroResult(NecroType) necro_core_infer_let(NecroCoreInfer* infer, NecroCoreAst* ast)
// {
//     assert(ast->ast_type == NECRO_CORE_AST_LET);
//     while (ast != NULL)
//     {
//         if (ast->ast_type != NECRO_CORE_AST_LET)
//             return necro_core_infer_go(infer, ast);
//         necro_try(NecroType, necro_core_infer_go(infer, ast->let.bind));
//         ast = ast->let.expr;
//     }
//     return ok(NecroType, NULL);
// }

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
        if (alt->case_alt.expr == NULL)
            break;
        if (alt->case_alt.expr->ast_type == NECRO_CORE_AST_LET && alt->case_alt.expr->necro_type == NULL)
            break;
        NecroType*    body_type = necro_try_result(NecroType, necro_core_infer_go(infer, alt->case_alt.expr));
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, body_type, zero_loc, zero_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, result_type, body_type, NULL, zero_loc, zero_loc));
        alt->case_alt.expr->necro_type = body_type;
        // assert(alt->case_alt.expr->necro_type != NULL);
        // necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, result_type, body_type, NULL, zero_loc, zero_loc));
        alts = alts->next;
    }
    ast->necro_type = result_type;
    assert(ast->necro_type != NULL);
    return ok(NecroType, ast->necro_type);
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
    case NECRO_CORE_AST_LOOP:      return necro_core_infer_loop(infer, ast);
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


///////////////////////////////////////////////////////
// Hash
///////////////////////////////////////////////////////
size_t necro_core_ast_hash(NecroCoreAst* ast)
{
    if (ast == NULL)
        return 0;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:       return 6803 ^ ast->var.ast_symbol->name->hash;
    case NECRO_CORE_AST_CASE_ALT:  return 4229 ^ necro_core_ast_hash(ast->case_alt.pat) ^ necro_core_ast_hash(ast->case_alt.expr);
    case NECRO_CORE_AST_APP:       return 6967 ^ necro_core_ast_hash(ast->app.expr1) ^ necro_core_ast_hash(ast->app.expr2);
    case NECRO_CORE_AST_LAM:       return 1747 ^ necro_core_ast_hash(ast->lambda.arg) ^ necro_core_ast_hash(ast->lambda.expr);
    case NECRO_CORE_AST_BIND:      return 2381 ^ ast->bind.ast_symbol->name->hash ^ necro_core_ast_hash(ast->bind.expr) ^ necro_core_ast_hash(ast->bind.initializer);
    case NECRO_CORE_AST_LET:       return 7873 ^ necro_core_ast_hash(ast->let.bind) ^ necro_core_ast_hash(ast->let.expr);
    case NECRO_CORE_AST_LOOP:
        if (ast->loop.loop_type == NECRO_LOOP_FOR)
            return 7529 ^ ast->loop.for_loop.max_loops ^ necro_core_ast_hash(ast->loop.for_loop.range_init) ^ necro_core_ast_hash(ast->loop.value_init) ^ necro_core_ast_hash(ast->loop.for_loop.index_pat) ^ necro_core_ast_hash(ast->loop.value_pat) ^ necro_core_ast_hash(ast->loop.do_expression);
        else
            return 7531 ^ necro_core_ast_hash(ast->loop.while_loop.while_expression) ^ necro_core_ast_hash(ast->loop.value_init) ^ necro_core_ast_hash(ast->loop.value_pat) ^ necro_core_ast_hash(ast->loop.do_expression);
    case NECRO_CORE_AST_CASE:
    {
        size_t            hash = 6367 ^ necro_core_ast_hash(ast->case_expr.expr);
        NecroCoreAstList* alts = ast->case_expr.alts;
        while (alts != NULL)
        {
            hash = hash ^ necro_core_ast_hash(alts->data);
            alts = alts->next;
        }
        return hash;
    }
    case NECRO_CORE_AST_LIT:
    {
        if (ast->lit.type != NECRO_AST_CONSTANT_ARRAY)
            return 5333 ^ (size_t)ast->lit.uint_literal;
        size_t            hash     = 5449;
        NecroCoreAstList* elements = ast->lit.array_literal_elements;
        while (elements != NULL)
        {
            hash     = hash ^ necro_core_ast_hash(elements->data);
            elements = elements->next;
        }
        return hash;
    }
    case NECRO_CORE_AST_BIND_REC:
    case NECRO_CORE_AST_DATA_DECL:
    case NECRO_CORE_AST_DATA_CON:
    default:                       assert(false && "Unimplemented Ast in necro_core_ast_hash"); return 0;
    }
}

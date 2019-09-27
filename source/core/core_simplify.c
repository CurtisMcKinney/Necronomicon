/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdint.h>
#include <inttypes.h>

#include "core_simplify.h"
#include "type.h"
#include "core_infer.h"
#include "alias_analysis.h"
#include "monomorphize.h"
#include "kind.h"

NecroCoreAst* necro_core_ast_duplicate_with_subs(NecroPagedArena* arena, NecroCoreAst* ast, NecroCoreAstSymbolSubList* subs)
{
    UNUSED(arena);
    UNUSED(ast);
    UNUSED(subs);
    return NULL;
}

///////////////////////////////////////////////////////
// Pre-Simplify
//--------------------
// Very simple pre-simplification phase using hardcoded rewrite rules
// Performs these simplifications:
//     * Unwraps "wrapped" types (i.e. types of shape data Wrapper a = Wrapper a (polymorphism isn't important, it's the fact that it's a single constructor with a single member))
//     * Inlines id
//     * Inlines |>, <|, >>, <<
//     * Inlines lambda applications of the form: ((\x -> ...) y) ==> (let x = y in ...)
// We do this before defunctionalization because we want to reduce the amount of higher order functions floating around,
// which will in turn make defunctionalization smoother, as well as produce faster code.
// NOTE: Need to be very careful about floating asts in and out of case expressions or lambdas, because in necrolang this can semantically alter the program's meaning!!!
///////////////////////////////////////////////////////

/*
    TODO:
        * Need to unwrap data declaration types!
*/

typedef struct NecroCorePreSimplify
{
    NecroPagedArena* arena;
    NecroIntern*     intern;
    NecroBase*       base;
} NecroCorePreSimplify;

NecroCoreAst* necro_core_ast_pre_simplify_go(NecroCorePreSimplify* context, NecroCoreAst* ast);
NecroType*    necro_type_inline_wrapper_types(NecroPagedArena* arena, NecroBase* base, NecroType* type);

NecroCoreAst* necro_core_ast_pre_simplify_inline_wrapper_types(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    ast->necro_type = necro_type_inline_wrapper_types(context->arena, context->base, ast->necro_type);
    return ast;
}

NecroCoreAst* necro_core_ast_pre_simplify_var(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    if (ast->var.ast_symbol->inline_ast != NULL)
    {
        return necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_deep_copy(context->arena, ast->var.ast_symbol->inline_ast));
    }
    else if (ast->var.ast_symbol->is_constructor && ast->var.ast_symbol->is_wrapper)
    {
        // Replace wrapper type cons with 'id' lambda
        NecroType*          unwrapped_con_type = necro_type_inline_wrapper_types(context->arena, context->base, ast->necro_type);
        assert(unwrapped_con_type->type == NECRO_TYPE_FUN);
        NecroType*          arg_type           = unwrapped_con_type->fun.type1;
        NecroCoreAstSymbol* new_symbol    = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "x"), arg_type);
        NecroCoreAst*       new_var       = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_var(context->arena, new_symbol, arg_type));
        NecroCoreAst*       new_lam       = necro_core_ast_create_lam(context->arena, new_var, necro_core_ast_deep_copy(context->arena, new_var));
        new_lam->necro_type               = necro_type_fn_create(context->arena, arg_type, arg_type);
        return new_lam;
    }
    else
    {
        ast->var.ast_symbol->type = necro_type_inline_wrapper_types(context->arena, context->base, ast->var.ast_symbol->type);
        return necro_core_ast_pre_simplify_inline_wrapper_types(context, ast);
    }
}

NecroCoreAst* necro_core_ast_pre_simplify_lit(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LIT);
    if (ast->lit.type != NECRO_AST_CONSTANT_ARRAY)
        return necro_core_ast_pre_simplify_inline_wrapper_types(context, ast);

    // TODO:

    // size_t            count        = 0;
    // NecroCoreAstList* elements     = ast->lit.array_literal_elements;
    // while (elements != NULL)
    // {
    //     count++;
    //     NecroType* element_expr_type = necro_try_result(NecroType, necro_core_infer_go(infer, elements->data));
    //     necro_try(NecroType, necro_type_unify_with_info(infer->arena, NULL, infer->base, element_type, element_expr_type, NULL, zero_loc, zero_loc));
    //     elements = elements->next;
    // }
    // NecroType* arity_type = necro_type_nat_create(infer->arena, count);
    // arity_type->kind      = infer->base->nat_kind->type;
    // NecroType* array_type = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, element_type);
    // ast->necro_type       = array_type;
    // assert(ast->necro_type != NULL);

    return necro_core_ast_pre_simplify_inline_wrapper_types(context, ast);
}

// TODO: Finish!
NecroCoreAst* necro_core_ast_pre_simplify_case(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_CASE);

    // rewrite case expressions over wrapped types
    NecroType* case_type = necro_type_find(ast->case_expr.expr->necro_type);
    if (case_type->type == NECRO_TYPE_CON && case_type->con.con_symbol->is_wrapper && ast->case_expr.alts->next == NULL)
    {
        NecroCoreAst* alt = ast->case_expr.alts->data;
        NecroCoreAst* pat = alt->case_alt.pat;
        if(
            pat->ast_type == NECRO_CORE_AST_APP &&
            pat->app.expr1->ast_type == NECRO_CORE_AST_VAR &&
            pat->app.expr1->var.ast_symbol->is_constructor &&
            pat->app.expr1->var.ast_symbol->is_wrapper &&
            pat->app.expr2->ast_type == NECRO_CORE_AST_VAR &&
            !pat->app.expr2->var.ast_symbol->is_constructor)
        {
            NecroCoreAst*       con_var    = pat->app.expr2;
            NecroCoreAstSymbol* new_symbol = necro_core_ast_symbol_create_by_renaming(context->arena, necro_intern_unique_string(context->intern, con_var->var.ast_symbol->name->str), con_var->var.ast_symbol);
            NecroCoreAst*       new_var    = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_var(context->arena, new_symbol, con_var->necro_type));
            con_var->var.ast_symbol->inline_ast = new_var;
            NecroCoreAst*       new_bind   = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_bind(context->arena, new_symbol, ast->case_expr.expr, NULL));
            NecroCoreAst*       new_let    = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_let(context->arena, new_bind, alt->case_alt.expr));
            return new_let;
        }
    }
    // Inline var
    else if (ast->case_expr.alts->next == NULL)
    {
        NecroCoreAst* alt = ast->case_expr.alts->data;
        if (alt->case_alt.pat->ast_type == NECRO_CORE_AST_VAR)
        {
            NecroCoreAst*       var        = alt->case_alt.pat;
            NecroCoreAstSymbol* new_symbol = necro_core_ast_symbol_create_by_renaming(context->arena, necro_intern_unique_string(context->intern, var->var.ast_symbol->name->str), var->var.ast_symbol);
            NecroCoreAst*       new_var    = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_var(context->arena, new_symbol, var->necro_type));
            var->var.ast_symbol->inline_ast = new_var;
            NecroCoreAst*       new_bind   = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_bind(context->arena, new_symbol, ast->case_expr.expr, NULL));
            NecroCoreAst*       new_let    = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_let(context->arena, new_bind, alt->case_alt.expr));
            return new_let;
        }
    }
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, ast);
}

NecroCoreAst* necro_core_ast_pre_simplify_bind(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    ast->bind.ast_symbol->type = necro_type_inline_wrapper_types(context->arena, context->base, ast->bind.ast_symbol->type);
    NecroCoreAst* expr         = necro_core_ast_pre_simplify_go(context, ast->bind.expr);
    NecroCoreAst* initializer  = necro_core_ast_pre_simplify_go(context, ast->bind.initializer);
    if (expr == ast->bind.expr && initializer == ast->bind.initializer)
        return ast;
    NecroCoreAst* new_ast = necro_core_ast_create_bind(context->arena, ast->bind.ast_symbol, expr, initializer);
    new_ast->necro_type   = expr->necro_type;
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, new_ast);
}

NecroCoreAst* necro_core_ast_pre_simplify_lam(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    NecroCoreAst* arg  = necro_core_ast_pre_simplify_go(context, ast->lambda.arg);
    NecroCoreAst* expr = necro_core_ast_pre_simplify_go(context, ast->lambda.expr);
    if (arg == ast->lambda.arg && expr == ast->lambda.expr)
        return ast;
    NecroCoreAst* new_ast = necro_core_ast_create_lam(context->arena, arg, expr);
    new_ast->necro_type   = NULL; // TODO?
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, new_ast);
}

NecroCoreAst* necro_core_ast_pre_simplify_for(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_FOR);
    NecroCoreAst* range_init = necro_core_ast_pre_simplify_go(context, ast->for_loop.range_init);
    NecroCoreAst* value_init = necro_core_ast_pre_simplify_go(context, ast->for_loop.value_init);
    NecroCoreAst* index_arg  = necro_core_ast_pre_simplify_go(context, ast->for_loop.index_arg);
    NecroCoreAst* value_arg  = necro_core_ast_pre_simplify_go(context, ast->for_loop.value_arg);
    NecroCoreAst* expression = necro_core_ast_pre_simplify_go(context, ast->for_loop.expression);
    if (range_init == ast->for_loop.range_init &&
        value_init == ast->for_loop.value_init &&
        index_arg  == ast->for_loop.index_arg  &&
        value_arg  == ast->for_loop.value_arg  &&
        expression == ast->for_loop.expression)
        return ast;
    NecroCoreAst* new_ast = necro_core_ast_create_for_loop(context->arena, ast->for_loop.max_loops, range_init, value_init, index_arg, value_arg, expression);
    new_ast->necro_type   = expression->necro_type;
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, new_ast);
}

// TODO / NOTE: Need global let crawling function
NecroCoreAst* necro_core_ast_pre_simplify_let(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LET);
    if (ast->let.bind->ast_type == NECRO_CORE_AST_DATA_DECL && ast->let.bind->data_decl.ast_symbol->is_wrapper)
    {
        return necro_core_ast_pre_simplify_go(context, ast->let.expr);
    }
    NecroCoreAst* bind = necro_core_ast_pre_simplify_go(context, ast->let.bind);
    NecroCoreAst* expr = necro_core_ast_pre_simplify_go(context, ast->let.expr);
    if (bind == ast->let.bind && expr == ast->let.expr)
        return ast;
    NecroCoreAst* new_ast = necro_core_ast_create_let(context->arena, bind, expr);
    new_ast->necro_type   = expr == NULL ? NULL : expr->necro_type;
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, new_ast);
}

void necro_core_ast_pre_simplify_data_con(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_DATA_CON);
    assert(!ast->data_decl.ast_symbol->is_wrapper);
    ast->data_con.ast_symbol->type = necro_type_inline_wrapper_types(context->arena, context->base, ast->data_con.ast_symbol->type);
    ast->data_con.type             = necro_type_inline_wrapper_types(context->arena, context->base, ast->data_con.type);
}

NecroCoreAst* necro_core_ast_pre_simplify_data_decl(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_DATA_DECL);
    assert(!ast->data_decl.ast_symbol->is_wrapper);
    NecroCoreAstList* data_cons = ast->data_decl.con_list;
    while (data_cons != NULL)
    {
        necro_core_ast_pre_simplify_data_con(context, data_cons->data);
        data_cons = data_cons->next;
    }
    return ast;
}

// TODO: Faster exit somehow?
// Rewrite: \x -> x ==> x
NecroCoreAst* necro_core_ast_inline_id(NecroCorePreSimplify* context, NecroCoreAst* ast, NecroCoreAst* fn, NecroCoreAst* param)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    if (fn->ast_type == NECRO_CORE_AST_VAR)
    {
        if (fn->var.ast_symbol->is_constructor)
            return ast;
        fn = fn->var.ast_symbol->ast;
        if (fn == NULL || fn->ast_type != NECRO_CORE_AST_BIND)
            return ast;
        fn = fn->bind.expr;
    }
    if (fn->ast_type != NECRO_CORE_AST_LAM)
        return ast;
    // Match: \x -> x
    NecroCoreAst* arg  = fn->lambda.arg;
    NecroCoreAst* expr = fn->lambda.expr;
    if (expr->ast_type != NECRO_CORE_AST_VAR || expr->var.ast_symbol != arg->var.ast_symbol)
        return ast;
    return param;
}

// Rewrite: \x -> \f -> f x ==> f x
NecroCoreAst* necro_core_ast_inline_pipe_forward(NecroCorePreSimplify* context, NecroCoreAst* ast, NecroCoreAst* fn, NecroCoreAst* left, NecroCoreAst* right)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    if (fn->ast_type == NECRO_CORE_AST_VAR)
    {
        fn = fn->var.ast_symbol->ast;
        if (fn->ast_type != NECRO_CORE_AST_BIND)
            return ast;
        fn = fn->bind.expr;
    }
    if (fn->ast_type != NECRO_CORE_AST_LAM)
        return ast;
    NecroCoreAst* arg1 = fn->lambda.arg;
    NecroCoreAst* fn2  = fn->lambda.expr;
    if (fn2->ast_type != NECRO_CORE_AST_LAM)
        return ast;
    NecroCoreAst* arg2 = fn2->lambda.arg;
    NecroCoreAst* expr = fn2->lambda.expr;
    if (expr->ast_type != NECRO_CORE_AST_APP)
        return ast;
    NecroCoreAst* lam_left  = expr->app.expr1;
    NecroCoreAst* lam_right = expr->app.expr2;
    if (lam_left->ast_type != NECRO_CORE_AST_VAR || lam_right->ast_type != NECRO_CORE_AST_VAR || lam_left->var.ast_symbol != arg2->var.ast_symbol || lam_right->var.ast_symbol != arg1->var.ast_symbol)
        return ast;
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_app(context->arena, right, left));
}

// Rewrite: \f -> \x -> f x ==> f x
NecroCoreAst* necro_core_ast_inline_pipe_back(NecroCorePreSimplify* context, NecroCoreAst* ast, NecroCoreAst* fn, NecroCoreAst* left, NecroCoreAst* right)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    if (fn->ast_type == NECRO_CORE_AST_VAR)
    {
        fn = fn->var.ast_symbol->ast;
        if (fn->ast_type != NECRO_CORE_AST_BIND)
            return ast;
        fn = fn->bind.expr;
    }
    if (fn->ast_type != NECRO_CORE_AST_LAM)
        return ast;
    NecroCoreAst* arg1 = fn->lambda.arg;
    NecroCoreAst* fn2  = fn->lambda.expr;
    if (fn2->ast_type != NECRO_CORE_AST_LAM)
        return ast;
    NecroCoreAst* arg2 = fn2->lambda.arg;
    NecroCoreAst* expr = fn2->lambda.expr;
    if (expr->ast_type != NECRO_CORE_AST_APP)
        return ast;
    NecroCoreAst* lam_left  = expr->app.expr1;
    NecroCoreAst* lam_right = expr->app.expr2;
    if (lam_left->ast_type != NECRO_CORE_AST_VAR || lam_right->ast_type != NECRO_CORE_AST_VAR || lam_left->var.ast_symbol != arg1->var.ast_symbol || lam_right->var.ast_symbol != arg2->var.ast_symbol)
        return ast;
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_app(context->arena, left, right));
}

NecroCoreAst* necro_ast_inline_app_lambda(NecroCorePreSimplify* context, NecroCoreAst* ast, NecroCoreAst* fn, NecroCoreAst* param)
{
    if (fn->ast_type != NECRO_CORE_AST_LAM)
        return ast;
    NecroCoreAst*       lam_arg         = fn->lambda.arg;
    NecroCoreAstSymbol* new_symbol      = necro_core_ast_symbol_create_by_renaming(context->arena, necro_intern_unique_string(context->intern, lam_arg->var.ast_symbol->name->str), lam_arg->var.ast_symbol);
    NecroCoreAst*       new_var         = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_var(context->arena, new_symbol, lam_arg->necro_type));
    lam_arg->var.ast_symbol->inline_ast = new_var;
    NecroCoreAst*       new_bind        = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_bind(context->arena, new_symbol, param, NULL));
    NecroCoreAst*       new_let         = necro_core_ast_pre_simplify_inline_wrapper_types(context, necro_core_ast_create_let(context->arena, new_bind, fn->lambda.expr));
    return new_let;
}

NecroCoreAst* necro_core_ast_pre_simplify_app(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);

    // Unwrap fully applied wrapper types
    if (ast->app.expr1->ast_type == NECRO_CORE_AST_VAR && ast->app.expr1->var.ast_symbol->is_constructor && ast->app.expr1->var.ast_symbol->is_wrapper)
    {
        return ast->app.expr2;
    }

    // rewrite uops (id, lambda, etc...)
    else if (ast->app.expr1->ast_type == NECRO_CORE_AST_VAR || ast->app.expr1->ast_type == NECRO_CORE_AST_LAM)
    {
        NecroCoreAst* fn      = ast->app.expr1;
        NecroCoreAst* param   = ast->app.expr2;
        NecroCoreAst* inlined = necro_core_ast_inline_id(context, ast, fn, param);
        if (inlined != ast) return inlined;
        inlined               = necro_ast_inline_app_lambda(context, ast, fn, param);
        if (inlined != ast) return inlined;
    }

    // rewrite binops (|>, <|, >>, <<, etc...)
    else if (ast->app.expr1->ast_type == NECRO_CORE_AST_APP && (ast->app.expr1->app.expr1->ast_type == NECRO_CORE_AST_VAR || ast->app.expr1->app.expr1->ast_type == NECRO_CORE_AST_LAM))
    {
        NecroCoreAst* fn      = ast->app.expr1->app.expr1;
        NecroCoreAst* left    = ast->app.expr1->app.expr2;
        NecroCoreAst* right   = ast->app.expr2;
        NecroCoreAst* inlined = necro_core_ast_inline_pipe_forward(context, ast, fn, left, right);
        if (inlined != ast) return inlined;
        inlined               = necro_core_ast_inline_pipe_back(context, ast, fn, left, right);
        if (inlined != ast) return inlined;
    }

    // Go Deeper
    NecroCoreAst* expr1 = necro_core_ast_pre_simplify_go(context, ast->app.expr1);
    NecroCoreAst* expr2 = necro_core_ast_pre_simplify_go(context, ast->app.expr2);
    if (expr1 == ast->app.expr1 && expr2 == ast->app.expr2)
        return ast;
    NecroCoreAst* new_ast = necro_core_ast_create_app(context->arena, expr1, expr2);
    new_ast->necro_type   = NULL; // TODO?
    return necro_core_ast_pre_simplify_inline_wrapper_types(context, new_ast);
}

NecroCoreAst* necro_core_ast_pre_simplify_go(NecroCorePreSimplify* context, NecroCoreAst* ast)
{
    if (ast == NULL)
        return NULL;
    // Keeps simplifying until we hit identity
    NecroCoreAst* prev_ast = NULL;
    while (ast != prev_ast)
    {
        prev_ast = ast;
        switch (ast->ast_type)
        {
        case NECRO_CORE_AST_VAR:       ast = necro_core_ast_pre_simplify_var(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));       break;
        case NECRO_CORE_AST_LIT:       ast = necro_core_ast_pre_simplify_lit(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));       break;
        case NECRO_CORE_AST_LET:       ast = necro_core_ast_pre_simplify_let(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));       break;
        case NECRO_CORE_AST_FOR:       ast = necro_core_ast_pre_simplify_for(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));       break;
        case NECRO_CORE_AST_LAM:       ast = necro_core_ast_pre_simplify_lam(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));       break;
        case NECRO_CORE_AST_APP:       ast = necro_core_ast_pre_simplify_app(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));       break;
        case NECRO_CORE_AST_CASE:      ast = necro_core_ast_pre_simplify_case(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));      break;
        case NECRO_CORE_AST_BIND:      ast = necro_core_ast_pre_simplify_bind(context, necro_core_ast_pre_simplify_inline_wrapper_types(context, prev_ast));      break;
        case NECRO_CORE_AST_DATA_DECL: ast = necro_core_ast_pre_simplify_data_decl(context, prev_ast); break;
        case NECRO_CORE_AST_DATA_CON:  break;
            // case NECRO_CORE_AST_BIND_REC:
        default:
            assert(false && "Unimplemented Ast in necro_core_ast_pre_simplify");
            return NULL;
        }
    }
    return ast;
}

NecroType* necro_type_inline_wrapper_types(NecroPagedArena* arena, NecroBase* base, NecroType* type)
{
    type = necro_type_find(type);
    if (type == NULL)
        return NULL;
    switch (type->type)
    {
    case NECRO_TYPE_VAR: return type;
    case NECRO_TYPE_CON:
    {
        if (!type->con.con_symbol->is_primitive && type->con.con_symbol->is_wrapper)
        {
            NecroCoreAst* data_decl = type->con.con_symbol->core_ast_symbol->ast;
            assert(data_decl->ast_type == NECRO_CORE_AST_DATA_DECL);
            NecroCoreAst* data_con  = data_decl->data_decl.con_list->data;
            assert(data_con->ast_type == NECRO_CORE_AST_DATA_CON);
            NecroType*    data_con_type = data_con->data_con.type;
            NecroType* data_con_inst = unwrap_result(NecroType, necro_type_instantiate(arena, NULL, base, data_con_type, NULL));
            NecroType* arg_type      = necro_type_fresh_var(arena, NULL);
            NecroType* this_con      = necro_type_fn_create(arena, arg_type, type);
            unwrap(NecroType, necro_kind_infer(arena, base, this_con, NULL_LOC, NULL_LOC));
            unwrap(NecroType, necro_type_unify(arena, NULL, base, data_con_inst, this_con, NULL));
            return necro_type_deep_copy(arena, necro_type_find(arg_type));
        }
        NecroType* args = necro_type_inline_wrapper_types(arena, base, type->con.args);
        if (args == type->con.args)
            return type;
        NecroType* new_type = necro_type_con_create(arena, type->con.con_symbol, args);
        new_type->kind      = type->kind;
        new_type->ownership = type->ownership;
        return new_type;
    }
    case NECRO_TYPE_FUN:
    {
        NecroType* type1 = necro_type_inline_wrapper_types(arena, base, type->fun.type1);
        NecroType* type2 = necro_type_inline_wrapper_types(arena, base, type->fun.type2);
        if (type1 == type->fun.type1 && type2 == type->fun.type2)
            return type;
        NecroType* new_type = necro_type_fn_create(arena, type1, type2);
        new_type->kind      = type->kind;
        new_type->ownership = type->ownership;
        return new_type;
    }
    case NECRO_TYPE_APP:
    {
        NecroType* type1 = necro_type_inline_wrapper_types(arena, base, type->app.type1);
        NecroType* type2 = necro_type_inline_wrapper_types(arena, base, type->app.type2);
        if (type1 == type->app.type1 && type2 == type->app.type2)
            return type;
        NecroType* new_type = necro_type_app_create(arena, type1, type2);
        new_type->kind      = type->kind;
        new_type->ownership = type->ownership;
        return new_type;
    }
    case NECRO_TYPE_LIST:
    {
        NecroType* type1 = necro_type_inline_wrapper_types(arena, base, type->list.item);
        NecroType* type2 = necro_type_inline_wrapper_types(arena, base, type->list.next);
        if (type1 == type->list.item && type2 == type->list.next)
            return type;
        NecroType* new_type = necro_type_list_create(arena, type1, type2);
        new_type->kind      = type->kind;
        new_type->ownership = type->ownership;
        return new_type;
    }
    case NECRO_TYPE_FOR:
    {
        NecroType* inner_type = necro_type_inline_wrapper_types(arena, base, type->for_all.type);
        if (inner_type == type->for_all.type)
            return type;
        NecroType* new_type = necro_type_for_all_create(arena, type->for_all.var_symbol, inner_type);
        new_type->kind      = type->kind;
        new_type->ownership = type->ownership;
        return new_type;
    }
    case NECRO_TYPE_NAT:
    case NECRO_TYPE_SYM:
        return type;
    default:
        assert(false);
        return NULL;
    }
}

void necro_core_ast_pre_simplify(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    NecroCorePreSimplify context = (NecroCorePreSimplify) { .arena = &core_ast_arena->arena, .intern = intern, .base = base };
    core_ast_arena->root = necro_core_ast_pre_simplify_go(&context, core_ast_arena->root);
    unwrap(void, necro_core_infer(intern, base, core_ast_arena));
    if (info.compilation_phase == NECRO_PHASE_TRANSFORM_TO_MACHINE && info.verbosity > 0)
    {
        necro_core_ast_pretty_print(core_ast_arena->root);
    }
}


///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_CORE_AST_PRE_SIMPLIFY_VERBOSE 1
void necro_core_ast_pre_simplfy_test(const char* test_name, const char* str)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCoreAstArena   core_ast        = necro_core_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast));
    necro_core_ast_pre_simplify(info, &intern, &base, &core_ast);

    // Print
#if NECRO_CORE_AST_PRE_SIMPLIFY_VERBOSE
    printf("\n");
    necro_core_ast_pretty_print(core_ast.root);
#endif
    printf("Core %s test: Passed\n", test_name);
    fflush(stdout);

    // Clean up
    necro_core_ast_arena_destroy(&core_ast);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}


void necro_core_ast_pre_simplify_test()
{
    necro_announce_phase("Pre-Simplify");

/*

    {
        const char* test_name   = "Identity 1";
        const char* test_source = ""
            "x = True\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline |> 1";
        const char* test_source = ""
            "x = True |> id\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline |> 2";
        const char* test_source = ""
            "x = True |> id |> id\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline |> 3";
        const char* test_source = ""
            "x :: Int\n"
            "x = 0 |> add 1\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline |> 4";
        const char* test_source = ""
            "x :: Int\n"
            "x = 0 |> add 1 |> mul 4\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline <| 1";
        const char* test_source = ""
            "x :: Int\n"
            "x = add 1 <| 2\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline <| 2";
        const char* test_source = ""
            "x :: Int\n"
            "x = sub 5 <| add 1 <| 2\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline <| 3";
        const char* test_source = ""
            "x :: Int\n"
            "x = id <| sub 5 <| id <| add 1 <| 2\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline <| 4";
        const char* test_source = ""
            "x :: Maybe Int\n"
            "x = Just <| id <| sub 5 <| id <| add 1 <| 2\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline <| 5";
        const char* test_source = ""
            "x :: Maybe Int\n"
            "x = Just <| add (mul 2 <| 3) <| 4 \n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline 1";
        const char* test_source = ""
            "myPipe f x = f x\n"
            "x :: Maybe Int\n"
            "x = myPipe Just 0\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline 2";
        const char* test_source = ""
            "myPipe z w = w z\n"
            "x :: Maybe Int\n"
            "x = myPipe 0 Just\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline 3";
        const char* test_source = ""
            "myId y = y\n"
            "x :: Int\n"
            "x = myId 0\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline Lambda 1";
        const char* test_source = ""
            "x :: Int\n"
            "x = (+ 1) 2\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline Lambda 2";
        const char* test_source = ""
            "x :: Int\n"
            "x = 2 |> (333 -)\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline Lambda 3";
        const char* test_source = ""
            "x :: Int\n"
            "x = 2 |> (333 -) |> (+ 666) |> id\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline Lambda 4";
        const char* test_source = ""
            "x :: Int\n"
            "x = 2 |> (\\x -> x * x)\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Inline Lambda 5";
        const char* test_source = ""
            "x :: Int -> Int\n"
            "x = 2 |> (\\x y -> x * y)\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 1";
        const char* test_source = ""
            "data Wrapper a = Wrapper a\n"
            "x :: Wrapper Int\n"
            "x = Wrapper 3\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 2";
        const char* test_source = ""
            "data Wrapper a = Wrapper a\n"
            "x :: Wrapper Int\n"
            "x = 3 |> Wrapper\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 3";
        const char* test_source = ""
            "data MonoWrapper = MonoWrapper (#Int, Bool#)\n"
            "x :: MonoWrapper\n"
            "x = MonoWrapper (#0, True#)\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 4";
        const char* test_source = ""
            "data RevTuple a b c = RevTuple (c, b, a)\n"
            "x :: RevTuple Int Bool Float\n"
            "x = RevTuple (3, False, 1)\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 5";
        const char* test_source = ""
            "data PhantomLancer p = PhantomLancer ()\n"
            "x :: PhantomLancer Bool\n"
            "x = PhantomLancer ()\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 6";
        const char* test_source = ""
            "data Wrapper a = Wrapper a\n"
            "x :: Wrapper (Bool -> Bool)\n"
            "x = Wrapper id\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 7";
        const char* test_source = ""
            "data Wrapper a = Wrapper a\n"
            "x :: Wrapper (Bool -> Wrapper Bool)\n"
            "x = Wrapper Wrapper\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 8";
        const char* test_source = ""
            "data Wrapper a = Wrapper a\n"
            "x :: Wrapper (Wrapper (Wrapper Int), Wrapper Bool)\n"
            "x = Wrapper (Wrapper (Wrapper 333), Wrapper False)\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 9";
        const char* test_source = ""
            "data Time'   = Time' Float\n"
            "data SVal' a = SVal' a | SEnd'\n"
            "data Seq'  a = Seq' (Time' -> SVal' a)\n"
            "x :: Seq' Bool\n"
            "x = Seq' go\n"
            "  where\n"
            "    go t = SVal' True\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "This will likely fail";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = w |> printInt 0 |> printInt 1 |> printInt 2\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap 10";
        const char* test_source = ""
            "data Time'   = Time' Float\n"
            "data SVal' a = SVal' Time' a | SEnd'\n"
            "data Seq'  a = Seq' (Time' -> SVal' a)\n"
            "x :: Seq' Bool\n"
            "x = Seq' <| \\t -> SVal' t True\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap Case 1";
        const char* test_source = ""
            "x :: Channel\n"
            "x = case Mono (BlockRate 440) of\n"
            "  Mono c -> c\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Mono Test";
        const char* test_source = ""
            "instance Audio Mono where\n"
            "  pureAudio c = Mono c\n"
            "instance Audio Stereo where\n"
            "  pureAudio c = Stereo (#c, c#)\n"
            "coolMono :: Mono\n"
            "coolMono = pureAudio (BlockRate 1.0)\n"
            "coolStereo :: Stereo\n"
            "coolStereo = pureAudio (BlockRate 1.0)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    {
        const char* test_name   = "Unwrap Case 2";
        const char* test_source = ""
            "stereo' :: Mono -> Mono -> Stereo\n"
            "stereo' ml mr =\n"
            "  case ml of\n"
            "    Mono l -> case mr of\n"
            "      Mono r -> Stereo (#l, r#)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

*/

    {
        const char* test_name   = "Simplify Case 1";
        const char* test_source = ""
            "goAway :: Int\n"
            "goAway =\n"
            "  case 666 of\n"
            "    i -> i\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

/*

    // TODO: Arrays
    // TODO: Case

    // TODO: Finish!
    // {
    //     const char* test_name   = "Unwrap 11";
    //     const char* test_source = ""
    //         "data Time' = Time' Float\n"
    //         "instance Num Time' where\n";
    //     necro_core_ast_pre_simplfy_test(test_name, test_source);
    // }

    {
        const char* test_name   = "Unwrap 12";
        const char* test_source = ""
            // "data Time'   = Time' Float\n"
            "data SVal' a = SVal' Float a | SEnd'\n"
            "data Seq'  a = Seq' (Float -> SVal' a)\n"
            "x :: Seq' Bool\n"
            "x = Seq' <| \\t -> SVal' t True\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

    // NOTE: This should parse and its not???
    {
        const char* test_name   = "Inline lambda 1";
        const char* test_source = ""
            "x :: Maybe Int\n"
            "x = (\ff xx -> ff xx) Just 0\n";
        necro_core_ast_pre_simplfy_test(test_name, test_source);
    }

*/

}

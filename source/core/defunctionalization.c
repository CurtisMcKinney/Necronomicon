/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "defunctionalization.h"
#include <stdio.h>
#include "type.h"
#include "monomorphize.h"
#include "core_infer.h"
#include "lambda_lift.h"
#include "alias_analysis.h"
#include "kind.h"

/*
    - Largely based on "High Performance Defunctionalisation in Futhark": https://futhark-lang.org/publications/tfp18.pdf
    - Some obvious differences are that we support branching and arrays, and we treat functions as a whole instead of lambda piecemeal.
    - If we could obviate the need for using lifted type in recursive value check we could nix lifted types entirely.

    TODO
        - Refactor Env to dynamically generate arities as required!
        - transform: MyCoolIntFn (Int -> Int) ==> MyCoolIntFn_aa1 a
        - inline all HOF: myCoolHOF :: (Int -> Int) -> Int ==> INLINED with (Int -> Int) ==> CoolFnEnv_aa2

        - Handle partial application: Create an Env data type which captures partially applied variables, create a fn which takes Env and arguments, then applies original fn with all given values. (This fn has to be hoisted to top...)

        - Handle Functions inside of Product Types and Sum Types
        - Handle Functions returned at Branches: data _BranchFn2 a b, data _BranchFn3 a b c, etc..
        - Handle Functions inside of Product Types and Sum Types which are returned by Branches...
        - Handle Functions inside of Arrays: Create a new env sum type which contains a member for each Env inserted into that array

        - At End, Prune:
            * MyCoolIntFn (Int -> Int)
            * myCoolHOF :: (Int -> Int) -> Int

        - Must still completely type check with necro_core_infer at the end of transformation with no black magic requried!
*/

struct NecroStaticValue;
struct NecroStaticValueList;

typedef enum
{
    NECRO_STATIC_VALUE_DYN,
    NECRO_STATIC_VALUE_ENV,
    NECRO_STATIC_VALUE_FUN,
    NECRO_STATIC_VALUE_CON,
    NECRO_STATIC_VALUE_SUM,

    // Maybe require?
    // NECRO_STATIC_VALUE_ARR_SUM
    // NECRO_STATIC_VALUE_BRN_SUM

} NECRO_STATIC_VALUE_TYPE;

typedef struct
{
    NecroType* type;
} NecroStaticValueDynamic;

typedef struct
{
    NecroType*                   fn_type;
    NecroCoreAstSymbol*          env_con_symbol;
    NecroCoreAstSymbol*          fn_symbol;
    struct NecroStaticValue*     expr_static_value;
    struct NecroStaticValueList* arg_static_values;
} NecroStaticValueEnv;

typedef struct
{
    NecroCoreAstSymbol*      fn_symbol;
    struct NecroStaticValue* expr_static_value;
} NecroStaticValueFunction;

typedef struct
{
    NecroCoreAstSymbol*          con_symbol;
    NecroType*                   con_fn_type;
    struct NecroStaticValueList* args;
} NecroStaticValueConstructor;

typedef struct
{
    NecroType*                   type;    // The Type of the Data Type
    struct NecroStaticValueList* options; // Constructors containing further types.
} NecroStaticValueSum;

typedef struct NecroStaticValue
{
    union
    {
        NecroStaticValueDynamic     dyn;
        NecroStaticValueEnv         env;
        NecroStaticValueFunction    fun;
        NecroStaticValueConstructor con;
        NecroStaticValueSum         sum;
    };
    NECRO_STATIC_VALUE_TYPE type;
    NecroType*              necro_type;
} NecroStaticValue;

NECRO_DECLARE_ARENA_LIST(struct NecroStaticValue*, StaticValue, static_value);

typedef struct
{
    NecroPagedArena* arena;
    NecroIntern*     intern;
    NecroBase*       base;
} NecroDefunctionalizeContext;

NecroDefunctionalizeContext necro_defunctionalize_context_create(NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    return (NecroDefunctionalizeContext)
    {
        .arena  = &core_ast_arena->arena,
        .intern = intern,
        .base   = base,
    };
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroStaticValue* necro_defunctionalize_go(NecroDefunctionalizeContext* context, NecroCoreAst* ast);

///////////////////////////////////////////////////////
// Static Values
///////////////////////////////////////////////////////
NecroStaticValue* necro_static_value_alloc(NecroPagedArena* arena, NECRO_STATIC_VALUE_TYPE type, NecroType* necro_type)
{
    NecroStaticValue* static_value = necro_paged_arena_alloc(arena, sizeof(NecroStaticValue));
    static_value->type             = type;
    static_value->necro_type       = necro_type;
    return static_value;
}

NecroStaticValue* necro_static_value_create_dyn(NecroPagedArena* arena, NecroType* type)
{
    NecroStaticValue* static_value = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_DYN, type);
    static_value->dyn.type         = type;
    return static_value;
}

NecroStaticValue* necro_static_value_create_env(NecroPagedArena* arena, NecroType* env_type, NecroType* fn_type, NecroCoreAstSymbol* env_con_symbol, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* expr_static_value, NecroStaticValueList* arg_static_values)
{
    NecroStaticValue* static_value      = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_ENV, env_type);
    static_value->env.fn_type           = fn_type;
    static_value->env.env_con_symbol    = env_con_symbol;
    static_value->env.fn_symbol         = fn_symbol;
    static_value->env.expr_static_value = expr_static_value;
    static_value->env.arg_static_values = arg_static_values;
    return static_value;
}

NecroStaticValue* necro_static_value_create_fun(NecroPagedArena* arena, NecroType* necro_type, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* expr_static_value)
{
    NecroStaticValue* static_value      = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_FUN, necro_type);
    static_value->fun.fn_symbol         = fn_symbol;
    static_value->fun.expr_static_value = expr_static_value;
    return static_value;
}

NecroStaticValue* necro_static_value_create_con(NecroPagedArena* arena, NecroType* necro_type, NecroCoreAstSymbol* con_symbol, NecroType* con_fn_type, NecroStaticValueList* args)
{
    NecroStaticValue* static_value = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_CON, necro_type);
    static_value->con.con_symbol   = con_symbol;
    static_value->con.con_fn_type  = con_fn_type;
    static_value->con.args         = args;
    return static_value;
}

// TODO: Handle ownership!
// Note: This calls defunctionalize_go on all the arguments in the apps....is that a good idea!?!?!
NecroStaticValue* necro_static_value_create_env_from_expr(NecroDefunctionalizeContext* context, NecroCoreAst* ast, NecroType* a_fn_type, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* fn_expr_static_value)
{
    size_t        app_count = 0;
    NecroCoreAst* var_ast   = ast;
    while (var_ast->ast_type == NECRO_CORE_AST_APP)
    {
        app_count++;
        var_ast = var_ast->app.expr1;
    }
    NecroType*            fn_type           = a_fn_type;
    NecroAstSymbol*       env_type_symbol   = necro_base_get_env_type(context->base, app_count);
    NecroCoreAstSymbol*   env_con_symbol    = necro_base_get_env_con(context->base, app_count)->core_ast_symbol;
    NecroType*            env_type          = NULL;
    NecroCoreAst*         apps              = ast;
    NecroStaticValueList* arg_static_values = NULL;
    while (apps->ast_type == NECRO_CORE_AST_APP)
    {
        assert(fn_type->type == NECRO_TYPE_FUN);
        NecroStaticValue* arg_static_value = necro_defunctionalize_go(context, apps->app.expr2);
        env_type                           = necro_type_list_create(context->arena, arg_static_value->necro_type, env_type);
        arg_static_values                  = necro_cons_static_value_list(context->arena, arg_static_value, arg_static_values);
        fn_type = fn_type->fun.type2;
        apps    = apps->app.expr1;
    }
    env_type                = necro_type_con_create(context->arena, env_type_symbol, env_type);
    unwrap(void, necro_kind_infer_default_unify_with_star(context->arena, context->base, env_type, NULL, zero_loc, zero_loc));
    NecroType* env_mono_con = unwrap_result(NecroType, necro_type_instantiate(context->arena, NULL, context->base, env_con_symbol->type, NULL));
    *var_ast                = *necro_core_ast_create_var(context->arena, env_con_symbol, env_mono_con);
    return necro_static_value_create_env(context->arena, env_type, a_fn_type, env_con_symbol, fn_symbol, fn_expr_static_value, arg_static_values);
}

///////////////////////////////////////////////////////
// Defunctionalize
///////////////////////////////////////////////////////

NecroStaticValue* necro_defunctionalize_var(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    // // Primitives
    // if (ast->var.ast_symbol->is_primitive)
    // {
    //     if (ast->necro_type->type == NECRO_TYPE_FUN)
    //         return necro_static_value_create_fun(context->arena, ast->necro_type, ast->var.ast_symbol->static_value->fun.fn_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(ast->necro_type)));
    //     else
    //         return necro_static_value_create_dyn(context->arena, ast->necro_type);
    // }
    // _primUndefined
    if (ast->var.ast_symbol == context->base->prim_undefined->core_ast_symbol)
    {
        return necro_static_value_create_dyn(context->arena, ast->necro_type);
    }
    // Inlined var
    else if (ast->var.ast_symbol->inline_ast != NULL)
    {
        // deep_copy into
        *ast = *necro_core_ast_deep_copy(context->arena, ast->var.ast_symbol->inline_ast);
        return necro_defunctionalize_go(context, ast);
    }
    // Constructors
    // else if (ast->var.ast_symbol->static_value->necro_type->type == NECRO_TYPE_FOR ||
    //         (ast->var.ast_symbol->static_value->necro_type->ownership != NULL && (ast->var.ast_symbol->static_value->necro_type->ownership->type == NECRO_TYPE_FOR || ast->var.ast_symbol->static_value->necro_type->ownership->type == NECRO_TYPE_VAR)))
    else if (ast->var.ast_symbol->is_constructor)
    {
        // NOTE: The only polymorphic functions which should make it into here are data constructor functions!
        assert(ast->necro_type != NULL);
        assert(ast->necro_type->type != NECRO_TYPE_FOR);
        if (ast->necro_type->type == NECRO_TYPE_FUN)
            return necro_static_value_create_fun(context->arena, ast->necro_type, ast->var.ast_symbol->static_value->fun.fn_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(ast->necro_type)));
        else
            return necro_static_value_create_dyn(context->arena, ast->necro_type);
    }
    assert(ast->var.ast_symbol->static_value != NULL);
    return ast->var.ast_symbol->static_value;
}

// TODO: Handle static values from data_cons, which can change DataTypes which hold Functions
NecroStaticValue* necro_defunctionalize_data_con(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_DATA_CON);
    NecroType* con_type = ast->data_con.type;
    while (con_type->type == NECRO_TYPE_FOR)
        con_type = con_type->for_all.type;
    while (con_type->type == NECRO_TYPE_FUN)
    {
        ast->data_con.ast_symbol->arity++;
        con_type = con_type->fun.type2;
    }
    if (ast->data_con.ast_symbol->arity > 0)
        ast->data_con.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->data_con.ast_symbol->type, ast->data_con.ast_symbol, necro_static_value_create_dyn(context->arena, ast->data_con.data_type_type));
    else
        ast->data_con.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, ast->data_con.data_type_type);
    return ast->data_con.ast_symbol->static_value;
}

NecroStaticValue* necro_defunctionalize_data_decl(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_DATA_DECL);
    NecroCoreAstList* data_cons = ast->data_decl.con_list;
    while (data_cons != NULL)
    {
        // TODO: Handle static values from data_cons, which can change DataTypes which hold Functions
        necro_defunctionalize_data_con(context, data_cons->data);
        data_cons = data_cons->next;
    }
    return NULL;
}

NecroStaticValue* necro_defunctionalize_lit(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LIT);
    switch (ast->lit.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:   return necro_static_value_create_dyn(context->arena, context->base->float_type->type);
    case NECRO_AST_CONSTANT_INTEGER:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN: return necro_static_value_create_dyn(context->arena, context->base->int_type->type);
    case NECRO_AST_CONSTANT_CHAR:
    case NECRO_AST_CONSTANT_CHAR_PATTERN:    return necro_static_value_create_dyn(context->arena, context->base->char_type->type);
    case NECRO_AST_CONSTANT_STRING:
    {
        // TODO: Better string type handling
        NecroType* arity_type = necro_type_nat_create(context->arena, strlen(ast->lit.string_literal->str) + 1);
        arity_type->kind      = context->base->nat_kind->type;
        NecroType* array_type = necro_type_con2_create(context->arena, context->base->array_type, arity_type, context->base->char_type->type);
        return necro_static_value_create_dyn(context->arena, array_type);
    }
    case NECRO_AST_CONSTANT_ARRAY:
        // TODO: Finish
        assert(false);
        return NULL;
    case NECRO_AST_CONSTANT_TYPE_INT:
    default:
        assert(false);
        return NULL;
    }
}

NecroStaticValue* necro_defunctionalize_for(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_FOR);
    NecroStaticValue* range_init_static_value = necro_defunctionalize_go(context, ast->for_loop.range_init);
    NecroStaticValue* value_init_static_value = necro_defunctionalize_go(context, ast->for_loop.value_init);
    NecroStaticValue* index_arg_static_value  = necro_defunctionalize_go(context, ast->for_loop.index_arg);
    NecroStaticValue* value_arg_static_value  = necro_defunctionalize_go(context, ast->for_loop.value_arg);
    NecroStaticValue* expression_static_value = necro_defunctionalize_go(context, ast->for_loop.expression);
    UNUSED(range_init_static_value);
    UNUSED(value_init_static_value);
    UNUSED(index_arg_static_value);
    UNUSED(value_arg_static_value);
    return expression_static_value;
}

NecroStaticValue* necro_defunctionalize_let(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LET);
    while (ast != NULL)
    {
        // Non-Let Ast
        if (ast->ast_type != NECRO_CORE_AST_LET)
        {
            return necro_defunctionalize_go(context, ast);
        }
        // Inline single var assignments, then move onto Let expr
        else if (ast->let.bind->ast_type == NECRO_CORE_AST_BIND && ast->let.bind->bind.expr->ast_type == NECRO_CORE_AST_VAR && ast->let.bind->bind.expr->var.ast_symbol != context->base->prim_undefined->core_ast_symbol && ast->let.expr != NULL)
        {
            ast->let.bind->bind.ast_symbol->inline_ast = ast->let.bind->bind.expr;
            *ast = *ast->let.expr;
        }
        // Normal Let Ast
        else
        {
            necro_defunctionalize_go(context, ast->let.bind);
            ast = ast->let.expr;
        }
    }
    return NULL;
}

NecroStaticValue* necro_defunctionalize_bind(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    ast->bind.ast_symbol->ast = ast;
    // Calculate arity
    NecroCoreAst* expr = ast->bind.expr;
    while (expr->ast_type == NECRO_CORE_AST_LAM)
    {
        ast->bind.ast_symbol->arity++;
        expr = expr->lambda.expr;
    }

    // Skip Higher order functions
    if (!ast->bind.ast_symbol->is_constructor && necro_type_is_higher_order_function(ast->bind.ast_symbol->type, ast->bind.ast_symbol->arity))
    {
        if (ast->bind.expr->ast_type == NECRO_CORE_AST_LAM)
            ast->bind.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->bind.ast_symbol->type, ast->bind.ast_symbol, NULL);
        else
            ast->bind.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, ast->bind.ast_symbol->type);
        return ast->bind.ast_symbol->static_value;
    }

    // Defunctionalize expr
    NecroStaticValue* static_value = necro_defunctionalize_go(context, ast->bind.expr);
    assert(static_value->necro_type != NULL);
    assert(static_value->type != NECRO_STATIC_VALUE_FUN);
    // Set StaticValue and NecroType
    if (ast->bind.expr->ast_type == NECRO_CORE_AST_LAM)
    {
        if (static_value->type == NECRO_STATIC_VALUE_ENV)
        {
            NecroType* curr_bind_type = ast->bind.ast_symbol->type;
            for (size_t i = 0; i < ast->bind.ast_symbol->arity; ++i)
            {
                assert(curr_bind_type->type == NECRO_TYPE_FUN);
                curr_bind_type = curr_bind_type->fun.type2;
            }
            *curr_bind_type                    = *necro_type_deep_copy(context->arena, static_value->necro_type);
            ast->bind.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->bind.ast_symbol->type, ast->bind.ast_symbol, static_value);
        }
        else
        {
            ast->bind.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->bind.ast_symbol->type, ast->bind.ast_symbol, static_value);
            ast->bind.ast_symbol->type         = ast->bind.ast_symbol->static_value->necro_type;
        }
    }
    else
    {
        ast->bind.ast_symbol->type         = static_value->necro_type;
        ast->bind.ast_symbol->static_value = static_value;
    }
    return static_value;
}

NecroStaticValue* necro_defunctionalize_lam(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    // Use dyn function for HOF which we don't know anything about, and which must be inlined out later on.
    ast->lambda.arg->var.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, ast->lambda.arg->var.ast_symbol->type);
    NecroType* arg_type = ast->lambda.arg->var.ast_symbol->type;
    while (arg_type->type == NECRO_TYPE_FUN)
    {
        ast->lambda.arg->var.ast_symbol->arity++;
        arg_type = arg_type->fun.type2;
    }
    NecroStaticValue* expr_static_value = necro_defunctionalize_go(context, ast->lambda.expr);
    if (expr_static_value->type == NECRO_STATIC_VALUE_FUN)
    {
        assert(ast->lambda.expr->ast_type == NECRO_CORE_AST_VAR);
        // assert(ast->lambda.expr->ast_type == NECRO_CORE_AST_APP || ast->lambda.expr->ast_type == NECRO_CORE_AST_VAR);
        return necro_static_value_create_env_from_expr(context, ast->lambda.expr, expr_static_value->necro_type, expr_static_value->fun.fn_symbol, expr_static_value->fun.expr_static_value);
    }
    return expr_static_value;
}

NecroStaticValue* necro_defunctionalize_app_env(NecroDefunctionalizeContext* context, NecroCoreAst* app_ast, NecroCoreAst* var_ast, NecroStaticValue* fn_static_value, const int32_t app_count)
{
    // Take into account free variables for arity difference / saturation
    int32_t               free_var_count         = 0;
    NecroStaticValueList* free_var_static_values = fn_static_value->env.arg_static_values;
    while (free_var_static_values != NULL)
    {
        free_var_count++;
        free_var_static_values = free_var_static_values->next;
    }
    const int32_t arity           = (int32_t) fn_static_value->env.fn_symbol->arity;
    int32_t       difference      = arity - (app_count + free_var_count);
    // const bool    is_higher_order = necro_type_is_higher_order_function(fn_static_value->env.fn_type, arity);
    // Saturated
    if (difference == 0)
    {
        // if (!fn_static_value->env.fn_symbol->is_constructor && is_higher_order == true)
        // {
        //     assert(false);
        //     return NULL; // TODO: Inline
        // }
        // else
        // {
            // Saturated, First Order, Env
            NecroCoreAst* expr_ast  = necro_core_ast_create_var(context->arena, fn_static_value->env.fn_symbol, fn_static_value->env.fn_type);
            NecroCoreAst* pat_ast   = NULL;
            NecroType*    env_args  = fn_static_value->necro_type->con.args;
            bool          is_env_fn = env_args != NULL;
            // Apply env vars
            if (is_env_fn)
            {
                NecroType* env_mono_con = unwrap_result(NecroType, necro_type_instantiate(context->arena, NULL, context->base, fn_static_value->env.env_con_symbol->type, NULL));
                pat_ast = necro_core_ast_create_var(context->arena, fn_static_value->env.env_con_symbol, env_mono_con);
                while (env_args != NULL)
                {
                    NecroCoreAstSymbol* env_var_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "free_var"), env_args->list.item);
                    env_var_symbol->static_value       = necro_static_value_create_dyn(context->arena, env_args->list.item); // TODO: Might need full arg static values here
                    NecroCoreAst*       env_pat_var    = necro_core_ast_create_var(context->arena, env_var_symbol, env_args->list.item);
                    NecroCoreAst*       env_expr_var   = necro_core_ast_create_var(context->arena, env_var_symbol, env_args->list.item);
                    pat_ast                            = necro_core_ast_create_app(context->arena, pat_ast, env_pat_var);
                    expr_ast                           = necro_core_ast_create_app(context->arena, expr_ast, env_expr_var);
                    env_args                           = env_args->list.next;
                }
            }
            // NOTE: Naive app iterations loads args on backwards
            // Apply App vars
            NecroCoreAst* apps  = app_ast;
            size_t        arg_i = 0;
            while (apps->ast_type == NECRO_CORE_AST_APP)
            {
                // HACK: Lame!
                NecroCoreAst* arg_app = app_ast;
                for (size_t i = arg_i + 1; i < (size_t) app_count; ++i)
                    arg_app = arg_app->app.expr1;
                arg_app  = arg_app->app.expr2;
                expr_ast = necro_core_ast_create_app(context->arena, expr_ast, arg_app);
                // necro_defunctionalize_go(context, apps->app.expr2);
                // expr_ast = necro_core_ast_create_app(context->arena, expr_ast, apps->app.expr2);
                apps     = apps->app.expr1;
                arg_i++;
            }
            NecroStaticValue* expr_sv = necro_defunctionalize_go(context, expr_ast);
            if (is_env_fn)
            {
                // Create case on env
                NecroCoreAst* alt_ast = necro_core_ast_create_case_alt(context->arena, pat_ast, expr_ast);
                *app_ast              = *necro_core_ast_create_case(context->arena, var_ast, necro_cons_core_ast_list(context->arena, alt_ast, NULL));
            }
            else
            {
                *app_ast = *expr_ast;
            }
            // return fn_static_value->env.expr_static_value;
            return expr_sv;
        // }
    }
    // Undersaturated
    else if (difference > 0)
    {
        // Under Saturated, First Order, Env
        NecroCoreAst* expr_ast  = necro_core_ast_create_var(context->arena, fn_static_value->env.fn_symbol, fn_static_value->env.fn_type);
        NecroCoreAst* pat_ast   = NULL;
        NecroType*    env_args  = fn_static_value->necro_type->con.args;
        bool          is_env_fn = env_args != NULL;
        // Apply env vars
        if (is_env_fn)
        {
            NecroType* env_mono_con = unwrap_result(NecroType, necro_type_instantiate(context->arena, NULL, context->base, fn_static_value->env.env_con_symbol->type, NULL));
            pat_ast                 = necro_core_ast_create_var(context->arena, fn_static_value->env.env_con_symbol, env_mono_con);
            while (env_args != NULL)
            {
                NecroCoreAstSymbol* env_var_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "free_var"), env_args->list.item);
                env_var_symbol->static_value       = necro_static_value_create_dyn(context->arena, env_args->list.item);
                NecroCoreAst*       env_pat_var    = necro_core_ast_create_var(context->arena, env_var_symbol, env_args->list.item);
                NecroCoreAst*       env_expr_var   = necro_core_ast_create_var(context->arena, env_var_symbol, env_args->list.item);
                pat_ast                            = necro_core_ast_create_app(context->arena, pat_ast, env_pat_var);
                expr_ast                           = necro_core_ast_create_app(context->arena, expr_ast, env_expr_var);
                env_args                           = env_args->list.next;
            }
        }
        // Apply App vars
        NecroCoreAst* apps = app_ast;
        while (apps->ast_type == NECRO_CORE_AST_APP)
        {
            // necro_defunctionalize_go(context, apps->app.expr2); // defunctionalize_go called by necro_static_value_create_env_from_expr
            expr_ast = necro_core_ast_create_app(context->arena, expr_ast, apps->app.expr2);
            apps     = apps->app.expr1;
        }
        if (is_env_fn)
        {
            // Create case on env
            NecroCoreAst* alt_ast = necro_core_ast_create_case_alt(context->arena, pat_ast, expr_ast);
            *app_ast              = *necro_core_ast_create_case(context->arena, var_ast, necro_cons_core_ast_list(context->arena, alt_ast, NULL));
        }
        else
        {
            *app_ast = *expr_ast;
        }
        return necro_static_value_create_env_from_expr(context, expr_ast, fn_static_value->env.fn_type, fn_static_value->env.fn_symbol, fn_static_value->env.expr_static_value);
    }
    // Oversaturated
    else
    {
        // This works since the oversaturation can never be due to too many free vars in the env struct
        // Transform: f x y z ==> let temp_aa1 = f x in temp_aa1 y z
        NecroCoreAst* ast1 = app_ast;
        while (difference < 0)
        {
            difference++;
            ast1 = ast1->app.expr1;
        }
        // TODO: Correct type here...how!?
        NecroSymbol         temp_name   = necro_intern_unique_string(context->intern, "tmp");
        NecroCoreAstSymbol* temp_symbol = necro_core_ast_symbol_create(context->arena, temp_name, necro_type_fresh_var(context->arena, NULL));
        temp_symbol->type->kind         = context->base->star_kind->type;
        NecroCoreAst*       ast2        = necro_core_ast_create_var(context->arena, temp_symbol, temp_symbol->type);
        necro_core_ast_swap(ast1, ast2);
        // TODO: Handle initializer
        NecroCoreAst*       init_ast    = NULL;
        NecroCoreAst*       new_bind    = necro_core_ast_create_bind(context->arena, temp_symbol, ast2, init_ast);
        NecroCoreAst*       ast3        = necro_core_ast_create_let(context->arena, new_bind, NULL);
        necro_core_ast_swap(app_ast, ast3);
        app_ast->let.expr               = ast3;
        return necro_defunctionalize_let(context, app_ast);
    }
}

NecroStaticValue* necro_defunctionalize_app_con(NecroDefunctionalizeContext* context, NecroCoreAst* app_ast, NecroCoreAst* var_ast, NecroStaticValue* fn_static_value)
{
    // Fully Saturated, First Order, Fun, Constructor
    NecroCoreAst*         apps             = app_ast;
    NecroStaticValueList* arg_svs          = NULL;
    NecroType*            arg_types        = NULL;
    NecroType*            constructed_type = necro_type_con_create(context->arena, fn_static_value->fun.expr_static_value->necro_type->con.con_symbol, NULL);
    constructed_type->kind                 = context->base->star_kind->type;
    NecroType*            con              = constructed_type;
    while (apps->ast_type == NECRO_CORE_AST_APP)
    {
        arg_svs = necro_cons_static_value_list(context->arena, necro_defunctionalize_go(context, apps->app.expr2), arg_svs);
        if (arg_svs->data->type == NECRO_STATIC_VALUE_FUN)
        {
            assert(apps->app.expr2->ast_type == NECRO_CORE_AST_VAR);
            arg_svs->data = necro_static_value_create_env_from_expr(context, apps->app.expr2, arg_svs->data->necro_type, arg_svs->data->fun.fn_symbol, arg_svs->data->fun.expr_static_value);
        }
        arg_types = necro_type_list_create(context->arena, arg_svs->data->necro_type, arg_types);
        con       = necro_type_fn_create(context->arena, arg_svs->data->necro_type, con);
        con->kind = context->base->star_kind->type;
        apps = apps->app.expr1;
    }
    arg_svs = necro_reverse_static_value_list(context->arena, arg_svs);
    // TODO: How to handle something like: data TwoFnsOneVar a = LeftFn a | RightFn a -- Where each side gets a different Env types...
    // Also, Two Maybes with two different Env types.
    // These all stem from returning from funtions from case.
    // Thus we do the branch trick to all inner Envs, but then we need to open them up and apply Branch1_1 and Branch1_2 to each contained Env, necessitates copying data though.
    // TODO: uvars...
    constructed_type->con.args = arg_types;
    var_ast->necro_type        = con;
    return necro_static_value_create_con(context->arena, constructed_type, var_ast->var.ast_symbol, con, arg_svs);
}

NecroStaticValue* necro_defunctionalize_app_fun(NecroDefunctionalizeContext* context, NecroCoreAst* app_ast, NecroCoreAst* var_ast, NecroStaticValue* fn_static_value, const int32_t app_count)
{
    const int32_t arity           = (int32_t) var_ast->var.ast_symbol->arity;
    int32_t       difference      = arity - app_count;
    const bool    is_higher_order = necro_type_is_higher_order_function(var_ast->necro_type, arity);
    // Saturated
    if (difference == 0)
    {
        if (!var_ast->var.ast_symbol->is_constructor && is_higher_order)
        {
            assert(false);
            return NULL; // TODO: Inline
        }
        else if (var_ast->var.ast_symbol->is_constructor)
        {
            return necro_defunctionalize_app_con(context, app_ast, var_ast, fn_static_value);
        }
        else
        {
            // Fully Saturated, First Order, Fun
            NecroCoreAst* apps = app_ast;
            while (apps->ast_type == NECRO_CORE_AST_APP)
            {
                necro_defunctionalize_go(context, apps->app.expr2);
                apps = apps->app.expr1;
            }
            return fn_static_value->fun.expr_static_value;
        }
    }
    // Undersaturated
    else if (difference > 0)
    {
        return necro_static_value_create_env_from_expr(context, app_ast, fn_static_value->necro_type, fn_static_value->fun.fn_symbol, fn_static_value->fun.expr_static_value);
    }
    // Oversaturated
    else
    {
        // Transform: f x y z ==> let temp_aa1 = f x in temp_aa1 y z
        NecroCoreAst* ast1 = app_ast;
        while (difference < 0)
        {
            difference++;
            ast1 = ast1->app.expr1;
        }
        // TODO: Correct type here...how!?
        NecroSymbol         temp_name   = necro_intern_unique_string(context->intern, "tmp");
        NecroCoreAstSymbol* temp_symbol = necro_core_ast_symbol_create(context->arena, temp_name, necro_type_fresh_var(context->arena, NULL));
        temp_symbol->type->kind         = context->base->star_kind->type;
        NecroCoreAst*       ast2        = necro_core_ast_create_var(context->arena, temp_symbol, temp_symbol->type);
        necro_core_ast_swap(ast1, ast2);
        // TODO: Handle initializer!
        NecroCoreAst*       init_ast    = NULL;
        NecroCoreAst*       new_bind    = necro_core_ast_create_bind(context->arena, temp_symbol, ast2, init_ast);
        NecroCoreAst*       ast3        = necro_core_ast_create_let(context->arena, new_bind, NULL);
        necro_core_ast_swap(app_ast, ast3);
        app_ast->let.expr               = ast3;
        return necro_defunctionalize_let(context, app_ast);
    }
}

NecroStaticValue* necro_defunctionalize_app(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    int32_t       app_count          = 0;
    NecroCoreAst* app                = ast;
    while (app->ast_type == NECRO_CORE_AST_APP)
    {
        app_count++;
        app = app->app.expr1;
    }
    assert(app->ast_type == NECRO_CORE_AST_VAR);
    NecroStaticValue* fn_static_value = necro_defunctionalize_go(context, app);
    switch (fn_static_value->type)
    {
    case NECRO_STATIC_VALUE_FUN: return necro_defunctionalize_app_fun(context, ast, app, fn_static_value, app_count);
    case NECRO_STATIC_VALUE_ENV: return necro_defunctionalize_app_env(context, ast, app, fn_static_value, app_count);
    default:                     assert(false); return NULL;
    }
}

//--------------------
// Case
//--------------------
typedef enum
{
    NECRO_CASE_ALT_ELIMINATE,
    NECRO_CASE_ALT_KEEP,
} NECRO_CASE_ALT_STATUS;
NECRO_CASE_ALT_STATUS necro_defunctionalize_case_pat(NecroDefunctionalizeContext* context, NecroCoreAst* ast, NecroStaticValue* expr_sv)
{
    assert(ast != NULL);
    switch (ast->ast_type)
    {

    case NECRO_CORE_AST_APP:
        switch (expr_sv->type)
        {
        case NECRO_STATIC_VALUE_CON:
        {
            NecroCoreAst* var_ast = ast;
            while (var_ast->ast_type == NECRO_CORE_AST_APP)
                var_ast = var_ast->app.expr1;
            if (var_ast->var.ast_symbol != expr_sv->con.con_symbol && !necro_type_exact_unify(ast->necro_type, expr_sv->necro_type))
                return NECRO_CASE_ALT_ELIMINATE;
            NecroStaticValueList* rev_con_args = expr_sv->con.args;
            while (ast->ast_type == NECRO_CORE_AST_APP)
            {
                if (necro_defunctionalize_case_pat(context, ast->app.expr2, rev_con_args->data) == NECRO_CASE_ALT_ELIMINATE)
                    return NECRO_CASE_ALT_ELIMINATE;
                rev_con_args = rev_con_args->next;
                ast          = ast->app.expr1;
            }
            assert(ast->ast_type == NECRO_CORE_AST_VAR);
            if (var_ast->var.ast_symbol == expr_sv->con.con_symbol)
                ast->necro_type = expr_sv->con.con_fn_type;
            return NECRO_CASE_ALT_KEEP;
        }

        case NECRO_STATIC_VALUE_DYN:
        {
            NecroType*   arg_types = expr_sv->dyn.type->con.args;
            const size_t arg_count = necro_type_list_count(arg_types);
            size_t       arg_i     = 0;
            while (ast->ast_type == NECRO_CORE_AST_APP)
            {
                // HACK: Kinda lame...
                NecroType* arg_type = arg_types;
                for (size_t i = arg_i + 1; i < arg_count; ++i)
                    arg_type = arg_type->list.next;
                arg_type = arg_type->list.item;
                necro_defunctionalize_case_pat(context, ast->app.expr2, necro_static_value_create_dyn(context->arena, arg_type));
                // rev_con_args = rev_con_args->next;
                ast = ast->app.expr1;
                arg_i++;
            }
            assert(ast->ast_type == NECRO_CORE_AST_VAR);
            return NECRO_CASE_ALT_KEEP;
        }
        default:
            assert(false);
            return NECRO_CASE_ALT_KEEP;
        }

    case NECRO_CORE_AST_VAR:
        if (ast->var.ast_symbol->is_constructor)
        {
            if (expr_sv->type == NECRO_STATIC_VALUE_CON && ast->var.ast_symbol != expr_sv->con.con_symbol && !necro_type_exact_unify(ast->necro_type, expr_sv->necro_type))
                return NECRO_CASE_ALT_ELIMINATE;
            ast->necro_type = expr_sv->necro_type;
            return NECRO_CASE_ALT_KEEP;
        }
        ast->var.ast_symbol->static_value = expr_sv;
        ast->var.ast_symbol->type         = expr_sv->necro_type;
        ast->necro_type                   = expr_sv->necro_type;
        return NECRO_CASE_ALT_KEEP;
    case NECRO_CORE_AST_LIT: return NECRO_CASE_ALT_KEEP;
    default:                 assert(false && "Unimplemented Ast in necro_defunctionalize_case_pat"); return NECRO_CASE_ALT_KEEP;
    }
}

// TODO: Case with returning functions!!!!!!!!!!!!!!!
NecroStaticValue* necro_defunctionalize_case(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    NecroStaticValue* expr_sv = necro_defunctionalize_go(context, ast->case_expr.expr);

    if (expr_sv->type == NECRO_STATIC_VALUE_FUN)
    {
        assert(ast->case_expr.expr->ast_type == NECRO_CORE_AST_VAR);
        expr_sv = necro_static_value_create_env_from_expr(context, ast->case_expr.expr, expr_sv->necro_type, expr_sv->fun.fn_symbol, expr_sv->fun.expr_static_value);
    }

    NecroCoreAstList* alts     = ast->case_expr.alts;
    NecroStaticValue* case_sv  = NULL;
    NecroCoreAstList* prev_alt = NULL;
    while (alts != NULL)
    {
        if (necro_defunctionalize_case_pat(context, alts->data->case_alt.pat, expr_sv) == NECRO_CASE_ALT_ELIMINATE)
        {
            // TODO: Test Eliminate alt
            if (prev_alt == NULL)
                ast->case_expr.alts = alts->next;
            else
                prev_alt->next = alts->next;
            alts = alts->next;
            continue;
        }
        NecroStaticValue* alt_sv = necro_defunctionalize_go(context, alts->data->case_alt.expr);
        // Branch Static Value?
        // TOOD: What to do with this!?
        case_sv  = alt_sv;
        prev_alt = alts;
        alts     = alts->next;
    }
    return case_sv;
}

NecroStaticValue* necro_defunctionalize_go(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    if (ast == NULL)
        return NULL;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:       return necro_defunctionalize_var(context, ast);
    case NECRO_CORE_AST_LIT:       return necro_defunctionalize_lit(context, ast);
    case NECRO_CORE_AST_LET:       return necro_defunctionalize_let(context, ast);
    case NECRO_CORE_AST_BIND:      return necro_defunctionalize_bind(context, ast);
    case NECRO_CORE_AST_FOR:       return necro_defunctionalize_for(context, ast);
    case NECRO_CORE_AST_LAM:       return necro_defunctionalize_lam(context, ast);
    case NECRO_CORE_AST_APP:       return necro_defunctionalize_app(context, ast);
    case NECRO_CORE_AST_CASE:      return necro_defunctionalize_case(context, ast);
    // case NECRO_CORE_AST_BIND_REC:
    case NECRO_CORE_AST_DATA_DECL: return necro_defunctionalize_data_decl(context, ast);
    case NECRO_CORE_AST_DATA_CON:  return necro_defunctionalize_data_con(context, ast);
    default:                       assert(false && "Unimplemented Ast in necro_defunctionalize_go"); return NULL;
    }
}

void necro_core_defunctionalize(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    UNUSED(info);
    NecroDefunctionalizeContext context = necro_defunctionalize_context_create(intern, base, core_ast_arena);
    necro_defunctionalize_go(&context, core_ast_arena->root);
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_CORE_LAMBDA_LIFT_VERBOSE 1
void necro_defunctionalize_test_result(const char* test_name, const char* str)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
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
    necro_core_lambda_lift(info, &intern, &base, &core_ast);
    necro_core_defunctionalize(info, &intern, &base, &core_ast);
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));

    // Print
#if NECRO_CORE_LAMBDA_LIFT_VERBOSE
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
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

// TODO: Test unique types
// TODO: Test Recursive values!
void necro_core_defunctionalize_test()
{
    necro_announce_phase("Defunctionalize");

/*

    {
        const char* test_name   = "Identity 1";
        const char* test_source = ""
            "x = True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Identity 2";
        const char* test_source = ""
            "x = True && False\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturate 1";
        const char* test_source = ""
            "x = eq True \n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturate 2";
        const char* test_source = ""
            "wtf :: Int -> Bool -> Bool -> Bool\n"
            "wtf f g b = b\n"
            "notQuite = wtf 0 True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturate, Then Apply Saturated 1";
        const char* test_source = ""
            "f = eq True \n"
            "x = f False\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturate, Then Apply Saturated 2";
        const char* test_source = ""
            "f = eq\n"
            "x = f True False\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Oversaturate 1";
        const char* test_source = ""
            "id' x = add\n"
            "boom :: Int\n"
            "boom = id' () 0 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Oversaturate 2";
        const char* test_source = ""
            "id' x = add 0\n"
            "boom :: Int\n"
            "boom = id' () 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Double Undersaturated";
        const char* test_source = ""
            "allTheAnds x y z w = x && y && z && w\n"
            "under1 = allTheAnds True\n"
            "under2 = under1 False True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Double Undersaturated 2";
        const char* test_source = ""
            "allTheAnds x y z w = x && y && z && w\n"
            "under1 = allTheAnds True\n"
            "under2 = under1 False True\n"
            "go = under2 False\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Oversaturate 2";
        const char* test_source = ""
            "id' :: () -> () -> Int -> Int\n"
            "id' x y = add 100\n"
            "dud = id' ()\n"
            "boom = dud () 666 \n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Oversaturate 3";
        const char* test_source = ""
            "id' :: () -> () -> Int -> Int -> Int\n"
            "id' x y = add\n"
            "dud = id' ()\n"
            "boom = dud () 666 777\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Oversaturate 4";
        const char* test_source = ""
            "id' :: () -> () -> Int -> Int\n"
            "id' x y = add 666\n"
            "dud = id' ()\n"
            "boom = dud () 777\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda Lift";
        const char* test_source = ""
            "f = r where\n"
            "  t = True\n"
            "  r = (\\x -> x || t) False\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Constructor 1";
        const char* test_source = ""
            "data PairOfNothing = PairOfNothing () ()\n"
            "partialPair = PairOfNothing ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Constructor 2";
        const char* test_source = ""
            "data PairOfNothing = PairOfNothing () ()\n"
            "partialPair = PairOfNothing ()\n"
            "perfectPair = partialPair ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Constructor 2";
        const char* test_source = ""
            "data PairOfSomething a b = PairOfSomething a b\n"
            "partialPair = PairOfSomething True\n"
            "perfectPair = partialPair ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Constructor Fn 1";
        const char* test_source = ""
            "maybeJustMaybe :: Maybe (Int -> Int -> Int)\n"
            "maybeJustMaybe = Just add\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Constructor Fn 2";
        const char* test_source = ""
            "maybeJustMaybe :: Maybe (Int -> Int)\n"
            "maybeJustMaybe = Just (add 0)\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturated Constructor Fn 1";
        const char* test_source = ""
            "data Pair a b = Pair a b\n"
            "imperfectPair :: Bool -> Pair (Int -> Int) Bool\n"
            "imperfectPair = Pair (add 0)\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturated Constructor Fn 2";
        const char* test_source = ""
            "data Pair a b = Pair a b\n"
            "imperfectPair :: Bool -> Pair (Int -> Int) Bool\n"
            "imperfectPair = Pair (add 0)\n"
            "perfectPair = imperfectPair False\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturated HOF";
        const char* test_source = ""
            "wtfHof :: (Int -> Int) -> (Bool -> Bool) -> Bool -> Bool\n"
            "wtfHof f g b = g b\n"
            "and' b1 b2 = b1 && b2\n"
            "notQuite = wtfHof (add 0) (and' False)\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturated HOF 2";
        const char* test_source = ""
            "wtfHof :: (Int -> Int) -> (Bool -> Bool) -> Bool -> Bool\n"
            "wtfHof f g b = g b\n"
            "and' b1 b2 = b1 && b2\n"
            "notQuite = wtfHof (add 33) (wtfHof (sub 666) (and' False))\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Undersaturated HOF 3";
        const char* test_source = ""
            "wtfHof :: (Int -> Int) -> (Bool -> Bool) -> Bool -> Bool\n"
            "wtfHof f g b = g b\n"
            "and' b1 b2 = b1 && b2\n"
            "notQuite i i2 = wtfHof (add i) (wtfHof (sub i2) (and' False))\n"
            "almost = notQuite 100\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 1";
        const char* test_source = ""
            "trueOrFalse =\n"
            "  case True of\n"
            "    True  -> False\n"
            "    False -> True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 2";
        const char* test_source = ""
            "maybeNothing =\n"
            "  case (Just False) of\n"
            "    Nothing -> True\n"
            "    Just b  -> b\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 2-2";
        const char* test_source = ""
            "maybeNothing =\n"
            "  case Nothing of\n"
            "    Nothing -> True\n"
            "    Just b  -> b\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 3-1";
        const char* test_source = ""
            "maybeNothing =\n"
            "  case (eq True, eq False) of\n"
            "    (f, g) -> f\n"
            "appNothing = maybeNothing True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 3-2";
        const char* test_source = ""
            "maybeNothing =\n"
            "  case Just (eq True) of\n"
            "    Just f  -> f False\n"
            "    Nothing -> True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 4";
        const char* test_source = ""
            "rollEm' =\n"
            "  case add of\n"
            "    f -> f\n"
            "useEm :: Int\n"
            "useEm = rollEm' 100 200\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 5";
        const char* test_source = ""
            "countEm' :: Int -> Int -> Int -> Int -> Int -> Int\n"
            "countEm' v w x y z = v + w * x - y + z\n"
            "rollEm' =\n"
            "  case (countEm' 1 2) of\n"
            "    f -> f\n"
            "useEm :: Int\n"
            "useEm = rollEm' 3 4 5\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }
*/

    {
        const char* test_name   = "Double Up";
        const char* test_source = ""
            "data TwoInts   = TwoInts Int Int\n"
            "data DoubleUp  = DoubleUp TwoInts TwoInts\n"
            "doubleDown :: Int -> DoubleUp\n"
            "doubleDown i = DoubleUp (TwoInts i i) (TwoInts i i)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

// TODO: FIX, looks like this never got cleaned up or something broke things.
/*
    {
        const char* test_name   = "Case 6";
        const char* test_source = ""
            "data Either' a b = Left' a | Right' b\n"
            "eitherOr =\n"
            "  case Left' (eq True) of\n"
            "    Left'  l -> l\n"
            "    Right' r -> r\n"
            "leftPlease = eitherOr True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case 7";
        const char* test_source = ""
            "data Either' a b = Left' a | Right' b\n"
            "eitherOr b =\n"
            "  case Left' (eq True) of\n"
            "    Left'  l -> l b\n"
            "    Right' r -> r ()\n"
            "leftPlease = eitherOr True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }
*/

}

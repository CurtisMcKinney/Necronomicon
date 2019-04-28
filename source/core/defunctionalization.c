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

/*
    - Largely based on "High Performance Defunctionalisation in Futhark": https://futhark-lang.org/publications/tfp18.pdf
    - Some obvious differences are that we support the branching extension
    - Perhaps also Arrays (since all containers have a fixed bounded size in Necro).
    - If we could obviate the need for using lifted type in recursive value check we could nix lifted types entirely.

    TODO
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

// TODO: Compare this to DataDecl and DataCon in NecroCoreAst
typedef struct
{
    NecroType*                   type; // The Type of the constructor.
    NecroAstSymbol*              con;  // The Specific constructor of the provided type. Replace with NecroCoreAstSymbol or whatever when that's done.
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

NecroStaticValue* necro_static_value_create_env(NecroPagedArena* arena, NecroType* env_type, NecroCoreAstSymbol* env_con_symbol, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* expr_static_value, NecroStaticValueList* arg_static_values)
{
    NecroStaticValue* static_value      = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_ENV, env_type);
    static_value->env.env_con_symbol    = env_con_symbol;
    static_value->env.fn_symbol         = fn_symbol;
    static_value->env.expr_static_value = expr_static_value;
    static_value->env.arg_static_values = arg_static_values;
    return static_value;
}

NecroStaticValue* necro_static_value_create_fun(NecroPagedArena* arena, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* expr_static_value)
{
    NecroStaticValue* static_value      = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_FUN, fn_symbol->type);
    static_value->fun.fn_symbol         = fn_symbol;
    static_value->fun.expr_static_value = expr_static_value;
    return static_value;
}

// Note: This calls defunctionalize_go on all the arguments in the apps....is that a good idea!?!?!
NecroStaticValue* necro_static_value_create_env_from_expr(NecroDefunctionalizeContext* context, NecroCoreAst* ast, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* fn_expr_static_value)
{
    size_t        app_count = 0;
    NecroCoreAst* var_ast   = ast;
    while (var_ast->ast_type == NECRO_CORE_AST_APP)
    {
        app_count++;
        var_ast = var_ast->app.expr1;
    }
    NecroType*            fn_type                = fn_symbol->type;
    NecroAstSymbol*       env_type_symbol        = necro_base_get_env_type(context->base, app_count);
    NecroCoreAstSymbol*   env_con_symbol         = necro_base_get_env_con(context->base, app_count)->core_ast_symbol;
    NecroType*            env_type_head          = NULL;
    NecroType*            env_type_curr          = NULL;
    NecroCoreAst*         apps                   = ast;
    NecroStaticValueList* arg_static_values_head = NULL;
    NecroStaticValueList* arg_static_values_curr = NULL;
    // TODO: Ensure fn type is monomorphic...How to handle partially applied constructor functions!?!?!
    while (apps->ast_type == NECRO_CORE_AST_APP)
    {
        assert(fn_type->type == NECRO_TYPE_FUN);
        if (env_type_head == NULL)
        {
            env_type_head = necro_type_list_create(context->arena, fn_type->fun.type1, NULL);
            env_type_curr = env_type_head;
        }
        else
        {
            env_type_curr->list.next = necro_type_list_create(context->arena, fn_type->fun.type1, NULL);
            env_type_curr            = env_type_curr->list.next;
        }
        NecroStaticValue* arg_static_value = necro_defunctionalize_go(context, apps->app.expr2);
        if (arg_static_values_head == NULL)
        {
            arg_static_values_head = necro_cons_static_value_list(context->arena, arg_static_value, NULL);
            arg_static_values_curr = arg_static_values_head;
        }
        else
        {
            arg_static_values_curr->next = necro_cons_static_value_list(context->arena, arg_static_value, NULL);
            arg_static_values_curr       = arg_static_values_curr->next;
        }
        fn_type                            = fn_type->fun.type2;
        apps                               = apps->app.expr1;
    }
    *var_ast                           = *necro_core_ast_create_var(context->arena, env_con_symbol);
    return necro_static_value_create_env(context->arena, necro_type_con_create(context->arena, env_type_symbol, env_type_head), env_con_symbol, fn_symbol, fn_expr_static_value, arg_static_values_head);
}

///////////////////////////////////////////////////////
// Defunctionalize
///////////////////////////////////////////////////////

NecroStaticValue* necro_defunctionalize_var(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
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
        ast->data_con.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->data_con.ast_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(ast->data_con.type)));
    else
        ast->data_con.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(ast->data_con.type));
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
    assert(ast->bind.ast_symbol->arity < INT32_MAX);
    if (necro_type_is_higher_order_function(ast->bind.ast_symbol->type, (int32_t) ast->bind.ast_symbol->arity))
        return NULL;
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
            ast->bind.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->bind.ast_symbol, static_value);
        }
        else
        {
            ast->bind.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->bind.ast_symbol, static_value);
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
        return necro_static_value_create_env_from_expr(context, ast->lambda.expr, expr_static_value->fun.fn_symbol, expr_static_value->fun.expr_static_value);
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
    const bool    is_higher_order = necro_type_is_higher_order_function(fn_static_value->env.fn_symbol->type, arity);
    // Saturated
    if (difference == 0)
    {
        if (is_higher_order == true)
        {
            assert(false);
            return NULL; // TODO: Inline
        }
        else
        {
            // Under Saturated, First Order, Env
            NecroCoreAst* expr_ast  = necro_core_ast_create_var(context->arena, fn_static_value->env.fn_symbol);
            NecroCoreAst* pat_ast   = NULL;
            NecroType*    env_args  = fn_static_value->necro_type->con.args;
            bool          is_env_fn = env_args != NULL;
            // Apply env vars
            if (is_env_fn)
            {
                pat_ast = necro_core_ast_create_var(context->arena, fn_static_value->env.env_con_symbol);
                while (env_args != NULL)
                {
                    NecroCoreAstSymbol* env_var_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "free_var"), env_args->list.item);
                    env_var_symbol->static_value       = necro_static_value_create_dyn(context->arena, env_args->list.item);
                    NecroCoreAst*       env_pat_var    = necro_core_ast_create_var(context->arena, env_var_symbol);
                    NecroCoreAst*       env_expr_var   = necro_core_ast_create_var(context->arena, env_var_symbol);
                    pat_ast                            = necro_core_ast_create_app(context->arena, pat_ast, env_pat_var);
                    expr_ast                           = necro_core_ast_create_app(context->arena, expr_ast, env_expr_var);
                    env_args                           = env_args->list.next;
                }
            }
            // Apply App vars
            NecroCoreAst* apps = app_ast;
            while (apps->ast_type == NECRO_CORE_AST_APP)
            {
                necro_defunctionalize_go(context, apps->app.expr2);
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
            return fn_static_value->env.expr_static_value;
        }
    }
    // Undersaturated
    else if (difference > 0)
    {
        // Under Saturated, First Order, Env
        NecroCoreAst* expr_ast  = necro_core_ast_create_var(context->arena, fn_static_value->env.fn_symbol);
        NecroCoreAst* pat_ast   = NULL;
        NecroType*    env_args  = fn_static_value->necro_type->con.args;
        bool          is_env_fn = env_args != NULL;
        // Apply env vars
        if (is_env_fn)
        {
            pat_ast = necro_core_ast_create_var(context->arena, fn_static_value->env.env_con_symbol);
            while (env_args != NULL)
            {
                NecroCoreAstSymbol* env_var_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "free_var"), env_args->list.item);
                env_var_symbol->static_value       = necro_static_value_create_dyn(context->arena, env_args->list.item);
                NecroCoreAst*       env_pat_var    = necro_core_ast_create_var(context->arena, env_var_symbol);
                NecroCoreAst*       env_expr_var   = necro_core_ast_create_var(context->arena, env_var_symbol);
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
        return necro_static_value_create_env_from_expr(context, expr_ast, fn_static_value->env.fn_symbol, fn_static_value->env.expr_static_value);
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
        NecroCoreAst*       ast2        = necro_core_ast_create_var(context->arena, temp_symbol);
        necro_core_ast_swap(ast1, ast2);
        NecroCoreAst*       new_bind    = necro_core_ast_create_bind(context->arena, temp_symbol, ast2);
        NecroCoreAst*       ast3        = necro_core_ast_create_let(context->arena, new_bind, NULL);
        necro_core_ast_swap(app_ast, ast3);
        app_ast->let.expr               = ast3;
        return necro_defunctionalize_let(context, app_ast);
    }
}

NecroStaticValue* necro_defunctionalize_app_fun(NecroDefunctionalizeContext* context, NecroCoreAst* app_ast, NecroCoreAst* var_ast, NecroStaticValue* fn_static_value, const int32_t app_count)
{
    const int32_t arity           = (int32_t) var_ast->var.ast_symbol->arity;
    int32_t       difference      = arity - app_count;
    const bool    is_higher_order = necro_type_is_higher_order_function(var_ast->var.ast_symbol->type, arity);
    // Saturated
    if (difference == 0)
    {
        if (is_higher_order == true)
        {
            assert(false);
            return NULL; // TODO: Inline
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
        return necro_static_value_create_env_from_expr(context, app_ast, fn_static_value->fun.fn_symbol, fn_static_value->fun.expr_static_value);
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
        NecroCoreAst*       ast2        = necro_core_ast_create_var(context->arena, temp_symbol);
        necro_core_ast_swap(ast1, ast2);
        NecroCoreAst*       new_bind    = necro_core_ast_create_bind(context->arena, temp_symbol, ast2);
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

// // TODO: Case with returning functions!!!!!!!!!!!!!!!
// NecroStaticValue* necro_defunctionalize_case(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
// {
//     assert(context != NULL);
//     assert(ast->ast_type == NECRO_CORE_AST_CASE);
//     return NULL;
// }

// NOTE: Leave bound lambdas be.
// App is where the magic happens: Inline functions which apply higher-order functions, pass in the higher order function as an Env constructor, unwrap and apply function at each call site
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
    // case NECRO_CORE_AST_CASE:      necro_defunctionalize_case(ll, ast); return;
    // case NECRO_CORE_AST_BIND_REC:
    case NECRO_CORE_AST_DATA_DECL: return necro_defunctionalize_data_decl(context, ast);
    case NECRO_CORE_AST_DATA_CON:  return necro_defunctionalize_data_con(context, ast);
    default:                       assert(false && "Unimplemented Ast in necro_defunctionalize_go"); return NULL;
    }
}

void necro_defunctionalize(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
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
    necro_defunctionalize(info, &intern, &base, &core_ast);
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

// TODO: Recursive values!
// TODO:
void necro_defunctionalize_test()
{
    necro_announce_phase("Defunctionalize");

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

}

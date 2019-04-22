/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "defunctionalization.h"
#include <stdio.h>
#include "type.h"

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
    NECRO_STATIC_VALUE_LAM,
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
    NecroCoreAstSymbol*      env_symbol; // If we have an env this points to its type
    NecroCoreAstSymbol*      fn_symbol;
    struct NecroStaticValue* expr_static_value;
} NecroStaticValueLambda;

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
        NecroStaticValueLambda      lam;
        NecroStaticValueConstructor con;
        NecroStaticValueSum         sum;
    };
    NECRO_STATIC_VALUE_TYPE type;
} NecroStaticValue;

NECRO_DECLARE_ARENA_LIST(struct NecroStaticValue, StaticValue, static_value);

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

NecroStaticValue* necro_static_value_alloc(NecroPagedArena* arena, NECRO_STATIC_VALUE_TYPE type)
{
    NecroStaticValue* static_value = necro_paged_arena_alloc(arena, sizeof(NecroStaticValue));
    static_value->type             = type;
    return static_value;
}

NecroStaticValue* necro_static_value_create_dyn(NecroPagedArena* arena, NecroType* type)
{
    NecroStaticValue* static_value = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_DYN);
    static_value->dyn.type         = type;
    return static_value;
}

NecroStaticValue* necro_static_value_create_lam(NecroPagedArena* arena, NecroCoreAstSymbol* env_symbol, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* expr_static_value)
{
    NecroStaticValue* static_value      = necro_static_value_alloc(arena, NECRO_STATIC_VALUE_LAM);
    static_value->lam.env_symbol        = env_symbol;
    static_value->lam.fn_symbol         = fn_symbol;
    static_value->lam.expr_static_value = expr_static_value;
    return static_value;
}

///////////////////////////////////////////////////////
// Defunctionalize
///////////////////////////////////////////////////////
NecroStaticValue* necro_defunctionalize_go(NecroDefunctionalizeContext* context, NecroCoreAst* ast);

NecroStaticValue* necro_defunctionalize_var(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
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
    return NULL;
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
        if (ast->ast_type != NECRO_CORE_AST_LET)
            return necro_defunctionalize_go(context, ast);
        necro_defunctionalize_go(context, ast->let.bind);
        ast = ast->let.expr;
    }
    return NULL;
}

NecroStaticValue* necro_defunctionalize_bind(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    NecroStaticValue* static_value     = necro_defunctionalize_go(context, ast->bind.expr);
    if (ast->bind.expr->ast_type == NECRO_CORE_AST_LAM)
    {
        ast->bind.ast_symbol->static_value = necro_static_value_create_lam(context->arena, NULL, ast->bind.ast_symbol, static_value);
        ast->bind.ast_symbol->ast          = ast;
    }
    else
    {
        ast->bind.ast_symbol->static_value = static_value;
    }
    // Calculate arity
    NecroCoreAst* expr = ast->bind.expr;
    while (expr->ast_type == NECRO_CORE_AST_LAM)
    {
        ast->bind.ast_symbol->arity++;
        expr = expr->lambda.expr;
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
    return expr_static_value;
}

NecroStaticValue* necro_defunctionalize_app(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    bool          applied_lambda = false;
    int32_t       app_count      = 0;
    NecroCoreAst* app            = ast;
    while (app->ast_type == NECRO_CORE_AST_APP)
    {
        if (necro_defunctionalize_go(context, app->app.expr2)->type != NECRO_STATIC_VALUE_DYN)
            applied_lambda = true;
        app_count++;
        app = app->app.expr1;
    }
    assert(app->ast_type == NECRO_CORE_AST_VAR);
    NecroStaticValue* fn_static_value = necro_defunctionalize_go(context, app);
    int32_t           arity           = (int32_t) app->var.ast_symbol->arity;
    int32_t           difference      = arity - app_count;

    // TODO: Take into account applied_lambda !?!?!

    // Undersaturated
    if (difference < 0)
    {
        // If Dyn Value
        // Else
        // If Static Value
        // Create Env deriving its type from applied expressions
        // Create new function
        return NULL;
    }
    // Oversaturated
    else if (difference > 0)
    {
        return NULL;
    }
    // Fully Saturated
    else
    {
        if (fn_static_value->type == NECRO_STATIC_VALUE_DYN)
        {
            // Find Result type and return static_value_dyn using it.
            NecroType* dyn_type = fn_static_value->dyn.type;
            for (int32_t i = 0; i < app_count; ++i)
            {
                assert(dyn_type->type == NECRO_TYPE_FUN);
                dyn_type = dyn_type->fun.type2;
            }
            return necro_static_value_create_dyn(context->arena, dyn_type);
        }
        else if (fn_static_value->type == NECRO_STATIC_VALUE_LAM)
        {
            if (applied_lambda == true)
            {
                return NULL; // TODO:
            }
            else if (fn_static_value->lam.env_symbol != NULL)
            {
                // Static Value has an env, which means app is the env value, we need to apply fn to it.
                NecroCoreAst* core_ast1 = app;                                                                                                                        // Contains env
                NecroCoreAst* core_ast2 = necro_core_ast_create_app(context->arena, necro_core_ast_create_var(context->arena, fn_static_value->lam.fn_symbol), NULL); // Contains app fn NULL
                necro_core_ast_swap(core_ast1, core_ast2);
                core_ast1->app.expr2    = core_ast2;
                // Now core_ast1 contains: app fn env
                // Now core_ast2 contains: env
                return fn_static_value->lam.expr_static_value;
            }
            else
            {
                // No lambdas applied, No Env, just a simple function call.
                return fn_static_value->lam.expr_static_value;
            }
        }
        else
        {
            assert(false && "TODO");
            return NULL;
        }
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

void necro_defunctionalize_test()
{
}

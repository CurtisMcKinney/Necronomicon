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
#include "core_simplify.h"
#include "kind.h"

/*
    - Largely based on "High Performance Defunctionalisation in Futhark": https://futhark-lang.org/publications/tfp18.pdf
    - Some obvious differences are that we support branching and arrays, and we treat functions as a whole instead of lambda piecemeal.
    - If we could obviate the need for using lifted type in recursive value check we could nix lifted types entirely.

    TODO
        - Look at  HOFs being passed HOFs (i.e. 3rd order, 4th order functions, etc), Things are getting wonky. Perhaps we need a refactor with a more well thought out flow

        - Filter HOFs out of ast tree!

        - Case really needs to be looked at again, this seems to fundamentally mishandle monomorphic and polymorphic types!
        - Type vars which resolve to functions and are used in multiple places will need to have a sum type environment wrapped around the actual fn env type used!
        - Test unique types
        - Test Recursive values!
        - Arrays!

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
    // NECRO_STATIC_VALUE_TVAR_SUM // For type variables which resolve to function types which are used in multiple places in a data type
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

typedef struct NecroHOFBucket
{
    NecroCoreAstSymbol*        fn_symbol;
    NecroCoreAstSymbolSubList* subs;
    size_t                     hash;
    NecroCoreAst*              inst_ast;
} NecroHOFBucket;

typedef struct NecroHOFCache
{
    NecroHOFBucket* buckets;
    size_t          count;
    size_t          capacity;
} NecroHOFCache;

typedef struct
{
    NecroHOFCache    hof_cache;
    NecroPagedArena* arena;
    NecroIntern*     intern;
    NecroBase*       base;
    NecroCoreAst*    lift_point;
    bool             at_top;
} NecroDefunctionalizeContext;

NecroHOFCache necro_hof_cache_empty()
{
    return (NecroHOFCache)
    {
        .buckets  = NULL,
        .count    = 0,
        .capacity = 0,
    };
}

NecroHOFCache necro_hof_cache_create()
{
    const size_t initial_capacity = 32;
    NecroHOFCache cache = (NecroHOFCache)
    {
        .buckets  = emalloc(initial_capacity * sizeof(NecroHOFBucket)),
        .count    = 0,
        .capacity = initial_capacity,
    };
    for (size_t i = 0; i < initial_capacity; ++i)
        cache.buckets[i] = (NecroHOFBucket) { .fn_symbol = NULL, .subs = NULL, .hash = 0, .inst_ast = NULL };
    return cache;
}

void necro_hof_cache_destroy(NecroHOFCache* cache)
{
    assert(cache != NULL);
    free(cache->buckets);
    *cache = necro_hof_cache_empty();
}

NecroDefunctionalizeContext necro_defunctionalize_context_empty()
{
    return (NecroDefunctionalizeContext)
    {
        .hof_cache  = necro_hof_cache_empty(),
        .arena      = NULL,
        .intern     = NULL,
        .base       = NULL,
        .lift_point = NULL,
        .at_top     = true,
    };
}

NecroDefunctionalizeContext necro_defunctionalize_context_create(NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    return (NecroDefunctionalizeContext)
    {
        .hof_cache  = necro_hof_cache_create(),
        .arena      = &core_ast_arena->arena,
        .intern     = intern,
        .base       = base,
        .lift_point = NULL,
        .at_top     = true,
    };
}

void necro_defunctionalize_context_destroy(NecroDefunctionalizeContext* context)
{
    assert(context != NULL);
    necro_hof_cache_destroy(&context->hof_cache);
    *context = necro_defunctionalize_context_empty();
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroStaticValue* necro_defunctionalize_go(NecroDefunctionalizeContext* context, NecroCoreAst* ast);
NecroCoreAst*     necro_hof_cache_get(NecroHOFCache* cache, NecroCoreAstSymbol* fn_symbol, NecroCoreAstSymbolSubList* subs);
void              necro_hof_cache_insert(NecroHOFCache* cache, NecroCoreAstSymbol* fn_symbol, NecroCoreAstSymbolSubList* subs, NecroCoreAst* inst_ast);

///////////////////////////////////////////////////////
// Static Values
///////////////////////////////////////////////////////
NecroStaticValue* necro_static_value_alloc(NecroPagedArena* arena, NECRO_STATIC_VALUE_TYPE type, NecroType* necro_type)
{
    assert(necro_type != NULL);
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

// Note: This calls defunctionalize_go on all the arguments in the apps....is that a good idea!?!?!
NecroStaticValue* necro_static_value_create_env_from_expr(NecroDefunctionalizeContext* context, NecroCoreAst* ast, NecroType* a_fn_type, NecroCoreAstSymbol* fn_symbol, NecroStaticValue* fn_expr_static_value)
{
    // TODO: Update app types!?!?!?!?!?
    size_t        app_count = 0;
    NecroCoreAst* var_ast   = ast;
    while (var_ast->ast_type == NECRO_CORE_AST_APP)
    {
        app_count++;
        var_ast = var_ast->app.expr1;
    }
    NecroCoreAstSymbol*   env_con_symbol    = necro_base_get_env_con(context->base, app_count)->core_ast_symbol;
    NecroCoreAst*         apps              = ast;
    NecroStaticValueList* arg_static_values = NULL;
    while (apps->ast_type == NECRO_CORE_AST_APP)
    {
        NecroStaticValue* arg_static_value = necro_defunctionalize_go(context, apps->app.expr2);
        arg_static_values                  = necro_cons_static_value_list(context->arena, arg_static_value, arg_static_values);
        apps                               = apps->app.expr1;
    }
    NecroType*            env_mono_con = necro_type_instantiate(context->arena, NULL, context->base, env_con_symbol->type, NULL, zero_loc, zero_loc);
    NecroStaticValueList* arg_svs      = arg_static_values;
    NecroType*            env_con_fn   = env_mono_con;
    while (app_count > 0)
    {
        assert(env_con_fn->type == NECRO_TYPE_FUN);
        unwrap(NecroType, necro_type_unify(context->arena, NULL, context->base, arg_svs->data->necro_type, env_con_fn->fun.type1, NULL));
        env_con_fn->ownership = context->base->ownership_share->type;
        env_con_fn->kind      = context->base->star_kind->type;
        arg_svs               = arg_svs->next;
        env_con_fn            = env_con_fn->fun.type2;
        app_count--;
    }
    *var_ast            = *necro_core_ast_create_var(context->arena, env_con_symbol, env_mono_con);
    NecroType* env_type = necro_type_get_fully_applied_fun_type(env_mono_con);
    env_type->ownership = context->base->ownership_share->type;
    env_type->kind      = context->base->star_kind->type;
    return necro_static_value_create_env(context->arena, env_type, a_fn_type, env_con_symbol, fn_symbol, fn_expr_static_value, arg_static_values);
}

///////////////////////////////////////////////////////
// Defunctionalize
///////////////////////////////////////////////////////
// TODO: Refactor lambda_lift to use this function and put this function somewhere useful, perhaps core_ast.c?
void necro_core_ast_lift_point_surgery(NecroCoreAst** lift_point_ref, NecroCoreAst** let_ast_ref)
{
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // And now for some fun in place ast surgery
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    //--------------------
    // 0. Get Current values
    NecroCoreAst* lift_point = *lift_point_ref;
    NecroCoreAst* let_ast    = *let_ast_ref;

    //--------------------
    // 1. Swap contents of ast node with lift_point node
    necro_core_ast_swap(lift_point, let_ast);

    //--------------------
    // 2. Find last lifted let
    NecroCoreAst* last_lifted_let = let_ast;
    assert(last_lifted_let->ast_type == NECRO_CORE_AST_LET);
    while (last_lifted_let->let.expr != NULL)
    {
        assert(last_lifted_let->ast_type == NECRO_CORE_AST_LET);
        last_lifted_let = last_lifted_let->let.expr;
    }

    //--------------------
    // 3. Set expr pointer in last_lifted_let to point to what was the ast node (but now swapped into lift_point)
    last_lifted_let->let.expr = lift_point;

    //--------------------
    // 4. Continue recursing at ast->let.expr (now swapped into ll->lift_point->let.expr)
    *let_ast_ref              = lift_point->let.expr;

    //--------------------
    // 5. Reset lift_point to NULL
    *lift_point_ref           = NULL;
}

void necro_core_ast_lift_point_append(NecroCoreAst** lift_point_ref, NecroCoreAst* lifted_ast)
{
    assert(lifted_ast->ast_type == NECRO_CORE_AST_LET);
    NecroCoreAst* lift_point = *lift_point_ref;
    if (lift_point == NULL)
    {
        *lift_point_ref = lifted_ast;
        return;
    }
    assert(lift_point->ast_type == NECRO_CORE_AST_LET);
    while (lift_point->let.expr != NULL)
    {
        assert(lift_point->ast_type == NECRO_CORE_AST_LET);
        lift_point = lift_point->let.expr;
    }
    lift_point->let.expr = lifted_ast;
}

NecroStaticValue* necro_defunctionalize_let(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LET);

    const bool at_top = context->at_top;
    context->at_top   = false;
    while (ast != NULL)
    {
        // Non-Let Ast
        if (ast->ast_type != NECRO_CORE_AST_LET)
        {
            NecroStaticValue* sv = necro_defunctionalize_go(context, ast);
            if (at_top && context->lift_point != NULL)
                necro_core_ast_lift_point_surgery(&context->lift_point, &ast);
            context->at_top = at_top;
            return sv;
        }
        // HOF binding, prune from ast
        else if (ast->let.bind->ast_type == NECRO_CORE_AST_BIND && !ast->let.bind->bind.ast_symbol->is_constructor && necro_type_is_higher_order_function(ast->let.bind->bind.ast_symbol->type, ast->let.bind->bind.ast_symbol->arity))
        {
            necro_defunctionalize_go(context, ast->let.bind);
            *ast = *ast->let.expr; // Prune from ast
        }
        // Rewrite let f _ = ... in expr ==> expr
        else if (ast->let.bind->ast_type == NECRO_CORE_AST_BIND && ast->let.bind->bind.expr->ast_type == NECRO_CORE_AST_LAM && ast->let.bind->bind.expr->lambda.expr->ast_type != NECRO_CORE_AST_LAM && ast->let.bind->bind.expr->lambda.arg->var.ast_symbol->is_wildcard)
        {
            necro_defunctionalize_go(context, ast->let.bind);
            *ast = *ast->let.expr;
        }
        // Normal Let Ast
        else
        {
            necro_defunctionalize_go(context, ast->let.bind);
            if (at_top && context->lift_point != NULL)
                necro_core_ast_lift_point_surgery(&context->lift_point, &ast);
            else
                ast = ast->let.expr;
        }
    }
    context->at_top = at_top;
    return NULL;
}

NecroStaticValue* necro_defunctionalize_var(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    // _primUndefined
    if (ast->var.ast_symbol == context->base->prim_undefined->core_ast_symbol)
    {
        return necro_static_value_create_dyn(context->arena, ast->necro_type);
    }
    // _proj (Handle with is_primitive instead!?)
    else if (ast->var.ast_symbol == context->base->proj_fn->core_ast_symbol)
    {
        NecroStaticValue* sv = necro_static_value_create_fun(context->arena, ast->necro_type, ast->var.ast_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(ast->necro_type)));
        return necro_static_value_create_env_from_expr(context, ast, sv->necro_type, sv->fun.fn_symbol, sv->fun.expr_static_value);
    }
    // Constructors
    else if (ast->var.ast_symbol->is_constructor)
    {
        assert(ast->necro_type != NULL);
        NecroType* con_type = necro_type_strip_for_all(necro_type_find(ast->necro_type)); // NOTE: The only polymorphic functions which should make it into here are data constructor functions!
        if (con_type->type != NECRO_TYPE_FUN)
            return necro_static_value_create_dyn(context->arena, con_type);
        NecroStaticValue* con_static_value = necro_static_value_create_fun(context->arena, con_type, ast->var.ast_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(con_type)));
        return necro_static_value_create_env_from_expr(context, ast, con_static_value->necro_type, con_static_value->fun.fn_symbol, con_static_value->fun.expr_static_value);
    }
    else
    {
        assert(ast->var.ast_symbol->static_value != NULL);
        NecroStaticValue* var_static_value = ast->var.ast_symbol->static_value;
        if (var_static_value->type == NECRO_STATIC_VALUE_FUN)
            return necro_static_value_create_env_from_expr(context, ast, var_static_value->necro_type, var_static_value->fun.fn_symbol, var_static_value->fun.expr_static_value);
        else
            return ast->var.ast_symbol->static_value;
    }
}

// TODO: Handle static values from data_cons, which can change DataTypes which hold Functions
NecroStaticValue* necro_defunctionalize_data_con(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_DATA_CON);
    ast->data_con.ast_symbol->arity = necro_type_arity(necro_type_find(ast->data_con.type));
    if (ast->data_con.ast_symbol->arity > 0)
        ast->data_con.ast_symbol->static_value = necro_static_value_create_fun(context->arena, ast->data_con.ast_symbol->type, ast->data_con.ast_symbol, necro_static_value_create_dyn(context->arena, ast->data_con.data_type_type));
    else
        ast->data_con.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, ast->data_con.data_type_type);
    return ast->data_con.ast_symbol->static_value;
}

NecroStaticValue* necro_defunctionalize_data_decl(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    // HACK / TODO / NOTE: Removing data_decl defunctionalization for now as it needs more consideration/work and currently its borking things down stream!
    // if (true)
    //     return NULL;
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
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:            return necro_static_value_create_dyn(context->arena, context->base->float_type->type);
    case NECRO_AST_CONSTANT_INTEGER:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:          return necro_static_value_create_dyn(context->arena, context->base->int_type->type);
    case NECRO_AST_CONSTANT_UNSIGNED_INTEGER:
    case NECRO_AST_CONSTANT_UNSIGNED_INTEGER_PATTERN: return necro_static_value_create_dyn(context->arena, context->base->uint_type->type);
    case NECRO_AST_CONSTANT_CHAR:
    case NECRO_AST_CONSTANT_CHAR_PATTERN:             return necro_static_value_create_dyn(context->arena, context->base->char_type->type);
    case NECRO_AST_CONSTANT_STRING:
    {
        // TODO: Better string type handling
        NecroType* arity_type = necro_type_nat_create(context->arena, strlen(ast->lit.string_literal->str) + 1);
        arity_type->kind      = context->base->nat_kind->type;
        NecroType* array_type = necro_type_con2_create(context->arena, context->base->array_type, arity_type, context->base->char_type->type);
        return necro_static_value_create_dyn(context->arena, array_type);
    }
    case NECRO_AST_CONSTANT_ARRAY:
    {
        // TODO: Finish
        NecroCoreAstList* elements = ast->lit.array_literal_elements;
        while (elements != NULL)
        {
            necro_defunctionalize_go(context, elements->data);
            elements = elements->next;
        }
        return necro_static_value_create_dyn(context->arena, ast->necro_type);
        // // TODO: Finish
        // assert(false);
        // return NULL;
    }
    case NECRO_AST_CONSTANT_TYPE_INT:
    default:
        assert(false);
        return NULL;
    }
}

// TODO: Will need to tinker with this to get functions in data structures working correctly in for loops!
NecroStaticValue* necro_defunctionalize_loop(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LOOP);
    // ast->loop.value_pat->var.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, ast->necro_type);
    NecroStaticValue* value_init_static_value         = necro_defunctionalize_go(context, ast->loop.value_init);
    ast->loop.value_pat->var.ast_symbol->static_value = value_init_static_value;
    // UNUSED(value_init_static_value);
    // NOTE: This assumes that these have floated cases from here to the expression!
    if (ast->loop.loop_type == NECRO_LOOP_FOR)
    {
        NecroStaticValue* range_init_static_value                  = necro_defunctionalize_go(context, ast->loop.for_loop.range_init);
        ast->loop.for_loop.index_pat->var.ast_symbol->static_value = range_init_static_value;
        // UNUSED(range_init_static_value);
    }
    else
    {
        NecroStaticValue* while_expression_static_value = necro_defunctionalize_go(context, ast->loop.while_loop.while_expression);
        UNUSED(while_expression_static_value);
    }
    NecroStaticValue* do_expression_static_value = necro_defunctionalize_go(context, ast->loop.do_expression);
    return do_expression_static_value;
}

NecroStaticValue* necro_defunctionalize_bind(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    ast->bind.ast_symbol->ast = ast;

    // Calculate arity
    NecroCoreAst* expr = ast->bind.expr;
    ast->bind.ast_symbol->arity = 0;
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

    // If we're a recursive value, set to dyn
    if (ast->bind.initializer != NULL)
    {
        ast->bind.ast_symbol->static_value = necro_static_value_create_dyn(context->arena, ast->bind.initializer->necro_type);
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
            // TODO: Recalculate arity!?!?!?
            // Seems off here...
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
        necro_type_unify_con_uninhabited_args(context->arena, context->base, ast->bind.ast_symbol->type, static_value->necro_type);
        ast->bind.ast_symbol->type         = static_value->necro_type;
        ast->bind.ast_symbol->static_value = static_value;
    }
    // TODO: Types seem a bit wonky...figure it out!
    // ast->bind.ast_symbol->type = ast->bind.ast_symbol->static_value->necro_type;
    // ast->necro_type = ast->bind.ast_symbol->type;
    return static_value;
}

NecroStaticValue* necro_defunctionalize_lam(NecroDefunctionalizeContext* context, NecroCoreAst* ast)
{
    assert(context != NULL);
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    // Use dyn function for HOF which we don't know anything about, and which must be inlined out later on.
    if (ast->lambda.arg->var.ast_symbol->static_value == NULL)
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
        //     printf("Kaboom?\n");
        //     // assert(false && "TODO: Env HOFs");
        //     // return NULL; // TODO: Inline, look at regular fun HOF applications
        // }
        // else
        // {
            // Saturated, First Order, Env
            NecroCoreAst*         expr_ast    = necro_core_ast_create_var(context->arena, fn_static_value->env.fn_symbol, fn_static_value->env.fn_type);
            NecroStaticValueList* env_arg_svs = fn_static_value->env.arg_static_values;
            // Env proj
            if (env_arg_svs != NULL)
            {
                int64_t index = 0;
                while (env_arg_svs != NULL)
                {
                    NecroType*    proj_type    = necro_type_fn_create(context->arena, fn_static_value->necro_type, necro_type_fn_create(context->arena, context->base->int_type->type, env_arg_svs->data->necro_type));
                    necro_kind_default_type_kinds(context->arena, context->base, proj_type);
                    proj_type->ownership       = context->base->ownership_share->type;
                    NecroCoreAst* proj_var_ast = necro_core_ast_create_var(context->arena, necro_base_get_proj_symbol(context->arena, context->base), proj_type);
                    NecroCoreAst* env_var_ast  = necro_core_ast_create_var(context->arena, var_ast->var.ast_symbol, var_ast->necro_type);
                    NecroCoreAst* index_ast    = necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = index, .type = NECRO_AST_CONSTANT_INTEGER });
                    NecroCoreAst* proj_app_ast = necro_core_ast_create_app(context->arena, proj_var_ast, env_var_ast);
                    proj_app_ast->necro_type   = proj_type->fun.type2;
                    proj_app_ast               = necro_core_ast_create_app(context->arena, proj_app_ast, index_ast);
                    proj_app_ast->necro_type   = proj_type->fun.type2->fun.type2;
                    expr_ast                   = necro_core_ast_create_app(context->arena, expr_ast, proj_app_ast);
                    env_arg_svs                = env_arg_svs->next;
                    index++;
                }
            }
            // Apply App vars, NOTE: Naive app iterations loads args on backwards
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
                apps     = apps->app.expr1;
                arg_i++;
            }
            NecroStaticValue* expr_sv = necro_defunctionalize_go(context, expr_ast);
            *app_ast                  = *expr_ast;
            return expr_sv;
        // }
    }
    // Undersaturated
    else if (difference > 0)
    {
        // TODO: Switch to proj function!
        // Under Saturated, First Order, Env

        NecroCoreAst* expr_ast  = necro_core_ast_create_var(context->arena, fn_static_value->env.fn_symbol, fn_static_value->env.fn_type);
        NecroStaticValueList* env_arg_svs = fn_static_value->env.arg_static_values;
        // Env proj
        if (env_arg_svs != NULL)
        {
            int64_t index = 0;
            while (env_arg_svs != NULL)
            {
                NecroType*    proj_type    = necro_type_fn_create(context->arena, fn_static_value->necro_type, necro_type_fn_create(context->arena, context->base->int_type->type, env_arg_svs->data->necro_type));
                necro_kind_default_type_kinds(context->arena, context->base, proj_type);
                proj_type->ownership       = context->base->ownership_share->type;
                NecroCoreAst* proj_var_ast = necro_core_ast_create_var(context->arena, necro_base_get_proj_symbol(context->arena, context->base), proj_type);
                NecroCoreAst* env_var_ast  = necro_core_ast_create_var(context->arena, var_ast->var.ast_symbol, var_ast->necro_type);
                NecroCoreAst* index_ast    = necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = index, .type = NECRO_AST_CONSTANT_INTEGER });
                NecroCoreAst* proj_app_ast = necro_core_ast_create_app(context->arena, proj_var_ast, env_var_ast);
                proj_app_ast->necro_type   = proj_type->fun.type2;
                proj_app_ast               = necro_core_ast_create_app(context->arena, proj_app_ast, index_ast);
                proj_app_ast->necro_type   = proj_type->fun.type2->fun.type2;
                expr_ast                   = necro_core_ast_create_app(context->arena, expr_ast, proj_app_ast);
                env_arg_svs                = env_arg_svs->next;
                index++;
            }
        }

        // NecroCoreAst* pat_ast   = NULL;
        // NecroType*    env_args  = fn_static_value->necro_type->con.args;
        // bool          is_env_fn = env_args != NULL;
        // // Apply env vars
        // if (is_env_fn)
        // {
        //     NecroType* env_mono_con = unwrap_result(NecroType, necro_type_instantiate(context->arena, NULL, context->base, fn_static_value->env.env_con_symbol->type, NULL));
        //     pat_ast                 = necro_core_ast_create_var(context->arena, fn_static_value->env.env_con_symbol, env_mono_con);
        //     while (env_args != NULL)
        //     {
        //         NecroCoreAstSymbol* env_var_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "free_var"), env_args->list.item);
        //         env_var_symbol->static_value       = necro_static_value_create_dyn(context->arena, env_args->list.item);
        //         NecroCoreAst*       env_pat_var    = necro_core_ast_create_var(context->arena, env_var_symbol, env_args->list.item);
        //         NecroCoreAst*       env_expr_var   = necro_core_ast_create_var(context->arena, env_var_symbol, env_args->list.item);
        //         pat_ast                            = necro_core_ast_create_app(context->arena, pat_ast, env_pat_var);
        //         expr_ast                           = necro_core_ast_create_app(context->arena, expr_ast, env_expr_var);
        //         env_args                           = env_args->list.next;
        //     }
        // }

        // Apply App vars
        NecroCoreAst* apps = app_ast;
        while (apps->ast_type == NECRO_CORE_AST_APP)
        {
            // necro_defunctionalize_go(context, apps->app.expr2); // defunctionalize_go called by necro_static_value_create_env_from_expr
            expr_ast = necro_core_ast_create_app(context->arena, expr_ast, apps->app.expr2);
            apps     = apps->app.expr1;
        }
        // if (is_env_fn)
        // {
        //     // Create case on env
        //     NecroCoreAst* alt_ast = necro_core_ast_create_case_alt(context->arena, pat_ast, expr_ast);
        //     *app_ast              = *necro_core_ast_create_case(context->arena, var_ast, necro_cons_core_ast_list(context->arena, alt_ast, NULL));
        // }
        // else
        // {
            *app_ast = *expr_ast;
        // }
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

// TODO: Rexamine this shit....!
// TODO: This needs to be rexamined ALOT
NecroStaticValue* necro_defunctionalize_app_con(NecroDefunctionalizeContext* context, NecroCoreAst* app_ast, NecroCoreAst* var_ast, NecroStaticValue* fn_static_value)
{
    UNUSED(fn_static_value);
    NecroType*            original_type    = necro_type_find(app_ast->necro_type);
    // Fully Saturated, First Order, Fun, Constructor
    NecroCoreAst*         apps             = app_ast;
    NecroStaticValueList* arg_svs          = NULL;
    //--------------------
    // Go Deeper
    NecroType* con_fn_type = necro_type_instantiate(context->arena, NULL, context->base, var_ast->var.ast_symbol->type, NULL, zero_loc, zero_loc);
    while (apps->ast_type == NECRO_CORE_AST_APP)
    {
        // applied_
        arg_svs = necro_cons_static_value_list(context->arena, necro_defunctionalize_go(context, apps->app.expr2), arg_svs);
        // NecroType* app_type = necro_type_find(apps->necro_type);
        // assert(app_type->type == NECRO_TYPE_FUN);
        // app_type->fun.type1 = arg_svs->data->necro_type;
        if (arg_svs->data->type == NECRO_STATIC_VALUE_FUN)
        {
            assert(false && "TODO: Finish HOF Constructors!");
            // assert(apps->app.expr2->ast_type == NECRO_CORE_AST_VAR);
            // arg_svs->data = necro_static_value_create_env_from_expr(context, apps->app.expr2, arg_svs->data->necro_type, arg_svs->data->fun.fn_symbol, arg_svs->data->fun.expr_static_value);
        }
        // arg_types = necro_type_list_create(context->arena, arg_svs->data->necro_type, arg_types); // TODO / NOTE: This is NOT how things work, con fns don't map 1:1 onto type vars!!!!
        // con       = necro_type_fn_create(context->arena, arg_svs->data->necro_type, con);
        // con->kind = context->base->star_kind->type;
        apps = apps->app.expr1;
    }
    //--------------------
    // Unify inhabited types
    NecroStaticValueList* args_svs_type_go = arg_svs;
    NecroType*            con_fn_type_go   = con_fn_type;
    while (args_svs_type_go != NULL)
    {
        assert(con_fn_type_go->type == NECRO_TYPE_FUN);
        unwrap(NecroType, necro_type_unify(context->arena, NULL, context->base, args_svs_type_go->data->necro_type, con_fn_type_go->fun.type1, NULL));
        args_svs_type_go = args_svs_type_go->next;
        con_fn_type_go   = con_fn_type_go->fun.type2;
    }
    app_ast->necro_type         = necro_type_find(necro_type_get_fully_applied_fun_type(necro_type_find(con_fn_type_go)));
    NecroType* constructed_type = app_ast->necro_type;
    necro_type_unify_con_uninhabited_args(context->arena, context->base, original_type, constructed_type);
    // TODO: Do we reverse or not!?!?!?!?!?!?!?
    // arg_svs = necro_reverse_static_value_list(context->arena, arg_svs);
    // TODO: How to handle something like: data TwoFnsOneVar a = LeftFn a | RightFn a -- Where each side gets a different Env types...
    // Also, Two Maybes with two different Env types.
    // These all stem from returning from funtions from case.
    // Thus we do the branch trick to all inner Envs, but then we need to open them up and apply Branch1_1 and Branch1_2 to each contained Env, necessitates copying data though.
    // TODO: uvars...
    // constructed_type->con.args = arg_types;
    // var_ast->necro_type = con;
    return necro_static_value_create_con(context->arena, constructed_type, var_ast->var.ast_symbol, con_fn_type, arg_svs);
}

NecroCoreAst* necro_defunctionalize_gen_subs_from_args(NecroDefunctionalizeContext* context, NecroCoreAst* lam_ast, NecroCoreAst* app_ast, NecroCoreAstSymbolSubList** subs_ref)
{
    if (app_ast->ast_type != NECRO_CORE_AST_APP)
        return lam_ast;
    NecroCoreAst*       new_lam_ast       = necro_defunctionalize_gen_subs_from_args(context, lam_ast, app_ast->app.expr1, subs_ref);
    NecroStaticValue*   arg_static_value  = necro_defunctionalize_go(context, app_ast->app.expr2);
    assert(new_lam_ast->ast_type == NECRO_CORE_AST_LAM);
    NecroCoreAstSymbol* symbol_to_replace = new_lam_ast->lambda.arg->var.ast_symbol;
    // TODO: Set Types correctly!
    switch (arg_static_value->type)
    {
    case NECRO_STATIC_VALUE_DYN:
    {
        // No Subs
        break;
    }
    case NECRO_STATIC_VALUE_FUN:
    {
        // Drop lambda, Create fn sub
        NecroType*    fn_type = app_ast->app.expr2->necro_type;
        *app_ast              = *app_ast->app.expr1; // Drop arg
        NecroCoreAst* fn_var  = necro_core_ast_create_var(context->arena, arg_static_value->fun.fn_symbol, fn_type);
        *subs_ref             = necro_cons_core_ast_symbol_sub_list(context->arena, (NecroCoreAstSymbolSub) { .symbol_to_replace = symbol_to_replace, .new_ast = fn_var, .new_lambda_var = NULL }, *subs_ref);
        break;
    }
    case NECRO_STATIC_VALUE_CON:
    {
        // No Subs?
        // // Drop lambda, Create con sub
        // NecroType*    fn_type    = app_ast->app.expr2->necro_type;
        // *app_ast                 = *app_ast->app.expr1; // Drop arg
        // NecroCoreAst* con_fn_var = necro_core_ast_create_var(context->arena, arg_static_value->con.con_symbol, fn_type);
        // *subs_ref                = necro_cons_core_ast_symbol_sub_list(context->arena, (NecroCoreAstSymbolSub) { .symbol_to_replace = symbol_to_replace, .new_ast = con_fn_var, .new_lambda_var = NULL }, *subs_ref);
        break;
    }
    case NECRO_STATIC_VALUE_ENV:
    {
        if (arg_static_value->env.arg_static_values == NULL)
        {
            // Env0, Drop Lambda, Create fn sub
            *app_ast             = *app_ast->app.expr1; // Drop arg
            NecroCoreAst* fn_var = necro_core_ast_create_var(context->arena, arg_static_value->env.fn_symbol, arg_static_value->env.fn_type);
            *subs_ref            = necro_cons_core_ast_symbol_sub_list(context->arena, (NecroCoreAstSymbolSub) { .symbol_to_replace = symbol_to_replace, .new_ast = fn_var, .new_lambda_var = NULL }, *subs_ref);
        }
        else
        {
            // But how to handle hof forwarding to be used yet again? I guess the defunctionalization pass will handle that, not the most efficient but....it should work?
            // EnvN, create fn sub and create env unwrapping code
            NecroCoreAstSymbol*   env_var_sym = necro_core_ast_symbol_create_by_renaming(context->arena, necro_intern_unique_string(context->intern, new_lam_ast->lambda.arg->var.ast_symbol->name->str), new_lam_ast->lambda.arg->var.ast_symbol);
            // env_var_sym->type                 = app_ast->app.expr2->necro_type;
            env_var_sym->type                 = arg_static_value->necro_type;
            env_var_sym->static_value         = arg_static_value;
            NecroCoreAst*         env_var     = necro_core_ast_create_var(context->arena, env_var_sym, env_var_sym->type);
            NecroCoreAst*         fn_ast      = necro_core_ast_create_var(context->arena, arg_static_value->env.fn_symbol, arg_static_value->env.fn_type);
            size_t                arg_num     = 0;
            NecroStaticValueList* env_arg_svs = arg_static_value->env.arg_static_values;
            while (env_arg_svs != NULL)
            {
                // Create Env projection
                NecroCoreAst* proj_slot    = necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = arg_num, .type = NECRO_AST_CONSTANT_INTEGER });
                proj_slot->necro_type      = context->base->int_type->type;
                NecroType*    proj_type    = necro_type_fn_create(context->arena, env_var->necro_type, necro_type_fn_create(context->arena, proj_slot->necro_type, env_arg_svs->data->necro_type));
                unwrap_result(void, necro_kind_infer_default(context->arena, context->base, proj_type, zero_loc, zero_loc));
                NecroCoreAst* proj_var     = necro_core_ast_create_var(context->arena, necro_base_get_proj_symbol(context->arena, context->base), proj_type);
                NecroCoreAst* proj_app_env = necro_core_ast_create_app(context->arena, proj_var, env_var);
                proj_app_env->necro_type   = proj_var->necro_type->fun.type2;
                NecroCoreAst* proj_expr    = necro_core_ast_create_app(context->arena, proj_app_env, proj_slot);
                proj_expr->necro_type      = proj_app_env->necro_type->fun.type2;
                // Apply to fn_ast
                NecroType*    fn_ast_type  = necro_type_find(fn_ast->necro_type);
                assert(fn_ast_type->type == NECRO_TYPE_FUN);
                fn_ast                     = necro_core_ast_create_app(context->arena, fn_ast, proj_expr);
                fn_ast->necro_type         = fn_ast_type->fun.type2;
                fn_ast->necro_type->kind   = fn_ast_type->kind;
                env_arg_svs                = env_arg_svs->next;
                arg_num++;
            }
            *subs_ref = necro_cons_core_ast_symbol_sub_list(context->arena, (NecroCoreAstSymbolSub) { .symbol_to_replace = symbol_to_replace, .new_ast = fn_ast, .new_lambda_var = env_var }, *subs_ref);
        }
        break;
    }
    default:
        assert(false);
        break;
    }
    return new_lam_ast->lambda.expr;
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
            // Fully Saturated, Higher Order, Fun
            // TODO: Cache function somewhere so we don't duplicate functions with same signature!
            assert(var_ast->var.ast_symbol->ast != NULL);
            assert(var_ast->var.ast_symbol->ast->ast_type == NECRO_CORE_AST_BIND);
            // Gen subs from args
            NecroCoreAstSymbolSubList* subs = NULL;
            necro_defunctionalize_gen_subs_from_args(context, var_ast->var.ast_symbol->ast->bind.expr, app_ast, &subs);
            // Specialize/Defunctionalize function
            NecroCoreAst* new_bind    = necro_hof_cache_get(&context->hof_cache, var_ast->var.ast_symbol, subs);
            const bool    should_lift = new_bind == NULL;
            if (new_bind == NULL)
            {
                new_bind = necro_core_ast_duplicate_with_subs(context->arena, context->intern, var_ast->var.ast_symbol->ast, subs);
                necro_hof_cache_insert(&context->hof_cache, var_ast->var.ast_symbol, subs, new_bind);
            }
            NecroStaticValue* result_static_value = necro_defunctionalize_go(context, new_bind);
            if (necro_type_find(new_bind->necro_type)->type == NECRO_TYPE_FUN)
            {
                // Apply args to specialized function, then hoist specialized function to top
                // App var location can shift around due to inlining, make sure replace the current app var
                NecroCoreAst* curr_var_ast = app_ast;
                while (curr_var_ast->ast_type == NECRO_CORE_AST_APP)
                {
                    curr_var_ast = curr_var_ast->app.expr1;
                }
                assert(curr_var_ast->ast_type == NECRO_CORE_AST_VAR);
                *curr_var_ast = *necro_core_ast_create_var(context->arena, new_bind->bind.ast_symbol, new_bind->necro_type);
                if (should_lift)
                    necro_core_ast_lift_point_append(&context->lift_point, necro_core_ast_create_let(context->arena, new_bind, NULL));
            }
            else
            {
                // It's a value, replace entire app_ast with inlined value
                *app_ast = *new_bind->bind.expr;
                assert(new_bind->bind.initializer == NULL);
            }
            return result_static_value;
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
        // Go deeper on args
        // NecroCoreAst* apps = app_ast;
        // while (apps->ast_type == NECRO_CORE_AST_APP)
        // {
        //     necro_defunctionalize_go(context, apps->app.expr2);
        //     apps = apps->app.expr1;
        // }
        // necro_static_value_create_env_from_expr calls defunctionalize_go on all arguments
        return necro_static_value_create_env_from_expr(context, app_ast, fn_static_value->necro_type, fn_static_value->fun.fn_symbol, fn_static_value->fun.expr_static_value);
    }
    // Oversaturated
    else
    {
        // Transform: f x y z ==> let temp_aa1 = f x in temp_aa1 y z
        NecroCoreAst* ast1      = app_ast;
        int32_t       curr_diff = difference;
        while (curr_diff < 0)
        {
            assert(ast1->ast_type == NECRO_CORE_AST_APP);
            curr_diff++;
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
    NecroStaticValue* fn_static_value = NULL;
    // _primUndefined
    if (app->var.ast_symbol == context->base->prim_undefined->core_ast_symbol)
    {
        assert(false);
    }
    // _proj
    else if (app->var.ast_symbol == context->base->proj_fn->core_ast_symbol)
    {
        assert(ast->ast_type == NECRO_CORE_AST_APP);
        assert(ast->app.expr1->ast_type == NECRO_CORE_AST_APP);
        assert(ast->app.expr1->app.expr1->ast_type == NECRO_CORE_AST_VAR);
        NecroStaticValue* con_sv    = necro_defunctionalize_go(context, ast->app.expr1->app.expr2);
        NecroCoreAst*     index_ast = ast->app.expr2;
        assert(index_ast->ast_type == NECRO_CORE_AST_LIT);
        assert(index_ast->lit.type == NECRO_AST_CONSTANT_INTEGER);
        const size_t      index     = (size_t) index_ast->lit.int_literal;
        if (con_sv->type == NECRO_STATIC_VALUE_CON || con_sv->type == NECRO_STATIC_VALUE_ENV)
        {
            NecroStaticValueList* args = (con_sv->type == NECRO_STATIC_VALUE_CON) ? con_sv->con.args : con_sv->env.arg_static_values;
            for (size_t i = 0; i < index; ++i)
            {
                assert(args != NULL);
                args = args->next;
            }
            NecroStaticValue* arg_sv = args->data;
            return arg_sv;
        }
        else
        {
            // fn_static_value = necro_static_value_create_fun(context->arena, app->necro_type, app->var.ast_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(app->necro_type)));
            return necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(app->necro_type));
        }
    }
    // Constructors
    else if (app->var.ast_symbol->is_constructor)
    {
        assert(app->necro_type != NULL);
        NecroType* con_type = necro_type_strip_for_all(necro_type_find(app->necro_type)); // NOTE: The only polymorphic functions which should make it into here are data constructor functions!
        if (con_type->type == NECRO_TYPE_FUN)
            fn_static_value = necro_static_value_create_fun(context->arena, con_type, app->var.ast_symbol, necro_static_value_create_dyn(context->arena, necro_type_get_fully_applied_fun_type(con_type)));
        else
            fn_static_value = necro_static_value_create_dyn(context->arena, con_type);
    }
    else
    {

        assert(app->var.ast_symbol->static_value != NULL);
        fn_static_value = app->var.ast_symbol->static_value;
    }
    switch (fn_static_value->type)
    {
    case NECRO_STATIC_VALUE_FUN: return necro_defunctionalize_app_fun(context, ast, app, fn_static_value, app_count);
    case NECRO_STATIC_VALUE_ENV: return necro_defunctionalize_app_env(context, ast, app, fn_static_value, app_count);
    default:                     assert(false); return NULL;
    }
}

// TODO: Case really needs to be looked at again, this seems to fundamentally mishandle monomorphic and polymorphic types!
//--------------------
// Case
//--------------------
// TODO: Remove Keep / Eliminate system entirely!
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
            if (var_ast->var.ast_symbol != expr_sv->con.con_symbol)
            {
                // Different constructor
                while (ast->ast_type == NECRO_CORE_AST_APP)
                {
                    if (necro_defunctionalize_case_pat(context, ast->app.expr2, necro_static_value_create_dyn(context->arena, ast->app.expr2->necro_type)) == NECRO_CASE_ALT_ELIMINATE)
                        return NECRO_CASE_ALT_ELIMINATE;
                    ast = ast->app.expr1;
                }
                assert(ast->ast_type == NECRO_CORE_AST_VAR);
                return NECRO_CASE_ALT_KEEP;
            }
            else
            {
                // Matching Constructor
                NecroStaticValueList* rev_con_args = necro_reverse_static_value_list(context->arena, expr_sv->con.args); // TODO: A Better way
                while (ast->ast_type == NECRO_CORE_AST_APP)
                {
                    if (necro_defunctionalize_case_pat(context, ast->app.expr2, rev_con_args->data) == NECRO_CASE_ALT_ELIMINATE)
                        return NECRO_CASE_ALT_ELIMINATE;
                    rev_con_args = rev_con_args->next;
                    ast = ast->app.expr1;
                }
                assert(ast->ast_type == NECRO_CORE_AST_VAR);
                ast->necro_type = expr_sv->con.con_fn_type;
                return NECRO_CASE_ALT_KEEP;
            }
        }

        case NECRO_STATIC_VALUE_DYN:
        {
            // NecroType*   arg_types = expr_sv->dyn.type->con.args;
            // const size_t arg_count = necro_type_list_count(arg_types);
            // size_t       arg_i     = 0;
            while (ast->ast_type == NECRO_CORE_AST_APP)
            {
                // HACK: Kinda lame...
                // NecroType* arg_type = arg_types;
                // for (size_t i = arg_i + 1; i < arg_count; ++i)
                //     arg_type = arg_type->list.next;
                // arg_type = arg_type->list.item;
                // necro_defunctionalize_case_pat(context, ast->app.expr2, necro_static_value_create_dyn(context->arena, arg_type));
                necro_defunctionalize_case_pat(context, ast->app.expr2, necro_static_value_create_dyn(context->arena, ast->app.expr2->necro_type));
                // rev_con_args = rev_con_args->next;
                ast = ast->app.expr1;
                // arg_i++;
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
    case NECRO_CORE_AST_LOOP:      return necro_defunctionalize_loop(context, ast);
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
    NecroDefunctionalizeContext context = necro_defunctionalize_context_create(intern, base, core_ast_arena);
    necro_defunctionalize_go(&context, core_ast_arena->root);
    if ((info.compilation_phase == NECRO_PHASE_DEFUNCTIONALIZATION && info.verbosity > 0) || info.verbosity > 1)
    {
        necro_core_ast_pretty_print(core_ast_arena->root);
    }
    necro_defunctionalize_context_destroy(&context);
}


///////////////////////////////////////////////////////
// NecroHOFCache
///////////////////////////////////////////////////////
size_t necro_hof_cache_hash_sub(NecroCoreAstSymbolSub* sub)
{
    size_t hash = necro_type_hash(sub->new_ast->necro_type) ^ necro_core_ast_hash(sub->new_ast);
    return hash;
}

size_t necro_hof_cache_hash_subs(NecroCoreAstSymbolSubList* subs)
{
    size_t hash = 0;
    while (subs != NULL)
    {
        hash = hash ^ necro_hof_cache_hash_sub(&subs->data);
        subs = subs->next;
    }
    return hash;
}

size_t necro_hof_cache_hash(NecroCoreAstSymbol* fn_symbol, NecroCoreAstSymbolSubList* subs)
{
    return fn_symbol->name->hash ^ necro_hof_cache_hash_subs(subs);
}

bool necro_hof_cache_is_equal_sub(NecroCoreAstSymbolSub* sub1, NecroCoreAstSymbolSub* sub2)
{
    // TODO: necro_core_ast_is_equal!
    return necro_type_exact_unify(sub1->new_ast->necro_type, sub2->new_ast->necro_type);
}

bool necro_hof_cache_is_equal_subs(NecroCoreAstSymbolSubList* subs1, NecroCoreAstSymbolSubList* subs2)
{
    while (subs1 != NULL && subs2 != NULL)
    {
        if (!necro_hof_cache_is_equal_sub(&subs1->data, &subs2->data))
            return false;
        subs1 = subs1->next;
        subs2 = subs2->next;
    }
    return subs1 == NULL && subs2 == NULL;
}

size_t necro_hof_cache_is_equal(NecroCoreAstSymbol* fn_symbol1, NecroCoreAstSymbolSubList* subs1, NecroCoreAstSymbol* fn_symbol2, NecroCoreAstSymbolSubList* subs2)
{
    return fn_symbol1 == fn_symbol2 && necro_hof_cache_is_equal_subs(subs1, subs2);
}

void necro_hof_cache_grow(NecroHOFCache* cache)
{
    NecroHOFBucket* old_buckets  = cache->buckets;
    const size_t    old_count    = cache->count;
    const size_t    old_capacity = cache->capacity;
    cache->capacity              = old_capacity * 2;
    cache->count                 = 0;
    cache->buckets               = emalloc(cache->capacity * sizeof(NecroHOFBucket));
    for (size_t i = 0; i < cache->capacity; ++i)
        cache->buckets[i] = (NecroHOFBucket) { .fn_symbol = NULL, .subs = NULL, .hash = 0, .inst_ast = NULL  };
    for (size_t i = 0; i < old_capacity; ++i)
    {
        if (old_buckets[i].fn_symbol == NULL)
            continue;
        size_t index = old_buckets[i].hash & (cache->capacity - 1);
        while (cache->buckets[index].fn_symbol != NULL)
        {
            index = (index + 1) & (cache->capacity - 1);
        }
        cache->buckets[index] = old_buckets[i];
        cache->count++;
    }
    assert(cache->count == old_count);
    free(old_buckets);
}

NecroCoreAst* necro_hof_cache_get(NecroHOFCache* cache, NecroCoreAstSymbol* fn_symbol, NecroCoreAstSymbolSubList* subs)
{
    assert(cache != NULL);
    assert(fn_symbol != NULL);
    assert(subs != NULL);
    if ((cache->count * 2) >= cache->capacity)
        necro_hof_cache_grow(cache);
    size_t hash  = necro_hof_cache_hash(fn_symbol, subs);
    size_t index = hash & (cache->capacity - 1);
    while (cache->buckets[index].fn_symbol != NULL)
    {
        if (cache->buckets[index].hash == hash && necro_hof_cache_is_equal(fn_symbol, subs, cache->buckets[index].fn_symbol, cache->buckets[index].subs))
        {
           assert(cache->buckets[index].inst_ast != NULL);
           return cache->buckets[index].inst_ast;
        }
        index = (index + 1) & (cache->capacity - 1);
    }
    return NULL;
}

void necro_hof_cache_insert(NecroHOFCache* cache, NecroCoreAstSymbol* fn_symbol, NecroCoreAstSymbolSubList* subs, NecroCoreAst* inst_ast)
{
    assert(cache != NULL);
    assert(fn_symbol != NULL);
    assert(subs != NULL);
    if ((cache->count * 2) >= cache->capacity)
        necro_hof_cache_grow(cache);
    size_t hash  = necro_hof_cache_hash(fn_symbol, subs);
    size_t index = hash & (cache->capacity - 1);
    while (cache->buckets[index].fn_symbol != NULL)
    {
        if (cache->buckets[index].hash == hash && necro_hof_cache_is_equal(fn_symbol, subs, cache->buckets[index].fn_symbol, cache->buckets[index].subs))
        {
            cache->buckets[index].hash      = hash;
            cache->buckets[index].fn_symbol = fn_symbol;
            cache->buckets[index].subs      = subs;
            cache->buckets[index].inst_ast  = inst_ast;
        }
        index = (index + 1) & (cache->capacity - 1);
    }
    cache->buckets[index].hash      = hash;
    cache->buckets[index].fn_symbol = fn_symbol;
    cache->buckets[index].subs      = subs;
    cache->buckets[index].inst_ast  = inst_ast;
    cache->count++;
}


///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_defunctionalize_test_result(const char* test_name, const char* str)
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
    info.verbosity                      = 0;
    info.compilation_phase              = NECRO_PHASE_DEFUNCTIONALIZATION;

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &base, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast));
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));
    necro_core_ast_pre_simplify(info, &intern, &base, &core_ast);
    necro_core_lambda_lift(info, &intern, &base, &core_ast);
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));
    necro_core_defunctionalize(info, &intern, &base, &core_ast);
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));
    necro_core_ast_pre_simplify(info, &intern, &base, &core_ast);

    // necro_core_ast_pretty_print(core_ast.root);

    // Print
    printf("Core %s test: Passed\n", test_name);

    // Clean up
    necro_core_ast_arena_destroy(&core_ast);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}

void necro_core_defunctionalize_test()
{
    necro_announce_phase("Defunctionalize");

/*


*/
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

    // TODO / NOTE: Constructors holding Functions is currently under construction
    // {
    //     const char* test_name   = "Constructor Fn 1";
    //     const char* test_source = ""
    //         "maybeJustMaybe :: Maybe (Int -> Int -> Int)\n"
    //         "maybeJustMaybe = Just add\n";
    //     necro_defunctionalize_test_result(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Constructor Fn 2";
    //     const char* test_source = ""
    //         "maybeJustMaybe :: Maybe (Int -> Int)\n"
    //         "maybeJustMaybe = Just (add 0)\n";
    //     necro_defunctionalize_test_result(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Undersaturated Constructor Fn 1";
    //     const char* test_source = ""
    //         "data Pair a b = Pair a b\n"
    //         "imperfectPair :: Bool -> Pair (Int -> Int) Bool\n"
    //         "imperfectPair = Pair (add 0)\n";
    //     necro_defunctionalize_test_result(test_name, test_source);
    // }

    // {
    //     const char* test_name   = "Undersaturated Constructor Fn 2";
    //     const char* test_source = ""
    //         "data Pair a b = Pair a b\n"
    //         "imperfectPair :: Bool -> Pair (Int -> Int) Bool\n"
    //         "imperfectPair = Pair (add 0)\n"
    //         "perfectPair = imperfectPair False\n";
    //     necro_defunctionalize_test_result(test_name, test_source);
    // }

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

    // TODO / NOTE: Constructors holding Functions is currently under construction
    // {
    //     const char* test_name   = "Case 3-1";
    //     const char* test_source = ""
    //         "maybeNothing =\n"
    //         "  case (eq True, eq False) of\n"
    //         "    (f, g) -> f\n"
    //         "appNothing = maybeNothing True\n";
    //     necro_defunctionalize_test_result(test_name, test_source);
    // }

    // TODO / NOTE: Constructors holding Functions is currently under construction
    // {
    //     const char* test_name   = "Case 3-2";
    //     const char* test_source = ""
    //         "maybeNothing =\n"
    //         "  case Just (eq True) of\n"
    //         "    Just f  -> f False\n"
    //         "    Nothing -> True\n";
    //     necro_defunctionalize_test_result(test_name, test_source);
    // }

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

    {
        const char* test_name   = "Pat Assignment 1";
        const char* test_source = ""
            "unboxedTuple :: (#Bool, Int#)\n"
            "unboxedTuple = (#True, 0#)\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Unboxed Tuple 6";
        const char* test_source = ""
            "data TripleThreat a = TripleThreat (#a, a, a#)\n"
            "tripleThreat :: TripleThreat (Maybe Bool)\n"
            "tripleThreat = TripleThreat (#Nothing, Nothing, Just True#)\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 1";
        const char* test_source = ""
            "intOp :: (Int -> Int -> Int) -> Int -> Int\n"
            "intOp f i = f i i\n"
            "result = intOp add 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 2";
        const char* test_source = ""
            "intOp :: (Int -> Int -> Int) -> Int\n"
            "intOp f = f 22 33\n"
            "result = intOp add\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 3";
        const char* test_source = ""
            "intOp :: (Int -> Int -> Int) -> (Int -> Int -> Int) -> Int -> Int -> Int -> Int\n"
            "intOp f g x y z = g (f x y) z\n"
            "result = intOp add sub 44 55 6\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 4";
        const char* test_source = ""
            "goOp :: Int -> (Int -> Int -> Int) -> Int\n"
            "goOp i f = f i i\n"
            "intOp :: (Int -> Int -> Int) -> Int -> Int\n"
            "intOp f i = goOp i f\n"
            "result = intOp add 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 5";
        const char* test_source = ""
            "intOp :: (Int -> Int -> Int) -> Int -> Int\n"
            "intOp f i = f (f i i) i\n"
            "result = intOp add 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 6";
        const char* test_source = ""
            "conOp :: (Int -> Maybe Int) -> Int -> Maybe Int\n"
            "conOp f i = f (add i i)\n"
            "result = conOp Just 22\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 7";
        const char* test_source = ""
            "data Wrapper a = Wrapper a\n"
            "conOp :: (Int -> Wrapper Int) -> Int -> Wrapper Int\n"
            "conOp f i = f (add i i)\n"
            "result = conOp Wrapper 22\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 8";
        const char* test_source = ""
            "conOp :: (Int -> f Int) -> Int -> f Int\n"
            "conOp f i = f (add i i)\n"
            "result  = conOp Just 22\n"
            "result2 = conOp Mono 33\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 8";
        const char* test_source = ""
            "intOp :: (Int -> Int) -> Int -> Int\n"
            "intOp f i = f (f i)\n"
            "result = intOp (add 2) 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 9";
        const char* test_source = ""
            "sum3 :: Int -> Int -> Int -> Int\n"
            "sum3 x y z = x + y + z\n"
            "intOp :: (Int -> Int) -> Int -> Int\n"
            "intOp f i = f (i + i)\n"
            "result = intOp (sum3 44 55) 1\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 9";
        const char* test_source = ""
            "revTuple :: a -> b -> UInt -> (UInt, b, a)\n"
            "revTuple x y z = (z, y, x)\n"
            "grouper :: (UInt -> (UInt, b, a)) -> UInt -> (UInt, b, a)\n"
            "grouper f i = f (i + i)\n"
            "result = grouper (revTuple () False) 300\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 10";
        const char* test_source = ""
            "revTuple :: a -> b -> UInt -> (UInt, b, a)\n"
            "revTuple x y z = (z, y, x)\n"
            "grouper :: (UInt -> (UInt, b, a)) -> UInt -> (UInt, b, a)\n"
            "grouper f i = f (i + i)\n"
            "forward :: (UInt -> (UInt, b, a)) -> UInt -> (UInt, b, a)\n"
            "forward f i = grouper f (i + i)\n"
            "result = forward (revTuple () False) 300\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Saturated HOF 11";
        const char* test_source = ""
            "addSomeShit3 :: Int -> Int -> Int -> Int\n"
            "addSomeShit3 x y z = x + y + z\n"
            "addSomeShit2 :: (Int -> Int) -> Int -> Int\n"
            "addSomeShit2 f x = f (x * x)\n"
            "addSomeShit1 :: (Int -> Int -> Int) -> Int -> Int\n"
            "addSomeShit1 f x = addSomeShit2 (f 33) x\n"
            "result = addSomeShit1 (addSomeShit3 22) 300\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Turtles all the way down 1";
        const char* test_source = ""
            "deeper :: (Int -> Int) -> Int -> Int\n"
            "deeper f x = f (x * x)\n"
            "turtle :: ((Int -> Int) -> Int -> Int) -> Int -> Int\n"
            "turtle f x = f (add 44) x\n"
            "result = turtle deeper 300\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "SeqValue 1";
        const char* test_source = ""
            "seqValueTest1 :: SeqValue Int -> SeqValue Int\n"
            "seqValueTest1 x = map id x\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "SeqValue 2";
        const char* test_source = ""
            "seqValueTest2 :: SeqValue Int -> SeqValue Int -> SeqValue Int\n"
            "seqValueTest2 x y = map2 add x y\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "SeqValue 3";
        const char* test_source = ""
            "seqValueTest3 :: SeqValue Int -> SeqValue Int\n"
            "seqValueTest3 x = x * 3\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "SeqValue 4";
        const char* test_source = ""
            "seqValueTest4 :: SeqValue Float -> SeqValue Float\n"
            "seqValueTest4 x = x / 2\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 1";
        const char* test_source = ""
            "seqTest :: Seq Bool\n"
            "seqTest = pure True\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 2";
        const char* test_source = ""
            "seqTest :: Seq Bool\n"
            "seqTest = pure True\n"
            "seqGo :: SeqValue Bool\n"
            "seqGo = runSeq seqTest ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 3";
        const char* test_source = ""
            "seqTest :: Seq Int\n"
            "seqTest = pure 0\n"
            "coolSeq :: Seq Int\n"
            "coolSeq = map (add 1) seqTest\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq seqTest ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "HOF Fun 1";
        const char* test_source = ""
            "doubleUpInt :: (Int -> Int) -> Int -> Int\n"
            "doubleUpInt f x = f x + f x\n"
            "doubleDownInt :: (Int -> Int -> Int) -> Int -> Int\n"
            "doubleDownInt f x = f x x + f x x\n"
            "integrity :: Int -> Int\n"
            "integrity i = doubleUpInt (doubleDownInt add) i\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "HOF Fun 2";
        const char* test_source = ""
            "doubleUpInt :: (Int -> Int) -> Int -> Int\n"
            "doubleUpInt f x = f x + f x\n"
            "doubleDownInt :: (Int -> Int) -> Int -> Int\n"
            "doubleDownInt f x = f x + f x\n"
            "integrity :: Int -> Int\n"
            "integrity i = doubleUpInt (doubleDownInt (add 666)) i\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 4";
        const char* test_source = ""
            "seqTest :: Seq Int\n"
            "seqTest = pure 0\n"
            "coolSeq :: Seq Int\n"
            "coolSeq = map (add 1) seqTest\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq coolSeq ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 5";
        const char* test_source = ""
            "seqTest :: Seq Int\n"
            "seqTest = pure 0\n"
            "coolSeq :: Seq Int\n"
            "coolSeq = map2 add seqTest seqTest\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq coolSeq ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 6";
        const char* test_source = ""
            "coolSeq :: Seq Int\n"
            "coolSeq = 666 * 22 + 33\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq coolSeq ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 7";
        const char* test_source = ""
            "coolSeq :: Seq Float\n"
            "coolSeq = 77.7 - 666 * 22 + fromInt mouseX * 4 - 256 * 10 + 33.3\n"
            "seqGo :: SeqValue Float\n"
            "seqGo = runSeq coolSeq ()\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Seq 8";
        const char* test_source = ""
            "coolSeq :: Seq Int\n"
            "coolSeq = 666 * 22 + 3 * 4 - 256 * 10\n"
            "seqGo :: SeqValue Int\n"
            "seqGo = runSeq (coolSeq + coolSeq) ()\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print [11 22 _ <4 5 6>] w\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Print Seq 2";
        const char* test_source = ""
            "main :: *World -> *World\n"
            "main w = print ([11 12 13] + [<21 22> <23 24 25> 26]) w\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

/*

    {
        const char* test_name   = "Poly 0";
        const char* test_source = ""
            "myCoolSynth :: Audio Mono\n"
            "myCoolSynth = poly saw [440 220 _ <110 55 _ 330>]\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }


    // TODO: Partially applied functions seem broken right now?
    {
        const char* test_name   = "Turtles all the way down 2";
        const char* test_source = ""
            "deeper :: (Int -> Int -> Int) -> Int -> Int\n"
            "deeper f x = f x x\n"
            "turtle :: (Int -> Int) -> Int -> Int\n"
            "turtle f x = f (x + x)\n"
            "result :: Int\n"
            "result = turtle (deeper add) 300\n";
        necro_defunctionalize_test_result(test_name, test_source);
    }

// TODO: FIX, looks like this never got cleaned up or something broke things.
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

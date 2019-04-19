/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "lambda_lift.h"
#include <stdio.h>
#include "type.h"
#include "core_create.h"
#include "hash_table.h"
#include "monomorphize.h"
#include "alias_analysis.h"

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
// NECRO_DECLARE_ARENA_LIST(NecroVar, Var, var);
//
// typedef struct NecroLambdaLiftSymbolInfo
// {
//     NecroVar                          var;
//     NecroVarList*                     free_vars;
//     struct NecroLambdaLiftSymbolInfo* parent;
//     NecroVar                          renamed_var;
// } NecroLambdaLiftSymbolInfo;
// NECRO_DECLARE_VECTOR(NecroLambdaLiftSymbolInfo, NecroLambdaLiftSymbol, lambda_lift_symbol);
// NECRO_DECLARE_ARENA_CHAIN_TABLE(NecroVar, Var, var);
//
// typedef struct NecroLambdaLift
// {
//     NecroPagedArena             arena;
//     NecroSnapshotArena          snapshot_arena;
//     NecroLambdaLiftSymbolVector ll_symtable;
//     NecroIntern*                intern;
//     NecroSymTable*              symtable;
//     NecroScopedSymTable*        scoped_symtable;
//     NecroPrimTypes*             prim_types;
//     NecroCoreAST_Expression*    lift_point;
//     size_t                      num_anon_functions;
//     NecroVarTable               lifted_env;
// } NecroLambdaLift;

NECRO_DECLARE_VECTOR(NecroCoreAstSymbol*, NecroCoreAstSymbol, core_ast_symbol);
typedef struct NecroCoreScopeNode
{
    NecroCoreAstSymbol* ast_symbol;
    NecroCoreAstSymbol* renamed_ast_symbol;
} NecroCoreScopeNode;

typedef struct NecroCoreScope
{
    struct NecroCoreScope* free_vars;
    struct NecroCoreScope* parent;
    NecroCoreScopeNode*    data;
    size_t                 size;
    size_t                 count;
} NecroCoreScope;

typedef struct NecroLambdaLift
{
    NecroPagedArena*         ast_arena;
    NecroPagedArena          ll_arena;
    NecroSnapshotArena       snapshot_arena;
    NecroIntern*             intern;
    NecroBase*               base;
    NecroCoreAst*            lift_point;
    size_t                   clash_suffix;
    NecroCoreScope*          scope;
    NecroCoreScope*          global_scope;
} NecroLambdaLift;

//--------------------
// Forward Declarations
//--------------------
void                necro_core_lambda_lift_go(NecroLambdaLift* ll, NecroCoreAst* ast);
void                necro_core_lambda_lift_pat(NecroLambdaLift* ll, NecroCoreAst* ast);
NecroCoreScope*     necro_core_scope_create(NecroPagedArena* arena, NecroCoreScope* parent);
bool                necro_core_scope_find_in_this_scope(NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol);
NecroCoreAstSymbol* necro_core_scope_find_renamed_in_this_scope(NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol);
void                necro_core_scope_insert(NecroPagedArena* arena, NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol);

//--------------------
// NecroLambdaLift
//--------------------
NecroLambdaLift necro_lambda_lift_empty()
{
    return (NecroLambdaLift)
    {
        .ast_arena          = NULL,
        .ll_arena           = necro_paged_arena_empty(),
        .snapshot_arena     = necro_snapshot_arena_empty(),
        .intern             = NULL,
        .base               = NULL,
        .lift_point         = NULL,
        .clash_suffix       = 0,
        .scope              = NULL,
        .global_scope       = NULL,
        // .free_vars          = necro_empty_core_ast_symbol_vector(),
    };
}

NecroLambdaLift necro_lambda_lift_create(NecroPagedArena* ast_arena, NecroIntern* intern, NecroBase* base)
{
    NecroPagedArena ll_arena     = necro_paged_arena_create();
    NecroCoreScope* global_scope = necro_core_scope_create(&ll_arena, NULL);
    return (NecroLambdaLift)
    {
        .ast_arena      = ast_arena,
        .ll_arena       = ll_arena,
        .snapshot_arena = necro_snapshot_arena_create(),
        .intern         = intern,
        .base           = base,
        .lift_point     = NULL,
        .clash_suffix   = 0,
        .scope          = global_scope,
        .global_scope   = global_scope,
        // .free_vars          = necro_create_core_ast_symbol_vector(),
    };
}

void necro_lambda_lift_destroy(NecroLambdaLift* ll)
{
    necro_paged_arena_destroy(&ll->ll_arena);
    necro_snapshot_arena_destroy(&ll->snapshot_arena);
    // necro_destroy_core_ast_symbol_vector(&ll->free_vars);
    *ll = necro_lambda_lift_empty();
}

void necro_core_lambda_lift(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroCoreAstArena* core_ast_arena)
{
    UNUSED(info);
    NecroLambdaLift ll = necro_lambda_lift_create(&core_ast_arena->arena, intern, base);
    necro_core_lambda_lift_go(&ll, core_ast_arena->root);
    necro_lambda_lift_destroy(&ll);
}

///////////////////////////////////////////////////////
// NecroCoreScope
///////////////////////////////////////////////////////
#define NECRO_CORE_SCOPE_INITIAL_SIZE 4 // Change this to 8 from 4

NecroCoreScope* necro_core_scope_create(NecroPagedArena* arena, NecroCoreScope* parent)
{
    NecroCoreScope* scope = necro_paged_arena_alloc(arena, sizeof(NecroCoreScope));
    scope->free_vars      = NULL;
    scope->parent         = parent;
    scope->data           = necro_paged_arena_alloc(arena, NECRO_CORE_SCOPE_INITIAL_SIZE * sizeof(NecroCoreScopeNode));
    scope->size           = NECRO_CORE_SCOPE_INITIAL_SIZE;
    scope->count          = 0;
    for (size_t slot = 0; slot < scope->size; ++slot)
        scope->data[slot] = (NecroCoreScopeNode) { .ast_symbol = NULL, .renamed_ast_symbol = NULL };
    return scope;
}

inline void necro_core_scope_grow(NecroPagedArena* arena, NecroCoreScope* scope)
{
    assert(scope != NULL);
    assert(scope->data != NULL);
    assert(scope->count < scope->size);
    NecroCoreScopeNode* prev_data  = scope->data;
    size_t              prev_size  = scope->size;
    size_t              prev_count = scope->count;
    scope->size                   *= 2;
    scope->data                    = necro_paged_arena_alloc(arena, scope->size * sizeof(NecroCoreScopeNode));
    scope->count                   = 0;
    for (size_t slot = 0; slot < scope->size; ++slot)
        scope->data[slot] = (NecroCoreScopeNode) { .ast_symbol = NULL, .renamed_ast_symbol = NULL };
    for (size_t prev_slot = 0; prev_slot < prev_size; ++prev_slot)
    {
        NecroCoreAstSymbol* ast_symbol         = prev_data[prev_slot].ast_symbol;
        NecroCoreAstSymbol* renamed_ast_symbol = prev_data[prev_slot].renamed_ast_symbol;
        if (ast_symbol == NULL)
            continue;
        NecroSymbol symbol = ast_symbol->name;
        size_t      slot   = symbol->hash & (scope->size - 1);
        while (scope->data[slot].ast_symbol != NULL)
        {
            assert (scope->data[slot].ast_symbol->name != symbol);
            slot = (slot + 1) & (scope->size - 1);
        }
        scope->data[slot].ast_symbol         = ast_symbol;
        scope->data[slot].renamed_ast_symbol = renamed_ast_symbol;
        scope->count++;
    }
    assert(scope->count == prev_count);
}

void necro_core_scope_insert(NecroPagedArena* arena, NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol)
{
    assert(scope != NULL);
    assert(scope->data != NULL);
    assert(scope->count < scope->size);
    NecroSymbol symbol = ast_symbol->name;
    if (scope->count >= (scope->size / 2))
        necro_core_scope_grow(arena, scope);
    size_t slot = symbol->hash & (scope->size - 1);
    while (scope->data[slot].ast_symbol != NULL)
    {
        if (scope->data[slot].ast_symbol->name == symbol)
            return;
        slot = (slot + 1) & (scope->size - 1);
    }
    scope->data[slot].ast_symbol         = ast_symbol;
    scope->data[slot].renamed_ast_symbol = NULL;
    scope->count++;
}

void necro_core_scope_insert_with_renamed(NecroPagedArena* arena, NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol, NecroCoreAstSymbol* renamed_ast_symbol)
{
    assert(scope != NULL);
    assert(scope->data != NULL);
    assert(scope->count < scope->size);
    NecroSymbol symbol = ast_symbol->name;
    if (scope->count >= (scope->size / 2))
        necro_core_scope_grow(arena, scope);
    size_t slot = symbol->hash & (scope->size - 1);
    while (scope->data[slot].ast_symbol != NULL)
    {
        if (scope->data[slot].ast_symbol->name == symbol)
            return;
        slot = (slot + 1) & (scope->size - 1);
    }
    scope->data[slot].ast_symbol         = ast_symbol;
    scope->data[slot].renamed_ast_symbol = renamed_ast_symbol;
    scope->count++;
}

bool necro_core_scope_find_in_this_scope(NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol)
{
    assert(scope != NULL);
    assert(scope->data != NULL);
    assert(scope->count < scope->size);
    NecroSymbol symbol = ast_symbol->name;
    for (size_t slot = symbol->hash & (scope->size - 1); scope->data[slot].ast_symbol != NULL; slot = (slot + 1) & (scope->size - 1))
    {
        if (scope->data[slot].ast_symbol->name == symbol)
            return true;
    }
    return false;
}

NecroCoreAstSymbol* necro_core_scope_find_renamed_in_this_scope(NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol)
{
    assert(scope != NULL);
    assert(scope->data != NULL);
    assert(scope->count < scope->size);
    NecroSymbol symbol = ast_symbol->name;
    for (size_t slot = symbol->hash & (scope->size - 1); scope->data[slot].ast_symbol != NULL; slot = (slot + 1) & (scope->size - 1))
    {
        if (scope->data[slot].ast_symbol->name == symbol)
            return scope->data[slot].renamed_ast_symbol;
    }
    return NULL;
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroCoreAstSymbol* necro_core_lambda_lift_create_renamed_symbol(NecroLambdaLift* ll, NecroCoreAstSymbol* ast_symbol)
{
    NecroSymbol renamed_symbol = necro_intern_unique_string(ll->intern, ast_symbol->name->str, &ll->clash_suffix);
    return necro_core_ast_symbol_create_by_renaming(ll->ast_arena, renamed_symbol, ast_symbol);
}

void necro_core_lambda_lift_push_scope(NecroLambdaLift* ll)
{
    ll->scope            = necro_core_scope_create(&ll->ll_arena, ll->scope);
    ll->scope->free_vars = necro_core_scope_create(&ll->ll_arena, NULL);
}

void necro_core_lambda_lift_pop_scope(NecroLambdaLift* ll)
{
    // Float free vars not contained in parent scope up into parent free vars
    for (size_t i = 0; i < ll->scope->free_vars->size; ++i)
    {
        NecroCoreAstSymbol* ast_symbol = ll->scope->free_vars->data[i].ast_symbol;
        if (ast_symbol == NULL || necro_core_scope_find_in_this_scope(ll->scope->parent, ast_symbol))
            continue;
        // Rename and insert
        NecroCoreAstSymbol* renamed = necro_core_lambda_lift_create_renamed_symbol(ll, ast_symbol);
        necro_core_scope_insert_with_renamed(&ll->ll_arena, ll->scope->parent->free_vars, ast_symbol, renamed);
    }
    // Pop scope
    ll->scope = ll->scope->parent;
}

inline bool necro_core_lambda_lift_is_free_var(NecroLambdaLift* ll, NecroCoreAstSymbol* ast_symbol)
{
    return !(necro_core_scope_find_in_this_scope(ll->scope, ast_symbol) || necro_core_scope_find_in_this_scope(ll->global_scope, ast_symbol));
}

bool necro_core_lambda_lift_is_bind_fn(NecroCoreAst* ast)
{
    if (ast->ast_type == NECRO_CORE_AST_BIND)
    {
        return ast->bind.expr->ast_type == NECRO_CORE_AST_LAM;
    }
    else if (ast->ast_type == NECRO_CORE_AST_DATA_DECL)
    {
        return false;
    }
    else
    {
        assert(false);
        return false;
    }
}

///////////////////////////////////////////////////////
// LambdaLift
///////////////////////////////////////////////////////
void necro_core_lambda_lift_app(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    necro_core_lambda_lift_go(ll, ast->app.expr1);
    necro_core_lambda_lift_go(ll, ast->app.expr2);
}

void necro_core_lambda_lift_for(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_FOR);
    necro_core_lambda_lift_go(ll, ast->for_loop.range_init);
    necro_core_lambda_lift_go(ll, ast->for_loop.value_init);
    necro_core_lambda_lift_go(ll, ast->for_loop.index_arg);
    necro_core_lambda_lift_go(ll, ast->for_loop.value_arg);
    necro_core_lambda_lift_go(ll, ast->for_loop.expression);
}

void necro_core_lambda_lift_case(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_CASE);
    necro_core_lambda_lift_go(ll, ast->case_expr.expr);
    NecroCoreAstList* alts = ast->case_expr.alts;
    while (alts != NULL)
    {
        necro_core_lambda_lift_pat(ll, alts->data->case_alt.pat);
        necro_core_lambda_lift_go(ll, alts->data->case_alt.expr);
        alts = alts->next;
    }
}

void necro_core_lambda_lift_var(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_VAR);
    if (ast->var.ast_symbol == ll->base->prim_undefined->core_ast_symbol)
        return;
    //--------------------
    // If free var, add to free_var list, and switch var to use renamed symbol
    if (!ast->var.ast_symbol->is_constructor && necro_core_lambda_lift_is_free_var(ll, ast->var.ast_symbol))
    {
        NecroCoreAstSymbol* renamed = necro_core_scope_find_renamed_in_this_scope(ll->scope->free_vars, ast->var.ast_symbol);
        if (renamed == NULL)
        {
            renamed = necro_core_lambda_lift_create_renamed_symbol(ll, ast->var.ast_symbol);
            necro_core_scope_insert_with_renamed(&ll->ll_arena, ll->scope->free_vars, ast->var.ast_symbol, renamed);
        }
        ast->var.ast_symbol = renamed;
    }
    //--------------------
    // Apply free_vars
    NecroCoreScope* free_vars = ast->var.ast_symbol->free_vars;
    if (free_vars == NULL || free_vars->count == 0)
        return;
    assert(free_vars != NULL);
    for (size_t i = 0; i < free_vars->size; ++i)
    {
        NecroCoreAstSymbol* ast_symbol = free_vars->data[i].ast_symbol;
        if (ast_symbol == NULL)
            continue;
        NecroCoreAst* free_var = necro_core_ast_create_var(ll->ast_arena, ast_symbol);
        necro_core_lambda_lift_var(ll, free_var);
        // Apply
        NecroCoreAst* app_ast = necro_core_ast_create_app(ll->ast_arena, NULL, free_var);
        // In place swap
        NecroCoreAst  temp = *ast;
        *ast               = *app_ast;
        *app_ast           = temp;
        ast->app.expr1     = app_ast;
    }
}

// TODO: We're going to blow the stack if we recurse like this
void necro_core_lambda_lift_let(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LET);
    while (ast != NULL)
    {
        if (ast->ast_type != NECRO_CORE_AST_LET)
        {
            necro_core_lambda_lift_go(ll, ast);
            return;
        }
        // NecroCoreAst* prev_lift_point = ll->lift_point;
        necro_core_lambda_lift_go(ll, ast->let.bind);
        if (necro_core_lambda_lift_is_bind_fn(ast->let.bind) && ll->scope->parent != NULL)
        {
            // We lifted a function out, so we need to collapse let ast
            *ast = *ast->let.expr;
        }
        else if (ll->lift_point != NULL && ll->scope->parent == NULL)
        {
            // We're a top level ast node and at some point in bind we lifted a function.
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // And now for some in place Ast surgery
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // 1. Swap contents of ast node with lift_point node
            NecroCoreAst temp = *ll->lift_point;
            *ll->lift_point   = *ast;
            *ast              = temp;
            // 2. Find last lifted let
            NecroCoreAst* last_lifted_let = ast;
            assert(last_lifted_let->ast_type == NECRO_CORE_AST_LET);
            while (last_lifted_let->let.expr != NULL)
            {
                assert(last_lifted_let->ast_type == NECRO_CORE_AST_LET);
                last_lifted_let = last_lifted_let->let.expr;
            }
            // 3. Set expr pointer in last_lifted_let to point to what was the ast node (but now swapped into lift_point)
            last_lifted_let->let.expr = ll->lift_point;
            // 4. Continue recursing at ast->let.expr (now swapped into ll->lift_point->let.expr)
            ast                       = ll->lift_point->let.expr;
            // 5. Reset lift_point to NULL
            ll->lift_point            = NULL;
        }
        else
        {
            // We're a normal let binding with no funny business
            ast = ast->let.expr;
        }
    }
}

void necro_core_lambda_lift_bind(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    if (ll->scope->parent == NULL)
    {
        necro_core_scope_insert(&ll->ll_arena, ll->scope, ast->bind.ast_symbol);
        necro_core_lambda_lift_push_scope(ll);
        necro_core_lambda_lift_go(ll, ast->bind.expr);
        assert(ll->scope->free_vars->count == 0);
        ast->bind.ast_symbol->free_vars = ll->scope->free_vars;
        necro_core_lambda_lift_pop_scope(ll);
        return;
    }
    else if (!necro_core_lambda_lift_is_bind_fn(ast))
    {
        necro_core_scope_insert(&ll->ll_arena, ll->scope, ast->bind.ast_symbol);
        necro_core_lambda_lift_go(ll, ast->bind.expr);
        return;
    }
    // Lift and insert into global scope
    necro_core_scope_insert(&ll->ll_arena, ll->global_scope, ast->bind.ast_symbol);
    necro_core_lambda_lift_push_scope(ll);
    necro_core_lambda_lift_go(ll, ast->bind.expr);
    // Lift named lambda
    NecroCoreAst* lifted_let      = necro_core_ast_create_let(ll->ast_arena, ast, NULL);
    NecroCoreAst* last_lifted_let = ll->lift_point;
    if (last_lifted_let == NULL)
    {
        ll->lift_point = lifted_let;
    }
    else
    {
        while (last_lifted_let->let.expr != NULL)
            last_lifted_let = last_lifted_let->let.expr;
        last_lifted_let->let.expr = lifted_let;
    }
    NecroCoreScope* free_vars       = ll->scope->free_vars;
    ast->bind.ast_symbol->free_vars = free_vars;
    // Add Lambdas, starting from end
    for (int32_t i = free_vars->size - 1; i >= 0; i--)
    {
        NecroCoreAstSymbol* ast_symbol = free_vars->data[i].renamed_ast_symbol;
        if (ast_symbol == NULL)
            continue;
        ast->bind.expr             = necro_core_ast_create_lam(ll->ast_arena, necro_core_ast_create_var(ll->ast_arena, ast_symbol), ast->bind.expr);
        ast->bind.ast_symbol->type = necro_type_fn_create(ll->ast_arena, ast_symbol->type, ast->bind.ast_symbol->type);
    }
    ast->necro_type = ast->bind.ast_symbol->type;
    necro_core_lambda_lift_pop_scope(ll);
}

// TODO: Anonymous Lambdas
// Lift anonymous lambdas at app?
void necro_core_lambda_lift_lam(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    necro_core_lambda_lift_pat(ll, ast->lambda.arg);
    necro_core_lambda_lift_go(ll, ast->lambda.expr);
}

///////////////////////////////////////////////////////
// LambdaLift Pat
///////////////////////////////////////////////////////
void necro_core_lambda_lift_pat(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    if (ast == NULL)
        return;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_APP:
        necro_core_lambda_lift_pat(ll, ast->app.expr1);
        necro_core_lambda_lift_pat(ll, ast->app.expr2);
        return;
    case NECRO_CORE_AST_VAR:
        if (!ast->var.ast_symbol->is_constructor && ast->var.ast_symbol != ll->base->prim_undefined->core_ast_symbol)
            necro_core_scope_insert(&ll->ll_arena, ll->scope, ast->var.ast_symbol);
        return;
    case NECRO_CORE_AST_LIT: return;
    default:                 assert(false && "Unimplemented Ast in necro_core_lambda_lift_pat");     return;
    }
}

///////////////////////////////////////////////////////
// LambdaLift Go
///////////////////////////////////////////////////////
// TODO: Simple Rewrite rules for |>, >>, and map
void necro_core_lambda_lift_go(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    if (ast == NULL)
        return;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_VAR:       necro_core_lambda_lift_var(ll, ast);  return;
    case NECRO_CORE_AST_APP:       necro_core_lambda_lift_app(ll, ast);  return;
    case NECRO_CORE_AST_FOR:       necro_core_lambda_lift_for(ll, ast);  return;
    case NECRO_CORE_AST_CASE:      necro_core_lambda_lift_case(ll, ast); return;
    case NECRO_CORE_AST_LET:       necro_core_lambda_lift_let(ll, ast);  return;
    case NECRO_CORE_AST_BIND:      necro_core_lambda_lift_bind(ll, ast); return;
    case NECRO_CORE_AST_LAM:       necro_core_lambda_lift_lam(ll, ast);  return;
    // case NECRO_CORE_AST_BIND_REC:
    case NECRO_CORE_AST_DATA_DECL: return;
    case NECRO_CORE_AST_DATA_CON:  return;
    case NECRO_CORE_AST_LIT:       return;
    default:                       assert(false && "Unimplemented Ast in necro_core_lambda_lift_go"); return;
    }
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_CORE_LAMBDA_LIFT_VERBOSE 1
void necro_core_lambda_lift_test_result(const char* test_name, const char* str)
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

    // Assert
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

void necro_core_lambda_lift_test()
{
    necro_announce_phase("Lambda Lift");

    {
        const char* test_name   = "Identity 1";
        const char* test_source = ""
            "x = True\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Identity 2";
        const char* test_source = ""
            "f x y = x || y\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lift 1";
        const char* test_source = ""
            "z = f True False\n"
            "  where\n"
            "    f x y = x || y\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lift 2";
        const char* test_source = ""
            "z = f True\n"
            "  where\n"
            "    y   = False\n"
            "    f x = x || y\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lift 3";
        const char* test_source = ""
            "z = f True\n"
            "  where\n"
            "    y   = False\n"
            "    z   = True\n"
            "    f x = g x || g y\n"
            "      where\n"
            "        g w = w && z\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lift 4";
        const char* test_source = ""
            "z = f True\n"
            "  where\n"
            "    y   = False\n"
            "    z   = True\n"
            "    f x = g x || g y\n"
            "      where\n"
            "        g w = w && z || y\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    // g and f are out of order here!
    {
        const char* test_name   = "Lift 5";
        const char* test_source = ""
            "z = g True\n"
            "  where\n"
            "    y   = False\n"
            "    f x = x || y\n"
            "    g w = f w\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    // TODO: Anonymous functions!
    // TODO: Test higher order free_vars!
    // TODO: Test nested higher order free_vars!
    // TODO: Test recursive values!
    // TODO: Test recursive values in nested functions!
}

///////////////////////////////////////////////////////
// Lambda Lifting
///////////////////////////////////////////////////////
// bool necro_bind_is_fn(NecroCoreAST_Expression* bind_ast)
// {
//     assert(bind_ast->expr_type == NECRO_CORE_EXPR_BIND);
//     NecroCoreAST_Expression* bind_expr = bind_ast->bind.expr;
//     return bind_expr->expr_type == NECRO_CORE_EXPR_LAM;
// }
//
// bool necro_is_in_env(NecroVarList* env, NecroVar var)
// {
//     // while (env != NULL && env->data.id.id != var.id.id)
//     while (env != NULL && env->data.symbol != var.symbol)
//     {
//         env = env->next;
//     }
//     return env != NULL;
// }
//
// bool necro_is_free_var(NecroLambdaLift* ll, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer, NecroVar var)
// {
//     if (necro_is_in_env(env, var))
//         return false;
//     NecroVar* lifted_env_result = necro_var_table_get(&ll->lifted_env, var.id.id);
//     if (lifted_env_result != NULL && lifted_env_result->id.id != 0)
//         return false;
//     if (outer != NULL && necro_is_in_env(outer->free_vars, var))
//         return false;
//     // TODO: Fix this! Broken after symtable changes
//     // NecroID found_id = necro_scope_find(ll->scoped_symtable->top_scope, var.symbol);
//     // return found_id.id != var.id.id;
//     return false;
// }
//
// NecroVar necro_find_in_env(NecroVarList* env, NecroVar var)
// {
//     // while (env != NULL && env->data.id.id != var.id.id)
//     while (env != NULL)
//     {
//         if (env->data.symbol != var.symbol)
//             return env->data;
//         env = env->next;
//     }
//     return (NecroVar) { 0, 0 };
// }
//
// NecroVar necro_find_free_var(NecroLambdaLift*ll, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer, NecroVar var)
// {
//     NecroVar result = necro_find_in_env(env, var);
//     if (result.id.id != 0)
//         return result;
//     NecroVar* lifted_env_result = necro_var_table_get(&ll->lifted_env, var.id.id);
//     if (lifted_env_result != NULL && lifted_env_result->id.id == var.id.id)
//         return *lifted_env_result;
//     result = (outer != NULL) ? necro_find_in_env(outer->free_vars, var) : (NecroVar) { 0, 0 };
//     if (result.id.id != 0)
//         return result;
//     // TODO: FIX THIS!! broken after symtable changes
//     // NecroID found_id = necro_scope_find(ll->scoped_symtable->top_scope, var.symbol);
//     // return (NecroVar) { .id = found_id, .symbol = var.symbol };
//     return (NecroVar) { 0 };
// }
//
// NecroLambdaLiftSymbolInfo* necro_lambda_lift_symtable_get(NecroLambdaLift* ll, NecroVar var)
// {
//     assert(ll != NULL);
//     assert(var.id.id != 0);
//     while (var.id.id >= ll->ll_symtable.length)
//     {
//         NecroLambdaLiftSymbolInfo info = { var, NULL, NULL, (NecroVar) { 0, 0 } };
//         necro_push_lambda_lift_symbol_vector(&ll->ll_symtable, &info);
//     }
//     return ll->ll_symtable.data + var.id.id;
// }
//
// NecroCoreAST_CaseAlt* necro_lambda_lift_case_alt(NecroLambdaLift* ll, NecroCoreAST_CaseAlt* alts, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     if (alts == NULL)
//         return NULL;
//
//     // Push Env
//     NecroVarList*            new_env = env;
//     NecroCoreAST_Expression* alt_con = necro_lambda_lift_pat_go(ll, alts->altCon, &new_env, outer);
//     NecroCoreAST_Expression* expr    = necro_lambda_lift_go(ll, alts->expr, new_env, outer);
//     // Pop Env
//
//     NecroCoreAST_CaseAlt*    next    = necro_lambda_lift_case_alt(ll, alts->next, env, outer);
//     return necro_create_core_case_alt(&ll->arena, expr, alt_con, next);
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_case(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
//     NecroCoreAST_Expression* expr    = necro_lambda_lift_go(ll, in_ast->case_expr.expr, env, outer);
//     NecroCoreAST_CaseAlt*    alts    = necro_lambda_lift_case_alt(ll, in_ast->case_expr.alts, env, outer);
//     NecroCoreAST_Expression* out_ast = necro_create_core_case(&ll->arena, expr, alts);
//     out_ast->necro_type              = in_ast->necro_type;
//     out_ast->case_expr.type          = in_ast->case_expr.type;
//     return out_ast;
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_list(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
//
//     // if (in_ast->list.expr == NULL)
//     //     return necro_lambda_lift_go(ll, in_ast->list.next, env, outer);
//
//     NecroCoreAST_Expression* initial_lift_point = ll->lift_point;
//
//     // NecroCoreAST_Expression* item_lift = NULL;
//     NecroCoreAST_Expression* item = necro_lambda_lift_go(ll, in_ast->list.expr, env, outer);
//     NecroCoreAST_Expression* next = necro_lambda_lift_go(ll, in_ast->list.next, env, outer);
//
//     // item_lift = ll->lift_point;
//     // while (ll->lift_point != NULL)
//     // {
//     //     item_lift = necro_create_core_list(&ll->arena, ll->lift_point->list.expr, item_lift);
//     //     ll->lift_point = ll->lift_point->list.next;
//     // }
//     // ll->lift_point = NULL;
//
//     NecroCoreAST_Expression* out_ast = necro_create_core_list(&ll->arena, item, next);
//
//     if (ll->lift_point != NULL && initial_lift_point == NULL && outer == NULL)
//     {
//         assert(ll->lift_point->expr_type == NECRO_CORE_EXPR_LIST);
//         NecroCoreAST_Expression* tmp = ll->lift_point;
//         while (tmp->list.next != NULL)
//             tmp = tmp->list.next;
//         tmp->list.next = out_ast;
//         tmp = NULL;
//         out_ast = ll->lift_point;
//         // ll->lift_point = initial_lift_point;
//         ll->lift_point = NULL;
//     }
//
//     // NecroCoreAST_Expression* head = NULL;
//     // if (in_ast->list.expr == NULL)
//     //     head = necro_create_core_list(&ll->arena, NULL, NULL);
//     // else
//     //     head = necro_create_core_list(&ll->arena, necro_lambda_lift_go(ll, in_ast->list.expr, env, outer), NULL);
//     // head->necro_type = in_ast->necro_type;
//     // NecroCoreAST_Expression* curr = head;
//     // in_ast = in_ast->list.next;
//     // while (in_ast != NULL)
//     // {
//     //     if (in_ast->list.expr == NULL)
//     //         curr->list.next = necro_create_core_list(&ll->arena, NULL, NULL);
//     //     else
//     //         curr->list.next = necro_create_core_list(&ll->arena, necro_lambda_lift_go(ll, in_ast->list.expr, env, outer), NULL);
//     //     if (ll->lift_point != NULL)
//     //     {
//     //         assert(ll->lift_point->expr_type == NECRO_CORE_EXPR_LIST);
//     //         NecroCoreAST_Expression* tmp = ll->lift_point;
//     //         while (tmp->list.next != NULL)
//     //             tmp = tmp->list.next;
//     //         tmp->list.next  = curr->list.next;
//     //         curr->list.next = ll->lift_point;
//     //         curr = tmp->list.next;
//     //         ll->lift_point = NULL;
//     //     }
//     //     else
//     //     {
//     //         curr = curr->list.next;
//     //     }
//     //     in_ast = in_ast->list.next;
//     // }
//
//     return out_ast;
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_smash_lambda(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
//     NecroCoreAST_Expression* arg_ast  = necro_deep_copy_core_ast(&ll->arena, in_ast->lambda.arg);
//     env                               = necro_cons_var_list(&ll->arena, arg_ast->var, env);
//     NecroCoreAST_Expression* expr_ast =
//         (in_ast->lambda.expr->expr_type == NECRO_CORE_EXPR_LAM) ?
//         necro_lambda_lift_smash_lambda(ll, in_ast->lambda.expr, env, outer) :
//         necro_lambda_lift_go(ll, in_ast->lambda.expr, env, outer);
//     NecroCoreAST_Expression* out_ast  = necro_create_core_lam(&ll->arena, arg_ast, expr_ast);
//     out_ast->necro_type               = in_ast->necro_type;
//     return out_ast;
// }
//
// NecroCoreAST_Expression* necro_lift_lambda_and_insert_into_env(NecroLambdaLift* ll, NecroCoreAST_Expression* ast_to_lift, NecroVar var)
// {
//     if (ll->lift_point == NULL)
//     {
//         ll->lift_point = necro_create_core_list(&ll->arena, ast_to_lift, ll->lift_point);
//         necro_var_table_insert(&ll->lifted_env, var.id.id, &var);
//         return ll->lift_point;
//     }
//     else
//     {
//         NecroCoreAST_Expression* tmp = ll->lift_point;
//         while (tmp->list.next != NULL)
//             tmp = tmp->list.next;
//         tmp->list.next = necro_create_core_list(&ll->arena, ast_to_lift, NULL);
//         necro_var_table_insert(&ll->lifted_env, var.id.id, &var);
//         return tmp->list.next;
//     }
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_lambda(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
//
//     // Create anonymous function name
//     ll->num_anon_functions++;
//     NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&ll->snapshot_arena);
//
//     char num_anon_func_buf[20] = { 0 };
//     snprintf(num_anon_func_buf, 20, "%zu", ll->num_anon_functions);
//     const char* num_anon_func_buf_ptr = (const char*) num_anon_func_buf;
//
//     const char*        fn_name  = necro_snapshot_arena_concat_strings(&ll->snapshot_arena, 2, (const char*[]) { "_anon_fn_", num_anon_func_buf_ptr });
//     NecroSymbol        var_sym  = necro_intern_string(ll->intern, fn_name);
//     NecroID            var_id   = necro_scoped_symtable_new_symbol_info(ll->scoped_symtable, ll->scoped_symtable->top_scope, necro_symtable_create_initial_symbol_info(var_sym, (NecroSourceLoc) { 0 }, NULL));
//     NecroVar           fn_var   = (NecroVar) { .id = var_id, .symbol = var_sym };
//     NecroSymbolInfo*   s_info   = necro_symtable_get(ll->symtable, var_id);
//     s_info->type                = in_ast->necro_type;
//
//     // TODO: Function which does this AND inserts it into top level scope!!!
//     NecroCoreAST_Expression* lambda_lift_point = necro_lift_lambda_and_insert_into_env(ll, NULL, fn_var);
//     // NecroCoreAST_Expression* lambda_lift_point = ll->lift_point;
//     // if (ll->lift_point == NULL)
//     // {
//     //     ll->lift_point = necro_create_core_list(&ll->arena, NULL, ll->lift_point);
//     //     lambda_lift_point = ll->lift_point;
//     // }
//     // else
//     // {
//     //     NecroCoreAST_Expression* tmp = ll->lift_point;
//     //     while (tmp->list.next != NULL)
//     //         tmp = tmp->list.next;
//     //     tmp->list.next = necro_create_core_list(&ll->arena, NULL, NULL);
//     //     lambda_lift_point = tmp->list.next;
//     // }
//
//     // Env and Outer
//     NecroVarList*              prev_env   = env;
//     NecroLambdaLiftSymbolInfo* prev_outer = outer;
//     env                                   = NULL;
//     outer                                 = necro_lambda_lift_symtable_get(ll, fn_var);
//     outer->parent                         = prev_outer;
//     ll->lift_point                        = necro_create_core_list(&ll->arena, NULL, ll->lift_point);
//
//     // Go deeper
//     NecroCoreAST_Expression* arg_ast  = necro_deep_copy_core_ast(&ll->arena, in_ast->lambda.arg);
//     env                               = necro_cons_var_list(&ll->arena, arg_ast->var, env);
//     NecroCoreAST_Expression* expr_ast =
//         (in_ast->lambda.expr->expr_type == NECRO_CORE_EXPR_LAM) ?
//         necro_lambda_lift_smash_lambda(ll, in_ast->lambda.expr, env, outer) :
//         necro_lambda_lift_go(ll, in_ast->lambda.expr, env, outer);
//     expr_ast                          = necro_create_core_lam(&ll->arena, arg_ast, expr_ast);
//     expr_ast->necro_type              = in_ast->necro_type;
//
//     // Lift
//     NecroCoreAST_Expression*   bind_ast  = necro_create_core_bind(&ll->arena, expr_ast, fn_var);
//     NecroLambdaLiftSymbolInfo* info      = outer;
//     NecroVarList*              free_vars = necro_reverse_var_list(&ll->arena, info->free_vars);
//     // TODO: Look at Types and arities to figure out what's going wrong.
//     while (free_vars != NULL)
//     // while (false)
//     {
//         NecroVar env_var = necro_find_free_var(ll, env, outer, free_vars->data);
//         expr_ast     = necro_create_core_lam(&ll->arena, necro_create_core_var(&ll->arena, env_var), expr_ast);
//         s_info->type = necro_type_fn_create(&ll->arena, necro_symtable_get(ll->symtable, free_vars->data.id)->type, s_info->type);
//         // if (prev_outer != NULL && necro_is_free_var(ll, prev_env, prev_outer, free_vars->data))
//         // {
//         //     // prev_outer->free_vars = necro_cons_var_list(&ll->arena, free_vars->data, prev_outer->free_vars);
//         //     prev_outer->free_vars = necro_cons_var_list(&ll->arena, necro_copy_var(ll, free_vars->data), prev_outer->free_vars);
//         // }
//         free_vars = free_vars->next;
//     }
//     bind_ast->bind.expr = expr_ast;
//
//     lambda_lift_point->list.expr = bind_ast;
//
//     // Apply free vars
//     necro_snapshot_arena_rewind(&ll->snapshot_arena, snapshot);
//     return necro_lambda_lift_go(ll, necro_create_core_var(&ll->arena, fn_var), prev_env, prev_outer);
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_bind(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);
//
//     // TODO: Nested lambdas messes up ordering at lift point!
//     // TODO: Rename lifted functions to avoid name clashes!
//
//     // NecroVarList*              prev_env   = env;
//     NecroLambdaLiftSymbolInfo* prev_outer = outer;
//
//     bool is_fn = necro_bind_is_fn(in_ast);
//     if (is_fn)
//     {
//         env           = necro_cons_var_list(&ll->arena, in_ast->bind.var, NULL);
//         outer         = necro_lambda_lift_symtable_get(ll, in_ast->bind.var);
//         outer->parent = prev_outer;
//     }
//
//     NecroCoreAST_Expression* expr_ast = is_fn ?
//         necro_lambda_lift_smash_lambda(ll, in_ast->bind.expr, env, outer) :
//         necro_lambda_lift_go(ll, in_ast->bind.expr, env, outer);
//     NecroCoreAST_Expression* bind_ast = necro_create_core_bind(&ll->arena, expr_ast, in_ast->var);
//     bind_ast->necro_type              = in_ast->necro_type;
//     bind_ast->bind.is_recursive       = in_ast->bind.is_recursive;
//
//     if (is_fn)
//     // if (false)
//     {
//         NecroLambdaLiftSymbolInfo* info      = outer;
//         NecroSymbolInfo*           s_info    = necro_symtable_get(ll->symtable, info->var.id);
//         NecroVarList*              free_vars = necro_reverse_var_list(&ll->arena, info->free_vars);
//         while (free_vars != NULL)
//         {
//             NecroVar env_var = necro_find_free_var(ll, env, outer, free_vars->data);
//             expr_ast     = necro_create_core_lam(&ll->arena, necro_create_core_var(&ll->arena, env_var), expr_ast);
//             s_info->type = necro_type_fn_create(&ll->arena, necro_symtable_get(ll->symtable, free_vars->data.id)->type, s_info->type);
//             // if (prev_outer != NULL && necro_is_free_var(ll, prev_env, prev_outer, free_vars->data))
//             // {
//             //     // prev_outer->free_vars = necro_cons_var_list(&ll->arena, free_vars->data, prev_outer->free_vars);
//             //     prev_outer->free_vars = necro_cons_var_list(&ll->arena, necro_copy_var(ll, free_vars->data), prev_outer->free_vars);
//             // }
//             free_vars = free_vars->next;
//         }
//         bind_ast->bind.expr = expr_ast;
//
//     }
//
//     return bind_ast;
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_let(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
//
//     env                               = necro_cons_var_list(&ll->arena, in_ast->let.bind->bind.var, env);
//     NecroCoreAST_Expression* bind_ast = necro_lambda_lift_go(ll, in_ast->let.bind, env, outer);
//
//     // NecroCoreAST_Expression* let_lift_point = NULL;
//     if (necro_bind_is_fn(in_ast->let.bind))
//         necro_lift_lambda_and_insert_into_env(ll, bind_ast, in_ast->let.bind->bind.var);
//         // let_lift_point = necro_lift_lambda_and_insert_into_env(ll, bind_ast);
//
//     NecroCoreAST_Expression* expr_ast = necro_lambda_lift_go(ll, in_ast->let.expr, env, outer);
//
//     if (necro_bind_is_fn(in_ast->let.bind))
//     {
//         // Lift to top level
//         // ll->lift_point = necro_create_core_list(&ll->arena, bind_ast, ll->lift_point);
//         // let_lift_point->list.expr = bind_ast;
//         return expr_ast;
//     }
//     else
//     {
//         NecroCoreAST_Expression* out_ast  = necro_create_core_let(&ll->arena, bind_ast, expr_ast);
//         out_ast->necro_type               = in_ast->necro_type;
//         return out_ast;
//     }
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_var(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
//
//     NecroVar var = necro_find_free_var(ll, env, outer, in_ast->var);
//
//     NecroCoreAST_Expression* out_ast = necro_create_core_var(&ll->arena, var);
//     // Set var as free var if not bound
//     out_ast->necro_type              = in_ast->necro_type;
//     // if (necro_is_free_var(ll, env, outer, in_ast->var))
//     if (var.id.id != in_ast->var.id.id)
//     {
//         // Logic for binding....
//         // needs new ID!!!!
//         assert(outer != NULL);
//         out_ast->var = necro_copy_var(ll, in_ast->var);
//         var          = out_ast->var;
//         outer->free_vars = necro_cons_var_list(&ll->arena, out_ast->var, outer->free_vars);
//     }
//
//     // Apply free vars to variable
//     NecroLambdaLiftSymbolInfo* info = necro_lambda_lift_symtable_get(ll, in_ast->var);
//     assert(info != NULL);
//     NecroVarList* free_vars = info->free_vars;
//     while (free_vars != NULL)
//     {
//         NecroVar env_var = necro_find_free_var(ll, env, outer, free_vars->data);// necro_create_core_var(&ll->arena, free_vars->data)
//         if (env_var.id.id == 0)
//         {
//             // IT'S MISSING, DO SOMETHING!
//             env_var = necro_copy_var(ll, free_vars->data);
//             outer->free_vars = necro_cons_var_list(&ll->arena, env_var, outer->free_vars);
//         }
//         // out_ast                        = necro_create_core_app(&ll->arena, out_ast, necro_create_core_var(&ll->arena, free_vars->data));
//         out_ast                        = necro_create_core_app(&ll->arena, out_ast, necro_create_core_var(&ll->arena, env_var));
//         out_ast->app.exprB->necro_type = necro_symtable_get(ll->symtable, free_vars->data.id)->type;
//         free_vars                      = free_vars->next;
//     }
//     return out_ast;
// }
//
// ///////////////////////////////////////////////////////
// // Lambda Lift Go
// ///////////////////////////////////////////////////////
// NecroCoreAST_Expression* necro_lambda_lift_go(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList* env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     if (in_ast == NULL)
//         return NULL;
//     switch (in_ast->expr_type)
//     {
//     case NECRO_CORE_EXPR_VAR:       return necro_lambda_lift_var(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_LAM:       return necro_lambda_lift_lambda(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_LET:       return necro_lambda_lift_let(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_BIND:      return necro_lambda_lift_bind(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_LIST:      return necro_lambda_lift_list(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_CASE:      return necro_lambda_lift_case(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_APP:       return necro_lambda_lift_app(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_DATA_DECL: return necro_deep_copy_core_ast(&ll->arena, in_ast); // TODO: This is only correct if in default state, not in PAT state!
//     case NECRO_CORE_EXPR_DATA_CON:  return necro_deep_copy_core_ast(&ll->arena, in_ast); // TODO: This is only correct if in default state, not in PAT state!
//     case NECRO_CORE_EXPR_LIT:       return necro_deep_copy_core_ast(&ll->arena, in_ast);
//     case NECRO_CORE_EXPR_TYPE:      return necro_deep_copy_core_ast(&ll->arena, in_ast);
//     default:                        assert(false && "Unimplemented AST type in necro_lambda_lift_go"); return NULL;
//     }
// }
//
// ///////////////////////////////////////////////////////
// // Lambda Lift Pat
// ///////////////////////////////////////////////////////
// NecroCoreAST_Expression* necro_lambda_lift_app_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
//     NecroCoreAST_Expression* expr_a  = necro_lambda_lift_pat_go(ll, in_ast->app.exprA, env, outer);
//     NecroCoreAST_Expression* expr_b  = necro_lambda_lift_pat_go(ll, in_ast->app.exprB, env, outer);
//     NecroCoreAST_Expression* out_ast = necro_create_core_app(&ll->arena, expr_a, expr_b);
//     out_ast->necro_type              = in_ast->necro_type;
//     out_ast->app.persistent_slot     = in_ast->app.persistent_slot;
//     return out_ast;
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_data_con_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
//     // Go deeper
//     if (in_ast->data_con.next != NULL)
//     {
//         NecroCoreAST_Expression con;
//         con.expr_type                    = NECRO_CORE_EXPR_DATA_CON;
//         con.data_con                     = *in_ast->data_con.next;
//         NecroCoreAST_Expression* args    = necro_lambda_lift_app_pat(ll, in_ast->data_con.arg_list, env, outer);
//         NecroCoreAST_DataCon*    next    = &necro_lambda_lift_app_pat(ll, &con, env, outer)->data_con;
//         NecroCoreAST_Expression* out_ast = necro_create_core_data_con(&ll->arena, in_ast->data_con.condid, args, next);
//         out_ast->necro_type              = in_ast->necro_type;
//         return out_ast;
//     }
//     else
//     {
//         NecroCoreAST_Expression* out_ast = necro_create_core_data_con(&ll->arena, in_ast->data_con.condid, necro_lambda_lift_app_pat(ll, in_ast->data_con.arg_list, env, outer), NULL);
//         out_ast->necro_type              = in_ast->necro_type;
//         return out_ast;
//     }
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_list_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
//     NecroCoreAST_Expression* head = NULL;
//     if (in_ast->list.expr == NULL)
//         head = necro_create_core_list(&ll->arena, NULL, NULL);
//     else
//         head = necro_create_core_list(&ll->arena, necro_lambda_lift_pat_go(ll, in_ast->list.expr, env, outer), NULL);
//     head->necro_type = in_ast->necro_type;
//     NecroCoreAST_Expression* curr = head;
//     in_ast = in_ast->list.next;
//     while (in_ast != NULL)
//     {
//         if (in_ast->list.expr == NULL)
//             curr->list.next = necro_create_core_list(&ll->arena, NULL, NULL);
//         else
//             curr->list.next = necro_create_core_list(&ll->arena, necro_lambda_lift_pat_go(ll, in_ast->list.expr, env, outer), NULL);
//         curr   = curr->list.next;
//         in_ast = in_ast->list.next;
//     }
//     return head;
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_var_pat(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env)
// {
//     assert(ll != NULL);
//     assert(in_ast != NULL);
//     assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
//     NecroCoreAST_Expression* out_ast = necro_create_core_var(&ll->arena, in_ast->var);
//     out_ast->necro_type              = in_ast->necro_type;
//     *env                             = necro_cons_var_list(&ll->arena, in_ast->var, *env);
//     return out_ast;
// }
//
// NecroCoreAST_Expression* necro_lambda_lift_pat_go(NecroLambdaLift* ll, NecroCoreAST_Expression* in_ast, NecroVarList** env, NecroLambdaLiftSymbolInfo* outer)
// {
//     assert(ll != NULL);
//     if (in_ast == NULL)
//         return NULL;
//     switch (in_ast->expr_type)
//     {
//     case NECRO_CORE_EXPR_VAR:       return necro_lambda_lift_var_pat(ll, in_ast, env);
//     case NECRO_CORE_EXPR_LIST:      return necro_lambda_lift_list_pat(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_DATA_CON:  return necro_lambda_lift_data_con_pat(ll, in_ast, env, outer);
//     case NECRO_CORE_EXPR_APP:       return necro_lambda_lift_app_pat(ll, in_ast, env, outer);
//
//     case NECRO_CORE_EXPR_BIND:      assert(false); return NULL;
//     case NECRO_CORE_EXPR_LAM:       assert(false); return NULL;
//     case NECRO_CORE_EXPR_LET:       assert(false); return NULL;
//     case NECRO_CORE_EXPR_CASE:      assert(false); return NULL;
//     case NECRO_CORE_EXPR_DATA_DECL: assert(false); return NULL;
//
//     case NECRO_CORE_EXPR_LIT:       return necro_deep_copy_core_ast(&ll->arena, in_ast);
//     case NECRO_CORE_EXPR_TYPE:      return necro_deep_copy_core_ast(&ll->arena, in_ast);
//     default:                        assert(false && "Unimplemented AST type in necro_lambda_lift_go"); return NULL;
//     }
// }


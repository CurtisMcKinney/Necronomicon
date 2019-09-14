/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "lambda_lift.h"
#include <stdio.h>
#include "type.h"
#include "monomorphize.h"
#include "alias_analysis.h"
#include "core_infer.h"

// TODO: Can hot swap:
//   * Global Values
//   * hot swap "synth" function and "pattern" function at polys only. Old versions still kicking around.
//   * How to make chained pattern hot swaps work!?
//   * Recompile chunks of code

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
typedef struct NecroCoreScopeNode
{
    NecroCoreAstSymbol* ast_symbol;
    NecroCoreAstSymbol* renamed_ast_symbol;
    NecroType*          necro_type;
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
    NecroIntern*             intern;
    NecroBase*               base;
    NecroCoreAst*            lift_point;
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
void                necro_core_scope_insert(NecroPagedArena* arena, NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol, NecroType* necro_type);

//--------------------
// NecroLambdaLift
//--------------------
NecroLambdaLift necro_lambda_lift_empty()
{
    return (NecroLambdaLift)
    {
        .ast_arena          = NULL,
        .ll_arena           = necro_paged_arena_empty(),
        .intern             = NULL,
        .base               = NULL,
        .lift_point         = NULL,
        .scope              = NULL,
        .global_scope       = NULL,
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
        .intern         = intern,
        .base           = base,
        .lift_point     = NULL,
        .scope          = global_scope,
        .global_scope   = global_scope,
    };
}

void necro_lambda_lift_destroy(NecroLambdaLift* ll)
{
    necro_paged_arena_destroy(&ll->ll_arena);
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
#define NECRO_CORE_SCOPE_INITIAL_SIZE 8

NecroCoreScope* necro_core_scope_create(NecroPagedArena* arena, NecroCoreScope* parent)
{
    NecroCoreScope* scope = necro_paged_arena_alloc(arena, sizeof(NecroCoreScope));
    scope->free_vars      = NULL;
    scope->parent         = parent;
    scope->data           = necro_paged_arena_alloc(arena, NECRO_CORE_SCOPE_INITIAL_SIZE * sizeof(NecroCoreScopeNode));
    scope->size           = NECRO_CORE_SCOPE_INITIAL_SIZE;
    scope->count          = 0;
    for (size_t slot = 0; slot < scope->size; ++slot)
        scope->data[slot] = (NecroCoreScopeNode) { .ast_symbol = NULL, .renamed_ast_symbol = NULL, .necro_type = NULL };
    return scope;
}

static inline void necro_core_scope_grow(NecroPagedArena* arena, NecroCoreScope* scope)
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
        NecroType*          necro_type         = prev_data[prev_slot].necro_type;
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
        scope->data[slot].necro_type         = necro_type;
        scope->count++;
    }
    assert(scope->count == prev_count);
}

void necro_core_scope_insert(NecroPagedArena* arena, NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol, NecroType* necro_type)
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
    scope->data[slot].necro_type         = necro_type;
    scope->count++;
}

void necro_core_scope_insert_with_renamed(NecroPagedArena* arena, NecroCoreScope* scope, NecroCoreAstSymbol* ast_symbol, NecroCoreAstSymbol* renamed_ast_symbol, NecroType* necro_type)
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
    scope->data[slot].necro_type         = necro_type;
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
    NecroSymbol renamed_symbol = necro_intern_unique_string(ll->intern, ast_symbol->name->str);
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
        NecroType*          necro_type = ll->scope->free_vars->data[i].necro_type;
        if (ast_symbol == NULL || necro_core_scope_find_in_this_scope(ll->scope->parent, ast_symbol))
            continue;
        // Rename and insert
        NecroCoreAstSymbol* renamed = necro_core_lambda_lift_create_renamed_symbol(ll, ast_symbol);
        necro_core_scope_insert_with_renamed(&ll->ll_arena, ll->scope->parent->free_vars, ast_symbol, renamed, necro_type);
    }
    // Pop scope
    ll->scope = ll->scope->parent;
}

static inline bool necro_core_lambda_lift_is_free_var(NecroLambdaLift* ll, NecroCoreAstSymbol* ast_symbol)
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
void necro_core_lambda_lift_for(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_FOR);
    necro_core_lambda_lift_go(ll, ast->for_loop.range_init);
    necro_core_lambda_lift_go(ll, ast->for_loop.value_init);
    necro_core_lambda_lift_pat(ll, ast->for_loop.index_arg);
    necro_core_lambda_lift_pat(ll, ast->for_loop.value_arg);
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
            necro_core_scope_insert_with_renamed(&ll->ll_arena, ll->scope->free_vars, ast->var.ast_symbol, renamed, ast->necro_type);
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
        NecroCoreAst* free_var = necro_core_ast_create_var(ll->ast_arena, ast_symbol, free_vars->data[i].necro_type);
        necro_core_lambda_lift_var(ll, free_var);
        // Apply
        NecroCoreAst* app_ast = necro_core_ast_create_app(ll->ast_arena, NULL, free_var);
        // Swap
        necro_core_ast_swap(ast, app_ast);
        ast->app.expr1 = app_ast;
    }
}

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
        necro_core_lambda_lift_go(ll, ast->let.bind);
        if (necro_core_lambda_lift_is_bind_fn(ast->let.bind) && ll->scope->parent != NULL)
        {
            // We lifted a function out, so we need to collapse let ast
            *ast = *ast->let.expr;
        }
        else if (ll->lift_point != NULL && ll->scope->parent == NULL)
        {
            // TODO: Somehow we tripped up lambda lift, FIXXX!
            // TODO: Update to use necro_core_ast_swap
            // We're a top level ast node and at some point in bind we lifted a function.
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // And now for some in place Ast surgery
            //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            // 1. Swap contents of ast node with lift_point node
            necro_core_ast_swap(ll->lift_point, ast);
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

// NOTE: This is only used for anonymous lambdas
void necro_core_lambda_lift_lam(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    // Create name, symbol, and var
    NecroSymbol         anon_name   = necro_intern_unique_string(ll->intern, "anonymous");
    NecroCoreAstSymbol* anon_symbol = necro_core_ast_symbol_create(ll->ast_arena, anon_name, ast->necro_type);
    NecroCoreAst*       anon_var    = necro_core_ast_create_var(ll->ast_arena, anon_symbol, anon_symbol->type);
    // In-place Swap ast with anon_var
    NecroCoreAst        temp        = *anon_var;
    *anon_var                       = *ast;
    *ast                            = temp;
    // Create bind and go deeper
    NecroCoreAst*       anon_bind   = necro_core_ast_create_bind(ll->ast_arena, anon_symbol, anon_var, NULL);
    necro_core_lambda_lift_go(ll, anon_bind);
    necro_core_lambda_lift_go(ll, ast);
}

// NOTE: This is only used for bound lambdas
void necro_core_lambda_lift_bound_lam(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_LAM);
    necro_core_lambda_lift_pat(ll, ast->lambda.arg);
    if (ast->lambda.expr->ast_type == NECRO_CORE_AST_LAM)
        necro_core_lambda_lift_bound_lam(ll, ast->lambda.expr);
    else
        necro_core_lambda_lift_go(ll, ast->lambda.expr);
}

void necro_core_lambda_lift_bind(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_BIND);
    if (ll->scope->parent == NULL)
    {
        necro_core_scope_insert(&ll->ll_arena, ll->scope, ast->bind.ast_symbol, ast->bind.ast_symbol->type);
        necro_core_lambda_lift_push_scope(ll);
        if (necro_core_lambda_lift_is_bind_fn(ast))
            necro_core_lambda_lift_bound_lam(ll, ast->bind.expr);
        else
            necro_core_lambda_lift_go(ll, ast->bind.expr);
        assert(ll->scope->free_vars->count == 0);
        ast->bind.ast_symbol->free_vars = ll->scope->free_vars;
        necro_core_lambda_lift_pop_scope(ll);
        return;
    }
    else if (!necro_core_lambda_lift_is_bind_fn(ast))
    {
        necro_core_scope_insert(&ll->ll_arena, ll->scope, ast->bind.ast_symbol, ast->bind.ast_symbol->type);
        necro_core_lambda_lift_go(ll, ast->bind.expr);
        return;
    }
    // Lift and insert into global scope
    necro_core_scope_insert(&ll->ll_arena, ll->global_scope, ast->bind.ast_symbol, ast->bind.ast_symbol->type);
    necro_core_lambda_lift_push_scope(ll);
    necro_core_lambda_lift_bound_lam(ll, ast->bind.expr);
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
    NecroType*      f_ownership     = ast->bind.ast_symbol->type->ownership;
    // Add Lambdas, starting from end
    for (int32_t i = free_vars->size - 1; i >= 0; i--)
    {
        NecroCoreAstSymbol* ast_symbol = free_vars->data[i].renamed_ast_symbol;
        if (ast_symbol == NULL)
            continue;
        ast->bind.expr                         = necro_core_ast_create_lam(ll->ast_arena, necro_core_ast_create_var(ll->ast_arena, ast_symbol, ast_symbol->type), ast->bind.expr);
        ast->bind.ast_symbol->type             = necro_type_fn_create(ll->ast_arena, ast_symbol->type, ast->bind.ast_symbol->type);
        ast->bind.ast_symbol->type->ownership  = f_ownership;
    }
    ast->necro_type = ast->bind.ast_symbol->type;
    necro_core_lambda_lift_pop_scope(ll);
}

void necro_core_lambda_lift_app(NecroLambdaLift* ll, NecroCoreAst* ast)
{
    assert(ast->ast_type == NECRO_CORE_AST_APP);
    necro_core_lambda_lift_go(ll, ast->app.expr1);
    necro_core_lambda_lift_go(ll, ast->app.expr2);
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
            necro_core_scope_insert(&ll->ll_arena, ll->scope, ast->var.ast_symbol, ast->necro_type);
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

    {
        const char* test_name   = "Higher Lift 1";
        const char* test_source = ""
            "top :: Int\n"
            "top = app f 10\n"
            "  where\n"
            "    app g x = g x\n"
            "    f y = y * 100\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Higher Lift 2";
        const char* test_source = ""
            "top :: Int\n"
            "top = app f 10\n"
            "  where\n"
            "    f y = y * 100\n"
            "    app g x = g x + h x\n"
            "      where\n"
            "          h y = y - 100\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Rec Lift 1";
        const char* test_source = ""
            "countItUp :: Float\n"
            "countItUp = x where\n"
            "  x ~ 0 = f x\n"
            "  f y   = y + 10\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda 1";
        const char* test_source = ""
            "f = \\x y -> x || y\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda 2";
        const char* test_source = ""
            "f = (\\x -> x || True) False\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda 3";
        const char* test_source = ""
            "f = r where\n"
            "  t = True\n"
            "  r = (\\x -> x || t) False\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "In Case Of Emergency";
        const char* test_source = ""
            "inCaseOfEmergency b = breakGlass where\n"
            "  breakGlass = case b of\n"
            "    True  -> \\x -> x && b\n"
            "    False -> \\y -> y || b\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lift Unique 1";
        const char* test_source = ""
            "utest :: *Bool -> *Bool\n"
            "utest b = f True where\n"
            "  f x = b\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lift Unique 2";
        const char* test_source = ""
            "utest :: *Bool -> *Bool -> *(Bool, Bool)\n"
            "utest b c = f True where\n"
            "  f x = (b, c)\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda 4";
        const char* test_source = ""
            "top :: Int\n"
            "top = r\n"
            "  where\n"
            "    app g x = g x\n"
            "    w       = 10\n"
            "    r       = app (\\y -> y * w) 100\n";
        necro_core_lambda_lift_test_result(test_name, test_source);
    }

    // // TODO: We need bind rec to properly handle this
    // {
    //     const char* test_name   = "Rec Lift 2";
    //     const char* test_source = ""
    //         "countItUp :: Float\n"
    //         "countItUp = x where\n"
    //         "  x ~ 0 = f y\n"
    //         "  y ~ 0 = f x\n"
    //         "  f z   = z + 10\n";
    //     necro_core_lambda_lift_test_result(test_name, test_source);
    // }

}

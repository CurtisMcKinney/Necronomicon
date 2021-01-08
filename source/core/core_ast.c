/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdint.h>
#include <inttypes.h>

#include "core_ast.h"
#include "type.h"
#include "core_infer.h"
#include "monomorphize.h"
#include "alias_analysis.h"
#include "kind.h"
#include "runtime.h"
#include "math_utility.h"

#define NECRO_CORE_DEBUG 0
#if NECRO_CORE_DEBUG
#define TRACE_CORE(...) printf(__VA_ARGS__)
#else
#define TRACE_CORE(...)
#endif

///////////////////////////////////////////////////////
// Core Ast Creation
///////////////////////////////////////////////////////
NecroCoreAst* necro_core_ast_alloc(NecroPagedArena* arena, NECRO_CORE_AST_TYPE ast_type)
{
    NecroCoreAst* ast    = necro_paged_arena_alloc(arena, sizeof(NecroCoreAst));
    ast->ast_type        = ast_type;
    ast->necro_type      = NULL;
    ast->persistent_slot = 0xFFFFFFFF;
    return ast;
}

NecroCoreAst* necro_core_ast_create_lit(NecroPagedArena* arena, NecroAstConstant constant)
{
    NecroCoreAst* ast        = necro_core_ast_alloc(arena, NECRO_CORE_AST_LIT);
    ast->lit.type            = constant.type;
    switch (constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
    case NECRO_AST_CONSTANT_FLOAT:
        ast->lit.float_literal = constant.double_literal;
        break;
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
    case NECRO_AST_CONSTANT_INTEGER:
        ast->lit.int_literal = constant.int_literal;
        break;
    case NECRO_AST_CONSTANT_UNSIGNED_INTEGER_PATTERN:
    case NECRO_AST_CONSTANT_UNSIGNED_INTEGER:
        ast->lit.uint_literal = constant.uint_literal;
        break;
    case NECRO_AST_CONSTANT_STRING:
        ast->lit.string_literal = constant.symbol;
        break;
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
    case NECRO_AST_CONSTANT_CHAR:
        ast->lit.char_literal = constant.char_literal;
        break;
    case NECRO_AST_CONSTANT_ARRAY:
        assert(false);
        break;
    case NECRO_AST_CONSTANT_TYPE_INT:
    case NECRO_AST_CONSTANT_TYPE_STRING:
        assert(false);
        break;
    }
    return ast;
}

NecroCoreAst* necro_core_ast_create_array_lit(NecroPagedArena* arena, NecroCoreAstList* elements)
{
    NecroCoreAst* ast               = necro_core_ast_alloc(arena, NECRO_CORE_AST_LIT);
    ast->lit.type                   = NECRO_AST_CONSTANT_ARRAY;
    ast->lit.array_literal_elements = elements;
    return ast;
}

NecroCoreAst* necro_core_ast_create_var(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroType* necro_type)
{
    assert(necro_type != NULL);
    // assert(necro_type->kind != NULL);
    NecroCoreAst* ast        = necro_core_ast_alloc(arena, NECRO_CORE_AST_VAR);
    ast->var.ast_symbol      = ast_symbol;
    ast->necro_type          = necro_type;
    return ast;
}

NecroCoreAst* necro_core_ast_create_bind(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroCoreAst* expr, NecroCoreAst* initializer)
{
    NecroCoreAst* ast     = necro_core_ast_alloc(arena, NECRO_CORE_AST_BIND);
    ast->bind.ast_symbol  = ast_symbol;
    ast->bind.expr        = expr;
    ast->bind.initializer = initializer;
    ast_symbol->ast       = ast;
    return ast;
}

NecroCoreAst* necro_core_ast_create_let(NecroPagedArena* arena, NecroCoreAst* bind, NecroCoreAst* expr)
{
    NecroCoreAst* ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_LET);
    ast->let.bind     = bind;
    ast->let.expr     = expr;
    return ast;
}

NecroCoreAst* necro_core_ast_create_lam(NecroPagedArena* arena, NecroCoreAst* arg, NecroCoreAst* expr)
{
    NecroCoreAst* ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_LAM);
    ast->lambda.arg   = arg;
    ast->lambda.expr  = expr;
    return ast;
}

// NecroCoreAst* necro_core_ast_create_app(NecroPagedArena* arena, NecroCoreAst* expr1, NecroCoreAst* expr2, NecroType* necro_type)
NecroCoreAst* necro_core_ast_create_app(NecroPagedArena* arena, NecroCoreAst* expr1, NecroCoreAst* expr2)
{
    // assert(necro_type != NULL);
    // assert(necro_type->kind != NULL);
    NecroCoreAst* ast        = necro_core_ast_alloc(arena, NECRO_CORE_AST_APP);
    ast->app.expr1           = expr1;
    ast->app.expr2           = expr2;
    // ast->necro_type          = necro_type;
    return ast;
}

NecroCoreAst* necro_core_ast_create_data_con(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroType* type, NecroType* data_type_type)
{
    // assert(type != NULL);
    // assert(type->kind != NULL);
    assert(data_type_type != NULL);
    assert(data_type_type->kind != NULL);
    NecroCoreAst* ast             = necro_core_ast_alloc(arena, NECRO_CORE_AST_DATA_CON);
    ast->data_con.ast_symbol      = ast_symbol;
    ast->data_con.type            = type;
    ast->data_con.data_type_type  = data_type_type;
    ast->data_con.ast_symbol->ast = ast;
    return ast;
}

NecroCoreAst* necro_core_ast_create_data_decl(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroCoreAstList* con_list)
{
    NecroCoreAst* ast         = necro_core_ast_alloc(arena, NECRO_CORE_AST_DATA_DECL);
    ast->data_decl.ast_symbol = ast_symbol;
    ast->data_decl.con_list   = con_list;
    ast_symbol->ast           = ast;
    return ast;
}

NecroCoreAst* necro_core_ast_create_case(NecroPagedArena* arena, NecroCoreAst* expr, NecroCoreAstList* alts)
{
    NecroCoreAst* ast   = necro_core_ast_alloc(arena, NECRO_CORE_AST_CASE);
    ast->case_expr.expr = expr;
    ast->case_expr.alts = alts;
    return ast;
}

NecroCoreAst* necro_core_ast_create_case_alt(NecroPagedArena* arena, NecroCoreAst* pat, NecroCoreAst* expr)
{
    NecroCoreAst* ast  = necro_core_ast_alloc(arena, NECRO_CORE_AST_CASE_ALT);
    ast->case_alt.pat  = pat;
    ast->case_alt.expr = expr;
    return ast;
}

NecroCoreAst* necro_core_ast_create_for_loop(NecroPagedArena* arena, size_t max_loops, NecroCoreAst* range_init, NecroCoreAst* value_init, NecroCoreAst* index_arg, NecroCoreAst* value_arg, NecroCoreAst* expression)
{
    NecroCoreAst* ast             = necro_core_ast_alloc(arena, NECRO_CORE_AST_LOOP);
    ast->loop.value_pat           = value_arg;
    ast->loop.value_init          = value_init;
    ast->loop.for_loop.index_pat  = index_arg;
    ast->loop.for_loop.range_init = range_init;
    ast->loop.for_loop.max_loops  = max_loops;
    ast->loop.do_expression       = expression;
    ast->loop.loop_type           = NECRO_LOOP_FOR;
    return ast;
}

NecroCoreAst* necro_core_ast_create_while_loop(NecroPagedArena* arena, NecroCoreAst* value_pat, NecroCoreAst* value_init, NecroCoreAst* while_expression, NecroCoreAst* do_expression)
{
    NecroCoreAst* ast                     = necro_core_ast_alloc(arena, NECRO_CORE_AST_LOOP);
    ast->loop.value_pat                   = value_pat;
    ast->loop.value_init                  = value_init;
    ast->loop.while_loop.while_expression = while_expression;
    ast->loop.do_expression               = do_expression;
    ast->loop.loop_type                   = NECRO_LOOP_WHILE;
    return ast;
}

NecroCoreAst* necro_core_ast_deep_copy_lit(NecroPagedArena* arena, NecroCoreAst* ast)
{
    if (ast->lit.type != NECRO_AST_CONSTANT_ARRAY)
    {
        NecroCoreAst* new_ast = necro_core_ast_alloc(arena, NECRO_CORE_AST_LIT);
        new_ast->lit          = ast->lit;
        new_ast->necro_type   = ast->necro_type;
        return new_ast;
    }
    assert(false && "TODO: Finish!");
    return NULL;
}

NecroCoreAst* necro_core_ast_deep_copy(NecroPagedArena* arena, NecroCoreAst* ast)
{
    assert(arena != NULL);
    if (ast == NULL)
        return NULL;
    NecroCoreAst* copy_ast = NULL;
    switch (ast->ast_type)
    {
    // TODO: Finish
    case NECRO_CORE_AST_LIT:       copy_ast = necro_core_ast_deep_copy_lit(arena, ast); break;
    case NECRO_CORE_AST_DATA_DECL: assert(false && "TODO: FINISH"); copy_ast = necro_core_ast_create_data_decl(arena, ast->data_decl.ast_symbol, NULL); break; // TODO: Finish
    case NECRO_CORE_AST_CASE:      assert(false && "TODO: FINISH"); copy_ast = necro_core_ast_create_case(arena, necro_core_ast_deep_copy(arena, ast->case_expr.expr), NULL); break; // TODO: Finish!
    // case NECRO_CORE_AST_BIND_REC:
    case NECRO_CORE_AST_BIND:      copy_ast = necro_core_ast_create_bind(arena, ast->bind.ast_symbol, necro_core_ast_deep_copy(arena, ast->bind.expr), necro_core_ast_deep_copy(arena, ast->bind.initializer)); break;
    case NECRO_CORE_AST_LET:       copy_ast = necro_core_ast_create_let(arena, necro_core_ast_deep_copy(arena, ast->let.bind), necro_core_ast_deep_copy(arena, ast->let.expr)); break;
    case NECRO_CORE_AST_VAR:       copy_ast = necro_core_ast_create_var(arena, ast->var.ast_symbol, necro_type_deep_copy(arena, ast->necro_type)); break;
    case NECRO_CORE_AST_LAM:       copy_ast = necro_core_ast_create_lam(arena, necro_core_ast_deep_copy(arena, ast->lambda.arg), necro_core_ast_deep_copy(arena, ast->lambda.expr)); break;
    case NECRO_CORE_AST_APP:       copy_ast = necro_core_ast_create_app(arena, necro_core_ast_deep_copy(arena, ast->app.expr1), necro_core_ast_deep_copy(arena, ast->app.expr2)); break;
    case NECRO_CORE_AST_DATA_CON:  copy_ast = necro_core_ast_create_data_con(arena, ast->data_con.ast_symbol, necro_type_deep_copy(arena, ast->data_con.type), necro_type_deep_copy(arena, ast->data_con.data_type_type)); break;
    case NECRO_CORE_AST_LOOP:
        if (ast->loop.loop_type == NECRO_LOOP_FOR)
            copy_ast = necro_core_ast_create_for_loop(arena, ast->loop.for_loop.max_loops, necro_core_ast_deep_copy(arena, ast->loop.for_loop.range_init), necro_core_ast_deep_copy(arena, ast->loop.value_init), necro_core_ast_deep_copy(arena, ast->loop.for_loop.index_pat), necro_core_ast_deep_copy(arena, ast->loop.value_pat), necro_core_ast_deep_copy(arena, ast->loop.do_expression));
        else
            copy_ast = necro_core_ast_create_while_loop(arena, necro_core_ast_deep_copy(arena, ast->loop.value_pat), necro_core_ast_deep_copy(arena, ast->loop.value_init), necro_core_ast_deep_copy(arena, ast->loop.while_loop.while_expression), necro_core_ast_deep_copy(arena, ast->loop.do_expression));
        break;

    default:                       assert(false && "Unimplemented Ast in necro_core_ast_deep_copy"); return NULL;
    }
    copy_ast->necro_type = necro_type_deep_copy(arena, ast->necro_type);
    return copy_ast;
}

size_t necro_core_ast_num_args(NecroCoreAst* bind_ast)
{
    assert(bind_ast->ast_type == NECRO_CORE_AST_BIND || bind_ast->ast_type == NECRO_CORE_AST_BIND_REC);
    size_t        num_args = 0;
    NecroCoreAst* lambdas  = (bind_ast->ast_type == NECRO_CORE_AST_BIND) ? bind_ast->bind.expr : NULL; // TODO: Finish with bind_rec
    while (lambdas->ast_type == NECRO_CORE_AST_LAM)
    {
        num_args++;
        lambdas = lambdas->lambda.expr;
    }
    return num_args;
}

void necro_core_ast_swap(NecroCoreAst* ast1, NecroCoreAst* ast2)
{
    NecroCoreAst temp = *ast1;
    *ast1             = *ast2;
    *ast2             = temp;
}

///////////////////////////////////////////////////////
// NecroCoreAstArena
///////////////////////////////////////////////////////
NecroCoreAstArena necro_core_ast_arena_empty()
{
    return (NecroCoreAstArena)
    {
        .arena = necro_paged_arena_empty(),
        .root  = NULL,
    };
}

NecroCoreAstArena necro_core_ast_arena_create()
{
    NecroPagedArena arena = necro_paged_arena_create();
    return (NecroCoreAstArena)
    {
        .arena = arena,
        .root  = NULL,
    };
}

void necro_core_ast_arena_destroy(NecroCoreAstArena* ast_arena)
{
    necro_paged_arena_destroy(&ast_arena->arena);
    *ast_arena = necro_core_ast_arena_empty();
}

///////////////////////////////////////////////////////
// Transform to Core
///////////////////////////////////////////////////////
typedef struct NecroCoreAstTransform
{
    NecroPagedArena*   arena;
    NecroIntern*       intern;
    NecroBase*         base;
    NecroAstArena*     ast_arena;
    NecroCoreAstArena* core_ast_arena;
} NecroCoreAstTransform;

NecroCoreAstTransform necro_core_ast_transform_empty()
{
    return (NecroCoreAstTransform)
    {
        .arena          = NULL,
        .intern         = NULL,
        .base           = NULL,
        .ast_arena      = NULL,
        .core_ast_arena = NULL,
    };
}

NecroCoreAstTransform necro_core_ast_transform_create(NecroIntern* intern, NecroBase* base, NecroAstArena* ast_arena, NecroCoreAstArena* core_ast_arena)
{
    necro_core_ast_arena_destroy(core_ast_arena);
    *core_ast_arena = necro_core_ast_arena_create();
    return (NecroCoreAstTransform)
    {
        .arena           = &core_ast_arena->arena,
        .intern          = intern,
        .base            = base,
        .ast_arena       = ast_arena,
        .core_ast_arena  = core_ast_arena,
    };
}

void necro_ast_transform_destroy(NecroCoreAstTransform* core_ast_transform)
{
    *core_ast_transform = necro_core_ast_transform_empty();
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_go(NecroCoreAstTransform* context, NecroAst* ast);
NecroResult(void) necro_ast_transform_to_core(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroAstArena* ast_arena, NecroCoreAstArena* core_ast_arena)
{
    // Create
    NecroCoreAstTransform core_ast_transform = necro_core_ast_transform_create(intern, base, ast_arena, core_ast_arena);
    // Transform Base
    core_ast_transform.core_ast_arena->root  = necro_try_map_result(NecroCoreAst, void, necro_ast_transform_to_core_go(&core_ast_transform, base->ast.root));
    assert(core_ast_transform.core_ast_arena->root->ast_type == NECRO_CORE_AST_LET);
    // Transform everything else (Cheap and easy "modules"...)
    NecroCoreAst* final_base_let = core_ast_transform.core_ast_arena->root;
    while (final_base_let->let.expr != NULL)
        final_base_let = final_base_let->let.expr;
    final_base_let->let.expr = necro_try_map_result(NecroCoreAst, void, necro_ast_transform_to_core_go(&core_ast_transform, ast_arena->root));
    // Cleanup
    if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_TRANSFORM_TO_CORE && info.verbosity > 0))
        necro_core_ast_pretty_print(core_ast_transform.core_ast_arena->root);
    necro_ast_transform_destroy(&core_ast_transform);
    return ok_void();
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_pat(NecroCoreAstTransform* context, NecroAst* ast);

// TODO: (Chad) -> add bind rec support and mutually recursive pattern assignment support
NecroResult(NecroCoreAst) necro_ast_transform_to_core_pat_assignment(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_PAT_ASSIGNMENT);
    NecroCoreAst* rhs_core = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->pat_assignment.rhs));
    rhs_core->necro_type = ast->pat_assignment.rhs->necro_type;
    NecroCoreAst* alt_pat  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, ast->pat_assignment.pat));
    alt_pat->necro_type = ast->pat_assignment.pat->necro_type;
    NecroCoreAst* alt_expr = NULL;
    NecroCoreAst* alt_ast  = necro_core_ast_create_case_alt(context->arena, alt_pat, alt_expr);
    alt_ast->necro_type = ast->pat_assignment.pat->necro_type;
    NecroCoreAstList* alts = necro_append_core_ast_list(context->arena, alt_ast, NULL);
    NecroCoreAst* core_ast = necro_core_ast_create_case(context->arena, rhs_core, alts);
    core_ast->necro_type = ast->necro_type;
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_var(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_VARIABLE);
    assert(ast->necro_type != NULL);
    NecroCoreAst* core_ast = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->variable.ast_symbol), necro_type_deep_copy(context->arena, ast->necro_type));
    assert(core_ast->necro_type != NULL);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_con(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_CONID);
    assert(ast->necro_type != NULL);
    NecroCoreAst* core_ast = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->conid.ast_symbol), necro_type_deep_copy(context->arena, ast->necro_type));
    assert(core_ast->necro_type != NULL);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_lit(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_CONSTANT);
    // Technically this is a bit dubious since we're doing type punning...
    NecroAstConstant constant;
    constant.int_literal   = ast->constant.int_literal;
    constant.type          = ast->constant.type;
    NecroCoreAst* core_ast = necro_core_ast_create_lit(context->arena, constant);
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_app(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    NecroCoreAst* expr1    = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->fexpression.aexp));
    NecroCoreAst* expr2    = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->fexpression.next_fexpression));
    NecroCoreAst* core_ast = necro_core_ast_create_app(context->arena,expr1, expr2);
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_data_decl(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_DATA_DECLARATION);
    NecroCoreAst* core_ast  = necro_core_ast_create_data_decl(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol), NULL);
    core_ast->necro_type    = core_ast->data_decl.ast_symbol->type;
    NecroAst*     cons      = ast->data_declaration.constructor_list;
    while (cons != NULL)
    {
        NecroCoreAst*  con_ast       = necro_core_ast_create_data_con(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, cons->list.item->constructor.conid->conid.ast_symbol), NULL, ast->data_declaration.simpletype->necro_type);
        con_ast->data_con.type       = con_ast->data_con.ast_symbol->type;
        core_ast->data_decl.con_list = necro_append_core_ast_list(context->arena, con_ast, core_ast->data_decl.con_list);
        cons                         = cons->list.next_item;
    }
    return ok(NecroCoreAst, core_ast);
}

// NOTE: Filters out type signatures, class declarations, polymorphic values, etc
bool necro_core_ast_should_filter(NecroBase* base, NecroAst* ast)
{
    UNUSED(base);
    switch (ast->type)
    {
    case NECRO_AST_TYPE_CLASS_INSTANCE:
    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
        return true;
    case NECRO_AST_DATA_DECLARATION:
        return false;
    case NECRO_AST_APATS_ASSIGNMENT:
        // return necro_type_is_polymorphic(ast->apats_assignment.ast_symbol->type) || necro_type_is_polymorphic(ast->apats_assignment.ast_symbol->type->ownership);
        // return necro_type_is_polymorphic_ignoring_ownership(base, ast->apats_assignment.ast_symbol->type);
        return necro_type_is_polymorphic(ast->apats_assignment.ast_symbol->type);
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        return necro_type_is_polymorphic(ast->simple_assignment.ast_symbol->type);
        // return necro_type_is_polymorphic_ignoring_ownership(base, ast->simple_assignment.ast_symbol->type);
        // return necro_type_is_polymorphic(ast->necro_type) || necro_type_is_polymorphic(ast->necro_type->ownership);
        // return necro_type_is_polymorphic(ast->necro_type) || necro_type_is_polymorphic(ast->necro_type->ownership);
    case NECRO_AST_PAT_ASSIGNMENT:
        return necro_type_is_polymorphic(ast->pat_assignment.pat->necro_type);
        // return necro_type_is_polymorphic_ignoring_ownership(base, ast->pat_assignment.pat->necro_type);
        // return necro_type_is_polymorphic_ignoring_ownership(base, ast->pat_assignment.pat->necro_type) || necro_type_is_polymorphic(ast->pat_assignment.pat->necro_type->ownership);
    default:
        assert(false);
        return true;
    }
}

// NOTE: This is the thorniest one for ast_to_core, though it's not tooooo bad.
// Part of the complexity comes from needing to float instance declarations up into global scope.
// Going to do this the simple way for now...Flattening everything into singular Bind instead of using BindRecs for recursive groups
NecroResult(NecroCoreAst) necro_ast_transform_to_core_declaration_group_list(NecroCoreAstTransform* context, NecroAst* ast, NecroCoreAst* last_expr)
{
    if (ast == NULL)
        return ok(NecroCoreAst, last_expr);
    assert(ast->type == NECRO_AST_DECLARATION_GROUP_LIST);
    NecroCoreAst* ast_chain_head = NULL;
    NecroCoreAst* ast_chain_curr = NULL;
    while (ast != NULL)
    {
        NecroAst* declaration_group = ast->declaration_group_list.declaration_group;
        while (declaration_group != NULL)
        {
            // Instance Declarations
            if (declaration_group->declaration.declaration_impl->type == NECRO_AST_TYPE_CLASS_INSTANCE && declaration_group->declaration.declaration_impl->type_class_instance.declarations != NULL)
            {
                NecroCoreAst* instance_declarations = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_declaration_group_list(context, declaration_group->declaration.declaration_impl->type_class_instance.declarations, NULL));
                if (instance_declarations != NULL)
                {
                    assert(instance_declarations->ast_type == NECRO_CORE_AST_LET);
                    NecroCoreAst* instance_curr = instance_declarations;
                    while (instance_curr->let.expr !=  NULL)
                        instance_curr = instance_curr->let.expr;
                    if (ast_chain_head == NULL)
                    {
                        ast_chain_head = instance_declarations;
                        ast_chain_curr = instance_curr;
                    }
                    else
                    {
                        ast_chain_curr->let.expr = instance_declarations;
                        ast_chain_curr           = instance_curr;
                    }
                }
            }
            // Normal Declarations
            else if (!necro_core_ast_should_filter(context->base, declaration_group->declaration.declaration_impl))
            {
                NecroCoreAst* binder = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, declaration_group->declaration.declaration_impl));

                assert(
                    binder->ast_type == NECRO_CORE_AST_DATA_DECL ||
                    binder->ast_type == NECRO_CORE_AST_BIND ||
                    binder->ast_type == NECRO_CORE_AST_BIND_REC ||
                    binder->ast_type == NECRO_CORE_AST_CASE
                );


                NecroCoreAst* ast_chain_next = NULL;
                switch(binder->ast_type)
                {
                    case NECRO_CORE_AST_DATA_DECL:
                    case NECRO_CORE_AST_BIND:
                    case NECRO_CORE_AST_BIND_REC:
                    case NECRO_CORE_AST_LET:
                        ast_chain_next = necro_core_ast_create_let(context->arena, binder, NULL);
                        break;
                    case NECRO_CORE_AST_CASE:
                        ast_chain_next = binder;
                        break;
                    default:
                        assert(false && "binder->ast_type not supported!");
                }

                assert(ast_chain_next != NULL);

                if (ast_chain_head == NULL)
                {
                    ast_chain_head = ast_chain_next;
                    ast_chain_curr = ast_chain_next;
                }
                else
                {
                    switch(ast_chain_curr->ast_type)
                    {
                        case NECRO_CORE_AST_LET:
                            ast_chain_curr->let.expr = ast_chain_next;
                            break;
                        case NECRO_CORE_AST_CASE:
                            assert(ast_chain_curr->case_expr.alts != NULL);
                            assert(ast_chain_curr->case_expr.alts->data != NULL);
                            assert(ast_chain_curr->case_expr.alts->data->ast_type == NECRO_CORE_AST_CASE_ALT);
                            assert(ast_chain_curr->case_expr.alts->data->case_alt.expr == NULL);
                            ast_chain_curr->case_expr.alts->data->case_alt.expr = ast_chain_next;
                            break;
                        default:
                            assert(false && "ast_chain_curr->type not supported!");
                    }
                    ast_chain_curr = ast_chain_next;
                }
            }

            declaration_group = declaration_group->declaration.next_declaration;
        }
        ast = ast->declaration_group_list.next;
    }

    if (ast_chain_head == NULL)
        return ok(NecroCoreAst, NULL);

    switch(ast_chain_curr->ast_type)
    {
        case NECRO_CORE_AST_LET:
            ast_chain_curr->let.expr = last_expr;
            break;
        case NECRO_CORE_AST_CASE:
            assert(ast_chain_curr->case_expr.alts != NULL);
            assert(ast_chain_curr->case_expr.alts->data != NULL);
            assert(ast_chain_curr->case_expr.alts->data->ast_type == NECRO_CORE_AST_CASE_ALT);
            assert(ast_chain_curr->case_expr.alts->data->case_alt.expr == NULL);
            ast_chain_curr->case_expr.alts->data->case_alt.expr = last_expr;
            break;
        default:
            assert(false && "ast_chain_curr->type not supported!");
    }

    assert(ast_chain_head != NULL);
    return ok(NecroCoreAst, ast_chain_head );
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_let(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_LET_EXPRESSION);
    NecroCoreAst* expr_ast  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->let_expression.expression));
    assert(expr_ast->necro_type != NULL);
    NecroCoreAst* binds_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_declaration_group_list(context, ast->let_expression.declarations, expr_ast));
    binds_ast->necro_type    = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(binds_ast->necro_type != NULL);
    return ok(NecroCoreAst, binds_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_rhs(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    NecroCoreAst* expr_ast  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->right_hand_side.expression));
    assert(expr_ast->necro_type != NULL);
    NecroCoreAst* binds_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_declaration_group_list(context, ast->right_hand_side.declarations, expr_ast));
    binds_ast->necro_type    = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(binds_ast->necro_type != NULL);
    return ok(NecroCoreAst, binds_ast);
}

// TODO: Check for exhaustive case expressions!
NecroResult(NecroCoreAst) necro_ast_transform_to_core_lambda_apats(NecroCoreAstTransform* context, NecroAst* ast, NecroCoreAst* lambda_expr)
{
    NecroCoreAst* lambda_head     = NULL;
    NecroCoreAst* lambda_curr     = NULL;
    NecroAst*     apats           = ast->lambda.apats;
    NecroCoreAst* apat_cases_head = NULL;
    NecroCoreAst* apat_cases_tail = NULL;
    while (apats != NULL)
    {
        if (apats->apats.apat->type == NECRO_AST_VARIABLE || apats->apats.apat->type == NECRO_AST_WILDCARD)
        {
            NecroCoreAst* var  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, apats->apats.apat));
            if (lambda_head == NULL)
            {
                lambda_curr = necro_core_ast_create_lam(context->arena, var, NULL);
                lambda_head = lambda_curr;
            }
            else
            {
                lambda_curr->lambda.expr = necro_core_ast_create_lam(context->arena, var, NULL);
                lambda_curr = lambda_curr->lambda.expr;
            }
        }
        else
        {
            NecroCoreAst* apat_variable = NULL;
            { // Generate variable placeholder for the apat argument, used for case destructuring
                NecroType* apat_arg_type = necro_type_deep_copy(context->arena, apats->apats.apat->necro_type);
                NecroCoreAstSymbol* apat_symbol = necro_core_ast_symbol_create(
                    context->arena,
                    necro_intern_unique_string(context->intern, "_apatsArg"),
                    apat_arg_type
                );
                assert(apat_symbol != NULL);

                apat_variable = necro_core_ast_create_var(
                    context->arena,
                    apat_symbol,
                    apat_arg_type
                );

                assert(apat_variable != NULL);
                assert(apat_variable->necro_type != NULL);

            }

            { // Create lambda using the variable as the argument name
                if (lambda_head == NULL)
                {
                    lambda_curr = necro_core_ast_create_lam(context->arena, apat_variable, NULL);
                    lambda_head = lambda_curr;
                }
                else
                {
                    lambda_curr->lambda.expr = necro_core_ast_create_lam(context->arena, apat_variable, NULL);
                    lambda_curr = lambda_curr->lambda.expr;
                }
            }

            { // build the case expression on the apat argument, referencing the variable now bound to the lambda argument

                // @Curtis: How do I figure this out?
                // TODO: Add error handling for data types with multiple constructors
                /* if (apats->apats.apat->type == NECRO_AST_CONSTRUCTOR) */
                /* { */
                /*     // Get base type here, find number of constructors some how, throw error if more than one! */
                /* } */

                NecroCoreAst* alt_pat  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, apats->apats.apat));
                NecroCoreAst* alt_expr = NULL;
                NecroCoreAst* alt_ast  = necro_core_ast_create_case_alt(context->arena, alt_pat, alt_expr);
                NecroCoreAstList* alts = NULL;
                alts                   = necro_append_core_ast_list(context->arena, alt_ast, alts);
                assert(alts != NULL);
                NecroCoreAst* case_ast = necro_core_ast_create_case(context->arena, apat_variable, alts);
                if (apat_cases_tail != NULL)
                {
                    apat_cases_tail->case_expr.alts->data->case_alt.expr = case_ast;
                }

                apat_cases_tail = case_ast;

                if (apat_cases_head == NULL)
                {
                    apat_cases_head = apat_cases_tail;
                }

                case_ast->necro_type = necro_type_deep_copy(context->arena, lambda_expr->necro_type);
            }
        }
        apats = apats->apats.next_apat;
    }

    assert(lambda_curr != NULL);
    if (apat_cases_tail != NULL)
    { // Float case statements to be after all the lambdas, with the final case expression being the final expression for the whole apats expression
        assert(apat_cases_head != NULL);
        lambda_curr->lambda.expr = apat_cases_head;
        apat_cases_tail->case_expr.alts->data->case_alt.expr = lambda_expr;
    }
    else
    {
        lambda_curr->lambda.expr = lambda_expr;
    }

    return ok(NecroCoreAst, lambda_head);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_lambda(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_LAMBDA);
    NecroCoreAst* expr_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->lambda.expression));
    NecroCoreAst* lam_ast  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_lambda_apats(context, ast, expr_ast));
    lam_ast->necro_type    = necro_type_deep_copy(context->arena, ast->necro_type);
    return ok(NecroCoreAst, lam_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_apats_assignment(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_APATS_ASSIGNMENT);
    NecroCoreAst* rhs_ast    = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->apats_assignment.rhs));
    NecroCoreAst* lambda_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_lambda_apats(context, ast, rhs_ast));
    NecroCoreAst* bind_ast   = necro_core_ast_create_bind(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->apats_assignment.ast_symbol), lambda_ast, NULL);
    return ok(NecroCoreAst, bind_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_simple_assignment(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    // TODO: How to handle initializers?
    NecroCoreAst* expr_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->simple_assignment.rhs));
    NecroCoreAst* init_ast = NULL;
    if (ast->simple_assignment.initializer != NULL)
    {
        init_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->simple_assignment.initializer));
    }
    NecroCoreAst* bind_ast = necro_core_ast_create_bind(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->simple_assignment.ast_symbol), expr_ast, init_ast);
    return ok(NecroCoreAst, bind_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_bin_op(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_BIN_OP);
    // NOTE: 1 + 2 --> (+) 1 2 --> ((+) 1) 2 --> App (App (+) 1) 2
    NecroCoreAst* op_ast    = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create_from_ast_symbol(context->arena, ast->bin_op.ast_symbol), ast->bin_op.op_type);
    NecroCoreAst* left_ast  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->bin_op.lhs));
    NecroCoreAst* right_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->bin_op.rhs));
    NecroCoreAst* inner_app = necro_core_ast_create_app(context->arena, op_ast, left_ast);
    NecroCoreAst* outer_app = necro_core_ast_create_app(context->arena, inner_app, right_ast);
    outer_app->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(outer_app->necro_type != NULL);
    return ok(NecroCoreAst, outer_app);
}

NecroType* necro_core_ast_create_mono_tuple_con(NecroCoreAstTransform* context, NecroAst* ast)
{
    // Create Mono Constructor Type Function
    assert(ast->necro_type->type == NECRO_TYPE_CON);
    NecroType* mono_tuple_con_type_head = NULL;
    NecroType* mono_tuple_con_type_curr = NULL;
    NecroType* type_args                = ast->necro_type->con.args;
    while (type_args != NULL)
    {
        if (mono_tuple_con_type_head == NULL)
        {
            mono_tuple_con_type_head = necro_type_fn_create(context->arena, necro_type_deep_copy(context->arena, type_args->list.item), NULL);
            mono_tuple_con_type_curr = mono_tuple_con_type_head;
        }
        else
        {
            mono_tuple_con_type_curr->fun.type2 = necro_type_fn_create(context->arena, necro_type_deep_copy(context->arena, type_args->list.item), NULL);
            mono_tuple_con_type_curr            = mono_tuple_con_type_curr->fun.type2;
        }
        type_args = type_args->list.next;
    }
    mono_tuple_con_type_curr->fun.type2 = necro_type_deep_copy(context->arena, ast->necro_type);
    unwrap(void, necro_kind_infer_default_unify_with_star(context->arena, context->base, mono_tuple_con_type_head, NULL, zero_loc, zero_loc));
    return mono_tuple_con_type_head;
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_tuple(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_TUPLE);
    // Count args
    NecroAst* args  = ast->tuple.expressions;
    size_t    count = 0;
    while (args != NULL)
    {
        count++;
        args = args->list.next_item;
    }

    NecroType* mono_tuple_con = necro_core_ast_create_mono_tuple_con(context, ast);

    // Create Con var
    NecroCoreAst* tuple_con = !ast->tuple.is_unboxed ?
        necro_core_ast_create_var(context->arena, necro_base_get_tuple_con(context->base, count)->core_ast_symbol, mono_tuple_con) :
        necro_core_ast_create_var(context->arena, necro_base_get_unboxed_tuple_con(context->base, count)->core_ast_symbol, mono_tuple_con);
    // Apply args
    NecroCoreAst* tuple_ast  = tuple_con;
    args                     = ast->tuple.expressions;
    // TODO: Finish creating intermediate types for each app!
    // NecroType*    tuple_type = ast->
    while (args != NULL)
    {
        NecroCoreAst* arg_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, args->list.item));
        tuple_ast             = necro_core_ast_create_app(context->arena, tuple_ast, arg_ast);
        args = args->list.next_item;
    }

    tuple_ast->necro_type = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(tuple_ast->necro_type != NULL);
    return ok(NecroCoreAst, tuple_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_array(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_EXPRESSION_ARRAY);
    NecroCoreAstList* elements_head = NULL;
    NecroCoreAstList* elements_curr = NULL;
    NecroAst*         ast_elements  = ast->expression_array.expressions;
    while (ast_elements != NULL)
    {
        NecroCoreAst* core_element = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast_elements->list.item));
        if (elements_head == NULL)
        {
            elements_head = necro_cons_core_ast_list(context->arena, core_element, NULL);
            elements_curr = elements_head;
        }
        else
        {
            elements_curr->next = necro_cons_core_ast_list(context->arena, core_element, NULL);
            elements_curr       = elements_curr->next;
        }
        ast_elements = ast_elements->list.next_item;
    }
    NecroCoreAst* core_array_ast = necro_core_ast_create_array_lit(context->arena, elements_head);
    core_array_ast->necro_type = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(core_array_ast->necro_type != NULL);
    return ok(NecroCoreAst, core_array_ast);
}

// Simple interpreter for Natural Numbers and operations on Natural Numbers
size_t necro_nat_to_size_t(const NecroBase* base, const NecroType* n)
{
    n = necro_type_find_const(n);
    if (n->type == NECRO_TYPE_NAT)
        return n->nat.value;
    else if (n->type == NECRO_TYPE_CON && n->con.con_symbol == base->block_size_type)
        return necro_runtime_get_block_size();
    else if (n->type == NECRO_TYPE_CON && n->con.con_symbol == base->sample_rate_type)
        return necro_runtime_get_sample_rate();
    else if (n->type == NECRO_TYPE_CON && n->con.con_symbol == base->nat_mul_type)
    {
        assert(n->con.args != NULL);
        assert(n->con.args->list.next != NULL);
        NecroType*   arg1   = n->con.args->list.item;
        NecroType*   arg2   = n->con.args->list.next->list.item;
        const size_t result = necro_nat_to_size_t(base, arg1) * necro_nat_to_size_t(base, arg2);
        return result;
    }
    else if (n->type == NECRO_TYPE_CON && n->con.con_symbol == base->nat_div_type)
    {
        assert(n->con.args != NULL);
        assert(n->con.args->list.next != NULL);
        NecroType*   arg1   = n->con.args->list.item;
        NecroType*   arg2   = n->con.args->list.next->list.item;
        const size_t result = necro_nat_to_size_t(base, arg1) / necro_nat_to_size_t(base, arg2);
        return result;
    }
    else if (n->type == NECRO_TYPE_CON && n->con.con_symbol == base->nat_max_type)
    {
        assert(n->con.args != NULL);
        assert(n->con.args->list.next != NULL);
        NecroType*   arg1   = n->con.args->list.item;
        NecroType*   arg2   = n->con.args->list.next->list.item;
        const size_t result = MAX(necro_nat_to_size_t(base, arg1), necro_nat_to_size_t(base, arg2));
        return result;
    }
    else if (n->type == NECRO_TYPE_CON && n->con.con_symbol == base->nat_next_power_of_2)
    {
        assert(n->con.args != NULL);
        assert(n->con.args->list.next == NULL);
        NecroType*   arg    = n->con.args->list.item;
        const size_t result = next_highest_pow_of_2((uint32_t)necro_nat_to_size_t(base, arg));
        return result;
    }
    assert(false);
    return 0;
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_for_loop(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_FOR_LOOP);
    NecroCoreAst* range_init = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->for_loop.range_init));
    NecroCoreAst* value_init = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->for_loop.value_init));
    NecroCoreAst* index_arg  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, ast->for_loop.index_apat));
    NecroCoreAst* value_arg  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, ast->for_loop.value_apat));
    NecroCoreAst* expression = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->for_loop.expression));
    assert(range_init->necro_type->type == NECRO_TYPE_CON);
    assert(range_init->necro_type->con.con_symbol == context->base->range_type);
    assert(range_init->necro_type->con.args != NULL);
    const size_t  max_loops  = necro_nat_to_size_t(context->base, range_init->necro_type->con.args->list.item);
    if (value_arg->ast_type != NECRO_CORE_AST_VAR)
    {
        // Desugar pattern to case statement
        NecroCoreAstSymbol* new_value_sym     = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "pval"), ast->necro_type);
        NecroCoreAst*       new_value_pat     = necro_core_ast_create_var(context->arena, new_value_sym, ast->necro_type);
        NecroCoreAst*       new_value_pat_var = necro_core_ast_create_var(context->arena, new_value_sym, ast->necro_type);
        NecroCoreAst*       expr_case_alt     = necro_core_ast_create_case_alt(context->arena, value_arg, expression);
        NecroCoreAst*       new_expression    = necro_core_ast_create_case(context->arena, new_value_pat_var, necro_cons_core_ast_list(context->arena, expr_case_alt, NULL));
        value_arg                             = new_value_pat;
        expression                            = new_expression;
    }
    NecroCoreAst* core_ast = necro_core_ast_create_for_loop(context->arena, max_loops, range_init, value_init, index_arg, value_arg, expression);
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(core_ast->necro_type != NULL);
    return ok(NecroCoreAst, core_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_while_loop(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_WHILE_LOOP);
    NecroCoreAst* value_pat         = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, ast->while_loop.value_apat));
    NecroCoreAst* value_init        = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->while_loop.value_init));
    NecroCoreAst* while_expression  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->while_loop.while_expression));
    NecroCoreAst* do_expression     = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->while_loop.do_expression));
    if (value_pat->ast_type != NECRO_CORE_AST_VAR)
    {
        // Desugar pattern to case statement
        NecroCoreAstSymbol* new_value_sym          = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "pval"), ast->necro_type);
        NecroCoreAst*       new_value_pat          = necro_core_ast_create_var(context->arena, new_value_sym, ast->necro_type);
        // Test case
        NecroCoreAst*       test_new_value_pat_var = necro_core_ast_create_var(context->arena, new_value_sym, ast->necro_type);
        NecroCoreAst*       test_case_alt          = necro_core_ast_create_case_alt(context->arena, necro_core_ast_deep_copy(context->arena, value_pat), while_expression);
        NecroCoreAst*       new_while_expression   = necro_core_ast_create_case(context->arena, test_new_value_pat_var, necro_cons_core_ast_list(context->arena, test_case_alt, NULL));
        // Expr case
        // NecroCoreAst*       expr_new_value_pat_var = necro_core_ast_create_var(context->arena, new_value_sym, value_pat->necro_type);
        // NecroCoreAst*       expr_case_alt          = necro_core_ast_create_case_alt(context->arena, value_pat, do_expression);
        // NecroCoreAst*       new_do_expression      = necro_core_ast_create_case(context->arena, expr_new_value_pat_var, necro_cons_core_ast_list(context->arena, expr_case_alt, NULL));
        value_pat                                  = new_value_pat;
        while_expression                           = new_while_expression;
        // do_expression                              = new_do_expression;
    }
    NecroCoreAst* core_ast = necro_core_ast_create_while_loop(context->arena, value_pat, value_init, while_expression, do_expression);
    core_ast->necro_type   = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(core_ast->necro_type != NULL);
    return ok(NecroCoreAst, core_ast);
}

//--------------------
// Case / Patterns
//--------------------
NecroResult(NecroCoreAst) necro_ast_transform_to_core_tuple_pat(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_TUPLE);
    // Count args
    NecroAst* args  = ast->tuple.expressions;
    size_t    count = 0;
    while (args != NULL)
    {
        count++;
        args = args->list.next_item;
    }
    NecroType* mono_tuple_con = necro_core_ast_create_mono_tuple_con(context, ast);
    // Create Con var
    NecroCoreAst* tuple_con = !ast->tuple.is_unboxed ?
        necro_core_ast_create_var(context->arena, necro_base_get_tuple_con(context->base, count)->core_ast_symbol, mono_tuple_con) :
        necro_core_ast_create_var(context->arena, necro_base_get_unboxed_tuple_con(context->base, count)->core_ast_symbol, mono_tuple_con);
    // Apply args
    NecroCoreAst* tuple_ast = tuple_con;
    args                    = ast->tuple.expressions;
    // TODO: Finish creating intermediate types for each app!
    while (args != NULL)
    {
        NecroCoreAst* arg_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, args->list.item));
        tuple_ast             = necro_core_ast_create_app(context->arena, tuple_ast, arg_ast); // TODO: Finish type
        args = args->list.next_item;
    }
    return ok(NecroCoreAst, tuple_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_wildcard(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_WILDCARD);
    NecroType*    wildcard_type               = necro_type_deep_copy(context->arena, ast->necro_type);
    NecroCoreAst* wildcard_ast                = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "wildcard"), wildcard_type), wildcard_type);
    wildcard_ast->var.ast_symbol->is_wildcard = true;
    return ok(NecroCoreAst, wildcard_ast); // NOTE, from Curtis: Wild cards represented as NULL causes issues, instead we're going to compile them as anonymous variables
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_pat(NecroCoreAstTransform* context, NecroAst* ast)
{
    if (ast == NULL)
        return ok(NecroCoreAst, NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:   return necro_ast_transform_to_core_var(context, ast);
    case NECRO_AST_CONID:      return necro_ast_transform_to_core_con(context, ast);
    case NECRO_AST_CONSTANT:   return necro_ast_transform_to_core_lit(context, ast);
    case NECRO_AST_TUPLE:      return necro_ast_transform_to_core_tuple_pat(context, ast);
    case NECRO_AST_WILDCARD:   return necro_ast_transform_to_core_wildcard(context, ast);
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroCoreAst* con_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, ast->constructor.conid));
        NecroAst*     args    = ast->constructor.arg_list;
        while (args != NULL)
        {
            NecroCoreAst* arg_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, args->list.item));
            con_ast               = necro_core_ast_create_app(context->arena, con_ast, arg_ast);
            args = args->list.next_item;
        }
        return ok(NecroCoreAst, con_ast);
    }
    case NECRO_AST_EXPRESSION_ARRAY:       return ok(NecroCoreAst, NULL);
    default:
        assert(false && "Unrecognized ast type");
        return ok(NecroCoreAst, NULL);
    }
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_case(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_CASE);
    NecroCoreAst*     expr_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->case_expression.expression));
    NecroCoreAstList* alts     = NULL;
    NecroAst*         ast_alts = ast->case_expression.alternatives;
    while (ast_alts != NULL)
    {
        NecroCoreAst* alt_pat  = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_pat(context, ast_alts->list.item->case_alternative.pat));
        NecroCoreAst* alt_expr = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast_alts->list.item->case_alternative.body));
        assert(alt_expr->necro_type != NULL);
        NecroCoreAst* alt_ast  = necro_core_ast_create_case_alt(context->arena, alt_pat, alt_expr);
        alts                   = necro_append_core_ast_list(context->arena, alt_ast, alts);
        ast_alts = ast_alts->list.next_item;
    }
    assert(alts != NULL);
    // TODO: Check for incomplete case statements
    NecroCoreAst* case_ast = necro_core_ast_create_case(context->arena, expr_ast, alts);
    case_ast->necro_type = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(case_ast->necro_type != NULL);
    return ok(NecroCoreAst, case_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_left_section(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_OP_LEFT_SECTION);
    assert(ast->op_left_section.op_necro_type->type == NECRO_TYPE_FUN);
    assert(ast->op_left_section.op_necro_type->fun.type2->type == NECRO_TYPE_FUN);

    // NOTE: (9 +) --> (\lambda_arg -> (App (App (bin_op) 9) lambda_arg ))

    NecroType* lam_arg_type = necro_type_deep_copy(context->arena, ast->op_left_section.op_necro_type->fun.type2->fun.type1);
    unwrap(NecroType, necro_kind_infer(context->arena, context->base, lam_arg_type, zero_loc, zero_loc));

    NecroCoreAstSymbol* lam_arg_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_string(context->intern, "left_section_arg"), lam_arg_type);
    assert(lam_arg_symbol != NULL);

    NecroCoreAst* lam_arg = necro_core_ast_create_var(context->arena, lam_arg_symbol, lam_arg_type);

    NecroCoreAst* op_ast = necro_core_ast_create_var(
        context->arena,
        necro_core_ast_symbol_create_from_ast_symbol(
            context->arena,
            ast->op_left_section.ast_symbol
        ),
        ast->op_left_section.op_necro_type
    );

    NecroCoreAst* left_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->op_left_section.left));

    NecroCoreAst* inner_app = necro_core_ast_create_app(context->arena, op_ast, left_ast);
    inner_app->necro_type = ast->necro_type;

    NecroCoreAst* outer_app = necro_core_ast_create_app(context->arena, inner_app, lam_arg);
    outer_app->necro_type = ast->op_left_section.op_necro_type;

    NecroCoreAst* lam_ast  = necro_core_ast_create_lam(context->arena, lam_arg, outer_app);
    lam_ast->necro_type    = necro_type_deep_copy(context->arena, ast->necro_type);

    return ok(NecroCoreAst, lam_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_right_section(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_OP_RIGHT_SECTION);
    assert(ast->op_right_section.op_necro_type->type == NECRO_TYPE_FUN);

    // NOTE: (+ 9) --> (\lambda_arg -> (App (App (bin_op) lambda_arg) 9 ))

    NecroType* lam_arg_type = necro_type_deep_copy(context->arena, ast->op_right_section.op_necro_type->fun.type1);
    unwrap(NecroType, necro_kind_infer(context->arena, context->base, lam_arg_type, zero_loc, zero_loc));

    NecroCoreAstSymbol* lam_arg_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_string(context->intern, "right_section_arg"), lam_arg_type);
    assert(lam_arg_symbol != NULL);

    NecroCoreAst* lam_arg = necro_core_ast_create_var(context->arena, lam_arg_symbol, lam_arg_type);

    NecroCoreAst* op_ast = necro_core_ast_create_var(
        context->arena,
        necro_core_ast_symbol_create_from_ast_symbol(
            context->arena,
            ast->op_right_section.ast_symbol
        ),
        ast->op_right_section.op_necro_type
    );

    NecroCoreAst* right_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, ast->op_right_section.right));

    NecroCoreAst* inner_app = necro_core_ast_create_app(context->arena, op_ast, lam_arg);
    inner_app->necro_type = ast->necro_type;

    NecroCoreAst* outer_app = necro_core_ast_create_app(context->arena, inner_app, right_ast);
    outer_app->necro_type = ast->op_right_section.op_necro_type;

    NecroCoreAst* lam_ast  = necro_core_ast_create_lam(context->arena, lam_arg, outer_app);
    lam_ast->necro_type    = necro_type_deep_copy(context->arena, ast->necro_type);

    return ok(NecroCoreAst, lam_ast);
}

NecroResult(NecroCoreAst) necro_ast_transform_to_core_if_then_else(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(ast->type == NECRO_AST_IF_THEN_ELSE);

    // If
    NecroAst* if_expr = ast->if_then_else.if_expr;
    assert(if_expr != NULL);
    NecroCoreAst* core_if_expr = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, if_expr));
    assert(core_if_expr != NULL);
    core_if_expr->necro_type = necro_type_deep_copy(context->arena, if_expr->necro_type);
    assert(core_if_expr->necro_type != NULL);


    NecroCoreAstList* alts = NULL;

    // Then
    {
        NecroAst* then_expr = ast->if_then_else.then_expr;
        assert(then_expr != NULL);
        NecroCoreAst* core_then_expr = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, then_expr));
        assert(core_then_expr != NULL);
        core_then_expr->necro_type = necro_type_deep_copy(context->arena, then_expr->necro_type);
        assert(core_then_expr->necro_type != NULL);

        NecroType* bool_type = context->base->bool_type->type;
        assert(bool_type != NULL);
        NecroType* true_type = bool_type;
        assert(true_type != NULL);
        NecroCoreAst* true_core = necro_core_ast_create_var(context->arena, context->base->true_con->core_ast_symbol, necro_type_deep_copy(context->arena, true_type));
        assert(true_core != NULL);
        assert(true_core->necro_type != NULL);

        NecroCoreAst* alt_pat = true_core;
        NecroCoreAst* alt_ast = necro_core_ast_create_case_alt(context->arena, alt_pat, core_then_expr);
        assert(alt_ast != NULL);
        alt_ast->necro_type = necro_type_deep_copy(context->arena, core_then_expr->necro_type);
        assert(alt_ast->necro_type != NULL);
        assert(alt_ast->case_alt.pat != NULL);
        alts = necro_cons_core_ast_list(context->arena, alt_ast, alts);
        assert(alts != NULL);
    }

    // Else
    {
        NecroAst* else_expr = ast->if_then_else.else_expr;
        assert(else_expr != NULL);
        NecroCoreAst* core_else_expr = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, else_expr));
        assert(core_else_expr != NULL);
        core_else_expr->necro_type = necro_type_deep_copy(context->arena, else_expr->necro_type);
        assert(core_else_expr->necro_type != NULL);

        NecroType* bool_type = context->base->bool_type->type;
        assert(bool_type != NULL);
        NecroType* false_type = bool_type;
        assert(false_type != NULL);
        NecroCoreAst* false_core = necro_core_ast_create_var(context->arena, context->base->false_con->core_ast_symbol, necro_type_deep_copy(context->arena, false_type));
        assert(false_core != NULL);
        assert(false_core->necro_type != NULL);

        NecroCoreAst* alt_pat = false_core;
        NecroCoreAst* alt_ast = necro_core_ast_create_case_alt(context->arena, alt_pat, core_else_expr);
        assert(alt_ast != NULL);
        alt_ast->necro_type = necro_type_deep_copy(context->arena, core_else_expr->necro_type);
        assert(alt_ast->necro_type != NULL);
        assert(alt_ast->case_alt.pat != NULL);
        alts = necro_cons_core_ast_list(context->arena, alt_ast, alts);
        assert(alts != NULL);
    }

    assert(alts != NULL);
    // TODO: Check for incomplete case statements
    NecroCoreAst* case_ast = necro_core_ast_create_case(context->arena, core_if_expr, alts);
    assert(case_ast != NULL);
    case_ast->necro_type = necro_type_deep_copy(context->arena, ast->necro_type);
    assert(case_ast->necro_type != NULL);
    return ok(NecroCoreAst, case_ast);
}

//--------------------
// Sequence Expressions
//--------------------
// TODO: Make sure to tell Chad that nodes cannot be shared across AST tree, since mutation can occur!
// TODO: SeqParam, fml
NecroResult(NecroCoreAst) necro_ast_transform_to_core_seq_expression(NecroCoreAstTransform* context, NecroAst* ast)
{
    assert(context != NULL);
    assert(ast->type == NECRO_AST_SEQ_EXPRESSION);

    //--------------------
    // Create lambda SeqParam arg symbol
    NecroCoreAstSymbol* lambda_arg_seq_param_symbol = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "seqParam"), context->base->seq_param_type->type);

    //--------------------
    // Create types and count expressions
    NecroType* seq_type       = necro_type_strip_for_all(necro_type_find(ast->necro_type));
    assert(seq_type->type == NECRO_TYPE_CON);
    NecroType* element_type   = necro_type_find(seq_type->con.args->list.item);
    assert(seq_type->type == NECRO_TYPE_CON);
    NecroType* seq_value_type = necro_type_con1_create(context->arena, context->base->seq_value_type, element_type);
    unwrap(NecroType, necro_kind_infer(context->arena, context->base, seq_value_type, ast->source_loc, ast->end_loc));
    size_t    num_expressions = 0;
    NecroAst* seq_expressions = ast->sequence_expression.expressions;
    while (seq_expressions != NULL)
    {
        num_expressions++;
        seq_expressions = seq_expressions->list.next_item;
    }
    //--------------------
    // Create Initializer
    NecroType*    acc_type           = necro_type_con2_create(context->arena, necro_base_get_unboxed_tuple_type(context->base, 2), context->base->int_type->type, seq_value_type);
    unwrap(NecroType, necro_kind_infer(context->arena, context->base, acc_type, ast->source_loc, ast->end_loc));
    NecroType*    init_tuple_fn_type = necro_type_fn_create(context->arena, context->base->int_type->type, necro_type_fn_create(context->arena, seq_value_type, acc_type));
    NecroCoreAst* init_tuple_ast     = necro_core_ast_create_var(context->arena, necro_base_get_unboxed_tuple_con(context->base, 2)->core_ast_symbol, init_tuple_fn_type);
    init_tuple_ast                   = necro_core_ast_create_app(context->arena, init_tuple_ast, necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }));
    init_tuple_ast->necro_type       = init_tuple_fn_type->fun.type2;
    init_tuple_ast                   = necro_core_ast_create_app(context->arena, init_tuple_ast, necro_core_ast_create_var(context->arena, context->base->prim_undefined->core_ast_symbol, seq_value_type));
    init_tuple_ast->necro_type       = init_tuple_fn_type->fun.type2->fun.type2;
    NecroCoreAstSymbol* acc_var_sym  = necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "seqAcc"), acc_type);
    //--------------------
    // Create Case
    NecroType*    proj_0_type   = necro_type_fn_create(context->arena, acc_type, necro_type_fn_create(context->arena, context->base->int_type->type, context->base->int_type->type));
    NecroCoreAst* case_proj_ast = necro_core_ast_create_var(context->arena, necro_base_get_proj_symbol(context->arena, context->base), proj_0_type);
    case_proj_ast               = necro_core_ast_create_app(context->arena, case_proj_ast, necro_core_ast_create_var(context->arena, acc_var_sym, acc_type));
    case_proj_ast->necro_type   = proj_0_type->fun.type2;
    case_proj_ast               = necro_core_ast_create_app(context->arena, case_proj_ast, necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }));
    case_proj_ast->necro_type   = proj_0_type->fun.type2->fun.type2;
    NecroCoreAstList* case_alts = NULL;
    seq_expressions             = ast->sequence_expression.expressions;
    size_t expression_index     = 0;
    while (seq_expressions != NULL)
    {
        NecroCoreAst* expression_ast = necro_try_result(NecroCoreAst, necro_ast_transform_to_core_go(context, seq_expressions->list.item));
        NecroType*    run_seq_type   = necro_type_find(ast->sequence_expression.run_seq_symbol->type);
        NecroCoreAst* run_seq_ast    = necro_core_ast_create_var(context->arena, ast->sequence_expression.run_seq_symbol->core_ast_symbol, run_seq_type);
        expression_ast               = necro_core_ast_create_app(context->arena, run_seq_ast, expression_ast);
        expression_ast->necro_type   = run_seq_type->fun.type2;
        // expression_ast               = necro_core_ast_create_app(context->arena, expression_ast, necro_core_ast_create_var(context->arena, context->base->unit_con->core_ast_symbol, context->base->unit_type->type));
        expression_ast               = necro_core_ast_create_app(context->arena, expression_ast, necro_core_ast_create_var(context->arena, lambda_arg_seq_param_symbol, context->base->seq_param_type->type));
        expression_ast->necro_type   = run_seq_type->fun.type2->fun.type2;
        NecroCoreAst* pat_ast        = necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = expression_index, .type = NECRO_AST_CONSTANT_INTEGER });
        pat_ast->necro_type          = context->base->int_type->type;
        case_alts                    = necro_append_core_ast_list(context->arena, necro_core_ast_create_case_alt(context->arena, pat_ast, expression_ast), case_alts);
        seq_expressions              = seq_expressions->list.next_item;
        expression_index++;
    }
    NecroCoreAst*     case_ast  = necro_core_ast_create_case(context->arena, case_proj_ast, case_alts);
    //--------------------
    // Create bind
    NecroCoreAst* proj_ast     = necro_core_ast_create_var(context->arena, necro_base_get_proj_symbol(context->arena, context->base), proj_0_type);
    proj_ast                   = necro_core_ast_create_app(context->arena, proj_ast, necro_core_ast_create_var(context->arena, acc_var_sym, acc_type));
    proj_ast->necro_type       = proj_0_type->fun.type2;
    proj_ast                   = necro_core_ast_create_app(context->arena, proj_ast, necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }));
    proj_ast->necro_type       = proj_0_type->fun.type2->fun.type2;
    NecroCoreAst* tick_fn_var  = necro_core_ast_create_var(context->arena, ast->sequence_expression.tick_symbol->core_ast_symbol, necro_type_find(ast->sequence_expression.tick_symbol->core_ast_symbol->type));
    NecroCoreAst* tick_ast     = necro_core_ast_create_app(context->arena, tick_fn_var, proj_ast);
    tick_ast->necro_type       = tick_fn_var->necro_type->fun.type2;
    tick_ast                   = necro_core_ast_create_app(context->arena, tick_ast, necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = num_expressions, .type = NECRO_AST_CONSTANT_INTEGER }));
    tick_ast->necro_type       = tick_fn_var->necro_type->fun.type2->fun.type2;
    tick_ast                   = necro_core_ast_create_app(context->arena, tick_ast, case_ast);
    tick_ast->necro_type       = tick_fn_var->necro_type->fun.type2->fun.type2->fun.type2;
    NecroCoreAst* acc_bind_ast = necro_core_ast_create_bind(context->arena, acc_var_sym, tick_ast, init_tuple_ast);
    //--------------------
    // Create let/in
    NecroType*    proj_1_type = necro_type_fn_create(context->arena, acc_type, necro_type_fn_create(context->arena, context->base->int_type->type, seq_value_type));
    NecroCoreAst* in_expr_ast = necro_core_ast_create_var(context->arena, necro_base_get_proj_symbol(context->arena, context->base), proj_1_type);
    in_expr_ast               = necro_core_ast_create_app(context->arena, in_expr_ast, necro_core_ast_create_var(context->arena, acc_var_sym, acc_var_sym->type));
    in_expr_ast->necro_type   = proj_1_type->fun.type2;
    in_expr_ast               = necro_core_ast_create_app(context->arena, in_expr_ast, necro_core_ast_create_lit(context->arena, (NecroAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER }));
    in_expr_ast->necro_type   = proj_1_type->fun.type2->fun.type2;
    NecroCoreAst* let_ast     = necro_core_ast_create_let(context->arena, acc_bind_ast, in_expr_ast);
    let_ast->necro_type       = seq_value_type;
    //--------------------
    // Create Seq
    // NecroType*    lam_type                   = necro_type_fn_create(context->arena, context->base->unit_type->type, seq_value_type);
    NecroType*    lam_type                   = necro_type_fn_create(context->arena, context->base->seq_param_type->type, seq_value_type);
    // TODO: Replace this with SeqParam
    // NecroCoreAst* lam_arg_ast                = necro_core_ast_create_var(context->arena, necro_core_ast_symbol_create(context->arena, necro_intern_unique_string(context->intern, "wildcard"), context->base->unit_type->type), context->base->unit_type->type);
    NecroCoreAst* lam_arg_ast                = necro_core_ast_create_var(context->arena, lambda_arg_seq_param_symbol, context->base->seq_param_type->type);
    lam_arg_ast->var.ast_symbol->is_wildcard = true;
    NecroCoreAst* lam_ast                    = necro_core_ast_create_lam(context->arena, lam_arg_ast, let_ast);
    NecroType*    seq_con_type               = necro_type_fn_create(context->arena, lam_type, seq_type);
    NecroCoreAst* seq_ast                    = necro_core_ast_create_var(context->arena, context->base->seq_con->core_ast_symbol, seq_con_type);
    seq_ast                                  = necro_core_ast_create_app(context->arena, seq_ast, lam_ast);
    seq_ast->necro_type                      = seq_type;
    return ok(NecroCoreAst, seq_ast);
}

//--------------------
// Go
//--------------------
NecroResult(NecroCoreAst) necro_ast_transform_to_core_go(NecroCoreAstTransform* context, NecroAst* ast)
{
    if (ast == NULL)
        return ok(NecroCoreAst, NULL);

    switch (ast->type)
    {
    case NECRO_AST_DECLARATION_GROUP_LIST: return necro_ast_transform_to_core_declaration_group_list(context, ast, NULL); // NOTE: Seems a little sketchy...
    case NECRO_AST_VARIABLE:               return necro_ast_transform_to_core_var(context, ast);
    case NECRO_AST_CONID:                  return necro_ast_transform_to_core_con(context, ast);
    case NECRO_AST_CONSTANT:               return necro_ast_transform_to_core_lit(context, ast);
    case NECRO_AST_FUNCTION_EXPRESSION:    return necro_ast_transform_to_core_app(context, ast);
    case NECRO_AST_DATA_DECLARATION:       return necro_ast_transform_to_core_data_decl(context, ast);
    case NECRO_AST_SIMPLE_ASSIGNMENT:      return necro_ast_transform_to_core_simple_assignment(context, ast);
    case NECRO_AST_LET_EXPRESSION:         return necro_ast_transform_to_core_let(context, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:        return necro_ast_transform_to_core_rhs(context, ast);
    case NECRO_AST_LAMBDA:                 return necro_ast_transform_to_core_lambda(context, ast);
    case NECRO_AST_APATS_ASSIGNMENT:       return necro_ast_transform_to_core_apats_assignment(context, ast);
    case NECRO_AST_BIN_OP:                 return necro_ast_transform_to_core_bin_op(context, ast);
    case NECRO_AST_TUPLE:                  return necro_ast_transform_to_core_tuple(context, ast);
    case NECRO_AST_FOR_LOOP:               return necro_ast_transform_to_core_for_loop(context, ast);
    case NECRO_AST_WHILE_LOOP:             return necro_ast_transform_to_core_while_loop(context, ast);
    case NECRO_AST_EXPRESSION_ARRAY:       return necro_ast_transform_to_core_array(context, ast);
    case NECRO_AST_CASE:                   return necro_ast_transform_to_core_case(context, ast);
    case NECRO_AST_OP_LEFT_SECTION:        return necro_ast_transform_to_core_left_section(context, ast);
    case NECRO_AST_OP_RIGHT_SECTION:       return necro_ast_transform_to_core_right_section(context, ast);
    case NECRO_AST_IF_THEN_ELSE:           return necro_ast_transform_to_core_if_then_else(context, ast);
    case NECRO_AST_PAT_ASSIGNMENT:         return necro_ast_transform_to_core_pat_assignment(context, ast);
    case NECRO_AST_WILDCARD:               return necro_ast_transform_to_core_wildcard(context, ast);
    case NECRO_AST_SEQ_EXPRESSION:         return necro_ast_transform_to_core_seq_expression(context, ast);

    // Not Implemented / Supported
    case NECRO_AST_ARITHMETIC_SEQUENCE:
    case NECRO_AST_EXPRESSION_LIST:
    case NECRO_AST_DO:
    case NECRO_BIND_ASSIGNMENT:
        assert(false && "Not currently implemented, nor planned to be supported");
        return ok(NecroCoreAst, NULL);

    // Never Encounter
    case NECRO_AST_TYPE_CLASS_INSTANCE:
    case NECRO_AST_TOP_DECL:
    case NECRO_AST_APATS:
    case NECRO_AST_LIST_NODE:
    case NECRO_AST_TYPE_APP:
    case NECRO_AST_BIN_OP_SYM:
    case NECRO_AST_CONSTRUCTOR:
    case NECRO_AST_SIMPLE_TYPE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_FUNCTION_TYPE:
    case NECRO_AST_DECL:
    case NECRO_AST_TYPE_ATTRIBUTE:
        assert(false && "Should never encounter this ast type here");
        return ok(NecroCoreAst, NULL);

    default:
        assert(false && "Unrecognized ast type");
        return ok(NecroCoreAst, NULL);
    }
}

///////////////////////////////////////////////////////
// Pretty Printing
///////////////////////////////////////////////////////
#define NECRO_CORE_AST_INDENT 2
void necro_core_ast_pretty_print_go(NecroCoreAst* ast, size_t depth);
void necro_core_ast_pretty_print_top_level_go(NecroCoreAst* ast)
{
    if (ast == NULL)
        return;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_CASE:
    {
        printf("case \n");
        print_white_space(NECRO_CORE_AST_INDENT);
        print_white_space(NECRO_CORE_AST_INDENT);
        necro_core_ast_pretty_print_go(ast->case_expr.expr, 2 + NECRO_CORE_AST_INDENT);
        printf("\n");
        print_white_space(NECRO_CORE_AST_INDENT);
        printf(" of\n");
        NecroCoreAstList* alts = ast->case_expr.alts;
        assert(alts != NULL);
        while (alts != NULL)
        {
            print_white_space(2 + NECRO_CORE_AST_INDENT);
            if (alts->data->case_alt.pat == NULL)
                printf("_");
            else
                necro_core_ast_pretty_print_go(alts->data->case_alt.pat, 2 + NECRO_CORE_AST_INDENT);
            printf(" -> ");
            printf("\n");
            printf("\n");
            necro_core_ast_pretty_print_top_level_go(alts->data->case_alt.expr);
            alts = alts->next;
        }
        return;
    }
    case NECRO_CORE_AST_LET:
    {
        while (ast != NULL)
        {
            if (ast->ast_type != NECRO_CORE_AST_LET)
            {
                if (ast->ast_type == NECRO_CORE_AST_CASE)
                {
                    necro_core_ast_pretty_print_top_level_go(ast);
                }
                else
                {
                    necro_core_ast_pretty_print_go(ast, 0);
                }
                return;
            }
            // printf("\n");
            // print_white_space(depth);
            necro_core_ast_pretty_print_go(ast->let.bind, 0);
            // printf("\n");
            if (ast->let.expr == NULL)
            {
                printf("\n");
                return;
            }
            printf("\n\n");
            assert(ast->let.expr && "ast->let.expr is null!");
            /* necro_core_ast_pretty_print_go(ast->let.expr, depth); */
            ast = ast->let.expr;
        }
        return;
    }
    default:
        assert(false && "Unimplemented AST type in necro_core_ast_pretty_print_top_levl_go");
        return;
    }
}

void necro_core_ast_pretty_print_go(NecroCoreAst* ast, size_t depth)
{
    if (ast == NULL)
        return;
    switch (ast->ast_type)
    {
    case NECRO_CORE_AST_APP:
    {
        necro_core_ast_pretty_print_go(ast->app.expr1, depth);
        printf(" ");
        if (ast->app.expr2->ast_type == NECRO_CORE_AST_APP || ast->app.expr2->ast_type == NECRO_CORE_AST_LAM)
            printf("(");
        necro_core_ast_pretty_print_go(ast->app.expr2, depth);
        if (ast->app.expr2->ast_type == NECRO_CORE_AST_APP || ast->app.expr2->ast_type == NECRO_CORE_AST_LAM)
            printf(")");
        return;
    }
    case NECRO_CORE_AST_BIND:
    {
        if (depth == 0)
        {
            printf("%s :: ", ast->bind.ast_symbol->name->str);
            necro_type_print(ast->bind.ast_symbol->type);
            // printf("\n    (arity = %d, isRecursive = %s)\n", info->arity, ast->list.expr->bind.is_recursive ? "true" : "false");
            printf("\n");
            print_white_space(depth);
        }
        printf("%s ", ast->bind.ast_symbol->name->str);
        NecroCoreAst* lambdas = ast->bind.expr;
        while (lambdas->ast_type == NECRO_CORE_AST_LAM)
        {
            printf("%s ", lambdas->lambda.arg->var.ast_symbol->name->str);
            lambdas = lambdas->lambda.expr;
        }
        printf("= ");
        if (depth == 0)
        {
            printf("\n");
            print_white_space(NECRO_CORE_AST_INDENT);
        }
        necro_core_ast_pretty_print_go(lambdas, depth + NECRO_CORE_AST_INDENT);
        // TODO: Print metadata
        // NecroSymbolInfo* info = necro_symtable_get(symtable, ast->list.expr->bind.var.id);
        // printf("\n(arity = %d, isRecursive = %s, stateType = ", info->arity, ast->list.expr->bind.is_recursive ? "true" : "false");
        // switch (info->state_type)
        // {
        // case NECRO_STATE_POLY:
        //     printf(" Poly)");
        //     break;
        // case NECRO_STATE_CONSTANT:
        //     printf(" Constant)");
        //     break;
        // case NECRO_STATE_POINTWISE:
        //     printf(" Pointwise)");
        //     break;
        // case NECRO_STATE_STATEFUL:
        //     printf(" Stateful)");
        //     break;
        // }
        return;
    }
    case NECRO_CORE_AST_LAM:
    {
        printf("\\%s -> ", ast->lambda.arg->var.ast_symbol->name->str);
        necro_core_ast_pretty_print_go(ast->lambda.expr, depth);
        return;
    }
    case NECRO_CORE_AST_CASE:
    {
        printf("case ");
        necro_core_ast_pretty_print_go(ast->case_expr.expr, depth);
        printf(" of\n");
        NecroCoreAstList* alts = ast->case_expr.alts;
        while (alts != NULL)
        {
            print_white_space(depth + NECRO_CORE_AST_INDENT);
            if (alts->data->case_alt.pat == NULL)
                printf("_");
            else
                necro_core_ast_pretty_print_go(alts->data->case_alt.pat, depth + NECRO_CORE_AST_INDENT);
            printf(" -> ");
            necro_core_ast_pretty_print_go(alts->data->case_alt.expr, depth + NECRO_CORE_AST_INDENT);
            if (alts->next != NULL)
                printf("\n");
            alts = alts->next;
        }
        return;
    }
    case NECRO_CORE_AST_LET:
    {
        while (ast != NULL)
        {
            if (ast->ast_type != NECRO_CORE_AST_LET)
            {
                necro_core_ast_pretty_print_go(ast, depth);
                return;
            }
            // printf("\n");
            // print_white_space(depth);
            if (depth > 0)
                printf("let ");
            necro_core_ast_pretty_print_go(ast->let.bind, depth);
            // printf("\n");
            if (ast->let.expr == NULL)
            {
                printf("\n");
                return;
            }
            if (depth > 0)
                printf(" in\n");
            else
                printf("\n\n");
            print_white_space(depth);
            assert(ast->let.expr && "ast->let.expr is null!");
            /* necro_core_ast_pretty_print_go(ast->let.expr, depth); */
            ast = ast->let.expr;
        }
        return;
    }
    case NECRO_CORE_AST_VAR:
    {
        printf("%s", ast->var.ast_symbol->name->str);
        return;
    }
    case NECRO_CORE_AST_LIT:
    {
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER:         printf("%" PRId64 "", ast->lit.int_literal);       return;
        case NECRO_AST_CONSTANT_INTEGER_PATTERN: printf("%" PRId64 "", ast->lit.int_literal);       return;
        case NECRO_AST_CONSTANT_FLOAT:           printf("%f", ast->lit.float_literal);       return;
        case NECRO_AST_CONSTANT_FLOAT_PATTERN:   printf("%f", ast->lit.float_literal);       return;
        case NECRO_AST_CONSTANT_CHAR:            printf("%c", ast->lit.char_literal);        return;
        case NECRO_AST_CONSTANT_STRING:          printf("%s", ast->lit.string_literal->str); return;
        case NECRO_AST_CONSTANT_ARRAY:
        {
            printf("{ ");
            NecroCoreAstList* elements = ast->lit.array_literal_elements;
            while (elements != NULL)
            {
                necro_core_ast_pretty_print_go(elements->data, depth);
                if (elements->next != NULL)
                    printf(", ");
                elements = elements->next;
            }
            printf(" }");
            return;
        }
        default:
            return;
        }
        return;
    }
    case NECRO_CORE_AST_DATA_DECL:
    {
        printf("data %s :: ", ast->data_decl.ast_symbol->name->str);
        necro_type_print(ast->data_decl.ast_symbol->type->kind);
        printf(" where\n");
        NecroCoreAstList* cons = ast->data_decl.con_list;
        while (cons != NULL)
        {
            necro_core_ast_pretty_print_go(cons->data, depth + NECRO_CORE_AST_INDENT);
            if (cons->next != NULL)
                printf("\n");
            cons = cons->next;
        }
        return;
    }
    case NECRO_CORE_AST_DATA_CON:
    {
        print_white_space(depth);
        printf("%s :: ", ast->data_con.ast_symbol->name->str);
        necro_type_print(ast->data_con.type);
        return;
    }
    case NECRO_CORE_AST_LOOP:
        printf("loop ");
        necro_core_ast_pretty_print_go(ast->loop.value_pat, depth);
        printf(" = ");
        necro_core_ast_pretty_print_go(ast->loop.value_init, depth);
        if (ast->loop.loop_type == NECRO_LOOP_FOR)
        {
            printf(" for ");
            necro_core_ast_pretty_print_go(ast->loop.for_loop.index_pat, depth);
            printf(" <- ");
            necro_core_ast_pretty_print_go(ast->loop.for_loop.range_init, depth);
        }
        else
        {
            printf(" while ");
            necro_core_ast_pretty_print_go(ast->loop.while_loop.while_expression, depth);
        }
        printf(" do");
        print_white_space(depth + NECRO_CORE_AST_INDENT);
        necro_core_ast_pretty_print_go(ast->loop.do_expression, depth + NECRO_CORE_AST_INDENT);
        return;
    default:
        assert(false && "Unimplemented AST type in necro_core_ast_pretty_print_go");
        return;
    }
}

void necro_core_ast_pretty_print(NecroCoreAst* ast)
{
    necro_core_ast_pretty_print_top_level_go(ast);
    printf("\n");
}

#define NECRO_CORE_AST_VERBOSE 0
void necro_core_test_result(const char* test_name, const char* str)
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
    necro_dependency_analyze(info, &intern, &base, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast));
    unwrap(void, necro_ast_transform_to_core(info, &intern, &base, &ast, &core_ast));
    unwrap(void, necro_core_infer(&intern, &base, &core_ast));

    // Print
#if NECRO_CORE_AST_VERBOSE
    printf("\n");
    printf("=======================================================================\n");
    necro_core_ast_pretty_print(core_ast.root);
    printf("=======================================================================\n");
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

void necro_core_ast_test()
{
    necro_announce_phase("Core");

    {
        const char* test_name   = "Poly 0";
        const char* test_source = ""
            "myCoolSynth :: Mono Audio\n"
            "myCoolSynth = poly (fromInt .> saw) [440 220 _ <110 55 _ 330>]\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_core_test_result(test_name, test_source);
    }

    // if (true)
    //     return;

    {
        const char* test_name   = "Basic 1";
        const char* test_source = ""
            "x = True\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Basic 2";
        const char* test_source = ""
            "f x y = x || y\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Basic 3";
        const char* test_source = ""
            "data Jump = In | The | Fire\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Basic 4";
        const char* test_source = ""
            "data Polymorph a = Bear a | Wolf a | Hydra a a a\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Basic 5";
        const char* test_source = ""
            "f x y = (x, y)\n"
            "z = f True () \n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Simple Loop";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "addItUp :: Int\n"
            "addItUp = loop x = 0 for i <- tenTimes do\n"
            "  x + 1\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Let It Work, Please";
        const char* test_source = ""
            "z :: Float -> Float\n"
            "z y = x + y where x = 100\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Let There be Lets";
        const char* test_source = ""
            "doStuff :: Float -> Float\n"
            "doStuff w = let x = 99 in w + x * y / z\n"
            "  where\n"
            "    y = 100\n"
            "    z = 200\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Maybe Nothing";
        const char* test_source = ""
            "m :: *Maybe Bool\n"
            "m = Just False\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case Closed";
        const char* test_source = ""
            "caseTest x =\n"
            "  case x of\n"
            "    True  -> False\n"
            "    _     -> True\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Case Open";
        const char* test_source = ""
            "caseTest x =\n"
            "  case x of\n"
            "    True  -> False\n"
            "    y     -> y\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Justified";
        const char* test_source = ""
            "caseTest t =\n"
            "  case t of\n"
            "    Just (Just x) -> x\n"
            "    _             -> ()\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Tuples Are Terrifying";
        const char* test_source = ""
            "caseTest t =\n"
            "  case t of\n"
            "    (x, y) -> x && y\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Arrayed";
        const char* test_source = ""
            "a = { True, False, True, True }\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "If then else doom";
        const char* test_source = ""
            "ifTest :: Bool -> Int\n"
            "ifTest t = if t then 1 else 0\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "If then else final doom forever";
        const char* test_source = ""
            "ifTest :: Bool -> Maybe Bool\n"
            "ifTest t = if t then (Just True) else Nothing\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "If then If Then else then doom then doom";
        const char* test_source = ""
            "ifTest :: Bool -> Bool\n"
            "ifTest t = if t then (if False then True else False) else (if False then True else False)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda Curry";
        const char* test_source = ""
            "lambdaCurry :: Int -> Int\n"
            "lambdaCurry = \\a -> a + 1\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Left Section";
        const char* test_source = ""
            "leftSection :: Int -> Int\n"
            "leftSection = (2 +)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Maybe Left Section";
        const char* test_source = ""
            "maybeLeftSection :: Maybe (Int -> Int)\n"
            "maybeLeftSection = Just (2 +)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Right Section";
        const char* test_source = ""
            "rightSection :: Int -> Int\n"
            "rightSection  = (+ 2)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Maybe Right Section";
        const char* test_source = ""
            "maybeRightSection :: Maybe (Int -> Int)\n"
            "maybeRightSection = Just (+ 2)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Pat Assignment";
        const char* test_source = ""
            "(l, r) = (True, False)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Last Pat Assignment";
        const char* test_source = ""
            "a :: Int\n"
            "a = 11\n"
            "(l, r) = (True, False)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Penultimate Pat Assignment";
        const char* test_source = ""
            "(l, r) = (True, False)\n"
            "a :: Int\n"
            "a = 11\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Mixed Pat Assignments";
        const char* test_source = ""
            "(l, r) = (True, False)\n"
            "a :: Int\n"
            "a = 11\n"
            "(e, w) = (True, False)\n"
            "(s, n) = (True, False)\n"
            "b :: Int\n"
            "b = 13\n"
            "(u, d) = (True, False)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Nested Pat Assignment";
        const char* test_source = ""
            "(l, r) = (n, s)\n"
            "   where\n"
            "       (s, n) = (w, e)\n"
            "       t = True\n"
            "       f = False\n"
            "       (e, w) = (t, f)\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Data Pat Assignment";
        const char* test_source = ""
            "data Book n = Pages n\n"
            "(Pages numPages) = Pages 666\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Rec 3.1";
        const char* test_source = ""
            "counter :: ((Int, Int), (Int, Int))\n"
            "counter = x where\n"
            "  x ~ ((0, 1), (2, 3)) =\n"
            "    case (gt mouseX 50, x) of\n"
            "      (True,  ((x, y), r)) -> (r, (y, x))\n"
            "      (False, ((z, w), r)) -> ((w, z), r)\n"
            "main :: *World -> *World\n"
            "main w = w\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Bind Rec";
        const char* test_source = ""
            "a :: Int\n"
            "a = b\n"
            "   where\n"
            "       b ~ 0 = c + 1\n"
            "       c ~ 1 = b - 2\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Apats Tuple arg";
        const char* test_source = ""
            "addPair :: (Int, Int) -> Int\n"
            "addPair (a, b) = a + b\n";
        necro_core_test_result(test_name, test_source);
    }


    {
        const char* test_name   = "Apats 2 Tuple args";
        const char* test_source = ""
            "addPairs :: (Int, Int) -> (Int, Int) -> Int\n"
            "addPairs (a, b) (c, d) = a + b + c + d\n";
        necro_core_test_result(test_name, test_source);
    }


    {
        const char* test_name   = "Apats 4 tuple args";
        const char* test_source = ""
            "concatPairs :: (Int, Int) -> (Int, Int) -> (Int, Int) -> (Int, Int) -> Int\n"
            "concatPairs (a, b) (c, d) (e, f) (g, h) = a + b - c + d - e + f - g + h\n";
        necro_core_test_result(test_name, test_source);
    }

    // @Curtis: This throws an assert in inference (pre-core) about a kind mismatch, what's up with that?
    /* { */
    /*     const char* test_name   = "Apats 4 tuple args to 8 tuple"; */
    /*     const char* test_source = "" */
    /*         "concatPairs :: (Int, Int) -> (Int, Int) -> (Int, Int) -> (Int, Int) -> (Int, Int, Int, Int, Int, Int, Int, Int)\n" */
    /*         "concatPairs (a, b) (c, d) (e, f) (g, h) = (a, b, c, d, e, f ,g, h)\n"; */
    /*     necro_core_test_result(test_name, test_source); */
    /* } */

    {
        const char* test_name   = "Apats 2 tuple args around var";
        const char* test_source = ""
            "addPairs :: (Int, Int) -> Int -> (Int, Int) -> Int\n"
            "addPairs (a, b) c (d, e) = a + b + c + d + e\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Apats with data type";
        const char* test_source = ""
            "data Grid = GridXY Int Int\n"
            "addPairs :: (Int, Int) -> Grid -> (Int, Int) -> Int\n"
            "addPairs (a, b) (GridXY c d) (e, f) = a + b + c + d + e + f\n";
        necro_core_test_result(test_name, test_source);
    }

    { // NOTE! This shouldn't compile because of a non-exhaustive pattern. @Curtis: Should it fail even earlier, say in the AST parser?
        const char* test_name   = "Apats with data type and multi constructor";
        const char* test_source = ""
            "data Book = Pages Int\n"
            "addPairs :: (Int, Int) -> Book -> (Int, Int) -> Bool -> Int\n"
            "addPairs (a, b) (Pages c) (d, e) True = a + b + c + d + e\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Apats with data enum type";
        const char* test_source = ""
            "data Universe = Infinite\n"
            "addPairs :: (Int, Int) -> Universe -> (Int, Int) -> Int\n"
            "addPairs (a, b) Infinite (c, d) = a + b + c + d\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Nested Apats 4 tuple args";
        const char* test_source = ""
            "mathPairs :: (Int, Int) -> (Int, Int) -> (Int, Int) -> (Int, Int) -> Int\n"
            "mathPairs (a, b) (c, d) (e, f) g = a + b - c + d - e + f - (mulTuple  g)\n"
            "   where\n"
            "       mulTuple (x, y) = x * y\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Nested Apats 4 tuple args and data";
        const char* test_source = ""
            "data Grid = GridXY Int Int\n"
            "mathPairs :: (Int, Int) -> (Int, Int) -> (Int, Int) -> (Int, Int) -> Int\n"
            "mathPairs (a, b) (c, d) (e, f) (g, h) = a + b - c + d - e + f - (subGrid (GridXY h g))\n"
            "   where\n"
            "       subGrid (GridXY x y) = x - y\n";
        necro_core_test_result(test_name, test_source);
    }

    {
        const char* test_name   = "Lambda Nested Apats 4 tuple args and data";
        const char* test_source = ""
            "data Grid = GridXY Int Int\n"
            "mathPairs :: (Int, Int) -> (Int, Int) -> (Int, Int) -> (Int, Int) -> Int\n"
            "mathPairs = \\(a, b) (c, d) (e, f) (g, h) -> a + b - c + d - e + f - (subGrid (GridXY h g))\n"
            "   where\n"
            "       subGrid (GridXY x y) = x - y\n";
        necro_core_test_result(test_name, test_source);
    }
}

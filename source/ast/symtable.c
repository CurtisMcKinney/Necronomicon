/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "type.h"
#include "type_class.h"
#include "symtable.h"

NecroScope necro_global_scope = { 0 };

//=====================================================
// NecroScopedSymTable
//=====================================================
#define NECRO_SCOPE_INITIAL_SIZE 8

inline NecroScope* necro_scope_create(NecroPagedArena* arena, NecroScope* parent)
{
    NecroScope* scope             = necro_paged_arena_alloc(arena, sizeof(NecroScope));
    scope->parent                 = parent;
    scope->buckets                = necro_paged_arena_alloc(arena, NECRO_SCOPE_INITIAL_SIZE * sizeof(NecroScopeNode));
    scope->size                   = NECRO_SCOPE_INITIAL_SIZE;
    scope->count                  = 0;
    scope->last_introduced_symbol = NULL;
    for (size_t bucket = 0; bucket < scope->size; ++bucket)
        scope->buckets[bucket] = (NecroScopeNode) { .symbol = NULL, .ast_symbol = NULL };
    return scope;
}

NecroScopedSymTable necro_scoped_symtable_empty()
{
    return (NecroScopedSymTable)
    {
        .arena               = necro_paged_arena_empty(),
        .top_scope           = NULL,
        .current_scope       = NULL,
        .top_type_scope      = NULL,
        .current_type_scope  = NULL,
    };
}

NecroScopedSymTable necro_scoped_symtable_create()
{
    NecroPagedArena  arena       = necro_paged_arena_create();
    NecroScope*      top_scope   = necro_scope_create(&arena, NULL);
    NecroScope*      type_scope  = necro_scope_create(&arena, NULL);
    return (NecroScopedSymTable)
    {
        .arena               = arena,
        .top_scope           = top_scope,
        .current_scope       = top_scope,
        .top_type_scope      = type_scope,
        .current_type_scope  = type_scope,
    };
}

void necro_scoped_symtable_destroy(NecroScopedSymTable* table)
{
    necro_paged_arena_destroy(&table->arena);
}

void necro_scoped_symtable_new_scope(NecroScopedSymTable* table)
{
    table->current_scope = necro_scope_create(&table->arena, table->current_scope);
}

void necro_scoped_symtable_pop_scope(NecroScopedSymTable* table)
{
    table->current_scope = table->current_scope->parent;
}

void necro_scoped_symtable_new_type_scope(NecroScopedSymTable* table)
{
    table->current_type_scope = necro_scope_create(&table->arena, table->current_type_scope);
}

void necro_scoped_symtable_pop_type_scope(NecroScopedSymTable* table)
{
    table->current_type_scope = table->current_type_scope->parent;
}

static inline void necro_scope_grow(NecroScope* scope, NecroPagedArena* arena)
{
    assert(scope != NULL);
    assert(scope->buckets != NULL);
    assert(scope->count < scope->size);
    NecroScopeNode* prev_buckets = scope->buckets;
    size_t          prev_size    = scope->size;
    size_t          prev_count   = scope->count;
    UNUSED(prev_count);
    scope->size                 *= 2;
    scope->buckets               = necro_paged_arena_alloc(arena, scope->size * sizeof(NecroScopeNode));
    scope->count                 = 0;
    for (size_t bucket = 0; bucket < scope->size; ++bucket)
        scope->buckets[bucket] = (NecroScopeNode) { .symbol = NULL, .ast_symbol = NULL };
    for (size_t bucket = 0; bucket < prev_size; ++bucket)
    {
        NecroSymbol symbol = prev_buckets[bucket].symbol;
        if (symbol == NULL)
            continue;
        necro_scope_insert_ast_symbol(arena, scope, prev_buckets[bucket].ast_symbol);
    }
    assert(scope->count == prev_count);
}

void necro_scope_insert_ast_symbol(NecroPagedArena* arena, NecroScope* scope, NecroAstSymbol* ast_symbol)
{
    assert(scope != NULL);
    assert(scope->buckets != NULL);
    assert(scope->count < scope->size);
    NecroSymbol symbol = ast_symbol->source_name;
    if (scope->count >= scope->size / 2)
        necro_scope_grow(scope, arena);
    size_t bucket = symbol->hash & (scope->size - 1);
    while (scope->buckets[bucket].symbol != NULL)
    {
        if (scope->buckets[bucket].symbol == symbol)
        {
            return;
        }
        bucket = (bucket + 1) & (scope->size - 1);
    }
    scope->buckets[bucket].symbol     = symbol;
    scope->buckets[bucket].ast_symbol = ast_symbol;
    scope->count++;
}

NecroAstSymbol* necro_scope_find_in_this_scope_ast_symbol(NecroScope* scope, NecroSymbol symbol)
{
    assert(scope != NULL);
    assert(scope->buckets != NULL);
    assert(scope->count < scope->size);

    for (size_t bucket = symbol->hash & (scope->size - 1); scope->buckets[bucket].symbol != NULL; bucket = (bucket + 1) & (scope->size - 1))
    {
        if (scope->buckets[bucket].symbol == symbol)
        {
            return scope->buckets[bucket].ast_symbol;
        }
    }
    return NULL;
}

bool necro_scope_contains(NecroScope* scope, NecroSymbol symbol)
{
    return necro_scope_find_in_this_scope_ast_symbol(scope, symbol) != NULL;
}

bool necro_scope_is_subscope_of(NecroScope* super_scope, NecroScope* maybe_sub_scope)
{
    while (maybe_sub_scope != NULL)
    {
        if (maybe_sub_scope == super_scope)
            return true;
        maybe_sub_scope = maybe_sub_scope->parent;
    }
    return false;
}

NecroAstSymbol* necro_scope_find_ast_symbol(NecroScope* scope, NecroSymbol symbol)
{
    NecroScope*     current_scope = scope;
    NecroAstSymbol* result        = NULL;
    while (current_scope != NULL)
    {
        if ((result = necro_scope_find_in_this_scope_ast_symbol(current_scope, symbol)) != NULL)
        {
            return result;
        }
        current_scope = current_scope->parent;
    }
    return NULL;
}

//=====================================================
// Build Scopes
//=====================================================
void necro_build_scopes_go(NecroScopedSymTable* scoped_symtable, NecroAst* input_node)
{
    if (input_node == NULL)
        return;
    input_node->scope = scoped_symtable->current_scope;
    switch (input_node->type)
    {
    case NECRO_AST_TOP_DECL:
        while (input_node != NULL)
        {
            input_node->scope = scoped_symtable->current_scope;
            necro_build_scopes_go(scoped_symtable, input_node->top_declaration.declaration);
            // necro_build_scopes_go(scoped_symtable, input_node->top_declaration.next_top_decl);
            input_node        = input_node->top_declaration.next_top_decl;
        }
        break;
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        necro_build_scopes_go(scoped_symtable, input_node->bin_op.lhs);
        necro_build_scopes_go(scoped_symtable, input_node->bin_op.rhs);
        break;
    case NECRO_AST_IF_THEN_ELSE:
        necro_build_scopes_go(scoped_symtable, input_node->if_then_else.if_expr);
        necro_build_scopes_go(scoped_symtable, input_node->if_then_else.then_expr);
        necro_build_scopes_go(scoped_symtable, input_node->if_then_else.else_expr);
        break;
    case NECRO_AST_DECL:
        necro_build_scopes_go(scoped_symtable, input_node->declaration.declaration_impl);
        necro_build_scopes_go(scoped_symtable, input_node->declaration.next_declaration);
        break;
    case NECRO_AST_DECLARATION_GROUP_LIST:
        necro_build_scopes_go(scoped_symtable, input_node->declaration_group_list.declaration_group);
        necro_build_scopes_go(scoped_symtable, input_node->declaration_group_list.next);
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->simple_assignment.initializer);
        necro_build_scopes_go(scoped_symtable, input_node->simple_assignment.rhs);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->apats_assignment.apats);
        necro_build_scopes_go(scoped_symtable, input_node->apats_assignment.rhs);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        necro_build_scopes_go(scoped_symtable, input_node->pat_assignment.pat);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->pat_assignment.rhs);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->right_hand_side.declarations);
        necro_build_scopes_go(scoped_symtable, input_node->right_hand_side.expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_LET_EXPRESSION:
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->let_expression.declarations);
        necro_build_scopes_go(scoped_symtable, input_node->let_expression.expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_build_scopes_go(scoped_symtable, input_node->fexpression.aexp);
        necro_build_scopes_go(scoped_symtable, input_node->fexpression.next_fexpression);
        break;

    case NECRO_AST_VARIABLE:
        switch (input_node->variable.var_type)
        {
        case NECRO_VAR_VAR:                  break;
        case NECRO_VAR_TYPE_FREE_VAR:        input_node->scope = scoped_symtable->current_type_scope; break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: input_node->scope = scoped_symtable->current_type_scope; break;
        case NECRO_VAR_SIG:                  break;
        case NECRO_VAR_CLASS_SIG:            break;
        case NECRO_VAR_DECLARATION:          input_node->variable.ast_symbol->is_top_level = scoped_symtable->current_scope == NULL; break;
        }
        if (input_node->variable.initializer != NULL)
        {
            necro_build_scopes_go(scoped_symtable, input_node->variable.initializer);
        }
        break;

    case NECRO_AST_APATS:
        necro_build_scopes_go(scoped_symtable, input_node->apats.apat);
        necro_build_scopes_go(scoped_symtable, input_node->apats.next_apat);
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->lambda.apats);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->lambda.expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_DO:
    {
        NecroAst* current_statement = input_node->do_statement.statement_list;
        size_t                 pop_count = 0;
        while (current_statement != NULL)
        {
            assert(current_statement->type == NECRO_AST_LIST_NODE);
            if (current_statement->list.item->type == NECRO_BIND_ASSIGNMENT ||
                current_statement->list.item->type == NECRO_PAT_BIND_ASSIGNMENT ||
                current_statement->list.item->type == NECRO_AST_DECL)
            {
                necro_scoped_symtable_new_scope(scoped_symtable);
                pop_count++;
            }
            necro_build_scopes_go(scoped_symtable, current_statement->list.item);
            current_statement = current_statement->list.next_item;
        }
        for (size_t i = 0; i < pop_count; ++i)
            necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    }
    case NECRO_AST_LIST_NODE:
        necro_build_scopes_go(scoped_symtable, input_node->list.item);
        necro_build_scopes_go(scoped_symtable, input_node->list.next_item);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        necro_build_scopes_go(scoped_symtable, input_node->expression_list.expressions);
        break;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_build_scopes_go(scoped_symtable, input_node->expression_array.expressions);
        break;
    case NECRO_AST_SEQ_EXPRESSION:
        necro_build_scopes_go(scoped_symtable, input_node->sequence_expression.expressions);
        break;
    case NECRO_AST_TUPLE:
        necro_build_scopes_go(scoped_symtable, input_node->tuple.expressions);
        break;
    case NECRO_BIND_ASSIGNMENT:
        necro_build_scopes_go(scoped_symtable, input_node->bind_assignment.expression);
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_build_scopes_go(scoped_symtable, input_node->pat_bind_assignment.pat);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->pat_bind_assignment.expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_build_scopes_go(scoped_symtable, input_node->arithmetic_sequence.from);
        necro_build_scopes_go(scoped_symtable, input_node->arithmetic_sequence.then);
        necro_build_scopes_go(scoped_symtable, input_node->arithmetic_sequence.to);
        break;
    case NECRO_AST_CASE:
        necro_build_scopes_go(scoped_symtable, input_node->case_expression.expression);
        necro_build_scopes_go(scoped_symtable, input_node->case_expression.alternatives);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->case_alternative.pat);
        necro_build_scopes_go(scoped_symtable, input_node->case_alternative.body);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_FOR_LOOP:
        necro_build_scopes_go(scoped_symtable, input_node->for_loop.range_init);
        necro_build_scopes_go(scoped_symtable, input_node->for_loop.value_init);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->for_loop.index_apat);
        necro_build_scopes_go(scoped_symtable, input_node->for_loop.value_apat);
        necro_build_scopes_go(scoped_symtable, input_node->for_loop.expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_WHILE_LOOP:
        necro_build_scopes_go(scoped_symtable, input_node->while_loop.value_init);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->while_loop.value_apat);
        necro_build_scopes_go(scoped_symtable, input_node->while_loop.while_expression);
        necro_build_scopes_go(scoped_symtable, input_node->while_loop.do_expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_CONID:
        switch (input_node->conid.con_type)
        {
        case NECRO_CON_VAR:              break;
        case NECRO_CON_TYPE_VAR:         input_node->scope = scoped_symtable->current_type_scope; break;
        case NECRO_CON_TYPE_DECLARATION: input_node->scope = scoped_symtable->current_type_scope; break;
        case NECRO_CON_DATA_DECLARATION: break;
        }
        break;

    case NECRO_AST_BIN_OP_SYM:
        necro_build_scopes_go(scoped_symtable, input_node->bin_op_sym.left);
        necro_build_scopes_go(scoped_symtable, input_node->bin_op_sym.op);
        necro_build_scopes_go(scoped_symtable, input_node->bin_op_sym.right);
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_build_scopes_go(scoped_symtable, input_node->op_left_section.left);
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_build_scopes_go(scoped_symtable, input_node->op_right_section.right);
        break;
    case NECRO_AST_TYPE_APP:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->type_app.ty);
        necro_build_scopes_go(scoped_symtable, input_node->type_app.next_ty);
        break;
    case NECRO_AST_CONSTRUCTOR:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->constructor.conid);
        necro_build_scopes_go(scoped_symtable, input_node->constructor.arg_list);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->simple_type.type_con);
        necro_build_scopes_go(scoped_symtable, input_node->simple_type.type_var_list);
        break;
    case NECRO_AST_DATA_DECLARATION:
        input_node->scope = scoped_symtable->top_scope;
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->data_declaration.simpletype);
        necro_build_scopes_go(scoped_symtable, input_node->data_declaration.constructor_list);
        necro_build_scopes_go(scoped_symtable, input_node->data_declaration.deriving_list);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->type_class_context.conid);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_context.varid);
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.context);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.tycls);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.tyvar);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);

        // necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.declarations);
        // necro_scoped_symtable_pop_type_scope(scoped_symtable);
        // TODO: Get AST arena into here and use that instead of the scope arena!
        // necro_create_dictionary_data_declaration(&scoped_symtable->arena, scoped_symtable->global_table->intern, input_node);
        // necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.dictionary_data_declaration);
        // necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.context);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.qtycls);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.inst);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        // necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.declarations);
        // necro_scoped_symtable_pop_type_scope(scoped_symtable);
        // necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->type_signature.var);
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_signature.context);
        necro_build_scopes_go(scoped_symtable, input_node->type_signature.type);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_EXPR_TYPE_SIGNATURE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->expr_type_signature.expression);
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->expr_type_signature.context);
        necro_build_scopes_go(scoped_symtable, input_node->expr_type_signature.type);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->function_type.type);
        necro_build_scopes_go(scoped_symtable, input_node->function_type.next_on_arrow);
        break;
    case NECRO_AST_TYPE_ATTRIBUTE:
        necro_build_scopes_go(scoped_symtable, input_node->attribute.attribute_type);
        break;
    default:
        assert(false);
        break;
    }
}

void necro_build_scopes(NecroCompileInfo info, NecroScopedSymTable* table, NecroAstArena* ast)
{
    necro_build_scopes_go(table, ast->root);
    if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_BUILD_SCOPES && info.verbosity > 0))
        necro_ast_arena_print(ast);
}

void necro_scope_print(NecroScope* scope, size_t whitespace)
{
    assert(scope != NULL);

    // print_white_space(whitespace);
    // printf("NecroScope\n");

    // print_white_space(whitespace);
    // printf("{\n");

    // print_white_space(whitespace + 4);
    // printf("size:  %zu\n", scope->size);

    // print_white_space(whitespace + 4);
    // printf("count: %zu\n", scope->count);

    // print_white_space(whitespace + 4);
    // printf("data:\n");

    // print_white_space(whitespace + 4);
    printf("[\n");

    for (size_t bucket = 0; bucket < scope->size; ++bucket)
    {
        if (scope->buckets[bucket].ast_symbol != NULL)
        {
            necro_ast_symbol_print_type_and_kind(scope->buckets[bucket].ast_symbol, whitespace + 4);
            printf("\n");
        }
    }

    // print_white_space(whitespace + 4);
    printf("]\n");

    // print_white_space(whitespace);
    // printf("}\n");
}

void necro_scoped_symtable_print_type_scope(NecroScopedSymTable* table)
{
    necro_scope_print(table->top_type_scope, 0);
}

void necro_scoped_symtable_print_top_scopes(NecroScopedSymTable* table)
{
    printf("Types:\n");
    necro_scope_print(table->top_type_scope, 0);
    printf("\nExpressions:\n");
    necro_scope_print(table->top_scope, 0);
}

void necro_scoped_symtable_print(NecroScopedSymTable* table)
{
    assert(table != NULL);
    printf("NecroScopedSymtable\n{\n");
    printf("    scopes (from this leaf only):\n");
    NecroScope* current_scope = table->current_scope;
    while (current_scope != NULL)
    {
        necro_scope_print(current_scope, 8);
        current_scope = current_scope->parent;
    }
    printf("}\n");
}

//=====================================================
// Testing
//=====================================================
void necro_scoped_symtable_test()
{
    necro_announce_phase("NecroScopedSymTable");

    NecroIntern         intern          = necro_intern_create();
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroScope*         top_scope       = scoped_symtable.current_scope;

    // Push Test
    necro_scoped_symtable_new_scope(&scoped_symtable);
    if (scoped_symtable.current_scope->parent != NULL)
        printf("Push test:      passed\n");
    else
        printf("Push test:      FAILED\n");

    // Pop Test
    necro_scoped_symtable_pop_scope(&scoped_symtable);
    if (scoped_symtable.current_scope->parent == NULL && scoped_symtable.current_scope == top_scope)
        printf("Pop test:       passed\n");
    else
        printf("Pop test:       FAILED\n");

    necro_intern_destroy(&intern);
}

NecroAstSymbol* necro_symtable_get_top_level_ast_symbol(NecroScopedSymTable* scoped_symtable, NecroSymbol symbol)
{
    return necro_scope_find_ast_symbol(scoped_symtable->top_scope, symbol);
}

NecroAstSymbol* necro_symtable_get_type_ast_symbol(NecroScopedSymTable* scoped_symtable, NecroSymbol symbol)
{
    return necro_scope_find_ast_symbol(scoped_symtable->top_type_scope, symbol);
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "type.h"
#include "type_class.h"
#include "symtable.h"
#include "core/core.h"

// Constants
#define NECRO_SYMTABLE_INITIAL_SIZE 512

NecroSymTable necro_create_symtable(NecroIntern* intern)
{
    NecroSymbolInfo* data = calloc(NECRO_SYMTABLE_INITIAL_SIZE, sizeof(NecroSymbolInfo));
    if (data == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating data in necro_create_symtable()\n");
        exit(1);
    }
    return (NecroSymTable)
    {
        data,
        NECRO_SYMTABLE_INITIAL_SIZE,
        1,
        intern,
    };
}

void necro_destroy_symtable(NecroSymTable* table)
{
    if (table == NULL || table->data == NULL)
        return;
    free(table->data);
    table->data  = NULL;
    table->size  = 0;
    table->count = 0;
}

NecroSymbolInfo necro_create_initial_symbol_info(NecroSymbol symbol, NecroSourceLoc source_loc, NecroScope* scope, NecroIntern* intern)
{
    return (NecroSymbolInfo)
    {
        .name                    = symbol,
        .string_name             = necro_intern_get_string(intern, symbol),
        .id                      = 0,
        .con_num                 = 0,
        .is_enum                 = false,
        .source_loc              = source_loc,
        .scope                   = scope,
        .ast                     = NULL,
        .core_ast                = NULL,
        .declaration_group       = NULL,
        .optional_type_signature = NULL,
        .type                    = NULL,
        .closure_type            = NULL,
        .type_status             = NECRO_TYPE_UNCHECKED,
        .method_type_class       = NULL,
        .type_class              = NULL,
        .type_class_instance     = NULL,
        .persistent_slot         = 0,
        .is_constructor          = false,
        .is_recursive            = false,
        .arity                   = -1,
        .necro_machine_ast       = NULL,
        .state_type              = NECRO_STATE_CONSTANT,
    };
}

inline void necro_symtable_grow(NecroSymTable* table)
{
    table->size *= 2;
    NecroSymbolInfo* new_data = realloc(table->data, table->size * sizeof(NecroSymbolInfo));
    if (new_data == NULL)
    {
        if (table->data != NULL)
            free(table->data);
        fprintf(stderr, "Malloc returned NULL in necro_symtable_grow!\n");
        exit(1);
    }
    table->data = new_data;
    assert(table->data != NULL);
}

NecroID necro_symtable_insert(NecroSymTable* table, NecroSymbolInfo info)
{
    if (table->count >= table->size)
        necro_symtable_grow(table);
    assert(table->count < table->size);
    info.id.id = table->count;
    table->data[table->count] = info;
    table->count++;
    return (NecroID) { table->count - 1 };
}

NecroSymbolInfo* necro_symtable_get(NecroSymTable* table, NecroID id)
{
    if (id.id < table->count)
    {
        return table->data + id.id;
    }
    else
    {
        // assert(false);
        return NULL;
    }
}

void necro_symtable_info_print(NecroSymbolInfo info, NecroIntern* intern, size_t whitespace)
{
    print_white_space(whitespace);
    printf("NecroSymbolInfo\n");

    print_white_space(whitespace);
    printf("{\n");

    print_white_space(whitespace + 4);
    printf("name:       %s\n",necro_intern_get_string(intern, info.name));

    print_white_space(whitespace + 4);
    printf("id:         %d\n", info.id.id);

    // print_white_space(whitespace + 4);
    // printf("size:       %d\n", info.data_size);

    print_white_space(whitespace + 4);
    printf("source loc: { line: %d, character: %d, pos: %d }\n", info.source_loc.line, info.source_loc.character, info.source_loc.pos);

    print_white_space(whitespace + 4);
    printf("scope:      %p\n", info.scope);

    print_white_space(whitespace + 4);
    printf("sig:        %p\n", info.optional_type_signature);

    if (info.type != NULL)
    {
        print_white_space(whitespace + 4);
        printf("type:       ");
        necro_print_type_sig(info.type, intern);
        // printf("\n");
    }

    print_white_space(whitespace + 4);
    if (info.method_type_class != NULL)
        printf("is_method:  true\n");
    else
        printf("is_method:  false\n");

    print_white_space(whitespace);
    printf("}\n");
}

void necro_symtable_print(NecroSymTable* table)
{
    printf("NecroSymTable\n{\n");
    printf("    size:  %d\n", table->size);
    printf("    count: %d\n", table->count);
    printf("    data:\n");
    printf("    [\n");
    for (size_t i = 0; i < table->count; ++i)
    {
        necro_symtable_info_print(table->data[i], table->intern, 8);
    }
    printf("    ]\n");
    printf("}\n");
}

void necro_symtable_test()
{
    puts("---------------------------");
    puts("-- NecroSymTable");
    puts("---------------------------\n");

    NecroIntern     intern       = necro_create_intern();
    NecroSymTable   symtable     = necro_create_symtable(&intern);

    // Symbol 1 test
    NecroSymbol     test_symbol1 = necro_intern_string(&intern, "test1");
    NecroSymbolInfo info1        = { .type = { 0},.name = test_symbol1,.id = { 0 }, };
    NecroID         id1          = necro_symtable_insert(&symtable, info1);

    // Symbol 2 test
    NecroSymbol     test_symbol2 = necro_intern_string(&intern, "fuck off!");
    NecroSymbolInfo info2        = { .type = { 0},.name = test_symbol2,.id = { 0 }, };
    NecroID         id2          = necro_symtable_insert(&symtable, info2);

    // necro_symtable_print(&symtable);

    NecroSymbolInfo* info1_test = necro_symtable_get(&symtable, id1);
    if (info1_test != NULL && info1_test->name.id == info1.name.id)
    {
        printf("Symbol1 test: passed\n");
    }
    else
    {
        printf("Symbol1 test: failed\n");
    }

    NecroSymbolInfo* info2_test = necro_symtable_get(&symtable, id2);
    if (info2_test != NULL && info2_test->name.id == info2.name.id)
    {
        printf("Symbol2 test: passed\n");
    }
    else
    {
        printf("Symbol2 test: failed\n");
    }

    necro_destroy_symtable(&symtable);
    necro_destroy_intern(&intern);

    necro_scoped_symtable_test();
}

//=====================================================
// NecroScopedSymTable
//=====================================================
#define NECRO_SCOPE_INITIAL_SIZE 8

inline NecroScope* necro_create_scope(NecroPagedArena* arena, NecroScope* parent)
{
    NecroScope* scope    = necro_paged_arena_alloc(arena, sizeof(NecroScope));
    scope->parent        = parent;
    scope->buckets       = necro_paged_arena_alloc(arena, NECRO_SCOPE_INITIAL_SIZE * sizeof(NecroScopeNode));
    scope->size          = NECRO_SCOPE_INITIAL_SIZE;
    scope->count         = 0;
    for (size_t bucket = 0; bucket < scope->size; ++bucket)
        scope->buckets[bucket] = (NecroScopeNode) { .id = NECRO_SYMTABLE_NULL_ID };
    return scope;
}

NecroScopedSymTable necro_create_scoped_symtable(NecroSymTable* global_table)
{
    NecroPagedArena  arena       = necro_create_paged_arena();
    NecroScope*      top_scope   = necro_create_scope(&arena, NULL);
    NecroScope*      type_scope  = necro_create_scope(&arena, NULL);
    return (NecroScopedSymTable)
    {
        .arena               = arena,
        .global_table        = global_table,
        .top_scope           = top_scope,
        .current_scope       = top_scope,
        .top_type_scope      = type_scope,
        .current_type_scope  = type_scope,
    };
}

void necro_destroy_scoped_symtable(NecroScopedSymTable* table)
{
    necro_destroy_paged_arena(&table->arena);
}

void necro_scoped_symtable_new_scope(NecroScopedSymTable* table)
{
    table->current_scope = necro_create_scope(&table->arena, table->current_scope);
}

void necro_scoped_symtable_pop_scope(NecroScopedSymTable* table)
{
    table->current_scope = table->current_scope->parent;
}

void necro_scoped_symtable_new_type_scope(NecroScopedSymTable* table)
{
    table->current_type_scope = necro_create_scope(&table->arena, table->current_type_scope);
}

void necro_scoped_symtable_pop_type_scope(NecroScopedSymTable* table)
{
    table->current_type_scope = table->current_type_scope->parent;
}

void necro_scope_insert(NecroScope* scope, NecroSymbol symbol, NecroID id, NecroPagedArena* arena);

inline void necro_scope_grow(NecroScope* scope, NecroPagedArena* arena)
{
    assert(scope != NULL);
    assert(scope->buckets != NULL);
    assert(scope->count < scope->size);
    // printf("GROW\n");
    NecroScopeNode* prev_buckets = scope->buckets;
    size_t          prev_size    = scope->size;
    size_t          prev_count   = scope->count;
    scope->size                 *= 2;
    scope->buckets               = necro_paged_arena_alloc(arena, scope->size * sizeof(NecroScopeNode));
    scope->count                 = 0;
    // memset(scope->buckets, 0, scope->size * sizeof(NecroScopeNode));
    for (size_t bucket = 0; bucket < scope->size; ++bucket)
        scope->buckets[bucket] = (NecroScopeNode) { .id = NECRO_SYMTABLE_NULL_ID };
    for (size_t bucket = 0; bucket < prev_size; ++bucket)
    {
        NecroID     id     = prev_buckets[bucket].id;
        NecroSymbol symbol = prev_buckets[bucket].symbol;
        if (id.id == NECRO_SYMTABLE_NULL_ID.id)
            continue;
        necro_scope_insert(scope, symbol, id, arena);
    }
    assert(scope->count == prev_count);
    // Leak prev_buckets since we're using an arena (which will free it later) and we care more about speed than memory conservation during this point
}

void necro_scope_insert(NecroScope* scope, NecroSymbol symbol, NecroID id, NecroPagedArena* arena)
{
    assert(scope != NULL);
    assert(scope->buckets != NULL);
    assert(scope->count < scope->size);
    assert(id.id != 0);
    if (scope->count >= scope->size / 2)
        necro_scope_grow(scope, arena);
    // printf("necro_scope_insert id: %d\n", id.id);
    size_t bucket = symbol.hash & (scope->size - 1);
    while (scope->buckets[bucket].id.id != NECRO_SYMTABLE_NULL_ID.id)
    {
        if (scope->buckets[bucket].symbol.id == symbol.id)
        {
            // printf("CONTAINS: %d\n", scope->buckets[bucket].symbol.id);
            return;
            // break;
        }
        bucket = (bucket + 1) & (scope->size - 1);
    }
    scope->buckets[bucket].symbol = symbol;
    scope->buckets[bucket].id     = id;
    scope->count++;
}

NecroID necro_this_scope_find(NecroScope* scope, NecroSymbol symbol)
{
    assert(scope != NULL);
    assert(scope->buckets != NULL);
    assert(scope->count < scope->size);

    for (size_t bucket = symbol.hash & (scope->size - 1); scope->buckets[bucket].id.id != NECRO_SYMTABLE_NULL_ID.id; bucket = (bucket + 1) & (scope->size - 1))
    {
        if (scope->buckets[bucket].symbol.id == symbol.id)
            return scope->buckets[bucket].id;
    }

    return NECRO_SYMTABLE_NULL_ID;
}

NecroID necro_scope_find(NecroScope* scope, NecroSymbol symbol)
{
    NecroID     id            = NECRO_SYMTABLE_NULL_ID;
    NecroScope* current_scope = scope;
    while (current_scope != NULL)
    {
        id = necro_this_scope_find(current_scope, symbol);
        if (id.id != NECRO_SYMTABLE_NULL_ID.id)
        {
            return id;
        }
        current_scope = current_scope->parent;
    }
    return id;
}

NecroID necro_scoped_symtable_new_symbol_info(NecroScopedSymTable* table, NecroScope* scope, NecroSymbolInfo info)
{
    NecroID id = necro_symtable_insert(table->global_table, info);
    necro_scope_insert(scope, info.name, id, &table->arena);
    return id;
}

NecroID necro_symtable_manual_new_symbol(NecroSymTable* symtable, NecroSymbol symbol)
{
    NecroSymbolInfo info = necro_create_initial_symbol_info(symbol, (NecroSourceLoc) { 0 }, NULL, symtable->intern);
    return necro_symtable_insert(symtable, info);
}

void necro_scope_set_last_introduced_id(NecroScope* scope, NecroID last_introduced_id)
{
    scope->last_introduced_id = last_introduced_id;
}

//=====================================================
// Build Scopes
//=====================================================
void necro_build_scopes_go(NecroScopedSymTable* scoped_symtable, NecroAST_Node_Reified* input_node)
{
    if (input_node == NULL || scoped_symtable->error.return_code == NECRO_ERROR)
        return;
    input_node->scope       = scoped_symtable->current_scope;
    switch (input_node->type)
    {
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
    case NECRO_AST_TOP_DECL:
        necro_build_scopes_go(scoped_symtable, input_node->top_declaration.declaration);
        necro_build_scopes_go(scoped_symtable, input_node->top_declaration.next_top_decl);
        break;
    case NECRO_AST_DECL:
        necro_build_scopes_go(scoped_symtable, input_node->declaration.declaration_impl);
        necro_build_scopes_go(scoped_symtable, input_node->declaration.next_declaration);
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
        necro_build_scopes_go(scoped_symtable, input_node->right_hand_side.declarations);
        necro_build_scopes_go(scoped_symtable, input_node->right_hand_side.expression);
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
        case NECRO_VAR_DECLARATION:          break;
        case NECRO_VAR_TYPE_FREE_VAR:        input_node->scope = scoped_symtable->current_type_scope; break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: input_node->scope = scoped_symtable->current_type_scope; break;
        case NECRO_VAR_SIG:                  break;
        case NECRO_VAR_CLASS_SIG:            break;
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
        necro_build_scopes_go(scoped_symtable, input_node->lambda.expression);
        necro_scoped_symtable_pop_scope(scoped_symtable);
        break;
    case NECRO_AST_DO:
    {
        // necro_build_scopes_go(scoped_symtable, input_node->do_statement.statement_list);
        NecroAST_Node_Reified* current_statement = input_node->do_statement.statement_list;
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
    case NECRO_AST_PAT_EXPRESSION:
        necro_build_scopes_go(scoped_symtable, input_node->pattern_expression.expressions);
        break;
    // case NECRO_AST_EXPRESSION_SEQUENCE:
    // {
    //     // Push
    //     NecroAST_Node_Reified* expressions = input_node->expression_sequence.expressions;
    //     assert(expressions != NULL); // Shouldn't be possible to have an empty sequence
    //     expressions->delay_scope = scoped_symtable->current_delay_scope;
    //     necro_build_scopes_go(scoped_symtable, expressions->list.item);
    //     expressions = expressions->list.next_item;
    //     while (expressions != NULL)
    //     {
    //         necro_scoped_symtable_new_delay_scope(scoped_symtable);
    //         expressions->delay_scope = scoped_symtable->current_delay_scope;
    //         necro_build_scopes_go(scoped_symtable, expressions->list.item);
    //         expressions = expressions->list.next_item;
    //     }
    //     // Pop
    //     expressions = input_node->expression_sequence.expressions;
    //     expressions = expressions->list.next_item;
    //     while (expressions != NULL)
    //     {
    //         necro_scoped_symtable_pop_delay_scope(scoped_symtable);
    //         expressions = expressions->list.next_item;
    //     }
    //     printf("seq\n");
    //     break;
    // }
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
        input_node->scope = scoped_symtable->current_type_scope;
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_scoped_symtable_new_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->data_declaration.simpletype);
        necro_build_scopes_go(scoped_symtable, input_node->data_declaration.constructor_list);
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
        necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.declarations);
        // TODO: Get AST arena into here and use that instead of the scope arena!
        // necro_create_dictionary_data_declaration(&scoped_symtable->arena, scoped_symtable->global_table->intern, input_node);
        // necro_build_scopes_go(scoped_symtable, input_node->type_class_declaration.dictionary_data_declaration);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.context);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.qtycls);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.inst);
        necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.declarations);
        // TODO: Get AST arena into here and use that instead of the scope arena!
        // necro_create_dictionary_instance(&scoped_symtable->arena, scoped_symtable->global_table->intern, input_node);
        // necro_build_scopes_go(scoped_symtable, input_node->type_class_instance.dictionary_instance);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->type_signature.var);
        necro_scoped_symtable_new_type_scope(scoped_symtable);
        necro_build_scopes_go(scoped_symtable, input_node->type_signature.context);
        necro_build_scopes_go(scoped_symtable, input_node->type_signature.type);
        necro_scoped_symtable_pop_type_scope(scoped_symtable);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        input_node->scope = scoped_symtable->current_type_scope;
        necro_build_scopes_go(scoped_symtable, input_node->function_type.type);
        necro_build_scopes_go(scoped_symtable, input_node->function_type.next_on_arrow);
        break;
    default:
        necro_error(&scoped_symtable->error, input_node->source_loc, "Unrecognized AST Node type found: %d", input_node->type);
        break;
    }
}

NECRO_RETURN_CODE necro_build_scopes(NecroScopedSymTable* table, NecroAST_Reified* ast)
{
    table->error.return_code = NECRO_SUCCESS;
    necro_build_scopes_go(table, ast->root);
    return table->error.return_code;
}

void necro_scope_print(NecroScope* scope, size_t whitespace, NecroIntern* intern, NecroSymTable* global_table)
{
    assert(scope != NULL);
    assert(intern != NULL);

    print_white_space(whitespace);
    printf("NecroScope\n");

    print_white_space(whitespace);
    printf("{\n");

    print_white_space(whitespace + 4);
    printf("size:  %d\n", scope->size);

    print_white_space(whitespace + 4);
    printf("count: %d\n", scope->count);

    print_white_space(whitespace + 4);
    printf("data:\n");

    print_white_space(whitespace + 4);
    printf("[\n");

    for (size_t bucket = 0; bucket < scope->size; ++bucket)
    {
        if (scope->buckets[bucket].id.id != NECRO_SYMTABLE_NULL_ID.id)
        {
            // print_white_space(whitespace + 8);
            NecroSymbolInfo info = *necro_symtable_get(global_table, scope->buckets[bucket].id);
            necro_symtable_info_print(info, intern, whitespace + 8);
            // printf("{ name: %s, id: %d }\n", necro_intern_get_string(intern, table->buckets[bucket].symbol), table->buckets[bucket].id.id);
        }
    }

    print_white_space(whitespace + 4);
    printf("]\n");

    print_white_space(whitespace);
    printf("}\n");
}

void necro_scoped_symtable_print_type_scope(NecroScopedSymTable* table)
{
    necro_scope_print(table->top_type_scope, 8, table->global_table->intern, table->global_table);
}

void necro_scoped_symtable_print(NecroScopedSymTable* table)
{
    assert(table != NULL);
    printf("NecroScopedSymtable\n{\n");
    printf("    scopes (from this leaf only):\n");
    NecroScope* current_scope = table->current_scope;
    while (current_scope != NULL)
    {
        necro_scope_print(current_scope, 8, table->global_table->intern, table->global_table);
        current_scope = current_scope->parent;
    }
    printf("}\n");
}

void necro_print_env_with_symtable(NecroSymTable* table, NecroInfer* infer)
{
    printf("\nEnv:\n[\n");
    for (size_t i = 1; i < table->count; ++i)
    {
        printf("    %s", necro_intern_get_string(infer->intern, table->data[i].name));
        if (infer->symtable->data[i].type != NULL)
        {
            printf(" => ");
            // if (infer->symtable->data[i].type->type == NECRO_TYPE_CON)
            // {
            //     printf(" (kind: %d) ", infer->symtable->data[i].type->con.arity);
            // }
            necro_print_type_sig_go(infer->symtable->data[i].type, infer->intern);
            printf(" :: ");
            // necro_print_kind(table->data[i].type->kind);
            necro_print_type_sig_go(infer->symtable->data[i].type->type_kind, infer->intern);
            printf("\n");
        }
        else
        {
            printf("\n");
        }
    }
    printf("]\n\n");
    // for now turning this off, turn back on for extra debugging
    // printf("TyVars, all\n[\n");
    // for (size_t i = 1; i <= infer->highest_id; ++i)
    // {
    //     if (i <= table->count) continue;
    //     printf("    %s", necro_id_as_character_string(infer, (NecroVar) { .id = (NecroID) { i }, .symbol = (NecroSymbol) {.id = 0, .hash = 0} }));
    //     if (infer->env.data[i]->var.bound != NULL)
    //     {

    //         printf(" => ");
    //         necro_print_type_sig_go(infer->env.data[i]->var.bound, infer->intern);
    //         printf(" :: ");
    //         necro_print_kind(infer->env.data[i]->kind);
    //         printf("\n");
    //     }
    //     // else if (infer->env.data[i]->var.arity != -1 && infer->env.data[i]->var.arity != 0)
    //     // {
    //     //     printf(" (kind: %d)\n", infer->env.data[i]->var.arity);
    //     // }
    //     else
    //     {
    //         printf(" :: ");
    //         necro_print_kind(infer->env.data[i]->kind);
    //         printf("\n");
    //     }
    // }
    // printf("]\n");
    printf("Total mem usage: %f mb\n", ((float) (sizeof(NecroSymbolInfo) * table->count + sizeof(NecroType) * infer->env.capacity) * 8) / 1000000.0);
}

//=====================================================
// Testing
//=====================================================
void necro_scoped_symtable_test()
{
    necro_announce_phase("NecroScopedSymTable");

    NecroIntern         intern          = necro_create_intern();
    NecroSymTable       symtable        = necro_create_symtable(&intern);
    NecroScopedSymTable scoped_symtable = necro_create_scoped_symtable(&symtable);

    NecroScope*         top_scope       = scoped_symtable.current_scope;
    // necro_scoped_symtable_print(&scoped_symtable);
    // printf("\n");

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

    // New / Find Test
    {
        NecroSymbol     test_sym = necro_intern_string(&intern, "pulseDemon");
        NecroSymbolInfo info     = { .name = test_sym, };
        NecroID         id       = necro_scoped_symtable_new_symbol_info(&scoped_symtable, scoped_symtable.current_scope, info);
        NecroID         found_id = necro_scope_find(scoped_symtable.current_scope, test_sym);
        if (id.id == found_id.id)
            printf("New/Find test:  passed\n");
        else
            printf("New/Find test:  FAILED\n");
    }

    // Push / New / Find Test
    {
        necro_scoped_symtable_new_scope(&scoped_symtable);
        NecroSymbol     test_sym  = necro_intern_string(&intern, "dragonEngine");
        NecroSymbolInfo info      = { .name = test_sym, };
        NecroID         id        = necro_scoped_symtable_new_symbol_info(&scoped_symtable, scoped_symtable.current_scope, info);

        NecroSymbol     test_sym3 = necro_intern_string(&intern, "pulseDemon");
        NecroSymbolInfo info3     = { .name = test_sym3, };
        NecroID         id3       = necro_scoped_symtable_new_symbol_info(&scoped_symtable, scoped_symtable.current_scope, info3);

        NecroSymbol     test_sym2 = necro_intern_string(&intern, "AcidicSlime");
        NecroSymbolInfo info2     = { .name = test_sym2, };
        NecroID         id2       = necro_scoped_symtable_new_symbol_info(&scoped_symtable, scoped_symtable.current_scope, info2);

        NecroID         found_id  = necro_scope_find(scoped_symtable.current_scope, test_sym);
        NecroID         found_id2 = necro_scope_find(scoped_symtable.current_scope, test_sym2);
        NecroID         found_id3 = necro_scope_find(scoped_symtable.current_scope, test_sym3);

        // necro_scoped_symtable_print(&scoped_symtable);

        if (id.id == found_id.id && id.id != NECRO_SYMTABLE_NULL_ID.id && found_id.id != NECRO_SYMTABLE_NULL_ID.id)
            printf("Push/New test:  passed\n");
        else
            printf("Push/New test:  FAILED\n");
        if (id2.id == found_id2.id && found_id.id != found_id2.id && id.id != id2.id && id2.id != NECRO_SYMTABLE_NULL_ID.id && found_id2.id != NECRO_SYMTABLE_NULL_ID.id)
            printf("Push/New2 test: passed\n");
        else
            printf("Push/New2 test: FAILED\n");
        if (id3.id == found_id3.id && found_id.id != found_id3.id && id.id != id3.id && id3.id != NECRO_SYMTABLE_NULL_ID.id && found_id3.id != NECRO_SYMTABLE_NULL_ID.id)
            printf("Push/New3 test: passed\n");
        else
            printf("Push/New3 test: FAILED\n");

        necro_scoped_symtable_pop_scope(&scoped_symtable);
        NecroID         found_id4 = necro_scope_find(scoped_symtable.current_scope, test_sym);
        NecroID         found_id5 = necro_scope_find(scoped_symtable.current_scope, test_sym2);
        if (found_id4.id == NECRO_SYMTABLE_NULL_ID.id && found_id5.id == NECRO_SYMTABLE_NULL_ID.id)
            printf("Pop/Find test:  passed\n");
        else
            printf("Pop/Find test:  FAILED\n");
    }

    necro_destroy_scoped_symtable(&scoped_symtable);

    scoped_symtable = necro_create_scoped_symtable(&symtable);
    // Grow Test
    {
        bool test_passed = true;
        for (size_t i = 0; i < 64; ++i)
        {
            char buffer[20];
            snprintf(buffer, 20, "grow%d", i);
            NecroSymbol     symbol = necro_intern_string(&intern, buffer);
            NecroSymbolInfo info   = necro_create_initial_symbol_info(symbol, (NecroSourceLoc){ 0, 0, 0 }, NULL, &intern);
            NecroID         id     = necro_scoped_symtable_new_symbol_info(&scoped_symtable, scoped_symtable.current_scope, info);
            if (id.id == 0)
            {
                test_passed = false;
                break;
            }
        }
        if (test_passed)
            printf("Grow test:      passed\n");
        else
            printf("Grow test:      FAILED\n");
    }

    necro_destroy_symtable(&symtable);
    necro_destroy_intern(&intern);
}

NecroSymbolInfo* necro_symtable_get_type_class_declaration_info(NecroSymTable* symtable, NecroAST_Node_Reified* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    uint32_t index = ast->type_class_declaration.tycls->conid.id.id;
    assert(index > 0);
    assert(index <= symtable->count);
    return symtable->data + index;
}

NecroSymbolInfo* necro_symtable_get_type_class_instance_info(NecroSymTable* symtable, NecroAST_Node_Reified* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    uint32_t index = ast->type_class_instance.instance_id.id;
    assert(index > 0);
    assert(index <= symtable->count);
    return symtable->data + index;
}

NecroVar necro_get_top_level_symbol_var(NecroScopedSymTable* scoped_symtable, const char* name)
{
    NecroSymbol symbol = necro_intern_string(scoped_symtable->global_table->intern, name);
    NecroID     id     = necro_scope_find(scoped_symtable->top_scope, symbol);
    assert(id.id != 0);
    return (NecroVar) { .id = id, .symbol = symbol };
}

NecroVar necro_get_type_symbol_var(NecroScopedSymTable* scoped_symtable, const char* name)
{
    NecroSymbol symbol = necro_intern_string(scoped_symtable->global_table->intern, name);
    NecroID     id     = necro_scope_find(scoped_symtable->top_type_scope, symbol);
    assert(id.id != 0);
    return (NecroVar) { .id = id, .symbol = symbol };
}

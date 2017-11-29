/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "symtable.h"

// Constants
#define NECRO_SYMTABLE_INITIAL_SIZE 512
#define NECRO_SYMTABLE_NULL_ID      ((NecroID) {0})

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

NecroSymbolInfo necro_create_initial_symbol_info(NecroSymbol symbol)
{
    return (NecroSymbolInfo)
    {
        .name          = symbol,
        .id            = 0,
        .data_size     = 0,
        .type          = {0},
        .local_var_num = 0,
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
    printf("name:      %s\n",necro_intern_get_string(intern, info.name));

    print_white_space(whitespace + 4);
    printf("id:        %d\n", info.id.id);

    print_white_space(whitespace + 4);
    printf("size:      %d\n", info.data_size);

    print_white_space(whitespace + 4);
    printf("local var: %d\n", info.local_var_num);

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
    NecroSymbolInfo info1        = { .type = { 0},.name = test_symbol1,.id = { 0 },.data_size = 4 };
    NecroID         id1          = necro_symtable_insert(&symtable, info1);

    // Symbol 2 test
    NecroSymbol     test_symbol2 = necro_intern_string(&intern, "fuck off!");
    NecroSymbolInfo info2        = { .type = { 0},.name = test_symbol2,.id = { 0 },.data_size = 8 };
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
#define NECRO_SCOPED_HASHTABLE_INITIAL_SIZE 8

inline NecroScopedHashtable* necro_create_scoped_hashtable(NecroPagedArena* arena, NecroScopedHashtable* parent)
{
    NecroScopedHashtable* table = necro_paged_arena_alloc(arena, sizeof(NecroScopedHashtable));
    table->parent               = parent;
    table->buckets              = necro_paged_arena_alloc(arena, NECRO_SCOPED_HASHTABLE_INITIAL_SIZE * sizeof(NecroScopedHashtableNode));
    table->size                 = NECRO_SCOPED_HASHTABLE_INITIAL_SIZE;
    table->count                = 0;
    memset(table->buckets, 0, NECRO_SCOPED_HASHTABLE_INITIAL_SIZE * sizeof(NecroScopedHashtableNode));
    return table;
}

NecroScopedSymTable necro_create_scoped_symtable(NecroSymTable* global_table)
{
    NecroPagedArena       arena         = necro_create_paged_arena();
    NecroScopedHashtable* current_scope = necro_create_scoped_hashtable(&arena, NULL);
    return (NecroScopedSymTable)
    {
        .arena         = arena,
        .global_table  = global_table,
        .current_scope = current_scope,
    };
}

void necro_destroy_scoped_symtable(NecroScopedSymTable* table)
{
    necro_destroy_paged_arena(&table->arena);
}

void necro_scoped_symtable_new_scope(NecroScopedSymTable* table)
{
    table->current_scope = necro_create_scoped_hashtable(&table->arena, table->current_scope);
}

void necro_scoped_symtable_pop_scope(NecroScopedSymTable* table)
{
    table->current_scope = table->current_scope->parent;
}

inline void necro_scoped_hashtable_grow(NecroScopedHashtable* table, NecroPagedArena* arena)
{
    assert(table != NULL);
    assert(table->buckets != NULL);
    assert(table->count < table->size);
    NecroScopedHashtableNode* prev_buckets = table->buckets;
    size_t                    prev_size    = table->size;
    size_t                    prev_count   = table->count;
    table->size                           *= 2;
    table->buckets                         = necro_paged_arena_alloc(arena, table->size * sizeof(NecroScopedHashtableNode));
    table->count                           = 0;
    memset(table->buckets, 0, NECRO_SCOPED_HASHTABLE_INITIAL_SIZE * sizeof(NecroScopedHashtableNode));
    for (size_t bucket = 0; bucket < prev_size; ++bucket)
    {
        NecroID     id     = prev_buckets[bucket].id;
        NecroSymbol symbol = prev_buckets[bucket].symbol;
        for (size_t bucket = symbol.hash & (table->size - 1); table->buckets[bucket].id.id == NECRO_SYMTABLE_NULL_ID.id; ++bucket)
        {
            table->buckets[bucket].symbol = symbol;
            table->buckets[bucket].id     = id;
            table->count++;
            break;
        }
    }
    assert(table->count == prev_count);
    // Leak prev_buckets since we're using an arena (which will free it later) and we care more about speed than memory conservation during this point
}

inline void necro_scoped_hashtable_insert(NecroScopedHashtable* table, NecroSymbol symbol, NecroID id, NecroPagedArena* arena, NecroSymTable* global_table)
{
    assert(table != NULL);
    assert(table->buckets != NULL);
    assert(table->count < table->size);
    if (table->count >= table->size / 2)
        necro_scoped_hashtable_grow(table, arena);
    size_t bucket = symbol.hash & (table->size - 1);
    while (table->buckets[bucket].id.id != NECRO_SYMTABLE_NULL_ID.id)
    {
        bucket = (bucket + 1) & (table->size - 1);
    }
    table->buckets[bucket].symbol = symbol;
    table->buckets[bucket].id     = id;
    // necro_symtable_get(global_table, id)->local_var_num = table->count; // Save this for later semantic analysis
    table->count++;
}

inline NecroID necro_scoped_hashtable_find(NecroScopedHashtable* table, NecroSymbol symbol)
{
    assert(table != NULL);
    assert(table->buckets != NULL);
    assert(table->count < table->size);

    for (size_t bucket = symbol.hash & (table->size - 1); table->buckets[bucket].id.id != NECRO_SYMTABLE_NULL_ID.id; bucket = (bucket + 1) & (table->size - 1))
    {
        if (table->buckets[bucket].symbol.id == symbol.id)
            return table->buckets[bucket].id;
    }

    return NECRO_SYMTABLE_NULL_ID;
}

NecroID necro_scoped_symtable_find(NecroScopedSymTable* table, NecroSymbol symbol)
{
    NecroID               id            = NECRO_SYMTABLE_NULL_ID;
    NecroScopedHashtable* current_scope = table->current_scope;
    while (current_scope != NULL)
    {
        id = necro_scoped_hashtable_find(current_scope, symbol);
        if (id.id != NECRO_SYMTABLE_NULL_ID.id)
        {
            return id;
        }
        current_scope = current_scope->parent;
    }
    return id;
}

NecroID necro_scoped_symtable_new_symbol_info(NecroScopedSymTable* table, NecroSymbolInfo info)
{
    NecroID id = necro_symtable_insert(table->global_table, info);
    necro_scoped_hashtable_insert(table->current_scope, info.name, id, &table->arena, table->global_table);
    return id;
}

void necro_scoped_hashtable_print(NecroScopedHashtable* table, size_t whitespace, NecroIntern* intern, NecroSymTable* global_table)
{
    assert(table != NULL);
    assert(intern != NULL);

    print_white_space(whitespace);
    printf("NecroScopedHashtable\n");

    print_white_space(whitespace);
    printf("{\n");

    print_white_space(whitespace + 4);
    printf("size:  %d\n", table->size);

    print_white_space(whitespace + 4);
    printf("count: %d\n", table->count);

    print_white_space(whitespace + 4);
    printf("data:\n");

    print_white_space(whitespace + 4);
    printf("[\n");

    for (size_t bucket = 0; bucket < table->size; ++bucket)
    {
        if (table->buckets[bucket].id.id != NECRO_SYMTABLE_NULL_ID.id)
        {
            // print_white_space(whitespace + 8);
            NecroSymbolInfo info = *necro_symtable_get(global_table, table->buckets[bucket].id);
            necro_symtable_info_print(info, intern, whitespace + 8);
            // printf("{ name: %s, id: %d }\n", necro_intern_get_string(intern, table->buckets[bucket].symbol), table->buckets[bucket].id.id);
        }
    }

    print_white_space(whitespace + 4);
    printf("]\n");

    print_white_space(whitespace);
    printf("}\n");
}

void necro_scoped_symtable_print(NecroScopedSymTable* table)
{
    assert(table != NULL);
    printf("NecroScopedSymtable\n{\n");
    printf("    scopes (from this leaf only):\n");
    NecroScopedHashtable* current_scope = table->current_scope;
    while (current_scope != NULL)
    {
        necro_scoped_hashtable_print(current_scope, 8, table->global_table->intern, table->global_table);
        current_scope = current_scope->parent;
    }
    printf("}\n");
}

void necro_scoped_symtable_test()
{
    printf("\n");
    puts("---------------------------");
    puts("-- NecroScopedSymTable");
    puts("---------------------------\n");

    NecroIntern         intern          = necro_create_intern();
    NecroSymTable       symtable        = necro_create_symtable(&intern);
    NecroScopedSymTable scoped_symtable = necro_create_scoped_symtable(&symtable);

    NecroScopedHashtable* top_scope     = scoped_symtable.current_scope;
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
        NecroSymbolInfo info     = { .name = test_sym,.data_size = 4 };
        NecroID         id       = necro_scoped_symtable_new_symbol_info(&scoped_symtable, info);
        NecroID         found_id = necro_scoped_symtable_find(&scoped_symtable, test_sym);
        if (id.id == found_id.id)
            printf("New/Find test:  passed\n");
        else
            printf("New/Find test:  FAILED\n");
    }

    // Push / New / Find Test
    {
        necro_scoped_symtable_new_scope(&scoped_symtable);
        NecroSymbol     test_sym  = necro_intern_string(&intern, "dragonEngine");
        NecroSymbolInfo info      = { .name = test_sym,.data_size = 8 };
        NecroID         id        = necro_scoped_symtable_new_symbol_info(&scoped_symtable, info);

        NecroSymbol     test_sym3 = necro_intern_string(&intern, "pulseDemon");
        NecroSymbolInfo info3     = { .name = test_sym3,.data_size = 32 };
        NecroID         id3       = necro_scoped_symtable_new_symbol_info(&scoped_symtable, info3);

        NecroSymbol     test_sym2 = necro_intern_string(&intern, "AcidicSlime");
        NecroSymbolInfo info2     = { .name = test_sym2,.data_size = 16 };
        NecroID         id2       = necro_scoped_symtable_new_symbol_info(&scoped_symtable, info2);

        NecroID         found_id  = necro_scoped_symtable_find(&scoped_symtable, test_sym);
        NecroID         found_id2 = necro_scoped_symtable_find(&scoped_symtable, test_sym2);
        NecroID         found_id3 = necro_scoped_symtable_find(&scoped_symtable, test_sym3);

        necro_scoped_symtable_print(&scoped_symtable);

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
        NecroID         found_id4 = necro_scoped_symtable_find(&scoped_symtable, test_sym);
        NecroID         found_id5 = necro_scoped_symtable_find(&scoped_symtable, test_sym2);
        if (found_id4.id == NECRO_SYMTABLE_NULL_ID.id && found_id5.id == NECRO_SYMTABLE_NULL_ID.id)
            printf("Pop/Find test:  passed\n");
        else
            printf("Pop/Find test:  FAILED\n");
    }

    // printf("\n");
    // necro_scoped_symtable_print(&scoped_symtable);
    // necro_symtable_print(&symtable);

    necro_destroy_scoped_symtable(&scoped_symtable);
    necro_destroy_symtable(&symtable);
    necro_destroy_intern(&intern);
}

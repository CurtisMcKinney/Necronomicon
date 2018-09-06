/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "utility/utility.h"
#include "hash_table.h"

#define NECRO_CHAIN_TABLE_INITIAL_SIZE 512

size_t hash(uint64_t input)
{
    return (size_t)(input * 37);
}

NecroArenaChainTable necro_empty_arena_chain_table()
{
    return (NecroArenaChainTable)
    {
        .arena     = necro_empty_paged_arena(),
        .buckets   = NULL,
        .size      = 0,
        .count     = 0,
        .data_size = 0
    };
}

NecroArenaChainTable necro_create_arena_chain_table(size_t data_size)
{
    NecroChainTableNode* buckets = calloc(NECRO_CHAIN_TABLE_INITIAL_SIZE, sizeof(NecroChainTableNode));
    if (buckets == NULL)
    {
        fprintf(stderr, "Malloc returned NULL in necro_create_chain_table!\n");
        exit(1);
    }
    for (size_t i = 0; i < NECRO_CHAIN_TABLE_INITIAL_SIZE; ++i)
        buckets[i] = (NecroChainTableNode) { .next = NULL, .key = 0 };
    return (NecroArenaChainTable)
    {
        .arena     = necro_create_paged_arena(),
        .buckets   = buckets,
        .size      = NECRO_CHAIN_TABLE_INITIAL_SIZE,
        .count     = 0,
        .data_size = data_size
    };
}

void necro_destroy_arena_chain_table(NecroArenaChainTable* table)
{
    if (table->buckets != NULL)
    {
        free(table->buckets);
        table->buckets = NULL;
    }
    necro_destroy_paged_arena(&table->arena);
    table->size      = 0;
    table->data_size = 0;
    table->count     = 0;
}

void necro_arena_chain_table_grow(NecroArenaChainTable* table)
{
    assert(table != NULL);
    NecroChainTableNode* prev_buckets = table->buckets;
    size_t               prev_size    = table->size;
    size_t               prev_count   = table->count;
    table->buckets                    = calloc(table->size * 2, sizeof(NecroChainTableNode));
    table->size                       = table->size * 2;
    table->count                      = 0;
    if (table->buckets == NULL)
    {
        if (prev_buckets != NULL)
            free(prev_buckets);
        fprintf(stderr, "Malloc returned NULL in necro_arena_chain_table_grow!\n");
        exit(1);
    }
    for (size_t i = 0; i < table->size; ++i)
        table->buckets[i] = (NecroChainTableNode) { .next = NULL, .key = 0 };
    for (size_t i = 0; i < prev_size; ++i)
    {
        NecroChainTableNode* curr = prev_buckets[i].next;
        while (curr != NULL)
        {
            size_t bucket = hash(curr->key) & (table->size - 1);
            NecroChainTableNode* new_prev = table->buckets + bucket;
            NecroChainTableNode* new_curr = new_prev->next;
            while (new_curr != NULL)
            {
                assert(new_curr->key != curr->key);
                new_prev = new_curr;
                new_curr = new_curr->next;
            }
            new_prev->next       = curr;
            curr                 = curr->next;
            new_prev->next->next = NULL;
            table->count++;
        }
    }
    assert(table->count == prev_count);
    free(prev_buckets);
}

void* necro_arena_chain_table_insert(NecroArenaChainTable* table, uint64_t key, void* data_to_be_copied_in)
{
    assert(table != NULL);
    if (table->count > table->size / 2)
        necro_arena_chain_table_grow(table);
    size_t               bucket = hash(key) & (table->size - 1);
    NecroChainTableNode* prev   = table->buckets + bucket;
    NecroChainTableNode* curr   = prev->next;
    while (curr != NULL)
    {
        if (curr->key == key)
        {
            if (data_to_be_copied_in != NULL)
                memcpy(curr + 1, data_to_be_copied_in, table->data_size);
            return (void*)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }
    NecroChainTableNode* node = necro_paged_arena_alloc(&table->arena, table->data_size + sizeof(NecroChainTableNode));
    if (data_to_be_copied_in != NULL)
        memcpy(node + 1, data_to_be_copied_in, table->data_size);
    node->key  = key;
    node->next = NULL;
    prev->next = node;
    table->count++;
    return ((void*)(node + 1));
}

void necro_arena_chain_table_iterate(NecroArenaChainTable* table, void (f)(void*, void*), void* extras)
{
    for (size_t i = 0; i < table->size; ++i)
    {
        NecroChainTableNode* curr = table->buckets[i].next;
        while (curr != NULL)
        {
            f(((void*)(curr + 1)), extras);
            curr = curr->next;
        }
    }
}

void* necro_arena_chain_table_get(NecroArenaChainTable* table, uint64_t key)
{
    assert(table != NULL);
    size_t               bucket = hash(key) & (table->size - 1);
    NecroChainTableNode* curr   = table->buckets[bucket].next;
    while (curr != NULL)
    {
        if (curr->key == key)
            return ((void*)(curr + 1));
        curr = curr->next;
    }
    return NULL;
}

NECRO_DECLARE_ARENA_CHAIN_TABLE(char, Char, char)
NECRO_DECLARE_ARENA_CHAIN_TABLE(size_t, Int, int)

void necro_arena_chain_table_test()
{
    necro_announce_phase("NecroArenaChainTable");

    // Insert / get test
    NecroCharTable char_table = necro_create_char_table();
    bool test_passed = true;
    for (char c = 97; c < 115; ++c)
    {
        char* in_c  = necro_char_table_insert(&char_table, c, &c);
        assert(in_c != NULL);
        char* get_c = necro_char_table_get(&char_table, c);
        if (get_c == NULL || *get_c != c)
        {
            assert(get_c != NULL);
            printf("Doesnt match, c: %d, in_c: %d, get_c: %d\n", c, *in_c, get_c == NULL ? 0 : *get_c);
            test_passed = false;
            break;
        }
    }
    if (test_passed)
        printf("Insert / get test: passed\n");
    else
        printf("Insert / get test: FAILED\n");
    necro_destroy_char_table(&char_table);

    // Grow Test
    NecroIntTable int_table = necro_create_int_table();
    test_passed = true;
    for (size_t i = 0; i < 4096; ++i)
    {
        size_t* in_i  = necro_int_table_insert(&int_table, i, NULL);
        *in_i         = i;
        size_t* get_i = necro_int_table_get(&int_table, i);
        if (in_i == NULL || *in_i != i || get_i == NULL || *get_i != i)
        {
            printf("Doesnt match, i: %zu, in_i: %zu, get_i: %zu\n", i, (in_i == NULL) ? 0 : *in_i, (get_i == NULL) ? 0 : *get_i);
            test_passed = false;
            break;
        }
    }
    if (test_passed)
        printf("Grow insert test:  passed\n");
    else
        printf("Grow insert test:  FAILED\n");
    necro_destroy_int_table(&int_table);
}
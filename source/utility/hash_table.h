/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef HASH_TABLE_H
#define HASH_TABLE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "arena.h"

typedef struct NecroChainTableNode
{
    struct NecroChainTableNode* next;
    uint64_t key;
} NecroChainTableNode;

typedef struct
{
    NecroPagedArena      arena;
    NecroChainTableNode* buckets;
    size_t               data_size;
    size_t               size;
    size_t               count;
} NecroArenaChainTable;

// Guarantees:
//     * O(1) inserstion and retrieval
//     * Destroying the table frees all memory associated with it
//     * All pointers returned by the table are stable
//     * All data inserted is copied in.
//     * Data is allocated from large chunks of memory for better cache coherency
//     * Never removes or deletes anything (use a different structure if you need removal)
//     * You can pass NULL into insert if you simply want to allocate space and then initialize that memory how you like
NecroArenaChainTable necro_empty_arena_chain_table();
NecroArenaChainTable necro_create_arena_chain_table(size_t data_size);
void                 necro_destroy_arena_chain_table(NecroArenaChainTable* table);
void*                necro_arena_chain_table_insert(NecroArenaChainTable* table, uint64_t key, void* data_to_be_copied_in);
void*                necro_arena_chain_table_get(NecroArenaChainTable* table, uint64_t key);
void                 necro_arena_chain_table_iterate(NecroArenaChainTable* table, void (f)(void*, void*), void* extras);
void                 necro_arena_chain_table_test();

#define NECRO_DECLARE_ARENA_CHAIN_TABLE(TABLE_DATA_TYPE, TABLE_CAMEL_NAME,TABLE_SNAKE_NAME)                                                                 \
typedef struct                                                                                                                                              \
{                                                                                                                                                           \
    NecroArenaChainTable chain_table;                                                                                                                       \
} Necro##TABLE_CAMEL_NAME##Table;                                                                                                                           \
static Necro##TABLE_CAMEL_NAME##Table necro_empty_##TABLE_SNAKE_NAME##_table()                                                                              \
{                                                                                                                                                           \
    return (Necro##TABLE_CAMEL_NAME##Table)                                                                                                                 \
    {                                                                                                                                                       \
        .chain_table = necro_empty_arena_chain_table()                                                                                                      \
    };                                                                                                                                                      \
}                                                                                                                                                           \
static Necro##TABLE_CAMEL_NAME##Table necro_create_##TABLE_SNAKE_NAME##_table()                                                                             \
{                                                                                                                                                           \
    return (Necro##TABLE_CAMEL_NAME##Table)                                                                                                                 \
    {                                                                                                                                                       \
        .chain_table = necro_create_arena_chain_table(sizeof(TABLE_DATA_TYPE))                                                                              \
    };                                                                                                                                                      \
}                                                                                                                                                           \
static void necro_destroy_##TABLE_SNAKE_NAME##_table(Necro##TABLE_CAMEL_NAME##Table* table)                                                                 \
{                                                                                                                                                           \
    necro_destroy_arena_chain_table(&table->chain_table);                                                                                                   \
}                                                                                                                                                           \
static TABLE_DATA_TYPE* necro_##TABLE_SNAKE_NAME##_table_insert(Necro##TABLE_CAMEL_NAME##Table* table, uint64_t key, TABLE_DATA_TYPE* data_to_be_copied)    \
{                                                                                                                                                           \
    return necro_arena_chain_table_insert(&table->chain_table, key, data_to_be_copied);                                                                     \
}                                                                                                                                                           \
static TABLE_DATA_TYPE* necro_##TABLE_SNAKE_NAME##_table_get(Necro##TABLE_CAMEL_NAME##Table* table, uint64_t key)                                           \
{                                                                                                                                                           \
    return necro_arena_chain_table_get(&table->chain_table, key);                                                                                           \
}                                                                                                                                                           \
static inline void necro_##TABLE_SNAKE_NAME##_table_iterate(Necro##TABLE_CAMEL_NAME##Table* table, void (f)(TABLE_DATA_TYPE*, void*), void* extras)         \
{                                                                                                                                                           \
    for (size_t i = 0; i < table->chain_table.size; ++i)\
    {\
        NecroChainTableNode* curr = table->chain_table.buckets[i].next;\
        while (curr != NULL)\
        {\
            f(((void*)(curr + 1)), extras);\
            curr = curr->next;\
        }\
    }\
}

#endif // HASH_TABLE_H

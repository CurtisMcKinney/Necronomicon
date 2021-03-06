/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_LIST_H
#define NECRO_LIST_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "arena.h"

// Guarantees:
//     * O(N) insertion, retrieval, iterations
//     * All data is cleared up when the arena is destroyed
//     * All pointers returned by the list are stable
//     * All data inserted is copied in.

#define NECRO_DECLARE_ARENA_LIST(TABLE_DATA_TYPE, CAMEL_NAME, SNAKE_NAME)                                                                   \
typedef struct Necro##CAMEL_NAME##List                                                                                                      \
{                                                                                                                                           \
    struct Necro##CAMEL_NAME##List* next;                                                                                                   \
    TABLE_DATA_TYPE                 data;                                                                                                   \
} Necro##CAMEL_NAME##List;                                                                                                                  \
static Necro##CAMEL_NAME##List* necro_cons_##SNAKE_NAME##_list(NecroPagedArena* arena, TABLE_DATA_TYPE data, Necro##CAMEL_NAME##List* next) \
{                                                                                                                                           \
    Necro##CAMEL_NAME##List* cell = necro_paged_arena_alloc(arena, sizeof(Necro##CAMEL_NAME##List));                                        \
    cell->next = next;                                                                                                                      \
    cell->data = data;                                                                                                                      \
    return cell;                                                                                                                            \
}                                                                                                                                           \
static Necro##CAMEL_NAME##List* necro_append_##SNAKE_NAME##_list(NecroPagedArena* arena, TABLE_DATA_TYPE data, Necro##CAMEL_NAME##List* head)\
{                                                                                                                                           \
    Necro##CAMEL_NAME##List* cell = necro_paged_arena_alloc(arena, sizeof(Necro##CAMEL_NAME##List));                                        \
    cell->next = NULL;                                                                                                                      \
    cell->data = data;                                                                                                                      \
    Necro##CAMEL_NAME##List* curr = head;                                                                                                   \
    if (curr != NULL)                                                                                                                       \
    {                                                                                                                                       \
        while (curr->next != NULL)                                                                                                          \
        {                                                                                                                                   \
            curr = curr->next;                                                                                                              \
        }                                                                                                                                   \
        curr->next = cell;                                                                                                                  \
        return head;                                                                                                                        \
    }                                                                                                                                       \
    return cell;                                                                                                                            \
}                                                                                                                                           \
static Necro##CAMEL_NAME##List* necro_reverse_##SNAKE_NAME##_list(NecroPagedArena* arena, Necro##CAMEL_NAME##List* head)                    \
{                                                                                                                                           \
    Necro##CAMEL_NAME##List* new_head = NULL;                                                                                               \
    while (head != NULL)                                                                                                                    \
    {                                                                                                                                       \
        new_head = necro_cons_##SNAKE_NAME##_list(arena, head->data, new_head);                                                             \
        head = head->next;                                                                                                                  \
    }                                                                                                                                       \
    return new_head;                                                                                                                        \
}                                                                                                                                           \
static size_t necro_count_##SNAKE_NAME##_list(Necro##CAMEL_NAME##List* head)                                                                \
{                                                                                                                                           \
    size_t count = 0;                                                                                                                       \
    while (head != NULL)                                                                                                                    \
    {                                                                                                                                       \
        count++;                                                                                                                            \
        head = head->next;                                                                                                                  \
    }                                                                                                                                       \
    return count;                                                                                                                           \
}

// // testing
// NECRO_DECLARE_ARENA_LIST(uint64_t, TestInt, testint);
// static void test_int_list()
// {
//     NecroTestIntList* list = necro_cons_testint_list(NULL, 666, NULL);
//     list = necro_snoc_testint_list(NULL, 333, list);
// }

#endif // NECRO_LIST_H

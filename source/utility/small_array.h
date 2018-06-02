/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_SMALL_ARRAY_H
#define NECRO_SMALL_ARRAY_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include "arena.h"

// Guarantees:

#define NECRO_DECLARE_SMALL_ARRAY(TABLE_DATA_TYPE, CAMEL_NAME, SNAKE_NAME, SMALL_COUNT)                               \
typedef struct                                                                                                        \
{                                                                                                                     \
    size_t           count;                                                                                           \
    TABLE_DATA_TYPE* _unsafe_ptr;                                                                                     \
    TABLE_DATA_TYPE  _unsafe_array[SMALL_COUNT];                                                                      \
} Necro##CAMEL_NAME##Array;                                                                                           \
Necro##CAMEL_NAME##Array necro_create_##SNAKE_NAME##_array(size_t count)                                              \
{                                                                                                                     \
    if (count > SMALL_COUNT)                                                                                          \
        return (Necro##CAMEL_NAME##Array) { ._unsafe_ptr = malloc(sizeof(TABLE_DATA_TYPE) * count), .count = count }; \
    else                                                                                                              \
        return (Necro##CAMEL_NAME##Array) { ._unsafe_ptr = NULL, .count = count};                                     \
}                                                                                                                     \
TABLE_DATA_TYPE* necro_##SNAKE_NAME##_array_get(Necro##CAMEL_NAME##Array* small_array, size_t index)                  \
{                                                                                                                     \
    if (small_array->count > SMALL_COUNT)                                                                             \
        return small_array->_unsafe_ptr + index;                                                                      \
    else                                                                                                              \
        return small_array->_unsafe_array + index;                                                                    \
}                                                                                                                     \
void necro_destroy_##SNAKE_NAME##_array(Necro##CAMEL_NAME##Array* small_array)                                        \
{                                                                                                                     \
    if (small_array->count > SMALL_COUNT)                                                                             \
    {                                                                                                                 \
        free(small_array->_unsafe_ptr);                                                                               \
        small_array->_unsafe_ptr = NULL;                                                                              \
    }                                                                                                                 \
}

// // testing
// NECRO_DECLARE_SMALL_ARRAY(uint64_t, TestInt, testint, 5);
// static void test_int_list()
// {
//     // NecroTestIntArray* arr = necro_cons_testint_list(NULL, 666, NULL);
//     // list = necro_snoc_testint_list(NULL, 333, list);
// }

#endif // NECRO_SMALL_ARRAY_H
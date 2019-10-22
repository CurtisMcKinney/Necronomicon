/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_DEQUEUE_H
#define NECRO_DEQUEUE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

// Guarantees:
//     * O(1) (Amortized) push_front, push_back, pop_back, pop_front
//     * All data is cleared up when the arena is destroyed
//     * Memory in the Dequeue is unstable and will realloc using standard dynamic array algorithm
//     * All data inserted is copied in.

#define NECRO_DECLARE_DEQUEUE(TABLE_DATA_TYPE, CAMEL_NAME, SNAKE_NAME)\
typedef struct Necro##CAMEL_NAME##Dequeue\
{\
    TABLE_DATA_TYPE*  data;\
    size_t            capacity;\
    size_t            count;\
    size_t            head;\
    size_t            tail;\
} Necro##CAMEL_NAME##Dequeue;\
static Necro##CAMEL_NAME##Dequeue necro_##SNAKE_NAME##_dequeue_empty()\
{\
    Necro##CAMEL_NAME##Dequeue dequeue;\
    dequeue.capacity = 0;\
    dequeue.count    = 0;\
    dequeue.head     = 0;\
    dequeue.tail     = 0;\
    dequeue.data     = NULL;\
    return dequeue;\
}\
static Necro##CAMEL_NAME##Dequeue necro_##SNAKE_NAME##_dequeue_create(size_t initial_capacity)\
{\
    assert(initial_capacity > 0);\
    Necro##CAMEL_NAME##Dequeue dequeue;\
    dequeue.capacity = initial_capacity;\
    dequeue.count    = 0;\
    dequeue.head     = 0;\
    dequeue.tail     = 0;\
    dequeue.data     = emalloc(dequeue.capacity * sizeof(TABLE_DATA_TYPE));\
    return dequeue;\
}\
static void necro_##SNAKE_NAME##_dequeue_destroy(Necro##CAMEL_NAME##Dequeue* dequeue)\
{\
    if (dequeue->data == NULL)\
        return;\
    free(dequeue->data);\
    *dequeue = necro_##SNAKE_NAME##_dequeue_empty();\
}\
static void necro_##SNAKE_NAME##_dequeue_grow(Necro##CAMEL_NAME##Dequeue* dequeue);\
static bool necro_##SNAKE_NAME##_dequeue_pop_front(Necro##CAMEL_NAME##Dequeue* dequeue, TABLE_DATA_TYPE* out_element)\
{\
    if (dequeue->count == 0)\
        return false;\
    dequeue->count--;\
    *out_element  = dequeue->data[dequeue->head];\
    dequeue->head = (dequeue->head + 1) % dequeue->capacity;\
    return true;\
}\
static bool necro_##SNAKE_NAME##_dequeue_pop_back(Necro##CAMEL_NAME##Dequeue* dequeue, TABLE_DATA_TYPE* out_element)\
{\
    if (dequeue->count == 0)\
        return false;\
    dequeue->count--;\
    *out_element  = dequeue->data[dequeue->tail];\
    dequeue->tail = (dequeue->tail == 0) ? (dequeue->capacity - 1) : (dequeue->tail - 1);\
    return true;\
}\
static void necro_##SNAKE_NAME##_dequeue_push_front(Necro##CAMEL_NAME##Dequeue* dequeue, TABLE_DATA_TYPE element)\
{\
    necro_##SNAKE_NAME##_dequeue_grow(dequeue);\
    dequeue->count++;\
    dequeue->data[dequeue->head] = element;\
    dequeue->head                = (dequeue->head == 0) ? (dequeue->capacity - 1) : (dequeue->head - 1);\
    assert(dequeue->head != dequeue->tail);\
}\
static void necro_##SNAKE_NAME##_dequeue_push_back(Necro##CAMEL_NAME##Dequeue* dequeue, TABLE_DATA_TYPE element)\
{\
    necro_##SNAKE_NAME##_dequeue_grow(dequeue);\
    dequeue->count++;\
    dequeue->data[dequeue->tail] = element;\
    dequeue->tail                = (dequeue->tail + 1) % dequeue->capacity;\
    assert(dequeue->head != dequeue->tail);\
}\
static void necro_##SNAKE_NAME##_dequeue_grow(Necro##CAMEL_NAME##Dequeue* dequeue)\
{\
    if ((dequeue->count + 1) < dequeue->capacity)\
        return;\
    size_t                     initial_count = dequeue->count;\
    Necro##CAMEL_NAME##Dequeue new_dequeue   = necro_##SNAKE_NAME##_dequeue_create(dequeue->capacity * 2);\
    TABLE_DATA_TYPE            element;\
    while (necro_##SNAKE_NAME##_dequeue_pop_front(dequeue, &element))\
    {\
        necro_##SNAKE_NAME##_dequeue_push_back(&new_dequeue, element);\
    }\
    assert(new_dequeue.count == initial_count);\
    necro_##SNAKE_NAME##_dequeue_destroy(dequeue);\
    *dequeue = new_dequeue;\
}

#endif // NECRO_DEQUEUE_H
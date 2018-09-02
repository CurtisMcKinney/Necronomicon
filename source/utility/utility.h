/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef UTILITY_H
#define UTILITY_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

//=====================================================
// A collection of general purpose structs and functions
//=====================================================

typedef struct
{
    uint32_t id;
} NecroID;

//=====================================================
// Error Messaging
//=====================================================
#define NECRO_MAX_ERROR_MESSAGE_LENGTH 2048

typedef enum
{
    NECRO_SUCCESS,
    NECRO_ERROR,
    NECRO_WARNING
} NECRO_RETURN_CODE;

typedef struct
{
    size_t line;
    size_t character;
    size_t pos;
} NecroSourceLoc;

typedef struct
{
    char              error_message[NECRO_MAX_ERROR_MESSAGE_LENGTH];
    NecroSourceLoc    source_loc;
    NECRO_RETURN_CODE return_code;
} NecroError;

NecroError necro_create_error();
size_t     necro_verror(NecroError* error, NecroSourceLoc source_loc, const char* error_message, va_list args);
size_t     necro_error(NecroError* error, NecroSourceLoc source_loc, const char* error_message, ...);
void       necro_print_error(NecroError* error, const char* input_string, const char* error_type);

//=====================================================
// NecroVector:
//   * A very simple dynamic array, also known as a "vector".
//   * Has separate length and capacity.
//     When an insertion would increase the length past the capacity
//     the data pointer is reallocated to be twice the size
//     This makes amortized insertions O(1),
//     however worst case is obviously worse than this.
//     Thus this should not be used is contexts where absolutely consistent
//     behavior and no allocations is to be expected!
//   * Values are copied upon insertion.
//   * Values will not have free called upon them when the vector is freed,
//     if this is necessary it is up to the user to free the values
//     contained within before freeing the vector.
//=====================================================
#define NECRO_INTIAL_VECTOR_SIZE 16
#define NECRO_DECLARE_VECTOR(type, camel_type, snake_type)                         \
typedef struct                                                                     \
{                                                                                  \
    type*  data;                                                                   \
    size_t length;                                                                 \
    size_t capacity;                                                               \
} camel_type##Vector;                                                              \
                                                                                   \
static camel_type##Vector necro_create_##snake_type##_vector()                     \
{                                                                                  \
    type* data = malloc(NECRO_INTIAL_VECTOR_SIZE * sizeof(type));                  \
    if (data == NULL)                                                              \
    {                                                                              \
        fprintf(stderr, "Malloc returned null creating vector!\n");                \
        exit(1);                                                                   \
    }                                                                              \
    return (camel_type##Vector)                                                    \
    {                                                                              \
        data,                                                                      \
        0,                                                                         \
        NECRO_INTIAL_VECTOR_SIZE                                                   \
    };                                                                             \
}                                                                                  \
                                                                                   \
static void necro_destroy_##snake_type##_vector(camel_type##Vector* vec)           \
{                                                                                  \
    vec->length   = 0;                                                             \
    vec->capacity = 0;                                                             \
    free(vec->data);                                                               \
    vec->data     = NULL;                                                          \
}                                                                                  \
                                                                                   \
static void necro_push_##snake_type##_vector(camel_type##Vector* vec, type* item)  \
{                                                                                  \
    if (vec->length >= vec->capacity)                                              \
    {                                                                              \
        vec->capacity  = vec->capacity * 2;                                        \
        type* new_data = realloc(vec->data, vec->capacity * sizeof(type));         \
        if (new_data == NULL)                                                      \
        {                                                                          \
            if (vec->data != NULL)                                                 \
                free(vec->data);                                                   \
            fprintf(stderr, "Malloc returned NULL in vector reallocation!\n");     \
            exit(1);                                                               \
        }                                                                          \
        vec->data = new_data;                                                      \
    }                                                                              \
    assert(vec->data != NULL);                                                     \
    vec->data[vec->length] = *item;                                                \
    vec->length++;                                                                 \
}                                                                                  \
                                                                                   \
static type necro_pop_##snake_type##_vector(camel_type##Vector* vec)               \
{                                                                                  \
    assert(vec->data != NULL);                                                     \
    assert(vec->length > 0);                                                       \
    vec->length--;                                                                 \
    type e = vec->data[vec->length];                                               \
    return e;                                                                      \
}

//=====================================================
// NecroStringSlice:
//     * A "slice" is a particular view into a larger string
//     * Since the view is part of a larger whole, it is not null-terminated
//     * This requires explicitly holding a length value to
//       accompany the data pointer.
//     * This should NOT be freed manually, as it lives for the length
//       of the owning string.
//=====================================================
typedef struct
{
    const char* data;
    size_t      length;
} NecroStringSlice;

inline uint32_t next_highest_pow_of_2(uint32_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return x;
}

// inline size_t next_highest_pow_of_2_size_t(size_t x)
// {
//     x--;
//     x |= x >> 1;
//     x |= x >> 2;
//     x |= x >> 4;
//     x |= x >> 8;
//     x |= x >> 16;
//     x |= x >> 32;
//     x++;
//     return x;
// }

static const uint32_t tab32[32] =
{
  0,   9,  1, 10, 13, 21,  2, 29,
  11, 14, 16, 18, 22, 25,  3, 30,
  8,  12, 20, 28, 15, 17, 24,  7,
  19, 27, 23,  6, 26,  5,  4, 31
};

static uint32_t log2_32(uint32_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return tab32[(uint32_t)(value * 0x07C4ACDD) >> 27];
}

inline size_t necro_hash(size_t input)
{
    return (size_t)(input * 37);
}

void print_white_space(size_t white_count);

static void necro_announce_phase(const char* phase_name)
{
    // fflush(stdout);
    // fflush(stderr);
    puts("");
    puts("--------------------------------");
    printf("-- %s\n", phase_name);
    puts("--------------------------------");
}

#define BIT(x) (1 << x)

#define STRING_TAB "  "

#define UNUSED(x) (void)(x)

///////////////////////////////////////////////////////
// Timing
///////////////////////////////////////////////////////
struct NecroTimer;
struct NecroTimer* necro_create_timer();
void               necro_destroy_timer(struct NecroTimer* timer);
void               necro_start_timer(struct NecroTimer* timer);
double             necro_stop_timer(struct NecroTimer* timer);
void               necro_stop_and_report_timer(struct NecroTimer* timer, const char* print_header);

#endif // UTILITY_H

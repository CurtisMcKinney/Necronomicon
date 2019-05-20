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

#include "debug_memory.h"

#if (!__DEBUG)
#define _RELEASE 1
#define __RELEASE 1
#define RELEASE 1
#endif

//=====================================================
// A collection of general purpose structs and functions
//=====================================================

static inline void necro_exit(int code)
{
    MEM_CHECK();
    exit(code);
}

typedef struct
{
    uint32_t id;
} NecroID;

#if DEBUG_MEMORY
#define emalloc(A_SIZE) __emalloc(A_SIZE, __FILE__, __LINE__)
#else
#define emalloc(A_SIZE) __emalloc(A_SIZE)
#endif

#if DEBUG_MEMORY
inline void* __emalloc(const size_t a_size, const char *srcFile, int srcLine)
#else
static inline void* __emalloc(const size_t a_size)
#endif
{
#if DEBUG_MEMORY
    #if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
    void* data = _malloc_dbg(a_size, _NORMAL_BLOCK, srcFile, srcLine);
    #else
    void* data = malloc(a_size);
    #endif // defined(_WIN32) || defined(WIN32) || defined(_WIN64)
#else // DEBUG_MEMORY
    void* data = malloc(a_size);
#endif
    if (data == NULL)
    {
        printf("Could not allocate enough memory: %zu\n", a_size);
        necro_exit(1);
    }
    return data;
}

//=====================================================
// Error Messaging
//=====================================================
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

#define INVALID_LINE SIZE_MAX
#define zero_loc ((NecroSourceLoc) { 0, 0, 0 })
#define NULL_LOC ((NecroSourceLoc) { INVALID_LINE , INVALID_LINE, INVALID_LINE })

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
#define NECRO_DECLARE_VECTOR(type, camel_type, snake_type)                               \
typedef struct                                                                           \
{                                                                                        \
    type*  data;                                                                         \
    size_t length;                                                                       \
    size_t capacity;                                                                     \
} camel_type##Vector;                                                                    \
                                                                                         \
static inline camel_type##Vector necro_empty_##snake_type##_vector()                     \
{                                                                                        \
    return (camel_type##Vector)                                                          \
    {                                                                                    \
        NULL,                                                                            \
        0,                                                                               \
        0                                                                                \
    };                                                                                   \
}                                                                                        \
                                                                                         \
static inline camel_type##Vector necro_create_##snake_type##_vector()                    \
{                                                                                        \
    type* data = emalloc(NECRO_INTIAL_VECTOR_SIZE * sizeof(type));                       \
    return (camel_type##Vector)                                                          \
    {                                                                                    \
        data,                                                                            \
        0,                                                                               \
        NECRO_INTIAL_VECTOR_SIZE                                                         \
    };                                                                                   \
}                                                                                        \
                                                                                         \
static inline void necro_destroy_##snake_type##_vector(camel_type##Vector* vec)          \
{                                                                                        \
    vec->length   = 0;                                                                   \
    vec->capacity = 0;                                                                   \
    if (vec->data != NULL)                                                               \
        free(vec->data);                                                                 \
    vec->data     = NULL;                                                                \
}                                                                                        \
                                                                                         \
static inline void necro_push_##snake_type##_vector(camel_type##Vector* vec, type* item) \
{                                                                                        \
    if (vec->length >= vec->capacity)                                                    \
    {                                                                                    \
        vec->capacity  = vec->capacity * 2;                                              \
        type* new_data = realloc(vec->data, vec->capacity * sizeof(type));               \
        if (new_data == NULL)                                                            \
        {                                                                                \
            if (vec->data != NULL)                                                       \
                free(vec->data);                                                         \
            fprintf(stderr, "Malloc returned NULL in vector reallocation!\n");           \
            necro_exit(1);                                                               \
        }                                                                                \
        vec->data = new_data;                                                            \
    }                                                                                    \
    assert(vec->data != NULL);                                                           \
    vec->data[vec->length] = *item;                                                      \
    vec->length++;                                                                       \
}                                                                                        \
                                                                                         \
static inline type necro_pop_##snake_type##_vector(camel_type##Vector* vec)              \
{                                                                                        \
    assert(vec->data != NULL);                                                           \
    assert(vec->length > 0);                                                             \
    vec->length--;                                                                       \
    return vec->data[vec->length];                                                       \
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

inline inline uint32_t next_highest_pow_of_2(uint32_t x)
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

static const uint32_t tab32[32] =
{
  0,   9,  1, 10, 13, 21,  2, 29,
  11, 14, 16, 18, 22, 25,  3, 30,
  8,  12, 20, 28, 15, 17, 24,  7,
  19, 27, 23,  6, 26,  5,  4, 31
};

static inline uint32_t log2_32(uint32_t value)
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

static inline void necro_announce_phase(const char* phase_name)
{
    printf("\n");
    printf("--------------------------------\n");
    printf("-- %s\n", phase_name);
    printf("--------------------------------\n");
}

#define BIT(x) (1 << x)

#define STRING_TAB "  "

#define UNUSED(x) (void)(x)

#if __RELEASE
#define DEBUG_BREAK() assert(false)
#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64)
#include <intrin.h>
#define DEBUG_BREAK() __debugbreak()
#elif defined(__unix)
#include <signal.h>
    #if __has_builtin(__builtin_debugtrap)
        #define DEBUG_BREAK() __builtin_debugtrap()
    #elif defined(SIGTRAP)
        #define DEBUG_BREAK() raise(SIGTRAP)
    #else
        #define DEBUG_BREAK() assert(false)
    #endif
#else
#define DEBUG_BREAK assert(false)
#endif

#define ASSERT_BREAK(b) if (!(b)) { fprintf(stderr, "ASSERT_BREAK failed in %s line %d: %s\n", __FUNCTION__, __LINE__, #b); DEBUG_BREAK(); }

typedef int process_error_code_t;
process_error_code_t necro_compile_in_child_process(const char* command_line_arguments);

///////////////////////////////////////////////////////
// Timing
///////////////////////////////////////////////////////
struct NecroTimer;
struct NecroTimer* necro_timer_create();
void               necro_timer_destroy(struct NecroTimer* timer);
void               necro_timer_start(struct NecroTimer* timer);
double             necro_timer_stop(struct NecroTimer* timer);
void               necro_timer_stop_and_report(struct NecroTimer* timer, const char* print_header);
#endif // UTILITY_H

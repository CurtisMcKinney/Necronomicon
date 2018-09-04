/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_RESULT_H
#define NECRO_RESULT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "utility.h"
#include "arena.h"

///////////////////////////////////////////////////////
// NOTE: Set to 1 to assert at NecroResult errors
///////////////////////////////////////////////////////
#define NECRO_ASSERT_RESULT_ERROR 0

///////////////////////////////////////////////////////
// Error Types
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_CANT_OPEN_FILE  = 1,
    NECRO_MALFORMED_FLOAT,
    NECRO_MALFORMED_STRING,
    NECRO_UNRECOGNIZED_CHARACTER_SEQUENCE,
    NECRO_MIXED_BRACES,
    NECRO_MULTIPLE_DEFINITIONS,
} NECRO_RESULT_ERROR_TYPE;

typedef struct
{
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroMalformedString;

typedef struct
{
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroMalformedFloat;

typedef struct
{
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroMixedBraces;

typedef struct
{
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroUnrecognizedCharacterSequence;

typedef struct
{
    NecroSourceLoc definition1_location;
    NecroSourceLoc definition2_location;
} NecroMultipleDefinitions;

typedef struct
{
    union
    {
        NecroMalformedString               malformed_string;
        NecroMalformedFloat                malformed_float;
        NecroUnrecognizedCharacterSequence unrecognized_character_sequence;
        NecroMixedBraces                   mixed_braces;
        NecroMultipleDefinitions           multiple_definitions;
    };
    NECRO_RESULT_ERROR_TYPE type;
} NecroResultError;

///////////////////////////////////////////////////////
// NecroResult
///////////////////////////////////////////////////////
#define NECRO_DECLARE_RESULT(TYPE) \
typedef struct \
{ \
    size_t num_errors; \
    union \
    { \
        NecroResultError* errors; \
        TYPE value; \
    }; \
} NecroResult_##TYPE; \
inline NecroResult_##TYPE ok_##TYPE(TYPE value) \
{ \
    return (NecroResult_##TYPE) { .value = value, .num_errors = 0 }; \
}

#define NECRO_DECLARE_PTR_RESULT(TYPE) \
typedef struct \
{ \
    size_t num_errors; \
    union \
    { \
        NecroResultError* errors; \
        TYPE* value; \
    }; \
} NecroResult_##TYPE;

NECRO_DECLARE_RESULT(size_t);
NECRO_DECLARE_RESULT(bool);
// NECRO_DECLARE_PTR_RESULT(void);

typedef struct
{
    size_t num_errors;
    union
    {
        NecroResultError* errors;
    };
} NecroResult_void;

typedef union
{
    NecroResult_void   void_result;
    NecroResult_bool   bool_result;
    NecroResult_size_t size_t_result;
} NecroResultUnion;

// TODO: If and when the compiler becomes threaded,
// replace this with a thread local storage version!
extern NecroResultUnion global_result;

///////////////////////////////////////////////////////
// Macro API
///////////////////////////////////////////////////////
#define NecroResult(TYPE) NecroResult_##TYPE
#define necro_try(TYPE, EXPR) (global_result.TYPE##_result = EXPR).value; if (global_result.TYPE##_result.num_errors != 0) return global_result.TYPE##_result;

#define unwrap(TYPE, EXPR) (global_result.TYPE##_result = EXPR).value; assert(global_result.TYPE##_result.num_errors == 0);

#define necro_try_map(TYPE, TYPE2, EXPR) (global_result.TYPE##_result = EXPR).value; if (global_result.TYPE##_result.num_errors != 0) return global_result.TYPE2##_result;

inline NecroResult(void) ok_void()
{
    return (NecroResult_void) { .num_errors = 0 };
}

///////////////////////////////////////////////////////
// Error API
///////////////////////////////////////////////////////
NecroResult(bool) necro_malformed_string_error(NecroPagedArena* arena, NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(bool) necro_malformed_float_error(NecroPagedArena* arena, NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(bool) necro_unrecognized_character_sequence_error(NecroPagedArena* arena, NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(bool) necro_mixed_braces_error(NecroPagedArena* arena, NecroSourceLoc location, NecroSourceLoc end_loc);
void              necro_print_result_error(NecroResultError error, const char* source_str, const char* source_name);
void              necro_print_result_errors(NecroResultError* errors, size_t num_errors, const char* source_str, const char* source_name);

// TODO: necro_map_result, necro_and_then

#endif // NECRO_RESULT_H

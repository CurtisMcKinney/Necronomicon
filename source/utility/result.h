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
struct NecroResultError;
typedef enum
{
    NECRO_CANT_OPEN_FILE  = 1,

    NECRO_LEX_MALFORMED_FLOAT,
    NECRO_LEX_MALFORMED_STRING,
    NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE,
    NECRO_LEX_MIXED_BRACES,

    NECRO_PARSE_ERROR,
    NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE,
    NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_RHS_EMPTY_WHERE,
    NECRO_PARSE_LET_FAILED_TO_PARSE,
    NECRO_PARSE_LET_EMPTY_IN,
    NECRO_PARSE_LET_EXPECTED_SEMICOLON,
    NECRO_PARSE_LET_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_LET_MISSING_IN,
    NECRO_PARSE_TUPLE_MISSING_PAREN,
    NECRO_PARSE_PAREN_EXPRESSION_FAILED_TO_PARSE,
    NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN,
    NECRO_PARSE_IF_FAILED_TO_PARSE,
    NECRO_PARSE_IF_MISSING_THEN,
    NECRO_PARSE_IF_MISSING_EXPR_AFTER_THEN,
    NECRO_PARSE_IF_MISSING_ELSE,
    NECRO_PARSE_IF_MISSING_EXPR_AFTER_ELSE,
    NECRO_PARSE_LAMBDA_FAILED_TO_PARSE,
    NECRO_PARSE_LAMBDA_MISSING_ARROW,
    NECRO_PARSE_LAMBDA_FAILED_TO_PARSE_PATTERN,
    NECRO_PARSE_DO_BIND_FAILED_TO_PARSE,
    NECRO_PARSE_DO_MISSING_RIGHT_BRACE,
    NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET,
    NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE,
    NECRO_PARSE_PATTERN_EMPTY_EXPRESSION_LIST,
    NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN,
    NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO,
    NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET,

    NECRO_MULTIPLE_DEFINITIONS,

} NECRO_RESULT_ERROR_TYPE;

typedef struct
{
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroDefaultErrorData;

typedef struct
{
    NecroSourceLoc source_loc1;
    NecroSourceLoc end_loc1;
    NecroSourceLoc source_loc2;
    NecroSourceLoc end_loc2;
} NecroDefaultErrorData2;

typedef struct
{
    NecroSourceLoc def1_source_loc;
    NecroSourceLoc def1_end_loc;
    NecroSourceLoc def2_source_loc;
    NecroSourceLoc def2_end_loc;
} NecroMultipleDefinitions;

typedef struct
{
    struct NecroResultError* errors;
    size_t                   num_errors;
} NecroErrorList;

typedef struct NecroResultError
{
    union
    {
        NecroDefaultErrorData    default_error_data;
        NecroDefaultErrorData    default_error_data2;
        NecroErrorList           error_list;
        NecroMultipleDefinitions multiple_definitions;
    };
    NECRO_RESULT_ERROR_TYPE type;
} NecroResultError;

///////////////////////////////////////////////////////
// NecroResult
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_RESULT_OK,
    NECRO_RESULT_ERROR,
} NECRO_RESULT_TYPE;
#define NECRO_DECLARE_RESULT(TYPE)              \
typedef struct                                  \
{                                               \
    NECRO_RESULT_TYPE type;                     \
    union                                       \
    {                                           \
        NecroResultError error;                 \
        TYPE value;                             \
    };                                          \
} NecroResult_##TYPE;                           \
inline NecroResult_##TYPE ok_##TYPE(TYPE value) \
{                                               \
    return (NecroResult_##TYPE)                 \
    {                                           \
        .value = value,                         \
        .type = NECRO_RESULT_OK                 \
    };                                          \
}

#define NECRO_DECLARE_PTR_RESULT(TYPE)                  \
typedef struct                                          \
{                                                       \
    NECRO_RESULT_TYPE type;                             \
    union                                               \
    {                                                   \
        NecroResultError error;                         \
        struct TYPE* value;                             \
    };                                                  \
} NecroResult_##TYPE;                                   \
inline NecroResult_##TYPE ok_##TYPE(struct TYPE* value) \
{                                                       \
    return (NecroResult_##TYPE)                         \
    {                                                   \
        .value = value,                                 \
        .type = NECRO_RESULT_OK                         \
    };                                                  \
}

typedef size_t NecroAST_LocalPtr;
typedef size_t NecroUnit;
#define necro_unit 0
NECRO_DECLARE_RESULT(size_t);
NECRO_DECLARE_RESULT(bool);
NECRO_DECLARE_RESULT(NecroAST_LocalPtr);
NECRO_DECLARE_RESULT(NecroUnit);

typedef union
{
    NecroResult_NecroUnit         NecroUnit_result;
    NecroResult_bool              bool_result;
    NecroResult_size_t            size_t_result;
    NecroResult_NecroAST_LocalPtr NecroAST_LocalPtr_result;
} NecroResultUnion;

// TODO: If and when the compiler becomes threaded,
// replace this with a thread local storage version!
extern NecroResultUnion global_result;

///////////////////////////////////////////////////////
// Macro API
///////////////////////////////////////////////////////
#define NecroResult(TYPE) NecroResult_##TYPE
#define necro_try(TYPE, EXPR) (global_result.TYPE##_result = EXPR).value; if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE##_result;
#define necro_try_map(TYPE, TYPE2, EXPR) (global_result.TYPE##_result = EXPR).value; if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE2##_result;
#define unwrap(TYPE, EXPR) (global_result.TYPE##_result = EXPR).value; assert(global_result.TYPE##_result.type == NECRO_RESULT_OK);

///////////////////////////////////////////////////////
// Error API
///////////////////////////////////////////////////////
NecroResult(bool)              necro_malformed_string_error(NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(bool)              necro_malformed_float_error(NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(NecroUnit)         necro_unrecognized_character_sequence_error(NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(NecroUnit)         necro_mixed_braces_error(NecroSourceLoc location, NecroSourceLoc end_loc);
NecroResult(NecroAST_LocalPtr) necro_parse_error(NecroSourceLoc location, NecroSourceLoc end_loc);
void                           necro_print_result_error(NecroResultError error, const char* source_str, const char* source_name);

// TODO: necro_map_result, necro_and_then

#endif // NECRO_RESULT_H

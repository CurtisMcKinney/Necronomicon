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
    NECRO_ERROR_CONS,

    NECRO_LEX_MALFORMED_FLOAT,
    NECRO_LEX_MALFORMED_STRING,
    NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE,
    NECRO_LEX_MIXED_BRACES,

    NECRO_PARSE_ERROR,
    NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE,
    NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE,
    NECRO_PARSE_RHS_EMPTY_WHERE,
    NECRO_PARSE_LET_FAILED_TO_PARSE,
    NECRO_PARSE_LET_EMPTY_IN,
    NECRO_PARSE_LET_EXPECTED_SEMICOLON,
    NECRO_PARSE_LET_EXPECTED_LEFT_BRACE,
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
    NECRO_PARSE_DO_LET_EXPECTED_LEFT_BRACE,
    NECRO_PARSE_DO_LET_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_DO_MISSING_RIGHT_BRACE,
    NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET,
    NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE,
    NECRO_PARSE_PATTERN_EMPTY_EXPRESSION_LIST,
    NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN,
    NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO,
    NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET,
    NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN,
    NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW,
    NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION,
    NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF,
    NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE,
    NECRO_PARSE_CASE_ALTERNATIVE_EMPTY,
    NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_FN_OP_EXPECTED_ACCENT,
    NECRO_PARSE_DATA_EXPECTED_TYPE,
    NECRO_PARSE_DATA_EXPECTED_ASSIGN,
    NECRO_PARSE_DATA_EXPECTED_DATA_CON,
    NECRO_PARSE_TYPE_EXPECTED_TYPE,
    NECRO_PARSE_TYPE_LIST_EXPECTED_RIGHT_BRACKET,
    NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_CONST_CON_MISSING_RIGHT_PAREN,

    NECRO_MULTIPLE_DEFINITIONS,

} NECRO_RESULT_ERROR_TYPE;

typedef struct
{
    NecroSourceLoc source_loc;
    NecroSourceLoc end_loc;
} NecroDefaultErrorData;

typedef struct
{
    struct NecroResultError* error1;
    struct NecroResultError* error2;
} NecroErrorCons;

// typedef struct
// {
//     NecroSourceLoc source_loc1;
//     NecroSourceLoc end_loc1;
//     NecroSourceLoc source_loc2;
//     NecroSourceLoc end_loc2;
// } NecroDefaultErrorData2;

// typedef struct
// {
//     NecroSourceLoc def1_source_loc;
//     NecroSourceLoc def1_end_loc;
//     NecroSourceLoc def2_source_loc;
//     NecroSourceLoc def2_end_loc;
// } NecroMultipleDefinitions;

// typedef struct
// {
//     struct NecroResultError* errors;
//     size_t                   num_errors;
// } NecroErrorList;

typedef struct NecroResultError
{
    union
    {
        NecroDefaultErrorData    default_error_data;
        NecroErrorCons           error_cons;
        // NecroDefaultErrorData    default_error_data2;
        // NecroErrorList           error_list;
        // NecroMultipleDefinitions multiple_definitions;
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

typedef size_t NecroParseAstLocalPtr;
// typedef size_t NecroUnit;
#define necro_unit 0
NECRO_DECLARE_RESULT(size_t);
NECRO_DECLARE_RESULT(bool);
NECRO_DECLARE_RESULT(NecroParseAstLocalPtr);
// NECRO_DECLARE_RESULT(NecroUnit);

typedef struct
{
    NECRO_RESULT_TYPE type;
    union
    {
        NecroResultError error;
        size_t           value;
    };
} NecroResult_void;
inline NecroResult_void ok_void()
{
    return (NecroResult_void) { .value = 0, .type = NECRO_RESULT_OK };
}

typedef union
{
    NecroResult_void                  void_result;
    NecroResult_bool                  bool_result;
    NecroResult_size_t                size_t_result;
    NecroResult_NecroParseAstLocalPtr NecroParseAstLocalPtr_result;
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
// TODO: Make errors allocate, otherwise NULL, to keep result types small and fast (ideally word sized)? Clean up after printing with manual free

NecroResult(bool)                  necro_malformed_string_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(bool)                  necro_malformed_float_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_unrecognized_character_sequence_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_mixed_braces_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_parse_error_cons(NecroResultError error1, NecroResultError error2);
NecroResult(NecroParseAstLocalPtr) necro_declarations_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_simple_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_apat_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_pat_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_rhs_empty_where_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_let_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_let_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_let_missing_in_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_let_empty_in_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_let_expected_semicolon_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_let_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_const_con_missing_right_brace(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_tuple_missing_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_paren_expression_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_paren_expression_missing_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_if_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_if_missing_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_if_missing_expr_after_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_if_missing_else_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_if_missing_expr_after_else_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_lambda_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_lambda_missing_arrow_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_lambda_failed_to_parse_pattern_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_do_let_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_do_let_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_do_bind_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_do_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_list_missing_right_bracket_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_array_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_pattern_empty_expression_list_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_arithmetic_sequence_failed_to_parse_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_arithmetic_sequence_failed_to_parse_to_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_arithmetic_sequence_missing_right_bracket_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_pattern_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_arrow_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_expression_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_of_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_empty_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_fn_op_expected_accent_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_data_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_data_expected_assign_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_data_expected_data_con_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_type_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_type_list_expected_right_bracket(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_class_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_instance_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);

void                               necro_result_error_print(NecroResultError error, const char* source_str, const char* source_name);

// TODO: necro_map_result, necro_and_then

#endif // NECRO_RESULT_H

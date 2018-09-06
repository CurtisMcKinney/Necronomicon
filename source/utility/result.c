/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "result.h"
#include <string.h>

NecroResultUnion global_result;

///////////////////////////////////////////////////////
// Construction
///////////////////////////////////////////////////////
// NecroResultError* necro_alloc_error(NecroPagedArena* arena, NecroResultError error)
// {
//     NecroResultError* error_ptr = necro_paged_arena_alloc(arena, sizeof(NecroResultError));
//     *error_ptr                  = error;
// #if NECRO_ASSERT_RESULT_ERROR
//     assert(false);
// #endif
//     return error_ptr;
// }

NecroResult(bool) necro_malformed_string_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return (NecroResult(bool))
    {
        .error =
        {
            .type               = NECRO_LEX_MALFORMED_STRING,
            .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
        },
        .type = NECRO_RESULT_ERROR
    };
}

NecroResult(bool) necro_malformed_float_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return (NecroResult(bool))
    {
        .error =
        {
            .type               = NECRO_LEX_MALFORMED_FLOAT,
            .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
        },
        .type = NECRO_RESULT_ERROR
    };
}

NecroResult(void) necro_unrecognized_character_sequence_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return (NecroResult(void))
    {
        .error =
        {
            .type               = NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE,
            .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
        },
        .type = NECRO_RESULT_ERROR
    };
}

NecroResult(void) necro_mixed_braces_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return (NecroResult(void))
    {
        .error =
        {
            .type               = NECRO_LEX_MIXED_BRACES,
            .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
        },
        .type = NECRO_RESULT_ERROR
    };
}

NecroResult(void) necro_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return (NecroResult(void))
    {
        .error =
        {
            .type               = NECRO_PARSE_ERROR,
            .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
        },
        .type = NECRO_RESULT_ERROR
    };
}

inline NecroResult(NecroAST_LocalPtr) necro_default_parse_error(NECRO_RESULT_ERROR_TYPE type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return (NecroResult(NecroAST_LocalPtr))
    {
        .error =
        {
            .type               = type,
            .default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc },
        },
        .type = NECRO_RESULT_ERROR
    };
}

NecroResult(NecroAST_LocalPtr) necro_declarations_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_simple_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_apat_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_pat_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_rhs_empty_where_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_RHS_EMPTY_WHERE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_let_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_let_empty_in_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EMPTY_IN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_let_expected_semicolon_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EXPECTED_SEMICOLON, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_let_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_let_missing_in_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_MISSING_IN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_tuple_missing_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_TUPLE_MISSING_PAREN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_paren_expression_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PAREN_EXPRESSION_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_paren_expression_missing_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_if_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_if_missing_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_THEN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_if_missing_expr_after_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_EXPR_AFTER_THEN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_if_missing_else_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_ELSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_if_missing_expr_after_else_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_EXPR_AFTER_ELSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_lambda_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LAMBDA_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_lambda_missing_arrow_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LAMBDA_MISSING_ARROW, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_lambda_failed_to_parse_pattern_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LAMBDA_FAILED_TO_PARSE_PATTERN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_do_bind_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DO_BIND_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_do_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DO_MISSING_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_list_missing_right_bracket_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_array_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_pattern_empty_expression_list_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PATTERN_EMPTY_EXPRESSION_LIST, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_arithmetic_sequence_failed_to_parse_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_arithmetic_sequence_failed_to_parse_to_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_arithmetic_sequence_missing_right_bracket_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_let_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EXPECTED_LEFT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_malformed_pattern_bind_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_MALFORMED_PATTERN_BIND, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_expected_pattern_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_expected_arrow_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_expected_expression_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_expected_of_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_empty_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EMPTY, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_case_alternative_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_fn_op_expected_accent_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_FN_OP_EXPECTED_ACCENT, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_data_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DATA_EXPECTED_TYPE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_data_expected_assign_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DATA_EXPECTED_ASSIGN, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_data_expected_data_con_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DATA_EXPECTED_DATA_CON, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_type_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_TYPE_EXPECTED_TYPE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_class_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroAST_LocalPtr) necro_instance_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}


///////////////////////////////////////////////////////
// Printing
///////////////////////////////////////////////////////
#ifdef WIN32
static const char* dir_separator = "\\";
#else
static const char* dir_separator = "/";
#endif

const char* file_base_name(const char* file_name)
{
    const char* search_string = strstr(file_name, dir_separator);
    do
    {
        file_name     = search_string + 1;
        search_string = strstr(file_name, dir_separator);
    } while (search_string != NULL);
    return file_name;
}

void necro_print_blank_error_gutter(NecroSourceLoc source_loc)
{
    fprintf(stderr, " ");
    size_t line = source_loc.line;
    while (line > 0)
    {
        fprintf(stderr, " ");
        line = line / 10;
    }
    fprintf(stderr, "  ");
}

void necro_print_error_gutter(NecroSourceLoc source_loc)
{
    fprintf(stderr, " %zu|  " , source_loc.line);
}

void necro_print_range_pointers(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_print_blank_error_gutter(source_loc);
    for (size_t i = 0; i < source_loc.character; ++i)
    {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^");
    for (size_t i = source_loc.character + 1; i < end_loc.character; ++i)
        fprintf(stderr, "-");
    fprintf(stderr, "\n");
}

void necro_print_line_at_source_loc(const char* source_str, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    size_t line_start = source_loc.pos;
    for (line_start = source_loc.pos; line_start > 0 && source_str[line_start] != '\0' && source_str[line_start] != '\n'; --line_start);
    if (source_str[line_start] == '\n')
        line_start++;
    size_t line_end   = line_start;
    for (line_end = line_start; source_str[line_end] != '\0' && source_str[line_end] != '\n'; ++line_end);
    necro_print_error_gutter(source_loc);
    fprintf(stderr, "%.*s\n", (int) (line_end - line_start), (source_str + line_start));
    necro_print_range_pointers(source_loc, end_loc);
}

void necro_print_default_error_format(const char* error_name, NecroSourceLoc source_loc, NecroSourceLoc end_loc, const char* source_str, const char* source_name, const char* explanation)
{
    // fprintf(stderr, "error[NE-%04u]: Malformed float\n", NECRO_MALFORMED_FLOAT);
    fprintf(stderr, "\n--- %s ------------------ ", error_name);
    // fprintf(stderr, N_L_CHAR " (%s:%zu:%zu)\n", source_name, source_loc.line, source_loc.character);
    // fprintf(stderr, "%s:%zu:%zu\n", file_base_name(source_name), source_loc.line, source_loc.character);
    fprintf(stderr, "%s\n", file_base_name(source_name));
    fprintf(stderr, "\n");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, "%s\n\n", explanation);
}

void necro_print_malformed_float_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "Float literals require digits both before and after the period (.) character.";
    necro_print_default_error_format("Malformed Float", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_malformed_string_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "Perhaps you forgot a closing quotation character (\") ?";
    necro_print_default_error_format("Malformed String", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_unrecognized_character_sequence_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "This is either an unholy message to Cthulhu or perhaps you mistyped some things...";
    necro_print_default_error_format("Unrecognized Character Sequence", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_mixed_braces_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "Necrolang uses significant whitespace which converts into implicit opening and closing braces.\nYou're likely adding an explicit closing brace where none is required.";
    necro_print_default_error_format("Mixed Braces", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Parse Error", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_declarations_missing_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing Right Brace", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_simple_assignment_rhs_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Assignment", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_apat_assignment_rhs_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Function", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_pat_assignment_rhs_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Pattern Assignment", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_rhs_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Definition", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_rhs_empty_where_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Empty \'where\' Declaration", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed \'let\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_empty_in_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Empty \'let\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_expected_semicolon_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Expected Semicolon after \'let\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_expected_left_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("\'let\' Expression Expected Left Brace", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_expected_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Expected Right Brace after \'let\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_missing_in_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing \'in\' After \'let\'", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_tuple_missing_paren_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Tuple Missing Parenthesis", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_paren_expression_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Parenthetical Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_paren_expression_missing_paren_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing Parenthesis", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed \'if\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_then_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing \'then\' in \'if\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_expr_after_then_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing \'then\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_else_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing \'else\' in \'if\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_expr_after_else_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing \'else\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_lambda_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Lambda Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_lambda_missing_arrow_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing Lambda Arrow", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_lambda_failed_to_parse_pattern_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Lambda Argument Pattern", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_do_bind_failed_to_parse_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed \'do\' Block", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_do_missing_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing Right Brace in \'do\' Block", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_list_missing_right_bracket_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("List Expression Missing Right Bracket", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_array_missing_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Array Expression Missing Right Bracket", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_pattern_empty_expression_list_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Empty List Expression???", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_arithmetic_sequence_failed_to_parse_then_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Arithmetic \'to\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_arithmetic_sequence_failed_to_parse_to_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Arithmetic \'from\' Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_arithmetic_sequence_missing_right_bracket_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Missing Bracket in Arithmetic Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_malformed_pattern_bind_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Pattern Bind", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_pattern_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Expected Pattern", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_arrow_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Expected Arrow", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_expression_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Expected Expression", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_of_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Expected \'of\'", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_left_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Expected Left Brace", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_empty_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Empty", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Case Alternative Expected Right Brace", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_fn_op_expected_accent_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Function Operator Expected Accent (\'`\')", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_data_expected_type_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Data Declaration Expected Type", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_data_expected_assign_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Data Declaration Expected Assignment (\'=\')", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_data_expected_data_con_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Data Declaration Expected Data Constructor", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_type_expected_type_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Malformed Function Type Signature", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_class_expected_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Type Class Expected Right Brace", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_instance_expected_right_brace_error(NecroResultError error, const char* source_str, const char* source_name)
{
    const char* explanation = "TODO: Explanation";
    necro_print_default_error_format("Class Instance Expected Right Brace", error.default_error_data.source_loc, error.default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_result_error(NecroResultError error, const char* source_str, const char* source_name)
{
    switch (error.type)
    {
    case NECRO_LEX_MALFORMED_FLOAT:                             necro_print_malformed_float_error(error, source_str, source_name); break;
    case NECRO_LEX_MALFORMED_STRING:                            necro_print_malformed_string_error(error, source_str, source_name); break;
    case NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE:             necro_print_unrecognized_character_sequence_error(error, source_str, source_name); break;
    case NECRO_LEX_MIXED_BRACES:                                necro_print_mixed_braces_error(error, source_str, source_name); break;

    case NECRO_PARSE_ERROR:                                     necro_print_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE:          necro_print_declarations_missing_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE:     necro_print_simple_assignment_rhs_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE:       necro_print_apat_assignment_rhs_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE:        necro_print_pat_assignment_rhs_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_RHS_FAILED_TO_PARSE:                       necro_print_rhs_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_RHS_EMPTY_WHERE:                           necro_print_rhs_empty_where_error(error, source_str, source_name); break;
    case NECRO_PARSE_LET_FAILED_TO_PARSE:                       necro_print_let_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_LET_EMPTY_IN:                              necro_print_let_empty_in_error(error, source_str, source_name); break;
    case NECRO_PARSE_LET_EXPECTED_SEMICOLON:                    necro_print_let_expected_semicolon_error(error, source_str, source_name); break;
    case NECRO_PARSE_LET_EXPECTED_LEFT_BRACE:                   necro_print_let_expected_left_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_LET_EXPECTED_RIGHT_BRACE:                  necro_print_let_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_LET_MISSING_IN:                            necro_print_let_missing_in_error(error, source_str, source_name); break;
    case NECRO_PARSE_TUPLE_MISSING_PAREN:                       necro_print_tuple_missing_paren_error(error, source_str, source_name); break;
    case NECRO_PARSE_PAREN_EXPRESSION_FAILED_TO_PARSE:          necro_print_paren_expression_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN:            necro_print_paren_expression_missing_paren_error(error, source_str, source_name); break;
    case NECRO_PARSE_IF_FAILED_TO_PARSE:                        necro_print_if_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_IF_MISSING_THEN:                           necro_print_if_missing_then_error(error, source_str, source_name); break;
    case NECRO_PARSE_IF_MISSING_EXPR_AFTER_THEN:                necro_print_if_missing_expr_after_then_error(error, source_str, source_name); break;
    case NECRO_PARSE_IF_MISSING_ELSE:                           necro_print_if_missing_else_error(error, source_str, source_name); break;
    case NECRO_PARSE_IF_MISSING_EXPR_AFTER_ELSE:                necro_print_if_missing_expr_after_else_error(error, source_str, source_name); break;
    case NECRO_PARSE_LAMBDA_FAILED_TO_PARSE:                    necro_print_lambda_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_LAMBDA_MISSING_ARROW:                      necro_print_lambda_missing_arrow_error(error, source_str, source_name); break;
    case NECRO_PARSE_LAMBDA_FAILED_TO_PARSE_PATTERN:            necro_print_lambda_failed_to_parse_pattern_error(error, source_str, source_name); break;
    case NECRO_PARSE_DO_BIND_FAILED_TO_PARSE:                   necro_print_do_bind_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_DO_MISSING_RIGHT_BRACE:                    necro_print_do_missing_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET:                necro_print_list_missing_right_bracket_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE:                 necro_print_array_missing_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_PATTERN_EMPTY_EXPRESSION_LIST:             necro_print_pattern_empty_expression_list_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN:  necro_print_arithmetic_sequence_failed_to_parse_then_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO:    necro_print_arithmetic_sequence_failed_to_parse_to_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET: necro_print_arithmetic_sequence_missing_right_bracket_error(error, source_str, source_name); break;
    case NECRO_PARSE_MALFORMED_PATTERN_BIND:                    necro_print_malformed_pattern_bind_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN:         necro_print_case_alternative_expected_pattern_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW:           necro_print_case_alternative_expected_arrow_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION:      necro_print_case_alternative_expected_expression_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF:              necro_print_case_alternative_expected_of_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE:      necro_print_case_alternative_expected_left_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EMPTY:                    necro_print_case_alternative_empty_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE:     necro_print_case_alternative_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_FN_OP_EXPECTED_ACCENT:                     necro_print_fn_op_expected_accent_error(error, source_str, source_name); break;
    case NECRO_PARSE_DATA_EXPECTED_TYPE:                        necro_print_data_expected_type_error(error, source_str, source_name); break;
    case NECRO_PARSE_DATA_EXPECTED_ASSIGN:                      necro_print_data_expected_assign_error(error, source_str, source_name); break;
    case NECRO_PARSE_DATA_EXPECTED_DATA_CON:                    necro_print_data_expected_data_con_error(error, source_str, source_name); break;
    case NECRO_PARSE_TYPE_EXPECTED_TYPE:                        necro_print_type_expected_type_error(error, source_str, source_name); break;
    case NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE:                necro_print_class_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE:             necro_print_instance_expected_right_brace_error(error, source_str, source_name); break;

    default:
        assert(false);
        break;
    }
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "result.h"
#include <string.h>
#include "ast_symbol.h"
#include "ast.h"
#include "type.h"
#include "type_class.h"
#include "infer.h"

NecroResultUnion global_result;

///////////////////////////////////////////////////////
// Error break: Place a break here to break on any error
///////////////////////////////////////////////////////
size_t necro_error_single_break_point()
{
    return 0;
}

///////////////////////////////////////////////////////
// Construction
///////////////////////////////////////////////////////
NecroResult(NecroParseAstLocalPtr) necro_parse_error_cons(NecroResultError* error1, NecroResultError* error2)
{
    necro_error_single_break_point();
    NecroResultError* error = emalloc(sizeof(NecroResultError));
    error->type             = NECRO_ERROR_CONS;
    error->error_cons       = (NecroErrorCons) { .error1 = error1, .error2 = error2 };
    return (NecroResult(NecroParseAstLocalPtr)) { .error = error, .type = NECRO_RESULT_ERROR };
}

static inline NecroResult(bool) necro_default_lex_error(NECRO_RESULT_ERROR_TYPE type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error   = emalloc(sizeof(NecroResultError));
    error->type               = type;
    error->default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc };
    return (NecroResult(bool)) { .error = error, .type = NECRO_RESULT_ERROR };
}

static inline NecroResult(NecroParseAstLocalPtr) necro_default_parse_error(NECRO_RESULT_ERROR_TYPE type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error   = emalloc(sizeof(NecroResultError));
    error->type               = type;
    error->default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc };
    return (NecroResult(NecroParseAstLocalPtr)) { .error = error, .type = NECRO_RESULT_ERROR };
}

static inline NecroResult(NecroAst) necro_default_ast_error(NECRO_RESULT_ERROR_TYPE type, NecroAstSymbol* ast_symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error       = emalloc(sizeof(NecroResultError));
    error->type                   = type;
    error->default_ast_error_data = (NecroDefaultAstErrorData)
    {
        .ast_symbol = ast_symbol,
        .source_loc = source_loc,
        .end_loc    = end_loc,
    };
    return (NecroResult(NecroAst)) { .error = error, .type = NECRO_RESULT_ERROR };
}

static inline NecroResult(NecroAst) necro_default_ast_error2(NECRO_RESULT_ERROR_TYPE type, NecroAstSymbol* ast_symbol1, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2)
{
    necro_error_single_break_point();
    NecroResultError* error         = emalloc(sizeof(NecroResultError));
    error->type                     = type;
    error->default_ast_error_data_2 = (NecroDefaultAstErrorData2)
    {
        .ast_symbol1 = ast_symbol1,
        .source_loc1 = source_loc1,
        .end_loc1    = end_loc1,
        .ast_symbol2 = ast_symbol2,
        .source_loc2 = source_loc2,
        .end_loc2    = end_loc2
    };
    return (NecroResult(NecroAst)) { .error = error, .type = NECRO_RESULT_ERROR };
}

NecroResult(bool) necro_malformed_string_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_lex_error(NECRO_LEX_MALFORMED_STRING, source_loc, end_loc);
}

NecroResult(bool) necro_malformed_float_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_lex_error(NECRO_LEX_MALFORMED_FLOAT, source_loc, end_loc);
}

NecroResult(void) necro_unrecognized_character_sequence_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(bool, void, necro_default_lex_error(NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE, source_loc, end_loc));
}

NecroResult(void) necro_mixed_braces_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(bool, void, necro_default_lex_error(NECRO_LEX_MIXED_BRACES, source_loc, end_loc));
}

NecroResult(void) necro_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error   = emalloc(sizeof(NecroResultError));
    error->type               = NECRO_PARSE_ERROR;
    error->default_error_data = (NecroDefaultErrorData) { .source_loc = source_loc, .end_loc = end_loc };
    return (NecroResult(void)) { .error = error, .type = NECRO_RESULT_ERROR };
}

NecroResult(NecroParseAstLocalPtr) necro_declarations_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_simple_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_apat_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_pat_assignment_rhs_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_rhs_empty_where_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_RHS_EMPTY_WHERE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_let_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_let_empty_in_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EMPTY_IN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_let_expected_semicolon_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EXPECTED_SEMICOLON, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_let_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_type_sig_expected_right_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_TYPE_SIG_EXPECTED_RIGHT_PAREN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_let_missing_in_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_MISSING_IN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_tuple_missing_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_TUPLE_MISSING_PAREN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_paren_expression_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PAREN_EXPRESSION_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_paren_expression_missing_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_if_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_if_missing_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_THEN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_if_missing_expr_after_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_EXPR_AFTER_THEN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_if_missing_else_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_ELSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_if_missing_expr_after_else_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_IF_MISSING_EXPR_AFTER_ELSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_lambda_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LAMBDA_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_lambda_missing_arrow_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LAMBDA_MISSING_ARROW, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_lambda_failed_to_parse_pattern_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LAMBDA_FAILED_TO_PARSE_PATTERN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_do_bind_failed_to_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DO_BIND_FAILED_TO_PARSE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_do_let_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DO_LET_EXPECTED_LEFT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_do_let_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DO_LET_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_do_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DO_MISSING_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_list_missing_right_bracket_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_array_missing_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_pattern_empty_expression_list_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_PATTERN_EMPTY_EXPRESSION_LIST, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_arithmetic_sequence_failed_to_parse_then_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_arithmetic_sequence_failed_to_parse_to_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_arithmetic_sequence_missing_right_bracket_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_let_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_LET_EXPECTED_LEFT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_pattern_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_malformed_for_loop_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_MALFORMED_FOR_LOOP, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_arrow_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_expression_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_of_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_left_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_empty_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EMPTY, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_case_alternative_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_fn_op_expected_accent_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_FN_OP_EXPECTED_ACCENT, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_data_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DATA_EXPECTED_TYPE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_data_expected_assign_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DATA_EXPECTED_ASSIGN, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_data_expected_data_con_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_DATA_EXPECTED_DATA_CON, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_type_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_TYPE_EXPECTED_TYPE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_type_list_expected_right_bracket(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_TYPE_LIST_EXPECTED_RIGHT_BRACKET, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_class_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_instance_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE, source_loc, end_loc);
}

NecroResult(NecroParseAstLocalPtr) necro_const_con_missing_right_brace(NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_parse_error(NECRO_PARSE_CONST_CON_MISSING_RIGHT_PAREN, source_loc, end_loc);
}

NecroResult(NecroAstSymbol) necro_multiple_definitions_error(NecroAstSymbol* ast_symbol1, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2)
{
    return necro_error_map(NecroAst, NecroAstSymbol, necro_default_ast_error2(NECRO_RENAME_MULTIPLE_DEFINITIONS, ast_symbol1, source_loc1, end_loc1, ast_symbol2, source_loc2, end_loc2));
}

NecroResult(NecroAstSymbol) necro_duplicate_type_signatures_error(NecroAstSymbol* ast_symbol1, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2)
{
    return necro_error_map(NecroAst, NecroAstSymbol, necro_default_ast_error2(NECRO_RENAME_MULTIPLE_TYPE_SIGNATURES, ast_symbol1, source_loc1, end_loc1, ast_symbol2, source_loc2, end_loc2));
}

NecroResult(NecroAstSymbol) necro_not_in_scope_error(NecroAstSymbol* ast_symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(NecroAst, NecroAstSymbol, necro_default_ast_error(NECRO_RENAME_NOT_IN_SCOPE, ast_symbol, source_loc, end_loc));
}

static inline NecroResult(NecroType) necro_default_type_error1(NECRO_RESULT_ERROR_TYPE error_type, NecroAstSymbol* ast_symbol, const NecroType* type, const NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error         = emalloc(sizeof(NecroResultError));
    error->type                     = error_type;
    error->default_type_error_data1 = (NecroDefaultTypeErrorData1)
    {
        .ast_symbol = ast_symbol,
        .type       = type,
        .macro_type = macro_type,
        .source_loc = source_loc,
        .end_loc    = end_loc,
    };
    return (NecroResult(NecroType)) { .error = error, .type = NECRO_RESULT_ERROR };
}

static inline NecroResult(NecroType) necro_default_type_error2(NECRO_RESULT_ERROR_TYPE error_type, NecroType* type1, NecroType* type2, NecroType* macro_type1, NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error         = emalloc(sizeof(NecroResultError));
    error->type                     = error_type;
    error->default_type_error_data2 = (NecroDefaultTypeErrorData2)
    {
        .type1       = type1,
        .type2       = type2,
        .macro_type1 = macro_type1,
        .macro_type2 = macro_type2,
        .source_loc  = source_loc,
        .end_loc     = end_loc,
    };
    return (NecroResult(NecroType)) { .error = error, .type = NECRO_RESULT_ERROR };
}

static inline NecroResult(NecroType) necro_default_type_class_error(NECRO_RESULT_ERROR_TYPE error_type, NecroAstSymbol* type_class_symbol, NecroType* type1, NecroType* type2, NecroType* macro_type1, NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_error_single_break_point();
    NecroResultError* error              = emalloc(sizeof(NecroResultError));
    error->type                          = error_type;
    error->default_type_class_error_data = (NecroDefaultTypeClassErrorData)
    {
        .type_class_symbol = type_class_symbol,
        .type1             = type1,
        .type2             = type2,
        .macro_type1       = macro_type1,
        .macro_type2       = macro_type2,
        .source_loc        = source_loc,
        .end_loc           = end_loc,
    };
    return (NecroResult(NecroType)) { .error = error, .type = NECRO_RESULT_ERROR };
}


///////////////////////////////////////////////////////
// NecroDefaultTypeErrorData1
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_type_uninitialized_recursive_value_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error1(NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE, ast_symbol, type, type, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_recursive_function_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error1(NECRO_TYPE_RECURSIVE_FUNCTION, ast_symbol, type, type, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_recursive_data_type_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error1(NECRO_TYPE_RECURSIVE_DATA_TYPE, ast_symbol, type, type, source_loc, end_loc);
}

NecroResult(void) necro_type_non_recursive_initialized_value_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(NecroType, void, necro_default_type_error1(NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE, ast_symbol, type, NULL, source_loc, end_loc));
}

NecroResult(NecroType) necro_type_polymorphic_pat_bind_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error1(NECRO_TYPE_POLYMORPHIC_PAT_BIND, ast_symbol, type, NULL, source_loc, end_loc);
}

NecroResult(void) necro_type_non_concrete_initialized_value_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(NecroType, void, necro_default_type_error1(NECRO_TYPE_NON_CONCRETE_INITIALIZED_VALUE, ast_symbol, type, NULL, source_loc, end_loc));
}

NecroResult(NecroType) necro_type_final_do_statement_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error1(NECRO_TYPE_FINAL_DO_STATEMENT, ast_symbol, type, NULL, source_loc, end_loc);
}

NecroResult(bool) necro_type_ambiguous_type_var_error(NecroAstSymbol* ast_symbol, const NecroType* type, const NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(NecroType, bool, necro_default_type_error1(NECRO_TYPE_AMBIGUOUS_TYPE_VAR, ast_symbol, type, macro_type, source_loc, end_loc));
}

NecroResult(NecroConstraintList) necro_type_not_a_class_error(NecroAstSymbol* ast_symbol, NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_error_map(NecroType, NecroConstraintList, necro_default_type_error1(NECRO_TYPE_NOT_A_CLASS, ast_symbol, type, NULL, source_loc, end_loc));
}

NecroResult(NecroType) necro_type_malformed_uniqueness_attribute_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error1(NECRO_TYPE_MALFORMED_UNIQUENESS_ATTRIBUTE, ast_symbol, type, NULL, source_loc, end_loc);
}

NecroResult(NecroConstraintList) necro_type_malformed_constraint(struct NecroConstraint* constraint)
{
    necro_error_single_break_point();
    NecroResultError* error     = emalloc(sizeof(NecroResultError));
    error->type                 = NECRO_CONSTRAINT_MALFORMED_CONSTRAINT;
    error->contraint_error_data = (NecroConstraintError)
    {
        .constraint = constraint
    };
    return (NecroResult(NecroConstraintList)) { .error = error, .type = NECRO_RESULT_ERROR };
}

///////////////////////////////////////////////////////
// NecroDefaultTypeErrorData2
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_type_rigid_type_variable_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_TYPE_RIGID_TYPE_VARIABLE, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_occurs_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_TYPE_OCCURS, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_occurs_error_partial(struct NecroType* type1, struct NecroType* type2)
{
    return necro_default_type_error2(NECRO_TYPE_OCCURS, type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
}

NecroResult(NecroType) necro_type_mismatched_type_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_TYPE_MISMATCHED_TYPE, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_mismatched_type_error_partial(struct NecroType* type1, struct NecroType* type2)
{
    return necro_default_type_error2(NECRO_TYPE_MISMATCHED_TYPE, type1, type2, NULL, NULL, NULL_LOC, NULL_LOC);
}

NecroResult(NecroType) necro_type_mismatched_arity_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_TYPE_MISMATCHED_ARITY, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_kind_mismatched_kind_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_KIND_MISMATCHED_KIND, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_kind_mismatched_arity_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_KIND_MISMATCHED_ARITY, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_kind_rigid_kind_variable_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_error2(NECRO_KIND_RIGID_KIND_VARIABLE, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

///////////////////////////////////////////////////////
// NecroDefaultTypeClassErrorData
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_type_ambiguous_class_error(NecroAstSymbol* type_class_symbol, NecroType* type1, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_class_error(NECRO_TYPE_AMBIGUOUS_CLASS, type_class_symbol, type1, NULL, type1, NULL, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_constrains_only_class_var_error(NecroAstSymbol* ast_symbol1, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2)
{
    return necro_error_map(NecroAst, NecroType, necro_default_ast_error2(NECRO_TYPE_CONSTRAINS_ONLY_CLASS_VAR, ast_symbol1, source_loc1, end_loc1, ast_symbol2, source_loc2, end_loc2));
}

NecroResult(NecroType) necro_type_not_an_instance_of_error_partial(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type)
{
    return necro_default_type_class_error(NECRO_TYPE_NOT_AN_INSTANCE_OF, type_class_ast_symbol, type, NULL, NULL, NULL, NULL_LOC, NULL_LOC);
}

NecroResult(NecroType) necro_type_not_an_instance_of_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)

{
    return necro_default_type_class_error(NECRO_TYPE_NOT_AN_INSTANCE_OF, type_class_ast_symbol, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_multiple_class_declarations_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_class_error(NECRO_TYPE_MULTIPLE_CLASS_DECLARATIONS, type_class_ast_symbol, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_multiple_instance_declarations_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_class_error(NECRO_TYPE_MULTIPLE_INSTANCE_DECLARATIONS, type_class_ast_symbol, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_not_a_visible_method_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_class_error(NECRO_TYPE_NOT_A_VISIBLE_METHOD, type_class_ast_symbol, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_no_explicit_implementation_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_class_error(NECRO_TYPE_NO_EXPLICIT_IMPLEMENTATION, type_class_ast_symbol, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

NecroResult(NecroType) necro_type_does_not_implement_super_class_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    return necro_default_type_class_error(NECRO_TYPE_DOES_NOT_IMPLEMENT_SUPER_CLASS, type_class_ast_symbol, type1, type2, macro_type1, macro_type2, source_loc, end_loc);
}

///////////////////////////////////////////////////////
// Audio Error
///////////////////////////////////////////////////////
NecroResult(void) necro_runtime_audio_error(const char* error_message)
{
    necro_error_single_break_point();
    NecroResultError* error         = emalloc(sizeof(NecroResultError));
    error->type                     = NECRO_RUNTIME_AUDIO_ERROR;
    error->runtime_audio_error_data = (NecroRuntimeAudioErrorData)
    {
        .error_message = error_message
    };
    return (NecroResult(void)) { .error = error, .type = NECRO_RESULT_ERROR };
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

#define NECRO_ERR_LEFT_CHAR "*"

void necro_print_blank_error_gutter(NecroSourceLoc source_loc)
{
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " ");
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
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %zu | " , source_loc.line);
}

// TODO: Take into account newlines!
void necro_print_range_pointers(const char* source_str, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    necro_print_blank_error_gutter(source_loc);
    for (size_t i = 0; i < source_loc.character; ++i)
    {
        fprintf(stderr, " ");
    }
    fprintf(stderr, "^");
    if (source_loc.line == end_loc.line)
    {
        for (size_t i = source_loc.character + 1; i < end_loc.character; ++i)
            fprintf(stderr, "^");
    }
    else
    {
        for (size_t pos = source_loc.pos; source_str[pos] != '\n' && source_str[pos] != '\0'; ++pos)
            fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");
}

void necro_print_line_at_source_loc(const char* source_str, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (source_loc.pos == NULL_LOC.pos || end_loc.pos == NULL_LOC.pos)
        return;
    size_t line_start = source_loc.pos;
    for (line_start = source_loc.pos; line_start > 0 && source_str[line_start] != '\0' && source_str[line_start] != '\n'; --line_start);
    if (source_str[line_start] == '\n')
        line_start++;
    size_t line_end   = line_start;
    for (line_end = line_start; source_str[line_end] != '\0' && source_str[line_end] != '\n'; ++line_end);
    necro_print_error_gutter(source_loc);
    fprintf(stderr, "%.*s\n", (int) (line_end - line_start), (source_str + line_start));
    necro_print_range_pointers(source_str, source_loc, end_loc);
}

void necro_print_error_header(const char* error_name)
{
    // // fprintf(stderr, "error[NE-%04u]: Malformed float\n", NECRO_MALFORMED_FLOAT);
    fprintf(stderr, "\n----- %s ----- ", error_name);
    fprintf(stderr, "\n" NECRO_ERR_LEFT_CHAR " \n");
    // necro_print_line_at_source_loc(source_str, source_loc, end_loc);
}

void necro_print_default_error_format(const char* error_name, NecroSourceLoc source_loc, NecroSourceLoc end_loc, const char* source_str, const char* source_name, const char* explanation)
{
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %s\n", explanation);
    fprintf(stderr, "\n");
    // fprintf(stderr, "%s:%zu:%zu\n", file_base_name(source_name), source_loc.line, source_loc.character);
    UNUSED(source_name);
}

void necro_print_default_ast_error_2_format(
    const char* error_name,
    NecroAstSymbol* ast_symbol1,
    NecroSourceLoc source_loc1,
    NecroSourceLoc end_loc1,
    NecroAstSymbol* ast_symbol2,
    NecroSourceLoc source_loc2,
    NecroSourceLoc end_loc2,
    const char* source_str,
    const char* source_name,
    const char* explanation1,
    const char* explanation2)
{
    necro_print_error_header(error_name);
    if (source_loc1.pos != INVALID_LINE)
    {
        necro_print_line_at_source_loc(source_str, source_loc1, end_loc1);
    }
    else if (ast_symbol1->source_name != NULL && ast_symbol1->source_name->str != NULL)
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " (Necro.Base) | %s\n", ast_symbol1->source_name->str);
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " \n");
    }
    else
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " (Necro.Base) | (a primitive type or value found in Necro.Base)\n");
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " \n");
    }
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %s\n", explanation1);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " \n");
    if (source_loc2.pos != INVALID_LINE)
    {
        necro_print_line_at_source_loc(source_str, source_loc2, end_loc2);
    }
    else if (ast_symbol2->source_name != NULL && ast_symbol2->source_name->str != NULL)
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " (Necro.Base) | %s (Necro.Base)\n", ast_symbol2->source_name->str);
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " \n");
    }
    else
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " (Necro.Base) | (a primitive type or value found in Necro.Base)\n");
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " \n");
    }
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %s\n", explanation2);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}


void necro_print_error_cons(NecroResultError* error, const char* source_str, const char* source_name)
{
    if (error->error_cons.error1 != NULL)
        necro_result_error_print(error->error_cons.error1, source_str, source_name);
    if (error->error_cons.error2 != NULL)
        necro_result_error_print(error->error_cons.error2, source_str, source_name);
}

void necro_print_malformed_float_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Float literals require digits both before and after the period (.) character.";
    necro_print_default_error_format("Malformed Float", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_malformed_string_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Perhaps you forgot a closing quotation character (\") ?";
    necro_print_default_error_format("Malformed String", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_unrecognized_character_sequence_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "This is either an unholy message to Cthulhu or perhaps you mistyped some things...";
    necro_print_default_error_format("Unrecognized Character Sequence", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_mixed_braces_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Necrolang uses significant whitespace which converts into implicit opening and closing braces.\nYou're likely adding an explicit closing brace where none is required.";
    necro_print_default_error_format("Mixed Braces", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "The Parser could not make sense of this line.";
    necro_print_default_error_format("Parse Error", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_declarations_missing_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "It's likely your indentation is off, or an erroneous character has been inserted.";
    necro_print_default_error_format("Malformed Declaration", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_simple_assignment_rhs_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Assignments should take the form: var = expr";
    necro_print_default_error_format("Malformed Assignment", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_apat_assignment_rhs_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Functions should take the form: fnName arg = expr";
    necro_print_default_error_format("Malformed Function", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_pat_assignment_rhs_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Pattern assignments should take the form: (var1, var2) = expr";
    necro_print_default_error_format("Malformed Pattern Assignment", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_rhs_empty_where_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Where declarations should take the form: where var = expr";
    necro_print_default_error_format("Malformed \'where\' Declaration", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'let\' expressions should take the form: let var = expr1 in expr2";
    necro_print_default_error_format("Malformed \'let\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_empty_in_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'let\' expressions should take the form: let var = expr1 in expr2";
    necro_print_default_error_format("Malformed \'let\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_expected_semicolon_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'let\' expressions should take the form: let var = expr1 in expr2";
    necro_print_default_error_format("Malformed \'let\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_expected_left_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'let\' expressions should take the form: let var = expr1 in expr2";
    necro_print_default_error_format("Malformed \'let\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_expected_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'let\' expressions should take the form: let var = expr1 in expr2";
    necro_print_default_error_format("Malformed \'let\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_type_sig_expected_right_paren_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Type variable signature should take the form: (tyvar :: Kind)";
    necro_print_default_error_format("Malformed Type Var Sig", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_let_missing_in_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'let\' expressions should take the form: let var = expr1 in expr2";
    necro_print_default_error_format("Expected \'in\' After \'let\'", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_tuple_missing_paren_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Tuples should take the form: (expr1, expr2)";
    necro_print_default_error_format("Malformed Tuple", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_paren_expression_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Parenthetical expressions should take the form: (expr1 expr2)";
    necro_print_default_error_format("Malformed Parenthetical Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_paren_expression_missing_paren_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Parenthetical expressions should take the form: (expr1 expr2)";
    necro_print_default_error_format("Malformed Parenthetical Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'if\' expressions should take the form: if cond then expr1 else expr2";
    necro_print_default_error_format("Malformed \'if\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_then_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected \'then\' in \'if\' expression. \'if\' expressions should take the form: if cond then expr1 else expr2";
    necro_print_default_error_format("Malformed \'if\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_expr_after_then_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected expression after \'then\'. \'if\' expressions should take the form: if cond then expr1 else expr2";
    necro_print_default_error_format("Malformed \'if\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_else_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected \'else\' in \'if\' expression. \'if\' expressions should take the form: if cond then expr1 else expr2";
    necro_print_default_error_format("Malformed \'if\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_if_missing_expr_after_else_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected expression after \'else\'. \'if\' expressions should take the form: if cond then expr1 else expr2";
    necro_print_default_error_format("Malformed \'if\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_lambda_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Lambdas should take the form: \\var -> expr";
    necro_print_default_error_format("Malformed Lambda Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_lambda_missing_arrow_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected \'->\'. Lambdas should take the form: \\var -> expr";
    necro_print_default_error_format("Malformed Lambda Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_lambda_failed_to_parse_pattern_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected arguments after \'\\\'. Lambdas should take the form: \\var -> expr";
    necro_print_default_error_format("Malformed Lambda Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_do_bind_failed_to_parse_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'do\' bindings should take the form: var <- expr";
    necro_print_default_error_format("Malformed \'do\' Bind", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_do_let_expected_left_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "It's likely your indentation is off, or an erroneous character has been inserted.";
    necro_print_default_error_format("Malformed \'let\' in \'do\' Block", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_do_let_expected_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "It's likely your indentation is off, or an erroneous character has been inserted.";
    necro_print_default_error_format("Malformed \'let\' in \'do\' Block", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_do_missing_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "It's likely your indentation is off, or an erroneous character has been inserted.";
    necro_print_default_error_format("Malformed \'do\' Block", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_list_missing_right_bracket_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Lists should take the form: [expr1, expr2, expr3]";
    necro_print_default_error_format("Malformed List", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_array_missing_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Arrays should take the form: {expr1, expr2, expr2}";
    necro_print_default_error_format("Malformed Array Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_pattern_empty_expression_list_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "\'pat\' expressions should take the form: pat arg1 arg2 arg3";
    necro_print_default_error_format("Malformed \'pat\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_arithmetic_sequence_failed_to_parse_then_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Arithmetic sequences should take the form: [fromExpr..toExpr], or [fromExpr, thenExpr..toExpr]";
    necro_print_default_error_format("Malformed Arithmetic \'then\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_arithmetic_sequence_failed_to_parse_to_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Arithmetic sequences should take the form: [fromExpr..toExpr], or [fromExpr, thenExpr..toExpr]";
    necro_print_default_error_format("Malformed Arithmetic \'to\' Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_arithmetic_sequence_missing_right_bracket_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Arithmetic sequences should take the form: [fromExpr..toExpr], or [fromExpr, thenExpr..toExpr]";
    necro_print_default_error_format("Malformed Arithmetic Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_pattern_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected a pattern here. Case alternatives take the form: Pat -> expr";
    necro_print_default_error_format("Malformed Case Alternative", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_arrow_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected \'->\' here. Case alternatives take the form: Pat -> expr";
    necro_print_default_error_format("Malformed Case Alternative", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_expression_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected expression here. Case alternatives take the form: Pat -> expr";
    necro_print_default_error_format("Malformed Case Alternative", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_of_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected \'of\' here. Case expressions take the form: case expr of { Pat -> expr; }";
    necro_print_default_error_format("Malformed Case Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_left_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Likely there is an indentation error or mistaken character. Case expressions take the form: case expr of { Pat -> expr; }";
    necro_print_default_error_format("Malformed Case Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_empty_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "No case alternatives. Case expressions take the form: case expr of { Pat -> expr; }";
    necro_print_default_error_format("Malformed Case Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_case_alternative_expected_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Likely there is an indentation error or mistaken character. Case expressions take the form: case expr of { Pat -> expr; }";
    necro_print_default_error_format("Malformed Case Expression", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_malformed_for_loop_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "For Loop expressions take the form: loop valueInit = value for index <- rangeInit do expr";
    necro_print_default_error_format("Malformed For Loop", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_fn_op_expected_accent_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Constructor Operators take the form: expr1 `Con` expr2";
    necro_print_default_error_format("Malformed Constructor Operator Pattern", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_data_expected_type_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected Type Name here. Data declarations take the form: data Type = Con1 Type2 | Con2";
    necro_print_default_error_format("Malformed Data Declaration", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_data_expected_assign_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected \'=\' here. Data declarations take the form: data Type = Con1 Type2 | Con2";
    necro_print_default_error_format("Malformed Data Declaration", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_data_expected_data_con_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected Data Constructor here. Data declarations take the form: data Type = Con1 Type2 | Con2";
    necro_print_default_error_format("Malformed Data Declaration", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_type_expected_type_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected Type after \'->\'. Function type signatures take the form: Type1 -> Type2";
    necro_print_default_error_format("Malformed Type Signature", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_type_list_expected_right_bracket_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "List types should take the form: [Type]";
    necro_print_default_error_format("Malformed List Type", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_class_expected_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Likely there is an indentation error or a mistaken character.";
    necro_print_default_error_format("Malformed Type Class", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_instance_expected_right_brace_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Likely there is an indentation error or a mistaken character.";
    necro_print_default_error_format("Malformed Class Instance", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_const_con_missing_right_paren(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Expected ')' here. Initial values should take the form: var ~ (InitCon1 InitCon2) = expr";
    necro_print_default_error_format("Malformed Initial Value", error->default_error_data.source_loc, error->default_error_data.end_loc, source_str, source_name, explanation);
}

void necro_print_multiple_definitions_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    necro_print_default_ast_error_2_format(
        "Multiple Declarations",
        error->default_ast_error_data_2.ast_symbol2,
        error->default_ast_error_data_2.source_loc2,
        error->default_ast_error_data_2.end_loc2,
        error->default_ast_error_data_2.ast_symbol1,
        error->default_ast_error_data_2.source_loc1,
        error->default_ast_error_data_2.end_loc1,
        source_str,
        source_name,
        "First declaration here.",
        "Second declaration here.");
}

void necro_print_duplicate_type_signatures_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    necro_print_default_ast_error_2_format(
        "Multiple Type Signatures",
        error->default_ast_error_data_2.ast_symbol2,
        error->default_ast_error_data_2.source_loc2,
        error->default_ast_error_data_2.end_loc2,
        error->default_ast_error_data_2.ast_symbol1,
        error->default_ast_error_data_2.source_loc1,
        error->default_ast_error_data_2.end_loc1,
        source_str,
        source_name,
        "First type signature found here.",
        "Second type signature found here.");
}

void necro_print_not_in_scope_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    necro_print_error_header("Name Not In Scope");
    necro_print_line_at_source_loc(source_str, error->default_ast_error_data.source_loc, error->default_ast_error_data.end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Could not find an identifier with this name in scope:\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR "     %s\n", error->default_ast_error_data.ast_symbol->source_name->str);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_uninitialized_recursive_value_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "All recursive values must be given a static initial value of the form: recursiveValue ~ StaticInitialValue = expr";
    necro_print_default_error_format("Uninitialized Recursive Value", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_recursive_function_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation =
        "Functions cannot be recursive.\n"
        NECRO_ERR_LEFT_CHAR " Consider using a recursive value for a similar effect";
    necro_print_default_error_format("Recursive Function", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_recursive_data_type_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation =
        "Data types cannot be recursive.\n"
        NECRO_ERR_LEFT_CHAR " Consider using a recursive value for a similar effect";
    necro_print_default_error_format("Recursive Data Type", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_non_concrete_initialized_value_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          explanation = "Recursive values must not contain functions or potentially higher order type variables.";
    const NecroSourceLoc source_loc  = error->default_type_error_data1.source_loc;
    const NecroSourceLoc end_loc     = error->default_type_error_data1.end_loc;
    necro_print_error_header("Higher Order Recursive Value");
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %s\n", explanation);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Found Type: ");
    necro_type_fprint(stderr, error->default_type_error_data1.type);
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_non_recursive_initialized_value_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "Non-recursive values cannot not have an initializer since it would have no effect";
    necro_print_default_error_format("Initialized Non-Recursive Value", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_polymorphic_pat_bind_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "All pattern bindings must be monomorphic";
    necro_print_default_error_format("Polymorphic pattern binding", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_final_do_statement_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "The final statement of a do block must produce a value.";
    necro_print_default_error_format("Malformed Do Block", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_type_mismatch(const NecroType* type1, const NecroType* type2, const NecroType* macro_type1, const NecroType* macro_type2)
{
    if (type1 == macro_type1 &&
        type2 == macro_type2)
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " Expected: ");
        necro_type_fprint(stderr, type1);
        fprintf(stderr, "\n");

        fprintf(stderr, NECRO_ERR_LEFT_CHAR " Found:    ");
        necro_type_fprint(stderr, type2);
        fprintf(stderr, "\n");
    }
    else
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " Couldn't match ");
        necro_type_fprint(stderr, type1);
        fprintf(stderr, " with ");
        necro_type_fprint(stderr, type2);
        fprintf(stderr, "\n");
        fprintf(stderr, NECRO_ERR_LEFT_CHAR "\n");

        fprintf(stderr, NECRO_ERR_LEFT_CHAR " Expected: ");
        necro_type_fprint(stderr, macro_type1);
        fprintf(stderr, "\n");

        fprintf(stderr, NECRO_ERR_LEFT_CHAR " Found:    ");
        necro_type_fprint(stderr, macro_type2);
        fprintf(stderr, "\n");
    }

}

void necro_print_mismatched_type_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name  = "Mismatched Types";
    const NecroSourceLoc source_loc  = error->default_type_error_data2.source_loc;
    const NecroSourceLoc end_loc     = error->default_type_error_data2.end_loc;
    const NecroType*     type1       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type1));
    const NecroType*     type2       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type2));
    const NecroType*     macro_type1 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type1));
    const NecroType*     macro_type2 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type2));
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    necro_print_type_mismatch(type1, type2, macro_type1, macro_type2);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_rigid_type_variable_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name  = "Rigid Type Variable";
    const NecroSourceLoc source_loc  = error->default_type_error_data2.source_loc;
    const NecroSourceLoc end_loc     = error->default_type_error_data2.end_loc;
    const NecroType*     type1       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type1));
    const NecroType*     type2       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type2));
    const NecroType*     macro_type1 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type1));
    const NecroType*     macro_type2 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type2));
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    necro_print_type_mismatch(type1, type2, macro_type1, macro_type2);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR "\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " '");
    if (type1->type == NECRO_TYPE_VAR && type1->var.is_rigid)
        necro_type_fprint(stderr, type1);
    else
        necro_type_fprint(stderr, type2);
    fprintf(stderr, "' is a rigid type variable provided by a type signature, and cannot be unified with another type!\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_malformed_uniqueness_attribute_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char* explanation = "In Data Constructors only anonymous dots may be applied to types, and they may only be applied to Type Constructors and Type Applications.";
    necro_print_default_error_format("Malformed Uniqueness Attribute", error->default_type_error_data1.source_loc, error->default_type_error_data1.end_loc, source_str, source_name, explanation);
}

void necro_print_mismatched_kind_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name  = "Mismatched Kinds";
    const NecroSourceLoc source_loc  = error->default_type_error_data2.source_loc;
    const NecroSourceLoc end_loc     = error->default_type_error_data2.end_loc;
    const NecroType*     type1       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type1));
    const NecroType*     type2       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type2));
    const NecroType*     macro_type1 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type1));
    const NecroType*     macro_type2 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type2));

    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    necro_print_type_mismatch(type1, type2, macro_type1, macro_type2);

    fprintf(stderr, NECRO_ERR_LEFT_CHAR "\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " 'Kinds' are the 'types' of types\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Perhaps you applied too few or too many type parameters to a type\n");

    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_occurs_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name  = "Infinite Type";
    const NecroSourceLoc source_loc  = error->default_type_error_data2.source_loc;
    const NecroSourceLoc end_loc     = error->default_type_error_data2.end_loc;
    const NecroType*     type1       = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.type1));
    const NecroType*     macro_type2 = necro_type_strip_for_all(necro_type_find(error->default_type_error_data2.macro_type2));

    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);

    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Attempting to assert that: ");
    necro_type_fprint(stderr, type1);
    fprintf(stderr, "\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " is equivalant to:          ");
    necro_type_fprint(stderr, macro_type2);
    fprintf(stderr, "\n");

    fprintf(stderr, NECRO_ERR_LEFT_CHAR "\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " This would create an infinite type!\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " A type becomes infinite when it contains itself.\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_not_a_class_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name = "Not A Type Class";
    const NecroSourceLoc source_loc = error->default_type_error_data1.source_loc;
    const NecroSourceLoc end_loc    = error->default_type_error_data1.end_loc;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Attempting to use %s as a type class, but it is not a type class.\n", error->default_type_error_data1.ast_symbol->source_name->str);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_constraint_malformed_constraint_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name = "Malformed Constraint";
    const NecroSourceLoc source_loc = error->contraint_error_data.constraint->source_loc;
    const NecroSourceLoc end_loc    = error->contraint_error_data.constraint->end_loc;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Constraints should be of the form:\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR "     - Num a\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR "  or - Num (f a)\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_not_a_visible_member_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name = "Not A Visible Method";
    const NecroSourceLoc source_loc = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc end_loc    = error->default_type_class_error_data.end_loc;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %s is not a visible method of the %s type class.\n", error->default_type_class_error_data.type_class_symbol->source_name->str, error->default_type_class_error_data.type2->con.con_symbol->source_name->str);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_no_explicit_implementation_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name = "No Explicit Implementation";
    const NecroSourceLoc source_loc = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc end_loc    = error->default_type_class_error_data.end_loc;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " No explicit implementation for method '%s'\n", error->default_type_class_error_data.type_class_symbol->source_name->str);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " In '%s' instance of Type Class '%s'\n", error->default_type_class_error_data.type1->con.con_symbol->source_name->str, error->default_type_class_error_data.type2->con.con_symbol->source_name->str);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_ambiguous_class_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name = "Ambiguous Type Class";
    const NecroSourceLoc source_loc = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc end_loc    = error->default_type_class_error_data.end_loc;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Could not deduce '(%s ", error->default_type_class_error_data.type_class_symbol->source_name->str);
    necro_print_type_sig_go_maybe_with_parens(stderr, error->default_type_class_error_data.type1);
    fprintf(stderr, ") from the context of this type signature\n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Perhaps you forgot to use the class constrained type variable in the type signature?\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);

}

void necro_print_type_constrains_only_class_var_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name = "Constrains Only Class Variable";
    const NecroSourceLoc source_loc = error->default_ast_error_data_2.source_loc1;
    const NecroSourceLoc end_loc    = error->default_ast_error_data_2.end_loc1;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Context (%s %s) constrains only the class variable %s\n", error->default_ast_error_data_2.ast_symbol1->source_name->str, error->default_ast_error_data_2.ast_symbol2->source_name->str, error->default_ast_error_data_2.ast_symbol2->source_name->str);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " In other words, this context is already asserted by the class declaration, making this redundant.\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_is_not_an_instance_of_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*          error_name  = "Not An Instance";
    const NecroSourceLoc source_loc  = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc end_loc     = error->default_type_class_error_data.end_loc;
    const NecroType*     type1       = necro_type_strip_for_all(necro_type_find(error->default_type_class_error_data.type1));
    const NecroType*     macro_type1 = necro_type_strip_for_all(necro_type_find(error->default_type_class_error_data.macro_type2));

    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc, end_loc);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " '");
    necro_type_fprint(stderr, type1);
    fprintf(stderr, "' is not an instance of class %s\n", error->default_type_class_error_data.type_class_symbol->source_name->str);
    if (type1 != macro_type1)
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " Found in Type: ");
        necro_type_fprint(stderr, macro_type1);
    }
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_multiple_class_declarations_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*           error_name   = "Multiple Class Declarations";
    const NecroSourceLoc  source_loc1  = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc  end_loc1     = error->default_type_class_error_data.end_loc;
    const NecroSourceLoc  source_loc2  = error->default_type_class_error_data.type_class_symbol->ast->source_loc;
    const NecroSourceLoc  end_loc2     = error->default_type_class_error_data.type_class_symbol->ast->end_loc;
    const NecroAstSymbol* class_symbol = error->default_type_class_error_data.type_class_symbol;

    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc1, end_loc1);

    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Multiple declarations for the Class '%s'\n", class_symbol->source_name->str);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Original declaration found here:\n");

    if (source_loc2.pos != INVALID_LINE)
    {
        necro_print_line_at_source_loc(source_str, source_loc2, end_loc2);
    }
    else
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " (Necro.Base) | %s\n", class_symbol->source_name->str);
    }


    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_multiple_instance_declarations_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*           error_name   = "Multiple Instance Declarations";
    const NecroSourceLoc  source_loc1  = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc  end_loc1     = error->default_type_class_error_data.end_loc;
    const NecroAstSymbol* class_symbol = error->default_type_class_error_data.type_class_symbol;
    const NecroAstSymbol* data_symbol  = necro_type_find(error->default_type_class_error_data.type1)->con.con_symbol;
    const NecroSourceLoc  source_loc2  = class_symbol->ast->source_loc;
    const NecroSourceLoc  end_loc2     = class_symbol->ast->end_loc;

    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc1, end_loc1);

    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Multiple Instance declarations for '%s %s'\n", class_symbol->source_name->str, data_symbol->source_name->str);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Original declaration found at: \n");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR "\n");

    if (source_loc2.pos != INVALID_LINE)
    {
        necro_print_line_at_source_loc(source_str, source_loc2, end_loc2);
    }
    else
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " (Necro.Base) | instance %s %s where\n", class_symbol->source_name->str, data_symbol->source_name->str);
    }

    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_type_does_not_implment_super_class_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*           error_name   = "Type Does Not Implement Super Class";
    const NecroAstSymbol* data_symbol  = necro_type_find(error->default_type_class_error_data.type1)->con.con_symbol;
    const NecroAstSymbol* class_symbol = necro_type_find(error->default_type_class_error_data.type2)->con.con_symbol;
    const NecroAstSymbol* super_symbol = error->default_type_class_error_data.type_class_symbol;
    const NecroSourceLoc  source_loc1  = error->default_type_class_error_data.source_loc;
    const NecroSourceLoc  end_loc1     = error->default_type_class_error_data.end_loc;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc1, end_loc1);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " '%s' should implement class '%s', but does not.\n", data_symbol->source_name->str, super_symbol->source_name->str);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " This is required because '%s' is a super class of '%s'\n", super_symbol->source_name->str, class_symbol->source_name->str);
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_ambiguous_type_var(NecroResultError* error, const char* source_str, const char* source_name)
{
    const char*           error_name   = "Ambiguous Type";
    const NecroSourceLoc  source_loc1  = error->default_type_error_data1.source_loc;
    const NecroSourceLoc  end_loc1     = error->default_type_error_data1.end_loc;
    const NecroType*      type         = error->default_type_error_data1.type;
    const NecroType*      macro_type   = error->default_type_error_data1.macro_type;
    necro_print_error_header(error_name);
    necro_print_line_at_source_loc(source_str, source_loc1, end_loc1);
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Could not infer a concrete type for the type variable: ");
    necro_type_fprint(stderr, type);
    fprintf(stderr, "\n");
    // TODO: Figure out context printing!!!!!!!
    if (type != macro_type)
    {
        fprintf(stderr, NECRO_ERR_LEFT_CHAR " In type: ");
        necro_type_fprint(stderr, macro_type);
        fprintf(stderr, "\n");
    }
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " Perhaps try adding an explicit type signature which supplies a concrete type.\n");
    fprintf(stderr, "\n");
    UNUSED(source_name);
}

void necro_print_runtime_audio_error(NecroResultError* error, const char* source_str, const char* source_name)
{
    UNUSED(source_str);
    UNUSED(source_name);
    necro_print_error_header("Audio");
    fprintf(stderr, NECRO_ERR_LEFT_CHAR " %s\n", error->runtime_audio_error_data.error_message);
}

// NOTE:
// Basic assumption is that and error will be freed after it is printed.
// Thus nested errors either need to call into necro_result_error_print
// to assure proper handling, or need to free the memory themselves.
void necro_result_error_print(NecroResultError* error, const char* source_str, const char* source_name)
{
    switch (error->type)
    {
    case NECRO_ERROR_CONS:                                      necro_print_error_cons(error, source_str, source_name); break;

    case NECRO_LEX_MALFORMED_FLOAT:                             necro_print_malformed_float_error(error, source_str, source_name); break;
    case NECRO_LEX_MALFORMED_STRING:                            necro_print_malformed_string_error(error, source_str, source_name); break;
    case NECRO_LEX_UNRECOGNIZED_CHARACTER_SEQUENCE:             necro_print_unrecognized_character_sequence_error(error, source_str, source_name); break;
    case NECRO_LEX_MIXED_BRACES:                                necro_print_mixed_braces_error(error, source_str, source_name); break;

    case NECRO_PARSE_ERROR:                                     necro_print_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE:          necro_print_declarations_missing_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE:     necro_print_simple_assignment_rhs_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE:       necro_print_apat_assignment_rhs_failed_to_parse_error(error, source_str, source_name); break;
    case NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE:        necro_print_pat_assignment_rhs_failed_to_parse_error(error, source_str, source_name); break;
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
    case NECRO_PARSE_DO_LET_EXPECTED_LEFT_BRACE:                necro_print_do_let_expected_left_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_DO_LET_EXPECTED_RIGHT_BRACE:               necro_print_do_let_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_DO_MISSING_RIGHT_BRACE:                    necro_print_do_missing_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET:                necro_print_list_missing_right_bracket_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE:                 necro_print_array_missing_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_PATTERN_EMPTY_EXPRESSION_LIST:             necro_print_pattern_empty_expression_list_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN:  necro_print_arithmetic_sequence_failed_to_parse_then_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO:    necro_print_arithmetic_sequence_failed_to_parse_to_error(error, source_str, source_name); break;
    case NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET: necro_print_arithmetic_sequence_missing_right_bracket_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN:         necro_print_case_alternative_expected_pattern_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW:           necro_print_case_alternative_expected_arrow_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION:      necro_print_case_alternative_expected_expression_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF:              necro_print_case_alternative_expected_of_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE:      necro_print_case_alternative_expected_left_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EMPTY:                    necro_print_case_alternative_empty_error(error, source_str, source_name); break;
    case NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE:     necro_print_case_alternative_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_MALFORMED_FOR_LOOP:                        necro_print_malformed_for_loop_error(error, source_str, source_name); break;
    case NECRO_PARSE_FN_OP_EXPECTED_ACCENT:                     necro_print_fn_op_expected_accent_error(error, source_str, source_name); break;
    case NECRO_PARSE_DATA_EXPECTED_TYPE:                        necro_print_data_expected_type_error(error, source_str, source_name); break;
    case NECRO_PARSE_DATA_EXPECTED_ASSIGN:                      necro_print_data_expected_assign_error(error, source_str, source_name); break;
    case NECRO_PARSE_DATA_EXPECTED_DATA_CON:                    necro_print_data_expected_data_con_error(error, source_str, source_name); break;
    case NECRO_PARSE_TYPE_EXPECTED_TYPE:                        necro_print_type_expected_type_error(error, source_str, source_name); break;
    case NECRO_PARSE_TYPE_LIST_EXPECTED_RIGHT_BRACKET:          necro_print_type_list_expected_right_bracket_error(error, source_str, source_name); break;
    case NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE:                necro_print_class_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE:             necro_print_instance_expected_right_brace_error(error, source_str, source_name); break;
    case NECRO_PARSE_CONST_CON_MISSING_RIGHT_PAREN:             necro_print_const_con_missing_right_paren(error, source_str, source_name); break;
    case NECRO_PARSE_TYPE_SIG_EXPECTED_RIGHT_PAREN:             necro_print_type_sig_expected_right_paren_error(error, source_str, source_name); break;

    case NECRO_RENAME_MULTIPLE_DEFINITIONS:                     necro_print_multiple_definitions_error(error, source_str, source_name); break;
    case NECRO_RENAME_MULTIPLE_TYPE_SIGNATURES:                 necro_print_duplicate_type_signatures_error(error, source_str, source_name); break;
    case NECRO_RENAME_NOT_IN_SCOPE:                             necro_print_not_in_scope_error(error, source_str, source_name); break;

    case NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE:              necro_print_uninitialized_recursive_value_error(error, source_str, source_name); break;
    case NECRO_TYPE_RECURSIVE_FUNCTION:                         necro_print_recursive_function_error(error, source_str, source_name); break;
    case NECRO_TYPE_RECURSIVE_DATA_TYPE:                        necro_print_recursive_data_type_error(error, source_str, source_name); break;
    case NECRO_TYPE_NON_CONCRETE_INITIALIZED_VALUE:             necro_print_non_concrete_initialized_value_error(error, source_str, source_name); break;
    case NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE:            necro_print_non_recursive_initialized_value_error(error, source_str, source_name); break;

    case NECRO_TYPE_MISMATCHED_TYPE:                            necro_print_mismatched_type_error(error, source_str, source_name); break;
    case NECRO_TYPE_POLYMORPHIC_PAT_BIND:                       necro_print_polymorphic_pat_bind_error(error, source_str, source_name); break;
    case NECRO_TYPE_OCCURS:                                     necro_print_occurs_error(error, source_str, source_name); break;
    case NECRO_TYPE_FINAL_DO_STATEMENT:                         necro_print_final_do_statement_error(error, source_str, source_name); break;
    case NECRO_TYPE_RIGID_TYPE_VARIABLE:                        necro_print_rigid_type_variable_error(error, source_str, source_name); break;
    case NECRO_TYPE_MALFORMED_UNIQUENESS_ATTRIBUTE:             necro_print_malformed_uniqueness_attribute_error(error, source_str, source_name); break;

    case NECRO_TYPE_NOT_A_CLASS:                                necro_print_type_not_a_class_error(error, source_str, source_name); break;
    case NECRO_TYPE_NOT_A_VISIBLE_METHOD:                       necro_print_type_not_a_visible_member_error(error, source_str, source_name); break;
    case NECRO_TYPE_NO_EXPLICIT_IMPLEMENTATION:                 necro_print_type_no_explicit_implementation_error(error, source_str, source_name); break;
    case NECRO_TYPE_AMBIGUOUS_CLASS:                            necro_print_type_ambiguous_class_error(error, source_str, source_name); break;
    case NECRO_TYPE_CONSTRAINS_ONLY_CLASS_VAR:                  necro_print_type_constrains_only_class_var_error(error, source_str, source_name); break;
    case NECRO_TYPE_NOT_AN_INSTANCE_OF:                         necro_print_type_is_not_an_instance_of_error(error, source_str, source_name); break;
    case NECRO_TYPE_MULTIPLE_CLASS_DECLARATIONS:                necro_print_type_multiple_class_declarations_error(error, source_str, source_name); break;
    case NECRO_TYPE_MULTIPLE_INSTANCE_DECLARATIONS:             necro_print_type_multiple_instance_declarations_error(error, source_str, source_name); break;
    case NECRO_TYPE_DOES_NOT_IMPLEMENT_SUPER_CLASS:             necro_print_type_does_not_implment_super_class_error(error, source_str, source_name); break;
    case NECRO_TYPE_AMBIGUOUS_TYPE_VAR:                         necro_print_ambiguous_type_var(error, source_str, source_name); break;

    case NECRO_CONSTRAINT_MALFORMED_CONSTRAINT:                 necro_print_constraint_malformed_constraint_error(error, source_str, source_name); break;

    case NECRO_KIND_MISMATCHED_KIND:                            necro_print_mismatched_kind_error(error, source_str, source_name); break;

    case NECRO_RUNTIME_AUDIO_ERROR:                             necro_print_runtime_audio_error(error, source_str, source_name); break;

    default:
        assert(false && "[necro_result_error_print] Unknown error type");
        break;
    }
    free(error);
}

void necro_result_error_destroy(NECRO_RESULT_TYPE result_type, NecroResultError* error)
{
    if (result_type == NECRO_RESULT_OK || error == NULL)
        return;
    if (error->type == NECRO_ERROR_CONS)
    {
        necro_result_error_destroy(result_type, error->error_cons.error1);
        necro_result_error_destroy(result_type, error->error_cons.error2);
    }
    free(error);
}

void necro_assert_on_error(NECRO_RESULT_TYPE result_type, NecroResultError* error)
{
    assert(result_type == NECRO_RESULT_OK);
    necro_result_error_destroy(result_type, error);
}

void necro_print_and_assert_on_error(NECRO_RESULT_TYPE result_type, NecroResultError* error, const char* source_str, const char* source_name)
{
    if (result_type != NECRO_RESULT_OK)
    {
        necro_result_error_print(error, source_str, source_name);
        fflush(stdout);
    }
    assert(result_type == NECRO_RESULT_OK);
}

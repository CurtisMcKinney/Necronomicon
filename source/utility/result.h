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
#include "ast_symbol.h"

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
struct NecroConstraint;

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
    NECRO_PARSE_MALFORMED_FOR_LOOP,
    NECRO_PARSE_FN_OP_EXPECTED_ACCENT,
    NECRO_PARSE_DATA_EXPECTED_TYPE,
    NECRO_PARSE_DATA_EXPECTED_ASSIGN,
    NECRO_PARSE_DATA_EXPECTED_DATA_CON,
    NECRO_PARSE_TYPE_EXPECTED_TYPE,
    NECRO_PARSE_TYPE_LIST_EXPECTED_RIGHT_BRACKET,
    NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE,
    NECRO_PARSE_CONST_CON_MISSING_RIGHT_PAREN,
    NECRO_PARSE_TYPE_SIG_EXPECTED_RIGHT_PAREN,

    NECRO_RENAME_MULTIPLE_DEFINITIONS,
    NECRO_RENAME_MULTIPLE_TYPE_SIGNATURES,
    NECRO_RENAME_NOT_IN_SCOPE,

    NECRO_TYPE_RIGID_TYPE_VARIABLE,
    NECRO_TYPE_NOT_AN_INSTANCE_OF,
    NECRO_TYPE_OCCURS,
    NECRO_TYPE_MISMATCHED_ARITY,
    NECRO_TYPE_MISMATCHED_TYPE,
    NECRO_TYPE_POLYMORPHIC_PAT_BIND,
    NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE,
    NECRO_TYPE_FINAL_DO_STATEMENT,
    NECRO_TYPE_AMBIGUOUS_CLASS,
    NECRO_TYPE_CONSTRAINS_ONLY_CLASS_VAR,
    NECRO_TYPE_AMBIGUOUS_TYPE_VAR,
    NECRO_TYPE_NON_CONCRETE_INITIALIZED_VALUE,
    NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE,
    NECRO_TYPE_NOT_A_CLASS,
    NECRO_TYPE_NOT_A_VISIBLE_METHOD,
    NECRO_TYPE_NO_EXPLICIT_IMPLEMENTATION,
    NECRO_TYPE_DOES_NOT_IMPLEMENT_SUPER_CLASS,
    NECRO_TYPE_MULTIPLE_CLASS_DECLARATIONS,
    NECRO_TYPE_MULTIPLE_INSTANCE_DECLARATIONS,

    NECRO_CONSTRAINT_MALFORMED_CONSTRAINT,

    NECRO_TYPE_RECURSIVE_FUNCTION,
    NECRO_TYPE_RECURSIVE_DATA_TYPE,
    NECRO_TYPE_LIFTED_TYPE_RESTRICTION,
    NECRO_TYPE_MISMATCHED_ORDER,

    NECRO_KIND_MISMATCHED_KIND,
    NECRO_KIND_MISMATCHED_ARITY,
    NECRO_KIND_RIGID_KIND_VARIABLE,

    NECRO_RUNTIME_AUDIO_ERROR,

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

typedef struct
{
    NecroAstSymbol* ast_symbol;
    NecroSourceLoc  source_loc;
    NecroSourceLoc  end_loc;
} NecroDefaultAstErrorData;

typedef struct
{
    NecroAstSymbol* ast_symbol1;
    NecroSourceLoc  source_loc1;
    NecroSourceLoc  end_loc1;
    NecroAstSymbol* ast_symbol2;
    NecroSourceLoc  source_loc2;
    NecroSourceLoc  end_loc2;
} NecroDefaultAstErrorData2;

typedef struct
{
    NecroAstSymbol*         ast_symbol;
    const struct NecroType* type;
    const struct NecroType* macro_type;
    NecroSourceLoc          source_loc;
    NecroSourceLoc          end_loc;
} NecroDefaultTypeErrorData1;

typedef struct
{
    struct NecroType* type1;
    struct NecroType* type2;
    struct NecroType* macro_type1;
    struct NecroType* macro_type2;
    NecroSourceLoc    source_loc;
    NecroSourceLoc    end_loc;
} NecroDefaultTypeErrorData2;

typedef struct
{
    NecroAstSymbol*   type_class_symbol;
    struct NecroType* type1;
    struct NecroType* type2;
    struct NecroType* macro_type1;
    struct NecroType* macro_type2;
    NecroSourceLoc    source_loc;
    NecroSourceLoc    end_loc;
} NecroDefaultTypeClassErrorData;

typedef struct
{
    const char* error_message;
} NecroRuntimeAudioErrorData;

typedef struct
{
    struct NecroConstraint* constraint;
} NecroConstraintError;

typedef struct NecroResultError
{
    union
    {
        NecroDefaultErrorData          default_error_data;
        NecroDefaultAstErrorData       default_ast_error_data;
        NecroDefaultAstErrorData2      default_ast_error_data_2;
        NecroDefaultTypeErrorData1     default_type_error_data1;
        NecroDefaultTypeErrorData2     default_type_error_data2;
        NecroDefaultTypeClassErrorData default_type_class_error_data;
        NecroRuntimeAudioErrorData     runtime_audio_error_data;
        NecroConstraintError           contraint_error_data;
        NecroErrorCons                 error_cons;
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
#define NECRO_DECLARE_RESULT(TYPE)                     \
typedef struct                                         \
{                                                      \
    NECRO_RESULT_TYPE type;                            \
    union                                              \
    {                                                  \
        NecroResultError* error;                       \
        TYPE value;                                    \
    };                                                 \
} NecroResult_##TYPE;                                  \
static inline NecroResult_##TYPE ok_##TYPE(TYPE value) \
{                                                      \
    return (NecroResult_##TYPE)                        \
    {                                                  \
        .value = value,                                \
        .type = NECRO_RESULT_OK                        \
    };                                                 \
}

#define NECRO_DECLARE_PTR_RESULT(TYPE)                         \
typedef struct                                                 \
{                                                              \
    NECRO_RESULT_TYPE type;                                    \
    union                                                      \
    {                                                          \
        NecroResultError* error;                               \
        struct TYPE* value;                                    \
    };                                                         \
} NecroResult_##TYPE;                                          \
static inline NecroResult_##TYPE ok_##TYPE(struct TYPE* value) \
{                                                              \
    return (NecroResult_##TYPE)                                \
    {                                                          \
        .value = value,                                        \
        .type = NECRO_RESULT_OK                                \
    };                                                         \
}

// NOTE: Try to keep all the value sizes to word size!
typedef size_t NecroParseAstLocalPtr;
NECRO_DECLARE_RESULT(NecroParseAstLocalPtr);
NECRO_DECLARE_RESULT(size_t);
NECRO_DECLARE_RESULT(bool);
struct NecroAst;
NECRO_DECLARE_PTR_RESULT(NecroAst);
struct NecroAstSymbol;
NECRO_DECLARE_PTR_RESULT(NecroAstSymbol);
struct NecroType;
NECRO_DECLARE_PTR_RESULT(NecroType);
struct NecroTypeClassContext;
NECRO_DECLARE_PTR_RESULT(NecroTypeClassContext);
struct NecroAliasSet;
NECRO_DECLARE_PTR_RESULT(NecroAliasSet);
struct NecroOccurrenceTrace;
NECRO_DECLARE_PTR_RESULT(NecroOccurrenceTrace);
struct NecroInstSub;
NECRO_DECLARE_PTR_RESULT(NecroInstSub);
NECRO_DECLARE_PTR_RESULT(NecroConstraint);
struct NecroConstraintList;
NECRO_DECLARE_PTR_RESULT(NecroConstraintList);
struct NecroCoreAst;
NECRO_DECLARE_PTR_RESULT(NecroCoreAst);

typedef struct
{
    NECRO_RESULT_TYPE type;
    union
    {
        NecroResultError* error;
        size_t            value;
    };
} NecroResult_void;
static inline NecroResult_void ok_void()
{
    return (NecroResult_void) { .value = 0, .type = NECRO_RESULT_OK };
}

typedef union
{
    NecroResult_void                  void_result;
    NecroResult_bool                  bool_result;
    NecroResult_size_t                size_t_result;
    NecroResult_NecroParseAstLocalPtr NecroParseAstLocalPtr_result;
    NecroResult_NecroAst              NecroAst_result;
    NecroResult_NecroAstSymbol        NecroAstSymbol_result;
    NecroResult_NecroType             NecroType_result;
    NecroResult_NecroTypeClassContext NecroTypeClassContext_result;
    NecroResult_NecroAliasSet         NecroAliasSet_result;
    NecroResult_NecroOccurrenceTrace  NecroOccurrenceTrace_result;
    NecroResult_NecroInstSub          NecroInstSub_result;
    NecroResult_NecroConstraint       NecroConstraint_result;
    NecroResult_NecroConstraintList   NecroConstraintList_result;
    NecroResult_NecroCoreAst          NecroCoreAst_result;
} NecroResultUnion;

// TODO: If and when the compiler becomes threaded,
// replace this with a thread local storage version!
extern NecroResultUnion global_result;

// #define necro_assert_on_error(RESULT, ERROR) assert(RESULT == NECRO_RESULT_OK); UNUSED(ERROR);
void necro_assert_on_error(NECRO_RESULT_TYPE result_type, NecroResultError* error);

///////////////////////////////////////////////////////
// Macro API
///////////////////////////////////////////////////////
#define NecroResult(TYPE) NecroResult_##TYPE
#define necro_try(TYPE, EXPR) (global_result.TYPE##_result = EXPR); if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE##_result;
#define necro_try_result(TYPE, EXPR) (global_result.TYPE##_result = EXPR).value; if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE##_result;
#define necro_try_map(TYPE, TYPE2, EXPR) (global_result.TYPE##_result = EXPR); if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE2##_result;
#define necro_try_map_result(TYPE, TYPE2, EXPR) (global_result.TYPE##_result = EXPR).value; if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE2##_result;
#define unwrap(TYPE, EXPR) (global_result.TYPE##_result = EXPR); necro_assert_on_error(global_result.TYPE##_result.type, global_result.TYPE##_result.error);
#define unwrap_or_print_error(TYPE, EXPR, SRC, NAME) (global_result.TYPE##_result = EXPR); necro_print_and_assert_on_error(global_result.TYPE##_result.type, global_result.TYPE##_result.error, SRC, NAME);
#define unwrap_result(TYPE, EXPR) (global_result.TYPE##_result = EXPR).value; necro_assert_on_error(global_result.TYPE##_result.type, global_result.TYPE##_result.error);
#define unwrap_result_or_print_error(TYPE, EXPR, SRC, NAME) (global_result.TYPE##_result = EXPR).value; necro_print_and_assert_on_error(global_result.TYPE##_result.type, global_result.TYPE##_result.error, SRC, NAME);
#define assert_ok(TYPE, EXPR) (global_result.TYPE##_result = EXPR); necro_assert_on_error(global_result.TYPE##_result.type, global_result.TYPE##_result.error);
#define necro_error_map(TYPE1, TYPE2, EXPR) (((NecroResultUnion) { .TYPE1##_result = EXPR }).TYPE2##_result);
#define necro_unreachable(TYPE) assert(false); return global_result.TYPE##_result
#define necro_try2_then_return(TYPE, EXPR1, EXPR2, THEN_EXPR) \
global_result.TYPE##_result = EXPR1; \
if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE##_result; \
global_result.TYPE##_result = EXPR2; \
if (global_result.TYPE##_result.type != NECRO_RESULT_OK) return global_result.TYPE##_result; \
return THEN_EXPR;
#define ok(TYPE, EXPR) ((NecroResult_##TYPE) { .value = EXPR, .type = NECRO_RESULT_OK  })

///////////////////////////////////////////////////////
// Error API
///////////////////////////////////////////////////////

// Lex
NecroResult(bool)                  necro_malformed_string_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(bool)                  necro_malformed_float_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_unrecognized_character_sequence_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_mixed_braces_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_parse_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);

// Parse
NecroResult(NecroParseAstLocalPtr) necro_parse_error_cons(NecroResultError* error1, NecroResultError* error2);
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
NecroResult(NecroParseAstLocalPtr) necro_type_sig_expected_right_paren_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
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
NecroResult(NecroParseAstLocalPtr) necro_malformed_for_loop_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_fn_op_expected_accent_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_data_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_data_expected_assign_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_data_expected_data_con_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_type_expected_type_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_type_list_expected_right_bracket(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_class_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroParseAstLocalPtr) necro_instance_expected_right_brace_error(NecroSourceLoc source_loc, NecroSourceLoc end_loc);

// Rename
NecroResult(NecroAstSymbol)        necro_multiple_definitions_error(NecroAstSymbol* ast_symbol1, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2);
NecroResult(NecroAstSymbol)        necro_duplicate_type_signatures_error(NecroAstSymbol* ast_symbol, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2);
NecroResult(NecroAstSymbol)        necro_not_in_scope_error(NecroAstSymbol* ast_symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

// Infer
NecroResult(NecroType)             necro_type_polymorphic_pat_bind_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

NecroResult(void)                  necro_type_non_concrete_initialized_value_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(void)                  necro_type_non_recursive_initialized_value_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_uninitialized_recursive_value_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_recursive_function_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_recursive_data_type_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_final_do_statement_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(bool)                  necro_type_ambiguous_type_var_error(NecroAstSymbol* ast_symbol, const struct NecroType* type, const struct NecroType* macro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

NecroResult(NecroType)             necro_type_mismatched_type_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_mismatched_type_error_partial( struct NecroType* type1, struct NecroType* type2);
NecroResult(NecroType)             necro_type_occurs_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_occurs_error_partial( struct NecroType* type1, struct NecroType* type2);

NecroResult(NecroType)             necro_type_rigid_type_variable_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

// TODO (Curtis 2-9-19): Isn't this a kind error?
NecroResult(NecroType)             necro_type_mismatched_arity_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

// TypeClass error type?
NecroResult(NecroType)             necro_type_not_an_instance_of_error_partial(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type);
NecroResult(NecroType)             necro_type_not_an_instance_of_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_ambiguous_class_error(NecroAstSymbol* type_class_symbol, struct NecroType* type1, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_constrains_only_class_var_error(NecroAstSymbol* ast_symbol1, NecroSourceLoc source_loc1, NecroSourceLoc end_loc1, NecroAstSymbol* ast_symbol2, NecroSourceLoc source_loc2, NecroSourceLoc end_loc2);
NecroResult(NecroType)             necro_type_multiple_class_declarations_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_multiple_instance_declarations_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_no_explicit_implementation_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_does_not_implement_super_class_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_type_not_a_visible_method_error(NecroAstSymbol* type_class_ast_symbol, struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

NecroResult(NecroConstraintList)   necro_type_not_a_class_error(NecroAstSymbol* ast_symbol, struct NecroType* type, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroConstraintList)   necro_type_malformed_constraint(struct NecroConstraint* constraint);

// Kind
NecroResult(NecroType)             necro_kind_mismatched_kind_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_kind_mismatched_arity_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType)             necro_kind_rigid_kind_variable_error(struct NecroType* type1, struct NecroType* type2, struct NecroType* macro_type1, struct NecroType* macro_type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc);

// Audio
NecroResult(void)                  necro_runtime_audio_error(const char* error_message);

void                               necro_print_and_assert_on_error(NECRO_RESULT_TYPE result_type, NecroResultError* error, const char* source_str, const char* source_name);
void                               necro_result_error_print(NecroResultError* error, const char* source_str, const char* source_name);
void                               necro_result_error_destroy(NECRO_RESULT_TYPE result_type, NecroResultError* error);

#endif // NECRO_RESULT_H

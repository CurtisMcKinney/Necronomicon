/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include "parse_test.h"
#include "lexer.h"
#include "result.h"

void necro_test_parse_error(const char* test_name, const char* str, NECRO_RESULT_ERROR_TYPE error_type)
{
    // Set up
    NecroIntern         intern = necro_create_intern();
    NecroLexTokenVector tokens = necro_empty_lex_token_vector();
    NecroAST            ast    = necro_empty_ast();
    NecroCompileInfo    info   = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(str, strlen(str), &intern, &tokens, info));
    NecroResult(void) result = necro_parse(&tokens, &intern, &ast, info);
    assert(result.type == NECRO_RESULT_ERROR);
    assert(result.error.type == error_type);
    printf("Parse %s test: Passed\n", test_name);

    // Clean up
    necro_destroy_intern(&intern);
    necro_destroy_lex_token_vector(&tokens);
    necro_destroy_ast(&ast);
}

void necro_test_parser()
{
    necro_announce_phase("NecroParser");

    // Error Tests
    necro_test_parse_error("ParseError", "consPat (x :) = x\n", NECRO_PARSE_ERROR);
    necro_test_parse_error("MalformedArray", "malformedArray = {3, 1, 2\n", NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE);
    necro_test_parse_error("MalformedList", "malformedList = [1, 2, 3\n", NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET);
    necro_test_parse_error("MalformedListType", "malformedListType :: [[Int]\n", NECRO_PARSE_TYPE_LIST_EXPECTED_RIGHT_BRACKET);
    necro_test_parse_error("MalformedDeclaration", "malformedDeclaration = y + z\n  where\n    y = 10\n    [zlkjsd\n", NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE);
    necro_test_parse_error("MalformedAssignment1", "malformedAssignment = =\n", NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedAssignment2", "malformedRHS = where x\n", NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedAssignment3", "topDec2 = x + y / z\n  where\n    x = 10\n    y = !!!20\n    z = 30\n", NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedFunction", "malformedFunction x y = !\n", NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedPatternAssignment", "(malformedPat1, malformedPat2) = ?\n", NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedWhere", "malformedWhere = x where []\n", NECRO_PARSE_RHS_EMPTY_WHERE);
    necro_test_parse_error("MalformedLet1", "malformedLet = let ! in x\n", NECRO_PARSE_LET_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedLet2", "emptyLet = let x = 3 in !\n", NECRO_PARSE_LET_EMPTY_IN);
    necro_test_parse_error("MalformedLet3", "letExpectedIn = let x = 3 !! in x\n", NECRO_PARSE_LET_MISSING_IN);
    necro_test_parse_error("MalformedTuple", "malformedTuple = (3, 2,\n", NECRO_PARSE_TUPLE_MISSING_PAREN);
    necro_test_parse_error("MalformedParenExpr", "malformedParenExpr = 3 + (2 * y\n", NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN);
    necro_test_parse_error("MalformedIf1", "malformedIf1 = if then 2 else 4\n", NECRO_PARSE_IF_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedIf2", "malformedIf2 = if True ! then 5 else 4\n", NECRO_PARSE_IF_MISSING_THEN);
    necro_test_parse_error("MalformedIf3", "malformedIf3 = if True then else 4\n", NECRO_PARSE_IF_MISSING_EXPR_AFTER_THEN);
    necro_test_parse_error("MalformedIf4", "malformedIf4 = if True then 10 ! else 4\n", NECRO_PARSE_IF_MISSING_ELSE);
    necro_test_parse_error("MalformedIf5", "malformedIf5 = if True then 10 else !\n", NECRO_PARSE_IF_MISSING_EXPR_AFTER_ELSE);
    necro_test_parse_error("MalformedLambda1", "malformedLambda = \\x ->\n", NECRO_PARSE_LAMBDA_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedLambda2", "malformedLambda2 = \\x\n", NECRO_PARSE_LAMBDA_MISSING_ARROW);
    necro_test_parse_error("MalformedLambda3", "malformedLambda3 = \\ -> x\n", NECRO_PARSE_LAMBDA_FAILED_TO_PARSE_PATTERN);
    necro_test_parse_error("MalformedBind", "malformedBind = do\n  x <- !\n", NECRO_PARSE_DO_BIND_FAILED_TO_PARSE);
    necro_test_parse_error("MalformedDoLet", "malformedDoLet = do\n  let x = 5\n      !\n", NECRO_PARSE_DO_LET_EXPECTED_RIGHT_BRACE);
    necro_test_parse_error("MalformedDoBlock", "malformedDoBlock = do\n  doIt\n  !\n", NECRO_PARSE_DO_MISSING_RIGHT_BRACE);
    necro_test_parse_error("MalformedArithmeticThen", "malformedArithmeticThen = [3,..,1..]\n", NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN);
    necro_test_parse_error("MalformedArithmeticTo", "malformedArithmeticTo = [3..!]\n", NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO);
    necro_test_parse_error("MalformedArithmeticSequence", "malformedArithmeticSequence = [3..0\n", NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET);
    necro_test_parse_error("MalformedCaseAlternative1", "malformedCaseAlternative =\n  case 3 of\n    data -> 3\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN);
    necro_test_parse_error("MalformedCaseAlternative2", "malformedCaseAlternativeArrow =\n  case Nothing of\n    Just x = 3\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW);
    necro_test_parse_error("MalformedCaseAlternative3", "malformedCaseAlternativeExpr =\n  case Nothing of\n    Just x -> data * 10\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION);
    necro_test_parse_error("MalformedCaseExpr1", "malformedCaseExpr =\n  case Nothing data\n    Just x -> x\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF);
    necro_test_parse_error("MalformedCaseExpr2", "malformedCaseExpr =\n  case Nothing of {\n    Just x -> x\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE);
    necro_test_parse_error("MalformedCaseExpr3", "malformedCaseExpr =\n  case Nothing of\n", NECRO_PARSE_CASE_ALTERNATIVE_EMPTY);
    necro_test_parse_error("MalformedCaseExpr4", "malformedCaseExpr =\n  case Nothing of\n    Just x -> x !!\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE);
    necro_test_parse_error("MalformedOpPat", "malformedOpPat (x `Con y) = x\n", NECRO_PARSE_FN_OP_EXPECTED_ACCENT);
    necro_test_parse_error("MalformedData1", "data !!MalformedData1 = Hello\n", NECRO_PARSE_DATA_EXPECTED_TYPE);
    necro_test_parse_error("MalformedData2", "data MalformedData2 !! Hello\n", NECRO_PARSE_DATA_EXPECTED_ASSIGN);
    necro_test_parse_error("MalformedData3", "data MalformedData3 = !! | Doom\n", NECRO_PARSE_DATA_EXPECTED_DATA_CON);
    necro_test_parse_error("MalformedType", "malformedTypeFn :: Int -> !!\n", NECRO_PARSE_TYPE_EXPECTED_TYPE);
    necro_test_parse_error("MalformedTypeClss", "class MalformedClass a where\n  x :: a -> a\n  !!\n", NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE);
    necro_test_parse_error("MalformedClassInstance", "instance MalformedInstance Int where\n  x y = y\n  !!\n", NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE);
    necro_test_parse_error("InitialValueError", "malformedInit ~ (Just 0 = 0\n", NECRO_PARSE_CONST_CON_MISSING_RIGHT_PAREN);

    // Parse Tests
}

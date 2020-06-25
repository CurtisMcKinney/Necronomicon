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
#include "parse_ast.h"
#include "utility.h"

void necro_parse_ast_test_error(const char* test_name, const char* str, NECRO_RESULT_ERROR_TYPE error_type)
{
    UNUSED(error_type);
    // Set up
    NecroIntern         intern = necro_intern_create();
    NecroLexTokenVector tokens = necro_empty_lex_token_vector();
    NecroParseAstArena  ast    = necro_parse_ast_arena_empty();
    NecroCompileInfo    info   = necro_test_compile_info();

    // Compile
    unwrap_or_print_error(void, necro_lex(info, &intern, str, strlen(str), &tokens), str, "Test");
    NecroResult(void) result = necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &ast);
    assert(result.type == NECRO_RESULT_ERROR);
    assert(result.error->type == error_type);
    printf("Parse %s test: Passed\n", test_name);

    // Clean up
    necro_result_error_destroy(result.type, result.error);
    necro_intern_destroy(&intern);
    necro_destroy_lex_token_vector(&tokens);
    necro_parse_ast_arena_destroy(&ast);
}

void necro_parse_ast_test(const char* test_name, const char* str, NecroIntern* intern2, NecroParseAstArena* ast2)
{
    // Set up
    NecroIntern         intern = necro_intern_create();
    NecroLexTokenVector tokens = necro_empty_lex_token_vector();
    NecroParseAstArena  ast    = necro_parse_ast_arena_empty();
    NecroCompileInfo    info   = necro_test_compile_info();

    // Compile
    unwrap_or_print_error(void, necro_lex(info, &intern, str, strlen(str), &tokens), str, "Test");
    unwrap_or_print_error(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &ast), str, "Test");

    /* puts("//////////////////////////////////////////////////////////////////"); */
    /* printf("Parsed string: %s\n", str); */
    /* puts("AST 1:"); */
    /* necro_parse_ast_print(&ast); */
    /* puts("AST 2:"); */
    /* necro_parse_ast_print(ast2); */

    // Compare
    necro_parse_ast_assert_eq(&ast, ast2);
    printf("Parse %s test: Passed\n", test_name);

    // Clean up
    necro_intern_destroy(&intern);
    necro_destroy_lex_token_vector(&tokens);
    necro_parse_ast_arena_destroy(&ast);
    necro_intern_destroy(intern2);
    necro_parse_ast_arena_destroy(ast2);
}

void necro_parse_test()
{
    necro_announce_phase("NecroParser");

    // Error Tests
    necro_parse_ast_test_error("ParseError", "consPat (x :) = x\n", NECRO_PARSE_ERROR);
    necro_parse_ast_test_error("MalformedArray", "malformedArray = {3, 1, 2\n", NECRO_PARSE_ARRAY_MISSING_RIGHT_BRACE);

    // necro_parse_ast_test_error("MalformedList", "malformedList = [1, 2, 3\n", NECRO_PARSE_LIST_MISSING_RIGHT_BRACKET);
    // necro_parse_ast_test_error("MalformedListType", "malformedListType :: [[Int]\n", NECRO_PARSE_TYPE_LIST_EXPECTED_RIGHT_BRACKET);

    necro_parse_ast_test_error("MalformedDeclaration", "malformedDeclaration = y + z\n  where\n    y = 10\n    [zlkjsd\n", NECRO_PARSE_DECLARATIONS_MISSING_RIGHT_BRACE);
    necro_parse_ast_test_error("MalformedAssignment1", "malformedAssignment = =\n", NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedAssignment2", "malformedRHS = where x\n", NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedAssignment3", "topDec2 = x + y / z\n  where\n    x = 10\n    y = !!!20\n    z = 30\n", NECRO_PARSE_SIMPLE_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedFunction", "malformedFunction x y = !\n", NECRO_PARSE_APAT_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedPatternAssignment", "(malformedPat1, malformedPat2) = ?\n", NECRO_PARSE_PAT_ASSIGNMENT_RHS_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedWhere", "malformedWhere = x where []\n", NECRO_PARSE_RHS_EMPTY_WHERE);
    necro_parse_ast_test_error("MalformedLet1", "malformedLet = let ! in x\n", NECRO_PARSE_LET_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedLet2", "emptyLet = let x = 3 in !\n", NECRO_PARSE_LET_EMPTY_IN);
    necro_parse_ast_test_error("MalformedLet3", "letExpectedIn = let x = 3 !! in x\n", NECRO_PARSE_LET_MISSING_IN);
    necro_parse_ast_test_error("MalformedTuple", "malformedTuple = (3, 2,\n", NECRO_PARSE_TUPLE_MISSING_PAREN);
    necro_parse_ast_test_error("MalformedParenExpr", "malformedParenExpr = 3 + (2 * y\n", NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN);
    necro_parse_ast_test_error("MalformedIf1", "malformedIf1 = if then 2 else 4\n", NECRO_PARSE_IF_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedIf2", "malformedIf2 = if True ! then 5 else 4\n", NECRO_PARSE_IF_MISSING_THEN);
    necro_parse_ast_test_error("MalformedIf3", "malformedIf3 = if True then else 4\n", NECRO_PARSE_IF_MISSING_EXPR_AFTER_THEN);
    necro_parse_ast_test_error("MalformedIf4", "malformedIf4 = if True then 10 ! else 4\n", NECRO_PARSE_IF_MISSING_ELSE);
    necro_parse_ast_test_error("MalformedIf5", "malformedIf5 = if True then 10 else !\n", NECRO_PARSE_IF_MISSING_EXPR_AFTER_ELSE);
    necro_parse_ast_test_error("MalformedLambda1", "malformedLambda = \\x ->\n", NECRO_PARSE_LAMBDA_FAILED_TO_PARSE);
    necro_parse_ast_test_error("MalformedLambda2", "malformedLambda2 = \\x\n", NECRO_PARSE_LAMBDA_MISSING_ARROW);
    necro_parse_ast_test_error("MalformedLambda3", "malformedLambda3 = \\ -> x\n", NECRO_PARSE_LAMBDA_FAILED_TO_PARSE_PATTERN);
    // necro_parse_ast_test_error("MalformedBind", "malformedBind = do\n  x <- !\n", NECRO_PARSE_DO_BIND_FAILED_TO_PARSE);
    // necro_parse_ast_test_error("MalformedDoLet", "malformedDoLet = do\n  let x = 5\n      !\n", NECRO_PARSE_DO_LET_EXPECTED_RIGHT_BRACE);
    // necro_parse_ast_test_error("MalformedDoBlock", "malformedDoBlock = do\n  doIt\n  !\n", NECRO_PARSE_DO_MISSING_RIGHT_BRACE);
    necro_parse_ast_test_error("MalformedArithmeticThen", "malformedArithmeticThen = [3,..,1..]\n", NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_THEN);
    necro_parse_ast_test_error("MalformedArithmeticTo", "malformedArithmeticTo = [3..!]\n", NECRO_PARSE_ARITHMETIC_SEQUENCE_FAILED_TO_PARSE_TO);
    necro_parse_ast_test_error("MalformedArithmeticSequence", "malformedArithmeticSequence = [3..0\n", NECRO_PARSE_ARITHMETIC_SEQUENCE_MISSING_RIGHT_BRACKET);
    necro_parse_ast_test_error("MalformedCaseAlternative1", "malformedCaseAlternative =\n  case 3 of\n    data -> 3\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_PATTERN);
    necro_parse_ast_test_error("MalformedCaseAlternative2", "malformedCaseAlternativeArrow =\n  case Nothing of\n    Just x = 3\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_ARROW);
    necro_parse_ast_test_error("MalformedCaseAlternative3", "malformedCaseAlternativeExpr =\n  case Nothing of\n    Just x -> data * 10\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_EXPRESSION);
    necro_parse_ast_test_error("MalformedCaseExpr1", "malformedCaseExpr =\n  case Nothing data\n    Just x -> x\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_OF);
    necro_parse_ast_test_error("MalformedCaseExpr2", "malformedCaseExpr =\n  case Nothing of {\n    Just x -> x\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_LEFT_BRACE);
    necro_parse_ast_test_error("MalformedCaseExpr3", "malformedCaseExpr =\n  case Nothing of\n", NECRO_PARSE_CASE_ALTERNATIVE_EMPTY);
    necro_parse_ast_test_error("MalformedCaseExpr4", "malformedCaseExpr =\n  case Nothing of\n    Just x -> x !!\n", NECRO_PARSE_CASE_ALTERNATIVE_EXPECTED_RIGHT_BRACE);
    necro_parse_ast_test_error("MalformedOpPat", "malformedOpPat (x `Con y) = x\n", NECRO_PARSE_FN_OP_EXPECTED_ACCENT);
    necro_parse_ast_test_error("MalformedData1", "data !!MalformedData1 = Hello\n", NECRO_PARSE_DATA_EXPECTED_TYPE);
    necro_parse_ast_test_error("MalformedData2", "data MalformedData2 !! Hello\n", NECRO_PARSE_DATA_EXPECTED_ASSIGN);
    necro_parse_ast_test_error("MalformedData3", "data MalformedData3 = !! | Doom\n", NECRO_PARSE_DATA_EXPECTED_DATA_CON);
    necro_parse_ast_test_error("MalformedType", "malformedTypeFn :: Int -> !!\n", NECRO_PARSE_TYPE_EXPECTED_TYPE);
    necro_parse_ast_test_error("MalformedTypeClss", "class MalformedClass a where\n  x :: a -> a\n  !!\n", NECRO_PARSE_CLASS_EXPECTED_RIGHT_BRACE);
    necro_parse_ast_test_error("MalformedClassInstance", "instance MalformedInstance Int where\n  x y = y\n  !!\n", NECRO_PARSE_INSTANCE_EXPECTED_RIGHT_BRACE);
    // necro_parse_ast_test_error("InitialValueError", "malformedInit ~ (Just 0 = 0\n", NECRO_PARSE_CONST_CON_MISSING_RIGHT_PAREN);
    necro_parse_ast_test_error("InitialValueError", "malformedInit ~ (Just 0 = 0\n", NECRO_PARSE_PAREN_EXPRESSION_MISSING_PAREN);
    necro_parse_ast_test_error("MalformedForLoop", "malformedForLoop =\n  loop x <- 0 for i <- each do x\n", NECRO_PARSE_MALFORMED_FOR_LOOP);


    // TODO: This should proc a parse error!
    // {
    //     NecroIntern        intern = necro_intern_create();
    //     NecroParseAstArena ast    = (NecroParseAstArena) { necro_arena_create(128 * sizeof(NecroParseAst)) };
    //     ast.root                  = null_local_ptr;
    //     const char* test_name   = "Instance Context";
    //     const char* test_source = ""
    //         "Num a => instance OgreMagi (Maybe a) where\n"
    //         "  twoHeads x = (Just x, x)\n";
    //     necro_parse_ast_test(test_name, test_source, &intern, &ast);
    // }

    // Parse Tests
    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constant1"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER }),
                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ConstantTest1", "constant1 = 10\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constant2"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .double_literal = 55.8, .type = NECRO_AST_CONSTANT_FLOAT }),
                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ConstantTest2", "constant2 = 55.8\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constant3"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .char_literal = 'c', .type = NECRO_AST_CONSTANT_CHAR }),
                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ConstantTest3", "constant3 = \'c\'\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constant4"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .symbol = necro_intern_string(&intern, "This is a test"), .type = NECRO_AST_CONSTANT_STRING }),
                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ConstantTest4", "constant4 = \"This is a test\"\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "binop1"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER }),
                            necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 5, .type = NECRO_AST_CONSTANT_INTEGER }),
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 3, .type = NECRO_AST_CONSTANT_INTEGER }),
                                NECRO_BIN_OP_MUL,
                                necro_intern_string(&intern, "*")),
                            NECRO_BIN_OP_ADD,
                            necro_intern_string(&intern, "+")),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("BinOp1", "binop1 = 10 + 5 * 3\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "binop2"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .double_literal = 1000.0, .type = NECRO_AST_CONSTANT_FLOAT }),
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .double_literal = 3.2, .type = NECRO_AST_CONSTANT_FLOAT }),
                                NECRO_BIN_OP_APPEND,
                                necro_intern_string(&intern, "<>")),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .double_literal = 44.4, .type = NECRO_AST_CONSTANT_FLOAT }),
                            NECRO_BIN_OP_BACK_PIPE,
                            necro_intern_string(&intern, "|>")),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("BinOp2", "binop2 = 1000.0 <> 3.2 |> 44.4\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "initialValue"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "initialValue"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER }),
                            NECRO_BIN_OP_ADD,
                            necro_intern_string(&intern, "+")),

                        null_local_ptr),
                    necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER })),
                null_local_ptr);
        necro_parse_ast_test("InitialValue", "initialValue ~ 10 = initialValue + 1\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "ifThenElse"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_if_then_else(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "True"), NECRO_CON_VAR),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .double_literal = 0.0, .type = NECRO_AST_CONSTANT_FLOAT }),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .double_literal = 1.0, .type = NECRO_AST_CONSTANT_FLOAT })),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("IfThenElse", "ifThenElse = if True then 0.0 else 1.0\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "apats"),

                    necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "f"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            null_local_ptr)),

                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_fexpr(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "f"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),

                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("Apats", "apats f x = f x\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "letX"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_let(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"),
                                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc, necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Nothing"), NECRO_CON_VAR), null_local_ptr),
                                    null_local_ptr),
                                null_local_ptr)),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Let", "letX = let x = Nothing in x\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "letX"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_let(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"),
                                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc, necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Nothing"), NECRO_CON_VAR), null_local_ptr),
                                    null_local_ptr),
                                null_local_ptr)),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);


        const char* test_code = ""
            "letX =\n"
            "   let x = Nothing in x\n";
        necro_parse_ast_test("Multi-line Let", test_code, &intern, &ast);
    }


    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "letX"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_let(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"),
                                    necro_parse_ast_create_rhs(
                                        &ast.arena,
                                        zero_loc,
                                        zero_loc,
                                        necro_parse_ast_create_let(&ast.arena, zero_loc, zero_loc,
                                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "y"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                            necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                                                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "y"),
                                                    necro_parse_ast_create_rhs(
                                                        &ast.arena,
                                                        zero_loc,
                                                        zero_loc,
                                                        necro_parse_ast_create_conid(
                                                            &ast.arena,
                                                            zero_loc,
                                                            zero_loc,
                                                            necro_intern_string(&intern, "Nothing"),
                                                            NECRO_CON_VAR
                                                        ),
                                                        null_local_ptr
                                                    ),
                                                    null_local_ptr
                                                ),
                                                null_local_ptr
                                            )
                                        ),
                                        null_local_ptr
                                    ),
                                    null_local_ptr
                                ),
                                null_local_ptr
                            )
                        ),

                        null_local_ptr
                    ),
                    null_local_ptr
                ),
                null_local_ptr
            );
        necro_parse_ast_test("Nested Let", "letX = let x = let y = Nothing in y in x\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "letX"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_let(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"),
                                    necro_parse_ast_create_rhs(
                                        &ast.arena,
                                        zero_loc,
                                        zero_loc,
                                        necro_parse_ast_create_let(&ast.arena, zero_loc, zero_loc,
                                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "y"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                            necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                                                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "y"),
                                                    necro_parse_ast_create_rhs(
                                                        &ast.arena,
                                                        zero_loc,
                                                        zero_loc,
                                                        necro_parse_ast_create_conid(
                                                            &ast.arena,
                                                            zero_loc,
                                                            zero_loc,
                                                            necro_intern_string(&intern, "Nothing"),
                                                            NECRO_CON_VAR
                                                        ),
                                                        null_local_ptr
                                                    ),
                                                    null_local_ptr
                                                ),
                                                null_local_ptr
                                            )
                                        ),
                                        null_local_ptr
                                    ),
                                    null_local_ptr
                                ),
                                null_local_ptr
                            )
                        ),

                        null_local_ptr
                    ),
                    null_local_ptr
                ),
                null_local_ptr
            );

        const char * test_code = ""
            "letX = \n"
            "   let x = let y = Nothing in y\n"
            "   in x\n";
        necro_parse_ast_test("Nested Multi-line Let", test_code, &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "wildCard"),

                    necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_wildcard(&ast.arena, zero_loc, zero_loc),
                        null_local_ptr),

                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 333, .type = NECRO_AST_CONSTANT_INTEGER }),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("WildCard", "wildCard _ = 333\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "giveUsLambdas"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_lambda(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "w"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                null_local_ptr),
                            necro_parse_ast_create_fexpr(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "w"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .char_literal = 'a', .type = NECRO_AST_CONSTANT_CHAR }))),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Lambda", "giveUsLambdas = \\w -> w \'a\'\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_pat_assignment(&ast.arena, zero_loc, zero_loc,

                    necro_parse_ast_create_tuple(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "l"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "r"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                null_local_ptr))),

                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_tuple(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "True"), NECRO_CON_VAR),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "False"), NECRO_CON_VAR),
                                    null_local_ptr))),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("PatAssignment", "(l, r) = (True, False)\n", &intern, &ast);
    }

    // NOTE: Removing list support for now
    // {
    //     NecroIntern        intern = necro_intern_create();
    //     NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
    //     ast.root                  =
    //         necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
    //             necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "expressionList"),
    //                 necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

    //                     necro_parse_ast_create_expression_list(&ast.arena, zero_loc, zero_loc,
    //                         necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                             necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .symbol = necro_intern_string(&intern, "one"), .type = NECRO_AST_CONSTANT_STRING }),
    //                             necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .symbol = necro_intern_string(&intern, "two"), .type = NECRO_AST_CONSTANT_STRING }),
    //                                 necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .symbol = necro_intern_string(&intern, "three"), .type = NECRO_AST_CONSTANT_STRING }),
    //                                     null_local_ptr)))),


    //                     null_local_ptr),
    //                 null_local_ptr),
    //             null_local_ptr);
    //     necro_parse_ast_test("List", "expressionList = [\"one\", \"two\", \"three\"]\n", &intern, &ast);
    // }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "expressionArray"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_expression_array(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .char_literal = '1', .type = NECRO_AST_CONSTANT_CHAR }),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .char_literal = '2', .type = NECRO_AST_CONSTANT_CHAR }),
                                    necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .char_literal = '3', .type = NECRO_AST_CONSTANT_CHAR }),
                                        null_local_ptr)))),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Array", "expressionArray = {\'1\', \'2\', \'3\'}\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "leftSection"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_op_left_section(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER }),
                            NECRO_BIN_OP_ADD,
                            necro_intern_string(&intern, "+")),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("LeftSection", "leftSection = (10 +)\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "rightSection"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_op_right_section(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 100, .type = NECRO_AST_CONSTANT_INTEGER }),
                            NECRO_BIN_OP_DIV,
                            necro_intern_string(&intern, "/")),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("RightSection", "rightSection = (/ 100)\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "coolSeq"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_seq_expr(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER }),
                                    necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "rest"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                        necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 3, .type = NECRO_AST_CONSTANT_INTEGER }),
                                            null_local_ptr)))), NECRO_SEQUENCE_SEQUENCE),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Seq", "coolSeq = [0 1 _ 3]\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constructor1"),

                    necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constructor(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Just"), NECRO_CON_VAR),
                            necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_constructor(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Just"), NECRO_CON_VAR),
                                    necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                        null_local_ptr)),
                                null_local_ptr)),
                        null_local_ptr),


                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("Constructor1", "constructor1 (Just (Just x)) = x\n", &intern, &ast);
    }

    // {
    //     NecroIntern        intern = necro_intern_create();
    //     NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
    //     ast.root                  =
    //         necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
    //             necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constructor2"),

    //                 necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
    //                     necro_parse_ast_create_expression_list(&ast.arena, zero_loc, zero_loc,
    //                         necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                             necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
    //                             necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
    //                                 necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 2, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
    //                                     null_local_ptr)))),
    //                     null_local_ptr),

    //                 necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
    //                     necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER }),
    //                     null_local_ptr)),

    //             null_local_ptr);
    //     necro_parse_ast_test("Constructor2", "constructor2 [0, 1, 2] = 1\n", &intern, &ast);
    // }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "constructor3"),

                    necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_tuple(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "y"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                    necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "z"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                        null_local_ptr)))),
                        null_local_ptr),

                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "z"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("Constructor3", "constructor3 (x, y, z) = z\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "binOpSym"),

                    necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_bin_op_sym(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, ":"), NECRO_CON_VAR),
                            necro_parse_ast_create_bin_op_sym(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "y"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, ":"), NECRO_CON_VAR),
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "xs"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER))),
                        null_local_ptr),

                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "xs"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("BinOpSym", "binOpSym (x : y : xs) = xs\n", &intern, &ast);
    }


    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "arithmeticSequence1"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_arithmetic_sequence(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }),
                            null_local_ptr,
                            null_local_ptr,
                            NECRO_ARITHMETIC_ENUM_FROM),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ArithmeticSequence1", "arithmeticSequence1 = [0..]\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "arithmeticSequence2"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_arithmetic_sequence(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }),
                            null_local_ptr,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 100, .type = NECRO_AST_CONSTANT_INTEGER }),
                            NECRO_ARITHMETIC_ENUM_FROM),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ArithmeticSequence2", "arithmeticSequence2 = [0..100]\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "arithmeticSequence3"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_arithmetic_sequence(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 100, .type = NECRO_AST_CONSTANT_INTEGER }),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1000, .type = NECRO_AST_CONSTANT_INTEGER }),
                            NECRO_ARITHMETIC_ENUM_FROM),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("ArithmeticSequence3", "arithmeticSequence3 = [0, 100..1000]\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "case1"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_case(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Nothing"), NECRO_CON_VAR),
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_case_alternative(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Nothing"), NECRO_CON_VAR),
                                    necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER })),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_case_alternative(&ast.arena, zero_loc, zero_loc,
                                        necro_parse_ast_create_constructor(&ast.arena, zero_loc, zero_loc,
                                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Just"), NECRO_CON_VAR),
                                            necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "i"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                                null_local_ptr)),
                                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "i"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                                    null_local_ptr))),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Case1", "case1 =\n  case Nothing of\n    Nothing -> 0\n    Just i  -> i\n", &intern, &ast);
    }

    // TODO: Reimplement for Array patterns
    // {
    //     NecroIntern        intern = necro_intern_create();
    //     NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
    //     ast.root                  =
    //         necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
    //             necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "case2"),
    //                 necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

    //                     necro_parse_ast_create_case(&ast.arena, zero_loc, zero_loc,
    //                         necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                         necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                             necro_parse_ast_create_case_alternative(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_expression_list(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                         necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
    //                                         necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                             necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
    //                                             necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                                 necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 2, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
    //                                                 null_local_ptr)))),
    //                                 necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER })),
    //                             necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_case_alternative(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_bin_op_sym(&ast.arena, zero_loc, zero_loc,
    //                                         necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "z"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                                         necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, ":"), NECRO_CON_VAR),
    //                                         necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "zs"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "z"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
    //                                 null_local_ptr))),

    //                     null_local_ptr),
    //                 null_local_ptr),
    //             null_local_ptr);
    //     necro_parse_ast_test("Case2", "case2 =\n  case x of\n    [0, 1, 2] -> 1\n    z : zs    -> z\n", &intern, &ast);
    // }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "forLoopTest"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_for_loop(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "each"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER }),
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "i"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER }),
                                NECRO_BIN_OP_ADD,
                                necro_intern_string(&intern, "+"))),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("forLoopTest", "forLoopTest =\n  loop x = 0 for i <- each do\n    x + 1\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "decl"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_bin_op(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 3, .type = NECRO_AST_CONSTANT_INTEGER }),
                            NECRO_BIN_OP_SUB,
                            necro_intern_string(&intern, "-")),


                        necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "x"),
                                necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1000, .type = NECRO_AST_CONSTANT_INTEGER }),
                                    null_local_ptr),
                                null_local_ptr),
                            null_local_ptr)),

                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Declaration", "decl = x - 3\n  where\n    x = 1000\n", &intern, &ast);
    }

    // {
    //     NecroIntern        intern = necro_intern_create();
    //     NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
    //     ast.root                  =
    //         necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
    //             necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "doBlock"),
    //                 necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

    //                     necro_parse_ast_create_do(&ast.arena, zero_loc, zero_loc,
    //                         necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                             necro_parse_ast_create_bind_assignment(&ast.arena, zero_loc, zero_loc,
    //                                 necro_intern_string(&intern, "d"),
    //                                 necro_parse_ast_create_fexpr(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "pure"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                                     necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER }))),
    //                             necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_fexpr(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "pure"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "d"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
    //                                 null_local_ptr))),

    //                     null_local_ptr),
    //                 null_local_ptr),
    //             null_local_ptr);
    //     necro_parse_ast_test("Do", "doBlock = do\n  d <- pure 10\n  pure d\n", &intern, &ast);
    // }

    // {
    //     NecroIntern        intern = necro_intern_create();
    //     NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
    //     ast.root                  =
    //         necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
    //             necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "patDo"),
    //                 necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

    //                     necro_parse_ast_create_do(&ast.arena, zero_loc, zero_loc,
    //                         necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                             necro_parse_ast_create_pat_bind_assignment(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_constructor(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Just"), NECRO_CON_VAR),
    //                                     necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
    //                                         necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "p"), NECRO_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                                         null_local_ptr)),
    //                                 necro_parse_ast_create_fexpr(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "pure"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                                     necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 100, .type = NECRO_AST_CONSTANT_INTEGER }))),

    //                             necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
    //                                 necro_parse_ast_create_fexpr(&ast.arena, zero_loc, zero_loc,
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "pure"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
    //                                     necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "p"), NECRO_VAR_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
    //                                 null_local_ptr))),

    //                     null_local_ptr),
    //                 null_local_ptr),
    //             null_local_ptr);
    //     necro_parse_ast_test("PatDo", "patDo = do\n  Just p <- pure 100\n  pure p\n", &intern, &ast);
    // }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,

                necro_parse_ast_create_data_declaration(&ast.arena, zero_loc, zero_loc,

                    necro_parse_ast_create_simple_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Either"), NECRO_CON_TYPE_DECLARATION),
                        necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_VAR_DECLARATION, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                null_local_ptr))),

                    necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_constructor(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Left"), NECRO_CON_DATA_DECLARATION),
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                null_local_ptr)),
                        necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_constructor(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Right"), NECRO_CON_DATA_DECLARATION),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                    null_local_ptr)),
                            null_local_ptr))),

                null_local_ptr);
        necro_parse_ast_test("Data", "data Either a b = Left a | Right b\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,

                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "typeSig1"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    null_local_ptr,
                    necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_type_app(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Maybe"), NECRO_CON_TYPE_VAR),
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Int"), NECRO_CON_TYPE_VAR)),
                        necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Int"), NECRO_CON_TYPE_VAR)),
                    NECRO_SIG_DECLARATION),

                null_local_ptr);
        necro_parse_ast_test("TypeSignature1", "typeSig1 :: Maybe Int -> Int\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,

                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "typeSig2"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    necro_parse_ast_create_class_context(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Num"), NECRO_CON_TYPE_VAR),
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                    necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                    NECRO_SIG_DECLARATION),

                null_local_ptr);
        necro_parse_ast_test("TypeSignature2", "typeSig2 :: Num a => a -> a\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,

                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "typeSig4"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_class_context(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Num"), NECRO_CON_TYPE_VAR),
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                        necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_class_context(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Show"), NECRO_CON_TYPE_VAR),
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                            null_local_ptr)),
                    necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER))),
                    NECRO_SIG_DECLARATION),

                null_local_ptr);
        necro_parse_ast_test("TypeSignature4", "typeSig4 :: (Num a, Show b) => a -> b -> b\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "attr"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    null_local_ptr,
                    necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_type_attribute(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            NECRO_TYPE_ATTRIBUTE_STAR),
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                    NECRO_SIG_DECLARATION),
                null_local_ptr);
        necro_parse_ast_test("Attribute1", "attr :: *a -> b\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "attr"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    null_local_ptr,
                    necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_type_attribute(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_type_app(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Maybe"), NECRO_CON_TYPE_VAR),
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                            NECRO_TYPE_ATTRIBUTE_STAR),
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                    NECRO_SIG_DECLARATION),
                null_local_ptr);
        necro_parse_ast_test("Attribute2", "attr :: *Maybe a -> b\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "attr"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    null_local_ptr,
                    necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                        necro_parse_ast_create_type_attribute(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_type_app(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Maybe"), NECRO_CON_TYPE_VAR),
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                            NECRO_TYPE_ATTRIBUTE_STAR)),
                    NECRO_SIG_DECLARATION),
                null_local_ptr);
        necro_parse_ast_test("Attribute3", "attr :: a -> *Maybe b\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "attr"), NECRO_VAR_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    null_local_ptr,
                    necro_parse_ast_create_type_attribute(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                                necro_parse_ast_create_type_app(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Maybe"), NECRO_CON_TYPE_VAR),
                                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "b"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER))),
                            NECRO_TYPE_ATTRIBUTE_STAR),
                    NECRO_SIG_DECLARATION),
                null_local_ptr);
        necro_parse_ast_test("Attribute4", "attr :: *(a -> Maybe b)\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,

                necro_parse_ast_create_class_declaration(&ast.arena, zero_loc, zero_loc,
                    necro_parse_ast_create_class_context(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Eq"), NECRO_CON_TYPE_VAR),
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "HaveSomeClass"), NECRO_CON_TYPE_DECLARATION),
                    necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                    necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_type_signature(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "methodToTheMadness"), NECRO_VAR_CLASS_SIG, null_local_ptr, NECRO_TYPE_ZERO_ORDER),
                            null_local_ptr,
                            necro_parse_ast_create_function_type(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Int"), NECRO_CON_TYPE_VAR),
                                necro_parse_ast_create_var(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "a"), NECRO_VAR_TYPE_FREE_VAR, null_local_ptr, NECRO_TYPE_ZERO_ORDER)),
                            NECRO_SIG_TYPE_CLASS),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("TypeClass", "class Eq a => HaveSomeClass a where\n  methodToTheMadness :: Int -> a\n", &intern, &ast);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,

                necro_parse_ast_create_instance(&ast.arena, zero_loc, zero_loc,
                    null_local_ptr,
                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "HaveSomeClass"), NECRO_CON_TYPE_VAR),
                    necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "Bool"), NECRO_CON_TYPE_VAR),
                    necro_parse_ast_create_decl(&ast.arena, zero_loc, zero_loc,
                        necro_parse_ast_create_apats_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "methodToTheMadness"),
                            necro_parse_ast_create_apats(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_wildcard(&ast.arena, zero_loc, zero_loc),
                                null_local_ptr),
                            necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_conid(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "True"), NECRO_CON_VAR),
                                null_local_ptr)),
                        null_local_ptr)),

                null_local_ptr);
        necro_parse_ast_test("Instance", "instance HaveSomeClass Bool where\n  methodToTheMadness _ = True\n", &intern, &ast);
    }

    {
        const char* test_name   = "Instance Declaration Error";
        const char* test_source = ""
            "class NumCollection c where\n"
            "  checkOutMyCollection :: Num a => a -> c a -> c a\n"
            "instance NumCollection (Array 1) where\n"
            "  checkOutMyCollection x c = c\n";
        necro_parse_ast_test_error(test_name, test_source, NECRO_PARSE_ERROR);
    }

    {
        NecroIntern        intern = necro_intern_create();
        NecroParseAstArena ast    = necro_parse_ast_arena_create(100 * sizeof(NecroParseAst));
        ast.root                  =
            necro_parse_ast_create_top_decl(&ast.arena, zero_loc, zero_loc,
                necro_parse_ast_create_simple_assignment(&ast.arena, zero_loc, zero_loc, necro_intern_string(&intern, "unboxedTuple"),
                    necro_parse_ast_create_rhs(&ast.arena, zero_loc, zero_loc,

                        necro_parse_ast_create_unboxed_tuple(&ast.arena, zero_loc, zero_loc,
                            necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 3, .type = NECRO_AST_CONSTANT_INTEGER }),
                                necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                    necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 2, .type = NECRO_AST_CONSTANT_INTEGER }),
                                    necro_parse_ast_create_list(&ast.arena, zero_loc, zero_loc,
                                        necro_parse_ast_create_constant(&ast.arena, zero_loc, zero_loc, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER }),
                                        null_local_ptr)))),

                        null_local_ptr),
                    null_local_ptr),
                null_local_ptr);
        necro_parse_ast_test("Unboxed Tuple test", "unboxedTuple = (#3, 2, 1#)\n", &intern, &ast);
    }

    /* { */
    /*     puts("Parse {{{ child process parse_test:  starting..."); */
    /*     assert(NECRO_COMPILE_IN_CHILD_PROCESS("parse_test.necro", "parse") == 0); */
    /*     puts("Parse }}} child process parse_test:  passed\n"); */
    /* } */
    /*  */
    /* { */
    /*     puts("Parse {{{ child process parseTest:  starting..."); */
    /*     assert(NECRO_COMPILE_IN_CHILD_PROCESS("parseTest.necro", "parse") == 0); */
    /*     puts("Parse }}} child process parseTest:  passed\n"); */
    /* } */
    /*  */
    /* { */
    /*     puts("Parse {{{ child process parseErrorTest:  starting..."); */
    /*     assert(NECRO_COMPILE_IN_CHILD_PROCESS("parseErrorTest.necro", "parse") == 0); */
    /*     puts("Parse }}} child process parseErrorTest:  passed\n"); */
    /* } */
    /*  */

    // {
    //     puts("Parse {{{ child process parse_test:  starting...");
    //     assert(NECRO_COMPILE_IN_CHILD_PROCESS("parse_test_lets.necro", "parse") == 0);
    //     puts("Parse }}} child process parse_test:  passed\n");
    // }

}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "necro.h"
#include "lexer.h"
#include "parser.h"

void necro_test_lex(char* input_string)
{
	//const char*   input_string = "+-*/\n!?-><=>=";
	printf("input_string:\n%s\n\n", input_string);
	NecroLexState lex_state = necro_create_lex_state(input_string);
	necro_lex(&lex_state);
	necro_print_lex_state(&lex_state);
	necro_destroy_lex_state(&lex_state);
	if (lex_state.tokens.length > 0 && parse_expression(&lex_state.tokens.data, lex_state.tokens.length))
	{
		puts("Parse succeeded");
	}
	else
	{
		puts("Parse failed");
	}
}

//=====================================================
// Parsing
//=====================================================

typedef enum
{
	LAMBDA_AST_VARIABLE,
	LAMBDA_AST_CONSTANT,
	LAMBDA_AST_APPLY,
	LAMBDA_AST_LAMBDA
} LAMBDA_AST;


//=====================================================
// TypeChecking
//=====================================================


//=====================================================
// Main
//=====================================================
int main(int argc, char** argv)
{
	int i;
	for (i = 1; i < argc; ++i)
	{
		necro_test_lex(argv[i]);
	}
	return 0;
}

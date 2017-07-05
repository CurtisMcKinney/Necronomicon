/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "necro.h"
#include "lexer.h"
#include "parser.h"
#include "intern.h"

void necro_test_lex(char* input_string)
{
	//const char*   input_string = "+-*/\n!?-><=>=";
	printf("input_string:\n%s\n\n", input_string);
	NecroLexState lex_state = necro_create_lex_state(input_string);
	necro_lex(&lex_state);
	necro_print_lex_state(&lex_state);
	necro_destroy_lex_state(&lex_state);
	NecroAST ast = { { 0, 0, 0 } };
	ast_prealloc(&ast, lex_state.tokens.length);
	if (lex_state.tokens.length > 0 && parse_ast(&lex_state.tokens.data, lex_state.tokens.length, &ast) == ParseSuccessful)
	{
		puts("Parse succeeded");
		print_ast(&ast);
	}
	else
	{
		puts("Parse failed");
	}
	destruct_arena(&ast.arena);
}

//=====================================================
// Main
//=====================================================
int main(int32_t argc, char** argv)
{
	if (argc > 1 && strcmp(argv[1], "-test_intern") == 0)
	{
		necro_test_intern();
		return 0;
	}

	for (int32_t i = 1; i < argc; ++i)
	{
		necro_test_lex(argv[i]);
	}
	return 0;
}

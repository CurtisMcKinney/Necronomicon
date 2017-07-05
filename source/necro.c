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
	printf("input_string:\n%s\n\n", input_string);

	NecroLexer lexer = necro_create_lexer(input_string);
	necro_lex(&lexer);
	necro_print_lexer(&lexer);

	NecroAST ast = { { 0, 0, 0 } };
	ast_prealloc(&ast, lexer.tokens.length);
	if (lexer.tokens.length > 0 && parse_ast(&lexer.tokens.data, lexer.tokens.length, &ast) == ParseSuccessful)
	{
		puts("Parse succeeded");
		print_ast(&ast);
	}
	else
	{
		puts("Parse failed");
	}

	// Cleanup
	necro_destroy_lexer(&lexer);
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

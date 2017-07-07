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
	// printf("input_string:\n%s\n\n", input_string);
	puts("--------------------------------");
	puts("-- Lexing");
	puts("--------------------------------");

	NecroLexer       lexer      = necro_create_lexer(input_string);
	NECRO_LEX_RESULT lex_result = necro_lex(&lexer);
	necro_print_lexer(&lexer);

	puts("--------------------------------");
	puts("-- Parsing");
	puts("--------------------------------");

	NecroAST ast = { construct_arena(lexer.tokens.length * sizeof(NecroAST_Node)) };
    NecroLexToken* tokens = lexer.tokens.data;
	if (lex_result == NECRO_LEX_RESULT_SUCCESSFUL &&
		lexer.tokens.length > 0 &&
		parse_ast(&tokens, &ast) == ParseSuccessful)
	{
		puts("Parse succeeded");
		print_ast(&ast);
	}
	else
	{
		puts("Parse failed");
	}

	// Cleanup
	puts("--------------------------------");
	puts("-- Cleaning Up");
	puts("--------------------------------");

	necro_destroy_lexer(&lexer);
	destruct_arena(&ast.arena);
}

//=====================================================
// Main
//=====================================================
int main(int32_t argc, char** argv)
{
    if (argc > 1 && strcmp(argv[1], "-test_lexer") == 0)
    {
        necro_test_lexer();
        return 0;
    }

	if (argc > 1 && strcmp(argv[1], "-test_intern") == 0)
	{
		necro_test_intern();
		return 0;
	}

	if (argc == 2)
	{
		FILE* file = fopen(argv[1], "r");
		if (!file)
		{
			fprintf(stderr, "Could not open file: %s\n", argv[1]);
			exit(1);
		}

		char*  str    = NULL;
		size_t length = 0;

		// Find length of files
		fseek(file, 0, SEEK_END);
		length = ftell(file);
		fseek(file, 0, SEEK_SET);

		// Allocate buffer
		str = malloc(length + 2);
		if (str)
		{
			// read contents of buffer
			length = fread(str, 1, length, file);
			str[length]     = '\n';
			str[length + 1] = '\0';

            // // Debug printout raw ascii codes
            // for (char* c = str; *c; ++c)
            // {
            //     printf("%d\n", (uint8_t)*c);
            // }

			// Lex buffer
			necro_test_lex(str);
		}
		else
		{
			fprintf(stderr, "Null character buffer.\n");
		}
		fclose(file);
        free(str);
	}
	else
	{
		fprintf(stderr, "Incorrect necro usage. Should be: necro filename\n");
	}

	return 0;
}

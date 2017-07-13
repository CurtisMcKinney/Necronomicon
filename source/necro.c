/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "necro.h"
#include "lexer.h"
#include "parser.h"
#include "intern.h"
#include "runtime.h"

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
    NecroAST_LocalPtr root_node_ptr = null_local_ptr;
    if (lex_result == NECRO_LEX_RESULT_SUCCESSFUL && lexer.tokens.length > 0)
    {
        char* null_error_message = NULL;
        const size_t current_token = 0;
        NecroParser parser = { null_error_message, &ast, tokens, current_token };
        if (parse_ast(&parser, &root_node_ptr) == ParseSuccessful)
        {
            puts("Parse succeeded");
            print_ast(&ast, &lexer.intern, root_node_ptr);
#if 0
            compute_ast_math(&ast, root_node_ptr);
#endif
        }
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
    if (argc > 1 && strcmp(argv[1], "-test_runtime") == 0)
    {
        necro_test_runtime();
    }
    else if (argc > 1 && strcmp(argv[1], "-test_lexer") == 0)
    {
        necro_test_lexer();
    }
    else if (argc > 1 && strcmp(argv[1], "-test_intern") == 0)
    {
        necro_test_intern();
    }
    else if (argc == 2)
    {
#ifdef WIN32
        FILE* file;
        size_t err = fopen_s(&file, argv[1], "r");
#else
        FILE* file = fopen(argv[1], "r");
#endif
        if (!file)
        {
            fprintf(stderr, "Could not open file: %s\n", argv[1]);
            exit(1);
        }

        char*  str    = NULL;
        size_t length = 0;

        // Find length of file
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

        // Cleanup
        fclose(file);
        free(str);
    }
    else
    {
        fprintf(stderr, "Incorrect necro usage. Should be: necro filename\n");
    }

    return 0;
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "driver.h"
#include "necro.h"

//=====================================================
// Main
//=====================================================
int main(int32_t argc, char** argv)
{
    if (argc == 3 && strcmp(argv[1], "-test") == 0)
    {
        if (strcmp(argv[2], "all") == 0)
        {
            necro_test(NECRO_TEST_ALL);
        }
        else if (strcmp(argv[2], "arena_chain_table") == 0)
        {
            necro_test(NECRO_TEST_ARENA_CHAIN_TABLE);
        }
        else if (strcmp(argv[2], "type") == 0)
        {
            necro_test(NECRO_TEST_TYPE);
        }
        else if (strcmp(argv[2], "infer") == 0)
        {
            necro_test(NECRO_TEST_INFER);
        }
        else if (strcmp(argv[2], "vm") == 0)
        {
            necro_test(NECRO_TEST_VM);
        }
        else if (strcmp(argv[2], "dvm") == 0)
        {
            necro_test(NECRO_TEST_DVM);
        }
        else if (strcmp(argv[2], "symtable") == 0)
        {
            necro_test(NECRO_TEST_DVM);
        }
        else if (strcmp(argv[2], "slab") == 0)
        {
            necro_test(NECRO_TEST_SLAB);
        }
        else if (strcmp(argv[2], "treadmill") == 0)
        {
            necro_test(NECRO_TEST_TREADMILL);
        }
        else if (strcmp(argv[2], "lexer") == 0)
        {
            necro_test(NECRO_TEST_LEXER);
        }
        else if (strcmp(argv[2], "intern") == 0)
        {
            necro_test(NECRO_TEST_INTERN);
        }
        else if (strcmp(argv[2], "vault") == 0)
        {
            necro_test(NECRO_TEST_VAULT);
        }
        else if (strcmp(argv[2], "archive") == 0)
        {
            necro_test(NECRO_TEST_ARCHIVE);
        }
        else if (strcmp(argv[2], "region") == 0)
        {
            necro_test(NECRO_TEST_REGION);
        }
    }
    else if (argc == 2 || argc == 3 || argc == 4)
    {
#ifdef WIN32
        FILE* file;
        fopen_s(&file, argv[1], "r");
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

            if (argc > 2 && strcmp(argv[2], "-lex_pre_layout") == 0)
            {
                necro_compile(str, NECRO_PHASE_LEX_PRE_LAYOUT);
            }
            else if (argc > 2 && strcmp(argv[2], "-lex") == 0)
            {
                necro_compile(str, NECRO_PHASE_LEX);
            }
            else if (argc > 2 && strcmp(argv[2], "-parse") == 0)
            {
                necro_compile(str, NECRO_PHASE_PARSE);
            }
            else if (argc > 2 && strcmp(argv[2], "-reify") == 0)
            {
                necro_compile(str, NECRO_PHASE_REIFY);
            }
            else if (argc > 2 && strcmp(argv[2], "-scope") == 0)
            {
                necro_compile(str, NECRO_PHASE_BUILD_SCOPES);
            }
            else if (argc > 2 && strcmp(argv[2], "-rename") == 0)
            {
                necro_compile(str, NECRO_PHASE_RENAME);
            }
            else if (argc > 2 && strcmp(argv[2], "-dependency") == 0)
            {
                necro_compile(str, NECRO_PHASE_DEPENDENCY_ANALYSIS);
            }
            else if (argc > 2 && strcmp(argv[2], "-infer") == 0)
            {
                necro_compile(str, NECRO_PHASE_INFER);
            }
            else if (argc > 2 && strcmp(argv[2], "-core") == 0)
            {
                necro_compile(str, NECRO_PHASE_TRANSFORM_TO_CORE);
            }
            else if (argc > 2 && strcmp(argv[2], "-sa") == 0)
            {
                necro_compile(str, NECRO_PHASE_STATE_ANALYSIS);
            }
            else if (argc > 2 && strcmp(argv[2], "-cc") == 0)
            {
                necro_compile(str, NECRO_PHASE_CLOSURE_CONVERSION);
            }
            else if (argc > 2 && strcmp(argv[2], "-machine") == 0)
            {
                necro_compile(str, NECRO_PHASE_TRANSFORM_TO_MACHINE);
            }
            else if (argc > 2 && strcmp(argv[2], "-codegen") == 0)
            {
                if (argc > 3 &&  strcmp(argv[3], "-opt") == 0)
                    necro_compile_opt(str, NECRO_PHASE_CODEGEN);
                else
                    necro_compile(str, NECRO_PHASE_CODEGEN);
            }
            else if (argc > 2 && strcmp(argv[2], "-jit") == 0)
            {
                if (argc > 3 &&  strcmp(argv[3], "-opt") == 0)
                    necro_compile_opt(str, NECRO_PHASE_JIT);
                else
                    necro_compile(str, NECRO_PHASE_JIT);
            }
            else
            {
                necro_compile(str, NECRO_PHASE_TRANSFORM_TO_CORE);
            }

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

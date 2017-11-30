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
    if (argc == 2 && strcmp(argv[1], "-test_all") == 0)
    {
        necro_test(NECRO_TEST_ALL);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_vm") == 0)
    {
        necro_test(NECRO_TEST_VM);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_dvm") == 0)
    {
        necro_test(NECRO_TEST_DVM);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_symtable") == 0)
    {
        necro_test(NECRO_TEST_DVM);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_slab") == 0)
    {
        necro_test(NECRO_TEST_SLAB);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_treadmill") == 0)
    {
        necro_test(NECRO_TEST_TREADMILL);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_lexer") == 0)
    {
        necro_test(NECRO_TEST_LEXER);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_intern") == 0)
    {
        necro_test(NECRO_TEST_INTERN);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_vault") == 0)
    {
        necro_test(NECRO_TEST_VAULT);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_archive") == 0)
    {
        necro_test(NECRO_TEST_ARCHIVE);
    }
    else if (argc == 2 && strcmp(argv[1], "-test_region") == 0)
    {
        necro_test(NECRO_TEST_REGION);
    }
    else if (argc == 2 || argc == 3)
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

            if (argc > 2 && strcmp(argv[2], "-lex") == 0)
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
            else if (argc > 2 && strcmp(argv[2], "-rename") == 0)
            {
                necro_compile(str, NECRO_PHASE_RENAME);
            }
            else
            {
                necro_compile(str, NECRO_PHASE_ALL);
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

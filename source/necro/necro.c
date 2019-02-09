/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "driver.h"
#include "necro.h"
#include "unicode_properties.h"

//=====================================================
// Main
//=====================================================
int main(int32_t argc, char** argv)
{
    ENABLE_AUTO_MEM_CHECK();
    if (argc == 3 && strcmp(argv[2], "-unicode_p") == 0)
    {
        necro_unicode_property_parse(argv[1]);
    }
    else if (argc == 3 && strcmp(argv[1], "-test") == 0)
    {
        if (strcmp(argv[2], "all") == 0)
        {
            necro_test(NECRO_TEST_ALL);
        }
        else if (strcmp(argv[2], "parser") == 0)
        {
            necro_test(NECRO_TEST_PARSER);
        }
        else if (strcmp(argv[2], "unicode") == 0)
        {
            necro_test(NECRO_TEST_UNICODE);
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
        else if (strcmp(argv[2], "lexer") == 0)
        {
            necro_test(NECRO_TEST_LEXER);
        }
        else if (strcmp(argv[2], "intern") == 0)
        {
            necro_test(NECRO_TEST_INTERN);
        }
        else if (strcmp(argv[2], "rename") == 0)
        {
            necro_test(NECRO_TEST_RENAME);
        }
        else if (strcmp(argv[2], "base") == 0)
        {
            necro_test(NECRO_TEST_BASE);
        }
    }
    else if (argc == 2 || argc == 3 || argc == 4)
    {
        const char* file_name = argv[1];
#ifdef WIN32
        FILE* file;
        fopen_s(&file, file_name, "r");
#else
        FILE* file = fopen(argv[1], "r");
#endif
        if (!file)
        {
            // TODO: Error handling
            fprintf(stderr, "Could not open file: %s\n", file_name);
            necro_exit(1);
        }

        char*  str    = NULL;
        size_t length = 0;

        // Find length of file
        fseek(file, 0, SEEK_END);
        length = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Allocate buffer
        str = emalloc(length + 2);

        // read contents of buffer
        length = fread(str, 1, length, file);
        str[length]     = '\n';
        str[length + 1] = '\0';

        if (argc > 2 && strcmp(argv[2], "-lex") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_LEX, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-parse") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_PARSE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-reify") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_REIFY, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-scope") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_BUILD_SCOPES, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-rename") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_RENAME, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-dep") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_DEPENDENCY_ANALYSIS, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-infer") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_INFER, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-type_class") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TYPE_CLASS_TRANSLATE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-core") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TRANSFORM_TO_CORE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-ll") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_LAMBDA_LIFT, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-cc") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_CLOSURE_CONVERSION, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-sa") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_STATE_ANALYSIS, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-machine") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TRANSFORM_TO_MACHINE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-codegen") == 0)
        {
            if (argc > 3 &&  strcmp(argv[3], "-opt") == 0)
                necro_compile(file_name, str, length, NECRO_PHASE_CODEGEN, NECRO_OPT_ON);
            else
                necro_compile(file_name, str, length, NECRO_PHASE_CODEGEN, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-jit") == 0)
        {
            if (argc > 3 &&  strcmp(argv[3], "-opt") == 0)
                necro_compile(file_name, str, length, NECRO_PHASE_JIT, NECRO_OPT_ON);
            else
                necro_compile(file_name, str, length, NECRO_PHASE_JIT, NECRO_OPT_OFF);
        }
        else
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TRANSFORM_TO_CORE, NECRO_OPT_OFF);
        }

        // Cleanup
        fclose(file);
        free(str);
    }
    else
    {
        fprintf(stderr, "Incorrect necro usage. Should be: necro filename\n");
    }

    MEM_CHECK();
    return 0;
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

//#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "driver.h"
#include "necro.h"
#include "unicode_properties.h"
#include "base.h"

//=====================================================
// Main
//=====================================================
int main(int32_t argc, char** argv)
{
    ENABLE_AUTO_MEM_CHECK();

    necro_base_global_init();
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
        else if (strcmp(argv[2], "parser") == 0 || strcmp(argv[2], "parse") == 0)
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
        else if (strcmp(argv[2], "lexer") == 0 || strcmp(argv[2], "lex") == 0)
        {
            necro_test(NECRO_TEST_LEXER);
        }
        else if (strcmp(argv[2], "intern") == 0)
        {
            necro_test(NECRO_TEST_INTERN);
        }
        else if (strcmp(argv[2], "rename") == 0 || strcmp(argv[2], "renamer") == 0)
        {
            necro_test(NECRO_TEST_RENAME);
        }
        else if (strcmp(argv[2], "base") == 0)
        {
            necro_test(NECRO_TEST_BASE);
        }
        else if (strcmp(argv[2], "monomorphize") == 0)
        {
            necro_test(NECRO_TEST_MONOMORPHIZE);
        }
        else if (strcmp(argv[2], "alias") == 0)
        {
            necro_test(NECRO_TEST_ALIAS);
        }
        else if (strcmp(argv[2], "core") == 0)
        {
            necro_test(NECRO_TEST_CORE);
        }
        else if (strcmp(argv[2], "psimpl") == 0)
        {
            necro_test(NECRO_TEST_PRE_SIMPLIFY);
        }
        else if (strcmp(argv[2], "lift") == 0 || strcmp(argv[2], "ll") == 0)
        {
            necro_test(NECRO_TEST_LAMBDA_LIFT);
        }
        else if (strcmp(argv[2], "defunc") == 0)
        {
            necro_test(NECRO_TEST_DEFUNCTIONALIZE);
        }
        else if (strcmp(argv[2], "sa") == 0)
        {
            necro_test(NECRO_TEST_STATE_ANALYSIS);
        }
        else if (strcmp(argv[2], "machine") == 0 || strcmp(argv[2], "mach") == 0)
        {
            necro_test(NECRO_TEST_MACH);
        }
        else if (strcmp(argv[2], "machine") == 0 || strcmp(argv[2], "mach") == 0)
        {
            necro_test(NECRO_TEST_MACH);
        }
        else if (strcmp(argv[2], "llvm") == 0)
        {
            necro_test(NECRO_TEST_LLVM);
        }
        else if (strcmp(argv[2], "jit") == 0)
        {
            necro_test(NECRO_TEST_JIT);
        }
        else if (strcmp(argv[2], "compile") == 0)
        {
            necro_test(NECRO_TEST_COMPILE);
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
        fclose(file);

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
        else if (argc > 2 && strcmp(argv[2], "-monomorphize") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_MONOMORPHIZE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-core") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TRANSFORM_TO_CORE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-ll") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_LAMBDA_LIFT, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-defunc") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_DEFUNCTIONALIZATION, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-sa") == 0)
        {
            necro_compile(file_name, str, length, NECRO_PHASE_STATE_ANALYSIS, NECRO_OPT_OFF);
        }
        else if ((argc > 2 && strcmp(argv[2], "-machine") == 0) || (argc > 2 && strcmp(argv[2], "-mach") == 0))
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TRANSFORM_TO_MACHINE, NECRO_OPT_OFF);
        }
        else if (argc > 2 && strcmp(argv[2], "-llvm") == 0)
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
        else if (argc > 2 && strcmp(argv[2], "-compile") == 0)
        {
            if (argc > 3 &&  strcmp(argv[3], "-opt") == 0)
                necro_compile(file_name, str, length, NECRO_PHASE_COMPILE, NECRO_OPT_ON);
            else
                necro_compile(file_name, str, length, NECRO_PHASE_COMPILE, NECRO_OPT_OFF);
        }
        else
        {
            necro_compile(file_name, str, length, NECRO_PHASE_TRANSFORM_TO_CORE, NECRO_OPT_OFF);
        }

        // Cleanup
        free(str);
    }
    else
    {
        fprintf(stderr, "Incorrect necro usage. Should be: necro filename\n");
    }
    necro_base_global_cleanup();

    SCOPED_MEM_CHECK();
    MEM_CHECK();
    return 0;
}

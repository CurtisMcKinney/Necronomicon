/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef DRIVER_H
#define DRIVER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility/result.h"

typedef enum
{
    NECRO_TEST_ALL,
    NECRO_TEST_VM,
    NECRO_TEST_DVM,
    NECRO_TEST_SYMTABLE,
    NECRO_TEST_SLAB,
    NECRO_TEST_TREADMILL,
    NECRO_TEST_LEXER,
    NECRO_TEST_INTERN,
    NECRO_TEST_VAULT,
    NECRO_TEST_ARCHIVE,
    NECRO_TEST_REGION,
    NECRO_TEST_INFER,
    NECRO_TEST_TYPE,
    NECRO_TEST_ARENA_CHAIN_TABLE,
    NECRO_TEST_UNICODE,
} NECRO_TEST;

typedef enum
{
    NECRO_BENCH_ALL,
    NECRO_BENCH_ARCHIVE
} NECRO_BENCH;

typedef enum
{
    NECRO_PHASE_NONE,
    NECRO_PHASE_ALL,
    NECRO_PHASE_LEX,
    NECRO_PHASE_PARSE,
    NECRO_PHASE_REIFY,
    NECRO_PHASE_BUILD_SCOPES,
    NECRO_PHASE_RENAME,
    NECRO_PHASE_DEPENDENCY_ANALYSIS,
    NECRO_PHASE_INFER,
    NECRO_PHASE_TRANSFORM_TO_CORE,
    NECRO_PHASE_LAMBDA_LIFT,
    NECRO_PHASE_CLOSURE_CONVERSION,
    NECRO_PHASE_STATE_ANALYSIS,
    NECRO_PHASE_TRANSFORM_TO_MACHINE,
    NECRO_PHASE_CODEGEN,
    NECRO_PHASE_JIT
} NECRO_PHASE;

struct NecroTimer;
typedef struct
{
    size_t             verbosity;
    struct NecroTimer* timer;
    NECRO_PHASE        compilation_phase;
} NecroCompileInfo;

void              necro_test(NECRO_TEST test);
void              necro_compile(const char* file_name, const char* input_string, size_t str_length, NECRO_PHASE compilation_phase);
void              necro_compile_opt(const char* file_name, const char* input_string, size_t str_length, NECRO_PHASE compilation_phase);

#endif // DRIVER_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_COMMON_H
#define RUNTIME_COMMON_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#endif // RUNTIME_COMMON_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "runtime/mouse.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

///////////////////////////////////////////////////////
// Necro Runtime
///////////////////////////////////////////////////////
extern DLLEXPORT void     _necro_init_runtime();
extern DLLEXPORT void     _necro_update_runtime();
extern DLLEXPORT void     _necro_error_exit(uint32_t error_code);
extern DLLEXPORT void     _necro_sleep(uint32_t milliseconds);
extern DLLEXPORT void     _necro_print(int64_t value);
extern DLLEXPORT void     _necro_set_root(uint32_t* root, uint32_t root_index, uint16_t num_slots);
extern DLLEXPORT void     _necro_initialize_root_set(uint32_t root_count);
extern DLLEXPORT int64_t* _necro_alloc(int64_t size_in_bytes, uint16_t slots_used, uint16_t slots, uint8_t segment);
extern DLLEXPORT void     _necro_collect();

///////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////
size_t necro_get_runtime_tick();
void   necro_gc_init();
void   necro_cleanup_gc();

#endif // RUNTIME_H
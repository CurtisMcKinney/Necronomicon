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

typedef struct NecroConstructorInfo NecroConstructorInfo;

///////////////////////////////////////////////////////
// Necro Runtime
///////////////////////////////////////////////////////
extern DLLEXPORT void         necro_runtime_init();
extern DLLEXPORT void         necro_runtime_update();
extern DLLEXPORT void         necro_runtime_error_exit(uint32_t error_code);
extern DLLEXPORT void         necro_runtime_sleep(uint32_t milliseconds);
extern DLLEXPORT void         necro_runtime_print(int value);
extern DLLEXPORT void         necro_runtime_debug_print(int value);
extern DLLEXPORT unsigned int necro_runtime_print_int(int value, unsigned int world);

//-------------------------
// new copying collector
extern DLLEXPORT int*     _necro_from_alloc(size_t size);
extern DLLEXPORT int*     _necro_to_alloc(size_t size);
extern DLLEXPORT void     _necro_copy_gc_initialize_root_set(size_t root_count);
extern DLLEXPORT void     _necro_copy_gc_set_root(int** root, size_t root_index, size_t data_id);
extern DLLEXPORT void     _necro_copy_gc_collect();
extern DLLEXPORT void     _necro_set_data_map(NecroConstructorInfo* a_data_map);
extern DLLEXPORT void     _necro_set_member_map(size_t* a_member_map);
extern DLLEXPORT void     _necro_flip_const();

///////////////////////////////////////////////////////
// Utility functions
///////////////////////////////////////////////////////
size_t necro_runtime_get_tick();
void   necro_copy_gc_init();
void   necro_copy_gc_cleanup();

#endif // RUNTIME_H
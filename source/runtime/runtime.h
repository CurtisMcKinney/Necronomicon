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
extern DLLEXPORT void     _necro_init_runtime();
extern DLLEXPORT void     _necro_update_runtime();
extern DLLEXPORT void     _necro_error_exit(uint32_t error_code);
extern DLLEXPORT void     _necro_sleep(uint32_t milliseconds);
extern DLLEXPORT void     _necro_print(int value);
extern DLLEXPORT void     _necro_debug_print(int value);

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
size_t necro_get_runtime_tick();
void   necro_copy_gc_init();
void   necro_copy_gc_cleanup();

#endif // RUNTIME_H
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
#include "result.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

//--------------------
// Runtime Management
extern DLLEXPORT void         necro_runtime_init();
extern DLLEXPORT void         necro_runtime_update();
extern DLLEXPORT unsigned int necro_runtime_is_done();
extern DLLEXPORT void         necro_runtime_shutdown();
extern DLLEXPORT void         necro_runtime_error_exit(uint32_t error_code);
extern DLLEXPORT void         necro_runtime_sleep(uint32_t milliseconds);

//--------------------
// IO
extern DLLEXPORT void         necro_runtime_print(int value);
extern DLLEXPORT void         necro_runtime_debug_print(int value);
extern DLLEXPORT unsigned int necro_runtime_print_int(int value, unsigned int world);
extern DLLEXPORT unsigned int necro_runtime_print_i64(int64_t value, unsigned int world);
extern DLLEXPORT unsigned int necro_runtime_print_f64(double value, unsigned int world);
extern DLLEXPORT unsigned int necro_runtime_print_char(unsigned int value, unsigned int world);
extern DLLEXPORT int          necro_runtime_get_mouse_x(unsigned int _dummy);
extern DLLEXPORT int          necro_runtime_get_mouse_y(unsigned int _dummy);
extern DLLEXPORT unsigned int necro_runtime_test_assertion(unsigned int is_true, unsigned int world);

//--------------------
// Audio
typedef int NecroLangCallback(void);
NecroResult(void)             necro_runtime_audio_init();
NecroResult(void)             necro_runtime_audio_start(NecroLangCallback* necro_init, NecroLangCallback* necro_main, NecroLangCallback* necro_shutdown);
NecroResult(void)             necro_runtime_audio_stop();
NecroResult(void)             necro_runtime_audio_shutdown();
extern DLLEXPORT unsigned int necro_runtime_get_sample_rate();
extern DLLEXPORT unsigned int necro_runtime_get_block_size();
extern DLLEXPORT unsigned int necro_runtime_out_audio_block(unsigned int channel_num, double* audio_block, unsigned int world);

//--------------------
// Memory
extern DLLEXPORT uint8_t*     necro_runtime_alloc(unsigned int size);
extern DLLEXPORT uint8_t*     necro_runtime_realloc(uint8_t* ptr, unsigned int size);
extern DLLEXPORT void         necro_runtime_free(uint8_t* data);

#endif // RUNTIME_H
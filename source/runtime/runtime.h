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
#include "runtime_common.h"
#include "runtime_audio.h"

//--------------------
// Runtime Management
extern DLLEXPORT void   necro_runtime_init();
extern DLLEXPORT void   necro_runtime_update();
extern DLLEXPORT size_t necro_runtime_is_done();
extern DLLEXPORT void   necro_runtime_shutdown();
extern DLLEXPORT void   necro_runtime_error_exit(size_t error_code);
extern DLLEXPORT void   necro_runtime_inexhaustive_case_exit(size_t* str, size_t str_length);
extern DLLEXPORT void   necro_runtime_sleep(uint32_t milliseconds);

//--------------------
// IO
// extern DLLEXPORT void   necro_runtime_print(int value);
// extern DLLEXPORT void   necro_runtime_debug_print(int value);
extern DLLEXPORT size_t necro_runtime_print_i32(int32_t value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_i64(int64_t value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_u32(uint32_t value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_u64(uint64_t value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_f32(float value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_f64(double value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_char(size_t value, size_t world);
extern DLLEXPORT size_t necro_runtime_print_string(size_t* str, size_t str_length, size_t world);
extern DLLEXPORT int    necro_runtime_get_mouse_x(size_t _dummy);
extern DLLEXPORT int    necro_runtime_get_mouse_y(size_t _dummy);
extern DLLEXPORT size_t necro_runtime_test_assertion(size_t is_true, size_t world);
bool                    necro_runtime_was_test_successful();
extern DLLEXPORT size_t necro_runtime_panic(size_t world);
extern DLLEXPORT size_t necro_runtime_open_file(size_t* str, uint64_t str_length);
extern DLLEXPORT size_t necro_runtime_close_file(size_t file);
extern DLLEXPORT size_t necro_runtime_write_int_to_file(int64_t value, size_t file);
extern DLLEXPORT size_t necro_runtime_write_uint_to_file(uint64_t value, size_t file);
extern DLLEXPORT size_t necro_runtime_write_float_to_file(double value, size_t file);
extern DLLEXPORT size_t necro_runtime_write_char_to_file(size_t value, size_t file);

//--------------------
// Audio
typedef int NecroLangCallback(void);
NecroResult(void)       necro_runtime_audio_init();
NecroResult(void)       necro_runtime_audio_start(NecroLangCallback* necro_init, NecroLangCallback* necro_main, NecroLangCallback* necro_shutdown);
NecroResult(void)       necro_runtime_audio_stop();
NecroResult(void)       necro_runtime_audio_shutdown();
extern DLLEXPORT size_t necro_runtime_get_sample_rate();
extern DLLEXPORT size_t necro_runtime_get_block_size();
extern DLLEXPORT size_t necro_runtime_out_audio_block(size_t channel_num, double* audio_block, size_t world);

//--------------------
// Memory
extern DLLEXPORT uint8_t* necro_runtime_alloc(size_t size);
extern DLLEXPORT uint8_t* necro_runtime_realloc(uint8_t* ptr, size_t size);
extern DLLEXPORT void     necro_runtime_free(uint8_t* data);

#endif // RUNTIME_H

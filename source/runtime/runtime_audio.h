/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_AUDIO_H
#define RUNTIME_AUDIO_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "runtime_common.h"

#define                   necro_runtime_audio_num_output_channels 2
static size_t             necro_runtime_audio_sample_rate         = 48000;
static size_t             necro_runtime_audio_block_size          = 256;

struct NecroDownsample;
struct NecroDownsample*         necro_downsample_create(const double freq_cutoff, const double sample_rate);
void                            necro_downsample(struct NecroDownsample* downsample, const size_t output_channel, const size_t num_output_channels, const size_t block_size, const size_t oversample_mul, double* input_buffer, float* output_buffer);
extern DLLEXPORT const size_t** necro_runtime_record_audio_block(const uint64_t a_channel_num, const uint64_t a_num_channels, const double* a_audio_block, size_t** a_scratch_buffer);
extern DLLEXPORT const size_t** necro_runtime_record_audio_block_finalize(const size_t* a_name, const uint64_t a_name_length, const uint64_t a_num_channels, size_t** a_scratch_buffer);
extern DLLEXPORT const size_t*  necro_runtime_open_audio_file(const size_t* a_name, const uint64_t a_name_length);

#endif // RUNTIME_AUDIO_H
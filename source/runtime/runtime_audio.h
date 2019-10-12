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

struct NecroDownsample;
struct NecroDownsample* necro_downsample_create(const double freq_cutoff, const double sample_rate);
void                    necro_downsample(struct NecroDownsample* downsample, const size_t block_size, const size_t oversample_mul, double* input_buffer, float* output_buffer);

#endif // RUNTIME_AUDIO_H
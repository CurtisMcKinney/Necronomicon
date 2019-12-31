/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "runtime_audio.h"


///////////////////////////////////////////////////////
// NecroDownsample
///////////////////////////////////////////////////////

/*
    FIR filter, Windowing design:
        * 1. Set filter sepcifications
        * 2. Calculate ideal filter response
        * 3. Generate coefficients by convolving ideal filter response with window function
        * 4. Multiply block by coefficients and sum
    Downsample setup:
        * Filter out frequencies above new nyquist
        * Decimate
*/

#define NECRO_DOWNSAMPLE_FILTER_NUM_TAPS      512
#define NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK 511

typedef struct NecroDownsample
{
    size_t sample_buffer_index;
    double sample_buffer[NECRO_DOWNSAMPLE_FILTER_NUM_TAPS];
    double coefficients[NECRO_DOWNSAMPLE_FILTER_NUM_TAPS];
} NecroDownsample;

double necro_sinc(const double x)
{
    if (x > -1.0E-5 && x < 1.0E-5)
        return(1.0);
    return sin(x) / x;
}

NecroDownsample* necro_downsample_create(const double freq_cutoff, const double sample_rate)
{
    NecroDownsample* downsample = malloc(sizeof(NecroDownsample));

    //--------------------
    // Init Sample Buffer
    downsample->sample_buffer_index = 0;
    for (size_t i = 0; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
        downsample->sample_buffer[i] = 0;

    //--------------------
    // Create FIR coefficients
    const double omega_c = freq_cutoff / sample_rate; // Normalized frequencies, 0 - 1
    const double m       = NECRO_DOWNSAMPLE_FILTER_NUM_TAPS - 1;
    for (size_t i = 0; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
    {
        const double arg = ((double) i) - m / 2.0; // Shift to make filter causal
        downsample->coefficients[i]  = omega_c * necro_sinc(omega_c * arg * M_PI);
    }

    //--------------------
    // Convolve FIR coefficients with window
    const double dm = m + 1;
    // const double beta = 3.0;
    for (size_t i = 0; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
    {
        double ri = (double) i + 1;
        // Hanning
        // downsample->coefficients[i] *= 0.5 - 0.5 * cos(2.0 * M_PI * ri / dm);
        // Hamming
        downsample->coefficients[i] *= 0.54 - 0.46 * cos(2.0 * M_PI * ri / dm);
        // Nuttall
        // downsample->coefficients[i]  *= 0.355768 - 0.487396 * cos(2.0 * M_PI * ri / dm) + 0.144232 * cos(4.0 * M_PI * ri / dm) - 0.012604 * cos(6.0 * M_PI * ri / dm);
        // Sinc
        // downsample->coefficients[i] *= pow(necro_sinc(((double) (2 * i + 1 - num_taps)) / m * M_PI), beta);
        // Sine
        // downsample->coefficients[i] *= pow(sin(((double) (i + 1)) * M_PI / dm), beta);
    }

    return downsample;
}

void necro_downsample(NecroDownsample* downsample, const size_t output_channel, const size_t num_output_channels, const size_t block_size, const size_t oversample_mul, double* input_buffer, float* output_buffer)
{
    //--------------------
    // Downsample and output
    size_t out_i = output_channel;
    for (size_t i = 0; i < block_size; ++i)
    {
        const size_t oversample_i = i * oversample_mul;

        //--------------------
        // 1. Insert samples into delay line
        for (size_t o = 0; o < oversample_mul; ++o)
        {
            downsample->sample_buffer[downsample->sample_buffer_index] = input_buffer[oversample_i + o];
            downsample->sample_buffer_index                            = (downsample->sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
        }

        //--------------------
        // 2. FIR Filter
        double fir_result          = 0.0;
        size_t sample_buffer_index = downsample->sample_buffer_index;
        size_t coefficient_index   = 0;
        while (coefficient_index < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS)
        {
            // Manually unroll 8 times
            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;

            fir_result          += downsample->sample_buffer[sample_buffer_index] * downsample->coefficients[coefficient_index];
            sample_buffer_index  = (sample_buffer_index + 1) & NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK;
            coefficient_index++;
        }

        //--------------------
        // 3. Decimate at new sample rate
        output_buffer[out_i] = (float)fir_result;
        out_i               += num_output_channels;
    }
}


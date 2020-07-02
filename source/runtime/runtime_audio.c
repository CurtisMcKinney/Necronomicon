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
#include "sndfile.h"
#include "utility/utility.h"


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

#define NECRO_DOWNSAMPLE_FILTER_NUM_TAPS      128
#define NECRO_DOWNSAMPLE_FILTER_NUM_TAPS_MASK 127

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

double bessel(double x)
{
    double sum  = 0.0;
    for (size_t i = 1; i < 10; ++i)
    {
        const double xpow = pow(x / 2.0, (double)i);
        size_t factorial = 1;
        for (size_t j = 1; j <= i; ++j)
            factorial *= j;
        sum += pow(xpow / (double)factorial, 2.0);
    }
    return 1.0 + sum;
}

// Refer to the book "Spectral Audio Signal Processing" for more info on windowing
// Refer to site iowahills.com for more information on windowing.
NecroDownsample* necro_downsample_create(const double freq_cutoff, const double sample_rate)
{
    NecroDownsample* downsample = emalloc(sizeof(NecroDownsample));

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
        const double arg = ((double) i) - (m / 2.0); // Shift to make filter causal
        downsample->coefficients[i]  = omega_c * necro_sinc(omega_c * arg * M_PI);
    }

    //--------------------
    // Convolve FIR coefficients with window
    const double dm   = m + 1;
    const double beta = 5.0; // range is 1 -> 10
    for (size_t i = 0; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
    {
        // double ri = (double) i + 1;
        // Hanning
        // downsample->coefficients[i] *= 0.5 - 0.5 * cos(2.0 * M_PI * ri / dm);
        // Hamming
        // downsample->coefficients[i] *= 0.54 - 0.46 * cos(2.0 * M_PI * ri / dm);
        // Nuttall
        // downsample->coefficients[i]  *= 0.355768 - 0.487396 * cos((2.0 * M_PI * ri) / dm) + 0.144232 * cos((4.0 * M_PI * ri) / dm) - 0.012604 * cos((6.0 * M_PI * ri) / dm);
        // Blackman - Nuttall
        // downsample->coefficients[i]  *= 0.3635819 - 0.4891775 * cos((2.0 * M_PI * ri) / dm) + 0.1365995 * cos((4.0 * M_PI * ri) / dm) - 0.0106411 * cos((6.0 * M_PI * ri) / dm);
        // Blackman - Harris
        // downsample->coefficients[i]  *= 0.35875 - 0.48829 * cos((2.0 * M_PI * ri) / dm) + 0.14128 * cos((4.0 * M_PI * ri) / dm) - 0.01168 * cos((6.0 * M_PI * ri) / dm);
        // Sinc
        // downsample->coefficients[i] *= pow(necro_sinc((((double) (2 * i + 1 - NECRO_DOWNSAMPLE_FILTER_NUM_TAPS)) / m) * M_PI), beta);
        // Sine
        // downsample->coefficients[i] *= pow(sin(((double) (i + 1)) * M_PI / dm), beta);
        // Kaiser-Bessel
        double arg = beta * sqrt(1.0 - pow((((double)(2 * i + 2)) - dm) / dm, 2.0));
        downsample->coefficients[i] *= bessel(arg) / bessel(beta);
    }

    // Unity gain adjustment
    double gain_sum = 0.0;
    for (size_t i = 0; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
        gain_sum += downsample->coefficients[i];
    gain_sum /= (double) NECRO_DOWNSAMPLE_FILTER_NUM_TAPS;
    for (size_t i = 0; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
        downsample->coefficients[i] *= gain_sum;

    // for (size_t i = NECRO_DOWNSAMPLE_FILTER_NUM_TAPS / 2; i < NECRO_DOWNSAMPLE_FILTER_NUM_TAPS; ++i)
    //     downsample->coefficients[i] *= 0.0;

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

typedef struct NecroRuntimeAudioFile
{
    uint64_t num_channels;
    uint64_t num_samples;
    double*  audio_data;
} NecroRuntimeAudioFile;

static const NecroRuntimeAudioFile NULL_AUDIO_FILE = { 0, 0, NULL };

/*
    Note: AudioFiles opened in the manner are READ ONLY,
    thus we're dynamically allocating memory and then simply never cleaning them up as they are expected to live for the life of the program.
    This is to maximize their ease of usage in a typical necro program (load some immutable audio files to be used for samples and fun audio processing).
    A different API is required for write or read/write audio files which can be mutated and which can be manually cleaned up
*/
extern DLLEXPORT const size_t* necro_runtime_open_audio_file(const size_t* a_name, const uint64_t a_name_length)
{
    if (a_name == NULL || a_name_length == 0)
    {
        fprintf(stderr, "Null audio file name in audioFileOpen\n");
        return (size_t*) &NULL_AUDIO_FILE;
    }

    // Create ascii string
    char* file_name = emalloc(a_name_length + 1);
    for (size_t i = 0; i < a_name_length; ++i)
    {
        // TODO: Unicode handling
        file_name[i] = (char) a_name[i];
    }
    file_name[a_name_length] = '\0';

    // Load file and file info
    SF_INFO	sf_info;
	memset (&sf_info, 0, sizeof(sf_info)); // Yes, this is in fact the way in which libsndfile wants you to initialize the SF_INFO struct...
    SNDFILE* snd_file = sf_open(file_name, SFM_READ, &sf_info);

    // Couldn't open audio file
    if (snd_file == NULL)
    {
        fprintf(stderr, "Unable to open audio file: %s\n", file_name);
        puts(sf_strerror(NULL));
        return (size_t*) &NULL_AUDIO_FILE;
    }

    // TODO / HACK: We're not worrying about sample rate conversion for the time being. When we have more time fill it in here!
    if ((size_t)sf_info.samplerate != necro_runtime_audio_sample_rate)
    {
        fprintf(stderr, "Incorrect sample rate: %d\n", sf_info.samplerate);
        return (size_t*) &NULL_AUDIO_FILE;
    }

    // Read audio data
    const uint64_t         num_channels   = sf_info.channels;
    const uint64_t         num_samples    = sf_info.frames;
    const size_t           buffer_size    = sf_info.channels * sf_info.frames;
    NecroRuntimeAudioFile* audio_file_ptr = emalloc(sizeof(NecroRuntimeAudioFile) + (buffer_size * sizeof(double))); // Allocating in one contiguous block
    double*                audio_data     = (double*)(audio_file_ptr + 1);
    size_t                 read_count     = 0;
    // do
    // {
        // NOTE: that audio data is interleaved when it arrives from libsndfile
        read_count = sf_read_double(snd_file, audio_data, buffer_size);
    // }
    // while (read_count > 0);
    sf_close(snd_file);

    if (read_count == 0)
    {
        fprintf(stderr, "Could not read audio data for audio file: %s\n", file_name);
        return (size_t*) &NULL_AUDIO_FILE;
    }

    // Set audio file ptr
    audio_file_ptr->num_channels = num_channels;
    audio_file_ptr->num_samples  = num_samples;
    audio_file_ptr->audio_data   = audio_data;

    // Clean up and return
    printf("Loaded audio file: %s\n", file_name);
    free(file_name);
    return (size_t*) audio_file_ptr;
}

typedef struct NecroScratchBuffer
{
    double*                    audio_buffer;
    size_t                     buffer_index;
    struct NecroScratchBuffer* head;
    struct NecroScratchBuffer* next;
} NecroScratchBuffer;

extern DLLEXPORT const size_t** necro_runtime_record_audio_block_finalize(const size_t* a_name, const uint64_t a_name_length, const uint64_t a_num_channels, size_t** a_scratch_buffer)
{
    if (a_scratch_buffer == NULL)
        return NULL;
    if (a_name == NULL || a_name_length == 0)
    {
        printf("Null audio file name in necro_runtime_record_audio!\n");
        return NULL;
    }

    // We're done, Open File and write contents of scratch buffer into it
    NecroScratchBuffer* curr_buffer = (NecroScratchBuffer*)*a_scratch_buffer;
    assert(curr_buffer != NULL);

    // Create ascii string
    char* file_name = emalloc(a_name_length + 1);
    for (size_t i = 0; i < a_name_length; ++i)
    {
        // TODO: Unicode handling
        file_name[i] = (char)a_name[i];
    }
    file_name[a_name_length] = '\0';

    // Set up sf_info
    SF_INFO	sf_info;
    memset(&sf_info, 0, sizeof(sf_info)); // Yes, this is in fact the way in which libsndfile wants you to initialize the SF_INFO struct...
    sf_info.format     = SF_FORMAT_WAV | SF_FORMAT_DOUBLE;
    sf_info.channels   = (int) a_num_channels;
    sf_info.samplerate = (int) necro_runtime_audio_sample_rate;
    sf_info.sections   = 1;
    sf_info.seekable   = SF_TRUE;
    sf_info.frames     = 0;

    curr_buffer = curr_buffer->head;
    while (curr_buffer != NULL)
    {
        sf_info.frames += curr_buffer->buffer_index / a_num_channels;
        curr_buffer     = curr_buffer->next;
    }
    curr_buffer = ((NecroScratchBuffer*)* a_scratch_buffer)->head;

    // Open file
    SNDFILE* snd_file = sf_open(file_name, SFM_WRITE, &sf_info);
    if (snd_file == NULL)
    {
        printf("Unable to open audio file: %s\n", file_name);
        puts(sf_strerror(NULL));
        return NULL;
    }

    // Write scratch buffer(s) to contiguous scratch buffer (it seems libsndfile doesn't allow you to write chunks of data to the file, but instead wants one contiguous buffer)
    while (curr_buffer != NULL)
    {
        // Write buffer
        sf_write_double(snd_file, curr_buffer->audio_buffer, curr_buffer->buffer_index);

        // Clean up block
        NecroScratchBuffer* prev_buffer = curr_buffer;
        curr_buffer                     = curr_buffer->next;
        free(prev_buffer->audio_buffer);
        free(prev_buffer);
        prev_buffer                     = NULL;
    }

    // Clean up
    // NOTE: The scratch buffer is handed to us and is allocated via the necrolang memory system, so we don't free it here!
    sf_close(snd_file);
    *a_scratch_buffer = NULL;
    free(file_name);
    return NULL;
}

extern DLLEXPORT const size_t** necro_runtime_record_audio_block(const uint64_t a_channel_num, const uint64_t a_num_channels, const double* a_audio_block, size_t** a_scratch_buffer)
{
    if (a_scratch_buffer == NULL)
        return NULL;

    const size_t        scratch_buffer_num_blocks = 5000;
    const size_t        scratch_buffer_size       = necro_runtime_audio_block_size * scratch_buffer_num_blocks * a_num_channels;
    NecroScratchBuffer* curr_buffer               = (NecroScratchBuffer*)*a_scratch_buffer;

    // Fresh scratch buffer
    if (curr_buffer == NULL)
    {
        curr_buffer               = emalloc(sizeof(NecroScratchBuffer));
        curr_buffer->audio_buffer = emalloc(scratch_buffer_size * sizeof(double));
        curr_buffer->buffer_index = 0;
        curr_buffer->head         = curr_buffer;
        curr_buffer->next         = NULL;
        *a_scratch_buffer         = (size_t*)curr_buffer;
    }
    // New scratch buffer block
    else if (curr_buffer->buffer_index >= scratch_buffer_size)
    {
        NecroScratchBuffer* new_buffer = emalloc(sizeof(NecroScratchBuffer));
        new_buffer->audio_buffer       = emalloc(scratch_buffer_size * sizeof(double));
        new_buffer->buffer_index       = 0;
        new_buffer->head               = curr_buffer->head;
        new_buffer->next               = NULL;
        curr_buffer->next              = new_buffer;
        curr_buffer                    = new_buffer;
        *a_scratch_buffer              = (size_t*)curr_buffer;
    }

    // Write audio into scratch buffer
    // for (size_t i = a_channel_num; i < necro_runtime_audio_block_size * a_num_channels; i += (size_t) a_num_channels)
    for (size_t i = 0; i < necro_runtime_audio_block_size; i++)
    {
        /*
            Note: audio channel are written interleaved as such:
            l0, r0, l1, r1, l2, r2...lN, rN
            libsnd file expects interleaved channels, so we might as well do it up front in the scratch buffer
        */
        curr_buffer->audio_buffer[curr_buffer->buffer_index + (i * a_num_channels) + a_channel_num] = a_audio_block[i];
    }
    if (a_channel_num >= a_num_channels - 1)
        curr_buffer->buffer_index += necro_runtime_audio_block_size * a_num_channels;
    return a_scratch_buffer;
}


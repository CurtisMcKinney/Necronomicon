/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#ifdef _WIN32
#define UNICODE 1
#define _UNICODE 1
#include <Windows.h>
#define _USE_MATH_DEFINES
#else
#include <unistd.h>
#endif

#include <math.h>
#include "portaudio.h"
#include "runtime.h"
#include "utility.h"
#include "runtime_audio.h"


///////////////////////////////////////////////////////
// Runtime Crossplatform
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_RUNTIME_UNINITIALIZED = 0,
    NECRO_RUNTIME_RUNNING       = 1,
    NECRO_RUNTIME_IS_DONE       = 2,
    NECRO_RUNTIME_SHUTDOWN      = 4
} NECRO_RUNTIME_STATE;

NECRO_RUNTIME_STATE necro_runtime_state = NECRO_RUNTIME_UNINITIALIZED;
int                 mouse_x             = 0;
int                 mouse_y             = 0;
bool                is_test_true        = true;

extern DLLEXPORT int necro_runtime_get_mouse_x(size_t _dummy)
{
    UNUSED(_dummy);
    return mouse_x;
}

extern DLLEXPORT int necro_runtime_get_mouse_y(size_t _dummy)
{
    UNUSED(_dummy);
    return mouse_y;
}

// extern DLLEXPORT void necro_runtime_print(int value)
// {
//     printf("%d", value);
// }

// extern DLLEXPORT void necro_runtime_debug_print(int value)
// {
//     printf("debug: %d", value);
// }

extern DLLEXPORT size_t necro_runtime_print_i32(int32_t value, size_t world)
{
    printf("%d", value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_i64(int64_t value, size_t world)
{
    printf("%" PRId64, value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_u32(uint32_t value, size_t world)
{
    printf("%u", value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_u64(uint64_t value, size_t world)
{
    printf("%zu", value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_f32(float value, size_t world)
{
    printf("%.17f", value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_f64(double value, size_t world)
{
    printf("%.17g", value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_char(size_t value, size_t world)
{
    if (value < 256)
        putchar((int) value);
    else
        printf("%zu", value);
    return world;
}

extern DLLEXPORT size_t necro_runtime_print_string(size_t* str, size_t str_length, size_t world)
{
    for (size_t i = 0; i < str_length; ++i)
    {
        putchar((int)str[i]);
    }
    putchar('\n');
    return world;
}

extern DLLEXPORT void necro_runtime_sleep(uint32_t milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000); // 100 Milliseconds
#endif
}

extern DLLEXPORT void necro_runtime_error_exit(size_t error_code)
{
    switch (error_code)
    {
    case 1:
        fprintf(stderr, "****Error: Non-exhaustive patterns in case statement!\n");
        break;
    default:
        fprintf(stderr, "****Error: Unrecognized error (%zu) during runtime!\n", error_code);
        break;
    }
    necro_exit((uint32_t) error_code);
}

extern DLLEXPORT void necro_runtime_inexhaustive_case_exit(size_t* str, size_t str_length)
{
    fprintf(stderr, "****  Error: Non-exhaustive patterns in case statement!\n");
    fprintf(stderr, "****    found in:\n");
    fprintf(stderr, "****      ");
    for (size_t i = 0; i < str_length; ++i)
    {
        fputc((int)str[i], stderr);
    }
    fprintf(stderr, "\n");
    necro_exit(1);
}

extern DLLEXPORT size_t necro_runtime_test_assertion(size_t is_true, size_t world)
{
    if (necro_runtime_state != NECRO_RUNTIME_RUNNING)
        return world;
    is_test_true        = is_true;
    necro_runtime_state = NECRO_RUNTIME_IS_DONE;
    return world;
}

bool necro_runtime_was_test_successful()
{
    return is_test_true;
}

extern DLLEXPORT size_t necro_runtime_panic(size_t world)
{
    fprintf(stderr, "****panic!\n");
    necro_exit(-1);
    return world;
}


//--------------------
// File IO
//--------------------

extern DLLEXPORT size_t necro_runtime_open_file(size_t* str, uint64_t str_length)
{
    if (str_length == 0)
        return 0;

    // Create ascii string
    char* file_name = malloc(str_length + 1);
    assert(file_name != NULL);
    for (size_t i = 0; i < str_length; ++i)
    {
        // TODO: Unicode handling
        file_name[i] = (char) str[i];
    }
    file_name[str_length] = '\0';

    // Open File
#ifdef WIN32
    FILE* file;
    fopen_s(&file, file_name, "w");
#else
    FILE* file = fopen(argv[1], "w");
#endif
    if (!file)
    {
        return 0;
    }

    // Clean up and return
    free(file_name);
    return (size_t)file;
}

extern DLLEXPORT size_t necro_runtime_close_file(size_t file)
{
    fclose((FILE*)file);
    return 0;
}

extern DLLEXPORT size_t necro_runtime_write_int_to_file(int64_t value, size_t file)
{
    char buffer[NECRO_ITOA_BUF_LENGTH];
    snprintf(buffer, NECRO_ITOA_BUF_LENGTH, "%" PRId64, value);
    fputs(buffer, (FILE*)file);
    return file;
}

extern DLLEXPORT size_t necro_runtime_write_uint_to_file(uint64_t value, size_t file)
{
    char buffer[NECRO_ITOA_BUF_LENGTH];
    snprintf(buffer, NECRO_ITOA_BUF_LENGTH, "%zu", value);
    fputs(buffer, (FILE*)file);
    return file;
}

extern DLLEXPORT size_t necro_runtime_write_float_to_file(double value, size_t file)
{
    char buffer[NECRO_ITOA_BUF_LENGTH];
    snprintf(buffer, NECRO_ITOA_BUF_LENGTH,  "%.17f", value);
    fputs(buffer, (FILE*)file);
    return file;
}

extern DLLEXPORT size_t necro_runtime_write_char_to_file(size_t value, size_t file)
{
    fputc((int)value, (FILE*)file);
    return file;
}


//--------------------
// Memory
//--------------------
typedef struct NecroHeap
{
    uint8_t* data;
    size_t   bump;
    size_t   capacity;
} NecroHeap;
NecroHeap necro_heap = { .data = NULL, .bump = 0, .capacity = 0 };

NecroHeap necro_heap_create(size_t capacity)
{
    return (NecroHeap) { .data = calloc(capacity, sizeof(uint8_t)), .bump = 0, .capacity = capacity };
}

void necro_heap_destroy(NecroHeap* heap)
{
    free(heap->data);
    heap->data     = NULL;
    heap->bump     = 0;
    heap->capacity = 0;
}

// HACK / TODO: Note this is completely ridiculous, but until we have a better allocation scheme this is what we're doing for now!
// TODO: Different Allocators based on size: Slab Allocator => Buddy => OS
extern DLLEXPORT uint8_t* necro_runtime_alloc(size_t size)
{
    // return malloc(size);
    if (size == 0)
        return NULL;
    assert(size % 8 == 0);
    // Make sure we're 64 byte aligned for proper simd usage
    size_t padding = 0;
    if ((((size_t)(necro_heap.data + necro_heap.bump)) % 64) != 0)
    {
        padding          = 64 - (((size_t)(necro_heap.data + necro_heap.bump)) % 64);
        necro_heap.bump += padding;
    }
    if (necro_heap.bump + size >= necro_heap.capacity)
    {
        fprintf(stderr, "Necro memory exhausted!\n");
        exit(665); // The neighbor of the beast
    }
    uint8_t* data    = necro_heap.data + necro_heap.bump;
    necro_heap.bump += size;
    // printf("Alloc: %zu bytes, %zu padding, %zu alignmod64, total: %fmb\n\n", size, padding, ((size_t)data) % 64, (((double)necro_heap.bump) / 1000000.0));
    return data;
}

extern DLLEXPORT uint8_t* necro_runtime_realloc(uint8_t* ptr, size_t size)
{
    // printf("necro_runtime_realloc, ptr: %p, size: %zu\n\n", ptr, size);
    // return realloc(ptr, size);
    necro_runtime_free(ptr);
    // printf("REALLOC\n");
    return necro_runtime_alloc(size);
}

extern DLLEXPORT void necro_runtime_free(uint8_t* data)
{
    UNUSED(data);
    // free(data);
}


///////////////////////////////////////////////////////
// Audio
///////////////////////////////////////////////////////
static NecroLangCallback* necro_runtime_audio_lang_callback       = NULL;
// static float**            necro_runtime_audio_output_buffer       = NULL;
static float*             necro_runtime_audio_output_buffer       = NULL;
static size_t             necro_runtime_audio_num_input_channels  = 0;
#define                   necro_runtime_audio_num_output_channels 2
static size_t             necro_runtime_audio_sample_rate         = 48000;
static size_t             necro_runtime_audio_block_size          = 256;
// #define                   necro_runtime_audio_oversample_amt      2
// static double             necro_runtime_audio_brick_wall_cutoff   = 20000.0;
static double             necro_runtime_audio_start_time          = 0.0;
static double             necro_runtime_audio_curr_time           = 0.0;
static PaStream*          necro_runtime_audio_pa_stream           = NULL;
struct NecroDownsample*   necro_runtime_audio_downsample[necro_runtime_audio_num_output_channels];

extern DLLEXPORT size_t necro_runtime_out_audio_block(size_t channel_num, double* audio_block, size_t world)
{
    if (channel_num >= necro_runtime_audio_num_output_channels || necro_runtime_is_done())
        return world;
    // float*                  output_buffer = necro_runtime_audio_output_buffer[channel_num];
    // struct NecroDownsample* downsample = necro_runtime_audio_downsample[channel_num];
    // necro_downsample(downsample, necro_runtime_audio_block_size, necro_runtime_audio_oversample_amt, audio_block, output_buffer);
    // necro_downsample(downsample, channel_num, necro_runtime_audio_num_output_channels, necro_runtime_audio_block_size, necro_runtime_audio_oversample_amt, audio_block, necro_runtime_audio_output_buffer);

    // output into interleaved audio buffer
    size_t out_i = channel_num;
    for (size_t i = 0; i < necro_runtime_audio_block_size; ++i)
    {
        necro_runtime_audio_output_buffer[out_i] = (float) audio_block[i];
        out_i += necro_runtime_audio_num_output_channels;
    }
    return world;
}

static int necro_runtime_audio_pa_callback(const void* input_buffer, void* output_buffer, unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags status_flags, void* user_data)
{
    UNUSED(input_buffer);
    UNUSED(user_data);
    if (necro_runtime_audio_lang_callback == NULL || necro_runtime_is_done())
        return 0;
    assert(necro_runtime_audio_block_size == frames_per_buffer);
    if ((status_flags & paOutputUnderflow) == paOutputUnderflow)
        printf("Output underflow!\n");
    if ((status_flags & paOutputOverflow) == paOutputOverflow)
    {
        printf("Output overflow!\n");
        return 0;
    }
    // RT IO
    // necro_runtime_audio_output_buffer = (float**) output_buffer;
    necro_runtime_audio_output_buffer = output_buffer;
    necro_runtime_audio_curr_time     = time_info->currentTime - necro_runtime_audio_start_time;
    // RT update
    necro_runtime_audio_lang_callback();
    return 0;
}

NecroResult(void) necro_runtime_audio_init()
{
    // if (necro_runtime_audio_downsample != NULL)
    //     free(necro_runtime_audio_downsample);
    // for (size_t i = 0; i < necro_runtime_audio_num_output_channels; ++i)
    //     necro_runtime_audio_downsample[i] = necro_downsample_create(necro_runtime_audio_brick_wall_cutoff, (double) (necro_runtime_audio_sample_rate * necro_runtime_audio_oversample_amt));
    PaError pa_error = Pa_Initialize();
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    // const PaSampleFormat audio_format = paFloat32 | paNonInterleaved;
    const PaSampleFormat audio_format = paFloat32;
    pa_error = Pa_OpenDefaultStream(&necro_runtime_audio_pa_stream, (int) necro_runtime_audio_num_input_channels, necro_runtime_audio_num_output_channels, audio_format, (double) necro_runtime_audio_sample_rate, (unsigned long) necro_runtime_audio_block_size, necro_runtime_audio_pa_callback, NULL);
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    return ok_void();
}

NecroResult(void) necro_runtime_audio_start(NecroLangCallback* necro_init, NecroLangCallback* necro_main, NecroLangCallback* necro_shutdown)
{
    assert(necro_init != NULL);
    assert(necro_main != NULL);
    assert(necro_shutdown != NULL);
    //--------------------
    // Init, then start RT thread
    necro_try(void, necro_runtime_audio_init());
    necro_heap = necro_heap_create(256000000);
    necro_runtime_init();
    necro_runtime_audio_lang_callback = necro_main;
    PaError pa_error = paNoError;
    if (necro_init() == 0)
        pa_error = Pa_StartStream(necro_runtime_audio_pa_stream);
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    //--------------------
    // NRT Update
    size_t     cpu_check             = 0;
    const long check_if_done_time_ms = 20;
    while (!necro_runtime_is_done())
    {
        // TODO: Need to synchronize this (lockless) with rt thread!
        necro_runtime_update();
        double cpu_load = Pa_GetStreamCpuLoad(necro_runtime_audio_pa_stream);
        if (cpu_check > 9)
        {
            printf("  cpu: %.2f%%  mem: %.2fmb        \r", cpu_load * 100.0, (((double)necro_heap.bump) / 1000000.0));
            cpu_check = 0;
        }
        cpu_check++;
        Pa_Sleep(check_if_done_time_ms);
        // necro_runtime_sleep(check_if_done_time_ms);
    }
    //--------------------
    // Shutdown
    necro_try(void, necro_runtime_audio_stop());
    necro_try(void, necro_runtime_audio_shutdown());
    // TODO: remove, for now freeing seems broken...
    // necro_shutdown();
    necro_runtime_shutdown();
    necro_heap_destroy(&necro_heap);
    return ok_void();
}

NecroResult(void) necro_runtime_audio_stop()
{
    // PaError pa_error = Pa_AbortStream(necro_runtime_audio_pa_stream);
    PaError pa_error = Pa_StopStream(necro_runtime_audio_pa_stream);
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    return ok_void();
}

NecroResult(void) necro_runtime_audio_shutdown()
{
    PaError pa_error = Pa_CloseStream(necro_runtime_audio_pa_stream);
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    pa_error         = Pa_Terminate();
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    for (size_t i = 0; i < necro_runtime_audio_num_output_channels; ++i)
    {
        if (necro_runtime_audio_downsample[i] != NULL)
            free(necro_runtime_audio_downsample[i]);
    }
    return ok_void();
}

extern DLLEXPORT size_t necro_runtime_get_sample_rate()
{
    // return necro_runtime_audio_sample_rate * necro_runtime_audio_oversample_amt;
    return necro_runtime_audio_sample_rate;
}

extern DLLEXPORT size_t necro_runtime_get_block_size()
{
    // return necro_runtime_audio_block_size * necro_runtime_audio_oversample_amt;
    return necro_runtime_audio_block_size;
}


///////////////////////////////////////////////////////
// Runtime Windows
///////////////////////////////////////////////////////
#ifdef _WIN32

#define UNICODE 1
#define _UNICODE 1
#include "Windows.h"

// HWND window;
HANDLE       h_in;
HANDLE       h_out;
DWORD        num_read;
INPUT_RECORD input_record[32];
COORD        mouse_coord;
DWORD        original_fdw_mode;

extern DLLEXPORT void necro_runtime_init()
{
    if (necro_runtime_state != NECRO_RUNTIME_UNINITIALIZED)
        return;
    is_test_true        = true;
    necro_runtime_state = NECRO_RUNTIME_RUNNING;
    h_in                = GetStdHandle(STD_INPUT_HANDLE);
    h_out               = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(h_in, &original_fdw_mode);
    DWORD fdw_mode      = ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(h_in, fdw_mode);
    fdw_mode            = ENABLE_MOUSE_INPUT;
    SetConsoleMode(h_in, fdw_mode);
}

extern DLLEXPORT void necro_runtime_update()
{
    if (necro_runtime_state != NECRO_RUNTIME_RUNNING)
        return;
    DWORD num_waiting = 0;
    if (GetNumberOfConsoleInputEvents(h_in, &num_waiting) == 0)
        return;
    if (num_waiting == 0)
        return;
    if (ReadConsoleInput(h_in, input_record, 32, &num_read) == 0)
        return;
    for (DWORD i = 0; i < num_read; ++i)
    {
        switch (input_record[i].EventType)
        {
        case KEY_EVENT:
            // printf("KEY_EVENT: %d\n", i);
            if (input_record[i].Event.KeyEvent.uChar.AsciiChar == 27 || input_record[i].Event.KeyEvent.uChar.AsciiChar == 3 || input_record[i].Event.KeyEvent.uChar.AsciiChar == 4)
            {
                necro_runtime_state = NECRO_RUNTIME_IS_DONE;
            }
            break;
        case MOUSE_EVENT:
            // printf("MOUSE_EVENT: %d\n", i);
            mouse_x = input_record[i].Event.MouseEvent.dwMousePosition.X;
            mouse_y = input_record[i].Event.MouseEvent.dwMousePosition.Y;
            break;
        case WINDOW_BUFFER_SIZE_EVENT:
            // printf("WINDOW_BUFFER_SIZE_EVENT: %d\n", i);
            break;
        case FOCUS_EVENT:
            // printf("FOCUS_EVENT: %d\n", i);
            break;
        case MENU_EVENT:
            // printf("MENU_EVENT: %d\n", i);
            break;
        default:
            break;
        }
    };
}

extern DLLEXPORT size_t necro_runtime_is_done()
{
    return necro_runtime_state >= NECRO_RUNTIME_IS_DONE;
}

extern DLLEXPORT void necro_runtime_shutdown()
{
    if (necro_runtime_state != NECRO_RUNTIME_IS_DONE)
        return;
    necro_runtime_state = NECRO_RUNTIME_SHUTDOWN;
    printf("And so it ends...\n");
    SetConsoleMode(h_in, original_fdw_mode);
}

#else
///////////////////////////////////////////////////////
// Runtime Unix
///////////////////////////////////////////////////////
#include <unistd.h>
#include <assert.h>

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>

static Window root;
static Display* display = NULL;

static int catchFalseAlarm()
{
  return 0;
}

extern DLLEXPORT void necro_runtime_init()
{
    if (necro_runtime_state != NECRO_RUNTIME_UNINITIALIZED)
        return;
    is_test_true        = true;

    XSetWindowAttributes attribs;
    assert(display == NULL);

    if (!(display = XOpenDisplay(0)))
    {
        fprintf (stderr, "Couldn't connect to %s\n", XDisplayName (0));
        necro_exit(EXIT_FAILURE);
    }

    attribs.override_redirect = true;
    XSetErrorHandler((XErrorHandler)catchFalseAlarm);
    XSync(display, 0);

    necro_runtime_state = NECRO_RUNTIME_RUNNING;
}

static void query_pointer(Display *d)
{
    static bool once = false;
    int i = 0;
    unsigned m;
    Window w;

    if (once == false)
    {
        once = true;
        root = DefaultRootWindow(d);
    }

    if (!XQueryPointer(d, root, &root, &w, &mouse_x, &mouse_y, &i, &i, &m)) {
        for (i = 0; i < ScreenCount(d); ++i)
        {
            if (root == RootWindow(d, i))
            {
                break;
            }
        }
    }

    /* fprintf(stdout, "X: %d Y: %d\n", x, y); */
}

extern DLLEXPORT void necro_runtime_update()
{
    if (necro_runtime_state != NECRO_RUNTIME_RUNNING)
        return;

    assert(display != NULL);
    query_pointer(display);

    // ADD ESCAPE KEY SUPPORT

    /* if (input_record[i].Event.KeyEvent.uChar.AsciiChar == 27 || input_record[i].Event.KeyEvent.uChar.AsciiChar == 3 || input_record[i].Event.KeyEvent.uChar.AsciiChar == 4) */
    /* { */
    /*     necro_runtime_state = NECRO_RUNTIME_IS_DONE; */
    /* } */
}

extern DLLEXPORT size_t necro_runtime_is_done()
{
    return necro_runtime_state >= NECRO_RUNTIME_IS_DONE;
}

extern DLLEXPORT void necro_runtime_shutdown()
{
    if (necro_runtime_state != NECRO_RUNTIME_IS_DONE)
        return;
    printf("And so it ends...\n");
    necro_runtime_state = NECRO_RUNTIME_SHUTDOWN;
}

#endif

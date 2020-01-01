/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

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
    NECRO_RUNTIME_SHUTDOWN      = 3
} NECRO_RUNTIME_STATE;

NECRO_RUNTIME_STATE necro_runtime_state = NECRO_RUNTIME_UNINITIALIZED;
int                 mouse_x             = 0;
int                 mouse_y             = 0;
bool                is_test_true        = true;

extern DLLEXPORT int necro_runtime_get_mouse_x(unsigned int _dummy)
{
    UNUSED(_dummy);
    return mouse_x;
}

extern DLLEXPORT int necro_runtime_get_mouse_y(unsigned int _dummy)
{
    UNUSED(_dummy);
    return mouse_y;
}

extern DLLEXPORT void necro_runtime_print(int value)
{
    printf("%d", value);
}

extern DLLEXPORT void necro_runtime_debug_print(int value)
{
    printf("debug: %d", value);
}

extern DLLEXPORT unsigned int necro_runtime_print_int(int value, unsigned int world)
{
    printf("%d", value);
    return world;
}

extern DLLEXPORT unsigned int necro_runtime_print_i64(int64_t value, unsigned int world)
{
    printf("%" PRId64, value);
    return world;
}

extern DLLEXPORT unsigned int necro_runtime_print_f32(float value, unsigned int world)
{
    printf("%.17f", value);
    return world;
}

extern DLLEXPORT unsigned int necro_runtime_print_f64(double value, unsigned int world)
{
    printf("%.17g", value);
    return world;
}

extern DLLEXPORT unsigned int necro_runtime_print_char(unsigned int value, unsigned int world)
{
    if (value < 256)
        putchar(value);
    else
        printf("%d", value);
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

extern DLLEXPORT void necro_runtime_error_exit(uint32_t error_code)
{
    switch (error_code)
    {
    case 1:
        fprintf(stderr, "****Error: Non-exhaustive patterns in case statement!\n");
        break;
    default:
        fprintf(stderr, "****Error: Unrecognized error (%d) during runtime!\n", error_code);
        break;
    }
    necro_exit(error_code);
}

extern DLLEXPORT unsigned int necro_runtime_test_assertion(unsigned int is_true, unsigned int world)
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

// TODO: Different Allocators based on size: Slab Allocator => Buddy => OS
extern DLLEXPORT uint8_t* necro_runtime_alloc(unsigned int size)
{
    return malloc(size);
}

extern DLLEXPORT uint8_t* necro_runtime_realloc(uint8_t* ptr, unsigned int size)
{
    return realloc(ptr, size);
}

extern DLLEXPORT void necro_runtime_free(uint8_t* data)
{
    free(data);
}

///////////////////////////////////////////////////////
// Audio
///////////////////////////////////////////////////////
static NecroLangCallback* necro_runtime_audio_lang_callback       = NULL;
// static float**            necro_runtime_audio_output_buffer       = NULL;
static float*             necro_runtime_audio_output_buffer       = NULL;
static unsigned int       necro_runtime_audio_num_input_channels  = 0;
#define                   necro_runtime_audio_num_output_channels 2
static unsigned int       necro_runtime_audio_sample_rate         = 48000;
static unsigned int       necro_runtime_audio_block_size          = 64;
#define                   necro_runtime_audio_oversample_amt      32
static double             necro_runtime_audio_brick_wall_cutoff   = 19500.0;
static double             necro_runtime_audio_start_time          = 0.0;
static double             necro_runtime_audio_curr_time           = 0.0;
static PaStream*          necro_runtime_audio_pa_stream           = NULL;
struct NecroDownsample*   necro_runtime_audio_downsample[necro_runtime_audio_num_output_channels];

extern DLLEXPORT unsigned int necro_runtime_out_audio_block(unsigned int channel_num, double* audio_block, unsigned int world)
{
    if (channel_num >= necro_runtime_audio_num_output_channels)
        return world;
    // float*                  output_buffer = necro_runtime_audio_output_buffer[channel_num];
    struct NecroDownsample* downsample = necro_runtime_audio_downsample[channel_num];
    // necro_downsample(downsample, necro_runtime_audio_block_size, necro_runtime_audio_oversample_amt, audio_block, output_buffer);
    necro_downsample(downsample, channel_num, necro_runtime_audio_num_output_channels, necro_runtime_audio_block_size, necro_runtime_audio_oversample_amt, audio_block, necro_runtime_audio_output_buffer);
    return world;
}

// Or break up and run the whole signal multiple times!?
static int necro_runtime_audio_pa_callback(const void* input_buffer, void* output_buffer, unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags status_flags, void* user_data)
{
    UNUSED(input_buffer);
    UNUSED(user_data);
    UNUSED(status_flags);
    if (necro_runtime_audio_lang_callback == NULL)
        return 0;
    assert(necro_runtime_audio_block_size == frames_per_buffer);
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
    for (size_t i = 0; i < necro_runtime_audio_num_output_channels; ++i)
        necro_runtime_audio_downsample[i] = necro_downsample_create(necro_runtime_audio_brick_wall_cutoff, necro_runtime_audio_sample_rate * necro_runtime_audio_oversample_amt);
    PaError pa_error = Pa_Initialize();
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    // const PaSampleFormat audio_format = paFloat32 | paNonInterleaved;
    const PaSampleFormat audio_format = paFloat32;
    pa_error = Pa_OpenDefaultStream(&necro_runtime_audio_pa_stream, necro_runtime_audio_num_input_channels, necro_runtime_audio_num_output_channels, audio_format, necro_runtime_audio_sample_rate, necro_runtime_audio_block_size, necro_runtime_audio_pa_callback, NULL);
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
    necro_runtime_init();
    necro_runtime_audio_lang_callback = necro_main;
    PaError pa_error = paNoError;
    if (necro_init() == 0)
        pa_error = Pa_StartStream(necro_runtime_audio_pa_stream);
    if (pa_error != paNoError) return necro_runtime_audio_error(Pa_GetErrorText(pa_error));
    //--------------------
    // NRT Update
    const long check_if_done_time_ms = 100;
    while (!necro_runtime_is_done())
    {
        // TODO: Need to synchronize this (lockless) with rt thread!
        necro_runtime_update();
        Pa_Sleep(check_if_done_time_ms);
    }
    //--------------------
    // Shutdown
    necro_runtime_audio_stop();
    // TODO: remove, for now freeing seems broken...
    // necro_shutdown();
    necro_runtime_shutdown();
    return ok_void();
}

NecroResult(void) necro_runtime_audio_stop()
{
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

extern DLLEXPORT unsigned int necro_runtime_get_sample_rate()
{
    return necro_runtime_audio_sample_rate * necro_runtime_audio_oversample_amt;
}

extern DLLEXPORT unsigned int necro_runtime_get_block_size()
{
    return necro_runtime_audio_block_size * necro_runtime_audio_oversample_amt;
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
    if (num_waiting == 0 || ReadConsoleInput(h_in, input_record, 32, &num_read) == 0)
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

extern DLLEXPORT unsigned int necro_runtime_is_done()
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
  static bool once;
  int i = 0;
  int x = 0, y = 0;
  unsigned m;
  Window w;

  if (once == false)
  {
    once = true;
    root = DefaultRootWindow(d);
  }

  if (!XQueryPointer(d, root, &root, &w, &x, &y, &i, &i, &m)) {
    for (i = 0; i < ScreenCount(d); ++i)
    {
      if (root == RootWindow(d, i))
      {
        break;
      }
    }
  }

  fprintf(stdout, "X: %d Y: %d\n", x, y);
}

extern DLLEXPORT void necro_runtime_update()
{
    if (necro_runtime_state != NECRO_RUNTIME_RUNNING)
        return;

    assert(display != NULL);
    query_pointer(display);
}

extern DLLEXPORT unsigned int necro_runtime_is_done()
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

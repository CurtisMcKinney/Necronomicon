/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "runtime.h"
#include "utility.h"

#ifdef _WIN32
#define UNICODE 1
#define _UNICODE 1
#include <Windows.h>
#else
#include <unistd.h>
#endif


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

int mouse_x = 0;
int mouse_y = 0;

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

extern DLLEXPORT unsigned int necro_runtime_get_sample_rate()
{
    // TODO!
    return 44100;
}


extern DLLEXPORT unsigned int necro_runtime_get_block_size()
{
    // TODO!
    return 1024;
}

extern DLLEXPORT void necro_runtime_print(int value)
{
    printf("%d\n", value);
}

extern DLLEXPORT void necro_runtime_debug_print(int value)
{
    printf("debug: %d\n", value);
}

extern DLLEXPORT unsigned int necro_runtime_print_int(int value, unsigned int world)
{
    printf("%d                                \r", value);
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
    case 2:
        fprintf(stderr, "****Error: Malformed closure application!\n");
        break;
    default:
        fprintf(stderr, "****Error: Unrecognized error (%d) during runtime!\n", error_code);
        break;
    }
    necro_exit(error_code);
}

// Different Allocators basedon size: Slab Allocator => Buddy => OS
extern DLLEXPORT uint8_t* necro_runtime_alloc(unsigned int size)
{
    return malloc(size);
}

extern DLLEXPORT void necro_runtime_free(uint8_t* data)
{
    free(data);
}

// Quick and easy binary gcd algorithm
extern DLLEXPORT int64_t necro_runtime_gcd(int64_t x, int64_t y)
{
    uint64_t a = x;
    uint64_t b = x;
    if (x == 0)
        return y;
    uint64_t d = 0;
    while (((a | b) & 1) == 0) // a and b are even
    {
        a >>= 1;
        b >>= 1;
        d++;
    }
    while (a != b)
    {
        if ((a & 1) == 0) // a is even
            a >>= 1;
        else if ((b & 1) == 0) // b is even
            b >>= 1;
        else if (a > b)
            a = (a - b) >> 1;
        else
            b = (b - a) >> 1;
    }
    int64_t gcd = a << d;
    return (x < 0) ? -gcd : gcd;
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

extern DLLEXPORT unsigned int necro_runtime_out_audio_block(unsigned int channel_num, double* audio_block, unsigned int world)
{
    UNUSED(channel_num);
    UNUSED(audio_block);
    // printf("[%f]\n", audio_block[0]);
    printf("out_audio_block[%d]", channel_num);
    return world;
}

#else
///////////////////////////////////////////////////////
// Runtime Unix
///////////////////////////////////////////////////////
#include <unistd.h>

extern DLLEXPORT void necro_runtime_init()
{
    if (necro_runtime_state != NECRO_RUNTIME_UNINITIALIZED)
        return;
    necro_runtime_state = NECRO_RUNTIME_RUNNING;
}

extern DLLEXPORT void necro_runtime_update()
{
    if (necro_runtime_state != NECRO_RUNTIME_RUNNING)
        return;
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
}

extern DLLEXPORT unsigned int necro_runtime_out_audio_block(unsigned int channel_num, double* audio_block, unsigned int world)
{
    UNUSED(channel_num);
    UNUSED(audio_block);
    // TODO:!
    return world;
}

#endif

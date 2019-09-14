/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include "mouse.h"
#include "runtime.h"
#include "utility.h"

//=====================================================
// API
//=====================================================
int mouse_x = 0;
int mouse_y = 0;

int _necro_mouse_x()
{
    // necro_poll_mouse();
    return mouse_x;
}

int _necro_mouse_y()
{
    // necro_poll_mouse();
    return mouse_y;
}

int necro_runtime_get_mouse_x(unsigned int _dummy)
{
    UNUSED(_dummy);
    // necro_poll_mouse();
    return mouse_x;
}

int necro_runtime_get_mouse_y(unsigned int _dummy)
{
    UNUSED(_dummy);
    // necro_poll_mouse();
    return mouse_y;
}

#ifdef _WIN32
//=====================================================
// Windows
//=====================================================
#define UNICODE 1
#define _UNICODE 1
#include "Windows.h"

// HWND window;
HANDLE       h_in;
HANDLE       h_out;
DWORD        num_read;
INPUT_RECORD input_record[32];
COORD        mouse_coord;
bool         mouse_initialized = false;
size_t       current_tick      = 0;

void necro_init_mouse()
{
    if (mouse_initialized)
        return;
    mouse_initialized = true;
    h_in              = GetStdHandle(STD_INPUT_HANDLE);
    h_out             = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD fdwMode = ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(h_in, fdwMode);
    fdwMode       = ENABLE_MOUSE_INPUT;
    SetConsoleMode(h_in, fdwMode);
}

void necro_poll_mouse()
{
    assert(mouse_initialized);
    DWORD num_waiting = 0;
    if (GetNumberOfConsoleInputEvents(h_in, &num_waiting) == 0)
        return;
    if (ReadConsoleInput(h_in, input_record, 32, &num_read) == 0)
        return;
    for (DWORD i = 0; i < num_read; ++i)
    {
        switch (input_record[i].EventType)
        {
        case KEY_EVENT:
            // printf("KEY_EVENT: %d\n", i);
            if (input_record[i].Event.KeyEvent.uChar.AsciiChar == 'x')
            {
                printf("And so it ends...\n");
                necro_exit(0);
            };
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

#else
//=====================================================
// Unix
//=====================================================
#include <unistd.h>

void necro_init_mouse()
{
}

void necro_poll_mouse()
{
}
#endif

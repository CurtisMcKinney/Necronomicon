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
    necro_poll_mouse();
    return mouse_x;
}

int _necro_mouse_y()
{
    necro_poll_mouse();
    return mouse_y;
}

#ifdef _WIN32
//=====================================================
// Windows
//=====================================================
#include "Windows.h"

// HWND window;
HANDLE       h_in;
HANDLE       h_out;
DWORD        num_read;
INPUT_RECORD input_record;
COORD        mouse_coord;
bool         mouse_initialized = false;
size_t       current_tick      = 0;

void necro_init_mouse()
{
    if (mouse_initialized)
        return;
    // window = GetConsoleWindow();
    mouse_initialized = true;
    h_in              = GetStdHandle(STD_INPUT_HANDLE);
    h_out             = GetStdHandle(STD_OUTPUT_HANDLE);
}

void necro_poll_mouse()
{
    necro_init_mouse();
    size_t runtime_tick = necro_get_runtime_tick();
    if (current_tick == runtime_tick)
        return;
    current_tick = runtime_tick;
    // CONSOLE_SCREEN_BUFFER_INFO screen_info;
    // GetConsoleScreenBufferInfo(window, &screen_info);
    DWORD  num_waiting = 0;
    if (GetNumberOfConsoleInputEvents(h_in, &num_waiting) == 0)
        return;
    if (num_waiting == 0)
        return;
    if (ReadConsoleInput(h_in, &input_record, 1, &num_read) == 0)
        return;
    switch (input_record.EventType)
    {
    // case KEY_EVENT:
    //     ++KeyEvents;
    //     SetConsoleCursorPosition(hOut,
    //         KeyWhere);
    //     cout << KeyEvents << flush;
    //     if (InRec.Event.KeyEvent.uChar.AsciiChar == 'x')
    //     {
    //         SetConsoleCursorPosition(hOut,
    //             EndWhere);
    //         cout << "Exiting..." << endl;
    //         Continue = FALSE;
    //     }
    //     break;
    case MOUSE_EVENT:
        mouse_x = input_record.Event.MouseEvent.dwMousePosition.X;
        mouse_y = input_record.Event.MouseEvent.dwMousePosition.Y;
        break;
    }
}

#else
//=====================================================
// Unix
//=====================================================
#include <unistd.h>

#endif
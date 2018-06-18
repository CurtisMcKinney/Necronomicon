/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include "mouse.h"
#include "utility.h"

//=====================================================
// API
//=====================================================
int64_t mouseX = 0;
int64_t mouseY = 0;

int64_t _necro_mouse_x()
{
    return mouseX;
}

int64_t _necro_mouse_y()
{
    return mouseY;
}

#ifdef _WIN32
//=====================================================
// Windows
//=====================================================
#include "Windows.h"

HWND window;

void necro_init_mouse()
{
    return;
    window = GetConsoleWindow();
}

void necro_poll_mouse()
{
    return;
    CONSOLE_SCREEN_BUFFER_INFO screen_info;
    GetConsoleScreenBufferInfo(window, &screen_info);
    mouseX = screen_info.dwCursorPosition.X;
    mouseY = screen_info.dwCursorPosition.Y;
}

#else
//=====================================================
// Unix
//=====================================================
#include <unistd.h>

#endif
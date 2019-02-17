/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include "utility.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

 ///////////////////////////////////////////////////////
process_error_code_t necro_compile_in_child_process(const char* command_line_arguments)
{
    assert(command_line_arguments && command_line_arguments[0]);

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

    const size_t length_command_line_arguments = strlen(command_line_arguments) + 1 /* null terminator */;
    wchar_t* wchar_command_line_arguments = (wchar_t*) _alloca(length_command_line_arguments * sizeof(wchar_t));
    memset(wchar_command_line_arguments, 0, length_command_line_arguments * sizeof(wchar_t));
    mbstowcs(wchar_command_line_arguments, command_line_arguments, length_command_line_arguments);

    // Start the child process.
    if (!CreateProcess(NULL,   // No module name (use command line)
        wchar_command_line_arguments,        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
    )
    {
        printf("CreateProcess failed (%d)!!!.\n", GetLastError());
        printf("Failed using this command: %s", command_line_arguments);
        printf("Also make sure your working directory is set to .. (which should be the root directory)");
        return 1;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);

    if (exit_code == (DWORD) EXIT_MEMORY_LEAK_DETECTED)
    {
        puts("==============================vvvvvvvvvvv!!");
        puts("Sub process failed because of memory leak!!");
        puts("==============================^^^^^^^^^^^!!");
    }

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code;

#else
    assert(false && "necro_compile_file_in_child_process not implemented for this platform yet! Add the implementation here.");
    return 1;
#endif
}

void print_white_space(size_t white_count)
{
    for (size_t i = 0; i < white_count; ++i)
        printf(" ");
}

///////////////////////////////////////////////////////
// Timer
///////////////////////////////////////////////////////
typedef struct NecroTimer
{
#if _WIN32
    LARGE_INTEGER ticks_per_sec;
    LARGE_INTEGER start_time;
    LARGE_INTEGER end_time;
    double        total_time_ms;
#else
    size_t todo;
#endif
} NecroTimer;

NecroTimer* necro_timer_create()
{
    NecroTimer* timer = malloc(sizeof(NecroTimer));
    if (timer == NULL)
    {
        fprintf(stderr, "Could not allocate memory in necro_timer_create\n");
        necro_exit(1);
    }
    *timer = (NecroTimer) { .start_time = 0, .end_time = 0 };
#if _WIN32
    QueryPerformanceFrequency(&timer->ticks_per_sec);
#else
    // TODO
#endif
    return timer;
}

void necro_timer_destroy(struct NecroTimer* timer)
{
    if (timer == NULL)
        return;
    free(timer);
}

void necro_timer_start(NecroTimer* timer)
{
#if _WIN32
    QueryPerformanceCounter(&timer->start_time);
#else
    // TODO
#endif
}

double necro_timer_stop(NecroTimer* timer)
{
#if _WIN32
    QueryPerformanceCounter(&timer->end_time);
    double time = ((timer->end_time.QuadPart - timer->start_time.QuadPart) * 1000.0) / timer->ticks_per_sec.QuadPart;
    timer->total_time_ms = time;
    return time;
#else
    // TODO
#endif
}

void necro_timer_stop_and_report(struct NecroTimer* timer, const char* print_header)
{
    double total_time_ms = necro_timer_stop(timer);
    printf("%s: %fms\n", print_header, total_time_ms);
}

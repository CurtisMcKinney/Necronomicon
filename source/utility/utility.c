/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "utility.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

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

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

NecroError necro_create_error()
{
    return (NecroError)
    {
        .error_message = "",
        .source_loc    = { 0, 0, 0 },
        .return_code   = NECRO_SUCCESS,
    };
}

size_t necro_verror(NecroError* error, NecroSourceLoc source_loc, const char* error_message, va_list args)
{
	error->return_code = NECRO_ERROR;
    error->source_loc  = source_loc;
    return vsnprintf(error->error_message, NECRO_MAX_ERROR_MESSAGE_LENGTH, error_message, args);
}

size_t necro_error(NecroError* error, NecroSourceLoc source_loc, const char* error_message, ...)
{
	va_list args;
	va_start(args, error_message);

	error->return_code = NECRO_ERROR;
    error->source_loc  = source_loc;
    size_t count = vsnprintf(error->error_message, NECRO_MAX_ERROR_MESSAGE_LENGTH, error_message, args);

	va_end(args);
    return count;
}

void necro_print_error(NecroError* error, const char* input_string, const char* error_type)
{
    size_t line_start = error->source_loc.pos - error->source_loc.character;
    size_t line_end   = line_start;
    for (line_end = line_start; input_string[line_end] != '\0' && input_string[line_end] != '\n'; ++line_end);
    printf("\n----------------------------------------------------------------------------\n");
    printf(" %s error at line %zu, character %zu:\n\n", error_type, error->source_loc.line + 1, error->source_loc.character + 1);
    printf("%.*s", (int) (line_end - line_start), (input_string + line_start));
    printf("\n\n %s\n", error->error_message);
    printf("----------------------------------------------------------------------------\n");
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

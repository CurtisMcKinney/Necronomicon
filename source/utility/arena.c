/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "arena.h"
#include "math_utility.h"
#include "utility.h"

// #define ARENA_DEBUG_PRINT 1
#define DEBUG_ARENA 0

#if DEBUG_ARENA
#define TRACE_ARENA(...) printf(__VA_ARGS__)
#else
#define TRACE_ARENA(...)
#endif

NecroArena necro_arena_empty()
{
    return (NecroArena) { .region = NULL, .capacity = 0, .size = 0 };
}

#if DEBUG_MEMORY
NecroArena __necro_arena_create(size_t capacity, const char *srcFile, int srcLine)
#else
NecroArena __necro_arena_create(size_t capacity)
#endif // DEBUG_MEMORY
{
    NecroArena arena;
    if (capacity > 0)
    {
#if DEBUG_MEMORY
        arena.region = __emalloc(capacity, srcFile, srcLine);
#else
        arena.region = __emalloc(capacity);
#endif // DEBUG_MEMORY
    }
    else
    {
        arena.region = NULL;
    }
    arena.capacity = capacity;
    arena.size = 0;
    return arena;
}

void necro_arena_destroy(NecroArena* arena)
{
    if (arena->region)
    {
        free(arena->region);
    }
    arena->region = NULL;
    arena->size = 0;
    arena->capacity = 0;
}

// NOTE: Arena now ALWAYS reallocs
void* necro_arena_alloc(NecroArena* arena, size_t size)
{
    // Branchless Align
    assert(arena->capacity > 0);
    assert(arena->region != NULL);
    size +=
        ((size & (sizeof(size_t) - 1)) != 0) *
        (sizeof(size_t) - (size & (sizeof(size_t) - 1)));
    if (arena->size + size >= arena->capacity)
    {
        size_t new_capacity = arena->capacity * 2;
        while (arena->capacity + size >= new_capacity)
            new_capacity *= 2;
        // arena->region    = (char*) realloc(arena->region, new_capacity);
        free(arena->region);
        arena->region    = (char*) emalloc(new_capacity);
        arena->capacity  = new_capacity;
    }
    void* local_region = (void*) (arena->region + arena->size);
    arena->size += size;
    return local_region;
}

//=====================================================
// NecroPagedArena
//=====================================================
#define NECRO_PAGED_ARENA_INITIAL_SIZE 512

NecroPagedArena necro_paged_arena_empty()
{
    return (NecroPagedArena)
    {
        .pages           = NULL,
        .data            = NULL,
        .size            = 0,
        .count           = 0,
        .total_mem_usage = 0,
    };
}

#if DEBUG_MEMORY
NecroPagedArena __necro_paged_arena_create(const char *srcFile, int srcLine)
#else
NecroPagedArena __necro_paged_arena_create()
#endif // DEBUG_MEMORY
{
#if DEBUG_MEMORY
    NecroArenaPage* page = __emalloc(sizeof(NecroArenaPage) + NECRO_PAGED_ARENA_INITIAL_SIZE, srcFile, srcLine);
#else
    NecroArenaPage* page = emalloc(sizeof(NecroArenaPage) + NECRO_PAGED_ARENA_INITIAL_SIZE);
#endif // DEBUG_MEMORY

    page->next = NULL;
    return (NecroPagedArena)
    {
        .pages           = page,
        .data            = (char*)(page + 1),
        .size            = NECRO_PAGED_ARENA_INITIAL_SIZE,
        .count           = 0,
        .total_mem_usage = NECRO_PAGED_ARENA_INITIAL_SIZE,
    };
}

NecroPagedArena necro_paged_arena_create_with_capacity(size_t capacity)
{
    NecroArenaPage* page = emalloc(sizeof(NecroArenaPage) + capacity);
    page->next = NULL;
    return (NecroPagedArena)
    {
        .pages           = page,
        .data            = (char*)(page + 1),
        .size            = capacity,
        .count           = 0,
        .total_mem_usage = capacity,
    };
}

#if DEBUG_MEMORY
void* __necro_paged_arena_alloc(NecroPagedArena* arena, size_t size, const char *srcFile, int srcLine)
#else
void* __necro_paged_arena_alloc(NecroPagedArena* arena, size_t size)
#endif // DEBUG_MEMORY
{
    assert(arena != NULL);
    assert(arena->pages != NULL);
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    if (size == 0)
        return NULL;
    // Branchless Align
    size +=
        ((size & (sizeof(size_t) - 1)) != 0) *
        (sizeof(size_t) - (size & (sizeof(size_t) - 1)));
    if (arena->count + size >= arena->size)
    {
        arena->size *= 2;
        while (arena->count + size >= arena->size)
            arena->size *= 2;
        TRACE_ARENA("allocating new page of size: %d\n", arena->size);
#if DEBUG_MEMORY
        NecroArenaPage* page = __emalloc(sizeof(NecroArenaPage) + arena->size, srcFile, srcLine);
#else
        NecroArenaPage* page = __emalloc(sizeof(NecroArenaPage) + arena->size);
#endif // DEBUG_MEMORY
        page->next              = arena->pages;
        arena->pages            = page;
        arena->data             = (char*)(page + 1);
        arena->count            = 0;
        arena->total_mem_usage += arena->size;
    }
    // TRACE_ARENA("paged_arena_alloc { data %p, count: %d, size: %d }, requested bytes: %d\n", arena->data, arena->count, arena->size, size);
    char* data = arena->data + arena->count;
    assert(data != NULL);
    arena->count += size;
    assert(arena->count < arena->size);
    return data;
}

void necro_paged_arena_destroy(NecroPagedArena* arena)
{
    assert(arena != NULL);
    if (arena->pages == NULL)
        return;
    NecroArenaPage* current_page = arena->pages;
    NecroArenaPage* next_page    = NULL;
    while (current_page != NULL)
    {
        next_page = current_page->next;
        free(current_page);
        current_page = next_page;
    }
    *arena = necro_paged_arena_empty();
}


//=====================================================
// NecroSnapshotArena
//=====================================================
#define NECRO_SNAPSHOT_ARENA_INITIAL_SIZE 1024

NecroSnapshotArena necro_snapshot_arena_empty()
{
    return (NecroSnapshotArena)
    {
        .pages     = NULL,
        .curr_page = NULL,
    };
}

NecroSnapshotArena necro_snapshot_arena_create()
{
    NecroSnapshotArenaPage* page = emalloc(sizeof(NecroSnapshotArenaPage) + NECRO_PAGED_ARENA_INITIAL_SIZE);
    page->next                   = NULL;
    page->size                   = NECRO_PAGED_ARENA_INITIAL_SIZE;
    page->count                  = 0;
    return (NecroSnapshotArena)
    {
        .pages     = page,
        .curr_page = page,
    };
}

void necro_snapshot_arena_destroy(NecroSnapshotArena* arena)
{
    assert(arena != NULL);
    if (arena->pages == NULL)
        return;
    NecroSnapshotArenaPage* current_page = arena->pages;
    NecroSnapshotArenaPage* next_page    = NULL;
    while (current_page != NULL)
    {
        next_page = current_page->next;
        free(current_page);
        current_page = next_page;
    }
    arena->pages     = NULL;
    arena->curr_page = NULL;
}

void* necro_snapshot_arena_alloc(NecroSnapshotArena* arena, size_t alloc_size)
{
    assert(arena != NULL);
    assert(arena->pages != NULL);
    assert(arena->curr_page != NULL);
    assert(arena->curr_page->count < arena->curr_page->size);
    if (alloc_size == 0)
        return NULL;
    // Branchless Align
    alloc_size +=
        ((alloc_size & (sizeof(size_t) - 1)) != 0) *
        (sizeof(size_t) - (alloc_size & (sizeof(size_t) - 1)));
    while (arena->curr_page->count + alloc_size >= arena->curr_page->size)
    {
        if (arena->curr_page->next != NULL)
        {
            arena->curr_page        = arena->curr_page->next;
            arena->curr_page->count = 0;
        }
        else
        {
            const size_t new_size = arena->curr_page->size * 2;
            TRACE_ARENA("allocating new snapshot arena page of size: %d\n", arena->curr_page->size);
// #if DEBUG_MEMORY
//          NecroArenaPage* page = __emalloc(sizeof(NecroArenaPage) + arena->size, srcFile, srcLine);
// #else
            NecroSnapshotArenaPage* new_page = emalloc(sizeof(NecroSnapshotArenaPage) + new_size);
// #endif // DEBUG_MEMORY
            arena->curr_page->next  = new_page;
            arena->curr_page        = new_page;
            arena->curr_page->next  = NULL;
            arena->curr_page->size  = new_size;
            arena->curr_page->count = 0;
        }
    }
    // TRACE_ARENA("paged_arena_alloc { data %p, count: %d, size: %d }, requested bytes: %d\n", arena->data, arena->count, arena->size, alloc_size);
    char* data = &arena->curr_page->data + arena->curr_page->count;
    arena->curr_page->count += alloc_size;
    assert(arena->curr_page->count < arena->curr_page->size);
    return data;
}

NecroArenaSnapshot necro_snapshot_arena_get(NecroSnapshotArena* arena)
{
    assert(arena != NULL);
    assert(arena->curr_page->count < arena->curr_page->size);
    return (NecroArenaSnapshot) { .page = arena->curr_page, .count = arena->curr_page->count };
}

void necro_snapshot_arena_rewind(NecroSnapshotArena* arena, NecroArenaSnapshot snapshot)
{
    arena->curr_page        = snapshot.page;
    arena->curr_page->count = snapshot.count;
    assert(arena->curr_page->count < arena->curr_page->size);
    // printf("rewind arena count: %d\n", arena->count);
}

char* necro_snapshot_arena_concat_strings(NecroSnapshotArena* arena, uint32_t string_count, const char** strings)
{
    size_t total_length = 1;
    for (size_t i = 0; i < string_count; ++i)
    {
        total_length += strlen(strings[i]);
    }
    char* buffer = necro_snapshot_arena_alloc(arena, total_length);
    buffer[0] = '\0';
    for (size_t i = 0; i < string_count; ++i)
    {
        strcat(buffer, strings[i]);
    }
    return buffer;
}

char* necro_snapshot_arena_concat_strings_with_lengths(NecroSnapshotArena* arena, uint32_t string_count, const char** strings, size_t* lengths)
{
    size_t total_length = 1;
    for (size_t i = 0; i < string_count; ++i)
    {
        total_length += lengths[i];
    }
    char* buffer = necro_snapshot_arena_alloc(arena, total_length);
    buffer[0] = '\0';
    for (size_t i = 0; i < string_count; ++i)
    {
        strcat(buffer, strings[i]);
    }
    return buffer;
}

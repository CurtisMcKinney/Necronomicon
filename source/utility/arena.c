/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "arena.h"
#include "math.h"
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

NecroArena necro_arena_create(size_t capacity)
{
    NecroArena arena;
    if (capacity > 0)
    {
        arena.region = emalloc(capacity);
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

void* necro_arena_alloc(NecroArena* arena, size_t size, NECRO_ARENA_ALLOC_POLICY alloc_policy)
{
    bool canFit = arena->capacity > (arena->size + size);
    if (!canFit && (alloc_policy == NECRO_ARENA_REALLOC))
    {
        size_t new_capacity = MAX(arena->capacity * 2, arena->capacity + size);
        char* new_region = (arena->region != NULL) ? (char*) realloc(arena->region, new_capacity) : (char*) emalloc(new_capacity);
        arena->region    = new_region;
        arena->capacity  = new_capacity;
        canFit = true;
    }

    if (canFit)
    {
        void* local_region = (void*) (arena->region + arena->size);
        arena->size += size;
        return local_region;
    }

    return 0;
}

//=====================================================
// NecroPagedArena
//=====================================================
#define NECRO_PAGED_ARENA_INITIAL_SIZE 512

NecroPagedArena necro_paged_arena_empty()
{
    return (NecroPagedArena)
    {
        .pages = NULL,
        .data  = NULL,
        .size  = 0,
        .count = 0,
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
    NecroArenaPage* page = __emalloc(sizeof(NecroArenaPage) + NECRO_PAGED_ARENA_INITIAL_SIZE);
#endif // DEBUG_MEMORY
    
    page->next = NULL;
    return (NecroPagedArena)
    {
        .pages = page,
        .data  = (char*)(page + 1),
        .size  = NECRO_PAGED_ARENA_INITIAL_SIZE,
        .count = 0,
    };
}

void* necro_paged_arena_alloc(NecroPagedArena* arena, size_t size)
{
    assert(arena != NULL);
    assert(arena->pages != NULL);
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    if (size == 0)
        return NULL;
    if (arena->count + size >= arena->size)
    {
        while (arena->count + size >= arena->size)
            arena->size *= 2;
        TRACE_ARENA("allocating new page of size: %d\n", arena->size);
        NecroArenaPage* page = emalloc(sizeof(NecroArenaPage) + arena->size);
        page->next   = arena->pages;
        arena->pages = page;
        arena->data  = (char*)(page + 1);
        arena->count = 0;
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
        if (next_page != NULL)
            free(current_page);
        current_page = next_page;
    }
    arena->pages = NULL;
    arena->data  = NULL;
    arena->size  = 0;
    arena->count = 0;
}

//=====================================================
// NecroSnapshotArena
//=====================================================
#define NECRO_SNAPSHOT_ARENA_INITIAL_SIZE 1024

NecroSnapshotArena necro_snapshot_arena_empty()
{
    return (NecroSnapshotArena)
    {
        .data  = NULL,
        .size  = 0,
        .count = 0,
    };
}

NecroSnapshotArena necro_snapshot_arena_create()
{
    return (NecroSnapshotArena)
    {
        .data  = emalloc(NECRO_SNAPSHOT_ARENA_INITIAL_SIZE),
        .size  = NECRO_SNAPSHOT_ARENA_INITIAL_SIZE,
        .count = 0
    };
}

void necro_snapshot_arena_destroy(NecroSnapshotArena* arena)
{
    assert(arena != NULL);
    if (arena->data == NULL)
        return;
    free(arena->data);
    arena->data  = NULL;
    arena->count = 0;
    arena->size  = 0;
}

void* necro_snapshot_arena_alloc(NecroSnapshotArena* arena, size_t bytes)
{
    assert(arena != NULL);
    assert(arena->data != NULL);
    if (arena->count + bytes >= arena->size)
    {
        size_t new_size = arena->size * 2;
        char*  new_data = (char*)realloc(arena->data, new_size);
        if (new_data == NULL)
        {
            if (arena->data != NULL)
                free(arena->data);
            assert(false && "Failed to allocate in necro_snapshot_arena_alloc");
            return NULL;
        }
        arena->data = new_data;
        arena->size = new_size;
    }
    char* allocated_data = arena->data + arena->count;
    arena->count += bytes;
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    assert(arena->count >= 0);
    // printf("alloc  arena count: %d\n", arena->count);
    return allocated_data;
}

NecroArenaSnapshot necro_snapshot_arena_get(NecroSnapshotArena* arena)
{
    assert(arena != NULL);
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    assert(arena->count >= 0);
    return (NecroArenaSnapshot) { .count = arena->count };
}

void necro_snapshot_arena_rewind(NecroSnapshotArena* arena, NecroArenaSnapshot snapshot)
{
    arena->count = snapshot.count;
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    assert(arena->count >= 0);
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

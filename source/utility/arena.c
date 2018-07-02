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

// #define ARENA_DEBUG_PRINT 1
#define DEBUG_ARENA 0

#if DEBUG_ARENA
#define TRACE_ARENA(...) printf(__VA_ARGS__)
#else
#define TRACE_ARENA(...)
#endif

NecroArena construct_arena(size_t capacity)
{
    NecroArena arena;
    if (capacity > 0)
    {
        arena.region = malloc(capacity);
        if (arena.region == NULL) exit(1);
    }
    else
    {
        arena.region = NULL;
    }

    arena.capacity = capacity;
    arena.size = 0;

#ifdef ARENA_DEBUG_PRINT
    printf("construct_arena { region %p, size: %d, capacity: %d }\n", arena.region, arena.size, arena.capacity);
#endif // ARENA_DEBUG_PRINT
    return arena;
}

void destruct_arena(NecroArena* arena)
{
    if (arena->region)
   {
        free(arena->region);
    }

    arena->region = NULL;
    arena->size = 0;
    arena->capacity = 0;
}

void* arena_alloc(NecroArena* arena, size_t size, arena_alloc_policy alloc_policy)
{
#ifdef ARENA_DEBUG_PRINT
    printf("arena_alloc %p { region %p, size: %d, capacity: %d }, requested size: %d\n", arena, arena->region, arena->size, arena->capacity, size);
#endif // ARENA_DEBUG_PRINT
    bool canFit = arena->capacity > (arena->size + size);
    if (!canFit && (alloc_policy == arena_allow_realloc))
    {
        size_t new_capacity = MAX(arena->capacity * 2, arena->capacity + size);
#ifdef ARENA_DEBUG_PRINT
        printf("arena_alloc %p { new_capacity: %d }\n", arena, new_capacity);
#endif // ARENA_DEBUG_PRINT
        char* new_region = (arena->region != NULL) ? (char*) realloc(arena->region, new_capacity) : (char*) malloc(new_capacity);
        if (new_region == NULL)
        {
            if (arena->region != NULL)
               free(arena->region);

#ifdef ARENA_DEBUG_PRINT
       printf("arena_alloc %p REALLOC FAILED!!! { new_region: %p, is NULL: %i }\n", arena, new_region, new_region == NULL);
#endif // ARENA_DEBUG_PRINT
            exit(1);
        }
        arena->region   = new_region;
        arena->capacity = new_capacity;
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

NecroArenaPtr arena_alloc_local(NecroArena* arena, size_t size, arena_alloc_policy alloc_policy)
{
    char* data = arena_alloc(arena, size, alloc_policy);
    return (NecroArenaPtr)(data - arena->region);
}

void* arena_deref_local(NecroArena* arena, NecroArenaPtr ptr)
{
    return NULL;
}

//=====================================================
// NecroPagedArena
//=====================================================
#define NECRO_PAGED_ARENA_INITIAL_SIZE 512
NecroPagedArena necro_create_paged_arena()
{
    NecroArenaPage* page = malloc(sizeof(NecroArenaPage) + NECRO_PAGED_ARENA_INITIAL_SIZE);
    if (page == NULL)
    {
        fprintf(stderr, "Allocation error: could not allocate %zu of memory for NecroRegionAllocator", sizeof(NecroArenaPage) + NECRO_PAGED_ARENA_INITIAL_SIZE);
        exit(1);
    }
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
    if (arena->count + size >= arena->size)
    {
        arena->size *= 2;
        TRACE_ARENA("allocating new page of size: %d\n", arena->size);
        NecroArenaPage* page = malloc(sizeof(NecroArenaPage) + arena->size);
        if (page == NULL)
        {
            fprintf(stderr, "Allocation error: could not allocate %d of memory for NecroPagedArena", sizeof(NecroArenaPage) + arena->size);
            exit(1);
        }
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

void necro_destroy_paged_arena(NecroPagedArena* arena)
{
    assert(arena != NULL);
    if (arena == NULL || arena->pages == NULL)
        return;
    NecroArenaPage* current_page = arena->pages;
    NecroArenaPage* next_page    = NULL;
    while (current_page != NULL)
    {
        next_page = current_page->next;
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
NecroSnapshotArena necro_create_snapshot_arena()
{
    return (NecroSnapshotArena)
    {
        .data  = malloc(NECRO_SNAPSHOT_ARENA_INITIAL_SIZE),
        .size  = NECRO_SNAPSHOT_ARENA_INITIAL_SIZE,
        .count = 0
    };
}

void necro_destroy_snapshot_arena(NecroSnapshotArena* arena)
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

NecroArenaSnapshot necro_get_arena_snapshot(NecroSnapshotArena* arena)
{
    assert(arena != NULL);
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    assert(arena->count >= 0);
    return (NecroArenaSnapshot) { .count = arena->count };
}

void necro_rewind_arena(NecroSnapshotArena* arena, NecroArenaSnapshot snapshot)
{
    arena->count = snapshot.count;
    assert(arena->data != NULL);
    assert(arena->count < arena->size);
    assert(arena->count >= 0);
    // printf("rewind arena count: %d\n", arena->count);
}

char* necro_concat_strings(NecroSnapshotArena* arena, uint32_t string_count, const char** strings)
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

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include "arena.h"
#include "math.h"

// #define ARENA_DEBUG_PRINT 1

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
    printf("construct_arena { region %p, size: %llu, capacity: %llu }\n", arena.region, arena.size, arena.capacity);
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
    printf("arena_alloc %p { region %p, size: %llu, capacity: %llu }, requested size: %llu\n", arena, arena->region, arena->size, arena->capacity, size);
#endif // ARENA_DEBUG_PRINT
    bool canFit = arena->capacity > (arena->size + size);
    if (!canFit && (alloc_policy == arena_allow_realloc))
    {
        size_t new_capacity = MAX(arena->capacity * 2, arena->capacity + size);
#ifdef ARENA_DEBUG_PRINT
        printf("arena_alloc %p { new_capacity: %llu }\n", arena, new_capacity);
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

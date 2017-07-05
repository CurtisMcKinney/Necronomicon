/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <assert.h>
#include "arena.h"
#include "math.h"

 NecroArena construct_arena(size_t capacity)
 {
     NecroArena arena;
     arena.region = (char*) malloc(capacity);
     arena.capacity = capacity;
     arena.size = 0;
     return arena;
 }

 void destruct_arena(NecroArena* arena)
 {
     free(arena->region);
     arena->region = 0;
     arena->size = 0;
     arena->capacity = 0;
 }

 void* arena_alloc(NecroArena* arena, size_t size, arena_alloc_policy alloc_policy)
 {
     bool canFit = arena->capacity > (arena->size + size);
     if (!canFit && (alloc_policy == arena_allow_realloc))
     {
         size_t new_capacity = MAX(arena->capacity * 2, arena->capacity + size);
         arena->region = (char*) realloc(arena->region, new_capacity);
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

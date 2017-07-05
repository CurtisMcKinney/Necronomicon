/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <assert.h>
#include "arena.h"
#include "math.h"

 NecroArena create_arena(size_t capacity)
 {
     NecroArena arena;
     arena.pRegion = (char*) malloc(capacity);
     arena.capacity = capacity;
     arena.size = 0;
     return arena;
 }

 void destroy_arena(NecroArena arena)
 {
     free(arena.pRegion);
 }

 void* arena_alloc(NecroArena* pArena, size_t size, arena_alloc_policy alloc_policy)
 {
     bool canFit = pArena->capacity > (pArena->size + size);
     if (!canFit && (alloc_policy == arena_allow_realloc))
     {
         size_t new_capacity = MAX(pArena->capacity * 2, pArena->capacity + size);
         pArena->pRegion = (char*) realloc(pArena->pRegion, new_capacity);
         pArena->capacity = new_capacity;
         canFit = true;
     }

     if (canFit)
     {
         void* pLocalRegion = (void*) (pArena->pRegion + pArena->size);
         pArena->size += size;
         return pLocalRegion;
     }

     return 0;
 }

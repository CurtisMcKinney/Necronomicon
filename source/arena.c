/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <assert.h>
#include "arena.h"
#include "math.h"

 NecroArena create_arena(size_t size)
 {
     NecroArena arena;
     arena.pRegion = (char*) malloc(size);
     arena.size = size;
     arena.position = 0;
     return arena;
 }

 void destroy_arena(NecroArena arena)
 {
     free(arena.pRegion);
 }

 void* arena_alloc(NecroArena* pArena, size_t size, arena_alloc_policy alloc_policy)
 {
     bool canFit = pArena->size > (pArena->position + size);
     if (!canFit && (alloc_policy == arena_allow_realloc))
     {
         size_t new_size = MAX(pArena->size * 2, size);
         pArena->pRegion = (char*) realloc(pArena->pRegion, new_size);
         pArena->size = new_size;
         canFit = true;
     }

     if (canFit)
     {
         void* pLocalRegion = (void*) (pArena->pRegion + pArena->position);
         pArena->position += size;
         return pLocalRegion;
     }

     return 0;
 }

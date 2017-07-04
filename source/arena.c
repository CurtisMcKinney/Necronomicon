/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdbool.h>
#include <assert.h>
#include "arena.h"

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

 void* arena_alloc(NecroArena* pArena, size_t size)
 {
     bool canFit = pArena->size > (pArena->position + size);
     assert(canFit);
     if (canFit)
     {
         void* pLocalRegion = (void*) (pArena->pRegion + pArena->position);
         pArena->position += size;
         return pLocalRegion;
     }

     return 0;
 }

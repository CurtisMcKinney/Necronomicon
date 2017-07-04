/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

// A very simple region based allocator. At the moment this doesn't support de-alloc within the arena

#include <stdlib.h>
#include <stdint.h>

typedef struct
{
    char* pRegion;
    size_t size;
    size_t position;
} NecroArena;

NecroArena create_arena(size_t size);
void destroy_arena(NecroArena arena);
void* arena_alloc(NecroArena* pArena, size_t size);

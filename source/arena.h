/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef ARENA_H
#define ARENA_H 1

// A very simple region based allocator. At the moment this doesn't support de-alloc within the arena
// This arena won't invalidate pointers unless arena_allow_realloc is used.

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

typedef enum
{
    arena_fixed_size,
    arena_allow_realloc
} arena_alloc_policy;

void* arena_alloc(NecroArena* pArena, size_t size, arena_alloc_policy alloc_policy);

#endif // ARENA_H

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
    char*  region;
    size_t capacity;
    size_t size;
} NecroArena;

NecroArena necro_empty_arena();
NecroArena construct_arena(size_t capacity);
void       destruct_arena(NecroArena* arena);

typedef enum
{
    arena_fixed_capacity,
    arena_allow_realloc
} arena_alloc_policy;

void* arena_alloc(NecroArena* arena, size_t size, arena_alloc_policy alloc_policy);

// LocalPtr
typedef uint32_t NecroArenaPtr;
static const NecroArenaPtr null_arena_ptr = (uint32_t) -1;
NecroArenaPtr arena_alloc_local(NecroArena* arena, size_t size, arena_alloc_policy alloc_policy);
void*         arena_deref_local(NecroArena* arena, NecroArenaPtr ptr);

//=====================================================
// NecroPagedArena
//=====================================================
typedef struct NecroArenaPage
{
    struct NecroArenaPage* next;
} NecroArenaPage;

typedef struct
{
    NecroArenaPage* pages;
    char*           data;
    size_t          size;
    size_t          count;
} NecroPagedArena;

NecroPagedArena necro_empty_paged_arena();
NecroPagedArena necro_create_paged_arena();
void*           necro_paged_arena_alloc(NecroPagedArena* arena, size_t size);
void            necro_destroy_paged_arena(NecroPagedArena* arena);

//=====================================================
// NecroSnapshotArena
//=====================================================
// NOTE: Pointers Retrieved from the arena are unstable!
typedef struct
{
    size_t count;
} NecroArenaSnapshot;

typedef struct
{
    char*  data;
    size_t count;
    size_t size;
} NecroSnapshotArena;

NecroSnapshotArena necro_empty_snapshot_arena();
NecroSnapshotArena necro_create_snapshot_arena();
void*              necro_snapshot_arena_alloc(NecroSnapshotArena* arena, size_t bytes);
void               necro_destroy_snapshot_arena(NecroSnapshotArena* arena);
NecroArenaSnapshot necro_get_arena_snapshot(NecroSnapshotArena* arena);
void               necro_rewind_arena(NecroSnapshotArena* arena, NecroArenaSnapshot snapshot);

char*              necro_concat_strings(NecroSnapshotArena* arena, uint32_t string_count, const char** strings);

#endif // ARENA_H

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

#include "debug_memory.h"

typedef struct
{
    char*  region;
    size_t capacity;
    size_t size;
} NecroArena;

NecroArena necro_arena_empty();
#if DEBUG_MEMORY
NecroArena __necro_arena_create(size_t capacity, const char *srcFile, int srcLine);
#else
NecroArena __necro_arena_create(size_t capacity);
#endif // DEBUG_MEMORY
void       necro_arena_destroy(NecroArena* arena);

#if DEBUG_MEMORY
#define necro_arena_create(CAPACITY) __necro_arena_create(CAPACITY, __FILE__, __LINE__)
#else
#define necro_arena_create(CAPACITY) __necro_arena_create(CAPACITY)
#endif // DEBUG_MEMORY

typedef enum
{
    NECRO_ARENA_FIXED,
    NECRO_ARENA_REALLOC
} NECRO_ARENA_ALLOC_POLICY;

void* necro_arena_alloc(NecroArena* arena, size_t size, NECRO_ARENA_ALLOC_POLICY alloc_policy);

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

NecroPagedArena necro_paged_arena_empty();
#if DEBUG_MEMORY
NecroPagedArena __necro_paged_arena_create(const char *srcFile, int srcLine);
#else
NecroPagedArena __necro_paged_arena_create();
#endif // DEBUG_MEMORY
void            necro_paged_arena_destroy(NecroPagedArena* arena);
#if DEBUG_MEMORY
void* __necro_paged_arena_alloc(NecroPagedArena* arena, size_t size, const char *srcFile, int srcLine);
#else
void* __necro_paged_arena_alloc(NecroPagedArena* arena, size_t size);
#endif // DEBUG_MEMORY

#if DEBUG_MEMORY
#define necro_paged_arena_create() __necro_paged_arena_create(__FILE__, __LINE__)
#define necro_paged_arena_alloc(ARENA, SIZE) __necro_paged_arena_alloc(ARENA, SIZE, __FILE__, __LINE__)
#else
#define necro_paged_arena_create() __necro_paged_arena_create()
#define necro_paged_arena_alloc __necro_paged_arena_alloc
#endif // DEBUG_MEMORY

//=====================================================
// NecroSnapshotArena
//=====================================================
// NOTE: Pointers and data are now stable UNTIL the arena is rewound to the snapshot.
typedef struct NecroSnapshotArenaPage
{
    struct NecroSnapshotArenaPage* next;
    size_t                         size;
    size_t                         count;
    char                           data;
} NecroSnapshotArenaPage;

typedef struct
{
    NecroSnapshotArenaPage* page;
    size_t                  count;
} NecroArenaSnapshot;

typedef struct
{
    NecroSnapshotArenaPage* pages;
    NecroSnapshotArenaPage* curr_page;
} NecroSnapshotArena;

NecroSnapshotArena necro_snapshot_arena_empty();
NecroSnapshotArena necro_snapshot_arena_create();
void               necro_snapshot_arena_destroy(NecroSnapshotArena* arena);
void*              necro_snapshot_arena_alloc(NecroSnapshotArena* arena, size_t bytes);
NecroArenaSnapshot necro_snapshot_arena_get(NecroSnapshotArena* arena);
void               necro_snapshot_arena_rewind(NecroSnapshotArena* arena, NecroArenaSnapshot snapshot);
char*              necro_snapshot_arena_concat_strings(NecroSnapshotArena* arena, uint32_t string_count, const char** strings);
char*              necro_snapshot_arena_concat_strings_with_lengths(NecroSnapshotArena* arena, uint32_t string_count, const char** strings, size_t* lengths);

#endif // ARENA_H

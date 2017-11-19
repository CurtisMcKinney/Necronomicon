/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef SLAB_H
#define SLAB_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utility.h"

//=====================================================
// Slab Allocator
//=====================================================
#define NECRO_SLAB_STEP_SIZE         8
#define NECRO_SLAB_STEP_SIZE_POW_2   3
#define NECRO_NUM_SLAB_STEPS         16
#define DEBUG_SLAB_ALLOCATOR         0

#if DEBUG_SLAB_ALLOCATOR
#define TRACE_SLAB_ALLOCATOR(...) printf(__VA_ARGS__)
#else
#define TRACE_SLAB_ALLOCATOR(...)
#endif

typedef struct NecroSlabPage
{
    struct NecroSlabPage* next_page;
} NecroSlabPage;

typedef struct
{
    NecroSlabPage* page_list;
    char*          free_lists[NECRO_NUM_SLAB_STEPS];
    size_t         page_sizes[NECRO_NUM_SLAB_STEPS];
} NecroSlabAllocator;

NecroSlabAllocator necro_create_slab_allocator(size_t initial_page_size);
void               necro_alloc_slab_page(NecroSlabAllocator* slab_allocator, size_t slab_bin);
void*              necro_alloc_slab(NecroSlabAllocator* slab_allocator, size_t size);
void               necro_free_slab(NecroSlabAllocator* slab_allocator, void* data, size_t size);
void               necro_destroy_slab_allocator(NecroSlabAllocator* slab_allocator);
void               necro_bench_slab();

#endif // SLAB_H

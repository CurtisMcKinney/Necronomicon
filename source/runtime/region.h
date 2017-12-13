/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef REGION_H
#define REGION_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utility.h"

//=====================================================
// Region Allocator
//     - Hybrid Region / Slab Allocator
//     - Like Region allocator you can collect all the memory in a single O(1) operation
//     - Like a slab allocator you can allocate and deallocate memory of a set fixed of fixed sizes on O(1)
//     - Regions are arranged in a stack
//     - Pushing a region to the top of the stack
//     - All allocations from the region allocator
//       are allocated into the top of the region stack
//       and occur in O(1)
//     - Freeing the top pops it from the stack and removes all memory
//       from that region AND all sub-regions in O(1)
//     - Memory allocation which backs all this up is done via pages
//     - When the free list is empty a new page (double in size from the last)
//       is allocated.
//     - The regions in this new memory are lazily added to the free_list each allocation
//=====================================================
#define NECRO_REGION_INITIAL_PAGE_SIZE 16
#define NECRO_REGION_NUM_BINS          8
#define NECRO_REGION_STACK_SIZE        64

// TODO: Switch head and tail to use bottom bin nodes
// TODO: NecroArchive

typedef struct NecroRegionPage
{
    struct NecroRegionPage* next_page;
} NecroRegionPage;

typedef struct NecroRegionNode
{
    struct NecroRegionNode* prev;
    struct NecroRegionNode* next;
    uint32_t                bin;
    // Data is directly after node in memory
} NecroRegionNode;

typedef struct NecroRegion
{
    NecroRegionNode* head[NECRO_REGION_NUM_BINS];
    NecroRegionNode* tail[NECRO_REGION_NUM_BINS];
} NecroRegion;

typedef struct NecroRegionBin
{
    NecroRegionPage* page_list;
    NecroRegionNode* free_nodes;
    NecroRegionNode* new_nodes;
    size_t           new_nodes_size;
    size_t           new_nodes_index;
    size_t           data_size;
} NecroRegionBin;

typedef struct
{
    NecroRegionBin   bins[NECRO_REGION_NUM_BINS];
    NecroRegion      stack[NECRO_REGION_STACK_SIZE];
    size_t           stack_pointer;
} NecroRegionAllocator;

NecroRegionAllocator necro_create_region_allocator();
NecroRegion          necro_region_new_region(NecroRegionAllocator* allocator);
void                 necro_region_push_region(NecroRegionAllocator* allocator, NecroRegion region);
void                 necro_region_pop_region(NecroRegionAllocator* allocator);
void                 necro_region_destroy_region(NecroRegionAllocator* allocator);
void*                necro_region_alloc(NecroRegionAllocator* allocator, size_t data_size);
void                 necro_region_free(NecroRegionAllocator* allocator, void* data);
void                 necro_destroy_region_allocator();
void                 necro_region_test();
void                 necro_region_print(NecroRegionAllocator* allocator);

#endif // REGION_H
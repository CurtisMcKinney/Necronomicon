/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "region.h"

#define DEBUG_REGION_ALLOCATOR         1

#if DEBUG_REGION_ALLOCATOR
#define TRACE_REGION_ALLOCATOR(...) printf(__VA_ARGS__)
#else
#define TRACE_REGION_ALLOCATOR(...)
#endif

//=====================================================
// Region Allocator
//=====================================================
// Uses lazy doubling scheme
inline necro_alloc_node_page(NecroRegionAllocator* allocator)
{
    assert(allocator != NULL);
    allocator->new_nodes_size *= 2;
    NecroRegionPage* page      = malloc(sizeof(NecroRegionPage) + sizeof(NecroRegionNode) * allocator->new_nodes_size);
    TRACE_REGION_ALLOCATOR("allocating node page: %p\n", page);
    if (page == NULL)
    {
        fprintf(stderr, "Allocation error: could not allocate %zu of memory for NecroRegionAllocator", sizeof(NecroRegionPage) * NECRO_REGION_INITIAL_PAGE_SIZE);
        exit(1);
    }
    page->next_page            = allocator->page_list;
    allocator->page_list       = page;
    allocator->new_nodes       = (NecroRegionNode*)(page + 1);
    allocator->new_nodes_index = 0;
}

inline NecroRegionNode* necro_alloc_node(NecroRegionAllocator* allocator)
{
    if (allocator->new_nodes_index < allocator->new_nodes_size)
    {
        NecroRegionNode* new_node   = allocator->new_nodes;
        allocator->new_nodes       += 1;
        allocator->new_nodes_index += 1;
        assert(new_node != NULL);
        return new_node;
    }
    else
    {
        if (allocator->new_nodes == NULL)
            necro_alloc_node_page(allocator);
        assert(allocator->free_nodes != NULL);
        NecroRegionNode* new_node = allocator->free_nodes;
        allocator->free_nodes = new_node->next;
        assert(new_node != NULL);
        return new_node;
    }
}

NecroRegionAllocator necro_create_region_allocator()
{
    NecroRegionAllocator allocator =
    {
        .page_list         = NULL,
        .free_nodes        = NULL,
        .new_nodes         = NULL,
        .new_nodes_size    = NECRO_REGION_INITIAL_PAGE_SIZE / 2,
        .new_nodes_index   = 0,
        .stack_pointer     = 0,
    };
    necro_alloc_node_page(&allocator);
    // necro_alloc_region_page(&allocator);
    NecroRegionNode* head = necro_alloc_node(&allocator);
    NecroRegionNode* tail = necro_alloc_node(&allocator);
    *head = (NecroRegionNode)
    {
        .prev      = NULL,
        .next      = tail,
        .data_size = 0
    };
    *tail = (NecroRegionNode)
    {
        .next      = NULL,
        .prev      = head,
        .data_size = 0
    };
    allocator.stack[0] = (NecroRegion)
    {
        .head       = head,
        .tail       = tail
    };
    return allocator;
}

void necro_destroy_region_allocator(NecroRegionAllocator* allocator)
{
    NecroRegionPage* current_page = allocator->page_list;
    while (current_page != NULL)
    {
        TRACE_REGION_ALLOCATOR("freeing page:    %p\n", current_page);
        NecroRegionPage* next_page = current_page->next_page;
        free(current_page);
        current_page = next_page;
    }
    *allocator = (NecroRegionAllocator)
    {
        .page_list       = NULL,
        .new_nodes       = NULL,
        .new_nodes_size  = 0,
        .new_nodes_index = 0,
        .free_nodes      = NULL,
        .stack_pointer   = 0,
    };
}

NecroRegion necro_region_new(NecroRegionAllocator* allocator)
{
    NecroRegion*     top = allocator->stack + allocator->stack_pointer;
    NecroRegionNode* head = necro_alloc_node(allocator);
    NecroRegionNode* tail = necro_alloc_node(allocator);
    *head = (NecroRegionNode)
    {
        .prev      = top->head,
        .next      = tail,
        .data_size = 0
    };
    *tail = (NecroRegionNode)
    {
        .next      = top->head->next,
        .prev      = head,
        .data_size = 0
    };
    top->head->next->prev = tail;
    top->head->next       = head;
    NecroRegion new_region =
    {
        .head       = head,
        .tail       = tail
    };
    return new_region;
}

void necro_region_push_region(NecroRegionAllocator* allocator, NecroRegion region)
{
    assert(allocator != NULL);
    allocator->stack_pointer++;
    assert(allocator->stack_pointer < NECRO_REGION_STACK_SIZE);
    allocator->stack[allocator->stack_pointer] = region;
}

void necro_region_pop_region(NecroRegionAllocator* allocator)
{
    assert(allocator != NULL);
    allocator->stack_pointer--;
    assert(allocator->stack_pointer >= 0);
}

void necro_region_free_region(NecroRegionAllocator* allocator)
{
    assert(allocator != NULL);
    assert(allocator->stack_pointer > 0);
    NecroRegion* top = allocator->stack + allocator->stack_pointer;
    assert(top != NULL);
    assert(top->head != NULL);
    assert(top->tail != NULL);
    assert(top->head->prev != NULL);
    assert(top->tail->next != NULL);
    top->tail->next->prev = top->head->prev;
    top->head->prev->next = top->tail->next;
    top->tail->next       = allocator->free_nodes;
    allocator->free_nodes = top->head;
    necro_region_pop_region(allocator);
}

void* necro_region_alloc(NecroRegionAllocator* allocator, size_t data_size)
{
    assert(allocator != NULL);
    // assert(data_size <= NECRO_REGION_DATA_SIZE); // TODO: Update
    assert(allocator->stack != NULL);
    NecroRegionNode* node = necro_alloc_node(allocator);
    NecroRegion*     top  = allocator->stack + allocator->stack_pointer;
    assert(top->head != NULL);
    assert(top->tail != NULL);
    *node = (NecroRegionNode)
    {
        .prev = top->head,
        .next = top->head->next,
    };
    top->head->next->prev = node;
    top->head->next       = node;
    return node + 1; // Data directly after node in memory
}

void necro_region_free(NecroRegionAllocator* allocator, void* data)
{
    assert(allocator != NULL);
    assert(data != NULL);
    NecroRegion*     top  = allocator->stack + allocator->stack_pointer;
    NecroRegionNode* node = ((NecroRegionNode*)data) - 1;
    assert(top != NULL);
    assert(top->head != NULL);
    assert(top->tail != NULL);
    assert(node != NULL);
    assert(node != top->head);
    assert(node != top->tail);
    node->prev->next      = node->next;
    node->next->prev      = node->prev;
    node->next            = allocator->free_nodes;
    allocator->free_nodes = node;
}

inline void print_white_space(size_t white_count)
{
    for (size_t i = 0; i < white_count; ++i)
        printf(" ");
}

void necro_region_print_node(NecroRegionNode* node, size_t white_count)
{
    print_white_space(white_count);
    if (node == NULL)
    {
        printf("NecroRegionNode == NULL\n");
    }
    printf("NecroRegionNode\n");
    print_white_space(white_count);
    printf("{\n");
    print_white_space(white_count);
    printf("    prev: %p\n", node->prev);
    print_white_space(white_count);
    printf("    next: %p\n", node->next);
    print_white_space(white_count);
    printf("    size: %d\n", node->data_size);
    print_white_space(white_count);
    printf("}\n");
}

void necro_region_print_region(NecroRegion* region, size_t white_count)
{
    print_white_space(white_count);
    if (region == NULL)
    {
        printf("NecroRegion == NULL\n");
    }
    printf("NecroRegion\n");
    print_white_space(white_count);
    printf("{\n");
    print_white_space(white_count);
    printf("    head, %p:\n", region->head);
    necro_region_print_node(region->head, white_count + 8);
    print_white_space(white_count);
    printf("    tail, %p:\n", region->tail);
    necro_region_print_node(region->tail, white_count + 8);
    print_white_space(white_count);
    printf("}\n");
}

void necro_region_print(NecroRegionAllocator* allocator)
{
    assert(allocator != NULL);
    printf("NecroRegionAllocator\n{\n");
    printf("    new_nodes_size:  %d\n", allocator->new_nodes_size);
    printf("    new_nodes_index: %d\n", allocator->new_nodes_index);
    size_t free_node_count = 0;
    NecroRegionNode* curr_node = allocator->free_nodes;
    while (curr_node != NULL)
    {
        free_node_count++;
        curr_node = curr_node->next;
    }
    printf("    free_nodes:      %d\n", free_node_count);
    printf("    stack:\n\n");
    for (size_t i = 0; i <= allocator->stack_pointer; i++)
    {
        necro_region_print_region(allocator->stack + i, 4);
    }
    printf("\n}\n");
}

void necro_region_test()
{
    puts("\n------");
    puts("Test Region Allocator\n");
    NecroRegionAllocator region_allocator = necro_create_region_allocator();
    assert(region_allocator.page_list != NULL);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test New Region\n");
    NecroRegion region = necro_region_new(&region_allocator);
    necro_region_push_region(&region_allocator, region);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test Free Region 1\n");
    necro_region_free_region(&region_allocator);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test allocation 1\n");
    region = necro_region_new(&region_allocator);
    necro_region_push_region(&region_allocator, region);
    char* data = necro_region_alloc(&region_allocator, 16);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test free 1\n");
    necro_region_free(&region_allocator, data);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test Free Region 2\n");
    necro_region_free_region(&region_allocator);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test allocation 2\n");
    region = necro_region_new(&region_allocator);
    necro_region_push_region(&region_allocator, region);
    char* data1 = necro_region_alloc(&region_allocator, 16);
    char* data2 = necro_region_alloc(&region_allocator, 16);
    necro_region_print(&region_allocator);

    puts("\n------");
    puts("Test allocation 2\n");
    necro_region_free_region(&region_allocator);
    necro_region_print(&region_allocator);
}
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "region.h"

#define DEBUG_REGION_ALLOCATOR 0

#if DEBUG_REGION_ALLOCATOR
#define TRACE_REGION_ALLOCATOR(...) printf(__VA_ARGS__)
#else
#define TRACE_REGION_ALLOCATOR(...)
#endif

//=====================================================
// Region Allocator
//=====================================================
inline uint32_t data_size_to_bin(uint32_t data_size)
{
    static const int deBruijnBitPosition[32] =
    {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };
    const uint32_t one_less_pow_2 = next_highest_pow_of_2(data_size) - 1;
    return deBruijnBitPosition[(uint32_t)(one_less_pow_2 * 0x07C4ACDDU) >> 27] - 2;
}

// Uses lazy doubling scheme
inline void necro_alloc_node_page(NecroRegionBin* bin)
{
    assert(bin != NULL);
    bin->new_nodes_size       *= 2;
    size_t           page_size = sizeof(NecroRegionPage) + (sizeof(NecroRegionNode) + bin->data_size) * bin->new_nodes_size;
    NecroRegionPage* page      = malloc(page_size);
    TRACE_REGION_ALLOCATOR("allocating node page %p, of size: %d\n", page, page_size);
    if (page == NULL)
    {
        fprintf(stderr, "Allocation error: could not allocate %zu of memory for NecroRegionAllocator", page_size);
        exit(1);
    }
    page->next_page       = bin->page_list;
    bin->page_list        = page;
    bin->free_nodes       = (NecroRegionNode*)(page + 1);
    bin->free_nodes->next = NULL;
    bin->new_nodes_index  = 1;
    bin->new_nodes        = (NecroRegionNode*)(((char*)bin->free_nodes) + (sizeof(NecroRegionNode) + bin->data_size));
}

inline NecroRegionNode* necro_alloc_node(NecroRegionBin* bin)
{
    if (bin->new_nodes_index < bin->new_nodes_size)
    {
        NecroRegionNode* new_node  = bin->new_nodes;
        bin->new_nodes             = (NecroRegionNode*)(((char*)new_node) + sizeof(NecroRegionNode) + bin->data_size);
        bin->new_nodes_index      += 1;
        assert(new_node != NULL);
        return new_node;
    }
    else
    {
        if (bin->free_nodes == NULL)
            necro_alloc_node_page(bin);
        assert(bin->free_nodes != NULL);
        NecroRegionNode* new_node = bin->free_nodes;
        bin->free_nodes = new_node->next;
        assert(new_node != NULL);
        return new_node;
    }
}

NecroRegionAllocator necro_create_region_allocator()
{
    NecroRegionAllocator allocator =
    {
        .stack_pointer = 0,
    };
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        allocator.bins[bin] = (NecroRegionBin)
        {
            .page_list       = NULL,
            .free_nodes      = NULL,
            .new_nodes       = NULL,
            .new_nodes_size  = NECRO_REGION_INITIAL_PAGE_SIZE / 2,
            .new_nodes_index = 0,
            .data_size       = 8 << bin,
        };
        necro_alloc_node_page(allocator.bins + bin);
        NecroRegionNode* head = necro_alloc_node(allocator.bins + bin);
        NecroRegionNode* tail = necro_alloc_node(allocator.bins + bin);
        assert(bin < UINT32_MAX);
        *head = (NecroRegionNode)
        {
            .prev = NULL,
            .next = tail,
            .bin  = (uint32_t) bin
        };
        *tail = (NecroRegionNode)
        {
            .next = NULL,
            .prev = head,
            .bin  = (uint32_t) bin
        };
        allocator.stack[0].head[bin] = head;
        allocator.stack[0].tail[bin] = tail;
    }
    return allocator;
}

void necro_destroy_region_allocator(NecroRegionAllocator* allocator)
{
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        NecroRegionPage* current_page = allocator->bins[bin].page_list;
        while (current_page != NULL)
        {
            TRACE_REGION_ALLOCATOR("freeing page:    %p\n", current_page);
            NecroRegionPage* next_page = current_page->next_page;
            free(current_page);
            current_page = next_page;
        }
        allocator->bins[bin] = (NecroRegionBin)
        {
            .page_list       = NULL,
            .free_nodes      = NULL,
            .new_nodes       = NULL,
            .new_nodes_size  = 0,
            .new_nodes_index = 0,
            .data_size       = 0
        };
    }
    *allocator = (NecroRegionAllocator)
    {
        .stack_pointer = 0,
    };
}

NecroRegion necro_region_new_region(NecroRegionAllocator* allocator)
{
    NecroRegion* top = allocator->stack + allocator->stack_pointer;
    NecroRegion  new_region;
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        NecroRegionNode* head = necro_alloc_node(allocator->bins + bin);
        NecroRegionNode* tail = necro_alloc_node(allocator->bins + bin);
        assert(bin < UINT32_MAX);
        *head = (NecroRegionNode)
        {
            .prev = top->head[bin],
            .next = tail,
            .bin  = (uint32_t) bin
        };
        *tail = (NecroRegionNode)
        {
            .next = top->head[bin]->next,
            .prev = head,
            .bin  = (uint32_t) bin
        };
        top->head[bin]->next->prev = tail;
        top->head[bin]->next       = head;
        new_region.head[bin]       = head;
        new_region.tail[bin]       = tail;
    }
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

void necro_region_destroy_region(NecroRegionAllocator* allocator)
{
    assert(allocator != NULL);
    assert(allocator->stack_pointer > 0);
    NecroRegion* top = allocator->stack + allocator->stack_pointer;
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        assert(top != NULL);
        assert(top->head[bin] != NULL);
        assert(top->tail[bin] != NULL);
        assert(top->head[bin]->prev != NULL);
        assert(top->tail[bin]->next != NULL);
        top->tail[bin]->next->prev      = top->head[bin]->prev;
        top->head[bin]->prev->next      = top->tail[bin]->next;
        top->tail[bin]->next            = allocator->bins[bin].free_nodes;
        allocator->bins[bin].free_nodes = top->head[bin];
    }
    necro_region_pop_region(allocator);
}

void* necro_region_alloc(NecroRegionAllocator* allocator, size_t data_size)
{
    assert(data_size <= UINT32_MAX);
    size_t bin = data_size_to_bin((uint32_t) data_size);
    TRACE_REGION_ALLOCATOR("alloc, data_size: %d, bin: %d\n", data_size, bin);
    assert(allocator != NULL);
    assert(bin < NECRO_REGION_NUM_BINS);
    assert(allocator->stack != NULL);
    NecroRegionNode* node = necro_alloc_node(allocator->bins + bin);
    NecroRegion*     top  = allocator->stack + allocator->stack_pointer;
    assert(top->head != NULL);
    assert(top->tail != NULL);
    assert(bin <= UINT32_MAX);
    *node = (NecroRegionNode)
    {
        .prev = top->head[bin],
        .next = top->head[bin]->next,
        .bin  = (uint32_t) bin
    };
    top->head[bin]->next->prev = node;
    top->head[bin]->next       = node;
    return node + 1; // Data directly after node in memory
}

void necro_region_free(NecroRegionAllocator* allocator, void* data)
{
    assert(allocator != NULL);
    assert(data != NULL);
    NecroRegion*     top  = allocator->stack + allocator->stack_pointer;
    NecroRegionNode* node = ((NecroRegionNode*)data) - 1;
    size_t           bin  = node->bin;
    assert(top != NULL);
    assert(top->head[bin] != NULL);
    assert(top->tail[bin] != NULL);
    assert(node != NULL);
    assert(node != top->head[bin]);
    assert(node != top->tail[bin]);
    node->prev->next                = node->next;
    node->next->prev                = node->prev;
    node->next                      = allocator->bins[bin].free_nodes;
    allocator->bins[bin].free_nodes = node;
}

inline size_t necro_region_count_free_list_in_bin(NecroRegionAllocator* allocator, size_t bin)
{
    size_t free_node_count = 0;
    NecroRegionNode* curr_node = allocator->bins[bin].free_nodes;
    while (curr_node != NULL)
    {
        free_node_count++;
        curr_node = curr_node->next;
    }
    return free_node_count;
}

inline size_t necro_region_bin_size(NecroRegion* region, size_t bin)
{
    size_t count = 0;
    NecroRegionNode* curr_node = region->head[bin]->next;
    while (curr_node != region->tail[bin])
    {
        count++;
        curr_node = curr_node->next;
    }
    return count;
}

void necro_region_print_node(NecroRegionNode* node)
{
    if (node == NULL)
    {
        printf("NecroRegionNode == NULL\n");
    }
    printf("{ addr: %p, prev: %p, next: %p, bin: %d }", node, node->prev, node->next, node->bin);
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
    printf("    head:\n");
    print_white_space(white_count + 4);
    printf("{\n");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        print_white_space(white_count + 8);
        necro_region_print_node(region->head[bin]);
        printf(",\n");
    }
    print_white_space(white_count + 4);
    printf("}\n");
    print_white_space(white_count);
    printf("    tail:\n");
    print_white_space(white_count + 4);
    printf("{\n");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        print_white_space(white_count + 8);
        necro_region_print_node(region->tail[bin]);
        printf(",\n");
    }
    print_white_space(white_count + 4);
    printf("}\n");
    print_white_space(white_count);
    printf("    size: {");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
    {
        printf("%zu, ", necro_region_bin_size(region, bin));
    }
    printf("}\n");
    print_white_space(white_count);
    printf("}\n");
}

void necro_region_print(NecroRegionAllocator* allocator)
{
    assert(allocator != NULL);
    printf("NecroRegionAllocator\n{\n");

    printf("    data_size:       {");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
        printf("%zu, ", allocator->bins[bin].data_size);
    printf("}\n");

    printf("    new_nodes_size:  {");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
        printf("%zu, ", allocator->bins[bin].new_nodes_size);
    printf("}\n");

    printf("    new_nodes_index: {");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
        printf("%zu, ", allocator->bins[bin].new_nodes_index);
    printf("}\n");

    printf("    free_nodes:      {");
    for (size_t bin = 0; bin < NECRO_REGION_NUM_BINS; ++bin)
        printf("%zu, ", necro_region_count_free_list_in_bin(allocator, bin));
    printf("}\n");
    printf("    stack:\n    {\n");
    for (size_t i = 0; i <= allocator->stack_pointer; ++i)
    {
        printf("        [%zu] = \n", i);
        necro_region_print_region(allocator->stack + i, 8);
        printf("        \n");
    }
    printf("    }\n");
    printf("\n}\n");
}

void necro_region_test()
{
    necro_announce_phase("NecroRegion");

    printf("Test Region Allocator: ");
    NecroRegionAllocator region_allocator = necro_create_region_allocator();
    if (region_allocator.bins[4].new_nodes_index == 3 && necro_region_count_free_list_in_bin(&region_allocator, 4) == 1)
        printf("passed\n");
    else
        printf("FAILED\n");


    printf("Test New Region:       ");
    NecroRegion region = necro_region_new_region(&region_allocator);
    necro_region_push_region(&region_allocator, region);
    if (region_allocator.bins[3].new_nodes_index == 5 && necro_region_count_free_list_in_bin(&region_allocator, 3) == 1 && necro_region_bin_size(&region, 3) == 0)
        printf("passed\n");
    else
        printf("FAILED\n");

    printf("Test Free Region 1:    ");
    necro_region_destroy_region(&region_allocator);
    if (region_allocator.bins[0].new_nodes_index == 5 && necro_region_count_free_list_in_bin(&region_allocator, 0) == 3 && necro_region_bin_size(region_allocator.stack, 4) == 0)
        printf("passed\n");
    else
        printf("FAILED\n");

    printf("Test allocation 1:     ");
    region = necro_region_new_region(&region_allocator);
    necro_region_push_region(&region_allocator, region);
    char* data = necro_region_alloc(&region_allocator, 16);
    if (region_allocator.bins[1].new_nodes_index == 8 && necro_region_bin_size(&region, 0) == 0 && necro_region_bin_size(&region, 1) == 1)
        printf("passed\n");
    else
        printf("FAILED\n");

    printf("Test Free Region 2:    ");
    int64_t* i_data = necro_region_alloc(&region_allocator, 8);
    *i_data         = 666;
    if (region_allocator.bins[1].new_nodes_index == 8 && necro_region_bin_size(&region, 0) == 1 && necro_region_bin_size(&region, 1) == 1)
        printf("passed\n");
    else
        printf("FAILED\n");

    printf("Test free 1:           ");
    necro_region_free(&region_allocator, data);
    if (region_allocator.bins[1].new_nodes_index == 8 && necro_region_count_free_list_in_bin(&region_allocator, 1) == 4 && region_allocator.bins[0].new_nodes_index == 8 && necro_region_count_free_list_in_bin(&region_allocator, 0) == 3 &&
        necro_region_bin_size(&region, 0) == 1 && necro_region_bin_size(&region, 1) == 0)
        printf("passed\n");
    else
        printf("FAILED\n");

    printf("Test Free Region 2:    ");
    necro_region_destroy_region(&region_allocator);
    if (region_allocator.bins[1].new_nodes_index == 8 && necro_region_count_free_list_in_bin(&region_allocator, 1) == 6 && region_allocator.bins[0].new_nodes_index == 8 && necro_region_count_free_list_in_bin(&region_allocator, 0) == 6 &&
        necro_region_bin_size(region_allocator.stack, 0) == 0 && necro_region_bin_size(region_allocator.stack, 1) == 0)
        printf("passed\n");
    else
        printf("FAILED\n");

    // necro_region_print(&region_allocator);

    necro_destroy_region_allocator(&region_allocator);

    // TODO: Nodes in circulation test

    // puts("\n------");
    // puts("Test allocation 2\n");
    // region = necro_region_new(&region_allocator);
    // necro_region_push_region(&region_allocator, region);
    // char* data1 = necro_region_alloc(&region_allocator, 16);
    // char* data2 = necro_region_alloc(&region_allocator, 16);
    // necro_region_print(&region_allocator);

    // puts("\n------");
    // puts("Test allocation 2\n");
    // necro_region_destroy_region(&region_allocator);
    // necro_region_print(&region_allocator);

    // necro_region_print(&region_allocator);
}
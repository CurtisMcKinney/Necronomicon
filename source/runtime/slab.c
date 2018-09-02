/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "slab.h"

#define DEBUG_SLAB_ALLOCATOR         0

#if DEBUG_SLAB_ALLOCATOR
#define TRACE_SLAB_ALLOCATOR(...) printf(__VA_ARGS__)
#else
#define TRACE_SLAB_ALLOCATOR(...)
#endif

//=====================================================
// Slab Allocator
//=====================================================
NecroSlabAllocator necro_create_slab_allocator(size_t initial_page_size)
{
    NecroSlabAllocator slab_allocator;
    slab_allocator.page_list = NULL;
    for (size_t i = 0; i < NECRO_NUM_SLAB_STEPS; ++i)
    {
        slab_allocator.free_lists[i] = NULL;
        slab_allocator.page_sizes[i] = initial_page_size / 2;
        necro_alloc_slab_page(&slab_allocator, i);
    }
    return slab_allocator;
}

void necro_alloc_slab_page(NecroSlabAllocator* slab_allocator, size_t slab_bin)
{
    slab_allocator->page_sizes[slab_bin] *= 2;
    size_t          size                  = (slab_bin + 1) * NECRO_SLAB_STEP_SIZE;
    NecroSlabPage*  page                  = malloc(sizeof(NecroSlabPage) + size * slab_allocator->page_sizes[slab_bin]);
    TRACE_SLAB_ALLOCATOR("allocating page: %p\n", page);
    if (page == NULL)
    {
        fprintf(stderr, "Allocation error: could not allocate %zu of memory for NecroSlabAllocator", sizeof(NecroSlabPage) + size * slab_allocator->page_sizes[slab_bin]);
        exit(1);
    }
    char* data                            = (char*)(page + 1);
    page->next_page                       = slab_allocator->page_list;
    slab_allocator->page_list             = page;
    for (size_t i = 0; i < slab_allocator->page_sizes[slab_bin]; ++i)
    {
        *((char**)(data + size * i))         = slab_allocator->free_lists[slab_bin];
        slab_allocator->free_lists[slab_bin] = data + size * i;
    }
}

void* necro_alloc_slab(NecroSlabAllocator* slab_allocator, size_t size)
{
    assert(size <= NECRO_NUM_SLAB_STEPS * NECRO_SLAB_STEP_SIZE);
    size_t slab_bin = (size >> NECRO_SLAB_STEP_SIZE_POW_2) - 1;
    if (slab_allocator->free_lists[slab_bin] == NULL)
    {
        TRACE_SLAB_ALLOCATOR("necro_alloc_slab_page, bin: %zu\n", slab_bin);
        necro_alloc_slab_page(slab_allocator, slab_bin);
    }
    assert(slab_allocator->free_lists[slab_bin] != NULL);
    char* slab = slab_allocator->free_lists[slab_bin];
    slab_allocator->free_lists[slab_bin] = *((char**)slab);
    TRACE_SLAB_ALLOCATOR("allocating slab: %p, size: %zu, bin: %zu\n", slab, size, slab_bin);
    return slab;
}

void necro_free_slab(NecroSlabAllocator* slab_allocator, void* data, size_t size)
{
    assert(size <= NECRO_NUM_SLAB_STEPS * NECRO_SLAB_STEP_SIZE);
    size_t slab_bin = (size >> NECRO_SLAB_STEP_SIZE_POW_2) - 1;
    TRACE_SLAB_ALLOCATOR("freeing slab:    %p, size: %zu, bin: %zu\n", data, size, slab_bin);
    *((char**)data) = slab_allocator->free_lists[slab_bin];
    slab_allocator->free_lists[slab_bin] = data;
}

void necro_destroy_slab_allocator(NecroSlabAllocator* slab_allocator)
{
    NecroSlabPage* current_page = slab_allocator->page_list;
    while (current_page != NULL)
    {
        TRACE_SLAB_ALLOCATOR("freeing page:    %p\n", current_page);
        NecroSlabPage* next_page = current_page->next_page;
        free(current_page);
        current_page = next_page;
    }
    slab_allocator->page_list = NULL;
    for (size_t i = 0; i < NECRO_NUM_SLAB_STEPS; ++i)
    {
        slab_allocator->free_lists[i] = NULL;
        slab_allocator->page_sizes[i] = 0;
    }
}

void print_test_result(const char* print_string, bool result)
{
    if (result)
        printf("%s passed\n", print_string);
    else
        printf("%s FAILED\n", print_string);
}

void necro_test_slab()
{
    puts("\n------");
    puts("Test Slab Allocator\n");
    NecroSlabAllocator slab_allocator = necro_create_slab_allocator(16384);
    assert(slab_allocator.page_list != NULL);
    for (size_t i = 0; i < NECRO_NUM_SLAB_STEPS; ++i)
    {
        assert(slab_allocator.page_sizes[i] == 16384);
        assert(slab_allocator.free_lists[i] != NULL);
    }

    int64_t* vec3_1 = necro_alloc_slab(&slab_allocator, sizeof(int64_t) * 3);
    vec3_1[0] = 4;
    vec3_1[1] = 5;
    vec3_1[2] = 6;

    int64_t* vec3_2 = necro_alloc_slab(&slab_allocator, sizeof(int64_t) * 3);
    vec3_2[0] = 6;
    vec3_2[1] = 5;
    vec3_2[2] = 4;

    print_test_result("vec 1 + 2 alloc:", vec3_1[0] == vec3_2[2] && vec3_1[1] == vec3_2[1] && vec3_1[2] == vec3_2[0]);
    necro_free_slab(&slab_allocator, vec3_1, sizeof(int64_t) * 3);
    print_test_result("vec 2 + 3 alloc:", vec3_2[0] == 6 && vec3_2[1] == 5 && vec3_2[2] == 4);

    int64_t* vec3_3 = necro_alloc_slab(&slab_allocator, sizeof(int64_t) * 3);
    vec3_3[0] = 5;
    vec3_3[1] = 4;
    vec3_3[2] = 6;
    print_test_result("vec 2 + 3 alloc:", vec3_3[1] == vec3_2[2] && vec3_3[0] == vec3_2[1] && vec3_3[2] == vec3_2[0]);

    necro_free_slab(&slab_allocator, vec3_2, sizeof(int64_t) * 3);
    necro_free_slab(&slab_allocator, vec3_3, sizeof(int64_t) * 3);

    necro_destroy_slab_allocator(&slab_allocator);
    assert(slab_allocator.page_list == NULL);
    for (size_t i = 0; i < NECRO_NUM_SLAB_STEPS; ++i)
    {
        assert(slab_allocator.page_sizes[i] == 0);
        assert(slab_allocator.free_lists[i] == NULL);
    }

    necro_bench_slab();
}

void necro_bench_slab()
{
    int64_t iterations = 65536 * 4;
    {
        // Slab bench
        NecroSlabAllocator slab_allocator = necro_create_slab_allocator(16384);
        double start_time = (double)clock() / (double)CLOCKS_PER_SEC;
        int64_t* mem;
        for (int64_t i = 0; i < iterations; ++i)
        {
            mem = necro_alloc_slab(&slab_allocator, sizeof(int64_t) * 4);
            // necro_free_slab(&slab_allocator, mem, sizeof(int64_t) * 4);
        }
        double  end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
        double  run_time    = end_time - start_time;
        int64_t ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
        puts("");
        printf("salloc Necronomicon benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n", iterations, run_time, ns_per_iter);
        necro_destroy_slab_allocator(&slab_allocator);
    }

    {
        // C bench
        double start_time = (double)clock() / (double)CLOCKS_PER_SEC;
        size_t* mem;
        for (int64_t i = 0; i < iterations; ++i)
        {
            mem = malloc(sizeof(size_t) * 4);
            // free(mem);
        }
        double  end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
        double  run_time    = end_time - start_time;
        int64_t ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
        puts("");
        printf("malloc Necronomicon benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n", iterations, run_time, ns_per_iter);
    }

}
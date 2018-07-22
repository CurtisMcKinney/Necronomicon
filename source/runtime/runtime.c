/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "runtime.h"
#include "utility.h"
#include "mouse.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define NECRO_GC_SLAB_TO_PAGES 1024
#define NECRO_GC_NUM_SEGMENTS 10

//=====================================================
// GC
//=====================================================
typedef struct NecroGCPage
{
    struct NecroGCPage* next;
} NecroGCPage;

typedef struct NecroSlab
{
    struct NecroSlab* prev;
    struct NecroSlab* next;
    uint16_t          slots_used;
    uint8_t           segment;
    int8_t            color;
    uint32_t          tag; // Used by necro, not gc
} NecroSlab;

typedef struct
{
    NecroGCPage* pages;
    NecroSlab*   free;
    NecroSlab*   black;
    NecroSlab*   black_tail;
    NecroSlab*   off_white;
    NecroSlab*   off_white_tail;
    size_t       slab_size;
    uint8_t      segment;
    size_t       page_count;
} NecroGCSegment;

typedef struct NecroGCRoot
{
    char**   slots;
    uint32_t num_slots;
} NecroGCRoot;

typedef struct
{
    NecroGCSegment segments[NECRO_GC_NUM_SEGMENTS]; // Increases by powers of 2
    NecroSlab*     gray;
    NecroGCRoot*   roots;
    uint32_t       root_count;
    uint32_t       counter;
    int8_t         white_color;
} NecroGC;

NecroGC gc;

inline NecroSlab* necro_cons_slab(NecroSlab* slab_1, NecroSlab* slab_2)
{
    // assert(slab_1 != NULL);
    slab_1->next = slab_2;
    if (slab_2 != NULL)
        slab_2->prev = slab_1;
    return slab_1;
}

inline size_t necro_count_slabs(NecroSlab* slab)
{
    size_t count = 0;
    while (slab != NULL)
    {
        count++;
        slab = slab->next;
    }
    return count;
}

inline void necro_unlink_slab(NecroSlab* slab_1, NecroGCSegment* segment)
{
    NecroSlab* prev = slab_1->prev;
    NecroSlab* next = slab_1->next;
    if (slab_1 == segment->off_white_tail)
        segment->off_white_tail = prev;
    if (slab_1 == segment->off_white)
        segment->off_white = next;
    if (prev != NULL)
        prev->next = next;
    if (next != NULL)
        next->prev = prev;
    slab_1->prev = NULL;
    slab_1->next = NULL;
}

void necro_gc_alloc_page(NecroGCSegment* segment, int8_t white_color)
{
    segment->page_count++;
    // printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    // printf("!! alloc_page: segment: %d, pages: %d, slab_size: %d\n", segment->segment, segment->page_count, segment->slab_size);
    NecroGCPage* page = malloc(sizeof(NecroGCPage) + segment->slab_size * NECRO_GC_SLAB_TO_PAGES);
    memset(page, 0, sizeof(NecroGCPage) + segment->slab_size * NECRO_GC_SLAB_TO_PAGES);
    if (page == NULL)
    {
        assert(false && "Could not allocate memory for necro_gc_alloc_page");
    }
    page->next        = segment->pages;
    segment->pages    = page;
    char*        data = ((char*)page) + sizeof(NecroGCPage);
    for (size_t i = 0; i < NECRO_GC_SLAB_TO_PAGES; ++i)
    {
        NecroSlab* slab  = (NecroSlab*)data;
        slab->color      = white_color;
        slab->segment    = segment->segment;
        slab->slots_used = 0;
        slab->tag        = 0;
        slab->prev       = NULL;
        segment->free    = necro_cons_slab(slab, segment->free);
        data           += segment->slab_size;
    }
    assert(segment->free != NULL);
}

NecroGCSegment necro_create_gc_segment(size_t slab_size, uint8_t segment, int8_t white_color)
{
    NecroGCSegment gcs;
    gcs.slab_size      = slab_size;
    gcs.segment        = segment;
    gcs.off_white      = NULL;
    gcs.off_white_tail = NULL;
    gcs.black          = NULL;
    gcs.black_tail     = NULL;
    gcs.free           = NULL;
    gcs.pages          = NULL;
    gcs.page_count     = 0;
    necro_gc_alloc_page(&gcs, white_color);
    return gcs;
}

void necro_destroy_gc_segment(NecroGCSegment* gcs)
{
    assert(gcs != NULL);
    if (gcs->pages == NULL)
        return;
    NecroGCPage* page = gcs->pages;
    while (page != NULL)
    {
        NecroGCPage* next = page->next;
        free(page);
        page = next;
    }
    gcs->pages     = NULL;
    gcs->slab_size = 0;
}

void necro_ensure_off_white_not_null(NecroGCSegment* gcs)
{
    if (gcs->off_white != NULL)
    {
        assert(gcs->off_white_tail != NULL);
        return;
    }
    assert(gcs->off_white_tail == NULL);
    if (gcs->free == NULL)
        necro_gc_alloc_page(gcs, gc.white_color);
    NecroSlab* slab     = gcs->free;
    gcs->free           = slab->next;
    slab->slots_used    = 0;
    slab->segment       = gcs->segment;
    slab->prev          = NULL;
    slab->next          = NULL;
    slab->color         = gc.white_color;
    gcs->off_white      = slab;
    gcs->off_white_tail = slab;
}

NecroGC necro_create_gc()
{
    NecroGC gc;
    gc.gray        = NULL;
    gc.white_color = 1;
    gc.roots       = NULL;
    gc.root_count  = 0;
    gc.counter     = 0;
    size_t  slots = 1;
    for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
    {
        gc.segments[i] = necro_create_gc_segment((slots * sizeof(char*)) + sizeof(NecroSlab), (uint8_t)i, gc.white_color);
        necro_ensure_off_white_not_null(gc.segments + i);
        slots *= 2;
    }
    return gc;
}

void necro_destroy_gc(NecroGC* gc)
{
    assert(gc != NULL);
    for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
    {
        necro_destroy_gc_segment(gc->segments + i);
    }
    free(gc->roots);
    gc->root_count = 0;
}

inline NecroSlab* necro_gc_slot_to_slab(char* slot)
{
    assert(slot != NULL);
    return (NecroSlab*)(slot - (sizeof(NecroSlab*) * 2));
}

//-----------------
// GC API
//-----------------
void necro_gc_init()
{
    gc = necro_create_gc();
}

void necro_cleanup_gc()
{
    necro_destroy_gc(&gc);
}

void necro_gc_scan_root(char** slot, size_t slots_used)
{
    for (size_t i = 0; i < slots_used; ++i)
    {
        if ((*slot) == NULL)
            goto necro_gc_scan_slab_next;
        NecroSlab* slab = necro_gc_slot_to_slab(*slot);
        // assert(slab != NULL);
        if (slab->color != gc.white_color)
            goto necro_gc_scan_slab_next;
        slab->color = 0;
        necro_unlink_slab(slab, gc.segments + slab->segment);
        gc.gray = necro_cons_slab(slab, gc.gray);
        // printf("pre-scanned: %p, gray count: %d\n", slab, necro_count_slabs(gc.gray));
    necro_gc_scan_slab_next:
        slot++;
    }
}

void necro_gc_scan_slab(NecroSlab* a_slab)
{
    assert(a_slab != NULL && "NULL slab in necro_gc_scan_slab");
    char** slot = (char**)(a_slab + 1);
    for (size_t i = 0; i < a_slab->slots_used; ++i)
    {
        if ((*slot) == NULL)
            goto necro_gc_scan_slab_next;
        NecroSlab* slab = necro_gc_slot_to_slab(*slot);
        // assert(slab != NULL);
        if (slab->color != gc.white_color)
            goto necro_gc_scan_slab_next;
        slab->color = 0;
        necro_unlink_slab(slab, gc.segments + slab->segment);
        gc.gray = necro_cons_slab(slab, gc.gray);
        // printf("pre-scanned: %p, gray count: %d\n", slab, necro_count_slabs(gc.gray));
    necro_gc_scan_slab_next:
        slot++;
    }
    a_slab->color = -gc.white_color;
    gc.segments[a_slab->segment].black = necro_cons_slab(a_slab, gc.segments[a_slab->segment].black);
    if (gc.segments[a_slab->segment].black_tail == NULL)
        gc.segments[a_slab->segment].black_tail = gc.segments[a_slab->segment].black;
}

void necro_print_tab(size_t depth)
{
    for (size_t i = 0; i < depth; ++i)
    {
        printf("\t");
    }
}

void necro_print_slab_go(NecroSlab* slab, size_t depth)
{
    // if (!slab || depth > 1) return;
    if (!slab) return;
    necro_print_tab(depth);
    printf("--------------------------\n");
    necro_print_tab(depth);
    printf("slab:    %p\n", slab);
    necro_print_tab(depth);
    printf("prev:    %p\n", slab->prev);
    necro_print_tab(depth);
    printf("next:    %p\n", slab->next);
    necro_print_tab(depth);
    printf("slots:   %d\n", slab->slots_used);
    necro_print_tab(depth);
    printf("segment: %d\n", slab->segment);
    necro_print_tab(depth);
    printf("color:   %d\n", slab->color);
    necro_print_tab(depth);
    printf("tag:     %d\n", slab->tag);
    char** slot = (char**)(slab + 1);
    for (size_t i = 0; i < slab->slots_used; ++i)
    {
        // NecroSlab* m_slab = necro_gc_slot_to_slab(*slot);
        necro_print_tab(depth);
        printf("slot: %p, member:  %p\n", slot, *slot);
        if ((*slot) != NULL)
            necro_print_slab_go(necro_gc_slot_to_slab(*slot), depth + 1);
        // slot += sizeof(char*);
        slot++;
        // slot += 4;
    }
    necro_print_tab(depth);
    printf("--------------------------\n");
}

void necro_print_slab(NecroSlab* slab)
{
    necro_print_slab_go(slab, 0);
}

//=====================================================
// Runtime functions
//=====================================================
extern DLLEXPORT void _necro_initialize_root_set(uint32_t root_count)
{
    // printf("init root_set, root_count: %d\n", root_count);
    gc.root_count = root_count;
    gc.roots      = malloc(root_count * sizeof(NecroGCRoot));
    if (gc.roots == 0)
    {
        fprintf(stderr, "Couldn't allocate memory in _necro_initialize_root_set");
    }
    for (size_t i = 0; i < root_count; ++i)
    {
        gc.roots[i].slots     = NULL;
        gc.roots[i].num_slots = 0;
    }
}

// overflowing slots used!??!?!?!
extern DLLEXPORT void _necro_set_root(uint32_t* root, uint32_t root_index, uint32_t num_slots)
{
    // printf("set root, index: %d, root: %p\n", root_index, root);
    // NecroSlab* slab      = necro_gc_slot_to_slab((char*)root);
    gc.roots[root_index].slots     = (char**)(((char*) root) + sizeof(uint64_t));
    gc.roots[root_index].num_slots = num_slots;
    // necro_print_slab(slab);
}

extern DLLEXPORT void _necro_collect()
{
    gc.counter++;
    if (gc.counter < 10)
        return;
    // printf("collect!, white_color: %d\n", gc.white_color);
// #if _WIN32
//     LARGE_INTEGER ticks_per_sec;
//     LARGE_INTEGER gc_begin_time;
//     LARGE_INTEGER gc_end_time;
//     double        gc_time;
//     QueryPerformanceFrequency(&ticks_per_sec);
//     QueryPerformanceCounter(&gc_begin_time);
// #endif
    gc.counter = 0;
    for (size_t i = 0; i < gc.root_count; ++i)
    {
        // necro_print_slab(gc.roots[i]);
        // assert(gc.roots[i] != NULL && "NULL root");
        necro_gc_scan_root(gc.roots[i].slots, gc.roots[i].num_slots);
    }
    while (gc.gray != NULL)
    {
        NecroSlab* slab = gc.gray;
        gc.gray = gc.gray->next;
        // printf("scan: %p, gray count: %d\n", slab, necro_count_slabs(gc.gray));
        slab->next = NULL;
        slab->prev = NULL;
        necro_gc_scan_slab(slab);
    }
    gc.white_color = -gc.white_color;
    for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
    {
        // size_t white_count = necro_count_slabs(gc.segments[i].off_white);
        // size_t black_count = necro_count_slabs(gc.segments[i].black);

        //------------------------------
        // Move off_white to free list, implicitly collecting garbage
        // Requires an off_white tail!
        if (gc.segments[i].off_white != NULL)
            assert(gc.segments[i].off_white_tail != NULL);
        if (gc.segments[i].off_white_tail != NULL)
        {
            assert(gc.segments[i].off_white != NULL);
            necro_cons_slab(gc.segments[i].off_white_tail, gc.segments[i].free);
            gc.segments[i].free = gc.segments[i].off_white;
            gc.segments[i].free->prev = NULL;
        }

        //------------------------------
        // Switch black with off white
        gc.segments[i].off_white      = gc.segments[i].black;
        gc.segments[i].off_white_tail = gc.segments[i].black_tail;

        necro_ensure_off_white_not_null(gc.segments + i);

        //------------------------------
        // Clear out black
        gc.segments[i].black      = NULL;
        gc.segments[i].black_tail = NULL;
        // if (white_count > 0 || black_count > 0)
        //     printf("segment: %d, allocated: %d, free: %d\n", i, necro_count_slabs(gc.segments[i].off_white), necro_count_slabs(gc.segments[i].free));
    }
// #if _WIN32
//     QueryPerformanceCounter(&gc_end_time);
//     gc_time = ((gc_end_time.QuadPart - gc_begin_time.QuadPart) * 1000.0) / ticks_per_sec.QuadPart;
//     // gc_time = (gc_end_time.QuadPart - gc_begin_time.QuadPart) / (ticks_per_sec.QuadPart / 1000.f);
//     printf("gc, latency: %f ms\n", gc_time);
// #endif
}

extern DLLEXPORT int64_t* _necro_alloc(uint32_t slots_used, uint8_t segment)
{
    // NOTE: Assumes that off_white and off_white_tail are not NULL!!!!
    NecroGCSegment* gcs = gc.segments + segment;
    if (gcs->free == NULL)
        necro_gc_alloc_page(gcs, gc.white_color);
    NecroSlab* slab      = gcs->free;
    gcs->free            = slab->next;
    slab->prev           = NULL;
    slab->tag            = 0;
    slab->slots_used     = slots_used;
    slab->segment        = segment;
    slab->color          = gc.white_color;
    slab->next           = gcs->off_white;
    gcs->off_white->prev = slab;
    gcs->off_white       = slab;
    // zero out memory...better to do it here!?
    int64_t** data = (int64_t**)(&slab->slots_used);
    for (uint32_t i = 0; i < slots_used; ++i)
    {
        data[i] = NULL;
    }
    return (int64_t*) data;
}

extern DLLEXPORT void _necro_print(int64_t value)
{
    printf("necro: %lld                      \r", value);
    // printf("necro: %lld                      \n", value);
}

extern DLLEXPORT void _necro_sleep(uint32_t milliseconds)
{
    // printf("sleep: %d\n", milliseconds);
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000); // 100 Milliseconds
#endif
}

extern DLLEXPORT void _necro_error_exit(uint32_t error_code)
{
    switch (error_code)
    {
    case 1:
        fprintf(stderr, "****Error: Non-exhaustive patterns in case statement!\n");
    case 2:
        fprintf(stderr, "****Error: Malformed closure application!\n");
        break;
    default:
        fprintf(stderr, "****Error: Unrecognized error (%d) during runtime!\n", error_code);
        break;
    }
    exit(error_code);
}

size_t runtime_tick = 0;
size_t necro_get_runtime_tick()
{
    return runtime_tick;
}

extern DLLEXPORT void _necro_init_runtime()
{
    runtime_tick = 0;
    // necro_init_mouse();
}

extern DLLEXPORT void _necro_update_runtime()
{
    runtime_tick++;
    // Update tick!
    // necro_poll_mouse();
}

///////////////////////////////////////////////////////
// Concurrent Copying GC
///////////////////////////////////////////////////////
#define NECRO_SPACE_SIZE 1048576 // 2 ^ 20, i.e. 1 mb

typedef struct NecroSpace
{
    union
    {
        struct
        {
            struct NecroSpace* next_space;
            char*              alloc_ptr;
            char*              end_ptr;
            size_t             data_start;
        };
        char data[NECRO_SPACE_SIZE];
    };
} NecroSpace;

typedef struct NecroBlock
{
    struct NecroBlock* forwarding_pointer;
    uint16_t           color;
    uint16_t           something_else;
    uint32_t           tag; // Used by necrolang
    // The rest of the data...
} NecroBlock;

typedef enum
{
    NECRO_MUTATE_VALUE,
    NECRO_MUTATE_REF,
} NECRO_MUTATION_TYPE;

typedef struct
{
    char*               value;
    // Scan fn_pointer!?
    NECRO_MUTATION_TYPE type;
} NecroMutationEntry;

typedef struct
{
    NecroMutationEntry* entries;
    size_t              capacity;
    size_t              count;
} NecroMutationLog;

typedef struct
{
    NecroMutationLog mutation_log;
    NecroSpace*      from_head;
    NecroSpace*      from_curr;

    NecroSpace*      to_head;
    NecroSpace*      to_curr;

    size_t           to_ptr;
    NecroGCRoot*     roots;
    uint32_t         root_count;
    uint32_t         counter;
} NecroCopyGC;

NecroCopyGC copy_gc;

// void _necro_copy_scan(Necro)
char* _necro_copy_alloc(uint32_t size)
{
    // char* data =
        // if (copy_gc)
    return NULL;
}

// NecroGC gc;
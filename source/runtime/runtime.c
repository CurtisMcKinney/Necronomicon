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
#include "machine/machine_copy.h"

#ifdef _WIN32
#define UNICODE 1
#define _UNICODE 1
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define NECRO_GC_SLAB_TO_PAGES 1024
#define NECRO_GC_NUM_SEGMENTS 10

// Set to 1 if you want debug trace messages from GC
#define NECRO_DEBUG_GC 0

#if NECRO_DEBUG_GC
#define NECRO_TRACE_GC(...) printf(__VA_ARGS__)
#else
#define NECRO_TRACE_GC(...)
#endif

///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
void necro_report_gc_statistics();

///////////////////////////////////////////////////////
// Runtime functions
///////////////////////////////////////////////////////
extern DLLEXPORT void _necro_print(int value)
{
#if NECRO_DEBUG_GC
    // necro_report_gc_statistics();
    printf("necro: %d                      \n", value);
#else
    printf("necro: %d                      \r", value);
#endif
}

extern DLLEXPORT void _necro_debug_print(int value)
{
    printf("debug: %d\n", value);
}

extern DLLEXPORT unsigned int _necro_print_int(int value, unsigned int world)
{
    printf("necro: %d\n", value);
    return world;
}

extern DLLEXPORT void _necro_sleep(uint32_t milliseconds)
{
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
        break;
    case 2:
        fprintf(stderr, "****Error: Malformed closure application!\n");
        break;
    default:
        fprintf(stderr, "****Error: Unrecognized error (%d) during runtime!\n", error_code);
        break;
    }
    necro_exit(error_code);
}

size_t runtime_tick = 0;
size_t necro_get_runtime_tick()
{
    return runtime_tick;
}

extern DLLEXPORT void _necro_init_runtime()
{
    runtime_tick = 0;
    necro_init_mouse();
}

extern DLLEXPORT void _necro_update_runtime()
{
    runtime_tick++;
}

///////////////////////////////////////////////////////
// Concurrent Copying GC
///////////////////////////////////////////////////////
#define NECRO_SPACE_SIZE                33554432 // 2 ^ 25, i.e. 32 mb
#define NECRO_INITIAL_MUTATION_LOG_SIZE 1024
#define NECRO_INITIAL_COPY_BUFFER_SIZE  1024

typedef struct NecroSpace
{
    union
    {
        struct
        {
            struct NecroSpace* next_space;
            char               data_start; // start of data
        };
        char data[NECRO_SPACE_SIZE];
    };
} NecroSpace;

typedef struct NecroValue
{
    void* _members_start;
} NecroValue;

typedef struct NecroBlock
{
    struct NecroValue* forwarding_pointer;
} NecroBlock;

typedef struct
{
    NecroValue* prev_block;
    NecroValue* new_block;
    size_t      data_id;
} NecroMutationEntry;

typedef struct
{
    NecroMutationEntry* entries;
    size_t              capacity;
    size_t              count;
} NecroMutationLog;

typedef struct
{
    size_t       data_id;
    NecroValue** root;
} NecropCopyGCRoot;

static NecroValue const_sentinel;

typedef struct
{
    NecroMutationLog   mutation_log;

    NecroSpace*        from_head;
    NecroSpace*        from_curr;
    char*              from_alloc_ptr;
    char*              from_end_ptr;
    NecroValue*        initial_forward_ptr;

    NecroSpace*        to_head;
    NecroSpace*        to_curr;
    char*              to_alloc_ptr;
    char*              to_end_ptr;
    // char*              to_scan_ptr;

    NecroSpace*        const_head;
    NecroSpace*        const_curr;
    char*              const_alloc_ptr;
    char*              const_end_ptr;

    NecropCopyGCRoot*  roots;
    uint32_t           root_count;
    uint32_t           counter;

    struct NecroTimer* timer;
} NecroCopyGC;

NecroCopyGC copy_gc;

static inline NecroSpace* necro_alloc_space()
{
    NecroSpace* space = emalloc(sizeof(NecroSpace));
    memset(space->data, 0, NECRO_SPACE_SIZE);
    space->next_space = NULL;
    return space;
}

void necro_destroy_space(NecroSpace* space)
{
    if (space == NULL)
        return;
    necro_destroy_space(space->next_space);
    free(space);
}

NecroCopyGC necro_create_copy_gc()
{
    NecroCopyGC necro_copy_gc;
    necro_copy_gc.roots          = NULL;
    necro_copy_gc.root_count     = 0;
    necro_copy_gc.counter        = 0;
    necro_copy_gc.timer          = necro_timer_create();

    necro_copy_gc.initial_forward_ptr = &const_sentinel;

    // From
    necro_copy_gc.from_head      = necro_alloc_space();
    necro_copy_gc.from_curr      = necro_copy_gc.from_head;
    necro_copy_gc.from_alloc_ptr = &necro_copy_gc.from_curr->data_start;
    necro_copy_gc.from_end_ptr   = necro_copy_gc.from_curr->data + NECRO_SPACE_SIZE;

    // To
    necro_copy_gc.to_head      = necro_alloc_space();
    necro_copy_gc.to_curr      = necro_copy_gc.to_head;
    necro_copy_gc.to_alloc_ptr = &necro_copy_gc.to_curr->data_start;
    necro_copy_gc.to_end_ptr   = necro_copy_gc.to_curr->data + NECRO_SPACE_SIZE;
    // necro_copy_gc.to_scan_ptr  = copy_gc.to_alloc_ptr;

    // Constant
    necro_copy_gc.const_head      = necro_alloc_space();
    necro_copy_gc.const_curr      = necro_copy_gc.const_head;
    necro_copy_gc.const_alloc_ptr = &necro_copy_gc.const_curr->data_start;
    necro_copy_gc.const_end_ptr   = necro_copy_gc.const_curr->data + NECRO_SPACE_SIZE;

    return necro_copy_gc;
}

void necro_destroy_copy_gc(NecroCopyGC* necro_copy_gc)
{
    assert(necro_copy_gc != NULL);
    necro_timer_destroy(necro_copy_gc->timer);
    necro_destroy_space(necro_copy_gc->from_head);
    necro_destroy_space(necro_copy_gc->to_head);
    necro_destroy_space(necro_copy_gc->const_head);
    if (necro_copy_gc->roots != NULL)
    {
        free(necro_copy_gc->roots);
        necro_copy_gc->roots = NULL;
    }
    necro_copy_gc->root_count = 0;
}

typedef struct
{
    size_t       data_id;
    NecroValue*  from_value;
    NecroValue** to_value;
} NecroCopyJob;

typedef struct
{
    NecroCopyJob* data;
    size_t        capacity;
    size_t        count;
} NecroCopyBuffer;

//--------------------
// Globals
//--------------------
NecroCopyBuffer        copy_buffer;
NecroConstructorInfo*  data_map;   // data_id + tag = ConstructorInfo
size_t*                member_map; // member_offset + i = member data_id

NecroCopyBuffer necro_create_copy_buffer()
{
    return (NecroCopyBuffer)
    {
        .data     = emalloc(NECRO_INITIAL_COPY_BUFFER_SIZE * sizeof(NecroCopyJob)),
        .capacity = NECRO_INITIAL_COPY_BUFFER_SIZE,
        .count    = 0,
    };
}

void necro_destroy_copy_buffer(NecroCopyBuffer* necro_copy_buffer)
{
     assert(necro_copy_buffer != NULL);
     if (necro_copy_buffer->data == NULL)
         return;
     free(necro_copy_buffer->data);
     necro_copy_buffer->data     = NULL;
     necro_copy_buffer->capacity = 0;
     necro_copy_buffer->count    = 0;
}

static inline void necro_create_new_copy_job(size_t data_id, NecroValue* from_value, NecroValue** to_value)
{
    if (copy_buffer.count + 1 >= copy_buffer.capacity)
    {
        copy_buffer.capacity *= 2;
        copy_buffer.data      = realloc(copy_buffer.data, copy_buffer.capacity * sizeof(NecroCopyBuffer));
    }
    copy_buffer.data[copy_buffer.count] = (NecroCopyJob) { .data_id = data_id, .from_value = from_value, .to_value = to_value };
    copy_buffer.count++;
}

// static inline NecroValue* necro_block_to_value(NecroBlock* block)
// {
//     return (NecroValue*)(block + 1);
// }

// static inline NecroBlock* necro_value_to_block(NecroValue* value)
// {
//     return ((NecroBlock*)value) - 1;
// }

#define necro_block_to_value(BLOCK) ((NecroValue*)(BLOCK + 1))
#define necro_value_to_block(VALUE) (((NecroBlock*)VALUE) - 1)

// TODO: GC for Arrays / Ptrs!!!!!!
void _necro_copy(size_t root_data_id, NecroValue* root)
{
    NecroCopyJob         job;
    NecroValue*          new_value;
    NecroConstructorInfo info;
    NecroBlock*          from_block;
    NecroValue**         from_members;
    NecroValue**         to_members;
    NecroValue*          _dummy;
    necro_create_new_copy_job(root_data_id, root, &_dummy);
    while (copy_buffer.count > 0)
    {
        copy_buffer.count--;
        job = copy_buffer.data[copy_buffer.count];
#if NECRO_DEBUG_GC
        assert(job.data_id != NECRO_UNBOXED_DATA_ID);
        assert(job.data_id != NECRO_SELF_DATA_ID);
        assert(job.from_value != NULL);
        // printf("copy job: data_id: %d, from_value: %p, to_value: %p\n", job.data_id, job.from_value, job.to_value);
#endif
        from_block = necro_value_to_block(job.from_value);
        // We've already copied this at some pointer, simply copy the forwarding pointer over
        if (from_block->forwarding_pointer != NULL)
        {
            if (from_block->forwarding_pointer == &const_sentinel)
                *job.to_value = necro_block_to_value(from_block); // TODO: Test this. But to do so, we need initializers functioning correctly (which requires Core support...)
            else
                *job.to_value = from_block->forwarding_pointer;
        }
        // Otherwise deep copy
        else
        {
            info = data_map[job.data_id];
            if (info.is_tagged_union)
            {
                info = data_map[job.data_id + *((size_t*)(job.from_value))];
            }
            new_value                      = (NecroValue*)_necro_to_alloc(info.size_in_bytes);
            from_block->forwarding_pointer = new_value;
            *job.to_value                  = new_value;
            from_members                   = (NecroValue**) job.from_value;
            to_members                     = (NecroValue**) new_value;
            size_t member_id               = 0;
            for (size_t i = 0; i < info.num_members; ++i)
            {
                member_id = member_map[info.members_offset + i];
                // NULL id
                if (member_id == NECRO_NULL_DATA_ID)
                {
                    to_members[i] = NULL;
                }
                // Unboxed type, simply copy over
                else if (member_id == NECRO_UNBOXED_DATA_ID)
                {
                    to_members[i] = from_members[i];
                }
                // NULL member
                else if (from_members[i] == NULL)
                {
                    to_members[i] = NULL;
                }
                // Pointer to some more data
                else
                {
                    if (member_id == NECRO_DYNAMIC_DATA_ID) // Dynamic type, retrieve data id int at previous index and go deeper
                        member_id = (size_t)from_members[i - 1];

                    if (copy_buffer.count + 1 >= copy_buffer.capacity)
                    {
                        copy_buffer.capacity *= 2;
                        copy_buffer.data      = realloc(copy_buffer.data, copy_buffer.capacity * sizeof(NecroCopyBuffer));
                    }
                    copy_buffer.data[copy_buffer.count].data_id    = member_id;
                    copy_buffer.data[copy_buffer.count].from_value = from_members[i];
                    copy_buffer.data[copy_buffer.count].to_value   = to_members + i;
                    copy_buffer.count++;
                }
            }
        }
    }
}

// #define NECRO_COPY_MAX_REC 64
// NecroValue* _necro_copy_rec(size_t data_id, NecroValue* from_value, size_t depth)
// {
//     if (depth >= NECRO_COPY_MAX_REC)
//         _necro_copy(data_id, from_value);
// #if NECRO_DEBUG_GC
//     assert(data_id != NECRO_UNBOXED_DATA_ID);
//     assert(from_value != NULL);
//     // printf("copy job: data_id: %d, from_value: %p, to_value: %p\n", job.data_id, job.from_value, job.to_value);
// #endif
//     NecroBlock*          from_block = necro_value_to_block(from_value);
//     NecroConstructorInfo info       = data_map[data_id];
//     if (info.is_tagged_union)
//         info = data_map[data_id + *((size_t*)(from_value))];
//     NecroValue*  new_value         = (NecroValue*)_necro_to_alloc(info.size_in_bytes);
//     from_block->forwarding_pointer = new_value;
//     NecroValue** from_members      = (NecroValue**) from_value;
//     NecroValue** to_members        = (NecroValue**) new_value;
//     size_t       member_id         = 0;
//     for (size_t i = 0; i < info.num_members; ++i)
//     {
//         member_id = member_map[info.members_offset + i];
//         // NULL id
//         if (member_id == NECRO_NULL_DATA_ID)
//         {
//             // Dynamic, perhaps separate type?
//             to_members[i] = NULL;
//         }
//         // Unboxed type, simply copy over
//         else if (member_id == NECRO_UNBOXED_DATA_ID)
//         {
//             // printf("    value member: from_value: %d\n", (int)from_members[i]);
//             to_members[i] = from_members[i];
//         }
//         // NULL member
//         else if (from_members[i] == NULL)
//         {
//             // printf("    NULL member\n");
//             to_members[i] = NULL;
//         }
//         // Forwarding pointer
//         else if (necro_value_to_block(from_members[i])->forwarding_pointer != NULL)
//         {
//             to_members[i] = necro_value_to_block(from_block)->forwarding_pointer;
//         }
//         // Deep copy
//         else
//         {
//             to_members[i] = _necro_copy_rec(member_id, from_members[i], depth + 1);
//         }
//     }
//     return new_value;
// }

// // Breadthfirst version
// void _necro_scan()
// {
//     // TODO: How do we skip to next space?
//     NecroConstructorInfo info;
//     size_t               member_id;
//     NecroBlock*          block;
//     NecroValue*          from_value;
//     NecroValue**         from_members;
//     NecroValue**         to_members;
//     size_t               i = 0;
//     while (copy_gc.to_scan_ptr < copy_gc.to_alloc_ptr)
//     {
//         // printf("scan, id: %d, addr %p\n", *((size_t*)copy_gc.to_scan_ptr), copy_gc.to_scan_ptr);
//         block                                                = (NecroBlock*) copy_gc.to_scan_ptr;
//         info                                                 = **((NecroConstructorInfo**)block); // info ptr (already offset by constructor) is stored at scan site, to be overwritten
//         from_value                                           = *(((NecroValue**)block) + 1);    // from value address stored after data_id at scan stire, to be overwritten
//         necro_value_to_block(from_value)->forwarding_pointer = necro_block_to_value(block);
//         from_members                                         = (NecroValue**) from_value;
//         to_members                                           = (NecroValue**) necro_block_to_value(block);
//         block->forwarding_pointer                            = NULL;
//         for (i = 0; i < info.num_members; ++i)
//         {
//             member_id = member_map[info.members_offset + i];
//             // NULL id
//             if (member_id == NECRO_NULL_DATA_ID)
//             {
//                 // Dynamic, perhaps separate type?
//                 // printf("    Dyn member\n");
//                 to_members[i] = NULL;
//             }
//             // Unboxed type, simply copy over
//             else if (member_id == NECRO_UNBOXED_DATA_ID)
//             {
//                 // printf("    value member: from_value: %d\n", (int)from_members[i]);
//                 to_members[i] = from_members[i];
//             }
//             // NULL member
//             else if (from_members[i] == NULL)
//             {
//                 // printf("    NULL member\n");
//                 to_members[i] = NULL;
//             }
//             // Already copied
//             else if (necro_value_to_block(from_members[i])->forwarding_pointer != NULL)
//             {
//                 // printf("    already copied\n");
//                 to_members[i] = necro_value_to_block(from_members[i])->forwarding_pointer;
//             }
//             // Deep copy
//             else
//             {
//                 if (data_map[member_id].is_tagged_union)
//                     member_id = member_id + *((size_t*)(from_value)); // Tag is stored as first member in a tagged union
//                 // printf("    deep copy, id: %d\n", member_id);
//                 to_members[i]                   = (NecroValue*)_necro_to_alloc(data_map[member_id].size_in_bytes);
//                 *(((NecroConstructorInfo**)to_members[i]) - 1) = data_map + member_id;
//                 *((NecroValue**)to_members[i])  = from_members[i];
//                 // printf("    deep copy, id: %d, from_value: %p\n", *(((size_t*)to_members[i]) - 1), *((NecroValue**)to_members[i]));
//             }
//         }
//         // printf(" end scan, num_members: %d, inc: %d\n", info.num_members, ((info.num_members + 1) * sizeof(NecroBlock*)) / 1644448);
//         copy_gc.to_scan_ptr += info.size_in_bytes;
//     }
// }
//
// void _necro_copy_root(size_t root_data_id, NecroValue* root)
// {
//     if (data_map[root_data_id].is_tagged_union)
//         root_data_id = root_data_id + *((size_t*)(root)); // Tag is stored as first member in a tagged union
//     NecroValue* to_value       = (NecroValue*)_necro_to_alloc(data_map[root_data_id].size_in_bytes);
//     // printf("    deep copy, id: %d, addr: %p\n", root_data_id, necro_value_to_block(to_value));
//     *(((NecroConstructorInfo**)to_value) - 1) = data_map + root_data_id; // Store data_id
//     *((NecroValue**)to_value)  = root;         // Store from_value
// }

#define NECRO_ONE_MEGABYTE_F ((double)1048576.0)

void necro_report_gc_statistics()
{
#if NECRO_DEBUG_GC
    // From
    NecroSpace* from_space     = copy_gc.from_head;
    size_t      from_allocated = 0;
    size_t      from_blocks    = 1;
    while (from_space != NULL && from_space->next_space != NULL)
    {
        from_allocated += sizeof(NecroSpace);
        from_blocks++;
        from_space = from_space->next_space;
    }
    from_allocated += (size_t)(copy_gc.from_alloc_ptr - &copy_gc.from_curr->data_start);
    // To
    NecroSpace* to_space     = copy_gc.to_head;
    size_t      to_allocated = 0;
    size_t      to_blocks    = 1;
    while (to_space != NULL && to_space->next_space != NULL)
    {
        to_allocated += sizeof(NecroSpace);
        to_blocks++;
        to_space = to_space->next_space;
    }
    to_allocated += (size_t)(copy_gc.to_alloc_ptr - &copy_gc.to_curr->data_start);
    // Const
    NecroSpace* const_space     = copy_gc.from_head;
    size_t      const_allocated = 0;
    size_t      const_blocks    = 1;
    while (from_space != NULL && from_space->next_space != NULL)
    {
        const_allocated += sizeof(NecroSpace);
        from_blocks++;
        from_space = from_space->next_space;
    }
    const_allocated += (size_t)(copy_gc.const_alloc_ptr - &copy_gc.const_curr->data_start);
    NECRO_TRACE_GC("--------------------------------\n");
    NECRO_TRACE_GC("-- GC statistics \n");
    NECRO_TRACE_GC("--------------------------------\n");
    NECRO_TRACE_GC(" from  alloc:  %fmb\n", ((double)from_allocated) / NECRO_ONE_MEGABYTE_F);
    NECRO_TRACE_GC(" to    alloc:  %fmb\n", ((double)to_allocated) / NECRO_ONE_MEGABYTE_F);
    NECRO_TRACE_GC(" const alloc:  %fmb\n", ((double)const_allocated) / NECRO_ONE_MEGABYTE_F);
    NECRO_TRACE_GC(" from  blocks: %d\n", from_blocks);
    NECRO_TRACE_GC(" to    blocks: %d\n", to_blocks);
    NECRO_TRACE_GC(" const blocks: %d\n", const_blocks);
    NECRO_TRACE_GC(" root  count:  %d\n", copy_gc.root_count);
#endif
}

//--------------------
// NecroCopyGC API
//--------------------
void necro_copy_gc_init()
{
    copy_buffer = necro_create_copy_buffer();
    copy_gc     = necro_create_copy_gc();
}

void necro_copy_gc_cleanup()
{
    necro_destroy_copy_gc(&copy_gc);
}

// Don't even enter unboxed roots into the gc at all!
extern DLLEXPORT void _necro_copy_gc_initialize_root_set(size_t root_count)
{
    // NECRO_TRACE_GC("init root_set, root_count: %d\n", root_count);
    assert(root_count < UINT32_MAX);
    copy_gc.root_count = (uint32_t) root_count;
    if (root_count == 0)
    {
        copy_gc.roots = NULL;
        return;
    }
    copy_gc.roots = emalloc(root_count * sizeof(NecropCopyGCRoot));
    for (size_t i = 0; i < root_count; ++i)
    {
        copy_gc.roots[i].root    = NULL;
        copy_gc.roots[i].data_id = (size_t) -1;
    }
}

extern DLLEXPORT void _necro_copy_gc_set_root(int** root, size_t root_index, size_t data_id)
{
    // NECRO_TRACE_GC("set root, index: %d, root: %p\n", root_index, root);
    assert(data_id != NECRO_UNBOXED_DATA_ID);
    copy_gc.roots[root_index].root    = (NecroValue**) root;
    copy_gc.roots[root_index].data_id = data_id;
}

// TODO: Serial version (parallelize later)
extern DLLEXPORT void _necro_copy_gc_collect()
{
#if NECRO_DEBUG_GC
    necro_start_timer(copy_gc.timer);
#endif

    //-----------
    // Copy Roots
    for (size_t i = 0; i < copy_gc.root_count; ++i)
    {
#if NECRO_DEBUG_GC
        assert(copy_gc.roots[i].data_id != NECRO_UNBOXED_DATA_ID);
#endif
        _necro_copy(copy_gc.roots[i].data_id, *copy_gc.roots[i].root);
        // _necro_copy_root(copy_gc.roots[i].data_id, *copy_gc.roots[i].root);
        // _necro_copy_rec(copy_gc.roots[i].data_id, *copy_gc.roots[i].root, 0);
    }

    //-----------
    // Scan
    // _necro_scan();

    //-----------
    // Mutation log

    //-----------
    // Replace roots
    for (size_t i = 0; i < copy_gc.root_count; ++i)
    {
#if NECRO_DEBUG_GC
        assert(copy_gc.roots[i].data_id != NECRO_UNBOXED_DATA_ID);
        // printf("copying root, original: %p, forward pointer: %p\n", *copy_gc.roots[i].root, necro_value_to_block(*copy_gc.roots[i].root)->forwarding_pointer);
#endif
        *copy_gc.roots[i].root = necro_value_to_block(*copy_gc.roots[i].root)->forwarding_pointer;
    }

    //-----------
    // Flip
    NecroSpace* from_head      = copy_gc.from_head;
    NecroSpace* to_head        = copy_gc.to_head;
    NecroSpace* to_curr        = copy_gc.to_curr;
    char*       from_alloc_ptr = copy_gc.from_alloc_ptr;
    UNUSED(from_alloc_ptr);
    char*       to_alloc_ptr   = copy_gc.to_alloc_ptr;
    char*       to_end_ptr     = copy_gc.to_end_ptr;
    copy_gc.from_head          = to_head;
    copy_gc.from_curr          = to_curr;
    copy_gc.from_alloc_ptr     = to_alloc_ptr;
    copy_gc.from_end_ptr       = to_end_ptr;
    copy_gc.to_head            = from_head;
    copy_gc.to_curr            = from_head;
    copy_gc.to_alloc_ptr       = &from_head->data_start;
    copy_gc.to_end_ptr         = from_head->data + NECRO_SPACE_SIZE;
    // copy_gc.to_scan_ptr        = copy_gc.to_alloc_ptr;
    // memset(from_head->data, 0, NECRO_SPACE_SIZE); // Only requried for testing while initializer system isn't functional

    //-----------
    // End
#if NECRO_DEBUG_GC
    necro_stop_and_report_timer(copy_gc.timer, "collect, time: ");
#endif
}

extern DLLEXPORT int* _necro_from_alloc(size_t size)
{
    NecroBlock* data    = (NecroBlock*)copy_gc.from_alloc_ptr;
    char*       new_end = copy_gc.from_alloc_ptr + size;
    if (new_end < copy_gc.from_end_ptr)
    {
        copy_gc.from_alloc_ptr   = new_end;
        data->forwarding_pointer = copy_gc.initial_forward_ptr;
        return (int*)(data + 1); // return address after header
    }
    else
    {
        assert(size < NECRO_SPACE_SIZE);
        if (copy_gc.from_curr->next_space == NULL)
            copy_gc.from_curr->next_space = necro_alloc_space();
        copy_gc.from_curr      = copy_gc.from_curr->next_space;
        copy_gc.from_alloc_ptr = &copy_gc.from_curr->data_start;
        copy_gc.from_end_ptr   = copy_gc.from_curr->data + NECRO_SPACE_SIZE;
        return _necro_from_alloc(size);
    }
}

inline extern DLLEXPORT int* _necro_to_alloc(size_t size)
{
    NecroBlock* data    = (NecroBlock*)copy_gc.to_alloc_ptr;
    char*       new_end = copy_gc.to_alloc_ptr + size;
    if (new_end < copy_gc.to_end_ptr)
    {
        copy_gc.to_alloc_ptr     = new_end;
        data->forwarding_pointer = NULL;
        return (int*)(data + 1); // return address after header
    }
    else
    {
        assert(size < NECRO_SPACE_SIZE);
        if (copy_gc.to_curr->next_space == NULL)
            copy_gc.to_curr->next_space = necro_alloc_space();
        copy_gc.to_curr      = copy_gc.to_curr->next_space;
        copy_gc.to_alloc_ptr = &copy_gc.to_curr->data_start;
        copy_gc.to_end_ptr   = copy_gc.to_curr->data + NECRO_SPACE_SIZE;
        return _necro_to_alloc(size);
    }
}

// extern DLLEXPORT int* _necro_const_alloc(size_t size)
// {
//     NecroBlock* data    = (NecroBlock*)copy_gc.const_alloc_ptr;
//     char*       new_end = copy_gc.const_alloc_ptr + size;
//     if (new_end < copy_gc.const_end_ptr)
//     {
//         copy_gc.const_alloc_ptr  = new_end;
//         data->forwarding_pointer = (NecroValue*)(data + 1); // Constant blocks forward to themselves
//         return (int*)(data + 1); // return address after header
//     }
//     else
//     {
//         assert(size < NECRO_SPACE_SIZE);
//         if (copy_gc.const_curr->next_space == NULL)
//             copy_gc.const_curr->next_space = necro_alloc_space();
//         copy_gc.const_curr      = copy_gc.const_curr->next_space;
//         copy_gc.const_alloc_ptr = &copy_gc.const_curr->data_start;
//         copy_gc.const_end_ptr   = copy_gc.const_curr->data + NECRO_SPACE_SIZE;
//         return _necro_const_alloc(size);
//     }
// }

extern DLLEXPORT void _necro_set_data_map(struct NecroConstructorInfo* a_data_map)
{
    data_map = a_data_map;
}

extern DLLEXPORT void _necro_set_member_map(size_t* a_member_map)
{
    member_map = a_member_map;
}

extern DLLEXPORT void _necro_flip_const()
{
    // From
    NecroSpace* from_head       = copy_gc.from_head;
    NecroSpace* from_curr       = copy_gc.from_curr;
    char*       from_alloc_ptr  = copy_gc.from_alloc_ptr;
    char*       from_end_ptr    = copy_gc.from_end_ptr;

    // Const
    NecroSpace* const_head      = copy_gc.const_head;
    NecroSpace* const_curr      = copy_gc.const_curr;
    char*       const_alloc_ptr = copy_gc.const_alloc_ptr;
    char*       const_end_ptr   = copy_gc.const_end_ptr;

    // Flip From
    copy_gc.from_head           = const_head;
    copy_gc.from_curr           = const_curr;
    copy_gc.from_alloc_ptr      = const_alloc_ptr;
    copy_gc.from_end_ptr        = const_end_ptr;

    // Flip Const
    copy_gc.const_head          = from_head;
    copy_gc.const_curr          = from_curr;
    copy_gc.const_alloc_ptr     = from_alloc_ptr;
    copy_gc.const_end_ptr       = from_end_ptr;

    copy_gc.initial_forward_ptr = NULL;
}


// TODO: Pre-emptively allocate NecroSpace memory if we're getting close to the end of the line

// //=====================================================
// // GC
// //=====================================================
// typedef struct NecroGCPage
// {
//     struct NecroGCPage* next;
// } NecroGCPage;

// typedef struct NecroSlab
// {
//     struct NecroSlab* prev;
//     struct NecroSlab* next;
//     uint16_t          slots_used;
//     uint8_t           segment;
//     int8_t            color;
//     uint32_t          tag; // Used by necro, not gc
// } NecroSlab;

// typedef struct
// {
//     NecroGCPage* pages;
//     NecroSlab*   free;
//     NecroSlab*   black;
//     NecroSlab*   black_tail;
//     NecroSlab*   off_white;
//     NecroSlab*   off_white_tail;
//     size_t       slab_size;
//     uint8_t      segment;
//     size_t       page_count;
// } NecroGCSegment;

// typedef struct NecroGCRoot
// {
//     char**   slots;
//     uint32_t num_slots;
// } NecroGCRoot;

// typedef struct
// {
//     NecroGCSegment segments[NECRO_GC_NUM_SEGMENTS]; // Increases by powers of 2
//     NecroSlab*     gray;
//     NecroGCRoot*   roots;
//     uint32_t       root_count;
//     uint32_t       counter;
//     int8_t         white_color;
// } NecroGC;

// NecroGC gc;

// inline NecroSlab* necro_cons_slab(NecroSlab* slab_1, NecroSlab* slab_2)
// {
//     // assert(slab_1 != NULL);
//     slab_1->next = slab_2;
//     if (slab_2 != NULL)
//         slab_2->prev = slab_1;
//     return slab_1;
// }

// inline size_t necro_count_slabs(NecroSlab* slab)
// {
//     size_t count = 0;
//     while (slab != NULL)
//     {
//         count++;
//         slab = slab->next;
//     }
//     return count;
// }

// inline void necro_unlink_slab(NecroSlab* slab_1, NecroGCSegment* segment)
// {
//     NecroSlab* prev = slab_1->prev;
//     NecroSlab* next = slab_1->next;
//     if (slab_1 == segment->off_white_tail)
//         segment->off_white_tail = prev;
//     if (slab_1 == segment->off_white)
//         segment->off_white = next;
//     if (prev != NULL)
//         prev->next = next;
//     if (next != NULL)
//         next->prev = prev;
//     slab_1->prev = NULL;
//     slab_1->next = NULL;
// }

// void necro_gc_alloc_page(NecroGCSegment* segment, int8_t white_color)
// {
//     segment->page_count++;
//     // printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
//     // printf("!! alloc_page: segment: %d, pages: %d, slab_size: %d\n", segment->segment, segment->page_count, segment->slab_size);
//     NecroGCPage* page = emalloc(sizeof(NecroGCPage) + segment->slab_size * NECRO_GC_SLAB_TO_PAGES);
//     memset(page, 0, sizeof(NecroGCPage) + segment->slab_size * NECRO_GC_SLAB_TO_PAGES);
//     if (page == NULL)
//     {
//         assert(false && "Could not allocate memory for necro_gc_alloc_page");
//     }
//     page->next        = segment->pages;
//     segment->pages    = page;
//     char*        data = ((char*)page) + sizeof(NecroGCPage);
//     for (size_t i = 0; i < NECRO_GC_SLAB_TO_PAGES; ++i)
//     {
//         NecroSlab* slab  = (NecroSlab*)data;
//         slab->color      = white_color;
//         slab->segment    = segment->segment;
//         slab->slots_used = 0;
//         slab->tag        = 0;
//         slab->prev       = NULL;
//         segment->free    = necro_cons_slab(slab, segment->free);
//         data           += segment->slab_size;
//     }
//     assert(segment->free != NULL);
// }

// NecroGCSegment necro_create_gc_segment(size_t slab_size, uint8_t segment, int8_t white_color)
// {
//     NecroGCSegment gcs;
//     gcs.slab_size      = slab_size;
//     gcs.segment        = segment;
//     gcs.off_white      = NULL;
//     gcs.off_white_tail = NULL;
//     gcs.black          = NULL;
//     gcs.black_tail     = NULL;
//     gcs.free           = NULL;
//     gcs.pages          = NULL;
//     gcs.page_count     = 0;
//     necro_gc_alloc_page(&gcs, white_color);
//     return gcs;
// }

// void necro_destroy_gc_segment(NecroGCSegment* gcs)
// {
//     assert(gcs != NULL);
//     if (gcs->pages == NULL)
//         return;
//     NecroGCPage* page = gcs->pages;
//     while (page != NULL)
//     {
//         NecroGCPage* next = page->next;
//         free(page);
//         page = next;
//     }
//     gcs->pages     = NULL;
//     gcs->slab_size = 0;
// }

// void necro_ensure_off_white_not_null(NecroGCSegment* gcs)
// {
//     if (gcs->off_white != NULL)
//     {
//         assert(gcs->off_white_tail != NULL);
//         return;
//     }
//     assert(gcs->off_white_tail == NULL);
//     if (gcs->free == NULL)
//         necro_gc_alloc_page(gcs, gc.white_color);
//     NecroSlab* slab     = gcs->free;
//     gcs->free           = slab->next;
//     slab->slots_used    = 0;
//     slab->segment       = gcs->segment;
//     slab->prev          = NULL;
//     slab->next          = NULL;
//     slab->color         = gc.white_color;
//     gcs->off_white      = slab;
//     gcs->off_white_tail = slab;
// }

// NecroGC necro_create_gc()
// {
//     NecroGC gc;
//     gc.gray        = NULL;
//     gc.white_color = 1;
//     gc.roots       = NULL;
//     gc.root_count  = 0;
//     gc.counter     = 0;
//     size_t  slots = 1;
//     for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
//     {
//         gc.segments[i] = necro_create_gc_segment((slots * sizeof(char*)) + sizeof(NecroSlab), (uint8_t)i, gc.white_color);
//         necro_ensure_off_white_not_null(gc.segments + i);
//         slots *= 2;
//     }
//     return gc;
// }

// void necro_destroy_gc(NecroGC* gc)
// {
//     assert(gc != NULL);
//     for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
//     {
//         necro_destroy_gc_segment(gc->segments + i);
//     }
//     free(gc->roots);
//     gc->root_count = 0;
// }

// inline NecroSlab* necro_gc_slot_to_slab(char* slot)
// {
//     assert(slot != NULL);
//     return (NecroSlab*)(slot - (sizeof(NecroSlab*) * 2));
// }

// //-----------------
// // GC API
// //-----------------
// void necro_gc_init()
// {
//     gc = necro_create_gc();
// }

// void necro_cleanup_gc()
// {
//     necro_destroy_gc(&gc);
// }

// void necro_gc_scan_root(char** slot, size_t slots_used)
// {
//     for (size_t i = 0; i < slots_used; ++i)
//     {
//         if ((*slot) == NULL)
//             goto necro_gc_scan_slab_next;
//         NecroSlab* slab = necro_gc_slot_to_slab(*slot);
//         // assert(slab != NULL);
//         if (slab->color != gc.white_color)
//             goto necro_gc_scan_slab_next;
//         slab->color = 0;
//         necro_unlink_slab(slab, gc.segments + slab->segment);
//         gc.gray = necro_cons_slab(slab, gc.gray);
//         // printf("pre-scanned: %p, gray count: %d\n", slab, necro_count_slabs(gc.gray));
//     necro_gc_scan_slab_next:
//         slot++;
//     }
// }

// void necro_gc_scan_slab(NecroSlab* a_slab)
// {
//     assert(a_slab != NULL && "NULL slab in necro_gc_scan_slab");
//     char** slot = (char**)(a_slab + 1);
//     for (size_t i = 0; i < a_slab->slots_used; ++i)
//     {
//         if ((*slot) == NULL)
//             goto necro_gc_scan_slab_next;
//         NecroSlab* slab = necro_gc_slot_to_slab(*slot);
//         // assert(slab != NULL);
//         if (slab->color != gc.white_color)
//             goto necro_gc_scan_slab_next;
//         slab->color = 0;
//         necro_unlink_slab(slab, gc.segments + slab->segment);
//         gc.gray = necro_cons_slab(slab, gc.gray);
//         // printf("pre-scanned: %p, gray count: %d\n", slab, necro_count_slabs(gc.gray));
//     necro_gc_scan_slab_next:
//         slot++;
//     }
//     a_slab->color = -gc.white_color;
//     gc.segments[a_slab->segment].black = necro_cons_slab(a_slab, gc.segments[a_slab->segment].black);
//     if (gc.segments[a_slab->segment].black_tail == NULL)
//         gc.segments[a_slab->segment].black_tail = gc.segments[a_slab->segment].black;
// }

// void necro_print_tab(size_t depth)
// {
//     for (size_t i = 0; i < depth; ++i)
//     {
//         printf("\t");
//     }
// }

// void necro_print_slab_go(NecroSlab* slab, size_t depth)
// {
//     // if (!slab || depth > 1) return;
//     if (!slab) return;
//     necro_print_tab(depth);
//     printf("--------------------------\n");
//     necro_print_tab(depth);
//     printf("slab:    %p\n", slab);
//     necro_print_tab(depth);
//     printf("prev:    %p\n", slab->prev);
//     necro_print_tab(depth);
//     printf("next:    %p\n", slab->next);
//     necro_print_tab(depth);
//     printf("slots:   %d\n", slab->slots_used);
//     necro_print_tab(depth);
//     printf("segment: %d\n", slab->segment);
//     necro_print_tab(depth);
//     printf("color:   %d\n", slab->color);
//     necro_print_tab(depth);
//     printf("tag:     %d\n", slab->tag);
//     char** slot = (char**)(slab + 1);
//     for (size_t i = 0; i < slab->slots_used; ++i)
//     {
//         // NecroSlab* m_slab = necro_gc_slot_to_slab(*slot);
//         necro_print_tab(depth);
//         printf("slot: %p, member:  %p\n", slot, *slot);
//         if ((*slot) != NULL)
//             necro_print_slab_go(necro_gc_slot_to_slab(*slot), depth + 1);
//         // slot += sizeof(char*);
//         slot++;
//         // slot += 4;
//     }
//     necro_print_tab(depth);
//     printf("--------------------------\n");
// }

// void necro_print_slab(NecroSlab* slab)
// {
//     necro_print_slab_go(slab, 0);
// }

// //=====================================================
// // Runtime functions
// //=====================================================
// extern DLLEXPORT void _necro_initialize_root_set(uint32_t root_count)
// {

//     // printf("init root_set, root_count: %d\n", root_count);
//     gc.root_count = root_count;
//     if (root_count == 0)
//     {
//         gc.roots = NULL;
//         return;
//     }
//     gc.roots = emalloc(root_count * sizeof(NecroGCRoot));
//     if (gc.roots == 0)
//     {
//         fprintf(stderr, "Couldn't allocate memory in _necro_initialize_root_set");
//     }
//     for (size_t i = 0; i < root_count; ++i)
//     {
//         gc.roots[i].slots     = NULL;
//         gc.roots[i].num_slots = 0;
//     }
// }

// // overflowing slots used!??!?!?!
// extern DLLEXPORT void _necro_set_root(uint32_t* root, uint32_t root_index, uint32_t num_slots)
// {
//     // printf("set root, index: %d, root: %p\n", root_index, root);
//     // NecroSlab* slab      = necro_gc_slot_to_slab((char*)root);
//     gc.roots[root_index].slots     = (char**)(((char*) root) + sizeof(uint64_t));
//     gc.roots[root_index].num_slots = num_slots;
//     // necro_print_slab(slab);
// }

// extern DLLEXPORT void _necro_collect()
// {
//     gc.counter++;
//     if (gc.counter < 10)
//         return;
//     // printf("collect!, white_color: %d\n", gc.white_color);
// // #if _WIN32
// //     LARGE_INTEGER ticks_per_sec;
// //     LARGE_INTEGER gc_begin_time;
// //     LARGE_INTEGER gc_end_time;
// //     double        gc_time;
// //     QueryPerformanceFrequency(&ticks_per_sec);
// //     QueryPerformanceCounter(&gc_begin_time);
// // #endif
//     gc.counter = 0;
//     for (size_t i = 0; i < gc.root_count; ++i)
//     {
//         // necro_print_slab(gc.roots[i]);
//         // assert(gc.roots[i] != NULL && "NULL root");
//         necro_gc_scan_root(gc.roots[i].slots, gc.roots[i].num_slots);
//     }
//     while (gc.gray != NULL)
//     {
//         NecroSlab* slab = gc.gray;
//         gc.gray = gc.gray->next;
//         // printf("scan: %p, gray count: %d\n", slab, necro_count_slabs(gc.gray));
//         slab->next = NULL;
//         slab->prev = NULL;
//         necro_gc_scan_slab(slab);
//     }
//     gc.white_color = -gc.white_color;
//     for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
//     {
//         // size_t white_count = necro_count_slabs(gc.segments[i].off_white);
//         // size_t black_count = necro_count_slabs(gc.segments[i].black);

//         //------------------------------
//         // Move off_white to free list, implicitly collecting garbage
//         // Requires an off_white tail!
//         if (gc.segments[i].off_white != NULL)
//             assert(gc.segments[i].off_white_tail != NULL);
//         if (gc.segments[i].off_white_tail != NULL)
//         {
//             assert(gc.segments[i].off_white != NULL);
//             necro_cons_slab(gc.segments[i].off_white_tail, gc.segments[i].free);
//             gc.segments[i].free = gc.segments[i].off_white;
//             gc.segments[i].free->prev = NULL;
//         }

//         //------------------------------
//         // Switch black with off white
//         gc.segments[i].off_white      = gc.segments[i].black;
//         gc.segments[i].off_white_tail = gc.segments[i].black_tail;

//         necro_ensure_off_white_not_null(gc.segments + i);

//         //------------------------------
//         // Clear out black
//         gc.segments[i].black      = NULL;
//         gc.segments[i].black_tail = NULL;
//         // if (white_count > 0 || black_count > 0)
//         //     printf("segment: %d, allocated: %d, free: %d\n", i, necro_count_slabs(gc.segments[i].off_white), necro_count_slabs(gc.segments[i].free));
//     }
// // #if _WIN32
// //     QueryPerformanceCounter(&gc_end_time);
// //     gc_time = ((gc_end_time.QuadPart - gc_begin_time.QuadPart) * 1000.0) / ticks_per_sec.QuadPart;
// //     // gc_time = (gc_end_time.QuadPart - gc_begin_time.QuadPart) / (ticks_per_sec.QuadPart / 1000.f);
// //     printf("gc, latency: %f ms\n", gc_time);
// // #endif
// }

// extern DLLEXPORT int64_t* _necro_alloc(uint32_t slots_used, uint8_t segment)
// {
//     // NOTE: Assumes that off_white and off_white_tail are not NULL!!!!
//     NecroGCSegment* gcs = gc.segments + segment;
//     if (gcs->free == NULL)
//         necro_gc_alloc_page(gcs, gc.white_color);
//     NecroSlab* slab      = gcs->free;
//     gcs->free            = slab->next;
//     slab->prev           = NULL;
//     slab->tag            = 0;
//     slab->slots_used     = slots_used;
//     slab->segment        = segment;
//     slab->color          = gc.white_color;
//     slab->next           = gcs->off_white;
//     gcs->off_white->prev = slab;
//     gcs->off_white       = slab;
//     // zero out memory...better to do it here!?
//     int64_t** data = (int64_t**)(&slab->slots_used);
//     for (uint32_t i = 0; i < slots_used; ++i)
//     {
//         data[i] = NULL;
//     }
//     return (int64_t*) data;
// }


/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

// TODO:
//    * Memory Management
//    * Switch statements for pattern matching
//    * c calls
//    * Audio

// Current value pointers for each construct?
// Gets updated when it's demanded?
/*
    Idea #1
    Semantic analysis: https://ruslanspivak.com/lsbasi-part13/
    Necronomicon is strict and ref counted, with these exceptions:
        - delay :: const a -> a -> a
          keyword, lazy, uses a weak ptr to second argument
          Induces 1 block sample delay.
          Only takes a constant expression in the first argument, preventing nested delays (and further memory than exactly 1 block).
          This means the language can only ever look back exactly 1 sample in time.
        - poly functions allow dynamic streams.
          These newly created can be destroyed, and thus this requires ref counting all nodes.
          GC isn't necessary because we are strict and only allow recursion through delay, which uses a weak ptr scheme.

    // Multiple fby's breaks this!?
    Idea #2
    Lucid style evaluation model :
        * An immutable Network where each node can process from any time N to time N + 1
        * Evaluation is fully lazy and allows full recursion, as long as a delay is introduced.
        * Each value has a place (variable) and time (time slice)).
        * This means you need envs for variable lookups, but you also need envs for time lookups!
        * However instead of their "Retirement Age" scheme, we use "Time Streams".

        * Each node caches a different value for each separate "Time stream" (i.e. Pattern slot, or synth),
          as well as caching the nodes value at time 0

        * This means that there is at most on cached value for each time stream (constant space)
        * Separate time streams can no longer share work, however they also always calculate one time slice value each tick (constant time)
        * This also supports LexicalTime scoping, since you simply look up the node's value for the non-local time stream up in the next scope in your env

        * This all works because we only provide the programmer with combinators that can at most reach one time slice into the past,
          such as fby (delays one sample), and [] sequences (each stream is stepped forward one time slice at a time)
        * Rewinding and other nonsense is strictly prohibited

        * This scheme works better for Necronomicon because we'd rather pay a little bit more overhead
          overall to maintain a clean constant time/space system rather than rely on complex
          GC system that could at any point pause the system for a long period of time.
        * The other upshot is the language gets to have correct lazy semantics

    // Idea #3
    All in on Lexical time scoping and pattern sequences:
        - No more "fby"
        - In lieu we have pattern sequences: they keep going until the sequence "yields" (as opposed to fby, which immediately switches from seq1 to seq2)
        - This simplifies the language and the semantics, since there's only 1 mechanism to introduce time asynchronicity
        - Each node cache's it's last value only
        - Only need ref counting for when streams get torn down
        - Streams get replicated when part of a pattern sequence or when spawned by poly

    Lexical time scoping
        - By default Necronomicon uses local time semantics
        - i.e. when you introduce a new time stream via fby or pattern sequences [],
          the values proceed, such that: x fby y == { x0, y0, y1, y2, ... };
        - Necronomicon optionally allows the user to use switch to "Lexical Time scoping" via the unary ~ operator
        - Thus, x fby ~y == { x0, y1, y2, y3, y4, ... }
        - More precisely it means reference the time stream y that already exists in this scope instead of
          creating a fresh time stream of y.
        - If there is no higher scoped version of that sequence (i.e. the sequence is defined at the same scope),
          then this simply translates into a noop.

    Different Stream states:
        - Const, Live, Rest, Inhibit, End?
        - Would allow for --> semantics (i.e. actual switching)
        - Pattern type functions such as seq and cycle would be defined in terms of this.

    Lists are simply chained "pattern" sequences (similar to fbys)?
    - Semantics are a little different than fbys:
    - Each sequence continues until it "yields"
    - Then sequence moves on to next stream
    myCoolLoop = 0 fby 100 fby 1 + myCoolLoop  == { 0, 100, 1 ... }
    myCoolLoop = 0 fby 100 fby 1 + ~myCoolLoop ==

    Memory Management Idea #1
    region based memory management, based on graphs
        * Each graph gets its own Region
        * Each Region is comprised of a current RegionPage list and the previous RegionPage list
        * Nodes can access data from either the current RegionPage list or the previous RegionPage list
        * At the beginning of each evaluation we free the previous RegionPage list,
          move the current RegionPage list to the previous RegionPage list,
          then allocate a new RegionPage and set the current RegionPage list to that.
        * Thus, we only ever have around twice the memory required
        * In return we get deterministic memory allocation and freeing

    Memory Management Idea #2
    Treadmill style incremental garbage collection,
    bounded by collection time,
    called at the end of every tick

    // TODO: Will need nodemap
*/

/*
    Structure of NecroStructs:
    - ref_count: 8 bytes
    - N args:    8 bytes * N
    // all structs heap allocated and ref_counted
*/

//=====================================================
// VM
//=====================================================

typedef enum
{
    // Integer operations
    N_PUSH_I,
    N_ADD_I,
    N_SUB_I,
    N_MUL_I,
    N_NEG_I,
    N_DIV_I,
    N_MOD_I,
    N_EQ_I,
    N_NEQ_I,
    N_LT_I,
    N_LTE_I,
    N_GT_I,
    N_GTE_I,
    N_BIT_AND_I,
    N_BIT_OR_I,
    N_BIT_XOR_I,
    N_BIT_LS_I,
    N_BIT_RS_I,

    // Functions
    N_CALL,
    N_RETURN,
    N_C_CALL_1,
    N_C_CALL_2,
    N_C_CALL_3,
    N_C_CALL_4,
    N_C_CALL_5,

    // structs
    N_MAKE_STRUCT,
    N_GET_FIELD,
    N_SET_FIELD,

    // Memory
    N_LOAD_L,
    N_STORE_L,

    // Jumping
    N_JMP,
    N_JMP_IF,
    N_JMP_IF_NOT,

    // Commands
    N_POP,
    N_PRINT,
    N_PRINT_STACK,
    N_HALT
} NECRO_BYTE_CODE;

// Type information, needed for GC
typedef struct
{
    uint64_t boxed_slot_bit_field;
    uint32_t size_in_bytes;
    uint32_t gc_segment;
    uint32_t num_slots;
} NecroTypeInfo;

// In-band 8 byte struct information.
// Every struct has this appended
// Used for pattern matching and for GC
typedef struct
{
    uint32_t pattern_tag;
    uint32_t type_index;
} NecroStructInfo;

// Out of band 16 byte Tag
typedef struct NecroTMTag
{
    struct NecroTMTag* prev;
    struct NecroTMTag* next;
} NecroTMTag;

typedef union NecroVal
{
    int64_t         int_value;
    double          float_value;
    union NecroVal* necro_pointer;
    NecroStructInfo struct_info;
} NecroVal;

typedef NecroVal (*necro_c_call_1)(NecroVal);
typedef NecroVal (*necro_c_call_2)(NecroVal, NecroVal);
typedef NecroVal (*necro_c_call_3)(NecroVal, NecroVal, NecroVal);
typedef NecroVal (*necro_c_call_4)(NecroVal, NecroVal, NecroVal, NecroVal);
typedef NecroVal (*necro_c_call_5)(NecroVal, NecroVal, NecroVal, NecroVal, NecroVal);

#define NECRO_INIITAL_NUM_PAGES 64
#define NECRO_REGION_PAGE_SIZE  (65536 - sizeof(void*)) // The last word size of bytes are reserved for the next pointer

//=====================================================
// Region based memory management
//=====================================================
typedef struct NecroRegionPage
{
    char data[NECRO_REGION_PAGE_SIZE];
    struct NecroRegionPage* next_page;
} NecroRegionPage;

typedef struct
{
    NecroRegionPage* previous_head;
    NecroRegionPage* previous_last;
    NecroRegionPage* current_head;
    NecroRegionPage* current_last;
    size_t           cursor;
} NecroRegion;

typedef struct
{
    char* node_map;
} NecroSubRegion;

typedef struct NecroRegionBlock
{
    struct NecroRegionBlock* next_block;
} NecroRegionBlock;

typedef struct
{
    NecroRegionBlock* page_blocks;
    NecroRegionPage*  free_list;
    size_t            current_block_size;
} NecroRegionPageAllocator;
NecroRegionPageAllocator necro_create_region_page_allocator();
void                     necro_destroy_region_page_allocator(NecroRegionPageAllocator* page_allocator);
NecroRegionPage*         necro_alloc_region_page(NecroRegionPageAllocator* page_allocator);
NecroRegion              necro_create_region(NecroRegionPageAllocator* page_allocator);
char*                    necro_alloc_into_region(NecroRegionPageAllocator* page_allocator, NecroRegion* region, size_t size);
void                     necro_cycle_region(NecroRegionPageAllocator* page_allocator, NecroRegion* region);

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


//=====================================================
// Treadmill Memory management:
//     * Incremental
//     * Precise
//     * Non-copying (Never invalidates pointers, never reallocates, never copies)
//     * Segmented
//     * Fixed Page length
//     * Bounded collection time
//     * Collection called every tick
//     * Adds up to trading overall efficiency
//       in space and time for lower latency.
//=====================================================

#define NECRO_NUM_TM_SEGMENTS 6
#define NECRO_TM_PAGE_SIZE    1024
#define DEBUG_TM              0

#if DEBUG_TM
#define TRACE_TM(...) printf(__VA_ARGS__)
#else
#define TRACE_TRACE_TM(...)
#endif

typedef struct NecroTMPageHeader
{
    NecroTMTag                tags[NECRO_TM_PAGE_SIZE];
    uint64_t                  ecru_flags[NECRO_TM_PAGE_SIZE / 64];
    struct NecroTMPageHeader* next_page;
} NecroTMPageHeader;

typedef struct
{
    NecroTypeInfo*     type_infos; // TODO: Have to get TypeInfo into here!
    NecroTMPageHeader* pages;
    NecroTMTag*        top[NECRO_NUM_TM_SEGMENTS];
    NecroTMTag*        bottom[NECRO_NUM_TM_SEGMENTS];
    NecroTMTag*        free[NECRO_NUM_TM_SEGMENTS];
    NecroTMTag*        scan[NECRO_NUM_TM_SEGMENTS];
} NecroTreadmill;

NecroTreadmill necro_create_treadmill(size_t num_initial_pages);
NecroVal       necro_treadmill_alloc(NecroTreadmill* treadmill, uint32_t size);

//=====================================================
// Testing
//=====================================================
void necro_test_vm();
void necro_trace_stack(int64_t opcode);
void necro_test_region();
void necro_test_slab();

#define NECRO_STACK_SIZE 1024
#define DEBUG_VM 0

#if DEBUG_VM
#define TRACE_STACK(opcode) necro_trace_stack(opcode)
#else
#define TRACE_STACK(opcode)
#endif

#endif // RUNTIME_H


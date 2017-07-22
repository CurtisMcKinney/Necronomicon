/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "runtime.h"
#include "utility.h"

// Prev Page, current page solves the problem
// We only ever have twice the memory
// Use regions and region pages instead?

//=====================================================
// VM
//=====================================================

void necro_trace_stack(int64_t opcode)
{
    switch (opcode)
    {
    case N_PUSH_I:      puts("N_PUSH_I");      return;
    case N_ADD_I:       puts("N_ADD_I");       return;
    case N_SUB_I:       puts("N_SUB_I");       return;
    case N_MUL_I:       puts("N_MUL_I");       return;
    case N_NEG_I:       puts("N_NEG_I");       return;
    case N_DIV_I:       puts("N_DIV_I");       return;
    case N_MOD_I:       puts("N_MOD_I");       return;
    case N_EQ_I:        puts("N_EQ_I");        return;
    case N_NEQ_I:       puts("N_NEQ_I");       return;
    case N_LT_I:        puts("N_LT_I");        return;
    case N_LTE_I:       puts("N_LTE_I");       return;
    case N_GT_I:        puts("N_GT_I");        return;
    case N_GTE_I:       puts("N_GTE_I");       return;
    case N_BIT_AND_I:   puts("N_BIT_AND_I");   return;
    case N_BIT_OR_I:    puts("N_BIT_OR_I");    return;
    case N_BIT_XOR_I:   puts("N_BIT_XOR_I");   return;
    case N_BIT_LS_I:    puts("N_BIT_LS_I");    return;
    case N_BIT_RS_I:    puts("N_BIT_RS_I");    return;
    case N_CALL:        puts("N_CALL");        return;
    case N_RETURN:      puts("N_RETURN");      return;
    case N_LOAD_L:      puts("N_LOAD_L");      return;
    case N_STORE_L:     puts("N_STORE_L");     return;
    case N_JMP:         puts("N_JMP");         return;
    case N_JMP_IF:      puts("N_JMP_IF");      return;
    case N_JMP_IF_NOT:  puts("N_JMP_IF_NOT");  return;
    case N_POP:         puts("N_POP");         return;
    case N_PRINT:       puts("N_PRINT");       return;
    case N_PRINT_STACK: puts("N_PRINT_STACK"); return;
    case N_HALT:        puts("N_HALT");        return;
    case N_MAKE_STRUCT: puts("N_MAKE_STRUCT"); return;
    case N_GET_FIELD:   puts("N_GET_FIELD");   return;
    case N_SET_FIELD:   puts("N_SET_FIELD");   return;
    default:            puts("UKNOWN");        return;
    }
}

// Therefore, must calculate the number of local args and make room for that when calling functions
// Then you use Stores to store into the local variables from the operand stack

// This is now more like 5.5 slower than C
// Stack machine with accumulator
uint64_t necro_run_vm(uint64_t* instructions, size_t heap_size)
{
    NecroSlabAllocator sa  = necro_create_slab_allocator(32);
    register int64_t*  sp  = malloc(NECRO_STACK_SIZE * sizeof(int64_t)); // Stack grows down
    register int64_t*  pc  = instructions;
    register int64_t*  fp  = 0;
    register int64_t   acc = 0;
    register int64_t   env = 0;
    sp = sp + (NECRO_STACK_SIZE - 1);
    fp = sp - 2;
    pc--;
    while (true)
    {
        int64_t opcode = *++pc;
        TRACE_STACK(opcode);
        switch (opcode)
        {
        // Integer operations
        case N_PUSH_I:
            *--sp = acc;
            acc = *++pc;
            break;
        case N_ADD_I:
            acc = acc + *sp++;
            break;
        case N_SUB_I:
            acc = acc - *sp++;
            break;
        case N_MUL_I:
            acc = acc * *sp++;
            break;
        case N_NEG_I:
            acc = -acc;
            break;
        case N_DIV_I:
            acc = acc / *sp++;
            break;
        case N_MOD_I:
            acc = acc % *sp++;
            break;
        case N_EQ_I:
            acc = acc == *sp++;
            break;
        case N_NEQ_I:
            acc = acc != *sp++;
            break;
        case N_LT_I:
            acc = acc < *sp++;
            break;
        case N_LTE_I:
            acc = acc <= *sp++;
            break;
        case N_GT_I:
            acc = acc > *sp++;
            break;
        case N_GTE_I:
            acc = acc >= *sp++;
            break;
        case N_BIT_AND_I:
            acc = acc & *sp++;
            break;
        case N_BIT_OR_I:
            acc = acc | *sp++;
            break;
        case N_BIT_XOR_I:
            acc = acc ^ *sp++;
            break;
        case N_BIT_LS_I:
            acc = acc << *sp++;
            break;
        case N_BIT_RS_I:
            acc = acc >> *sp++;
            break;

        // Functions
        // Stack Frame:
        //  - args
        //  <---------- fp points at last arguments
        //              LOAD_L loads in relation to fp
        //               0... N (incrementing) loads from the final argument down towards the first argument
        //              -6...-N (decrementing) loads from the first local variable up towarods final local variable
        //  - saved registers (pc, fp, env, argc)
        //  - Locals
        //  - operand stack
        //  When calling into a subroutine, the operand
        //  stack of the current frame becomes the args of the next
        case N_CALL: // one operand: number of args to reserve in current operand stack
            sp   -= 4;
            sp[0] = (int64_t) (pc + 1); // pc gets incremented at top, so set pc to one less than call site
            sp[1] = (int64_t) fp;
            sp[2] = env;
            sp[3] = *++pc;
            fp    = sp + 4;
            pc    = (int64_t*) acc - 1; // pc gets incremented at top, so set pc to one less than call site
            env   = acc + 1;
            break;

        case N_RETURN:
            sp   = fp - 4;
            pc   = (int64_t*)sp[0];
            fp   = (int64_t*)sp[1];
            env  = sp[2];
            sp  += sp[3] + 4;
            break;

        case N_C_CALL_1:
            acc = ((necro_c_call_1)(*++pc))((NecroVal) { acc }).int_value;
            break;
        case N_C_CALL_2:
            acc = ((necro_c_call_2)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }).int_value;
            sp++;
            break;
        case N_C_CALL_3:
            acc = ((necro_c_call_3)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }).int_value;
            sp += 2;
            break;
        case N_C_CALL_4:
            acc = ((necro_c_call_4)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }, (NecroVal) { sp[2] }).int_value;
            sp += 3;
            break;
        case N_C_CALL_5:
            acc = ((necro_c_call_5)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }, (NecroVal) { sp[2] }, (NecroVal) { sp[3] }).int_value;
            sp += 4;
            break;

        // structs
        case N_MAKE_STRUCT:
        {
            *--sp = acc;
            int64_t  num_fields = *++pc;
            size_t   size       = (1 + (size_t)num_fields) * sizeof(int64_t); // Will need 1 extra for refcount field
            int64_t* struct_ptr = necro_alloc_slab(&sa, size);
            struct_ptr[0]       = 0; // Set ref count to 0
            memcpy(struct_ptr + 1, sp, size);
            acc = (int64_t) struct_ptr;
            sp += num_fields;
            break;
        }

        case N_GET_FIELD: // acc = struct_ptr, next pc = field number, replaces acc with result
            acc = ((int64_t*)acc)[*++pc + 1];
            break;

        case N_SET_FIELD: // acc = struct_ptr, next pc = field number, sp[0] = value to set to, pops struct from acc
            ((int64_t*)acc)[*++pc + 1] = *sp++;
            acc = *sp++;
            break;

        // Memory
        case N_LOAD_L: // Loads a local variable relative to current frame pointer
            *--sp = acc;
            acc   = *(fp + *++pc);
            break;
        case N_STORE_L: // Stores a local variables relative to current frame pointer
            *(fp + *++pc) = acc;
            acc = *sp++;
            break;

        // Jumping
        case N_JMP:
            pc++;
            pc += *pc; // Using relative jumps
            break;
        case N_JMP_IF:
            pc++;
            if (acc)
                pc += *pc;
            acc = *sp--;
            break;
        case N_JMP_IF_NOT:
            pc++;
            if (!acc)
                pc += *pc;
            acc = *sp--;
            break;

        // Commands
        case N_POP:
            sp++;
            break;
        case N_PRINT:
            printf("PRINT: %lld\n", acc);
            break;
        case N_PRINT_STACK:
            printf("PRINT_STACK:\n    ACC: %lld\n", acc);
            {
                size_t i = 0;
                for (int64_t* sp2 = sp; sp2 <= fp; ++sp2)
                {
                    printf("    [%d]: %lld\n", i, *sp2);
                    ++i;
                }
            }
            break;
        case N_HALT:
            return acc;
        default:
            printf("Unrecognized command: %lld\n", *pc);
            return acc;
        }
    }

    // cleanup
    free(sp);
    necro_destroy_slab_allocator(&sa);
}

//=====================================================
// Treadmill
// Top >---(grey)---> Scan >---(black)---> Free >---(white)---> Bottom >---(ecru)---> Top {loop around}
// Note: This assumes the arena size will never exceed 2 ^ 27 = 134,217,728 nodes in each bin
//=====================================================

//---------------------
// Helper functions:
// Bit twiddle madness
//---------------------
inline NecroVal* necro_get_start_of_page_data_from_val(NecroVal* val, uint64_t data_size_in_bytes)
{
    return (NecroVal*) (((uint64_t)val) & ~(data_size_in_bytes - 1));
}

inline NecroTMPageHeader* necro_get_page_header_from_val(NecroVal* val, uint64_t data_size_in_bytes)
{
    return (NecroTMPageHeader*)(necro_get_start_of_page_data_from_val(val, data_size_in_bytes)) - 1;
}

inline NecroTMTag* necro_get_tag_from_val(NecroVal* val, uint64_t data_size_in_bytes)
{
    NecroTMPageHeader* page_header = necro_get_page_header_from_val(val, data_size_in_bytes);
    uint64_t           index       = val - ((NecroVal*)(page_header + 1));
    return page_header->tags + index;
}

inline NecroTMTag* necro_get_next_from_val(NecroVal* val, NecroTMPageHeader* page_header)
{
    uint64_t index = val - ((NecroVal*)(page_header + 1));
    return page_header->tags[index].next;
}

inline NecroTMTag* necro_get_prev_from_val(NecroVal* val, NecroTMPageHeader* page_header)
{
    uint64_t index = val - ((NecroVal*)(page_header + 1));
    return page_header->tags[index].prev;
}

inline bool necro_is_ecru_from_val(NecroVal* val, NecroTMPageHeader* page_header)
{
    uint64_t index      = (val - ((NecroVal*)(page_header + 1)));
    uint64_t ecru_index = index >> 6;
    uint64_t bit_index  = (uint64_t) 1 << (index & 63);
    return page_header->ecru_flags[ecru_index] & bit_index;
}

inline void necro_set_ecru_from_val(NecroVal* val, NecroTMPageHeader* page_header, bool is_ecru)
{
    uint64_t index      = (val - ((NecroVal*)(page_header + 1)));
    uint64_t ecru_index = index >> 6;
    uint64_t bit_index  = (uint64_t) 1 << (index & 63);
    if (is_ecru)
        page_header->ecru_flags[ecru_index] |= bit_index;
    else
        page_header->ecru_flags[ecru_index] &= ~bit_index;
}

inline NecroVal* necro_get_data_from_tag(NecroTMTag* tag)
{
    NecroTMPageHeader* page_header = (NecroTMPageHeader*)(((uint64_t)tag) & ~(sizeof(NecroTMTag*) - 1));
    uint64_t           index       = tag - page_header->tags;
    return (NecroVal*)(page_header + 1) + index;
}

inline bool necro_is_ecru_from_tag(NecroTMTag* tag)
{
    NecroTMPageHeader* page_header = (NecroTMPageHeader*)(((uint64_t)tag) & ~(sizeof(NecroTMTag*) - 1));
    uint64_t           index       = tag - page_header->tags;
    uint64_t           ecru_index  = index >> 6;
    uint64_t           bit_index   = (uint64_t) 1 << (index & 63);
    return page_header->ecru_flags[ecru_index] & bit_index;
}

inline void necro_set_ecru_from_tag(NecroTMTag* tag, bool is_ecru)
{
    NecroTMPageHeader* page_header = (NecroTMPageHeader*)(((uint64_t)tag) & ~(sizeof(NecroTMTag*) - 1));
    uint64_t           index       = tag - page_header->tags;
    uint64_t           ecru_index  = index >> 6;
    uint64_t           bit_index   = (uint64_t) 1 << (index & 63);
    if (is_ecru)
        page_header->ecru_flags[ecru_index] |= bit_index;
    else
        page_header->ecru_flags[ecru_index] &= ~bit_index;
}

inline void necro_unsnap(NecroTMTag* tag)
{
    NecroTMTag* prev = tag->prev;
    NecroTMTag* next = tag->next;
    prev->next       = next;
    next->prev       = prev;
    tag->prev        = NULL;
    tag->next        = NULL;
}

inline void necro_snap_in(NecroTMTag* tag, NecroTMTag* new_prev, NecroTMTag* new_next)
{
    new_prev->next = tag;
    tag->prev      = new_prev;
    tag->next      = new_next;
    new_next->prev = tag;
}

inline void necro_relink(NecroTMTag* tag, NecroTMTag* new_prev, NecroTMTag* new_next)
{
    necro_unsnap(tag);
    necro_snap_in(tag, new_prev, new_next);
}

//---------------
// API functions

// Segments go in powers of 2: 2, 4, 8, 16, 32, 64, etc
NecroTreadmill necro_create_treadmill(size_t num_initial_pages)
{
    NecroTreadmill treadmill;
    treadmill.pages = NULL;
    for (size_t segment = 0; segment < NECRO_NUM_TM_SEGMENTS; ++segment)
    {
        size_t size      = 2 << segment;
        size_t page_size = sizeof(NecroTMPageHeader) + size * sizeof(int64_t) * NECRO_TM_PAGE_SIZE;
        char*  data      = malloc(num_initial_pages * page_size);
        for (size_t page = 0; page < num_initial_pages; ++page)
        {
            NecroTMPageHeader* prev_page_header = (NecroTMPageHeader*)(data + page_size * (page == 0 ? num_initial_pages - 1 : page - 1));
            NecroTMPageHeader* page_header      = (NecroTMPageHeader*)(data + page_size * page);
            NecroTMPageHeader* next_page_header = (NecroTMPageHeader*)(data + page_size * ((page + 1) % num_initial_pages));
            for (size_t block = 0; block < NECRO_TM_PAGE_SIZE; ++block)
            {
                // Set prev pointer
                page_header->tags[block].prev = (block == 0) ?
                    prev_page_header->tags + (NECRO_TM_PAGE_SIZE - 1) :
                    page_header->tags + (block - 1);
                // Set next pointer
                page_header->tags[block].next = (block + 1 >= NECRO_TM_PAGE_SIZE) ?
                    next_page_header->tags :
                    page_header->tags + (block + 1);
                // Set ecru flags to false
                memset(page_header->ecru_flags, 0, NECRO_TM_PAGE_SIZE / 64);
                // Set up page list
                page_header->next_page = treadmill.pages;
                treadmill.pages = page_header;
            }
        }
        treadmill.free[segment]   = ((NecroTMPageHeader*)(data))->tags;
        treadmill.top[segment]    = ((NecroTMPageHeader*)(data))->tags;
        treadmill.bottom[segment] = ((NecroTMPageHeader*)(data))->tags;
        treadmill.scan[segment]   = ((NecroTMPageHeader*)(data))->tags;
    }
    return treadmill;
}

NecroVal necro_treadmill_alloc(NecroTreadmill* treadmill, uint32_t size_in_bytes)
{
    uint64_t num_slots = next_highest_pow_of_2(size_in_bytes) >> 3;
    assert(num_slots <= 64); // Right now this only supports up to 64 slots = 8 * 64 = 512 bytes
    uint64_t segment =
        (num_slots & 63)  * 5 +
        (num_slots & 31)  * 4 +
        (num_slots & 15)  * 3 +
        (num_slots & 7)   * 2 +
        (num_slots & 3)   * 1;
    if (treadmill->free[segment] == treadmill->bottom[segment])
    {
        // Flip, if out of memory, alloc more!
    }
    NecroTMTag* allocated_tag = treadmill->free[segment];
    NecroVal*   data          = necro_get_data_from_tag(allocated_tag);
    treadmill->free[segment]  = allocated_tag->next;
    necro_set_ecru_from_tag(allocated_tag, 1);
    NecroVal value;
    value.necro_pointer = data;
    return value;
}

inline bool necro_treadmill_scan(NecroTreadmill* treadmill, uint64_t segment)
{
    if (treadmill->scan[segment] == treadmill->top[segment])
        return true;
    NecroTMTag* tag = treadmill->scan[segment];
    if (!necro_is_ecru_from_tag(tag))
    {
        // Move scan pointer backwards
        treadmill->scan[segment] = treadmill->scan[segment]->prev;
        return false;
    }
    NecroVal*       data        = necro_get_data_from_tag(tag);
    NecroStructInfo struct_info = data->struct_info;
    NecroTypeInfo   type_info   = treadmill->type_infos[struct_info.type_index];
    necro_set_ecru_from_tag(tag, 0);

    for (uint64_t i = 0; i < type_info.num_slots; ++i)
    {
        // if not a Boxed member, skip
        if ((type_info.boxed_slot_bit_field | ((uint64_t)1 << i)) != ((uint64_t)1 << i))
            continue;
        NecroVal*   slot_value_ptr = data[i + 1].necro_pointer;
        NecroTMTag* slot_tag       = necro_get_tag_from_val(slot_value_ptr, type_info.size_in_bytes);
        if (!necro_is_ecru_from_tag(slot_tag))
            continue;
        necro_relink(slot_tag, treadmill->scan[segment]->prev, treadmill->scan[segment]); // Relink node behind the scan pointer
    }
    // Move scan pointer backwards
    treadmill->scan[segment] = treadmill->scan[segment]->prev;
    return false;
}

// Need to handle stopping and picking up mid scan
void necro_treadmill_collect(NecroTreadmill* treadmill, NecroVal root_ptr)
{
    // Only link root in at BEGINNING of collection cycle, not every re-entrance into it!
    NecroVal*       root_val    = root_ptr.necro_pointer;
    NecroStructInfo struct_info = root_val->struct_info;
    NecroTypeInfo   type_info   = treadmill->type_infos[struct_info.type_index];
    NecroTMTag*     tag         = necro_get_tag_from_val(root_val, type_info.size_in_bytes);
    necro_relink(tag, treadmill->top[type_info.gc_segment], treadmill->scan[type_info.gc_segment]); // Move Root to grey list between top and scan pointers
    treadmill->scan[type_info.gc_segment] = tag;
    // Continue scanning until all scan node of all segments points to same node as top pointer
    bool is_done = false;
    while (!is_done)
    {
        is_done = true;
        for (size_t i = 0; i < NECRO_NUM_TM_SEGMENTS; ++i)
        {
            // scan segment
            bool segment_done = necro_treadmill_scan(treadmill, i);
            is_done = is_done && segment_done;
        }
    }
    // Scan is done, do cleanup
}

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

void print_slab_allocator_test_result(const char* print_string, bool result)
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

    print_slab_allocator_test_result("vec 1 + 2 alloc:", vec3_1[0] == vec3_2[2] && vec3_1[1] == vec3_2[1] && vec3_1[2] == vec3_2[0]);
    necro_free_slab(&slab_allocator, vec3_1, sizeof(int64_t) * 3);
    print_slab_allocator_test_result("vec 2 + 3 alloc:", vec3_2[0] == 6 && vec3_2[1] == 5 && vec3_2[2] == 4);

    int64_t* vec3_3 = necro_alloc_slab(&slab_allocator, sizeof(int64_t) * 3);
    vec3_3[0] = 5;
    vec3_3[1] = 4;
    vec3_3[2] = 6;
    print_slab_allocator_test_result("vec 2 + 3 alloc:", vec3_3[1] == vec3_2[2] && vec3_3[0] == vec3_2[1] && vec3_3[2] == vec3_2[0]);

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
    int64_t iterations = 65536;
    {
        // Slab bench
        NecroSlabAllocator slab_allocator = necro_create_slab_allocator(16384);
        double start_time = (double)clock() / (double)CLOCKS_PER_SEC;
        int64_t* mem;
        for (size_t i = 0; i < iterations; ++i)
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
        for (size_t i = 0; i < iterations; ++i)
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

//=====================================================
// Buddy Allocator
//=====================================================
// NecroBuddyAllocator necro_create_buddy_allocator()
// {
//     // Allocate and initialize memory
//     size_t total_size = BUDDY_HEAP_MAX_SIZE * sizeof(NecroBuddyLeaf) + BUDDY_HEAP_FREE_FLAGS_SIZE;
//     char*  heap       = malloc(total_size);
//     char*  free_flags = heap + BUDDY_HEAP_MAX_SIZE;
//     memset(heap, (int) NULL, BUDDY_HEAP_MAX_SIZE * sizeof(NecroBuddyLeaf));
//     memset(free_flags, 0, BUDDY_HEAP_FREE_FLAGS_SIZE);
//     // Set up free lists
//     NecroBuddyAllocator necro_buddy;
//     necro_buddy.heap = (NecroBuddyLeaf*) heap;
//     for (size_t i = 0; i < BUDDY_HEAP_NUM_BINS; ++i)
//         necro_buddy.free_lists[i] = NULL;
//     necro_buddy.free_lists[BUDDY_HEAP_NUM_BINS - 1] = (NecroBuddyLeaf*)heap;
//     return necro_buddy;
// }

// char* necro_buddy_alloc(NecroBuddyAllocator* buddy, size_t size)
// {
//     size_t size_pow_of_2    = (size /* converto pow of 2*/) - BUDDY_HEAP_LEAF_SIZE_POW_2;

//     // Find next free block in closest size
//     size_t          current_pow_of_2 = size_pow_of_2;
//     NecroBuddyLeaf* current_slot     = buddy->free_lists[size_pow_of_2];

//     // Find next bin with free block
//     while (current_slot == NULL && current_pow_of_2 < BUDDY_HEAP_NUM_BINS)
//     {
//         current_pow_of_2++;
//         current_slot = buddy->free_lists[current_pow_of_2];
//     }

//     // even = i
//     // odd  = i - 1?

//     // iteratively break down blocks into buddies
//     for (size_t s = current_pow_of_2; current_slot != NULL && s > size_pow_of_2; --s)
//     {
//         NecroBuddyLeaf* buddy_1 = current_slot;
//         NecroBuddyLeaf* buddy_2 = current_slot + (2 << (current_pow_of_2 - 2));

//         // buddy->free_lists[size_pow_of_2] = *((int64_t*)starting_slot);
//     }

//     return (char*) current_slot;
// }

//=====================================================
// Region Allocator
//=====================================================
NecroRegionPageAllocator necro_create_region_page_allocator()
{
    NecroRegionBlock* page_block = malloc(sizeof(NecroRegionBlock) + NECRO_INIITAL_NUM_PAGES * sizeof(NecroRegionPage));
    if (page_block == NULL)
    {
        fprintf(stderr, "Allocation error, could not allocate memory for page allocator. Requested memory size: %d\n", sizeof(NecroRegionBlock) + NECRO_INIITAL_NUM_PAGES * sizeof(NecroRegionPage));
        exit(1);
    }
    NecroRegionPage*  free_list  = (NecroRegionPage*) (((char*) page_block) + sizeof(NecroRegionBlock));
    for (size_t i = 0; i < NECRO_INIITAL_NUM_PAGES - 1; ++i)
        free_list[i].next_page = &free_list[i + 1];
    free_list[NECRO_INIITAL_NUM_PAGES - 1].next_page = NULL;
    page_block->next_block = NULL;
    assert(page_block);
    assert(free_list);
    return (NecroRegionPageAllocator) { page_block, free_list, NECRO_INIITAL_NUM_PAGES };
}

void necro_destroy_region_page_allocator(NecroRegionPageAllocator* page_allocator)
{
    NecroRegionBlock* current_block = page_allocator->page_blocks;
    while (current_block != NULL)
    {
        NecroRegionBlock* next_block = current_block->next_block;
        free(current_block);
        current_block = next_block;
    }
}

NecroRegionPage* necro_alloc_region_page(NecroRegionPageAllocator* page_allocator)
{
    if (page_allocator->free_list == NULL)
    {
        // dynamically allocate more memory from OS
        page_allocator->current_block_size *= 2;
        fprintf(stderr, "Not enough memory in region page allocator, allocating %uz memory from the OS\n", page_allocator->current_block_size * sizeof(NecroRegionPage));
        NecroRegionBlock* page_block    = malloc(sizeof(NecroRegionBlock) + page_allocator->current_block_size * sizeof(NecroRegionPage));
        if (page_block == NULL)
        {
            fprintf(stderr, "Allocation error, could not allocate memory for page allocator. Requested memory size: %d\n", sizeof(NecroRegionBlock) + page_allocator->current_block_size * sizeof(NecroRegionPage));
            exit(1);
        }
        NecroRegionPage*  new_free_list = (NecroRegionPage*) (((char*) page_block) + sizeof(NecroRegionBlock));
        for (size_t i = 0; i < page_allocator->current_block_size - 1; ++i)
            new_free_list[i].next_page = &new_free_list[i + 1];
        new_free_list[page_allocator->current_block_size - 1].next_page = NULL;
        page_allocator->free_list   = new_free_list;
        page_block->next_block      = page_allocator->page_blocks;
        page_allocator->page_blocks = page_block;
    }
    assert(page_allocator->free_list);
    NecroRegionPage* page = page_allocator->free_list;
    page_allocator->free_list = page->next_page;
    page->next_page = NULL;
    return page;
}

NecroRegion necro_create_region(NecroRegionPageAllocator* page_allocator)
{
    NecroRegionPage* page      = necro_alloc_region_page(page_allocator);
    NecroRegionPage* last_page = necro_alloc_region_page(page_allocator);
    return (NecroRegion) { last_page, last_page, page, page, 0 };
}

char* necro_alloc_into_region(NecroRegionPageAllocator* page_allocator, NecroRegion* region, size_t size)
{
    size_t new_cursor = region->cursor + size;
    if (new_cursor >= NECRO_REGION_PAGE_SIZE)
    {
        new_cursor                      = size;
        region->current_last->next_page = necro_alloc_region_page(page_allocator);
        region->current_last            = region->current_last->next_page;
    }
    char* data = ((char*)region->current_last->data) + region->cursor;
    region->cursor = new_cursor;
    return data;
}

void necro_cycle_region(NecroRegionPageAllocator* page_allocator, NecroRegion* region)
{
    region->previous_last->next_page = page_allocator->free_list;
    page_allocator->free_list        = region->previous_head;
    NecroRegionPage* new_current     = necro_alloc_region_page(page_allocator);
    region->previous_head            = region->current_head;
    region->previous_last            = region->current_last;
    region->current_head             = new_current;
    region->current_last             = new_current;
}

void necro_test_region()
{
    puts("\n------");
    puts("Test Regions\n");

    NecroRegionPageAllocator page_allocator = necro_create_region_page_allocator();
    assert(page_allocator.current_block_size == NECRO_INIITAL_NUM_PAGES);
    assert(page_allocator.free_list          != NULL);
    assert(page_allocator.page_blocks        != NULL);

    NecroRegion region = necro_create_region(&page_allocator);
    assert(region.previous_head != NULL);
    assert(region.current_head  != NULL);
    assert(region.current_last  != NULL);
    assert(region.previous_last != NULL);
    assert(region.current_head  == region.current_last);
    assert(region.previous_head == region.previous_last);
    assert(region.cursor        == 0);

    // String 1 test
    char* hello = necro_alloc_into_region(&page_allocator, &region, 6);
    assert(hello);
    hello[0] = 'h';
    hello[1] = 'e';
    hello[2] = 'l';
    hello[3] = 'l';
    hello[4] = 'o';
    hello[5] = 0;
    if (strncmp(hello, "hello", 7) == 0)
        printf("Region string allocation 1 test: passed\n");
    else
        printf("Region string allocation 1 test: FAILED, result: %s\n", hello);

    // String 2 test
    char* world = necro_alloc_into_region(&page_allocator, &region, 6);
    world[0] = 'w';
    world[1] = 'o';
    world[2] = 'r';
    world[3] = 'l';
    world[4] = 'd';
    world[5] = 0;
    if (strncmp(world, "world", 7) == 0)
        printf("Region string allocation 2 test: passed\n");
    else
        printf("Region string allocation 2 test: FAILED, result: %s\n", world);
    assert(hello != world);

    // Cursor test
    if (region.cursor == 12)
        printf("Region cursor test:              passed\n");
    else
        printf("Region cursor test:              FAILED, result: %zu\n", region.cursor);

    // Cycle tests
    NecroRegionPage* current_head = region.current_head;
    NecroRegionPage* current_last = region.current_last;
    necro_cycle_region(&page_allocator, &region);
    if (region.current_head != current_head)
        printf("Region cycle head1 test:         passed\n");
    else
        printf("Region cycle head1 test:         FAILED\n");
    if (region.current_last != current_last)
        printf("Region cycle last1 test:         passed\n");
    else
        printf("Region cycle last1 test:         FAILED\n");
    if (region.previous_head == current_head)
        printf("Region cycle head2 test:         passed\n");
    else
        printf("Region cycle head2 test:         FAILED\n");
    if (region.previous_last == current_last)
        printf("Region cycle head2 test:         passed\n");
    else
        printf("Region cycle head2 test:         FAILED\n");

    // Alloc after cycle tests
    char* welt = necro_alloc_into_region(&page_allocator, &region, 5);
    welt[0] = 'w';
    welt[1] = 'e';
    welt[2] = 'l';
    welt[3] = 't';
    welt[4] = 0;
    if (strncmp(hello, "hello", 7) == 0)
        printf("Region string allocation 3 test: passed\n");
    else
        printf("Region string allocation 3 test: FAILED, result: %s\n", hello);
    if (strncmp(welt, "welt", 6) == 0)
        printf("Region string allocation 4 test: passed\n");
    else
        printf("Region string allocation 4 test: FAILED, result: %s\n", welt);
    // Cycling numbers test
    necro_cycle_region(&page_allocator, &region);
    size_t* num1 = (size_t*) necro_alloc_into_region(&page_allocator, &region, sizeof(size_t));
    *num1 = 10;
    necro_cycle_region(&page_allocator, &region);
    size_t* num2 = (size_t*) necro_alloc_into_region(&page_allocator, &region, sizeof(size_t));
    *num2 = 20;
    if (((size_t*)region.current_head->data)[0] == 20 && ((size_t*)region.previous_head->data)[0] == 10)
        printf("Region num cycling test:         passed\n");
    else
        printf("Region num cycling test:         FAILED, result: %zu, %zu, %zu, %zu\n", *num1, *num2, *((size_t*)region.current_head->data),  *((size_t*)region.previous_head->data));
    for (size_t i = 0; i < 16; ++i)
        printf("%zu\n", ((size_t*)region.current_head->data)[i]);
}

void necro_test_vm_eval(int64_t* instr, int64_t result, const char* print_string)
{
    int64_t vm_result = necro_run_vm(instr, 0);
    if (vm_result == result)
        printf("%s passed\n", print_string);
    else
        printf("%s FAILED\n    expected result: %lld\n    vm result:       %lld\n\n", print_string, result, vm_result);
#if DEBUG_VM
    puts("");
#endif
}

void necro_test_vm()
{
    puts("\n------");
    puts("Test VM\n");

    // Integer tests
    {
        int64_t instr[] =
        {
            N_PUSH_I, 7,
            N_PUSH_I, 6,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 13, "AddI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 4,
            N_PUSH_I, 9,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 5, "SubI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 3,
            N_MUL_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 30, "MulI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,     10,
            N_NEG_I,
            N_HALT
        };
        necro_test_vm_eval(instr, -10, "NegI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 1000,
            N_DIV_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 100, "DivI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 4,
            N_PUSH_I, 5,
            N_MOD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "ModI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 666,
            N_PUSH_I, 555,
            N_EQ_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "EqI:    ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 666,
            N_PUSH_I, 555,
            N_NEQ_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "NEqI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 1,
            N_PUSH_I, 2,
            N_LT_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "LTI:    ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 3,
            N_PUSH_I, 3,
            N_LTE_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "LTEI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 100,
            N_PUSH_I, 200,
            N_GT_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 1, "GTI:    ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 100,
            N_PUSH_I, 99,
            N_GTE_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "GTEI:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 7,
            N_PUSH_I, 6,
            N_BIT_AND_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 6 & 7, "BAND:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 13,
            N_PUSH_I, 4,
            N_BIT_OR_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 4 | 13, "BOR:    ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 22,
            N_PUSH_I, 11,
            N_BIT_XOR_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 11 ^ 22, "BXOR:   ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 2,
            N_PUSH_I, 4,
            N_BIT_LS_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 4 << 2, "BLS:    ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 1,
            N_PUSH_I, 5,
            N_BIT_RS_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 5 >> 1, "BRS:    ");
    }

    // Jump tests
    {
        int64_t instr[] =
        {
            N_PUSH_I, 6,
            N_JMP, 2,
            N_PUSH_I, 7,
            N_PUSH_I, 8,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 14, "Jmp:    ");
    }


    {
        int64_t instr[] =
        {
            N_PUSH_I, 3,
            N_PUSH_I, 1,
            N_JMP_IF, 4,
            N_PUSH_I, 4,
            N_JMP,    2,
            N_PUSH_I, 5,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 8, "JmpIf 1:");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 3,
            N_PUSH_I, 0,
            N_JMP_IF, 4,
            N_PUSH_I, 4,
            N_JMP,    2,
            N_PUSH_I, 5,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 7, "JmpIf 2:");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,     10,
            N_PUSH_I,     1,
            N_JMP_IF_NOT, 4,
            N_PUSH_I,     20,
            N_JMP,        2,
            N_PUSH_I,     50,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 10, "JmpIfN1:");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,     10,
            N_PUSH_I,     0,
            N_JMP_IF_NOT, 4,
            N_PUSH_I,     40,
            N_JMP,        2,
            N_PUSH_I,     50,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 40, "JmpIfN2:");
    }

    // Memory
    {
        int64_t instr[] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 5,
            N_MUL_I,
            N_LOAD_L, 0,
            N_LOAD_L, 0,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 100, "Load1:  ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 5,
            N_MUL_I,
            N_PUSH_I, 20,
            N_PUSH_I, 20,
            N_ADD_I,
            N_LOAD_L, 0,
            N_LOAD_L, -1,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, -10, "Load2:  ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,  0,
            N_PUSH_I,  0,
            N_PUSH_I,  60,
            N_STORE_L, 0,
            N_PUSH_I,  5,
            N_PUSH_I,  8,
            N_MUL_I,
            N_STORE_L, -1,
            N_LOAD_L,  0,
            N_LOAD_L,  -1,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, -20, "Store1: ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, 20,
            N_PUSH_I, (int64_t)(instr + 8),
            N_CALL,   2,
            N_LOAD_L, 1,
            N_LOAD_L, 0,
            N_ADD_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 30, "CALL1:  ");
    }

    {
        int64_t instr[] =
        {
            // Push 10, then 20, then call function
            N_PUSH_I, 10,
            N_PUSH_I, 20,
            N_PUSH_I, (int64_t)(instr + 18),
            N_CALL,   2,

            // With the result of the function on the stack, push 5, then call function again
            N_PUSH_I, 5,
            N_PUSH_I, (int64_t)(instr + 18),
            N_CALL,   2,

            // With the result of the function on the stack, push 10 then add the results, then halt
            N_PUSH_I, 10,
            N_ADD_I,
            N_HALT,

            // Function that will be called multiple times
            N_LOAD_L, 1,
            N_LOAD_L, 0,
            N_SUB_I,
            N_RETURN
        };
        necro_test_vm_eval(instr, 5, "CALL2:  ");
    }

    {
        int64_t instr[] =
        {
            // Push 5 and 6 to stack, then call function 1
            N_PUSH_I, 5,
            N_PUSH_I, 6,
            N_PUSH_I, (int64_t)(instr + 9), // function 1 addr
            N_CALL,   2, // call function1 with 2 arguments

            // End
            N_HALT,

            // Function 1
            N_LOAD_L, 1,
            N_LOAD_L, 0,
            N_ADD_I,
            N_PUSH_I, (int64_t)(instr + 19), // function 2 addr
            N_CALL,   1, // Call function2 with 1 argument
            N_RETURN,

            // Function 2
            N_LOAD_L, 0,
            N_LOAD_L, 0,
            N_MUL_I,
            N_RETURN
        };
        necro_test_vm_eval(instr, 121, "CALL3:  ");
    }

    {
        // (\x -> x2 + y where x2 = x + x; y = x) 10
        int64_t instr[] =
        {
            N_PUSH_I, 10,
            N_PUSH_I, (int64_t)(instr + 7), // addr of function
            N_CALL,   1,
            N_HALT,
            N_LOAD_L, 0,
            N_LOAD_L, 0,
            N_ADD_I,
            N_LOAD_L, 0,
            N_ADD_I,
            N_RETURN
        };
        necro_test_vm_eval(instr, 30, "CALL4:  ");
    }

    // Look into fuzzing technique?
    {
        // (\x -> x / y where x2 = x * x, y = 4 - x) 5
        int64_t instr[] =
        {
            N_PUSH_I, 5,
            N_PUSH_I, (int64_t)(instr + 7), // addr of function
            N_CALL,   1,

            N_HALT,

            // func-------
            // x2
            N_LOAD_L, 0,
            N_LOAD_L, 0,
            N_MUL_I,

            // y
            N_LOAD_L, 0,
            N_PUSH_I, 4,
            N_SUB_I,

            // x / y
            N_LOAD_L,  -7,
            N_LOAD_L,  -6,
            N_DIV_I,
            N_RETURN
        };
        necro_test_vm_eval(instr, -25, "CALL5:  ");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,      18,
            N_PUSH_I,      12,
            N_PUSH_I,      6,
            N_MAKE_STRUCT, 3,
            N_GET_FIELD,   2,
            N_HALT
        };
        necro_test_vm_eval(instr, 18, "struct1:");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,      18,
            N_PUSH_I,      12,
            N_PUSH_I,      6,
            N_MAKE_STRUCT, 3,
            N_LOAD_L,      0,
            N_GET_FIELD,   0,
            N_LOAD_L,      0,
            N_GET_FIELD,   1,
            N_LOAD_L,      0,
            N_GET_FIELD,   2,
            N_LOAD_L,      -1,
            N_LOAD_L,      -2,
            N_ADD_I,
            N_LOAD_L,      -3,
            N_SUB_I,
            N_HALT
        };
        necro_test_vm_eval(instr, 0, "struct2:");
    }

    {
        int64_t instr[] =
        {
            N_PUSH_I,      6,
            N_PUSH_I,      12,
            N_PUSH_I,      18,
            N_MAKE_STRUCT, 3,
            N_PUSH_I,      666,
            N_LOAD_L,      0,
            N_SET_FIELD,   2,
            N_LOAD_L,      0,
            N_GET_FIELD,   2,
            N_HALT
        };
        necro_test_vm_eval(instr, 666, "struct3:");
    }

}
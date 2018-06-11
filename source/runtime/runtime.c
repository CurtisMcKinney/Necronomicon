/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "runtime.h"
#include "utility.h"

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#include <Windows.h>
#else
#include <unistd.h>
#define DLLEXPORT
#endif

NecroRuntime necro_create_runtime()
{
    return (NecroRuntime)
    {
        .functions = (NecroRuntimeFunctions)
        {
            .necro_alloc = NULL,
            .necro_print = NULL
        }
    };
}

void necro_destroy_runtime(NecroRuntime* runtime)
{
}

#define NECRO_MAX_ALLOC 8192
char allocation_is_easy[NECRO_MAX_ALLOC];
uint64_t alloc_index = 0;
extern DLLEXPORT uint64_t* _necro_alloc(int64_t size)
{
    char* ptr = allocation_is_easy + alloc_index;
    // printf("alloc: %lld, %p\n", size, ptr);
    alloc_index += size;
    if (alloc_index >= NECRO_MAX_ALLOC)
        assert(false && "OUT OF MEMORY");
    return (uint64_t*) ptr;
}

extern DLLEXPORT void _necro_print(int64_t value)
{
    printf("necro: %lld\n", value);
}

extern DLLEXPORT void _necro_print_u64(uint64_t value)
{
    printf("necro: %lld\n", value);
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

void necro_declare_runtime_functions(NecroRuntime* runtime, NecroCodeGen* codegen)
{
    // NecroAlloc
    LLVMTypeRef  necro_alloc_args[1] = { LLVMInt64TypeInContext(codegen->context) };
    LLVMValueRef necro_alloc         = LLVMAddFunction(codegen->mod, "_necro_alloc", LLVMFunctionType(LLVMPointerType(LLVMInt64TypeInContext(codegen->context), 0), necro_alloc_args, 1, false));
    LLVMSetLinkage(necro_alloc, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_alloc, LLVMCCallConv);
    codegen->runtime->functions.necro_alloc = necro_alloc;

    // NecroPrint
    LLVMTypeRef  necro_print_args[1] = { LLVMInt64TypeInContext(codegen->context) };
    LLVMValueRef necro_print         = LLVMAddFunction(codegen->mod, "_necro_print", LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), necro_print_args, 1, false));
    LLVMSetLinkage(necro_print, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_print, LLVMCCallConv);
    codegen->runtime->functions.necro_print = necro_print;

    // NecroPrint64
    LLVMTypeRef  necro_print_args_u64[1] = { LLVMInt64TypeInContext(codegen->context) };
    LLVMValueRef necro_print_u64         = LLVMAddFunction(codegen->mod, "_necro_print_u64", LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), necro_print_args_u64, 1, false));
    LLVMSetLinkage(necro_print_u64, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_print_u64, LLVMCCallConv);
    codegen->runtime->functions.necro_print_u64 = necro_print_u64;

    // NecroSleep
    LLVMTypeRef  necro_sleep_args[1] = { LLVMInt32TypeInContext(codegen->context) };
    LLVMValueRef necro_sleep         = LLVMAddFunction(codegen->mod, "_necro_sleep", LLVMFunctionType(LLVMVoidTypeInContext(codegen->context), necro_sleep_args, 1, false));
    LLVMSetLinkage(necro_sleep, LLVMExternalLinkage);
    LLVMSetFunctionCallConv(necro_sleep, LLVMCCallConv);
    codegen->runtime->functions.necro_sleep = necro_sleep;
}

void necro_bind_runtime_functions(NecroRuntime* runtime, LLVMExecutionEngineRef engine)
{
    LLVMAddGlobalMapping(engine, runtime->functions.necro_alloc, _necro_alloc);
    LLVMAddGlobalMapping(engine, runtime->functions.necro_print, _necro_print);
    LLVMAddGlobalMapping(engine, runtime->functions.necro_sleep, _necro_sleep);
}

//=====================================================
// GC
//=====================================================
#define NECRO_GC_SLAB_TO_PAGES 512
#define NECRO_GC_NUM_SEGMENTS 10
typedef struct NecroGCPage
{
    struct NecroGCPage* next;
} NecroGCPage;

typedef struct NecroSlab
{
    struct NecroSlab* prev;
    struct NecroSlab* next;
    uint8_t           slots_used;
    uint8_t           segment;
    uint16_t          color;
    uint32_t          tag; // Used by necro, not gc
} NecroSlab;

typedef struct
{
    NecroGCPage* pages;
    NecroSlab*   free;
    NecroSlab*   black;
    NecroSlab*   off_white;
    size_t       slab_size;
    uint8_t      segment;
} NecroGCSegment;

inline NecroSlab* necro_cons_slab(NecroSlab* slab_1, NecroSlab* slab_2)
{
    slab_1->next = slab_2;
    slab_2->prev = slab_1;
    return slab_1;
}

inline void necro_unlink_slab(NecroSlab* slab_1)
{
    NecroSlab* prev = slab_1->prev;
    NecroSlab* next = slab_1->next;
    if (prev != NULL)
        prev->next = NULL;
    if (next != NULL)
        next->prev = NULL;
    slab_1->prev = NULL;
    slab_1->next = NULL;
}

void necro_gc_alloc_page(NecroGCSegment* segment, uint16_t white_color)
{
    NecroGCPage* page = malloc(sizeof(NecroGCPage) + segment->slab_size * NECRO_GC_SLAB_TO_PAGES);
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

char* necro_gcs_alloc(NecroGCSegment* gcs, uint16_t white_color)
{
    if (gcs->free == NULL)
        necro_gc_alloc_page(gcs, white_color);
    assert(gcs->free != NULL);
    NecroSlab* slab = gcs->free;
    necro_unlink_slab(slab);
    gcs->off_white = necro_cons_slab(slab, gcs->off_white);
    slab->color    = 1 - white_color;
    return (char*)(&slab->slots_used);
}

NecroGCSegment necro_create_gc_segment(size_t slab_size, uint8_t segment, uint16_t white_color)
{
    NecroGCSegment gcs;
    gcs.slab_size = slab_size;
    gcs.segment   = segment;
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

typedef struct
{
    NecroGCSegment segments[NECRO_GC_NUM_SEGMENTS]; // Increases by powers of 2
    NecroSlab*     gray;
    uint16_t       white_color;
    NecroSlab**    roots;
    uint32_t       root_count;
} NecroGC;

NecroGC necro_create_gc()
{
    NecroGC gc;
    gc.gray        = NULL;
    gc.white_color = 1;
    gc.roots       = NULL;
    gc.root_count  = 0;
    size_t  slots = 1;
    for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
    {
        gc.segments[i] = necro_create_gc_segment((slots * sizeof(void*)) + sizeof(NecroSlab), (uint8_t)i, gc.white_color);
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
    return (NecroSlab*)(slot - (sizeof(NecroSlab*) * 2));
}

//-----------------
// GC API
//-----------------
NecroGC gc;
char* necro_gc_alloc(uint32_t size_in_bytes)
{
    size_t slots   = next_highest_pow_of_2(((size_in_bytes - sizeof(uint64_t)) / sizeof(int64_t*)));
    size_t segment = log2_32(slots) - 1;
    assert(segment < NECRO_GC_NUM_SEGMENTS);
    return necro_gcs_alloc(gc.segments + segment, gc.white_color);
}

void necro_gc_initialize_root_set(uint32_t root_count)
{
    gc.root_count = root_count;
    gc.roots      = malloc(sizeof(NecroSlab*) * root_count);
}

void necro_gc_set_root(char* root, uint32_t root_index)
{
    gc.roots[root_index] = necro_gc_slot_to_slab(root);
}

void necro_gc_scan_slab(NecroSlab* slab, bool is_root)
{
    // Need to mark how many slot inside of necro data!!!
    char** slot = ((char**)slab) + sizeof(NecroSlab);
    for (size_t i = 0; i < slab->slots_used; ++i)
    {
        NecroSlab* slab = necro_gc_slot_to_slab(*slot);
        if (slab->color != gc.white_color)
        {
            slot += sizeof(int64_t*);
            continue;
        }
        slab->color   = 1 - gc.white_color;
        necro_unlink_slab(slab);
        gc.gray = necro_cons_slab(slab, gc.gray);
        slot += sizeof(int64_t*);
    }
    if (!is_root)
        gc.segments[slab->segment].black = necro_cons_slab(slab, gc.segments[slab->segment].black);
}

void necro_gc_collect()
{
    for (size_t i = 0; i < gc.root_count; ++i)
    {
        necro_gc_scan_slab(gc.roots[i], true);
    }
    while (gc.gray != NULL)
    {
        NecroSlab* slab = gc.gray;
        slab->color     = 1 - gc.white_color;
        necro_unlink_slab(slab);
        necro_gc_scan_slab(slab, false);
    }
    gc.white_color = 1 - gc.white_color;
    for (size_t i = 0; i < NECRO_GC_NUM_SEGMENTS; ++i)
    {
        //------------------------------
        // Move off_white to free list, implicitly collecting garbage
        // Requires an off_white tail!
        //------------------------------
        // Switch black with off white
        gc.segments[i].off_white = gc.segments[i].black;
        gc.segments[i].black     = NULL;
    }
}

//=====================================================
// VERY OLD VM STUFF
//=====================================================
// //=====================================================
// // VM
// //=====================================================

// void necro_trace_stack(int64_t opcode)
// {
//     switch (opcode)
//     {
//     case N_PUSH_I:      puts("N_PUSH_I");      return;
//     case N_ADD_I:       puts("N_ADD_I");       return;
//     case N_SUB_I:       puts("N_SUB_I");       return;
//     case N_MUL_I:       puts("N_MUL_I");       return;
//     case N_NEG_I:       puts("N_NEG_I");       return;
//     case N_DIV_I:       puts("N_DIV_I");       return;
//     case N_MOD_I:       puts("N_MOD_I");       return;
//     case N_EQ_I:        puts("N_EQ_I");        return;
//     case N_NEQ_I:       puts("N_NEQ_I");       return;
//     case N_LT_I:        puts("N_LT_I");        return;
//     case N_LTE_I:       puts("N_LTE_I");       return;
//     case N_GT_I:        puts("N_GT_I");        return;
//     case N_GTE_I:       puts("N_GTE_I");       return;
//     case N_BIT_AND_I:   puts("N_BIT_AND_I");   return;
//     case N_BIT_OR_I:    puts("N_BIT_OR_I");    return;
//     case N_BIT_XOR_I:   puts("N_BIT_XOR_I");   return;
//     case N_BIT_LS_I:    puts("N_BIT_LS_I");    return;
//     case N_BIT_RS_I:    puts("N_BIT_RS_I");    return;
//     case N_CALL:        puts("N_CALL");        return;
//     case N_RETURN:      puts("N_RETURN");      return;
//     case N_LOAD_L:      puts("N_LOAD_L");      return;
//     case N_STORE_L:     puts("N_STORE_L");     return;
//     case N_JMP:         puts("N_JMP");         return;
//     case N_JMP_IF:      puts("N_JMP_IF");      return;
//     case N_JMP_IF_NOT:  puts("N_JMP_IF_NOT");  return;
//     case N_POP:         puts("N_POP");         return;
//     case N_PRINT:       puts("N_PRINT");       return;
//     case N_PRINT_STACK: puts("N_PRINT_STACK"); return;
//     case N_HALT:        puts("N_HALT");        return;
//     case N_MAKE_STRUCT: puts("N_MAKE_STRUCT"); return;
//     case N_GET_FIELD:   puts("N_GET_FIELD");   return;
//     case N_SET_FIELD:   puts("N_SET_FIELD");   return;
//     default:            puts("UKNOWN");        return;
//     }
// }

// // Therefore, must calculate the number of local args and make room for that when calling functions
// // Then you use Stores to store into the local variables from the operand stack

// // This is now more like 5.5 slower than C
// // Stack machine with accumulator
// uint64_t necro_run_vm(uint64_t* instructions, size_t heap_size)
// {
//     NecroSlabAllocator sa  = necro_create_slab_allocator(32);
//     register int64_t*  sp  = malloc(NECRO_STACK_SIZE * sizeof(int64_t)); // Stack grows down
//     register int64_t*  pc  = instructions;
//     register int64_t*  fp  = 0;
//     register int64_t   acc = 0;
//     register int64_t   env = 0;
//     sp = sp + (NECRO_STACK_SIZE - 1);
//     fp = sp - 2;
//     pc--;
//     while (true)
//     {
//         int64_t opcode = *++pc;
//         TRACE_STACK(opcode);
//         switch (opcode)
//         {
//         // Integer operations
//         case N_PUSH_I:
//             *--sp = acc;
//             acc = *++pc;
//             break;
//         case N_ADD_I:
//             acc = acc + *sp++;
//             break;
//         case N_SUB_I:
//             acc = acc - *sp++;
//             break;
//         case N_MUL_I:
//             acc = acc * *sp++;
//             break;
//         case N_NEG_I:
//             acc = -acc;
//             break;
//         case N_DIV_I:
//             acc = acc / *sp++;
//             break;
//         case N_MOD_I:
//             acc = acc % *sp++;
//             break;
//         case N_EQ_I:
//             acc = acc == *sp++;
//             break;
//         case N_NEQ_I:
//             acc = acc != *sp++;
//             break;
//         case N_LT_I:
//             acc = acc < *sp++;
//             break;
//         case N_LTE_I:
//             acc = acc <= *sp++;
//             break;
//         case N_GT_I:
//             acc = acc > *sp++;
//             break;
//         case N_GTE_I:
//             acc = acc >= *sp++;
//             break;
//         case N_BIT_AND_I:
//             acc = acc & *sp++;
//             break;
//         case N_BIT_OR_I:
//             acc = acc | *sp++;
//             break;
//         case N_BIT_XOR_I:
//             acc = acc ^ *sp++;
//             break;
//         case N_BIT_LS_I:
//             acc = acc << *sp++;
//             break;
//         case N_BIT_RS_I:
//             acc = acc >> *sp++;
//             break;

//         // Functions
//         // Stack Frame:
//         //  - args
//         //  <---------- fp points at last arguments
//         //              LOAD_L loads in relation to fp
//         //               0... N (incrementing) loads from the final argument down towards the first argument
//         //              -6...-N (decrementing) loads from the first local variable up towarods final local variable
//         //  - saved registers (pc, fp, env, argc)
//         //  - Locals
//         //  - operand stack
//         //  When calling into a subroutine, the operand
//         //  stack of the current frame becomes the args of the next
//         case N_CALL: // one operand: number of args to reserve in current operand stack
//             sp   -= 4;
//             sp[0] = (int64_t) (pc + 1); // pc gets incremented at top, so set pc to one less than call site
//             sp[1] = (int64_t) fp;
//             sp[2] = env;
//             sp[3] = *++pc;
//             fp    = sp + 4;
//             pc    = (int64_t*) acc - 1; // pc gets incremented at top, so set pc to one less than call site
//             env   = acc + 1;
//             break;

//         case N_RETURN:
//             sp   = fp - 4;
//             pc   = (int64_t*)sp[0];
//             fp   = (int64_t*)sp[1];
//             env  = sp[2];
//             sp  += sp[3] + 4;
//             break;

//         case N_C_CALL_1:
//             acc = ((necro_c_call_1)(*++pc))((NecroVal) { acc }).int_value;
//             break;
//         case N_C_CALL_2:
//             acc = ((necro_c_call_2)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }).int_value;
//             sp++;
//             break;
//         case N_C_CALL_3:
//             acc = ((necro_c_call_3)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }).int_value;
//             sp += 2;
//             break;
//         case N_C_CALL_4:
//             acc = ((necro_c_call_4)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }, (NecroVal) { sp[2] }).int_value;
//             sp += 3;
//             break;
//         case N_C_CALL_5:
//             acc = ((necro_c_call_5)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }, (NecroVal) { sp[2] }, (NecroVal) { sp[3] }).int_value;
//             sp += 4;
//             break;

//         // structs
//         case N_MAKE_STRUCT:
//         {
//             *--sp = acc;
//             int64_t  num_fields = *++pc;
//             size_t   size       = (1 + (size_t)num_fields) * sizeof(int64_t); // Will need 1 extra for refcount field
//             int64_t* struct_ptr = necro_alloc_slab(&sa, size);
//             struct_ptr[0]       = 0; // Set ref count to 0
//             memcpy(struct_ptr + 1, sp, size);
//             acc = (int64_t) struct_ptr;
//             sp += num_fields;
//             break;
//         }

//         case N_GET_FIELD: // acc = struct_ptr, next pc = field number, replaces acc with result
//             acc = ((int64_t*)acc)[*++pc + 1];
//             break;

//         case N_SET_FIELD: // acc = struct_ptr, next pc = field number, sp[0] = value to set to, pops struct from acc
//             ((int64_t*)acc)[*++pc + 1] = *sp++;
//             acc = *sp++;
//             break;

//         // Memory
//         case N_LOAD_L: // Loads a local variable relative to current frame pointer
//             *--sp = acc;
//             acc   = *(fp + *++pc);
//             break;
//         case N_STORE_L: // Stores a local variables relative to current frame pointer
//             *(fp + *++pc) = acc;
//             acc = *sp++;
//             break;

//         // Jumping
//         case N_JMP:
//             pc++;
//             pc += *pc; // Using relative jumps
//             break;
//         case N_JMP_IF:
//             pc++;
//             if (acc)
//                 pc += *pc;
//             acc = *sp--;
//             break;
//         case N_JMP_IF_NOT:
//             pc++;
//             if (!acc)
//                 pc += *pc;
//             acc = *sp--;
//             break;

//         // Commands
//         case N_POP:
//             sp++;
//             break;
//         case N_PRINT:
//             printf("PRINT: %lld\n", acc);
//             break;
//         case N_PRINT_STACK:
//             printf("PRINT_STACK:\n    ACC: %lld\n", acc);
//             {
//                 size_t i = 0;
//                 for (int64_t* sp2 = sp; sp2 <= fp; ++sp2)
//                 {
//                     printf("    [%d]: %lld\n", i, *sp2);
//                     ++i;
//                 }
//             }
//             break;
//         case N_HALT:
//             return acc;
//         default:
//             printf("Unrecognized command: %lld\n", *pc);
//             return acc;
//         }
//     }

//     // cleanup
//     free(sp);
//     necro_destroy_slab_allocator(&sa);
// }

// //=====================================================
// // Treadmill
// // Top >---(grey)---> Scan >---(black)---> Free >---(white)---> Bottom >---(ecru)---> Top {loop around}
// //=====================================================

// //---------------------
// // Helper functions:
// // Bit twiddle madness
// //---------------------

// static inline NecroVal* necro_get_start_of_page_data_from_val(NecroVal* val, uint64_t data_size_in_bytes)
// {
//     return (NecroVal*) (((uint64_t)val) & ~(data_size_in_bytes * NECRO_TM_PAGE_SIZE - 1));
// }

// static inline NecroTMPageHeader* necro_get_page_header_from_val(NecroVal* val, uint64_t data_size_in_bytes)
// {
//     return ((NecroTMPageHeader*)(necro_get_start_of_page_data_from_val(val, data_size_in_bytes))) - 1;
// }

// static inline NecroTMTag* necro_get_tag_from_val(NecroVal* val, uint64_t data_size_in_bytes)
// {
//     NecroTMPageHeader* page_header = necro_get_page_header_from_val(val, data_size_in_bytes);
//     uint64_t           index       = (val - ((NecroVal*)(page_header + 1))) / data_size_in_bytes;
//     return page_header->tags + index;
// }

// static inline NecroVal* necro_get_data_from_tag(NecroTMTag* tag, uint64_t segment)
// {
//     NecroTMPageHeader* page_header = (NecroTMPageHeader*)(((uint64_t)tag) & ~(sizeof(NecroTMTag*) * NECRO_TM_PAGE_SIZE - 1));
//     uint64_t           index       = tag - page_header->tags;
//     uint64_t           num_slots   = 2 << segment;
//     return (NecroVal*)(page_header + 1) + index * num_slots;
// }

// static inline bool necro_is_ecru_from_tag(NecroTMTag* tag)
// {
//     NecroTMPageHeader* page_header = (NecroTMPageHeader*)(((uint64_t)tag) & ~(sizeof(NecroTMTag*) * NECRO_TM_PAGE_SIZE - 1));
//     uint64_t           index       = tag - page_header->tags;
//     uint64_t           ecru_index  = index >> 6;
//     uint64_t           bit_index   = (uint64_t) 1 << (index & 63);
//     return page_header->ecru_flags[ecru_index] & bit_index;
// }

// static inline void necro_set_ecru_from_tag(NecroTMTag* tag, bool is_ecru)
// {
//     NecroTMPageHeader* page_header = (NecroTMPageHeader*)(((uint64_t)tag) & ~(sizeof(NecroTMTag*) * NECRO_TM_PAGE_SIZE - 1));
//     uint64_t           index       = tag - page_header->tags;
//     uint64_t           ecru_index  = index >> 6;
//     uint64_t           bit_index   = (uint64_t) 1 << (index & 63);
//     if (is_ecru)
//         page_header->ecru_flags[ecru_index] |= bit_index;
//     else
//         page_header->ecru_flags[ecru_index] &= ~bit_index;
// }

// static inline void necro_unsnap(NecroTMTag* tag)
// {
//     assert(tag != NULL);
//     NecroTMTag* prev = tag->prev;
//     NecroTMTag* next = tag->next;
//     assert(prev != NULL);
//     assert(next != NULL);
//     prev->next       = next;
//     next->prev       = prev;
//     tag->prev        = NULL;
//     tag->next        = NULL;
// }

// static inline void necro_snap_in(NecroTMTag* tag, NecroTMTag* new_prev, NecroTMTag* new_next)
// {
//     new_prev->next = tag;
//     tag->prev      = new_prev;
//     tag->next      = new_next;
//     new_next->prev = tag;
// }

// static inline void necro_relink(NecroTMTag* tag, NecroTMTag* new_prev, NecroTMTag* new_next)
// {
//     necro_unsnap(tag);
//     necro_snap_in(tag, new_prev, new_next);
// }

// //---------------
// // API functions

// // Segments go in powers of 2: 2, 4, 8, 16, 32, 64, etc
// NecroTreadmill necro_create_treadmill(size_t num_initial_pages, NecroTypeInfo* type_info_table)
// {
//     NecroTreadmill treadmill;
//     treadmill.pages      = NULL;
//     treadmill.complete   = true;
//     treadmill.type_infos = type_info_table;
//     for (size_t segment = 0; segment < NECRO_NUM_TM_SEGMENTS; ++segment)
//     {
//         size_t size      = 2 << segment;
//         size_t page_size = sizeof(NecroTMPageHeader) + size * sizeof(int64_t) * NECRO_TM_PAGE_SIZE;
//         char*  data      = malloc(num_initial_pages * page_size);
//         if (data == NULL)
//         {
//             fprintf(stderr, "Allocation error: could not allocate memory in necro_create_treadmill\n");
//             exit(1);
//         }
//         for (size_t page = 0; page < num_initial_pages; ++page)
//         {
//             NecroTMPageHeader* prev_page_header = ((NecroTMPageHeader*)(data)) + (page == 0 ? num_initial_pages - 1 : page - 1);
//             NecroTMPageHeader* page_header      = ((NecroTMPageHeader*)(data)) + page;
//             NecroTMPageHeader* next_page_header = ((NecroTMPageHeader*)(data)) + ((page + 1) % num_initial_pages);
//             for (size_t block = 0; block < NECRO_TM_PAGE_SIZE; ++block)
//             {
//                 // Set prev pointer
//                 page_header->tags[block].prev = (block == 0) ?
//                     prev_page_header->tags + (NECRO_TM_PAGE_SIZE - 1) :
//                     page_header->tags + (block - 1);
//                 // Set next pointer
//                 page_header->tags[block].next = (block + 1 >= NECRO_TM_PAGE_SIZE) ?
//                     next_page_header->tags :
//                     page_header->tags + (block + 1);
//                 // Set ecru flags to false
//                 memset(page_header->ecru_flags, 0, (NECRO_TM_PAGE_SIZE / 64) * sizeof(uint64_t));
//                 // Set up page list
//             }
//         }
//         ((NecroTMPageHeader*)data)->next_page = treadmill.pages;
//         treadmill.pages                       = ((NecroTMPageHeader*)data);
//         treadmill.free[segment]               = ((NecroTMPageHeader*)(data))->tags;
//         treadmill.scan[segment]               = treadmill.free[segment]->prev->prev;
//         treadmill.top[segment]                = treadmill.scan[segment];
//         treadmill.bottom[segment]             = treadmill.top[segment]->prev;
//         treadmill.allocated_blocks[segment]   = 0;
//     }
//     return treadmill;
// }

// NecroVal necro_treadmill_alloc(NecroTreadmill* treadmill, uint32_t type_index)
// {
//     uint32_t segment = treadmill->type_infos[type_index].gc_segment;
//     assert(segment <= 6); // Right now this only supports up to 64 slots
//     if (treadmill->free[segment]->next == treadmill->bottom[segment])
//     {
//         // Free list hit rock bottom, alloc another page, link it in, then set it to white
//         NecroTMPageHeader* page_header = malloc(sizeof(NecroTMPageHeader) + (2 << segment) * sizeof(int64_t) * NECRO_TM_PAGE_SIZE);
//         if (page_header == NULL)
//         {
//             fprintf(stderr, "Allocation error: could not allocate memory in necro_treadmill_alloc\n");
//             exit(1);
//         }
//         for (size_t block = 0; block < NECRO_TM_PAGE_SIZE; ++block)
//         {
//             TRACE_TM("    ____Allocating page for segment: %zu____\n", segment);
//             // Set prev pointer
//             page_header->tags[block].prev = (block == 0) ?
//                 treadmill->free[segment] :
//                 page_header->tags + (block - 1);
//             // Set next pointer
//             page_header->tags[block].next = (block + 1 >= NECRO_TM_PAGE_SIZE) ?
//                 treadmill->bottom[segment] :
//                 page_header->tags + (block + 1);
//             // Set ecru flags to false
//             memset(page_header->ecru_flags, 0, (NECRO_TM_PAGE_SIZE / 64) * sizeof(uint64_t));
//             // Add page to page list
//             page_header->next_page = treadmill->pages;
//             treadmill->pages       = page_header;
//         }
//     }
//     treadmill->allocated_blocks[segment]++;
//     NecroTMTag* allocated_tag = treadmill->free[segment];
//     NecroVal*   data          = necro_get_data_from_tag(allocated_tag, segment);
//     treadmill->free[segment]  = allocated_tag->next;
//     necro_set_ecru_from_tag(allocated_tag, 0);
//     NecroVal value;
//     value.necro_pointer = data;
//     return value;
// }

// static inline bool necro_treadmill_scan(NecroTreadmill* treadmill, uint64_t segment)
// {
//     if (treadmill->scan[segment] == treadmill->top[segment])
//         return true;
//     NecroTMTag* tag = treadmill->scan[segment];
//     if (!necro_is_ecru_from_tag(tag))
//     {
//         TRACE_TM("    ____Scanned Grey: %p____\n", tag);
//         // Move scan pointer backwards
//         treadmill->scan[segment] = treadmill->scan[segment]->prev;
//         return false;
//     }
//     TRACE_TM("    ____Scanned Ecru: %p____\n", tag);
//     NecroVal*     data      = necro_get_data_from_tag(tag, segment);
//     NecroTypeInfo type_info = treadmill->type_infos[data->struct_info.type_index];
//     necro_set_ecru_from_tag(tag, 0);
//     for (uint64_t i = 0; i < type_info.num_slots; ++i)
//     {
//         // if not a Boxed member, skip
//         if ((type_info.boxed_slot_bit_field & ((uint64_t)1 << i)) != ((uint64_t)1 << i))
//             continue;
//         NecroVal*     slot_value_ptr = data[i + 1].necro_pointer;
//         NecroTypeInfo slot_type_info = treadmill->type_infos[slot_value_ptr->struct_info.type_index];
//         NecroTMTag*   slot_tag       = necro_get_tag_from_val(slot_value_ptr, slot_type_info.segment_size_in_bytes);
//         if (!necro_is_ecru_from_tag(slot_tag))
//             continue;
//         necro_relink(slot_tag, treadmill->scan[slot_type_info.gc_segment]->prev, treadmill->scan[slot_type_info.gc_segment]); // Relink node behind the scan pointer for that type's segment
//     }
//     // Move scan pointer backwards
//     treadmill->scan[segment] = treadmill->scan[segment]->prev;
//     return false;
// }

// void necro_treadmill_collect(NecroTreadmill* treadmill, NecroVal root_ptr)
// {
//     TRACE_TM("\n");

//     if (treadmill->complete)
//     {
//         // Only link root in at beginning of a new collection cycle, not every re-entrance into it!
//         NecroVal*       root_val  = root_ptr.necro_pointer;
//         NecroTypeInfo   type_info = treadmill->type_infos[root_val->struct_info.type_index];
//         NecroTMTag*     tag       = necro_get_tag_from_val(root_val, type_info.segment_size_in_bytes);
//         // printf("gc_segment: %d, scan: %p, next: %p\n", type_info.gc_segment, treadmill->scan[type_info.gc_segment], treadmill->scan[type_info.gc_segment]->next);
//         necro_relink(tag, treadmill->scan[type_info.gc_segment], treadmill->scan[type_info.gc_segment]->next); // Move Root to grey list between top and scan pointers
//         return;
//         treadmill->scan[type_info.gc_segment] = tag;
//         treadmill->complete                   = false;
//         TRACE_TM("    ____Starting scan:%p____\n", tag);
//     }
//     else
//     {
//         TRACE_TM("    ____Continuing scan____\n");
//     }

//     // Continue scanning until all scan node of all segments points to same node as top pointer
//     size_t count = 0; // doing simple count bounding for now
//     while (!treadmill->complete && count < 256)
//     {
//         bool is_done = true;
//         for (size_t i = 0; i < NECRO_NUM_TM_SEGMENTS; ++i)
//         {
//             // scan segment
//             bool segment_done = necro_treadmill_scan(treadmill, i);
//             is_done = is_done && segment_done;
//         }
//         treadmill->complete = is_done;
//         count++;
//     }
//     if (treadmill->complete)
//     {
//         TRACE_TM("    ____Scan complete, sweeping____\n");
//         for (size_t segment = 0; segment < NECRO_NUM_TM_SEGMENTS; ++segment)
//         {
//             // Move scan pointer to right before free pointer
//             treadmill->scan[segment] = treadmill->free[segment]->prev;

//             // Set black list to ecru
//             NecroTMTag* current_tag = treadmill->scan[segment]->prev;
//             while (current_tag != treadmill->top[segment]->prev)
//             {
//                 necro_set_ecru_from_tag(current_tag, 1);
//                 current_tag = current_tag->prev;
//             }

//             // Set ecru list to white
//             current_tag = treadmill->top[segment];
//             while (current_tag != treadmill->bottom[segment]->prev)
//             {
//                 necro_set_ecru_from_tag(current_tag, 0);
//                 current_tag = current_tag->prev;
//                 treadmill->allocated_blocks[segment]--;
//             }

//             // Move bottom pointer to top pointer
//             treadmill->bottom[segment] = treadmill->top[segment];
//             necro_set_ecru_from_tag(treadmill->bottom[segment], 0);

//             // Move top pointer to right before scan pointer
//             treadmill->top[segment] = treadmill->scan[segment]->prev;
//             necro_set_ecru_from_tag(treadmill->top[segment], 0);
//         }
//     }
//     else
//     {
//         TRACE_TM("    ____Pausing scan____\n");
//     }
//     TRACE_TM("\n");
// }

// void necro_destroy_treadmill(NecroTreadmill* treadmill)
// {
//     NecroTMPageHeader* current_page = treadmill->pages;
//     while (current_page != NULL)
//     {
//         NecroTMPageHeader* next_page = current_page->next_page;
//         free(current_page);
//         current_page = next_page;
//     }
//     treadmill->pages = NULL;
//     for (size_t i = 0; i < NECRO_NUM_TM_SEGMENTS; ++i)
//     {
//         treadmill->top[i]              = NULL;
//         treadmill->bottom[i]           = NULL;
//         treadmill->free[i]             = NULL;
//         treadmill->scan[i]             = NULL;
//         treadmill->allocated_blocks[i] = 0;
//     }
// }

// void necro_print_treadmill(NecroTreadmill* treadmill)
// {
//     printf("NecroTreadmill\n{\n");
//     printf("    complete: %d,\n", treadmill->complete);
//     printf("    pages:    %p,\n", treadmill->pages);
//     printf("    top:      %p,\n", treadmill->top);
//     printf("    bottom:   %p,\n", treadmill->bottom);
//     printf("    free:     %p,\n", treadmill->free);
//     printf("    scan:     %p,\n", treadmill->scan);
//     printf("    allocated:[\n");
//     for (size_t segment = 0 ; segment < NECRO_NUM_TM_SEGMENTS; ++segment)
//         printf("        { segment: %zu, blocks: %zu },\n", segment, treadmill->allocated_blocks[segment]);
//     printf("    ]\n");
//     printf("}\n");
// }

// void necro_test_treadmill()
// {
//     puts("\n------");
//     puts("Test Treadmill\n");

//     // Setup type info table
//     NecroTypeInfo* type_info = malloc(2 * sizeof(NecroTypeInfo));
//     type_info[0] = (NecroTypeInfo) { 0xF, 16, 0, 2 }; // Type 1, 2 slots, 1st is struct_info, 2nd boxed
//     type_info[1] = (NecroTypeInfo) { 0,   16, 0, 2 }; // Type 2, 2 slots, 1st is struct_info, 2nd unboxed

//     // Setup treadmill
//     NecroTreadmill treadmill = necro_create_treadmill(1, type_info);
//     // necro_print_treadmill(&treadmill);
//     // puts("");

//     // Alloc1 test
//     NecroVal val = necro_treadmill_alloc(&treadmill, 1);
//     val.necro_pointer[0].struct_info     = (NecroStructInfo) {0, 1}; // val struct info
//     val.necro_pointer[1].int_value = 100;
//     print_test_result("Alloc1 test:  ", val.necro_pointer[1].int_value == 100);

//     // Alloc2 test
//     NecroVal val_2 = necro_treadmill_alloc(&treadmill, 1);
//     val_2.necro_pointer[0].struct_info = (NecroStructInfo) {0, 1}; // val struct info
//     val_2.necro_pointer[1].int_value   = 200;
//     print_test_result("Alloc2 test:  ", val_2.necro_pointer[1].int_value == val.necro_pointer[1].int_value * 2);
//     print_test_result("Count1 test:  ", treadmill.allocated_blocks[0] == 2);

//     // Collect1 test
//     val.necro_pointer[0].struct_info   = (NecroStructInfo) {0, 0}; // val struct info
//     val.necro_pointer[1].necro_pointer = val_2.necro_pointer;      // val points at val_2
//     val_2.necro_pointer[0].struct_info = (NecroStructInfo) {0, 1}; // val struct info
//     val_2.necro_pointer[1].int_value   = 666;      // val points at val_2
//     necro_treadmill_collect(&treadmill, val);
//     print_test_result("Collect1 test:", treadmill.allocated_blocks[0] == 2);

//     // Count2 test
//     necro_treadmill_alloc(&treadmill, 0);
//     necro_treadmill_alloc(&treadmill, 0);
//     necro_treadmill_alloc(&treadmill, 0);
//     necro_treadmill_alloc(&treadmill, 0);
//     print_test_result("Count2 test:  ", treadmill.allocated_blocks[0] == 6);

//     // Collect2 test
//     necro_treadmill_collect(&treadmill, val);
//     print_test_result("Collect2 test:", treadmill.allocated_blocks[0] == 2);

//     necro_print_treadmill(&treadmill);
//     puts("");

//     // Cleanup
//     necro_destroy_treadmill(&treadmill);
//     free(type_info);
// }

// void necro_test_vm_eval(int64_t* instr, int64_t result, const char* print_string)
// {
//     int64_t vm_result = necro_run_vm(instr, 0);
//     if (vm_result == result)
//         printf("%s passed\n", print_string);
//     else
//         printf("%s FAILED\n    expected result: %lld\n    vm result:       %lld\n\n", print_string, result, vm_result);
// #if DEBUG_VM
//     puts("");
// #endif
// }

// void necro_test_vm()
// {
//     puts("\n------");
//     puts("Test VM\n");

//     // Integer tests
//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 7,
//             N_PUSH_I, 6,
//             N_ADD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 13, "AddI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 4,
//             N_PUSH_I, 9,
//             N_SUB_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 5, "SubI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 10,
//             N_PUSH_I, 3,
//             N_MUL_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 30, "MulI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,     10,
//             N_NEG_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, -10, "NegI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 10,
//             N_PUSH_I, 1000,
//             N_DIV_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 100, "DivI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 4,
//             N_PUSH_I, 5,
//             N_MOD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 1, "ModI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 666,
//             N_PUSH_I, 555,
//             N_EQ_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 0, "EqI:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 666,
//             N_PUSH_I, 555,
//             N_NEQ_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 1, "NEqI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 1,
//             N_PUSH_I, 2,
//             N_LT_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 0, "LTI:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 3,
//             N_PUSH_I, 3,
//             N_LTE_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 1, "LTEI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 100,
//             N_PUSH_I, 200,
//             N_GT_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 1, "GTI:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 100,
//             N_PUSH_I, 99,
//             N_GTE_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 0, "GTEI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 7,
//             N_PUSH_I, 6,
//             N_BIT_AND_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 6 & 7, "BAND:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 13,
//             N_PUSH_I, 4,
//             N_BIT_OR_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 4 | 13, "BOR:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 22,
//             N_PUSH_I, 11,
//             N_BIT_XOR_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 11 ^ 22, "BXOR:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 2,
//             N_PUSH_I, 4,
//             N_BIT_LS_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 4 << 2, "BLS:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 1,
//             N_PUSH_I, 5,
//             N_BIT_RS_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 5 >> 1, "BRS:    ");
//     }

//     // Jump tests
//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 6,
//             N_JMP, 2,
//             N_PUSH_I, 7,
//             N_PUSH_I, 8,
//             N_ADD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 14, "Jmp:    ");
//     }


//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 3,
//             N_PUSH_I, 1,
//             N_JMP_IF, 4,
//             N_PUSH_I, 4,
//             N_JMP,    2,
//             N_PUSH_I, 5,
//             N_ADD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 8, "JmpIf 1:");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 3,
//             N_PUSH_I, 0,
//             N_JMP_IF, 4,
//             N_PUSH_I, 4,
//             N_JMP,    2,
//             N_PUSH_I, 5,
//             N_ADD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 7, "JmpIf 2:");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,     10,
//             N_PUSH_I,     1,
//             N_JMP_IF_NOT, 4,
//             N_PUSH_I,     20,
//             N_JMP,        2,
//             N_PUSH_I,     50,
//             N_SUB_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 10, "JmpIfN1:");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,     10,
//             N_PUSH_I,     0,
//             N_JMP_IF_NOT, 4,
//             N_PUSH_I,     40,
//             N_JMP,        2,
//             N_PUSH_I,     50,
//             N_SUB_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 40, "JmpIfN2:");
//     }

//     // Memory
//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 10,
//             N_PUSH_I, 5,
//             N_MUL_I,
//             N_LOAD_L, 0,
//             N_LOAD_L, 0,
//             N_ADD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 100, "Load1:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 10,
//             N_PUSH_I, 5,
//             N_MUL_I,
//             N_PUSH_I, 20,
//             N_PUSH_I, 20,
//             N_ADD_I,
//             N_LOAD_L, 0,
//             N_LOAD_L, -1,
//             N_SUB_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, -10, "Load2:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,  0,
//             N_PUSH_I,  0,
//             N_PUSH_I,  60,
//             N_STORE_L, 0,
//             N_PUSH_I,  5,
//             N_PUSH_I,  8,
//             N_MUL_I,
//             N_STORE_L, -1,
//             N_LOAD_L,  0,
//             N_LOAD_L,  -1,
//             N_SUB_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, -20, "Store1: ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I, 10,
//             N_PUSH_I, 20,
//             N_PUSH_I, (int64_t)(instr + 8),
//             N_CALL,   2,
//             N_LOAD_L, 1,
//             N_LOAD_L, 0,
//             N_ADD_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 30, "CALL1:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             // Push 10, then 20, then call function
//             N_PUSH_I, 10,
//             N_PUSH_I, 20,
//             N_PUSH_I, (int64_t)(instr + 18),
//             N_CALL,   2,

//             // With the result of the function on the stack, push 5, then call function again
//             N_PUSH_I, 5,
//             N_PUSH_I, (int64_t)(instr + 18),
//             N_CALL,   2,

//             // With the result of the function on the stack, push 10 then add the results, then halt
//             N_PUSH_I, 10,
//             N_ADD_I,
//             N_HALT,

//             // Function that will be called multiple times
//             N_LOAD_L, 1,
//             N_LOAD_L, 0,
//             N_SUB_I,
//             N_RETURN
//         };
//         necro_test_vm_eval(instr, 5, "CALL2:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             // Push 5 and 6 to stack, then call function 1
//             N_PUSH_I, 5,
//             N_PUSH_I, 6,
//             N_PUSH_I, (int64_t)(instr + 9), // function 1 addr
//             N_CALL,   2, // call function1 with 2 arguments

//             // End
//             N_HALT,

//             // Function 1
//             N_LOAD_L, 1,
//             N_LOAD_L, 0,
//             N_ADD_I,
//             N_PUSH_I, (int64_t)(instr + 19), // function 2 addr
//             N_CALL,   1, // Call function2 with 1 argument
//             N_RETURN,

//             // Function 2
//             N_LOAD_L, 0,
//             N_LOAD_L, 0,
//             N_MUL_I,
//             N_RETURN
//         };
//         necro_test_vm_eval(instr, 121, "CALL3:  ");
//     }

//     {
//         // (\x -> x2 + y where x2 = x + x; y = x) 10
//         int64_t instr[] =
//         {
//             N_PUSH_I, 10,
//             N_PUSH_I, (int64_t)(instr + 7), // addr of function
//             N_CALL,   1,
//             N_HALT,
//             N_LOAD_L, 0,
//             N_LOAD_L, 0,
//             N_ADD_I,
//             N_LOAD_L, 0,
//             N_ADD_I,
//             N_RETURN
//         };
//         necro_test_vm_eval(instr, 30, "CALL4:  ");
//     }

//     // Look into fuzzing technique?
//     {
//         // (\x -> x / y where x2 = x * x, y = 4 - x) 5
//         int64_t instr[] =
//         {
//             N_PUSH_I, 5,
//             N_PUSH_I, (int64_t)(instr + 7), // addr of function
//             N_CALL,   1,

//             N_HALT,

//             // func-------
//             // x2
//             N_LOAD_L, 0,
//             N_LOAD_L, 0,
//             N_MUL_I,

//             // y
//             N_LOAD_L, 0,
//             N_PUSH_I, 4,
//             N_SUB_I,

//             // x / y
//             N_LOAD_L,  -7,
//             N_LOAD_L,  -6,
//             N_DIV_I,
//             N_RETURN
//         };
//         necro_test_vm_eval(instr, -25, "CALL5:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,      18,
//             N_PUSH_I,      12,
//             N_PUSH_I,      6,
//             N_MAKE_STRUCT, 3,
//             N_GET_FIELD,   2,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 18, "struct1:");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,      18,
//             N_PUSH_I,      12,
//             N_PUSH_I,      6,
//             N_MAKE_STRUCT, 3,
//             N_LOAD_L,      0,
//             N_GET_FIELD,   0,
//             N_LOAD_L,      0,
//             N_GET_FIELD,   1,
//             N_LOAD_L,      0,
//             N_GET_FIELD,   2,
//             N_LOAD_L,      -1,
//             N_LOAD_L,      -2,
//             N_ADD_I,
//             N_LOAD_L,      -3,
//             N_SUB_I,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 0, "struct2:");
//     }

//     {
//         int64_t instr[] =
//         {
//             N_PUSH_I,      6,
//             N_PUSH_I,      12,
//             N_PUSH_I,      18,
//             N_MAKE_STRUCT, 3,
//             N_PUSH_I,      666,
//             N_LOAD_L,      0,
//             N_SET_FIELD,   2,
//             N_LOAD_L,      0,
//             N_GET_FIELD,   2,
//             N_HALT
//         };
//         necro_test_vm_eval(instr, 666, "struct3:");
//     }

// }

// //=====================================================
// // Demand Vitural Machine (DVM)
// //=====================================================
// void necro_trace_stack_dvm(int64_t opcode)
// {
//     switch (opcode)
//     {
//     case DVM_DEMAND_I:    puts("DEMAND_I");    return;
//     case DVM_DEMAND_F:    puts("DEMAND_F");    return;
//     case DVM_DEMAND_A:    puts("DEMAND_A");    return;
//     case DVM_DEMAND_S:    puts("DEMAND_S");    return;
//     case DVM_STORE_I:     puts("STORE_I");     return;
//     case DVM_STORE_F:     puts("STORE_F");     return;
//     case DVM_STORE_A:     puts("STORE_A");     return;
//     case DVM_STORE_S:     puts("STORE_S");     return;
//     case DVM_PUSH_I:      puts("PUSH_I");      return;
//     case DVM_ADD_I:       puts("ADD_I");       return;
//     case DVM_SUB_I:       puts("SUB_I");       return;
//     case DVM_MUL_I:       puts("MUL_I");       return;
//     case DVM_NEG_I:       puts("NEG_I");       return;
//     case DVM_DIV_I:       puts("DIV_I");       return;
//     case DVM_MOD_I:       puts("MOD_I");       return;
//     case DVM_EQ_I:        puts("EQ_I");        return;
//     case DVM_NEQ_I:       puts("NEQ_I");       return;
//     case DVM_LT_I:        puts("LT_I");        return;
//     case DVM_LTE_I:       puts("LTE_I");       return;
//     case DVM_GT_I:        puts("GT_I");        return;
//     case DVM_GTE_I:       puts("GTE_I");       return;
//     case DVM_BIT_AND_I:   puts("BIT_AND_I");   return;
//     case DVM_BIT_OR_I:    puts("BIT_OR_I");    return;
//     case DVM_BIT_XOR_I:   puts("BIT_XOR_I");   return;
//     case DVM_BIT_LS_I:    puts("BIT_LS_I");    return;
//     case DVM_BIT_RS_I:    puts("BIT_RS_I");    return;
//     case DVM_CALL:        puts("CALL");        return;
//     case DVM_RETURN:      puts("RETURN");      return;
//     case DVM_LOAD_L:      puts("LOAD_L");      return;
//     case DVM_STORE_L:     puts("STORE_L");     return;
//     case DVM_JMP:         puts("JMP");         return;
//     case DVM_JMP_IF:      puts("JMP_IF");      return;
//     case DVM_JMP_IF_NOT:  puts("JMP_IF_NOT");  return;
//     case DVM_POP:         puts("POP");         return;
//     case DVM_PRINT:       puts("PRINT");       return;
//     case DVM_PRINT_STACK: puts("PRINT_STACK"); return;
//     case DVM_HALT:        puts("HALT");        return;
//     case DVM_MAKE_STRUCT: puts("MAKE_STRUCT"); return;
//     case DVM_GET_FIELD:   puts("GET_FIELD");   return;
//     case DVM_SET_FIELD:   puts("SET_FIELD");   return;
//     default:              puts("UKNOWN");      return;
//     }
// }

// uint64_t necro_run_dvm(uint64_t* instructions)
// {
//     NecroSlabAllocator sa    = necro_create_slab_allocator(32);
//     NecroVault         vault = necro_create_vault(&sa);
//     int32_t            epoch = 0;
//     register int64_t*  sp    = malloc(NECRO_STACK_SIZE * sizeof(int64_t)); // Stack grows down
//     register int64_t*  pc    = instructions;
//     register int64_t*  fp    = 0;
//     register int64_t   acc   = 0;
//     register int64_t   env   = 0;
//     sp = sp + (NECRO_STACK_SIZE - 1);
//     fp = sp - 2;
//     pc--;
//     while (true)
//     {
//         int64_t opcode = *++pc;
//         TRACE_STACK(opcode);
//         switch (opcode)
//         {

//         //====================================
//         // Int Ops
//         //====================================
//         case DVM_PUSH_I:
//             *--sp = acc;
//             acc = *++pc;
//             break;
//         case DVM_NEG_I:
//             acc = -acc;
//             break;

//         //====================================
//         // Int BinOps
//         //------------------------------------
//         // INPUT
//         // ACC:   Int Operand A
//         // sp[0]: Int Operand B
//         //------------------------------------
//         // OUTPUT
//         // ACC:   Int Result
//         //====================================
//         case DVM_ADD_I:
//             acc = acc + *sp++;
//             break;
//         case DVM_SUB_I:
//             acc = acc - *sp++;
//             break;
//         case DVM_MUL_I:
//             acc = acc * *sp++;
//             break;
//         case DVM_DIV_I:
//             acc = acc / *sp++;
//             break;
//         case DVM_MOD_I:
//             acc = acc % *sp++;
//             break;
//         case DVM_EQ_I:
//             acc = acc == *sp++;
//             break;
//         case DVM_NEQ_I:
//             acc = acc != *sp++;
//             break;
//         case DVM_LT_I:
//             acc = acc < *sp++;
//             break;
//         case DVM_LTE_I:
//             acc = acc <= *sp++;
//             break;
//         case DVM_GT_I:
//             acc = acc > *sp++;
//             break;
//         case DVM_GTE_I:
//             acc = acc >= *sp++;
//             break;
//         case DVM_BIT_AND_I:
//             acc = acc & *sp++;
//             break;
//         case DVM_BIT_OR_I:
//             acc = acc | *sp++;
//             break;
//         case DVM_BIT_XOR_I:
//             acc = acc ^ *sp++;
//             break;
//         case DVM_BIT_LS_I:
//             acc = acc << *sp++;
//             break;
//         case DVM_BIT_RS_I:
//             acc = acc >> *sp++;
//             break;

//         // Functions
//         // Stack Frame:
//         //  - args
//         //  <---------- fp points at last arguments
//         //              LOAD_L loads in relation to fp
//         //               0... N (incrementing) loads from the final argument down towards the first argument
//         //              -6...-N (decrementing) loads from the first local variable up towarods final local variable
//         //  - saved registers (pc, fp, env, argc)
//         //  - Locals
//         //  - operand stack
//         //  When calling into a subroutine, the operand
//         //  stack of the current frame becomes the args of the next
//         case DVM_CALL: // one operand: number of args to reserve in current operand stack
//             sp   -= 4;
//             sp[0] = (int64_t) (pc + 1); // pc gets incremented at top, so set pc to one less than call site
//             sp[1] = (int64_t) fp;
//             sp[2] = env;
//             sp[3] = *++pc;
//             fp    = sp + 4;
//             pc    = (int64_t*) acc - 1; // pc gets incremented at top, so set pc to one less than call site
//             env   = acc + 1;
//             break;

//         case DVM_RETURN:
//             sp   = fp - 4;
//             pc   = (int64_t*)sp[0];
//             fp   = (int64_t*)sp[1];
//             env  = sp[2];
//             sp  += sp[3] + 4;
//             break;

//         //====================================
//         // DEMAND_I
//         // Retrieve Int value from the vault at the given key
//         // and push to stack if present.
//         // If not present jump to addr, compute it,
//         // store it in the vault. then push to stack
//         //------------------------------------
//         // INPUT
//         // ACC:   Demand Code addr
//         // sp[0]: Time
//         // sp[1]: Place
//         //-------------------------------------
//         // OUTPUT 1 (Not Found)
//         // ACC:   Heap addr to store value into
//         // sp[0]: Return address to return to once value has been computed and stored
//         // PC:    Demand Code addr
//         //-------------------------------------
//         // OUTPUT 2 (Found, Or Computed/Returned)
//         // ACC:   Int value retrieved from vault or computed and returned
//         //====================================
//         case DVM_DEMAND_I:
//         {
//             NecroFindOrAllocResult result = necro_vault_find_or_alloc(&vault, (NecroVaultKey) { ((uint32_t*)sp)[1], ((uint32_t*)sp)[0] }, epoch, 8);
//             if (result.find_or_alloc_type == NECRO_FOUND)
//             {
//                 sp += 2;                        // Pop Args
//                 acc = *((int64_t*)result.data); // Retrieve value from memory put into acc
//             }
//             else
//             {
//                 sp += 1;                            // Pop Args
//                 *sp = (int64_t)(pc - instructions); // Top of stack is return address, pc gets incremented at top, so set pc to one less than call site
//                 pc  = instructions + (acc - 1);     // pc gets incremented at top, so set pc to one less than call site
//                 acc = (int64_t)result.data;         // set acc to address to store value into
//             }
//             break;
//         }

//         //====================================
//         // STORE_I
//         // Store an int value that has been demanded,
//         // then return to point where it was demanded
//         // with the value on the stack
//         //------------------------------------
//         // INPUT
//         // acc:   Int Value which was demanded
//         // sp[0]: Heap addr to store value into
//         // sp[1]: Return addr
//         //------------------------------------
//         // OUTPUT
//         // acc:   Int Value which was demanded
//         //====================================
//         case DVM_STORE_I:
//             *((int64_t*)(sp[0])) = acc;                  // Retrieve vault address from top of stack then store the int value (in acc) into it.
//             pc                   = instructions + sp[1]; // Set PC to return address
//             sp                  += 2;                    // Pop Args
//             break;


//         //     // structs
//         //     case N_MAKE_STRUCT:
//         //     {
//         //         *--sp = acc;
//         //         int64_t  num_fields = *++pc;
//         //         size_t   size       = (1 + (size_t)num_fields) * sizeof(int64_t); // Will need 1 extra for refcount field
//         //         int64_t* struct_ptr = necro_alloc_slab(&sa, size);
//         //         struct_ptr[0]       = 0; // Set ref count to 0
//         //         memcpy(struct_ptr + 1, sp, size);
//         //         acc = (int64_t) struct_ptr;
//         //         sp += num_fields;
//         //         break;
//         //     }

//         case DVM_GET_FIELD: // acc = struct_ptr, next pc = field number, replaces acc with result
//             acc = ((int64_t*)acc)[*++pc + 1];
//             break;

//         case DVM_SET_FIELD: // acc = struct_ptr, next pc = field number, sp[0] = value to set to, pops struct from acc
//             ((int64_t*)acc)[*++pc + 1] = *sp++;
//             acc = *sp++;
//             break;

//         // Memory
//         case DVM_LOAD_L: // Loads a local variable relative to current frame pointer
//             *--sp = acc;
//             acc   = *(fp + *++pc);
//             break;
//         case DVM_STORE_L: // Stores a local variables relative to current frame pointer
//             *(fp + *++pc) = acc;
//             acc = *sp++;
//             break;

//         //====================================
//         // JMP
//         // Unconditional relative jump to a
//         // different point in instructions
//         //------------------------------------
//         // INPUT
//         // PC+1:  Relative offset
//         //------------------------------------
//         // OUTPUT
//         // PC -> Moved by Relative Offset
//         //====================================
//         case DVM_JMP:
//             pc++;
//             pc += *pc; // Using relative jumps
//             break;
//         //====================================
//         // JMP_IF
//         // Conditional relative jump to a
//         // different point in instructions
//         // Tests for ACC
//         //------------------------------------
//         // INPUT
//         // PC+1:  Relative offset
//         // ACC:   Boolean to test
//         //------------------------------------
//         // OUTPUT (True)
//         // PC -> Moved by Relative offset
//         //====================================
//         case DVM_JMP_IF:
//             pc++;
//             if (acc)
//                 pc += *pc;
//             acc = *sp--;
//             break;
//         //====================================
//         // JMP_IF_NOT
//         // Conditional relative jump to a
//         // different point in instructions
//         // Tests for !ACC
//         //------------------------------------
//         // INPUT
//         // PC+1:  Relative offset
//         // ACC:   Boolean to test
//         //------------------------------------
//         // OUTPUT (False)
//         // PC -> Moved by Relative offset
//         //====================================
//         case DVM_JMP_IF_NOT:
//             pc++;
//             if (!acc)
//                 pc += *pc;
//             acc = *sp--;
//             break;

//         //====================================
//         // C calls
//         // Calls to foreign C functions
//         //------------------------------------
//         // INPUT
//         // ACC:   Arg0
//         // SP[N]: Arg(1+(0..N))
//         //------------------------------------
//         // OUTPUT
//         // ACC:   Result of C call
//         //====================================
//         case DVM_C_CALL_1:
//             acc = ((necro_c_call_1)(*++pc))((NecroVal) { acc }).int_value;
//             break;
//         case DVM_C_CALL_2:
//             acc = ((necro_c_call_2)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }).int_value;
//             sp++;
//             break;
//         case DVM_C_CALL_3:
//             acc = ((necro_c_call_3)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }).int_value;
//             sp += 2;
//             break;
//         case DVM_C_CALL_4:
//             acc = ((necro_c_call_4)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }, (NecroVal) { sp[2] }).int_value;
//             sp += 3;
//             break;
//         case DVM_C_CALL_5:
//             acc = ((necro_c_call_5)(*++pc))((NecroVal) { acc }, (NecroVal) { sp[0] }, (NecroVal) { sp[1] }, (NecroVal) { sp[2] }, (NecroVal) { sp[3] }).int_value;
//             sp += 4;
//             break;

//         // Commands
//         case DVM_POP:
//             sp++;
//             break;
//         case DVM_PRINT:
//             printf("PRINT: %lld\n", acc);
//             break;
//         case DVM_PRINT_STACK:
//             printf("PRINT_STACK:\n    ACC: %lld\n", acc);
//             {
//                 size_t i = 0;
//                 for (int64_t* sp2 = sp; sp2 <= fp; ++sp2)
//                 {
//                     printf("    [%d]: %lld\n", i, *sp2);
//                     ++i;
//                 }
//             }
//             break;
//         case DVM_HALT:
//             return acc;
//         default:
//             printf("Unrecognized command: %lld\n", *pc);
//             return acc;
//         }
//     }

//     // cleanup
//     free(sp);
//     necro_destroy_slab_allocator(&sa);
//     necro_destroy_vault(&vault);
// }

// void necro_test_dvm_eval(int64_t* instr, int64_t result, const char* print_string)
// {
//     int64_t vm_result = necro_run_dvm(instr);
//     if (vm_result == result)
//         printf("%s passed\n", print_string);
//     else
//         printf("%s FAILED\n    expected result: %lld\n    vm result:       %lld\n\n", print_string, result, vm_result);
// #if DEBUG_VM
//     puts("");
// #endif
// }

// void necro_test_dvm()
// {
//     necro_announce_phase("DVM");

//     // Integer tests
//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 7,
//             DVM_PUSH_I, 6,
//             DVM_ADD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 13, "AddI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 4,
//             DVM_PUSH_I, 9,
//             DVM_SUB_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 5, "SubI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, 3,
//             DVM_MUL_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 30, "MulI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I,     10,
//             DVM_NEG_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, -10, "NegI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, 1000,
//             DVM_DIV_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 100, "DivI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 4,
//             DVM_PUSH_I, 5,
//             DVM_MOD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 1, "ModI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 666,
//             DVM_PUSH_I, 555,
//             DVM_EQ_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 0, "EqI:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 666,
//             DVM_PUSH_I, 555,
//             DVM_NEQ_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 1, "NEqI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 1,
//             DVM_PUSH_I, 2,
//             DVM_LT_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 0, "LTI:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 3,
//             DVM_PUSH_I, 3,
//             DVM_LTE_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 1, "LTEI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 100,
//             DVM_PUSH_I, 200,
//             DVM_GT_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 1, "GTI:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 100,
//             DVM_PUSH_I, 99,
//             DVM_GTE_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 0, "GTEI:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 7,
//             DVM_PUSH_I, 6,
//             DVM_BIT_AND_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 6 & 7, "BAND:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 13,
//             DVM_PUSH_I, 4,
//             DVM_BIT_OR_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 4 | 13, "BOR:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 22,
//             DVM_PUSH_I, 11,
//             DVM_BIT_XOR_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 11 ^ 22, "BXOR:   ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 2,
//             DVM_PUSH_I, 4,
//             DVM_BIT_LS_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 4 << 2, "BLS:    ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 1,
//             DVM_PUSH_I, 5,
//             DVM_BIT_RS_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 5 >> 1, "BRS:    ");
//     }

//     // Jump tests
//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 6,
//             DVM_JMP, 2,
//             DVM_PUSH_I, 7,
//             DVM_PUSH_I, 8,
//             DVM_ADD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 14, "Jmp:    ");
//     }


//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 3,
//             DVM_PUSH_I, 1,
//             DVM_JMP_IF, 4,
//             DVM_PUSH_I, 4,
//             DVM_JMP,    2,
//             DVM_PUSH_I, 5,
//             DVM_ADD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 8, "JmpIf 1:");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 3,
//             DVM_PUSH_I, 0,
//             DVM_JMP_IF, 4,
//             DVM_PUSH_I, 4,
//             DVM_JMP,    2,
//             DVM_PUSH_I, 5,
//             DVM_ADD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 7, "JmpIf 2:");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I,     10,
//             DVM_PUSH_I,     1,
//             DVM_JMP_IF_NOT, 4,
//             DVM_PUSH_I,     20,
//             DVM_JMP,        2,
//             DVM_PUSH_I,     50,
//             DVM_SUB_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 10, "JmpIfN1:");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I,     10,
//             DVM_PUSH_I,     0,
//             DVM_JMP_IF_NOT, 4,
//             DVM_PUSH_I,     40,
//             DVM_JMP,        2,
//             DVM_PUSH_I,     50,
//             DVM_SUB_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 40, "JmpIfN2:");
//     }

//     // Memory
//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, 5,
//             DVM_MUL_I,
//             DVM_LOAD_L, 0,
//             DVM_LOAD_L, 0,
//             DVM_ADD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 100, "Load1:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, 5,
//             DVM_MUL_I,
//             DVM_PUSH_I, 20,
//             DVM_PUSH_I, 20,
//             DVM_ADD_I,
//             DVM_LOAD_L, 0,
//             DVM_LOAD_L, -1,
//             DVM_SUB_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, -10, "Load2:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I,  0,
//             DVM_PUSH_I,  0,
//             DVM_PUSH_I,  60,
//             DVM_STORE_L, 0,
//             DVM_PUSH_I,  5,
//             DVM_PUSH_I,  8,
//             DVM_MUL_I,
//             DVM_STORE_L, -1,
//             DVM_LOAD_L,  0,
//             DVM_LOAD_L,  -1,
//             DVM_SUB_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, -20, "Store1: ");
//     }

//     {
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, 20,
//             DVM_PUSH_I, (int64_t)(instr + 8),
//             DVM_CALL,   2,
//             DVM_LOAD_L, 1,
//             DVM_LOAD_L, 0,
//             DVM_ADD_I,
//             DVM_HALT
//         };
//         necro_test_dvm_eval(instr, 30, "CALL1:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             // Push 10, then 20, then call function
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, 20,
//             DVM_PUSH_I, (int64_t)(instr + 18),
//             DVM_CALL,   2,

//             // With the result of the function on the stack, push 5, then call function again
//             DVM_PUSH_I, 5,
//             DVM_PUSH_I, (int64_t)(instr + 18),
//             DVM_CALL,   2,

//             // With the result of the function on the stack, push 10 then add the results, then halt
//             DVM_PUSH_I, 10,
//             DVM_ADD_I,
//             DVM_HALT,

//             // Function that will be called multiple times
//             DVM_LOAD_L, 1,
//             DVM_LOAD_L, 0,
//             DVM_SUB_I,
//             DVM_RETURN
//         };
//         necro_test_dvm_eval(instr, 5, "CALL2:  ");
//     }

//     {
//         int64_t instr[] =
//         {
//             // Push 5 and 6 to stack, then call function 1
//             DVM_PUSH_I, 5,
//             DVM_PUSH_I, 6,
//             DVM_PUSH_I, (int64_t)(instr + 9), // function 1 addr
//             DVM_CALL,   2, // call function1 with 2 arguments

//             // End
//             DVM_HALT,

//             // Function 1
//             DVM_LOAD_L, 1,
//             DVM_LOAD_L, 0,
//             DVM_ADD_I,
//             DVM_PUSH_I, (int64_t)(instr + 19), // function 2 addr
//             DVM_CALL,   1, // Call function2 with 1 argument
//             DVM_RETURN,

//             // Function 2
//             DVM_LOAD_L, 0,
//             DVM_LOAD_L, 0,
//             DVM_MUL_I,
//             DVM_RETURN
//         };
//         necro_test_dvm_eval(instr, 121, "CALL3:  ");
//     }

//     {
//         // (\x -> x2 + y where x2 = x + x; y = x) 10
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 10,
//             DVM_PUSH_I, (int64_t)(instr + 7), // addr of function
//             DVM_CALL,   1,
//             DVM_HALT,
//             DVM_LOAD_L, 0,
//             DVM_LOAD_L, 0,
//             DVM_ADD_I,
//             DVM_LOAD_L, 0,
//             DVM_ADD_I,
//             DVM_RETURN
//         };
//         necro_test_dvm_eval(instr, 30, "CALL4:  ");
//     }

//     // Look into fuzzing technique?
//     {
//         // (\x -> x / y where x2 = x * x, y = 4 - x) 5
//         int64_t instr[] =
//         {
//             DVM_PUSH_I, 5,
//             DVM_PUSH_I, (int64_t)(instr + 7), // addr of function
//             DVM_CALL,   1,

//             DVM_HALT,

//             // func-------
//             // x2
//             DVM_LOAD_L, 0,
//             DVM_LOAD_L, 0,
//             DVM_MUL_I,

//             // y
//             DVM_LOAD_L, 0,
//             DVM_PUSH_I, 4,
//             DVM_SUB_I,

//             // x / y
//             DVM_LOAD_L,  -7,
//             DVM_LOAD_L,  -6,
//             DVM_DIV_I,
//             DVM_RETURN
//         };
//         necro_test_dvm_eval(instr, -25, "CALL5:  ");
//     }

//     // {
//     //     int64_t instr[] =
//     //     {
//     //         DVM_PUSH_I,      18,
//     //         DVM_PUSH_I,      12,
//     //         DVM_PUSH_I,      6,
//     //         DVM_MAKE_STRUCT, 3,
//     //         DVM_GET_FIELD,   2,
//     //         DVM_HALT
//     //     };
//     //     necro_test_dvm_eval(instr, 18, "struct1:");
//     // }

//     // {
//     //     int64_t instr[] =
//     //     {
//     //         DVM_PUSH_I,      18,
//     //         DVM_PUSH_I,      12,
//     //         DVM_PUSH_I,      6,
//     //         DVM_MAKE_STRUCT, 3,
//     //         DVM_LOAD_L,      0,
//     //         DVM_GET_FIELD,   0,
//     //         DVM_LOAD_L,      0,
//     //         DVM_GET_FIELD,   1,
//     //         DVM_LOAD_L,      0,
//     //         DVM_GET_FIELD,   2,
//     //         DVM_LOAD_L,      -1,
//     //         DVM_LOAD_L,      -2,
//     //         DVM_ADD_I,
//     //         DVM_LOAD_L,      -3,
//     //         DVM_SUB_I,
//     //         DVM_HALT
//     //     };
//     //     necro_test_dvm_eval(instr, 0, "struct2:");
//     // }

//     // {
//     //     int64_t instr[] =
//     //     {
//     //         DVM_PUSH_I,      6,
//     //         DVM_PUSH_I,      12,
//     //         DVM_PUSH_I,      18,
//     //         DVM_MAKE_STRUCT, 3,
//     //         DVM_PUSH_I,      666,
//     //         DVM_LOAD_L,      0,
//     //         DVM_SET_FIELD,   2,
//     //         DVM_LOAD_L,      0,
//     //         DVM_GET_FIELD,   2,
//     //         DVM_HALT
//     //     };
//     //     necro_test_dvm_eval(instr, 666, "struct3:");
//     // }

//     // TODO: Place == instruction code addr? Shouldn't that be unique and kill two birds with one stone? Multiple instantiations?!?!?! How?!?!?!
//     {
//         int64_t instr[] =
//         {
//             // Main
//             DVM_PUSH_I,   4,  // Operand A
//             DVM_PUSH_I,   1,  // Key - Place
//             DVM_PUSH_I,   0,  // Key - Time
//             DVM_PUSH_I,   11, // Demand code addr
//             DVM_DEMAND_I,     // Demand Int value at Key {1, 0, 0}, if not there evaluate code at
//             DVM_ADD_I,        // Add Operands A and B together

//             // End
//             DVM_HALT,

//             // Demand code
//             DVM_PUSH_I,   5,  // Push 5
//             DVM_STORE_I,      // Store it at the supplied address then return with 5 still on the stack, serving as Operand B
//         };
//         necro_test_dvm_eval(instr, 9, "Demand1:");
//     }

//     {
//         int64_t instr[] =
//         {
//             // Main
//             DVM_PUSH_I,   1,  // Key - Place
//             DVM_PUSH_I,   0,  // Key - Time
//             DVM_PUSH_I,   16, // Demand code addr
//             DVM_DEMAND_I,     // Demand Int value at Key {1, 0, 0}, if not there evaluate code at

//             DVM_PUSH_I,   1,  // Key - Place
//             DVM_PUSH_I,   0,  // Key - Time
//             DVM_PUSH_I,   16, // Demand code addr
//             DVM_DEMAND_I,     // Demand Int value at Key {1, 0, 0}, if not there evaluate code at

//             // Work Code
//             DVM_ADD_I,        // Add Operands A and B together

//             // End
//             DVM_HALT,

//             // Demand code
//             DVM_PUSH_I,   5,  // Push 5
//             DVM_PUSH_I,   3,  // Push 3
//             DVM_ADD_I,
//             DVM_STORE_I,      // Store it at the supplied address then return with 5 still on the stack
//         };
//         necro_test_dvm_eval(instr, 16, "Demand2:");
//     }
//     puts("");
// }
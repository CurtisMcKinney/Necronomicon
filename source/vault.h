/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef VAULT_H
#define VAULT_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utility.h"
#include "slab.h"

//=====================================================
// NecroVault: Linear Probing Hashtable with Lazy Incremental GC and Resizing
// Key derived from:
//     * Place, Universe, Time
// Values can either be:
//     * Int, Float, Pointer (Each 64-bits long
// Place 0 is considered NULL
//=====================================================

typedef struct
{
    uint32_t place;
    uint32_t universe;
    uint32_t time;
} NecroVaultKey;

typedef struct NecroVaultEntry
{
    union
    {
        // void*   pointer_data;
        int64_t int_data;
        double  float_data;
    };
    size_t        hash;
    NecroVaultKey key;
    int32_t       last_epoch;
    int32_t       retirement_age;
} NecroVaultEntry;

typedef struct
{
    NecroVaultEntry* prev_entries;
    size_t           prev_size;
    size_t           prev_count;
    NecroVaultEntry* curr_entries;
    size_t           curr_size;
    size_t           curr_count;
    size_t           lazy_move_index;
    size_t           incremental_gc_index;
} NecroVault;

// TODO: Don't return pointers???

// API
NecroVault       necro_create_vault();
void             necro_destroy_vault(NecroVault* vault);
int64_t*         necro_vault_find_int(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch);
NecroVaultEntry* necro_vault_find_entry(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch);
void             necro_vault_insert_int(NecroVault* vault, const NecroVaultKey key, const int64_t value, const int32_t curr_epoch);
void             necro_vault_print(const NecroVault* vault);
void             necro_vault_print_entry(NecroVaultEntry* entry);
void             necro_vault_test();
size_t           necro_vault_count(NecroVault* vault);


//=====================================================
// NecroChain: Separate Chaining Hashtable with Lazy Incremental GC and Resizing
// Key derived from:
//     * Place, Universe, Time
//=====================================================
// 32-bit size: 32 bytes
// 64-bit size: 40 bytes
typedef struct NecroChainNode
{
    struct NecroChainNode* next;       // 4 / 8
    size_t                 hash;       // 4 / 8
    NecroVaultKey          key;        // 12
    int32_t                last_epoch; // 4
    uint32_t               data_size;  // 4
    uint32_t               __padding;  // 4
    // data payload is directly after the NecroChainNode in memory
} NecroChainNode;

typedef struct NecroBucket
{
    NecroChainNode* head;
    size_t          has_nodes;
} NecroChainBucket;

typedef struct
{
    NecroChainBucket*   prev_buckets;
    size_t              prev_size;
    size_t              prev_count;
    NecroChainBucket*   curr_buckets;
    size_t              curr_size;
    size_t              curr_count;
    size_t              lazy_move_index;
    size_t              incremental_gc_index;
    NecroSlabAllocator* slab_allocator;
} NecroChain;

typedef enum
{
    NECRO_FOUND,
    NECRO_ALLOC
} NECRO_FIND_OR_ALLOC_TYPE;

typedef struct
{
    NECRO_FIND_OR_ALLOC_TYPE find_or_alloc_type;
    void*                    data;
} NecroFindOrAllocResult;

// API
NecroChain             necro_create_chain(NecroSlabAllocator* slab_allocator);
void                   necro_destroy_chain(NecroChain* chain);
void*                  necro_chain_find(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch);
NecroChainNode*        necro_chain_find_node(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch);
void                   necro_chain_incremental_gc(NecroChain* chain, const int32_t curr_epoch);
void*                  necro_chain_alloc(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size);
NecroFindOrAllocResult necro_chain_find_or_alloc(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size);
void                   necro_chain_print(NecroChain* chain);
void                   necro_chain_print_node(NecroChainNode* node);
void                   necro_chain_test();
void                   necro_chain_bench();

#endif // VAULT_H
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

// Switch this such that every thunk has its own Vault?

typedef struct
{
    uint32_t place;
    uint32_t time;
} NecroVaultKey;

// typedef struct NecroVaultEntry
// {
//     union
//     {
//         // void*   pointer_data;
//         int64_t int_data;
//         double  float_data;
//     };
//     size_t        hash;
//     NecroVaultKey key;
//     int32_t       last_epoch;
//     int32_t       retirement_age;
// } NecroVaultEntry;

// typedef struct
// {
//     NecroVaultEntry* prev_entries;
//     size_t           prev_size;
//     size_t           prev_count;
//     NecroVaultEntry* curr_entries;
//     size_t           curr_size;
//     size_t           curr_count;
//     size_t           lazy_move_index;
//     size_t           incremental_gc_index;
// } NecroVault;

// // TODO: Don't return pointers???

// // API
// NecroVault       necro_create_vault();
// void             necro_destroy_vault(NecroVault* vault);
// int64_t*         necro_vault_find_int(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch);
// NecroVaultEntry* necro_vault_find_entry(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch);
// void             necro_vault_insert_int(NecroVault* vault, const NecroVaultKey key, const int64_t value, const int32_t curr_epoch);
// void             necro_vault_print(const NecroVault* vault);
// void             necro_vault_print_entry(NecroVaultEntry* entry);
// void             necro_vault_test();
// size_t           necro_vault_count(NecroVault* vault);


//=====================================================
// NecroVault: Separate Chaining Hashtable with Lazy Incremental GC and Resizing
// Key derived from:
//     * Place, Universe, Time
//=====================================================
// 32-bit size: 24 bytes
// 64-bit size: 32 bytes
typedef struct NecroVaultNode
{
    struct NecroVaultNode* next;       // 4 / 8
    size_t                 hash;       // 4 / 8
    NecroVaultKey          key;        // 8
    int32_t                last_epoch; // 4
    uint32_t               data_size;  // 4
    // data payload is directly after the NecroVaultNode in memory
} NecroVaultNode;

typedef struct NecroBucket
{
    NecroVaultNode* head;
    size_t          has_nodes;
} NecroVaultBucket;

typedef struct
{
    NecroVaultBucket*   prev_buckets;
    size_t              prev_size;
    size_t              prev_count;
    NecroVaultBucket*   curr_buckets;
    size_t              curr_size;
    size_t              curr_count;
    size_t              lazy_move_index;
    size_t              incremental_gc_index;
    NecroSlabAllocator* slab_allocator;
} NecroVault;

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
NecroVault             necro_create_vault(NecroSlabAllocator* slab_allocator);
void                   necro_destroy_vault(NecroVault* vault);
void*                  necro_vault_find(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch);
NecroVaultNode*        necro_vault_find_node(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch);
void                   necro_vault_incremental_gc(NecroVault* vault, const int32_t curr_epoch);
void*                  necro_vault_alloc(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size);
NecroFindOrAllocResult necro_vault_find_or_alloc(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size);
void                   necro_vault_print(NecroVault* vault);
void                   necro_vault_print_node(NecroVaultNode* node);
void                   necro_vault_test();
void                   necro_vault_bench();

#endif // VAULT_H
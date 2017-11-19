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
// NecroVault: Lazy HashTable with Entry Iteration
// Key derived from:
//     * Place, Epoch, Time
// Values can either be:
//     * Int, Float, Pointer
//=====================================================

// TODO:
//    * Nix modulus operations! Use the power of powers of 2!

typedef struct NecroVaultEntry
{
    union
    {
        void*   pointer_data;
        double  float_data;
        int64_t int_data;
    };
    size_t  key;
    size_t  next;
    size_t  prev;
    int16_t slab_size;
    int16_t retirement_age;
    bool    is_dead;
} NecroVaultEntry;

typedef struct
{
    NecroVaultEntry* prev_entries;
    size_t           prev_size;
    size_t           prev_count;
    size_t           prev_iteration_head;
    NecroVaultEntry* curr_entries;
    size_t           curr_size;
    size_t           curr_count;
    size_t           curr_iteration_head;
} NecroVault;

// API
NecroVault necro_create_vault();
void       necro_destroy_vault(NecroVault* vault);
int64_t*   necro_vault_find_int(NecroVault* vault, size_t place, size_t epoch, size_t time);
void       necro_vault_delete(NecroVault* vault, size_t place, size_t epoch, size_t time);
// void       necro_insert_int(int32_t place, int32_t epoch, int32_t time, int64_t int_data);
// NecroSymbol necro_intern_string(NecroIntern* intern, const char* str);
// NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice);
// bool        necro_intern_contains_symbol(NecroIntern* intern, NecroSymbol symbol);
// void        necro_print_intern(NecroIntern* intern);
// void        necro_test_intern();

#endif // VAULT_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "vault.h"

// Constants
#define NECRO_VAULT_INITIAL_SIZE    8192
#define NECRO_VAULT_HASH_MAGIC      5381
#define NECRO_VAULT_HASH_MULTIPLIER 37
#define NECRO_VAULT_NULL_ID         0

NecroVault necro_create_vault()
{
    NecroVaultEntry* entries = malloc(NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultEntry));
    if (entries == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating entries in necro_create_intern()\n");
        exit(1);
    }

    for (int32_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
    {
        entries[i] = (NecroVaultEntry) { {0}, 0, 0, 0, 0, 0, true};
    }

    return (NecroVault)
    {
        NULL,
        0,
        0,
        0,
        entries,
        NECRO_VAULT_INITIAL_SIZE,
        0,
        0
    };
}

void necro_destroy_vault(NecroVault* vault)
{
    free(vault->prev_entries);
    vault->prev_size  = 0;
    vault->prev_count = 0;
    free(vault->curr_entries);
    vault->curr_size  = 0;
    vault->curr_count = 0;
}

inline size_t necro_hash_key(const size_t place, const size_t epoch, const size_t time)
{
    size_t hash = time;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    hash       += epoch;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    hash       += place;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    return hash;
}

void necro_vault_grow(NecroVault* vault)
{
    assert(vault->prev_entries == NULL);
    assert(vault->prev_count == 0);
    assert(vault->prev_iteration_head == 0);
    vault->prev_entries        = vault->curr_entries;
    vault->prev_size           = vault->curr_size;
    vault->prev_count          = vault->curr_count;
    vault->prev_iteration_head = vault->curr_iteration_head;
    vault->curr_size           = vault->prev_size * 2;
    vault->curr_count          = 0;
    vault->curr_iteration_head = 0;
    vault->curr_entries        = calloc(vault->curr_size, sizeof(NecroVaultEntry));
}

int64_t* necro_vault_find_int(NecroVault* vault, size_t place, size_t epoch, size_t time)
{
    const size_t key = necro_hash_key(place, epoch, time);
    if (key == NECRO_VAULT_NULL_ID)
        return NULL;

    // Check prev_entries
    for (size_t probe = key & (vault->prev_size - 1); vault->prev_entries[probe].key != NECRO_VAULT_NULL_ID; probe = (probe + 1) & (vault->prev_size - 1))
    {
        if (!vault->prev_entries[probe].is_dead && vault->prev_entries[probe].key == key)
            return &vault->prev_entries[probe].int_data;
    }

    // Check curr_entries
    for (size_t probe = key & (vault->curr_size - 1); vault->curr_entries[probe].key != NECRO_VAULT_NULL_ID; probe = (probe + 1) & (vault->curr_size - 1))
    {
        if (!vault->curr_entries[probe].is_dead && vault->curr_entries[probe].key == key)
            return &vault->curr_entries[probe].int_data;
    }

    return NULL;
}

void necro_vault_delete(NecroVault* vault, size_t place, size_t epoch, size_t time)
{
    const size_t key = necro_hash_key(place, epoch, time);
    if (key == NECRO_VAULT_NULL_ID)
        return;

    // Check prev_entries
    for (size_t probe = key & (vault->prev_size - 1); vault->prev_entries != NULL && vault->prev_entries[probe].key != NECRO_VAULT_NULL_ID; probe = (probe + 1) & (vault->prev_size - 1))
    {
        if (!vault->prev_entries[probe].is_dead && vault->prev_entries[probe].key == key)
        {
            NecroVaultEntry* found_entry = vault->prev_entries + probe;
            found_entry->is_dead         = true;
            vault->prev_count           -= 1;
            assert(vault->curr_count >= 0);
            if (found_entry->prev != 0)
                vault->prev_entries[found_entry->prev].next = found_entry->next;
            if (found_entry->next != 0)
                vault->prev_entries[found_entry->next].prev = found_entry->prev;
            if (vault->prev_iteration_head == probe)
                vault->prev_iteration_head = found_entry->next;
            if (vault->prev_count == 0)
            {
                free(vault->prev_entries);
                vault->prev_entries        = NULL;
                vault->prev_iteration_head = 0;
            }
            return;
        }
    }

    // Check curr_entries
    for (size_t probe = key & (vault->curr_size - 1); vault->curr_entries[probe].key != NECRO_VAULT_NULL_ID; probe = (probe + 1) & (vault->curr_size - 1))
    {
        if (!vault->curr_entries[probe].is_dead && vault->curr_entries[probe].key == key)
        {
            NecroVaultEntry* found_entry = vault->curr_entries + probe;
            found_entry->is_dead         = true;
            vault->prev_count           -= 1;
            assert(vault->curr_count >= 0);
            if (found_entry->prev != 0)
                vault->curr_entries[found_entry->prev].next = found_entry->next;
            if (found_entry->next != 0)
                vault->curr_entries[found_entry->next].prev = found_entry->prev;
            if (vault->curr_iteration_head == probe)
                vault->curr_iteration_head = found_entry->next;
            return;
        }
    }
}

// const char* necro_intern_get_string(NecroIntern* intern, NecroSymbol symbol)
// {
//     if (symbol.id == NECRO_INTERN_NULL_ID)
//         return NULL;
//     for (size_t probe = symbol.hash % intern->size; intern->entries[probe].symbol.id != NECRO_INTERN_NULL_ID; probe = (probe + 1) % intern->size)
//     {
//         if (intern->entries[probe].symbol.id == symbol.id)
//             return intern->strings.data + intern->entries[probe].string_index;
//     }
//     return NULL;
// }

// bool necro_intern_contains_symbol(NecroIntern* intern, NecroSymbol symbol)
// {
//     if (symbol.id == NECRO_INTERN_NULL_ID)
//         return false;
//     for (size_t probe = symbol.hash % intern->size; intern->entries[probe].symbol.id != NECRO_INTERN_NULL_ID; probe = (probe + 1) % intern->size)
//     {
//         if (intern->entries[probe].symbol.id == symbol.id)
//             return true;
//     }
//     return false;
// }

// NecroSymbol necro_intern_string(NecroIntern* intern, const char* str)
// {
//     assert(intern               != NULL);
//     assert(intern->entries      != NULL);
//     assert(intern->strings.data != NULL);

//     // Early exit on NULL strings
//     if (str == NULL)
//         return (NecroSymbol) { 0, NECRO_INTERN_NULL_ID };

//     // Grow if we're over 50% load
//     if (intern->count >= intern->size / 2)
//         necro_intern_grow(intern);

//     // Do linear probe
//     size_t hash  = necro_hash_string(str);
//     size_t probe = hash % intern->size;
//     while (intern->entries[probe].symbol.id != NECRO_INTERN_NULL_ID)
//     {
//         if (intern->entries[probe].symbol.hash == hash)
//         {
//             char* str_at_probe = intern->strings.data + intern->entries[probe].string_index;
//             if (str_at_probe != NULL && strcmp(str_at_probe, str) == 0)
//                 return intern->entries[probe].symbol;
//         }
//         probe = (probe + 1) % intern->size;
//     }

//     // Assert that this entry is in fact empty
//     assert(intern->entries[probe].symbol.id    == NECRO_INTERN_NULL_ID);
//     assert(intern->entries[probe].string_index == 0);

//     // Assign Entry Data
//     intern->entries[probe].symbol       = (NecroSymbol) { hash, intern->count + 1 };
//     intern->entries[probe].string_index = intern->strings.length;

//     // Push string contents into Char Vector
//     for (char* current_char = (char*)str; *current_char != '\0'; ++current_char)
//         necro_push_char_vector(&intern->strings, current_char);
//     necro_push_char_vector(&intern->strings, &NULL_CHAR);

//     // Increase count, return symbol
//     intern->count += 1;
//     return intern->entries[probe].symbol;
// }

// NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice)
// {
//     assert(intern               != NULL);
//     assert(intern->entries      != NULL);
//     assert(intern->strings.data != NULL);

//     // Early exit on NULL strings
//     if (slice.data == NULL)
//         return (NecroSymbol) { 0, NECRO_INTERN_NULL_ID };

//     // Grow if we're over 50% load
//     if (intern->count >= intern->size / 2)
//         necro_intern_grow(intern);

//     // Do Linear probe
//     size_t hash  = necro_hash_string_slice(slice);
//     size_t probe = hash % intern->size;
//     while (intern->entries[probe].symbol.id != NECRO_INTERN_NULL_ID)
//     {
//         if (intern->entries[probe].symbol.hash == hash)
//         {
//             char* str_at_probe = intern->strings.data + intern->entries[probe].string_index;
//             if (str_at_probe != NULL && strncmp(str_at_probe, slice.data, slice.length) == 0)
//                 return intern->entries[probe].symbol;
//         }
//         probe = (probe + 1) % intern->size;
//     }

//     // Assert that this is in fact en empty entry
//     assert(intern->entries[probe].symbol.id    == NECRO_INTERN_NULL_ID);
//     assert(intern->entries[probe].string_index == 0);

//     // Assign Entry data
//     intern->entries[probe].symbol       = (NecroSymbol) { hash, intern->count + 1 };
//     intern->entries[probe].string_index = intern->strings.length;

//     // Push string contents into Char Vector
//     for (size_t i = 0; i < slice.length; ++i)
//         necro_push_char_vector(&intern->strings, (char*) (slice.data + i));
//     necro_push_char_vector(&intern->strings, &NULL_CHAR);

//     // Increase count and return symbol
//     intern->count += 1;
//     return intern->entries[probe].symbol;
// }

// void necro_print_intern(NecroIntern* intern)
// {
//     printf("NecroIntern\n{\n");
//     printf("    size:    %d,\n", intern->size);
//     printf("    count:   %d,\n", intern->count);
//     printf("    data:\n    [\n");
//     for (size_t i = 0; i < intern->size; ++i)
//     {
//         if (intern->entries[i].symbol.id == NECRO_INTERN_NULL_ID)
//             continue;
//         printf("        hash: %d, id: %d, value: %s\n", intern->entries[i].symbol.hash, intern->entries[i].symbol.id, intern->strings.data + intern->entries[i].string_index);
//     }
//     printf("    ]\n");
//     printf("}\n\n");
// }

// //=====================================================
// // Testing
// //=====================================================
// void necro_test_intern_id(NecroIntern* intern, NecroSymbol symbol, const char* compare_str)
// {
//     printf("----\nstring: %s, hash: %d, symbol: %d\n", compare_str, symbol.hash, symbol.id);

//     // ID Test
//     if (symbol.id != NECRO_INTERN_NULL_ID)
//         puts("NecroIntern id test:         passed");
//     else
//         puts("NecroIntern id test:         failed");

//     // Hash Test
//     if (symbol.hash == necro_hash_string(compare_str))
//         puts("NecroIntern hash test:       passed");
//     else
//         puts("NecroIntern hash test:       failed");

//     // Contains test
//     if (necro_intern_contains_symbol(intern, symbol))
//         puts("NecroIntern contains test:   passed");
//     else
//         puts("NecroIntern contains test:   failed");

//     // Get test
//     if (strcmp(necro_intern_get_string(intern, symbol), compare_str) == 0)
//         puts("NecroIntern get test:        passed");
//     else
//         puts("NecroIntern get test:        failed");
// }

// void necro_test_intern()
// {
//     puts("--------------------------------");
//     puts("-- Testing NecroIntern");
//     puts("--------------------------------\n");

//     NecroIntern intern = necro_create_intern();

//     // Test ID1
//     NecroSymbol  id1    = necro_intern_string(&intern, "test");
//     necro_test_intern_id(&intern, id1, "test");

//     // Test ID2
//     NecroSymbol  id2    = necro_intern_string(&intern, "This is not a test");
//     necro_test_intern_id(&intern, id2, "This is not a test");

//     // Test ID3
//     NecroSymbol  id3    = necro_intern_string_slice(&intern, (NecroStringSlice) { "fuck yeah", 4 });
//     necro_test_intern_id(&intern, id3, "fuck");

//     // Test ID4
//     NecroSymbol  id4    = necro_intern_string_slice(&intern, (NecroStringSlice) { "please work?", 6 });
//     necro_test_intern_id(&intern, id4, "please");

//     // puts("\n---");
//     // necro_print_intern(&intern);
//     // puts("");

//     // Destroy test
//     necro_destroy_intern(&intern);
//     if (intern.entries == NULL)
//         puts("NecroIntern destroy test:    passed");
//     else
//         puts("NecroIntern destroy test:    failed");

//     // Grow test
//     puts("---\nGrow test");
//     intern = necro_create_intern();
//     NecroSymbol symbols[NECRO_INITIAL_INTERN_SIZE];
//     for (size_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
//     {
//         char num = '0' + (char) i;
//         symbols[i] = necro_intern_string_slice(&intern, (NecroStringSlice) { &num, 1 });
//     }

//     bool grow_test_passed = true;
//     for (size_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
//     {
//         grow_test_passed = grow_test_passed && necro_intern_contains_symbol(&intern, symbols[i]);
//     }

//     if (grow_test_passed)
//         puts("NecroIntern grow test:       passed");
//     else
//         puts("NecroIntern grow test:       failed");

//     // Grow Destroy test
//     necro_destroy_intern(&intern);
//     if (intern.entries == NULL)
//         puts("NecroIntern g destroy test:  passed");
//     else
//         puts("NecroIntern g destroy test:  failed");
// }

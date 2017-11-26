/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "vault.h"

// Constants
#define NECRO_VAULT_INITIAL_SIZE    8192
#define NECRO_VAULT_HASH_MAGIC      5381
#define NECRO_VAULT_HASH_MULTIPLIER 37
#define NECRO_VAULT_NULL_PLACE      0
#define NECRO_VAULT_GC_COUNT        4
#define NECRO_VAULT_INITIAL_AGE     2
#define NECRO_VAULT_RETIREMENT_AGE  2

#define NECRO_VAULT_DEBUG           0
#if NECRO_VAULT_DEBUG
#define TRACE_VAULT(...) printf(__VA_ARGS__)
#else
#define TRACE_VAULT(...)
#endif

inline bool necro_vault_compare_key(NecroVaultKey key1, NecroVaultKey key2)
{
    // return key1.place == key2.place && key1.time == key2.time && key1.universe == key2.universe;
    return key1.place == key2.place && key1.time == key2.time;
}

inline size_t necro_hash_key(const NecroVaultKey key)
{
    size_t hash = key.time;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    hash       += key.place;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    // hash       += key.universe;
    // hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    return hash;
    // uint32_t highorder = 0;
    // uint32_t h         = 0;
    // highorder = h & 0xf8000000;
    // h = h << 5;
    // h = h ^ (highorder >> 27);
    // h = h ^ key.time;
    // highorder = h & 0xf8000000;
    // h = h << 5;
    // h = h ^ (highorder >> 27);
    // h = h ^ key.place;
    // highorder = h & 0xf8000000;
    // h = h << 5;
    // h = h ^ (highorder >> 27);
    // h = h ^ key.universe;
    // return h;
}

// NecroVault necro_create_vault()
// {
//     TRACE_VAULT("create vault\n");

//     NecroVaultEntry* entries = malloc(NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultEntry));
//     if (entries == NULL)
//     {
//         fprintf(stderr, "Malloc returned null while allocating entries in necro_create_intern()\n");
//         exit(1);
//     }
//     memset(entries, 0, NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultEntry));

//     return (NecroVault)
//     {
//         NULL,
//         0,
//         0,
//         entries,
//         NECRO_VAULT_INITIAL_SIZE,
//         0,
//         0
//     };
// }

// void necro_destroy_vault(NecroVault* vault)
// {
//     TRACE_VAULT("destroy vault\n");
//     free(vault->prev_entries);
//     vault->prev_entries    = NULL;
//     vault->prev_size       = 0;
//     vault->prev_count      = 0;
//     free(vault->curr_entries);
//     vault->curr_entries    = NULL;
//     vault->curr_size       = 0;
//     vault->curr_count      = 0;
//     vault->lazy_move_index = 0;
// }

// void necro_vault_grow(NecroVault* vault)
// {
//     TRACE_VAULT("grow\n");
//     assert(vault->prev_entries == NULL);
//     assert(vault->prev_size == 0);
//     assert(vault->prev_count == 0);
//     vault->prev_entries    = vault->curr_entries;
//     vault->prev_size       = vault->curr_size;
//     vault->prev_count      = vault->curr_count;
//     vault->curr_size       = vault->prev_size * 2;
//     vault->curr_count      = 0;
//     vault->lazy_move_index = 0;
//     vault->curr_entries    = calloc(vault->curr_size, sizeof(NecroVaultEntry));
//     if (vault->curr_entries == NULL)
//     {
//         fprintf(stderr, "Malloc returned null in necro_vault_grow()\n");
//         exit(1);
//     }
// }

// inline bool is_circularly_between(size_t a, size_t b, size_t c)
// {
//     if (a <= c)
//         return a <= b && b < c;
//     else
//         return a <= b || b < c;
// }

// inline void necro_vault_delete_entry(NecroVaultEntry* table, const size_t table_size, size_t index)
// {
//     TRACE_VAULT("delete entry at index: %d\n", index);
//     assert(table != NULL);
//     size_t to   = index;
//     size_t from = index;
//     while (true)
//     {
//         from = (from + 1) & (table_size - 1);
//         if (table[from].key.place == NECRO_VAULT_NULL_PLACE)
//             break;
//         size_t h = table[from].hash & (table_size - 1);
//         if (is_circularly_between(h, to, from))
//         {
//             table[to] = table[from];
//             to        = from;
//         }
//     }
//     table[to] = (NecroVaultEntry) { .int_data = 0, .hash = 0, .key = { .place = 0,.universe = 0,.time = 0 }, .last_epoch = 0, .retirement_age = 0 };
// }

// inline bool necro_vault_lazy_gc(NecroVaultEntry* table, const size_t table_size, size_t* count, size_t index, NecroVaultEntry* entry, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("Lazy gc, curr_epoch: %d, entry.last_epoch: %d\n", curr_epoch, entry->last_epoch);
//     if (entry->last_epoch >= curr_epoch)
//         return false;
//     // TRACE_VAULT("Lazy gc, curr_epoch: %d, entry.last_epoch: %d, entry.retirement_age: %d\n", curr_epoch, entry->last_epoch, entry->retirement_age);
//     entry->retirement_age -= curr_epoch - entry->last_epoch;
//     entry->last_epoch      = curr_epoch;
//     // TRACE_VAULT("Lazy gc, new last_epoch: %d, new retirement_age: %d\n", entry->last_epoch, entry->retirement_age);
//     if (entry->retirement_age <= 0)
//     {
//         // TRACE_VAULT("Lazy gc, delete\n");
//         necro_vault_delete_entry(table, table_size, index);
//         *count = *count - 1;
//         return true;
//     }
//     return false;
// }

// inline NecroVaultEntry* necro_vault_find_entry_in_table(NecroVaultEntry* table, const size_t table_size, size_t* count, size_t hash, const NecroVaultKey key, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("find entry in table\n");
//     if (table == NULL)
//         return NULL;
//     uint32_t gc_count = NECRO_VAULT_GC_COUNT;
//     for (size_t probe = hash & (table_size - 1); table[probe].key.place != NECRO_VAULT_NULL_PLACE; probe = (probe + 1) & (table_size - 1))
//     {
//         NecroVaultEntry* entry = table + probe;
//         // found matching entry
//         if (necro_vault_compare_key(entry->key, key))
//         {
//             if (entry->last_epoch < curr_epoch)
//             {
//                 entry->last_epoch     = curr_epoch;
//                 entry->retirement_age = NECRO_VAULT_INITIAL_AGE;
//                 TRACE_VAULT("Bumped retirement age\n");
//             }
//             // TRACE_VAULT("found entry in table\n");
//             return entry;
//         }
//         // Perform lazy GC
//         else if (gc_count != 0 && necro_vault_lazy_gc(table, table_size, count, probe, entry, curr_epoch))
//         {
//             gc_count--;
//             probe = (probe - 1) & (table_size - 1);
//         }
//     }
//     return NULL;
// }

// int64_t* necro_vault_find_int(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("find int\n");
//     if (key.place == NECRO_VAULT_NULL_PLACE)
//         return NULL;
//     const size_t hash = necro_hash_key(key);

//     NecroVaultEntry* prev_entry = necro_vault_find_entry_in_table(vault->prev_entries, vault->prev_size, &vault->prev_count, hash, key, curr_epoch);
//     if (prev_entry != NULL)
//         return &prev_entry->int_data;

//     NecroVaultEntry* curr_entry = necro_vault_find_entry_in_table(vault->curr_entries, vault->curr_size, &vault->curr_count, hash, key, curr_epoch);
//     if (curr_entry != NULL)
//         return &curr_entry->int_data;

//     // TRACE_VAULT("entry not found in table\n");
//     return NULL;
// }

// NecroVaultEntry* necro_vault_find_entry(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("find entry\n");
//     if (key.place == NECRO_VAULT_NULL_PLACE)
//         return NULL;
//     const size_t hash = necro_hash_key(key);

//     NecroVaultEntry* prev_entry = necro_vault_find_entry_in_table(vault->prev_entries, vault->prev_size, &vault->prev_count, hash, key, curr_epoch);
//     if (prev_entry != NULL)
//         return prev_entry;

//     NecroVaultEntry* curr_entry = necro_vault_find_entry_in_table(vault->curr_entries, vault->curr_size, &vault->curr_count, hash, key, curr_epoch);
//     if (curr_entry != NULL)
//         return curr_entry;

//     return NULL;
// }

// inline void necro_vault_insert_entry(NecroVault* vault, const NecroVaultEntry a_entry, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("insert entry\n");
//     if (a_entry.key.place == NECRO_VAULT_NULL_PLACE)
//         return;
//     // Grow if we're over 50% load
//     if (vault->curr_count >= vault->curr_size / 2)
//         necro_vault_grow(vault);
//     size_t gc_count = NECRO_VAULT_GC_COUNT;
//     for (size_t probe = a_entry.hash & (vault->curr_size - 1); true; probe = (probe + 1) & (vault->curr_size - 1))
//     {
//         // if (probe == NECRO_VAULT_NULL_ID)
//         //     continue;
//         NecroVaultEntry* entry = vault->curr_entries + probe;
//         if (entry->key.place == NECRO_VAULT_NULL_PLACE)
//         {
//             vault->curr_count         += 1;
//             vault->curr_entries[probe] = a_entry;
//             TRACE_VAULT("insert into null, count: %d\n", vault->curr_count);
//             break;
//         }
//         else if (entry->hash == a_entry.hash && necro_vault_compare_key(entry->key, a_entry.key))
//         {
//             vault->curr_entries[probe] = a_entry;
//             TRACE_VAULT("insert at key, count: %d\n", vault->curr_count);
//             break;
//         }
//         // Perform lazy GC
//         else if (gc_count != 0 && necro_vault_lazy_gc(vault->curr_entries, vault->curr_size, &vault->curr_count, probe, entry, curr_epoch))
//         {
//             TRACE_VAULT("perform lazy gc\n");
//             gc_count--;
//             // probe--;
//             probe = (probe - 1) & (vault->curr_size - 1);
//         }
//         else
//         {
//             TRACE_VAULT("other, probe value: %d\n", probe);
//         }
//     }
// }

// inline void necro_vault_insert_entry_and_lazy_move(NecroVault* vault, const NecroVaultEntry a_entry, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("insert entry and lazy move\n");
//     assert(vault);
//     assert(vault->curr_entries);

//     // Perform lazy move
//     for (size_t i = 0; i < 2; ++ i)
//     {
//         if (vault->lazy_move_index < vault->prev_size && vault->prev_count > 0 && vault->prev_entries)
//         {
//             NecroVaultEntry* entry = vault->prev_entries + vault->lazy_move_index;
//             if (entry->key.place != NECRO_VAULT_NULL_PLACE)
//             {
//                 TRACE_VAULT("Lazy move\n");
//                 necro_vault_insert_entry(vault, *entry, curr_epoch);
//                 necro_vault_delete_entry(vault->prev_entries, vault->prev_size, vault->lazy_move_index);
//                 vault->prev_count--;
//                 if (vault->prev_count == 0)
//                 {
//                     free(vault->prev_entries);
//                     vault->prev_entries = NULL;
//                     vault->prev_size    = 0;
//                 }
//             }
//             vault->lazy_move_index++;
//         }
//     }

//     necro_vault_insert_entry(vault, a_entry, curr_epoch);
//     assert(vault->prev_count <= vault->prev_size / 2);
//     assert(vault->curr_count <= vault->curr_size / 2);
// }

// void necro_vault_insert_int(NecroVault* vault, const NecroVaultKey key, const int64_t value, const int32_t curr_epoch)
// {
//     // TRACE_VAULT("insert int\n");
//     necro_vault_insert_entry_and_lazy_move(vault, (NecroVaultEntry) { .int_data = value, .hash = necro_hash_key(key), .key = key, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);
// }


// size_t necro_vault_count(NecroVault* vault)
// {
//     return vault->prev_count + vault->curr_count;
// }

// void necro_vault_print_entry(NecroVaultEntry* entry)
// {
//     if (entry == NULL)
//         printf("NULL entry\n");
//     else
//         printf("NecroEntry { hash: %d, place: %d, universe: %d, time: %d, value: %lld, last_epoch: %d, retirement_age: %d }\n", entry->hash, entry->key.place, entry->key.universe, entry->key.time, entry->int_data, entry->last_epoch, entry->retirement_age);
// }

// void necro_vault_print(const NecroVault* vault)
// {
//     printf("NecroVault\n{\n");

//     printf("    prev_size:    %d,\n", vault->prev_size);
//     printf("    prev_count:   %d,\n", vault->prev_count);
//     printf("    prev_data:\n    [\n");
//     for (size_t i = 0; i < vault->prev_size; ++i)
//     {
//         NecroVaultEntry* entry = vault->prev_entries + i;
//         if (entry->key.place == NECRO_VAULT_NULL_PLACE)
//             continue;
//         printf("        [%d] = ", i);
//         necro_vault_print_entry(entry);
//     }
//     printf("    ]\n");

//     printf("    curr_size:    %d,\n", vault->curr_size);
//     printf("    curr_count:   %d,\n", vault->curr_count);
//     printf("    curr_data:\n    [\n");
//     for (size_t i = 0; i < vault->curr_size; ++i)
//     {
//         NecroVaultEntry* entry = vault->curr_entries + i;
//         if (entry->key.place == NECRO_VAULT_NULL_PLACE)
//             continue;
//         printf("        [%d] = ", i);
//         necro_vault_print_entry(entry);
//     }
//     printf("    ]\n");
//     printf("}\n\n");
// }

// //=====================================================
// // Testing
// //=====================================================
// void necro_test_vault_entry(NecroVault* vault, NecroVaultEntry a_entry, const int32_t curr_epoch)
// {
//     printf("----\nentry: ");
//     necro_vault_print_entry(&a_entry);

//     // NULL_ID Test
//     if (a_entry.key.place != NECRO_VAULT_NULL_PLACE)
//         puts("NecroVault null test:      passed");
//     else
//         puts("NecroVault null test:      failed");

//     NecroVaultEntry* entry = necro_vault_find_entry(vault, a_entry.key, curr_epoch);

//     // Contains Test
//     if (entry != NULL)
//         puts("NecroVault contains test: passed");
//     else
//         puts("NecroVault contains test: failed");

//     // Equals Test
//     if (entry->int_data == a_entry.int_data && entry->hash == a_entry.hash && necro_vault_compare_key(entry->key, a_entry.key) && entry->last_epoch == a_entry.last_epoch && entry->retirement_age == a_entry.retirement_age)
//     {
//         puts("NecroVault equals test:   passed");
//     }
//     else
//     {
//         puts("NecroVault equals test:   failed");
//         printf("                          ");
//         necro_vault_print_entry(entry);
//     }
// }

// void necro_vault_test()
// {
//     puts("--------------------------------");
//     puts("-- Testing NecroVault");
//     puts("--------------------------------\n");

//     NecroVault vault      = necro_create_vault();
//     int32_t    curr_epoch = 1;

//     // Test Entry1
//     NecroVaultKey key1 = { .place = 1, .universe = 0, .time = 1 };
//     necro_vault_insert_int(&vault, key1, 666, curr_epoch);
//     necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 666, .hash = necro_hash_key(key1), .key = key1, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

//     // Test Entry2
//     NecroVaultKey key2 = { .place = 2, .universe = 0, .time = 1 };
//     necro_vault_insert_int(&vault, key2, 1000, curr_epoch);
//     necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 1000, .hash = necro_hash_key(key2), .key = key2, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

//     // Test Entry3
//     NecroVaultKey key3 = { .place = 3, .universe = 1, .time = 2 };
//     necro_vault_insert_int(&vault, key3, 300, curr_epoch);
//     necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 300, .hash = necro_hash_key(key3), .key = key3, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

//     // Test Entry4
//     NecroVaultKey key4 = { .place = 4, .universe = 100, .time = 22 };
//     necro_vault_insert_int(&vault, key4, 44, curr_epoch);
//     necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 44, .hash = necro_hash_key(key4), .key = key4, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

//     // Double Insert Test
//     necro_vault_insert_int(&vault, key3, -1, curr_epoch);
//     necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = -1, .hash = necro_hash_key(key3), .key = key3, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

//     // puts("\n---");
//     // necro_vault_print(&vault);
//     // puts("");

//     // Destroy test
//     necro_destroy_vault(&vault);
//     if (vault.prev_entries == NULL && vault.curr_entries == NULL)
//         puts("NecroVault destroy test:    passed");
//     else
//         puts("NecroVault destroy test:    failed");

//     // Many inserts test
//     puts("---\nMany Inserts test");
//     vault = necro_create_vault();
//     bool test_passed = true;
//     for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
//     {
//         NecroVaultKey    key   = { .place = 1, .universe = 0, .time = i };
//         necro_vault_insert_int(&vault, key, i, 1);
//         NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 1);
//         if (entry == NULL || entry->int_data != i || !necro_vault_compare_key(entry->key, key))
//         {
//             puts("NecroVault many inserts test:   failed");
//             printf("                                ");
//             necro_vault_print_entry(entry);
//             test_passed = false;
//             break;
//         }
//     }
//     if (test_passed)
//     {
//         puts("NecroVault many inserts test:   passed");
//     }

//     // Many Re-inserts test
//     test_passed = true;
//     puts("---\nMany Re-Inserts test");
//     for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
//     {
//         NecroVaultKey    key   = { .place = 10, .universe = 0, .time = i };
//         necro_vault_insert_int(&vault, key, i + 10, 10);
//         NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 10);
//         if (entry == NULL || entry->int_data != i + 10 || !necro_vault_compare_key(entry->key, key))
//         {
//             puts("NecroVault re-inserts test:     failed");
//             printf("                                ");
//             necro_vault_print_entry(entry);
//             printf("Expected entry:\n");
//             printf("                                ");
//             necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 10, .hash = necro_hash_key(key), .key = key, .last_epoch = 10, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
//             // necro_vault_print(&vault);
//             test_passed = false;
//             break;
//         }
//     }
//     if (test_passed)
//     {
//         puts("NecroVault re-inserts test:      passed");
//         // necro_vault_print(&vault);
//     }

//     // Many Lookups test
//     test_passed = true;
//     puts("---\nMany Lookups test");
//     for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
//     {
//         NecroVaultKey    key   = { .place = 10, .universe = 0, .time = i };
//         NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 10);
//         if (entry == NULL || entry->int_data != i + 10 || !necro_vault_compare_key(entry->key, key))
//         {
//             // necro_vault_print(&vault);
//             puts("NecroVault many lookups test:     failed");
//             printf("                                ");
//             necro_vault_print_entry(entry);
//             printf("Expected entry:\n");
//             printf("                                ");
//             necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 10, .hash = necro_hash_key(key), .key = key, .last_epoch = 10, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
//             test_passed = false;
//             break;
//         }
//     }
//     if (test_passed)
//     {
//         puts("NecroVault many lookups test:      passed");
//     }

//     // Many Re-inserts2 test
//     test_passed = true;
//     puts("---\nMany Re-Inserts2 test");
//     for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
//     {
//         NecroVaultKey    key   = { .place = 20, .universe = 0, .time = i };
//         necro_vault_insert_int(&vault, key, i + 20, 20);
//         NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 20);
//         if (entry == NULL || entry->int_data != i + 20 || !necro_vault_compare_key(entry->key, key))
//         {
//             puts("NecroVault re-inserts2 test:     failed");
//             printf("                                ");
//             necro_vault_print_entry(entry);
//             printf("Expected entry:\n");
//             printf("                                ");
//             necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 20, .hash = necro_hash_key(key), .key = key, .last_epoch = 20, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
//             // necro_vault_print(&vault);
//             test_passed = false;
//             break;
//         }
//     }
//     if (test_passed)
//     {
//         puts("NecroVault re-inserts2 test:      passed");
//         // necro_vault_print(&vault);
//     }

//     // Many Lookups2 test
//     test_passed = true;
//     puts("---\nMany Lookups2 test");
//     for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
//     {
//         NecroVaultKey    key   = { .place = 20, .universe = 0, .time = i };
//         NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 20);
//         if (entry == NULL || entry->int_data != i + 20 || !necro_vault_compare_key(entry->key, key))
//         {
//             necro_vault_print(&vault);
//             puts("NecroVault many lookups2 test:     failed");
//             printf("                                ");
//             necro_vault_print_entry(entry);
//             printf("Expected entry:\n");
//             printf("                                ");
//             necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 20, .hash = necro_hash_key(key), .key = key, .last_epoch = 20, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
//             test_passed = false;
//             break;
//         }
//     }
//     if (test_passed)
//     {
//         puts("NecroVault many lookups2 test:      passed");
//     }

//     // MonotonicEpoch test
//     test_passed = true;
//     puts("---\nMonotonicEpoch test");
//     for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE * 6; ++i)
//     {
//         NecroVaultKey    key   = { .place = 30, .universe = 0, .time = i };
//         necro_vault_insert_int(&vault, key, i + 30, 100 + i);
//         NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 100 + i);
//         if (entry == NULL || entry->int_data != i + 30 || !necro_vault_compare_key(entry->key, key))
//         {
//             necro_vault_print(&vault);
//             puts("NecroVault many MonotonicEpoch test:     failed");
//             printf("                                ");
//             necro_vault_print_entry(entry);
//             printf("Expected entry:\n");
//             printf("                                ");
//             necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 30, .hash = necro_hash_key(key), .key = key, .last_epoch = 100 + i, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
//             test_passed = false;
//             break;
//         }
//     }
//     if (test_passed)
//     {
//         puts("NecroVault many MonotonicEpoch test:      passed");
//     }

//     printf("NecroVault size: %d, count: %d\n", vault.curr_size, necro_vault_count(&vault));
// }

//=====================================================
// NecroVault
//=====================================================
NecroVault necro_create_vault(NecroSlabAllocator* slab_allocator)
{
    TRACE_VAULT("create vault\n");

    NecroVaultBucket* curr_buckets = malloc(NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultBucket));
    if (curr_buckets == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating entries in necro_create_vault()\n");
        exit(1);
    }
    memset(curr_buckets, 0, NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultBucket));

    return (NecroVault)
    {
        .prev_buckets         = NULL,
        .prev_size            = 0,
        .prev_count           = 0,
        .curr_buckets         = curr_buckets,
        .curr_size            = NECRO_VAULT_INITIAL_SIZE,
        .curr_count           = 0,
        .lazy_move_index      = 0,
        .incremental_gc_index = 0,
        .slab_allocator       = slab_allocator
    };
}

inline NecroVaultNode* necro_vault_alloc_node(NecroSlabAllocator* slab_allocator, const size_t hash, const NecroVaultKey key, const int32_t curr_epoch, NecroVaultNode* next, const size_t data_size)
{
    NecroVaultNode* node = necro_alloc_slab(slab_allocator, sizeof(NecroVaultNode) + data_size);
    node->next           = next;
    node->key            = key;
    node->hash           = hash;
    node->last_epoch     = curr_epoch;
    node->data_size      = data_size;
    return node;
}

inline void necro_vault_free_node(NecroSlabAllocator* slab_allocator, NecroVaultNode* node)
{
    necro_free_slab(slab_allocator, node, sizeof(NecroVaultNode) + node->data_size);
}

inline void necro_vault_destroy_buckets(NecroVaultBucket* buckets, size_t size, NecroSlabAllocator* slab_allocator)
{
    TRACE_VAULT("destroy buckets\n");
    for (size_t i = 0; i < size; ++i)
    {
        if (!buckets[i].has_nodes)
            continue;
        NecroVaultNode* node = buckets[i].head;
        while (node != NULL)
        {
            NecroVaultNode* next = node->next;
            // necro_free_slab(slab_allocator, node, sizeof(NecroVaultNode));
            necro_vault_free_node(slab_allocator, node);
            node = next;
        }
    }
}

void necro_destroy_vault(NecroVault* vault)
{
    TRACE_VAULT("destroy vault\n");
    necro_vault_destroy_buckets(vault->prev_buckets, vault->prev_size, vault->slab_allocator);
    free(vault->prev_buckets);
    vault->prev_buckets         = NULL;
    vault->prev_size            = 0;
    vault->prev_count           = 0;
    necro_vault_destroy_buckets(vault->curr_buckets, vault->curr_size, vault->slab_allocator);
    free(vault->curr_buckets);
    vault->curr_buckets         = NULL;
    vault->curr_size            = 0;
    vault->curr_count           = 0;
    vault->lazy_move_index      = 0;
    vault->incremental_gc_index = 0;
    vault->slab_allocator       = NULL;
}

void necro_vault_destroy_prev_bucket(NecroVault* vault)
{
    if (vault->prev_buckets == NULL)
        return;
    TRACE_VAULT("Destroy prev_buckets\n");
    necro_vault_destroy_buckets(vault->prev_buckets, vault->prev_size, vault->slab_allocator);
    free(vault->prev_buckets);
    vault->prev_buckets = NULL;
    vault->prev_size    = 0;
    vault->prev_count   = 0;
}

void necro_vault_grow(NecroVault* vault)
{
    TRACE_VAULT("grow\n");
    assert(vault->prev_count == 0);
    necro_vault_destroy_prev_bucket(vault);
    assert(vault->prev_size == 0);
    assert(vault->prev_buckets == NULL);
    vault->prev_buckets         = vault->curr_buckets;
    vault->prev_size            = vault->curr_size;
    vault->prev_count           = vault->curr_count;
    vault->curr_size            = vault->prev_size * 2;
    vault->curr_count           = 0;
    vault->lazy_move_index      = 0;
    vault->incremental_gc_index = 0;
    vault->curr_buckets         = calloc(vault->curr_size, sizeof(NecroVaultBucket));
    if (vault->curr_buckets == NULL)
    {
        fprintf(stderr, "Malloc returned null in necro_vault_grow()\n");
        exit(1);
    }
}

inline bool necro_vault_lazy_gc(NecroVaultNode* node, const int32_t curr_epoch)
{
    // TRACE_VAULT("Lazy gc, curr_epoch: %d, entry.last_epoch: %d\n", curr_epoch, entry->last_epoch);
    if (node->last_epoch >= curr_epoch)
        return false;
    TRACE_VAULT("Lazy gc, curr_epoch: %d, entry.last_epoch: %d\n", curr_epoch, node->last_epoch);
    // node->retirement_age -= curr_epoch - node->last_epoch;
    int32_t epoch_difference = curr_epoch - node->last_epoch;
    node->last_epoch         = curr_epoch;
    TRACE_VAULT("Lazy gc, new last_epoch: %d, difference: %d\n", node->last_epoch, epoch_difference);
    // if (node->retirement_age <= 0)
    if (epoch_difference >= NECRO_VAULT_RETIREMENT_AGE)
    {
        TRACE_VAULT("Lazy gc, retire!\n");
        return true;
    }
    return false;
}

inline NecroVaultNode* necro_vault_find_node_in_buckets(NecroSlabAllocator* slab_allocator, NecroVaultBucket* buckets, const size_t buckets_size, size_t* count, size_t hash, const NecroVaultKey key, const int32_t curr_epoch)
{
    TRACE_VAULT("find node in buckets\n");
    if (buckets == NULL)
        return NULL;

    size_t index = hash & (buckets_size - 1);

    if (!buckets[index].has_nodes)
        return NULL;

    NecroVaultNode* prev = NULL;
    NecroVaultNode* node = buckets[index].head;
    while (node != NULL)
    {
        if (necro_vault_compare_key(node->key, key))
        {
            if (node->last_epoch < curr_epoch)
            {
                node->last_epoch     = curr_epoch;
                // node->retirement_age = NECRO_VAULT_INITIAL_AGE;
                TRACE_VAULT("Bumped retirement age\n");
            }
            TRACE_VAULT("found node in buckets\n");
            return node;
        }
        else
        {
            NecroVaultNode* next = node->next;
            if (necro_vault_lazy_gc(node, curr_epoch))
            {
                *count = *count - 1;
                if (prev != NULL)
                {
                    prev->next = next;
                }
                else
                {
                    assert(node == buckets[index].head);
                    buckets[index].head = node->next;
                }
                // necro_free_slab(slab_allocator, node, sizeof(NecroVaultNode));
                necro_vault_free_node(slab_allocator, node);
            }
            prev = node;
            node = next;
        }
    }

    buckets[index].has_nodes = buckets[index].head != NULL;

    return NULL;
}

void* necro_vault_find(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch)
{
    assert(vault != NULL);
    TRACE_VAULT("find\n");

    const size_t hash = necro_hash_key(key);

    necro_vault_incremental_gc(vault, curr_epoch);

    if ((hash & (vault->prev_size - 1)) >= vault->lazy_move_index)
    {
        NecroVaultNode* prev_node = necro_vault_find_node_in_buckets(vault->slab_allocator, vault->prev_buckets, vault->prev_size, &vault->prev_count, hash, key, curr_epoch);
        if (prev_node != NULL)
            return (prev_node + 1);
    }

    NecroVaultNode* curr_node = necro_vault_find_node_in_buckets(vault->slab_allocator, vault->curr_buckets, vault->curr_size, &vault->curr_count, hash, key, curr_epoch);
    if (curr_node != NULL)
        return (curr_node + 1);

    TRACE_VAULT("key not found in table\n");
    return NULL;
}

NecroVaultNode* necro_vault_find_node(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch)
{
    assert(vault != NULL);
    TRACE_VAULT("find node\n");

    const size_t hash = necro_hash_key(key);

    necro_vault_incremental_gc(vault, curr_epoch);

    if ((hash & (vault->prev_size - 1)) >= vault->lazy_move_index)
    {
        NecroVaultNode* prev_node = necro_vault_find_node_in_buckets(vault->slab_allocator, vault->prev_buckets, vault->prev_size, &vault->prev_count, hash, key, curr_epoch);
        if (prev_node != NULL)
            return prev_node;
    }

    NecroVaultNode* curr_node = necro_vault_find_node_in_buckets(vault->slab_allocator, vault->curr_buckets, vault->curr_size, &vault->curr_count, hash, key, curr_epoch);
    if (curr_node != NULL)
        return curr_node;

    TRACE_VAULT("key not found in table\n");
    return NULL;
}

inline void* necro_vault_alloc_with_key(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size)
{
    TRACE_VAULT("alloc with key\n");
    assert(vault);
    assert(vault->slab_allocator);
    assert(vault->curr_buckets);

    if (vault->curr_count > vault->curr_size / 2)
        necro_vault_grow(vault);

    size_t          hash  = necro_hash_key(key);
    size_t          index = hash & (vault->curr_size - 1);
    NecroVaultNode* prev  = NULL;
    NecroVaultNode* node  = NULL;
    NecroVaultNode* next  = NULL;

    if (!vault->curr_buckets[index].has_nodes)
    {
        TRACE_VAULT("allocating new node into empty bucket\n");
        // node = necro_alloc_slab(vault->slab_allocator, sizeof(NecroVaultNode));
        node = necro_vault_alloc_node(vault->slab_allocator, hash, key, curr_epoch, NULL, data_size);
        // necro_vault_initialize_node(node, hash, key, curr_epoch, NULL);
        vault->curr_buckets[index].head      = node;
        vault->curr_buckets[index].has_nodes = true;
        vault->curr_count++;
        return (node + 1);
    }

    // TODO: Redo these where we "swap" nodes, can't do that with mixed data sizes!
    node = vault->curr_buckets[index].head;
    while (node != NULL)
    {
        next = node->next;
        if (necro_vault_compare_key(node->key, key))
        {
            TRACE_VAULT("allocating into existing node with same key\n");
            necro_vault_free_node(vault->slab_allocator, node);
            node = necro_vault_alloc_node(vault->slab_allocator, hash, key, curr_epoch, next, data_size);
            if (prev == NULL)
                vault->curr_buckets[index].head = node;
            else
                prev->next = node;
            // necro_vault_initialize_node(node, hash, key, curr_epoch, next);
            return (node + 1);
        }
        else
        {
            if (necro_vault_lazy_gc(node, curr_epoch))
            {
                TRACE_VAULT("allocating into existing retired node\n");
                necro_vault_free_node(vault->slab_allocator, node);
                node = necro_vault_alloc_node(vault->slab_allocator, hash, key, curr_epoch, next, data_size);
                if (prev == NULL)
                    vault->curr_buckets[index].head = node;
                else
                    prev->next = node;
                // necro_vault_initialize_node(node, hash, key, curr_epoch, next);
                return (node + 1);
            }
            prev = node;
            node = next;
        }
    }

    TRACE_VAULT("allocating new node at end of vault\n");
    assert(prev != NULL);
    // node = necro_alloc_slab(vault->slab_allocator, sizeof(NecroVaultNode));
    node = necro_vault_alloc_node(vault->slab_allocator, hash, key, curr_epoch, NULL, data_size);
    // necro_vault_initialize_node(node, hash, key, curr_epoch, NULL);
    prev->next = node;
    vault->curr_count++;
    return (node + 1);
    // return ((char*)node) + sizeof(NecroVaultNode);
}

inline void necro_vault_insert_prev_node_into_curr_buckets(NecroVault* vault, NecroVaultNode* inserted_node, const int32_t curr_epoch)
{
    TRACE_VAULT("alloc prev node with key\n");
    assert(inserted_node);

    if (vault->curr_count > vault->curr_size / 2)
        necro_vault_grow(vault);

    size_t          hash  = necro_hash_key(inserted_node->key);
    size_t          index = hash & (vault->curr_size - 1);
    NecroVaultNode* prev  = NULL;
    NecroVaultNode* node  = NULL;
    NecroVaultNode* next  = NULL;
    inserted_node->next   = NULL;

    if (!vault->curr_buckets[index].has_nodes)
    {
        TRACE_VAULT("inserting prev node into empty bucket\n");
        vault->curr_buckets[index].head      = inserted_node;
        vault->curr_buckets[index].has_nodes = true;
        vault->curr_count++;
        return;
    }

    node = vault->curr_buckets[index].head;
    while (node != NULL)
    {
        next = node->next;
        if (necro_vault_compare_key(node->key, inserted_node->key))
        {
            inserted_node->next = next;
            if (prev)
            {
                TRACE_VAULT("inserting prev node by replacing existing node with same key\n");
                prev->next = inserted_node;
                // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                necro_vault_free_node(vault->slab_allocator, node);
            }
            else
            {
                TRACE_VAULT("inserting prev node by replacing existing HEAD node with same key\n");
                assert(node == vault->curr_buckets[index].head);
                vault->curr_buckets[index].head = inserted_node;
                assert(inserted_node->next == NULL);
                // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                necro_vault_free_node(vault->slab_allocator, node);
            }
            return;
        }
        else
        {
            if (necro_vault_lazy_gc(node, curr_epoch))
            {
                inserted_node->next = next;
                if (prev)
                {
                    TRACE_VAULT("inserting prev node by replacing existing retired node\n");
                    prev->next = inserted_node;
                    // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                    necro_vault_free_node(vault->slab_allocator, node);
                }
                else
                {
                    TRACE_VAULT("inserting prev node by replacing existing retired HEAD node\n");
                    assert(node == vault->curr_buckets[index].head);
                    vault->curr_buckets[index].head = inserted_node;
                    assert(inserted_node->next == NULL);
                    // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                    necro_vault_free_node(vault->slab_allocator, node);
                }
                return;
            }
            prev = node;
            node = next;
        }
    }

    TRACE_VAULT("inserting prev node at end of vault\n");
    assert(prev != NULL);
    prev->next = inserted_node;
    vault->curr_count++;
}

void* necro_vault_alloc(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size)
{
    assert(vault != NULL);
    assert(vault->slab_allocator != NULL);
    assert(vault->curr_buckets != NULL);

    necro_vault_incremental_gc(vault, curr_epoch);

    // Lazy move all nodes in a bucket into curr_buckets
    for (size_t i = 0; i < 2; ++i)
    {
        if (vault->prev_buckets != NULL)
        {
            TRACE_VAULT("lazy_move_index: %d, prev_size: %d, prev_count: %d\n", vault->lazy_move_index, vault->prev_size, vault->prev_count);
            assert(vault->lazy_move_index < vault->prev_size);
            assert(vault->prev_size  > 0);
            if (vault->prev_buckets[vault->lazy_move_index].has_nodes)
            {
                NecroVaultNode* node = vault->prev_buckets[vault->lazy_move_index].head;
                NecroVaultNode* next = NULL;
                while (node != NULL)
                {
                    assert(node->last_epoch >= 0);
                    next = node->next;
                    if (necro_vault_lazy_gc(node, curr_epoch))
                    {
                        TRACE_VAULT("lazy gc free node in prev_bucket during lazy move\n");
                        // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                        necro_vault_free_node(vault->slab_allocator, node);
                    }
                    else
                    {
                        necro_vault_insert_prev_node_into_curr_buckets(vault, node, curr_epoch);
                    }
                    vault->prev_count--;
                    node = next;
                    assert(vault->prev_count >= 0);
                }
            }
            vault->prev_buckets[vault->lazy_move_index].has_nodes = false;
            vault->prev_buckets[vault->lazy_move_index].head      = NULL;
            vault->lazy_move_index++;
            if (vault->prev_count <= 0 || vault->lazy_move_index == vault->prev_size)
            {
                necro_vault_destroy_prev_bucket(vault);
            }
        }
    }
    return necro_vault_alloc_with_key(vault, key, curr_epoch, data_size);
}

// naive implementation for now
NecroFindOrAllocResult necro_vault_find_or_alloc(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size)
{
    NecroFindOrAllocResult result;
    result.data = necro_vault_find(vault, key, curr_epoch);
    if (result.data != NULL)
    {
        assert((((NecroVaultNode*)result.data) - 1)->data_size == data_size);
        result.find_or_alloc_type = NECRO_FOUND;
    }
    else
    {
        result.data = necro_vault_alloc(vault, key, curr_epoch, data_size);
        result.find_or_alloc_type = NECRO_ALLOC;
    }
    return result;
}

void necro_vault_incremental_gc(NecroVault* vault, const int32_t curr_epoch)
{
    assert(vault != NULL);
    assert(vault->slab_allocator != NULL);
    assert(vault->curr_buckets != NULL);
    // GC a bucket at a time
    TRACE_VAULT("Incremental GC, incremental_gc_index: %d, curr_size: %d, curr_count: %d\n", vault->incremental_gc_index, vault->curr_size, vault->curr_count);
    if (vault->curr_buckets[vault->incremental_gc_index].has_nodes)
    {
        NecroVaultNode* prev = NULL;
        NecroVaultNode* node = vault->curr_buckets[vault->incremental_gc_index].head;
        NecroVaultNode* next = NULL;
        while (node != NULL)
        {
            assert(node->last_epoch >= 0);
            next = node->next;
            if (necro_vault_lazy_gc(node, curr_epoch))
            {
                TRACE_VAULT("incremental gc free node\n");
#if NECRO_VAULT_DEBUG
                necro_vault_print_node(node);
#endif
                vault->curr_count--;
                if (prev == NULL)
                {
                    assert(node == vault->curr_buckets[vault->incremental_gc_index].head);
                    vault->curr_buckets[vault->incremental_gc_index].head      = NULL;
                    vault->curr_buckets[vault->incremental_gc_index].has_nodes = false;
                    // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                    necro_vault_free_node(vault->slab_allocator, node);
                    break;
                }
                else
                {
                    prev->next = next;
                    // necro_free_slab(vault->slab_allocator, node, sizeof(NecroVaultNode));
                    necro_vault_free_node(vault->slab_allocator, node);
                }
            }
            prev = node;
            node = next;
        }
    }
    vault->incremental_gc_index = (vault->incremental_gc_index + 1) & (vault->curr_size - 1);
}

size_t necro_vault_count(NecroVault* vault)
{
    return vault->prev_count + vault->curr_count;
}

size_t necro_vault_largest_bucket(NecroVault* vault)
{
    size_t largest_count = 0;
    for (size_t i = 0; i < vault->prev_size; ++i)
    {
        size_t curr_count = 0;
        if (!vault->prev_buckets[i].has_nodes)
            continue;
        NecroVaultNode* node = vault->prev_buckets[i].head;
        while (node != NULL)
        {
            curr_count++;
            node = node->next;
        }
        if (curr_count > largest_count)
            largest_count = curr_count;
    }
    for (size_t i = 0; i < vault->curr_size; ++i)
    {
        size_t curr_count = 0;
        if (!vault->curr_buckets[i].has_nodes)
            continue;
        NecroVaultNode* node = vault->curr_buckets[i].head;
        while (node != NULL)
        {
            curr_count++;
            node = node->next;
        }
        if (curr_count > largest_count)
            largest_count = curr_count;
    }
    return largest_count;
}

void necro_vault_print_node(NecroVaultNode* node)
{
    if (node == NULL)
    {
        printf("NULL node\n");
    }
    else if (node->data_size == 8)
    {
        int64_t* data = (int64_t*)(node + 1);
        printf("NecroVaultNode { hash: %d, place: %d, time: %d, last_epoch: %d, data_size: %d, value: %lld }\n", node->hash, node->key.place, node->key.time, node->last_epoch, node->data_size, data[0]);
    }
    else if (node->data_size == 16)
    {
        int64_t* data = (int64_t*)(node + 1);
        printf("NecroVaultNode { hash: %d, place: %d, time: %d, last_epoch: %d, data_size: %d, value1: %lld, value2: %lld }\n", node->hash, node->key.place, node->key.time, node->last_epoch, node->data_size, data[0], data[2]);
    }
}

void necro_vault_print(NecroVault* vault)
{
    printf("NecroVault\n{\n");

    printf("    prev_size:    %d,\n", vault->prev_size);
    printf("    prev_count:   %d,\n", vault->prev_count);
    printf("    prev_data:\n    [\n");
    for (size_t i = 0; i < vault->prev_size; ++i)
    {
        if (!vault->prev_buckets[i].has_nodes)
            continue;
        NecroVaultNode* node = vault->prev_buckets[i].head;
        while (node != NULL)
        {
            if (node == vault->prev_buckets[i].head)
                printf("        [%d] = ", i);
            else
                printf("                  ");
            necro_vault_print_node(node);
            node = node->next;
        }
    }
    printf("    ]\n");

    printf("    curr_size:    %d,\n", vault->curr_size);
    printf("    curr_count:   %d,\n", vault->curr_count);
    printf("    curr_data:\n    [\n");
    for (size_t i = 0; i < vault->curr_size; ++i)
    {
        if (!vault->curr_buckets[i].has_nodes)
            continue;
        NecroVaultNode* node = vault->curr_buckets[i].head;
        while (node != NULL)
        {
            if (node == vault->curr_buckets[i].head)
                printf("        [%d] = ", i);
            else
                printf("                  ");
            necro_vault_print_node(node);
            node = node->next;
        }
    }
    printf("    ]\n");
    printf("}\n\n");
}

//=====================================================
// Testing
//=====================================================
bool necro_vault_test_int(NecroVault* vault, NecroVaultKey key, int32_t epoch, int64_t value, bool print_on_success, bool print_on_failure)
{
    NecroVaultNode* node     = necro_vault_find_node(vault, key, epoch);
    NecroVaultNode* expected = necro_alloc_slab(vault->slab_allocator, sizeof(NecroVaultNode) + 8);
    *expected                = (NecroVaultNode) { .next = NULL, .hash = necro_hash_key(key), .key = key, .last_epoch = epoch, .data_size = 8 };
    int64_t* expected_data   = (int64_t*)(expected + 1);
    *expected_data           = value;

    // Contains Test
    if (node != NULL)
    {
        if (print_on_success)
            puts("NecroVault contains test:      passed");
    }
    else
    {
        if (print_on_failure)
        {
            puts("NecroVault found test:        failed");
            printf("                     found:   NULL\n");
            printf("                  expected:   ");
            necro_vault_print_node(expected);
        }
        necro_free_slab(vault->slab_allocator, expected, sizeof(NecroVaultNode) + 8);
        return false;
    }

    // Node Test
    if (node->data_size = 8, node->hash == necro_hash_key(key) && necro_vault_compare_key(node->key, key) && node->last_epoch == epoch)
    {
        if (print_on_success)
            puts("NecroVault node test:          passed");
    }
    else
    {
        if (print_on_failure)
        {
            puts("NecroVault node test:          failed");
            printf("                      found:   ");
            necro_vault_print_node(node);
            printf("                   expected:   ");
            necro_vault_print_node(expected);
        }
        necro_free_slab(vault->slab_allocator, expected, sizeof(NecroVaultNode) + 8);
        return false;
    }

    // Value test
    int64_t* data = (int64_t*)(node + 1);
    if (data[0] == value)
    {
        if (print_on_success)
            puts("NecroVault data test:          passed");
    }
    else
    {
        if (print_on_failure)
        {
            puts("NecroVault data test:          failed");
            printf("                      found:   ");
            necro_vault_print_node(node);
            printf("                   expected:   ");
            necro_vault_print_node(expected);
        }
        necro_free_slab(vault->slab_allocator, expected, sizeof(NecroVaultNode) + 8);
        return false;
    }

    necro_free_slab(vault->slab_allocator, expected, sizeof(NecroVaultNode) + 8);
    return true;
}

void necro_vault_test()
{
    puts("--------------------------------");
    puts("-- Testing NecroVault");
    puts("--------------------------------\n");

    NecroSlabAllocator slab_allocator = necro_create_slab_allocator(4096);
    NecroVault         vault          = necro_create_vault(&slab_allocator);
    int32_t            curr_epoch     = 1;

    // puts("\n---");
    // necro_vault_print(&vault);
    // puts("");

    // Destroy test
    necro_destroy_vault(&vault);
    if (vault.prev_buckets == NULL && vault.curr_buckets == NULL)
        puts("NecroVault destroy test:       passed");
    else
        puts("NecroVault destroy test:       failed");

    vault = necro_create_vault(&slab_allocator);

    // Test Entry1
    NecroVaultKey key1   = { .place = 1, .time = 1 };
    int64_t*      alloc1 = necro_vault_alloc(&vault, key1, curr_epoch, 8);
    *alloc1              = 7;
    necro_vault_test_int(&vault, key1, curr_epoch, 7, true, true);

    // Test Entry2
    NecroVaultKey key2   = { .place = 2, .time = 1 };
    int64_t*      alloc2 = necro_vault_alloc(&vault, key2, curr_epoch, 8);
    *alloc2              = 254;
    necro_vault_test_int(&vault, key2, curr_epoch, 254, true, true);

    // Test Entry3
    NecroVaultKey key3   = { .place = 3, .time = 1 };
    int64_t*      alloc3 = necro_vault_alloc(&vault, key3, curr_epoch, 8);
    *alloc3              = 122;
    necro_vault_test_int(&vault, key3, curr_epoch, 122, true, true);

    // Test Entry4
    NecroVaultKey key4   = { .place = 4, .time = 1 };
    int64_t*      alloc4 = necro_vault_alloc(&vault, key4, curr_epoch, 8);
    *alloc4              = 1;
    necro_vault_test_int(&vault, key4, curr_epoch, 1, true, true);

    // Double Insert Test
    int64_t* alloc3_again = necro_vault_find(&vault, key3, curr_epoch);
    *alloc3_again         = 66;
    necro_vault_test_int(&vault, key3, curr_epoch, 66, true, true);

    // FindOrAlloc Test
    {
        int64_t                value  = 333;
        NecroVaultKey          key    = { .place = 66, .time = 200 };
        NecroFindOrAllocResult result = necro_vault_find_or_alloc(&vault, key, curr_epoch, 8);
        if (result.find_or_alloc_type == NECRO_ALLOC)
            puts("NecroVault found/alloc alloc:  passed");
        else
            puts("NecroVault found/alloc alloc:  failed");
        *((int64_t*)result.data)      = value;
        result = necro_vault_find_or_alloc(&vault, key, curr_epoch, 8);
        if (result.find_or_alloc_type == NECRO_FOUND)
            puts("NecroVault found/alloc found:  passed");
        else
            puts("NecroVault found/alloc found:  failed");
        necro_vault_test_int(&vault, key, curr_epoch, value, true, true);
    }

    // puts("\n---");
    // necro_vault_print(&vault);
    // puts("");

    necro_destroy_vault(&vault);
    vault = necro_create_vault(&slab_allocator);

    // Many inserts test
    bool test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        int32_t         epoch = 1;
        int64_t         value = (uint8_t)i;
        NecroVaultKey   key   = { .place = 1,.time = i };
        int64_t*        alloc = necro_vault_alloc(&vault, key, epoch, 8);
        *alloc                = value;
        if (!necro_vault_test_int(&vault, key, epoch, value, false, true))
        {
            test_passed = false;
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many inserts test:  passed");
    }

    // Many inserts2 test
    test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        int32_t         epoch = 10;
        int64_t         value = (uint8_t)i + 10;
        NecroVaultKey   key   = { .place = 10,.time = i };
        int64_t*        alloc = necro_vault_alloc(&vault, key, epoch, 8);
        *alloc                = value;
        if (!necro_vault_test_int(&vault, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_vault_print(&vault);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many inserts2 test: passed");
    }

    // Many Lookups test
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        int32_t         epoch = 10;
        int64_t         value = (uint8_t)i + 10;
        NecroVaultKey   key   = { .place = 10,.time = i };
        if (!necro_vault_test_int(&vault, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_vault_print(&vault);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many lookups2 test: passed");
    }

    // MonotonicEpoch test
    test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
    {
        int32_t         epoch = 100 + i;
        int64_t         value = (uint8_t)i + 20;
        NecroVaultKey   key   = { .place = 30,.time = i };
        int64_t*        alloc = necro_vault_alloc(&vault, key, epoch, 8);
        *alloc                = value;
        if (!necro_vault_test_int(&vault, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_vault_print(&vault);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroVault epoch test:         passed");
    }

    // Many inserts3 test
    test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE * 32; ++i)
    {
        int32_t         epoch = 2000000;
        int64_t         value = (uint8_t)i + 10;
        NecroVaultKey   key   = { .place = (i % 400) ,.time = (i % 1111) };
        int64_t*        alloc = necro_vault_alloc(&vault, key, epoch, 8);
        *alloc                = value;
        if (!necro_vault_test_int(&vault, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_vault_print(&vault);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many inserts3 test: passed");
    }

    // printf("vault prev_count: %d, curr_count: %d, total_count: %d, curr_size: %d, largest_bucket: %d", vault.prev_count, vault.curr_count, necro_vault_count(&vault), vault.curr_size, necro_vault_largest_bucket(&vault));
    // puts("\n---");
    // necro_vault_print(&vault);
    // puts("");

    necro_destroy_vault(&vault);
    necro_destroy_slab_allocator(&slab_allocator);
    necro_vault_bench();
}

void necro_vault_bench()
{
    int64_t iterations = 65536 * 4;
    {
        // Slab bench
        NecroSlabAllocator slab_allocator = necro_create_slab_allocator(16384);
        NecroVault         vault          = necro_create_vault(&slab_allocator);

        {
            double start_time = (double)clock() / (double)CLOCKS_PER_SEC;
            for (size_t i = 0; i < iterations; ++i)
            {
                const NecroVaultKey key       = { .place = i * 1000,.time = i };
                const int32_t       epoch     = 1;
                const uint32_t      data_size = 8;
                necro_vault_find_or_alloc(&vault, key, epoch, data_size);
            }
            double  end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
            double  run_time    = end_time - start_time;
            int64_t ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
            puts("");
            printf("Vault find_or_alloc alloc benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n", iterations, run_time, ns_per_iter);
        }

        {
            double start_time = (double)clock() / (double)CLOCKS_PER_SEC;
            for (size_t i = 0; i < iterations; ++i)
            {
                const NecroVaultKey key       = { .place = i * 1000,.time = i };
                const int32_t       epoch     = 1;
                const uint32_t      data_size = 8;
                necro_vault_find_or_alloc(&vault, key, epoch, data_size);
            }
            double  end_time    = (double) clock() / (double) CLOCKS_PER_SEC;
            double  run_time    = end_time - start_time;
            int64_t ns_per_iter = (int64_t) ((run_time / (double)iterations) * 1000000000);
            puts("");
            printf("Vault find_or_alloc find benchmark:\n    iterations:  %lld\n    run_time:    %f\n    ns/iter:     %lld\n", iterations, run_time, ns_per_iter);
        }

        printf("\nvault prev_count: %d, curr_count: %d, total_count: %d, curr_size: %d, largest_bucket: %d", vault.prev_count, vault.curr_count, necro_vault_count(&vault), vault.curr_size, necro_vault_largest_bucket(&vault));
        necro_destroy_vault(&vault);
        necro_destroy_slab_allocator(&slab_allocator);
    }
}

//=====================================================
// NecroArchive
//=====================================================
inline NecroArchiveHashTable necro_create_archive_hash_table(NecroRegionAllocator* region_allocator, size_t table_size, size_t data_size)
{
    NecroArchiveNode* buckets = necro_region_alloc(region_allocator, table_size * sizeof(NecroArchiveNode)); // buckets are raw nodes with no data payload!
    for (size_t bucket = 0; bucket < table_size; ++bucket)
        buckets[bucket] = (NecroArchiveNode) { .next = NULL, .time = 0, .last_epoch = 0 };
    return (NecroArchiveHashTable)
    {
        .buckets          = buckets,
        .size             = table_size,
        .count            = 0,
        .index            = 0,
        .region_allocator = region_allocator,
        .data_size        = data_size,
    };
}

NecroArchive necro_create_archive(NecroRegionAllocator* region_allocator, size_t data_size)
{
    TRACE_VAULT("create archive\n");
    return (NecroArchive)
    {
        .prev_table = (NecroArchiveHashTable) { .buckets = NULL, .size = 0, .count = 0, .index = 0, .region_allocator = NULL, .data_size = 0 },
        .curr_table = necro_create_archive_hash_table(region_allocator, NECRO_ARCHIVE_INITIAL_TABLE_SIZE, data_size),
    };
}

inline void necro_destroy_archive_hash_table(NecroArchiveHashTable* hash_table)
{
    assert(hash_table != NULL);
    assert(hash_table->buckets != NULL);
    assert(hash_table->region_allocator != NULL);
    necro_region_free(hash_table->region_allocator, hash_table->buckets);
    *hash_table = (NecroArchiveHashTable)
    {
        .buckets          = NULL,
        .size             = 0,
        .count            = 0,
        .index            = 0,
        .region_allocator = NULL,
        .data_size        = 0
    };
}

void necro_destroy_archive(NecroArchive* archive)
{
    necro_destroy_archive_hash_table(&archive->prev_table);
    necro_destroy_archive_hash_table(&archive->curr_table);
}

inline void necro_archive_grow(NecroArchive* archive)
{
    TRACE_VAULT("grow\n");
    assert(archive->prev_table.count == 0);
    if (archive->prev_table.buckets != NULL)
        necro_destroy_archive_hash_table(&archive->prev_table);
    assert(archive->prev_table.size == 0);
    assert(archive->prev_table.buckets == NULL);
    // Swap curr -> prev
    archive->prev_table.buckets          = archive->curr_table.buckets;
    archive->prev_table.size             = archive->curr_table.size;
    archive->prev_table.count            = archive->curr_table.count;
    archive->prev_table.index            = 0;
    archive->prev_table.region_allocator = archive->curr_table.region_allocator;
    // Make new curr table
    archive->curr_table.size            *= 2;
    archive->curr_table.count            = 0;
    archive->curr_table.index            = 0;
    archive->curr_table.buckets          = necro_region_alloc(archive->curr_table.region_allocator, archive->curr_table.size * sizeof(NecroArchiveNode));
}

inline size_t necro_hash_time(const int32_t time)
{
    return time * NECRO_VAULT_HASH_MULTIPLIER;
}

inline bool necro_archive_lazy_gc(NecroArchiveNode* node, const int32_t curr_epoch)
{
    if (node->last_epoch >= curr_epoch)
        return false;
    int32_t epoch_difference = curr_epoch - node->last_epoch;
    node->last_epoch         = curr_epoch;
    TRACE_VAULT("Lazy gc, curr_epoch: %d, difference: %d\n", curr_epoch, epoch_difference);
    if (epoch_difference >= NECRO_VAULT_RETIREMENT_AGE)
    {
        TRACE_VAULT("Lazy gc, retire!\n");
        return true;
    }
    return false;
}

inline void necro_archive_hash_table_incremental_gc(NecroArchiveHashTable* hash_table, const int32_t curr_epoch)
{
    assert(hash_table != NULL);
    assert(hash_table->region_allocator != NULL);
    assert(hash_table->buckets != NULL);
    // GC a bucket at a time
    TRACE_VAULT("Incremental GC, incremental_gc_index: %d, curr_size: %d, curr_count: %d\n", hash_table->index, hash_table->index, hash_table->count);
    NecroArchiveNode* prev = hash_table->buckets + hash_table->index;
    NecroArchiveNode* node = prev->next;
    NecroArchiveNode* next = NULL;
    while (node != NULL)
    {
        assert(node->last_epoch >= 0);
        next = node->next;
        if (necro_archive_lazy_gc(node, curr_epoch))
        {
            TRACE_VAULT("incremental gc free node\n");
#if NECRO_VAULT_DEBUG
            // necro_vault_print_node(node);
#endif
            hash_table->count--;
            necro_region_free(hash_table->region_allocator, node);

        }
        prev = node;
        node = next;
    }
    hash_table->index = (hash_table->index + 1) & (hash_table->size - 1);
}

void necro_archive_incremental_gc(NecroArchive* archive, const int32_t curr_epoch)
{
    necro_archive_hash_table_incremental_gc(&archive->curr_table, curr_epoch);
}

inline NecroArchiveNode* necro_archive_hash_table_find(NecroArchiveHashTable* hash_table, const int32_t time, const int32_t curr_epoch, const int32_t hash)
{
    assert(hash_table != NULL);
    TRACE_VAULT("find node in hash_table\n");
    if (hash_table->buckets == NULL)
        return NULL;
    const size_t      index = hash & (hash_table->size - 1);
    NecroArchiveNode* prev  = hash_table->buckets + index;
    NecroArchiveNode* node  = prev->next;
    NecroArchiveNode* next  = NULL;
    while (node != NULL)
    {
        next = node->next;
        if (node->time == time)
        {
            node->last_epoch = curr_epoch;
            TRACE_VAULT("found node in buckets\n");
            return node;
        }
        else if (necro_archive_lazy_gc(node, curr_epoch))
        {
            necro_region_free(hash_table->region_allocator, node);
            hash_table->count--;
            prev->next = next;
            node       = next;
        }
        else
        {
            prev = node;
            node = next;
        }
    }
    return NULL;
}

inline NecroFindOrAllocResult necro_archive_hash_table_find_or_alloc(NecroArchiveHashTable* hash_table, const int32_t time, const int32_t curr_epoch, const int32_t hash)
{
    TRACE_VAULT("necro_archive_hash_table_find_or_alloc\n");
    assert(hash_table != NULL);
    assert(hash_table->buckets != NULL);
    const size_t      index = hash & (hash_table->size - 1);
    NecroArchiveNode* prev  = hash_table->buckets + index;
    NecroArchiveNode* node  = prev->next;
    NecroArchiveNode* next  = NULL;
    while (node != NULL)
    {
        next = node->next;
        if (node->time == time)
        {
            node->last_epoch = curr_epoch;
            TRACE_VAULT("Found node in buckets\n");
            return (NecroFindOrAllocResult) { .find_or_alloc_type = NECRO_FOUND,.data = node + 1 };
        }
        else if (necro_archive_lazy_gc(node, curr_epoch))
        {
            // Alloc into existing!
            node->time       = time;
            node->last_epoch = curr_epoch;
            TRACE_VAULT("Alloc into gc'd node\n");
            return (NecroFindOrAllocResult) { .find_or_alloc_type = NECRO_ALLOC,.data = node + 1 };
        }
        else
        {
            prev = node;
            node = next;
        }
    }
    node             = necro_region_alloc(hash_table->region_allocator, sizeof(NecroArchiveNode) + hash_table->data_size);
    node->time       = time;
    node->last_epoch = curr_epoch;
    node->next       = NULL;
    prev->next       = node;
    TRACE_VAULT("Alloc new at end\n");
    return (NecroFindOrAllocResult) { .find_or_alloc_type = NECRO_ALLOC, .data = node + 1 };

}

inline void necro_archive_insert_prev_node_into_curr_table(NecroArchiveHashTable* hash_table, NecroArchiveNode* prev_node, const int32_t curr_epoch)
{
    TRACE_VAULT("Insert prev into curr_table\n");
    assert(hash_table != NULL);
    assert(hash_table->buckets != NULL);

    // Should always be done moving nodes from previous table to curr table before we need to grow again
    assert(hash_table->count < hash_table->size >> 1);

    const size_t      hash  = necro_hash_time(prev_node->time);
    const size_t      index = hash & (hash_table->size - 1);
    NecroArchiveNode* prev  = hash_table->buckets + index;
    NecroArchiveNode* node  = prev->next;
    NecroArchiveNode* next  = NULL;
    while (node != NULL)
    {
        next = node->next;
        assert(node->time != prev_node->time); // There should be no duplicate values at a specific time!
        if (necro_archive_lazy_gc(node, curr_epoch))
        {
            TRACE_VAULT("Swapped prev_node for gc'd node\n");
            prev_node->last_epoch = curr_epoch;
            prev_node->next       = next;
            prev->next            = prev_node;
            necro_region_free(hash_table->region_allocator, node);
            return;
        }
        else
        {
            prev = node;
            node = next;
        }
    }
    TRACE_VAULT("Insert prev_node at end of chain.\n");
    prev_node->last_epoch = curr_epoch;
    prev_node->next       = NULL;
    prev->next            = prev_node;
}

inline void necro_archive_lazy_move(NecroArchive* archive, const int32_t curr_epoch)
{
    // Lazy move all nodes in a bucket into curr_buckets
    if (archive->prev_table.buckets != NULL)
    {
        TRACE_VAULT("lazy_move, index: %d, prev_size: %d, prev_count: %d\n", archive->prev_table.index, archive->prev_table.size, archive->prev_table.count);
        assert(archive->prev_table.index < archive->prev_table.size);
        assert(archive->prev_table.size  > 0);
        NecroArchiveNode* prev = archive->prev_table.buckets + archive->prev_table.index;
        NecroArchiveNode* node = prev->next;
        NecroArchiveNode* next = NULL;
        while (node != NULL)
        {
            assert(node->last_epoch >= 0);
            next = node->next;
            if (necro_archive_lazy_gc(node, curr_epoch))
            {
                TRACE_VAULT("lazy gc free node in prev_table during lazy move\n");
                necro_region_free(archive->prev_table.region_allocator, node);
            }
            else
            {
                necro_archive_insert_prev_node_into_curr_table(&archive->curr_table, node, curr_epoch);
            }
            archive->prev_table.count--;
            node = next;
            assert(archive->prev_table.count >= 0);
        }
        archive->prev_table.buckets[archive->prev_table.index].next = NULL;
        archive->prev_table.index++;
        if (archive->prev_table.count <= 0 || archive->prev_table.index >= archive->prev_table.size)
        {
            necro_destroy_archive_hash_table(&archive->prev_table);
        }
    }
}

// void* necro_archive_find(NecroArchive* archive, const int32_t time, const int32_t curr_epoch)
// {
//     assert(archive != NULL);
//     TRACE_VAULT("find\n");

//     const size_t hash  = necro_hash_time(time);
//     // necro_vault_incremental_gc(vault, curr_epoch);

//     if ((hash & (archive->prev_table.size - 1)) >= archive->prev_table.index)
//     {
//         NecroArchiveNode* prev_node = necro_archive_hash_table_find(&archive->prev_table, time, curr_epoch, hash);
//         if (prev_node != NULL)
//             return prev_node + 1;
//     }

//     NecroArchiveNode* curr_node = necro_archive_hash_table_find(&archive->curr_table, time, curr_epoch, hash);
//     if (curr_node != NULL)
//         return curr_node + 1;

//     TRACE_VAULT("key not found in table\n");
//     return NULL;
// }

NecroFindOrAllocResult necro_archive_find_or_alloc(NecroArchive* archive, const int32_t time, const int32_t curr_epoch)
{
    assert(archive != NULL);
    TRACE_VAULT("find\n");
    const size_t hash  = necro_hash_time(time);
    // necro_vault_incremental_gc(vault, curr_epoch);
    if ((hash & (archive->prev_table.size - 1)) >= archive->prev_table.index)
    {
        NecroArchiveNode* prev_node = necro_archive_hash_table_find(&archive->prev_table, time, curr_epoch, hash);
        if (prev_node != NULL)
            return (NecroFindOrAllocResult) { .find_or_alloc_type = NECRO_FOUND, prev_node + 1 };
    }
    necro_archive_incremental_gc(archive, curr_epoch);
    if (archive->curr_table.count > archive->curr_table.size / 2)
        necro_archive_grow(archive);
    necro_archive_lazy_move(archive, curr_epoch);
    necro_archive_lazy_move(archive, curr_epoch);
    return necro_archive_hash_table_find_or_alloc(&archive->curr_table, time, curr_epoch, hash);
}
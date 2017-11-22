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

NecroVault necro_create_vault()
{
    TRACE_VAULT("create vault\n");

    NecroVaultEntry* entries = malloc(NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultEntry));
    if (entries == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating entries in necro_create_intern()\n");
        exit(1);
    }
    memset(entries, 0, NECRO_VAULT_INITIAL_SIZE * sizeof(NecroVaultEntry));

    return (NecroVault)
    {
        NULL,
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
    TRACE_VAULT("destroy vault\n");
    free(vault->prev_entries);
    vault->prev_entries    = NULL;
    vault->prev_size       = 0;
    vault->prev_count      = 0;
    free(vault->curr_entries);
    vault->curr_entries    = NULL;
    vault->curr_size       = 0;
    vault->curr_count      = 0;
    vault->lazy_move_index = 0;
}

inline bool necro_vault_compare_key(NecroVaultKey key1, NecroVaultKey key2)
{
    return key1.place == key2.place && key1.time == key2.time && key1.universe == key2.universe;
}

inline size_t necro_hash_key(const NecroVaultKey key)
{
    size_t hash = key.time;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    hash       += key.place;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
    hash       += key.universe;
    hash       *= NECRO_VAULT_HASH_MULTIPLIER;
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

void necro_vault_grow(NecroVault* vault)
{
    TRACE_VAULT("grow\n");
    assert(vault->prev_entries == NULL);
    assert(vault->prev_size == 0);
    assert(vault->prev_count == 0);
    vault->prev_entries    = vault->curr_entries;
    vault->prev_size       = vault->curr_size;
    vault->prev_count      = vault->curr_count;
    vault->curr_size       = vault->prev_size * 2;
    vault->curr_count      = 0;
    vault->lazy_move_index = 0;
    vault->curr_entries    = calloc(vault->curr_size, sizeof(NecroVaultEntry));
    if (vault->curr_entries == NULL)
    {
        fprintf(stderr, "Malloc returned null in necro_vault_grow()\n");
        exit(1);
    }
}

inline bool is_circularly_between(size_t a, size_t b, size_t c)
{
    if (a <= c)
        return a <= b && b < c;
    else
        return a <= b || b < c;
}

inline void necro_vault_delete_entry(NecroVaultEntry* table, const size_t table_size, size_t index)
{
    TRACE_VAULT("delete entry at index: %d\n", index);
    assert(table != NULL);
    size_t to   = index;
    size_t from = index;
    while (true)
    {
        from = (from + 1) & (table_size - 1);
        if (table[from].key.place == NECRO_VAULT_NULL_PLACE)
            break;
        size_t h = table[from].hash & (table_size - 1);
        if (is_circularly_between(h, to, from))
        {
            table[to] = table[from];
            to        = from;
        }
    }
    table[to] = (NecroVaultEntry) { .int_data = 0, .hash = 0, .key = { .place = 0,.universe = 0,.time = 0 }, .last_epoch = 0, .retirement_age = 0 };
}

inline bool necro_vault_lazy_gc(NecroVaultEntry* table, const size_t table_size, size_t* count, size_t index, NecroVaultEntry* entry, const int32_t curr_epoch)
{
    // TRACE_VAULT("Lazy gc, curr_epoch: %d, entry.last_epoch: %d\n", curr_epoch, entry->last_epoch);
    if (entry->last_epoch >= curr_epoch)
        return false;
    // TRACE_VAULT("Lazy gc, curr_epoch: %d, entry.last_epoch: %d, entry.retirement_age: %d\n", curr_epoch, entry->last_epoch, entry->retirement_age);
    entry->retirement_age -= curr_epoch - entry->last_epoch;
    entry->last_epoch      = curr_epoch;
    // TRACE_VAULT("Lazy gc, new last_epoch: %d, new retirement_age: %d\n", entry->last_epoch, entry->retirement_age);
    if (entry->retirement_age <= 0)
    {
        // TRACE_VAULT("Lazy gc, delete\n");
        necro_vault_delete_entry(table, table_size, index);
        *count = *count - 1;
        return true;
    }
    return false;
}

inline NecroVaultEntry* necro_vault_find_entry_in_table(NecroVaultEntry* table, const size_t table_size, size_t* count, size_t hash, const NecroVaultKey key, const int32_t curr_epoch)
{
    // TRACE_VAULT("find entry in table\n");
    if (table == NULL)
        return NULL;
    uint32_t gc_count = NECRO_VAULT_GC_COUNT;
    for (size_t probe = hash & (table_size - 1); table[probe].key.place != NECRO_VAULT_NULL_PLACE; probe = (probe + 1) & (table_size - 1))
    {
        NecroVaultEntry* entry = table + probe;
        // found matching entry
        if (necro_vault_compare_key(entry->key, key))
        {
            if (entry->last_epoch < curr_epoch)
            {
                entry->last_epoch     = curr_epoch;
                entry->retirement_age = NECRO_VAULT_INITIAL_AGE;
                TRACE_VAULT("Bumped retirement age\n");
            }
            // TRACE_VAULT("found entry in table\n");
            return entry;
        }
        // Perform lazy GC
        else if (gc_count != 0 && necro_vault_lazy_gc(table, table_size, count, probe, entry, curr_epoch))
        {
            gc_count--;
            probe = (probe - 1) & (table_size - 1);
        }
    }
    return NULL;
}

int64_t* necro_vault_find_int(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch)
{
    // TRACE_VAULT("find int\n");
    if (key.place == NECRO_VAULT_NULL_PLACE)
        return NULL;
    const size_t hash = necro_hash_key(key);

    NecroVaultEntry* prev_entry = necro_vault_find_entry_in_table(vault->prev_entries, vault->prev_size, &vault->prev_count, hash, key, curr_epoch);
    if (prev_entry != NULL)
        return &prev_entry->int_data;

    NecroVaultEntry* curr_entry = necro_vault_find_entry_in_table(vault->curr_entries, vault->curr_size, &vault->curr_count, hash, key, curr_epoch);
    if (curr_entry != NULL)
        return &curr_entry->int_data;

    // TRACE_VAULT("entry not found in table\n");
    return NULL;
}

NecroVaultEntry* necro_vault_find_entry(NecroVault* vault, const NecroVaultKey key, const int32_t curr_epoch)
{
    // TRACE_VAULT("find entry\n");
    if (key.place == NECRO_VAULT_NULL_PLACE)
        return NULL;
    const size_t hash = necro_hash_key(key);

    NecroVaultEntry* prev_entry = necro_vault_find_entry_in_table(vault->prev_entries, vault->prev_size, &vault->prev_count, hash, key, curr_epoch);
    if (prev_entry != NULL)
        return prev_entry;

    NecroVaultEntry* curr_entry = necro_vault_find_entry_in_table(vault->curr_entries, vault->curr_size, &vault->curr_count, hash, key, curr_epoch);
    if (curr_entry != NULL)
        return curr_entry;

    return NULL;
}

inline void necro_vault_insert_entry(NecroVault* vault, const NecroVaultEntry a_entry, const int32_t curr_epoch)
{
    // TRACE_VAULT("insert entry\n");
    if (a_entry.key.place == NECRO_VAULT_NULL_PLACE)
        return;
    // Grow if we're over 50% load
    if (vault->curr_count >= vault->curr_size / 2)
        necro_vault_grow(vault);
    size_t gc_count = NECRO_VAULT_GC_COUNT;
    for (size_t probe = a_entry.hash & (vault->curr_size - 1); true; probe = (probe + 1) & (vault->curr_size - 1))
    {
        // if (probe == NECRO_VAULT_NULL_ID)
        //     continue;
        NecroVaultEntry* entry = vault->curr_entries + probe;
        if (entry->key.place == NECRO_VAULT_NULL_PLACE)
        {
            vault->curr_count         += 1;
            vault->curr_entries[probe] = a_entry;
            TRACE_VAULT("insert into null, count: %d\n", vault->curr_count);
            break;
        }
        else if (entry->hash == a_entry.hash && necro_vault_compare_key(entry->key, a_entry.key))
        {
            vault->curr_entries[probe] = a_entry;
            TRACE_VAULT("insert at key, count: %d\n", vault->curr_count);
            break;
        }
        // Perform lazy GC
        else if (gc_count != 0 && necro_vault_lazy_gc(vault->curr_entries, vault->curr_size, &vault->curr_count, probe, entry, curr_epoch))
        {
            TRACE_VAULT("perform lazy gc\n");
            gc_count--;
            // probe--;
            probe = (probe - 1) & (vault->curr_size - 1);
        }
        else
        {
            TRACE_VAULT("other, probe value: %d\n", probe);
        }
    }
}

inline void necro_vault_insert_entry_and_lazy_move(NecroVault* vault, const NecroVaultEntry a_entry, const int32_t curr_epoch)
{
    // TRACE_VAULT("insert entry and lazy move\n");
    assert(vault);
    assert(vault->curr_entries);

    // Perform lazy move
    for (size_t i = 0; i < 2; ++ i)
    {
        if (vault->lazy_move_index < vault->prev_size && vault->prev_count > 0 && vault->prev_entries)
        {
            NecroVaultEntry* entry = vault->prev_entries + vault->lazy_move_index;
            if (entry->key.place != NECRO_VAULT_NULL_PLACE)
            {
                TRACE_VAULT("Lazy move\n");
                necro_vault_insert_entry(vault, *entry, curr_epoch);
                necro_vault_delete_entry(vault->prev_entries, vault->prev_size, vault->lazy_move_index);
                vault->prev_count--;
                if (vault->prev_count == 0)
                {
                    free(vault->prev_entries);
                    vault->prev_entries = NULL;
                    vault->prev_size    = 0;
                }
            }
            vault->lazy_move_index++;
        }
    }

    necro_vault_insert_entry(vault, a_entry, curr_epoch);
    assert(vault->prev_count <= vault->prev_size / 2);
    assert(vault->curr_count <= vault->curr_size / 2);
}

void necro_vault_insert_int(NecroVault* vault, const NecroVaultKey key, const int64_t value, const int32_t curr_epoch)
{
    // TRACE_VAULT("insert int\n");
    necro_vault_insert_entry_and_lazy_move(vault, (NecroVaultEntry) { .int_data = value, .hash = necro_hash_key(key), .key = key, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);
}


size_t necro_vault_count(NecroVault* vault)
{
    return vault->prev_count + vault->curr_count;
}

void necro_vault_print_entry(NecroVaultEntry* entry)
{
    if (entry == NULL)
        printf("NULL entry\n");
    else
        printf("NecroEntry { hash: %d, place: %d, universe: %d, time: %d, value: %lld, last_epoch: %d, retirement_age: %d }\n", entry->hash, entry->key.place, entry->key.universe, entry->key.time, entry->int_data, entry->last_epoch, entry->retirement_age);
}

void necro_vault_print(const NecroVault* vault)
{
    printf("NecroVault\n{\n");

    printf("    prev_size:    %d,\n", vault->prev_size);
    printf("    prev_count:   %d,\n", vault->prev_count);
    printf("    prev_data:\n    [\n");
    for (size_t i = 0; i < vault->prev_size; ++i)
    {
        NecroVaultEntry* entry = vault->prev_entries + i;
        if (entry->key.place == NECRO_VAULT_NULL_PLACE)
            continue;
        printf("        [%d] = ", i);
        necro_vault_print_entry(entry);
    }
    printf("    ]\n");

    printf("    curr_size:    %d,\n", vault->curr_size);
    printf("    curr_count:   %d,\n", vault->curr_count);
    printf("    curr_data:\n    [\n");
    for (size_t i = 0; i < vault->curr_size; ++i)
    {
        NecroVaultEntry* entry = vault->curr_entries + i;
        if (entry->key.place == NECRO_VAULT_NULL_PLACE)
            continue;
        printf("        [%d] = ", i);
        necro_vault_print_entry(entry);
    }
    printf("    ]\n");
    printf("}\n\n");
}

//=====================================================
// Testing
//=====================================================
void necro_test_vault_entry(NecroVault* vault, NecroVaultEntry a_entry, const int32_t curr_epoch)
{
    printf("----\nentry: ");
    necro_vault_print_entry(&a_entry);

    // NULL_ID Test
    if (a_entry.key.place != NECRO_VAULT_NULL_PLACE)
        puts("NecroVault null test:      passed");
    else
        puts("NecroVault null test:      failed");

    NecroVaultEntry* entry = necro_vault_find_entry(vault, a_entry.key, curr_epoch);

    // Contains Test
    if (entry != NULL)
        puts("NecroVault contains test: passed");
    else
        puts("NecroVault contains test: failed");

    // Equals Test
    if (entry->int_data == a_entry.int_data && entry->hash == a_entry.hash && necro_vault_compare_key(entry->key, a_entry.key) && entry->last_epoch == a_entry.last_epoch && entry->retirement_age == a_entry.retirement_age)
    {
        puts("NecroVault equals test:   passed");
    }
    else
    {
        puts("NecroVault equals test:   failed");
        printf("                          ");
        necro_vault_print_entry(entry);
    }
}

void necro_vault_test()
{
    puts("--------------------------------");
    puts("-- Testing NecroVault");
    puts("--------------------------------\n");

    NecroVault vault      = necro_create_vault();
    int32_t    curr_epoch = 1;

    // Test Entry1
    NecroVaultKey key1 = { .place = 1, .universe = 0, .time = 1 };
    necro_vault_insert_int(&vault, key1, 666, curr_epoch);
    necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 666, .hash = necro_hash_key(key1), .key = key1, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

    // Test Entry2
    NecroVaultKey key2 = { .place = 2, .universe = 0, .time = 1 };
    necro_vault_insert_int(&vault, key2, 1000, curr_epoch);
    necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 1000, .hash = necro_hash_key(key2), .key = key2, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

    // Test Entry3
    NecroVaultKey key3 = { .place = 3, .universe = 1, .time = 2 };
    necro_vault_insert_int(&vault, key3, 300, curr_epoch);
    necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 300, .hash = necro_hash_key(key3), .key = key3, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

    // Test Entry4
    NecroVaultKey key4 = { .place = 4, .universe = 100, .time = 22 };
    necro_vault_insert_int(&vault, key4, 44, curr_epoch);
    necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = 44, .hash = necro_hash_key(key4), .key = key4, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

    // Double Insert Test
    necro_vault_insert_int(&vault, key3, -1, curr_epoch);
    necro_test_vault_entry(&vault, (NecroVaultEntry) { .int_data = -1, .hash = necro_hash_key(key3), .key = key3, .last_epoch = curr_epoch, .retirement_age = NECRO_VAULT_INITIAL_AGE }, curr_epoch);

    // puts("\n---");
    // necro_vault_print(&vault);
    // puts("");

    // Destroy test
    necro_destroy_vault(&vault);
    if (vault.prev_entries == NULL && vault.curr_entries == NULL)
        puts("NecroVault destroy test:    passed");
    else
        puts("NecroVault destroy test:    failed");

    // Many inserts test
    puts("---\nMany Inserts test");
    vault = necro_create_vault();
    bool test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        NecroVaultKey    key   = { .place = 1, .universe = 0, .time = i };
        necro_vault_insert_int(&vault, key, i, 1);
        NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 1);
        if (entry == NULL || entry->int_data != i || !necro_vault_compare_key(entry->key, key))
        {
            puts("NecroVault many inserts test:   failed");
            printf("                                ");
            necro_vault_print_entry(entry);
            test_passed = false;
            break;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many inserts test:   passed");
    }

    // Many Re-inserts test
    test_passed = true;
    puts("---\nMany Re-Inserts test");
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        NecroVaultKey    key   = { .place = 10, .universe = 0, .time = i };
        necro_vault_insert_int(&vault, key, i + 10, 10);
        NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 10);
        if (entry == NULL || entry->int_data != i + 10 || !necro_vault_compare_key(entry->key, key))
        {
            puts("NecroVault re-inserts test:     failed");
            printf("                                ");
            necro_vault_print_entry(entry);
            printf("Expected entry:\n");
            printf("                                ");
            necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 10, .hash = necro_hash_key(key), .key = key, .last_epoch = 10, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
            // necro_vault_print(&vault);
            test_passed = false;
            break;
        }
    }
    if (test_passed)
    {
        puts("NecroVault re-inserts test:      passed");
        // necro_vault_print(&vault);
    }

    // Many Lookups test
    test_passed = true;
    puts("---\nMany Lookups test");
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        NecroVaultKey    key   = { .place = 10, .universe = 0, .time = i };
        NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 10);
        if (entry == NULL || entry->int_data != i + 10 || !necro_vault_compare_key(entry->key, key))
        {
            // necro_vault_print(&vault);
            puts("NecroVault many lookups test:     failed");
            printf("                                ");
            necro_vault_print_entry(entry);
            printf("Expected entry:\n");
            printf("                                ");
            necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 10, .hash = necro_hash_key(key), .key = key, .last_epoch = 10, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
            test_passed = false;
            break;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many lookups test:      passed");
    }

    // Many Re-inserts2 test
    test_passed = true;
    puts("---\nMany Re-Inserts2 test");
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
    {
        NecroVaultKey    key   = { .place = 20, .universe = 0, .time = i };
        necro_vault_insert_int(&vault, key, i + 20, 20);
        NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 20);
        if (entry == NULL || entry->int_data != i + 20 || !necro_vault_compare_key(entry->key, key))
        {
            puts("NecroVault re-inserts2 test:     failed");
            printf("                                ");
            necro_vault_print_entry(entry);
            printf("Expected entry:\n");
            printf("                                ");
            necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 20, .hash = necro_hash_key(key), .key = key, .last_epoch = 20, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
            // necro_vault_print(&vault);
            test_passed = false;
            break;
        }
    }
    if (test_passed)
    {
        puts("NecroVault re-inserts2 test:      passed");
        // necro_vault_print(&vault);
    }

    // Many Lookups2 test
    test_passed = true;
    puts("---\nMany Lookups2 test");
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
    {
        NecroVaultKey    key   = { .place = 20, .universe = 0, .time = i };
        NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 20);
        if (entry == NULL || entry->int_data != i + 20 || !necro_vault_compare_key(entry->key, key))
        {
            necro_vault_print(&vault);
            puts("NecroVault many lookups2 test:     failed");
            printf("                                ");
            necro_vault_print_entry(entry);
            printf("Expected entry:\n");
            printf("                                ");
            necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 20, .hash = necro_hash_key(key), .key = key, .last_epoch = 20, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
            test_passed = false;
            break;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many lookups2 test:      passed");
    }

    // MonotonicEpoch test
    test_passed = true;
    puts("---\nMonotonicEpoch test");
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE * 6; ++i)
    {
        NecroVaultKey    key   = { .place = 30, .universe = 0, .time = i };
        necro_vault_insert_int(&vault, key, i + 30, 100 + i);
        NecroVaultEntry* entry = necro_vault_find_entry(&vault, key, 100 + i);
        if (entry == NULL || entry->int_data != i + 30 || !necro_vault_compare_key(entry->key, key))
        {
            necro_vault_print(&vault);
            puts("NecroVault many MonotonicEpoch test:     failed");
            printf("                                ");
            necro_vault_print_entry(entry);
            printf("Expected entry:\n");
            printf("                                ");
            necro_vault_print_entry(&((NecroVaultEntry) { .int_data = i + 30, .hash = necro_hash_key(key), .key = key, .last_epoch = 100 + i, .retirement_age = NECRO_VAULT_INITIAL_AGE }));
            test_passed = false;
            break;
        }
    }
    if (test_passed)
    {
        puts("NecroVault many MonotonicEpoch test:      passed");
    }

    printf("NecroVault size: %d, count: %d\n", vault.curr_size, necro_vault_count(&vault));
}

//=====================================================
// NecroChain
//=====================================================
NecroChain necro_create_chain(NecroSlabAllocator* slab_allocator)
{
    TRACE_VAULT("create chain\n");

    NecroChainBucket* curr_buckets = malloc(NECRO_VAULT_INITIAL_SIZE * sizeof(NecroChainBucket));
    if (curr_buckets == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating entries in necro_create_chain()\n");
        exit(1);
    }
    memset(curr_buckets, 0, NECRO_VAULT_INITIAL_SIZE * sizeof(NecroChainBucket));

    return (NecroChain)
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

inline NecroChainNode* necro_chain_alloc_node(NecroSlabAllocator* slab_allocator, const size_t hash, const NecroVaultKey key, const int32_t curr_epoch, NecroChainNode* next, const size_t data_size)
{
    NecroChainNode* node = necro_alloc_slab(slab_allocator, sizeof(NecroChainNode) + data_size);
    node->next           = next;
    node->key            = key;
    node->hash           = hash;
    node->last_epoch     = curr_epoch;
    node->data_size      = data_size;
    return node;
}

inline void necro_chain_free_node(NecroSlabAllocator* slab_allocator, NecroChainNode* node)
{
    necro_free_slab(slab_allocator, node, sizeof(NecroChainNode) + node->data_size);
}

inline void necro_chain_destroy_buckets(NecroChainBucket* buckets, size_t size, NecroSlabAllocator* slab_allocator)
{
    TRACE_VAULT("destroy buckets\n");
    for (size_t i = 0; i < size; ++i)
    {
        if (!buckets[i].has_nodes)
            continue;
        NecroChainNode* node = buckets[i].head;
        while (node != NULL)
        {
            NecroChainNode* next = node->next;
            // necro_free_slab(slab_allocator, node, sizeof(NecroChainNode));
            necro_chain_free_node(slab_allocator, node);
            node = next;
        }
    }
}

void necro_destroy_chain(NecroChain* chain)
{
    TRACE_VAULT("destroy chain\n");
    necro_chain_destroy_buckets(chain->prev_buckets, chain->prev_size, chain->slab_allocator);
    free(chain->prev_buckets);
    chain->prev_buckets         = NULL;
    chain->prev_size            = 0;
    chain->prev_count           = 0;
    necro_chain_destroy_buckets(chain->curr_buckets, chain->curr_size, chain->slab_allocator);
    free(chain->curr_buckets);
    chain->curr_buckets         = NULL;
    chain->curr_size            = 0;
    chain->curr_count           = 0;
    chain->lazy_move_index      = 0;
    chain->incremental_gc_index = 0;
    chain->slab_allocator       = NULL;
}

void necro_chain_destroy_prev_bucket(NecroChain* chain)
{
    if (chain->prev_buckets == NULL)
        return;
    TRACE_VAULT("Destroy prev_buckets\n");
    necro_chain_destroy_buckets(chain->prev_buckets, chain->prev_size, chain->slab_allocator);
    free(chain->prev_buckets);
    chain->prev_buckets = NULL;
    chain->prev_size    = 0;
    chain->prev_count   = 0;
}

void necro_chain_grow(NecroChain* chain)
{
    TRACE_VAULT("grow\n");
    assert(chain->prev_count == 0);
    necro_chain_destroy_prev_bucket(chain);
    assert(chain->prev_size == 0);
    assert(chain->prev_buckets == NULL);
    chain->prev_buckets         = chain->curr_buckets;
    chain->prev_size            = chain->curr_size;
    chain->prev_count           = chain->curr_count;
    chain->curr_size            = chain->prev_size * 2;
    chain->curr_count           = 0;
    chain->lazy_move_index      = 0;
    chain->incremental_gc_index = 0;
    chain->curr_buckets         = calloc(chain->curr_size, sizeof(NecroChainBucket));
    if (chain->curr_buckets == NULL)
    {
        fprintf(stderr, "Malloc returned null in necro_chain_grow()\n");
        exit(1);
    }
}

inline bool necro_chain_lazy_gc(NecroChainNode* node, const int32_t curr_epoch)
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

inline NecroChainNode* necro_chain_find_node_in_buckets(NecroSlabAllocator* slab_allocator, NecroChainBucket* buckets, const size_t buckets_size, size_t* count, size_t hash, const NecroVaultKey key, const int32_t curr_epoch)
{
    TRACE_VAULT("find node in buckets\n");
    if (buckets == NULL)
        return NULL;

    size_t index = hash & (buckets_size - 1);

    if (!buckets[index].has_nodes)
        return NULL;

    NecroChainNode* prev = NULL;
    NecroChainNode* node = buckets[index].head;
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
            NecroChainNode* next = node->next;
            if (necro_chain_lazy_gc(node, curr_epoch))
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
                // necro_free_slab(slab_allocator, node, sizeof(NecroChainNode));
                necro_chain_free_node(slab_allocator, node);
            }
            prev = node;
            node = next;
        }
    }

    buckets[index].has_nodes = buckets[index].head != NULL;

    return NULL;
}

void* necro_chain_find(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch)
{
    assert(chain != NULL);
    TRACE_VAULT("find\n");

    const size_t hash = necro_hash_key(key);

    necro_chain_incremental_gc(chain, curr_epoch);

    if ((hash & (chain->prev_size - 1)) >= chain->lazy_move_index)
    {
        NecroChainNode* prev_node = necro_chain_find_node_in_buckets(chain->slab_allocator, chain->prev_buckets, chain->prev_size, &chain->prev_count, hash, key, curr_epoch);
        if (prev_node != NULL)
            return (prev_node + 1);
    }

    NecroChainNode* curr_node = necro_chain_find_node_in_buckets(chain->slab_allocator, chain->curr_buckets, chain->curr_size, &chain->curr_count, hash, key, curr_epoch);
    if (curr_node != NULL)
        return (curr_node + 1);

    TRACE_VAULT("key not found in table\n");
    return NULL;
}

NecroChainNode* necro_chain_find_node(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch)
{
    assert(chain != NULL);
    TRACE_VAULT("find node\n");

    const size_t hash = necro_hash_key(key);

    necro_chain_incremental_gc(chain, curr_epoch);

    if ((hash & (chain->prev_size - 1)) >= chain->lazy_move_index)
    {
        NecroChainNode* prev_node = necro_chain_find_node_in_buckets(chain->slab_allocator, chain->prev_buckets, chain->prev_size, &chain->prev_count, hash, key, curr_epoch);
        if (prev_node != NULL)
            return prev_node;
    }

    NecroChainNode* curr_node = necro_chain_find_node_in_buckets(chain->slab_allocator, chain->curr_buckets, chain->curr_size, &chain->curr_count, hash, key, curr_epoch);
    if (curr_node != NULL)
        return curr_node;

    TRACE_VAULT("key not found in table\n");
    return NULL;
}

inline void* necro_chain_alloc_with_key(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size)
{
    TRACE_VAULT("alloc with key\n");
    assert(chain);
    assert(chain->slab_allocator);
    assert(chain->curr_buckets);

    if (chain->curr_count > chain->curr_size / 2)
        necro_chain_grow(chain);

    size_t          hash  = necro_hash_key(key);
    size_t          index = hash & (chain->curr_size - 1);
    NecroChainNode* prev  = NULL;
    NecroChainNode* node  = NULL;
    NecroChainNode* next  = NULL;

    if (!chain->curr_buckets[index].has_nodes)
    {
        TRACE_VAULT("allocating new node into empty bucket\n");
        // node = necro_alloc_slab(chain->slab_allocator, sizeof(NecroChainNode));
        node = necro_chain_alloc_node(chain->slab_allocator, hash, key, curr_epoch, NULL, data_size);
        // necro_chain_initialize_node(node, hash, key, curr_epoch, NULL);
        chain->curr_buckets[index].head      = node;
        chain->curr_buckets[index].has_nodes = true;
        chain->curr_count++;
        return (node + 1);
    }

    // TODO: Redo these where we "swap" nodes, can't do that with mixed data sizes!
    node = chain->curr_buckets[index].head;
    while (node != NULL)
    {
        next = node->next;
        if (necro_vault_compare_key(node->key, key))
        {
            TRACE_VAULT("allocating into existing node with same key\n");
            necro_chain_free_node(chain->slab_allocator, node);
            node = necro_chain_alloc_node(chain->slab_allocator, hash, key, curr_epoch, next, data_size);
            if (prev == NULL)
                chain->curr_buckets[index].head = node;
            else
                prev->next = node;
            // necro_chain_initialize_node(node, hash, key, curr_epoch, next);
            return (node + 1);
        }
        else
        {
            if (necro_chain_lazy_gc(node, curr_epoch))
            {
                TRACE_VAULT("allocating into existing retired node\n");
                necro_chain_free_node(chain->slab_allocator, node);
                node = necro_chain_alloc_node(chain->slab_allocator, hash, key, curr_epoch, next, data_size);
                if (prev == NULL)
                    chain->curr_buckets[index].head = node;
                else
                    prev->next = node;
                // necro_chain_initialize_node(node, hash, key, curr_epoch, next);
                return (node + 1);
            }
            prev = node;
            node = next;
        }
    }

    TRACE_VAULT("allocating new node at end of chain\n");
    assert(prev != NULL);
    // node = necro_alloc_slab(chain->slab_allocator, sizeof(NecroChainNode));
    node = necro_chain_alloc_node(chain->slab_allocator, hash, key, curr_epoch, NULL, data_size);
    // necro_chain_initialize_node(node, hash, key, curr_epoch, NULL);
    prev->next = node;
    chain->curr_count++;
    return (node + 1);
    // return ((char*)node) + sizeof(NecroChainNode);
}

inline void necro_chain_insert_prev_node_into_curr_buckets(NecroChain* chain, NecroChainNode* inserted_node, const int32_t curr_epoch)
{
    TRACE_VAULT("alloc prev node with key\n");
    assert(inserted_node);

    if (chain->curr_count > chain->curr_size / 2)
        necro_chain_grow(chain);

    size_t          hash  = necro_hash_key(inserted_node->key);
    size_t          index = hash & (chain->curr_size - 1);
    NecroChainNode* prev  = NULL;
    NecroChainNode* node  = NULL;
    NecroChainNode* next  = NULL;
    inserted_node->next   = NULL;

    if (!chain->curr_buckets[index].has_nodes)
    {
        TRACE_VAULT("inserting prev node into empty bucket\n");
        chain->curr_buckets[index].head      = inserted_node;
        chain->curr_buckets[index].has_nodes = true;
        chain->curr_count++;
        return;
    }

    node = chain->curr_buckets[index].head;
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
                // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                necro_chain_free_node(chain->slab_allocator, node);
            }
            else
            {
                TRACE_VAULT("inserting prev node by replacing existing HEAD node with same key\n");
                assert(node == chain->curr_buckets[index].head);
                chain->curr_buckets[index].head = inserted_node;
                assert(inserted_node->next == NULL);
                // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                necro_chain_free_node(chain->slab_allocator, node);
            }
            return;
        }
        else
        {
            if (necro_chain_lazy_gc(node, curr_epoch))
            {
                inserted_node->next = next;
                if (prev)
                {
                    TRACE_VAULT("inserting prev node by replacing existing retired node\n");
                    prev->next = inserted_node;
                    // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                    necro_chain_free_node(chain->slab_allocator, node);
                }
                else
                {
                    TRACE_VAULT("inserting prev node by replacing existing retired HEAD node\n");
                    assert(node == chain->curr_buckets[index].head);
                    chain->curr_buckets[index].head = inserted_node;
                    assert(inserted_node->next == NULL);
                    // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                    necro_chain_free_node(chain->slab_allocator, node);
                }
                return;
            }
            prev = node;
            node = next;
        }
    }

    TRACE_VAULT("inserting prev node at end of chain\n");
    assert(prev != NULL);
    prev->next = inserted_node;
    chain->curr_count++;
}

void* necro_chain_alloc(NecroChain* chain, const NecroVaultKey key, const int32_t curr_epoch, const uint32_t data_size)
{
    assert(chain != NULL);
    assert(chain->slab_allocator != NULL);
    assert(chain->curr_buckets != NULL);

    necro_chain_incremental_gc(chain, curr_epoch);

    // Lazy move all nodes in a bucket into curr_buckets
    for (size_t i = 0; i < 2; ++i)
    {
        if (chain->prev_buckets != NULL)
        {
            TRACE_VAULT("lazy_move_index: %d, prev_size: %d, prev_count: %d\n", chain->lazy_move_index, chain->prev_size, chain->prev_count);
            assert(chain->lazy_move_index < chain->prev_size);
            assert(chain->prev_size  > 0);
            if (chain->prev_buckets[chain->lazy_move_index].has_nodes)
            {
                NecroChainNode* node = chain->prev_buckets[chain->lazy_move_index].head;
                NecroChainNode* next = NULL;
                while (node != NULL)
                {
                    assert(node->last_epoch >= 0);
                    next = node->next;
                    if (necro_chain_lazy_gc(node, curr_epoch))
                    {
                        TRACE_VAULT("lazy gc free node in prev_bucket during lazy move\n");
                        // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                        necro_chain_free_node(chain->slab_allocator, node);
                    }
                    else
                    {
                        necro_chain_insert_prev_node_into_curr_buckets(chain, node, curr_epoch);
                    }
                    chain->prev_count--;
                    node = next;
                    assert(chain->prev_count >= 0);
                }
            }
            chain->prev_buckets[chain->lazy_move_index].has_nodes = false;
            chain->prev_buckets[chain->lazy_move_index].head      = NULL;
            chain->lazy_move_index++;
            if (chain->prev_count <= 0 || chain->lazy_move_index == chain->prev_size)
            {
                necro_chain_destroy_prev_bucket(chain);
            }
        }
    }
    return necro_chain_alloc_with_key(chain, key, curr_epoch, data_size);
}

void necro_chain_incremental_gc(NecroChain* chain, const int32_t curr_epoch)
{
    assert(chain != NULL);
    assert(chain->slab_allocator != NULL);
    assert(chain->curr_buckets != NULL);
    // GC a bucket at a time
    TRACE_VAULT("Incremental GC, incremental_gc_index: %d, curr_size: %d, curr_count: %d\n", chain->incremental_gc_index, chain->curr_size, chain->curr_count);
    if (chain->curr_buckets[chain->incremental_gc_index].has_nodes)
    {
        NecroChainNode* prev = NULL;
        NecroChainNode* node = chain->curr_buckets[chain->incremental_gc_index].head;
        NecroChainNode* next = NULL;
        while (node != NULL)
        {
            assert(node->last_epoch >= 0);
            next = node->next;
            if (necro_chain_lazy_gc(node, curr_epoch))
            {
                TRACE_VAULT("incremental gc free node\n");
#if NECRO_VAULT_DEBUG
                necro_chain_print_node(node);
#endif
                chain->curr_count--;
                if (prev == NULL)
                {
                    assert(node == chain->curr_buckets[chain->incremental_gc_index].head);
                    chain->curr_buckets[chain->incremental_gc_index].head      = NULL;
                    chain->curr_buckets[chain->incremental_gc_index].has_nodes = false;
                    // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                    necro_chain_free_node(chain->slab_allocator, node);
                    break;
                }
                else
                {
                    prev->next = next;
                    // necro_free_slab(chain->slab_allocator, node, sizeof(NecroChainNode));
                    necro_chain_free_node(chain->slab_allocator, node);
                }
            }
            prev = node;
            node = next;
        }
    }
    chain->incremental_gc_index = (chain->incremental_gc_index + 1) & (chain->curr_size - 1);
}

size_t necro_chain_count(NecroChain* chain)
{
    return chain->prev_count + chain->curr_count;
}

size_t necro_chain_largest_bucket(NecroChain* chain)
{
    size_t largest_count = 0;
    for (size_t i = 0; i < chain->prev_size; ++i)
    {
        size_t curr_count = 0;
        if (!chain->prev_buckets[i].has_nodes)
            continue;
        NecroChainNode* node = chain->prev_buckets[i].head;
        while (node != NULL)
        {
            curr_count++;
            node = node->next;
        }
        if (curr_count > largest_count)
            largest_count = curr_count;
    }
    for (size_t i = 0; i < chain->curr_size; ++i)
    {
        size_t curr_count = 0;
        if (!chain->curr_buckets[i].has_nodes)
            continue;
        NecroChainNode* node = chain->curr_buckets[i].head;
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

void necro_chain_print_node(NecroChainNode* node)
{
    if (node == NULL)
    {
        printf("NULL node\n");
    }
    else if (node->data_size == 8)
    {
        int64_t* data = (int64_t*)(node + 1);
        printf("NecroChainNode { hash: %d, place: %d, universe: %d, time: %d, last_epoch: %d, data_size: %d, value: %lld }\n", node->hash, node->key.place, node->key.universe, node->key.time, node->last_epoch, node->data_size, data[0]);
    }
    else if (node->data_size == 16)
    {
        int64_t* data = (int64_t*)(node + 1);
        printf("NecroChainNode { hash: %d, place: %d, universe: %d, time: %d, last_epoch: %d, data_size: %d, value1: %lld, value2: %lld }\n", node->hash, node->key.place, node->key.universe, node->key.time, node->last_epoch, node->data_size, data[0], data[2]);
    }
}

void necro_chain_print(NecroChain* chain)
{
    printf("NecroChain\n{\n");

    printf("    prev_size:    %d,\n", chain->prev_size);
    printf("    prev_count:   %d,\n", chain->prev_count);
    printf("    prev_data:\n    [\n");
    for (size_t i = 0; i < chain->prev_size; ++i)
    {
        if (!chain->prev_buckets[i].has_nodes)
            continue;
        NecroChainNode* node = chain->prev_buckets[i].head;
        while (node != NULL)
        {
            if (node == chain->prev_buckets[i].head)
                printf("        [%d] = ", i);
            else
                printf("                  ");
            necro_chain_print_node(node);
            node = node->next;
        }
    }
    printf("    ]\n");

    printf("    curr_size:    %d,\n", chain->curr_size);
    printf("    curr_count:   %d,\n", chain->curr_count);
    printf("    curr_data:\n    [\n");
    for (size_t i = 0; i < chain->curr_size; ++i)
    {
        if (!chain->curr_buckets[i].has_nodes)
            continue;
        NecroChainNode* node = chain->curr_buckets[i].head;
        while (node != NULL)
        {
            if (node == chain->curr_buckets[i].head)
                printf("        [%d] = ", i);
            else
                printf("                  ");
            necro_chain_print_node(node);
            node = node->next;
        }
    }
    printf("    ]\n");
    printf("}\n\n");
}

//=====================================================
// Testing
//=====================================================
bool necro_chain_test_int(NecroChain* chain, NecroVaultKey key, int32_t epoch, int64_t value, bool print_on_success, bool print_on_failure)
{
    NecroChainNode* node     = necro_chain_find_node(chain, key, epoch);
    NecroChainNode* expected = necro_alloc_slab(chain->slab_allocator, sizeof(NecroChainNode) + 8);
    *expected                = (NecroChainNode) { .next = NULL, .hash = necro_hash_key(key), .key = key, .last_epoch = epoch, .data_size = 8 };
    int64_t* expected_data   = (int64_t*)(expected + 1);
    *expected_data           = value;

    // Contains Test
    if (node != NULL)
    {
        if (print_on_success)
            puts("NecroChain contains test:      passed");
    }
    else
    {
        if (print_on_failure)
        {
            puts("NecroChain found test:        failed");
            printf("                     found:   NULL\n");
            printf("                  expected:   ");
            necro_chain_print_node(expected);
        }
        necro_free_slab(chain->slab_allocator, expected, sizeof(NecroChainNode) + 8);
        return false;
    }

    // Node Test
    if (node->data_size = 8, node->hash == necro_hash_key(key) && necro_vault_compare_key(node->key, key) && node->last_epoch == epoch)
    {
        if (print_on_success)
            puts("NecroChain node test:          passed");
    }
    else
    {
        if (print_on_failure)
        {
            puts("NecroChain node test:          failed");
            printf("                      found:   ");
            necro_chain_print_node(node);
            printf("                   expected:   ");
            necro_chain_print_node(expected);
        }
        necro_free_slab(chain->slab_allocator, expected, sizeof(NecroChainNode) + 8);
        return false;
    }

    // Value test
    int64_t* data = (int64_t*)(node + 1);
    if (data[0] == value)
    {
        if (print_on_success)
            puts("NecroChain data test:          passed");
    }
    else
    {
        if (print_on_failure)
        {
            puts("NecroChain data test:          failed");
            printf("                      found:   ");
            necro_chain_print_node(node);
            printf("                   expected:   ");
            necro_chain_print_node(expected);
        }
        necro_free_slab(chain->slab_allocator, expected, sizeof(NecroChainNode) + 8);
        return false;
    }

    necro_free_slab(chain->slab_allocator, expected, sizeof(NecroChainNode) + 8);
    return true;
}

void necro_chain_test()
{
    puts("--------------------------------");
    puts("-- Testing NecroChain");
    puts("--------------------------------\n");

    NecroSlabAllocator slab_allocator = necro_create_slab_allocator(1024);
    NecroChain         chain          = necro_create_chain(&slab_allocator);
    int32_t            curr_epoch     = 1;

    // puts("\n---");
    // necro_chain_print(&chain);
    // puts("");

    // Destroy test
    necro_destroy_chain(&chain);
    if (chain.prev_buckets == NULL && chain.curr_buckets == NULL)
        puts("NecroChain destroy test:       passed");
    else
        puts("NecroChain destroy test:       failed");

    chain = necro_create_chain(&slab_allocator);

    // Test Entry1
    NecroVaultKey key1   = { .place = 1, .universe = 0, .time = 1 };
    int64_t*      alloc1 = necro_chain_alloc(&chain, key1, curr_epoch, 8);
    *alloc1              = 7;
    necro_chain_test_int(&chain, key1, curr_epoch, 7, true, true);

    // Test Entry2
    NecroVaultKey key2   = { .place = 2, .universe = 0, .time = 1 };
    int64_t*      alloc2 = necro_chain_alloc(&chain, key2, curr_epoch, 8);
    *alloc2              = 254;
    necro_chain_test_int(&chain, key2, curr_epoch, 254, true, true);

    // Test Entry3
    NecroVaultKey key3   = { .place = 3, .universe = 0, .time = 1 };
    int64_t*      alloc3 = necro_chain_alloc(&chain, key3, curr_epoch, 8);
    *alloc3              = 122;
    necro_chain_test_int(&chain, key3, curr_epoch, 122, true, true);

    // Test Entry4
    NecroVaultKey key4   = { .place = 4, .universe = 0, .time = 1 };
    int64_t*      alloc4 = necro_chain_alloc(&chain, key4, curr_epoch, 8);
    *alloc4              = 1;
    necro_chain_test_int(&chain, key4, curr_epoch, 1, true, true);

    // Double Insert Test
    int64_t* alloc3_again = necro_chain_find(&chain, key3, curr_epoch);
    *alloc3_again         = 66;
    necro_chain_test_int(&chain, key3, curr_epoch, 66, true, true);

    // puts("\n---");
    // necro_chain_print(&chain);
    // puts("");

    necro_destroy_chain(&chain);
    chain = necro_create_chain(&slab_allocator);

    // Many inserts test
    bool test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        int32_t         epoch = 1;
        int64_t         value = (uint8_t)i;
        NecroVaultKey   key   = { .place = 1,.universe = 0,.time = i };
        int64_t*        alloc = necro_chain_alloc(&chain, key, epoch, 8);
        *alloc                = value;
        if (!necro_chain_test_int(&chain, key, epoch, value, false, true))
        {
            test_passed = false;
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroChain many inserts test:  passed");
    }

    // Many inserts2 test
    test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        int32_t         epoch = 10;
        int64_t         value = (uint8_t)i + 10;
        NecroVaultKey   key   = { .place = 10,.universe = 0,.time = i };
        int64_t*        alloc = necro_chain_alloc(&chain, key, epoch, 8);
        *alloc                = value;
        if (!necro_chain_test_int(&chain, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_chain_print(&chain);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroChain many inserts2 test: passed");
    }

    // Many Lookups test
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE / 2; ++i)
    {
        int32_t         epoch = 10;
        int64_t         value = (uint8_t)i + 10;
        NecroVaultKey   key   = { .place = 10,.universe = 0,.time = i };
        if (!necro_chain_test_int(&chain, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_chain_print(&chain);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroChain many lookups2 test: passed");
    }

    // MonotonicEpoch test
    test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE; ++i)
    {
        int32_t         epoch = 100 + i;
        int64_t         value = (uint8_t)i + 20;
        NecroVaultKey   key   = { .place = 30,.universe = 0,.time = i };
        int64_t*        alloc = necro_chain_alloc(&chain, key, epoch, 8);
        *alloc                = value;
        if (!necro_chain_test_int(&chain, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_chain_print(&chain);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroChain epoch test:         passed");
    }

    // Many inserts3 test
    test_passed = true;
    for (size_t i = 0; i < NECRO_VAULT_INITIAL_SIZE * 32; ++i)
    {
        int32_t         epoch = 2000000;
        int64_t         value = (uint8_t)i + 10;
        NecroVaultKey   key   = { .place = (i % 400) ,.universe = (i %123),.time = (i % 290) };
        int64_t*        alloc = necro_chain_alloc(&chain, key, epoch, 8);
        *alloc                = value;
        if (!necro_chain_test_int(&chain, key, epoch, value, false, true))
        {
            test_passed = false;
            // necro_chain_print(&chain);
            return;
        }
    }
    if (test_passed)
    {
        puts("NecroChain many inserts3 test: passed");
    }

    // printf("chain prev_count: %d, curr_count: %d, total_count: %d, curr_size: %d, largest_bucket: %d", chain.prev_count, chain.curr_count, necro_chain_count(&chain), chain.curr_size, necro_chain_largest_bucket(&chain));
    // puts("\n---");
    // necro_chain_print(&chain);
    // puts("");
}
/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "intern.h"

// Constants
#define NECRO_INITIAL_INTERN_SIZE    512
#define NECRO_INTERN_HASH_MAGIC      5381
#define NECRO_INTERN_HASH_MULTIPLIER 33
#define NECRO_INTERN_NULL_ID         0

static char NULL_CHAR = '\0';

NecroIntern necro_intern_empty()
{
    return (NecroIntern)
    {
        // necro_empty_char_vector(),
        .arena   = necro_paged_arena_empty(),
        .symbols = NULL,
        .size    = 0,
        .count   = 0,
    };
}

NecroIntern necro_intern_create()
{
    NecroSymbol* symbols = malloc(NECRO_INITIAL_INTERN_SIZE * sizeof(NecroSymbol));
    if (symbols == NULL)
    {
        fprintf(stderr, "Malloc returned null while allocating entries in necro_create_intern()\n");
        exit(1);
    }

    for (int32_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
    {
        // entries[i] = (NecroInternEntry) { { .hash = 0, .id = NECRO_INTERN_NULL_ID, .str = NULL }, 0 };
        symbols[i] = (NecroSymbol) { .hash = 0, .id = NECRO_INTERN_NULL_ID, .str = NULL };
    }

    return (NecroIntern)
    {
        // necro_create_char_vector(),
        .arena   = necro_paged_arena_create(),
        .symbols = symbols,
        .size    = NECRO_INITIAL_INTERN_SIZE,
        .count   = 0,
    };
}

void necro_intern_destroy(NecroIntern* intern)
{
    intern->count      = 0;
    intern->size       = 0;
    if (intern->symbols != NULL)
        free(intern->symbols);
    intern->symbols    = NULL;
    // necro_destroy_char_vector(&intern->strings);
    necro_paged_arena_destroy(&intern->arena);
}

size_t necro_hash_string(const char* str)
{
    // Assuming that char is 8 bytes...
    uint8_t* u_str = (uint8_t*) str;
    size_t   hash  = NECRO_INTERN_HASH_MAGIC;
    while (*u_str != '\0')
    {
        hash = hash * NECRO_INTERN_HASH_MULTIPLIER + *u_str;
        u_str++;
    }
    return hash;
}

size_t necro_hash_string_slice(NecroStringSlice slice)
{
    // Assuming that char is 8 bytes...
    uint8_t* u_str = (uint8_t*) slice.data;
    size_t   hash  = NECRO_INTERN_HASH_MAGIC;
    for (size_t i = 0; i < slice.length; ++i)
    {
        hash = hash * NECRO_INTERN_HASH_MULTIPLIER + *u_str;
        u_str++;
    }
    return hash;
}

bool necro_intern_contains_symbol(NecroIntern* intern, NecroSymbol symbol)
{
    if (symbol.id == NECRO_INTERN_NULL_ID)
        return false;
    for (size_t probe = symbol.hash % intern->size; intern->symbols[probe].id != NECRO_INTERN_NULL_ID; probe = (probe + 1) % intern->size)
    {
        if (intern->symbols[probe].id == symbol.id)
            return true;
    }
    return false;
}

const char* necro_intern_get_string(NecroIntern* intern, NecroSymbol symbol)
{
    UNUSED(intern);
    return symbol.str;
    // if (symbol.id == NECRO_INTERN_NULL_ID)
    //     return NULL;
    // for (size_t probe = symbol.hash % intern->size; intern->entries[probe].symbol.id != NECRO_INTERN_NULL_ID; probe = (probe + 1) % intern->size)
    // {
    //     if (intern->entries[probe].symbol.id == symbol.id)
    //         return intern->strings.data + intern->entries[probe].string_index;
    // }
    // return NULL;
}

void necro_intern_grow(NecroIntern* intern)
{
    size_t          old_size    = intern->size;
    NecroSymbol*    old_symbols = intern->symbols;
    intern->size                = intern->size * 2;
    intern->symbols             = malloc(intern->size * sizeof(NecroSymbol));
    // printf("intern grow, old: %d, new: %d", old_size, intern->size);
    if (intern->symbols == NULL)
    {
        fprintf(stderr, "Malloc returned NULL while allocating memory for entries in necro_intern_grow()\n");
        exit(1);
    }
    // initialize new block of memory
    for (size_t i = 0; i < intern->size; ++i)
    {
        intern->symbols[i] = (NecroSymbol) { .hash = 0, .id = NECRO_INTERN_NULL_ID, .str = NULL };
    }
    // re-insert all data from old block of memory
    for (size_t i = 0; i < old_size; ++i)
    {
        if (old_symbols[i].id != NECRO_INTERN_NULL_ID)
        {
            size_t hash  = old_symbols[i].hash;
            size_t probe = hash % intern->size;
            while (intern->symbols[probe].id != NECRO_INTERN_NULL_ID)
            {
                probe = (probe + 1) % intern->size;
            }
            assert(intern->symbols[probe].id == NECRO_INTERN_NULL_ID);
            // assert(intern->entries[probe].string_index == 0);
            intern->symbols[probe] = old_symbols[i];
        }
    }
    free(old_symbols);
}

// TODO: Optimize memory allocation
NecroSymbol necro_intern_create_type_class_instance_symbol(NecroIntern* intern, NecroSymbol symbol, NecroSymbol type_class_name)
{
    const char* string1 = necro_intern_get_string(intern, symbol);
    const char* div     = "@";
    const char* string2 = necro_intern_get_string(intern, type_class_name);
    size_t      len1    = strlen(string1);
    size_t      lend    = strlen(div);
    size_t      len2    = strlen(string2);
    char*       str     = malloc((len1 + lend + len2 + 1) * sizeof(char));
    if (str == NULL)
    {
        fprintf(stderr, "Could not allocate memory in necro_intern_concat_symbol\n");
        exit(1);
    }
    // Copy str1
    for (size_t i = 0; i < len1; ++i)
        str[i] = string1[i];
    // Copy div
    for (size_t i = 0; i < lend; ++i)
        str[len1 + i] = div[i];
    // Copy str2
    for (size_t i = 0; i < len2; ++i)
        str[len1 + lend + i] = string2[i];
    str[len1 + lend + len2]   = '\0';
    NecroSymbol new_symbol = necro_intern_string(intern, str);
    free(str);
    return new_symbol;
}

// TODO: Optimize memory allocation
NecroSymbol necro_intern_get_type_class_member_symbol_from_instance_symbol(NecroIntern* intern, NecroSymbol symbol)
{
    const char* string1 = necro_intern_get_string(intern, symbol);
    size_t      len1    = 0;
    for (size_t i = 0; string1[i] != '@'; ++i)
        len1++;
    char* str = malloc((len1 + 1) * sizeof(char));
    if (str == NULL)
    {
        fprintf(stderr, "Could not allocate memory in necro_intern_get_type_class_member_symbol_from_instance_symbol\n");
        exit(1);
    }
    // Copy str1
    for (size_t i = 0; i < len1; ++i)
        str[i] = string1[i];
    str[len1] = '\0';
    NecroSymbol new_symbol = necro_intern_string(intern, str);
    free(str);
    return new_symbol;
}

// TODO: Optimize memory allocation
NecroSymbol necro_intern_concat_symbols(NecroIntern* intern, NecroSymbol symbol1, NecroSymbol symbol2)
{
    const char* string1 = necro_intern_get_string(intern, symbol1);
    const char* string2 = necro_intern_get_string(intern, symbol2);
    size_t      len1    = strlen(string1);
    size_t      len2    = strlen(string2);
    char*       str     = malloc((len1 + len2 + 1) * sizeof(char));
    if (str == NULL)
    {
        fprintf(stderr, "Could not allocate memory in necro_intern_concat_symbol\n");
        exit(1);
    }
    for (size_t i = 0; i < len1; ++i)
        str[i] = string1[i];
    for (size_t i = 0; i < len2; ++i)
        str[len1 + i] = string2[i];
    str[len1 + len2]   = '\0';
    NecroSymbol symbol = necro_intern_string(intern, str);
    free(str);
    return symbol;
}

NecroSymbol necro_intern_string(NecroIntern* intern, const char* str)
{
    assert(intern          != NULL);
    assert(intern->symbols != NULL);
    // assert(intern->strings.data != NULL);

    // Early exit on NULL strings
    if (str == NULL)
        return (NecroSymbol) { 0, NECRO_INTERN_NULL_ID };

    // Grow if we're over 50% load
    if (intern->count >= (intern->size / 2))
        necro_intern_grow(intern);

    // Do linear probe
    size_t hash  = necro_hash_string(str);
    size_t probe = hash % intern->size;
    while (intern->symbols[probe].id != NECRO_INTERN_NULL_ID)
    {
        if (intern->symbols[probe].hash == hash)
        {
            // char* str_at_probe = intern->strings.data + intern->entries[probe].string_index;
            const char* str_at_probe = intern->symbols[probe].str;
            if (str_at_probe != NULL && strcmp(str_at_probe, str) == 0)
                return intern->symbols[probe];
        }
        probe = (probe + 1) % intern->size;
    }

    // Assert that this entry is in fact empty
    // assert(intern->entries[probe].symbol.id    == NECRO_INTERN_NULL_ID);
    // assert(intern->entries[probe].string_index == 0);
    assert(intern->symbols[probe].str == NULL);

    // Assign Entry Data
    char* new_str          = necro_paged_arena_alloc(&intern->arena, strlen(str) + 1);
    strcpy(new_str, str);
    intern->symbols[probe] = (NecroSymbol) { .hash = hash, .id = intern->count + 1, .str = new_str };
    // intern->entries[probe].string_index = intern->strings.length;

    // Push string contents into Char Vector
    //  for (char* current_char = (char*)str; *current_char != '\0'; ++current_char)
    //      necro_push_char_vector(&intern->strings, current_char);
    //  necro_push_char_vector(&intern->strings, &NULL_CHAR);

    // Increase count, return symbol
    intern->count += 1;
    return intern->symbols[probe];
}

NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice)
{
    assert(intern          != NULL);
    assert(intern->symbols != NULL);
    // assert(intern->strings.data != NULL);

    // Early exit on NULL strings
    if (slice.data == NULL)
        return (NecroSymbol) { 0, NECRO_INTERN_NULL_ID };

    // Grow if we're over 50% load
    if (intern->count >= (intern->size / 2))
        necro_intern_grow(intern);

    // Do Linear probe
    size_t hash  = necro_hash_string_slice(slice);
    size_t probe = hash % intern->size;
    while (intern->symbols[probe].id != NECRO_INTERN_NULL_ID)
    {
        if (intern->symbols[probe].hash == hash)
        {
            // char* str_at_probe = intern->strings.data + intern->entries[probe].string_index;
            const char* str_at_probe = intern->symbols[probe].str;
            if (str_at_probe != NULL && strncmp(str_at_probe, slice.data, slice.length) == 0)
                return intern->symbols[probe];
        }
        probe = (probe + 1) % intern->size;
    }

    // Assert that this is in fact en empty entry
    // assert(intern->entries[probe].symbol.id    == NECRO_INTERN_NULL_ID);
    // assert(intern->entries[probe].string_index == 0);
    assert(intern->symbols[probe].str == NULL);

    // Assign Entry data
    // intern->entries[probe].symbol       = (NecroSymbol) { hash, intern->count + 1 };
    // intern->entries[probe].string_index = intern->strings.length;
    char* new_str          = necro_paged_arena_alloc(&intern->arena, slice.length + 1);
    strncpy(new_str, slice.data, slice.length);
    new_str[slice.length]  = '\0';
    intern->symbols[probe] = (NecroSymbol) { .hash = hash, .id = intern->count + 1, .str = new_str };

    // // Push string contents into Char Vector
    // for (size_t i = 0; i < slice.length; ++i)
    //     necro_push_char_vector(&intern->strings, (char*) (slice.data + i));
    // necro_push_char_vector(&intern->strings, &NULL_CHAR);

    // Increase count and return symbol
    intern->count += 1;
    return intern->symbols[probe];
}

void necro_intern_print(NecroIntern* intern)
{
    printf("NecroIntern\n{\n");
    printf("    size:    %zu,\n", intern->size);
    printf("    count:   %zu,\n", intern->count);
    printf("    data:\n    [\n");
    for (size_t i = 0; i < intern->size; ++i)
    {
        if (intern->symbols[i].id == NECRO_INTERN_NULL_ID)
            continue;
        // printf("        hash: %zu, id: %zu, value: %s\n", intern->entries[i].symbol.hash, intern->entries[i].symbol.id, intern->strings.data + intern->entries[i].string_index);
        printf("        hash: %zu, id: %zu, value: %s\n", intern->symbols[i].hash, intern->symbols[i].id, intern->symbols[i].str);
    }
    printf("    ]\n");
    printf("}\n\n");
}

//=====================================================
// Testing
//=====================================================
void necro_test_intern_id(NecroIntern* intern, NecroSymbol symbol, const char* compare_str)
{
    assert(symbol.id != NECRO_INTERN_NULL_ID);
    puts("Intern id test:         passed");

    assert(symbol.hash == necro_hash_string(compare_str));
    puts("Intern hash test:       passed");

    assert(necro_intern_contains_symbol(intern, symbol));
    puts("Intern contains test:   passed");

    assert(strcmp(necro_intern_get_string(intern, symbol), compare_str) == 0);
    puts("Intern get test:        passed");
}

void necro_intern_test()
{
    necro_announce_phase("NecroIntern");

    NecroIntern intern = necro_intern_create();

    // Test ID1
    NecroSymbol  id1    = necro_intern_string(&intern, "test");
    necro_test_intern_id(&intern, id1, "test");

    // Test ID2
    NecroSymbol  id2    = necro_intern_string(&intern, "This is not a test");
    necro_test_intern_id(&intern, id2, "This is not a test");

    // Test ID3
    NecroSymbol  id3    = necro_intern_string_slice(&intern, (NecroStringSlice) { "fuck yeah", 4 });
    necro_test_intern_id(&intern, id3, "fuck");

    // Test ID4
    NecroSymbol  id4    = necro_intern_string_slice(&intern, (NecroStringSlice) { "please work?", 6 });
    necro_test_intern_id(&intern, id4, "please");

    // Destroy test
    necro_intern_destroy(&intern);
    assert(intern.symbols == NULL);
    puts("Intern destroy test:    passed");

    // Grow test
    intern = necro_intern_create();
    NecroSymbol symbols[NECRO_INITIAL_INTERN_SIZE];
    char testChars[NECRO_INITIAL_INTERN_SIZE];
    for (size_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
    {
        testChars[i] = '0' + (char)i;
        const char* data = ((char*) testChars) + i;
        symbols[i] = necro_intern_string_slice(&intern, (NecroStringSlice) { data, 1 });
    }

    for (size_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
    {
        assert(necro_intern_contains_symbol(&intern, symbols[i]));
    }
    puts("Intern grow test:       passed");

    necro_intern_destroy(&intern);
    assert(intern.symbols == NULL);
    puts("Intern g destroy test:  passed");
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>

#include "intern.h"
#include "itoa.h"
#include "type.h"

// Constants
#define NECRO_INITIAL_INTERN_SIZE    4096
#define NECRO_INTERN_HASH_MAGIC      5381
#define NECRO_INTERN_HASH_MULTIPLIER 33

NecroIntern necro_intern_empty()
{
    return (NecroIntern)
    {
        .arena          = necro_paged_arena_empty(),
        .snapshot_arena = necro_snapshot_arena_empty(),
        .entries        = NULL,
        .size           = 0,
        .count          = 0,
        .clash_suffix   = 0,
    };
}

NecroIntern necro_intern_create()
{
    NecroInternEntry* entries = emalloc(NECRO_INITIAL_INTERN_SIZE * sizeof(NecroInternEntry));
    for (int32_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
    {
        entries[i] = (NecroInternEntry) { .hash = 0, .data = NULL };
    }

    NecroIntern intern =
    {
        .arena          = necro_paged_arena_create(),
        .snapshot_arena = necro_snapshot_arena_create(),
        .entries        = entries,
        .size           = NECRO_INITIAL_INTERN_SIZE,
        .count          = 0,
        .clash_suffix   = 0,
    };

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!
    // Intern keywords, in the same order as their listing in the NECRO_LEX_TOKEN_TYPE enum
    // MAKE SURE THAT THE FIRST N ENTRIES IN NECRO_LEX_TOKEN_TYPE ARE THE KEYWORD TYPES AND THAT THEY EXACTLY MATCH THEIR SYMBOLS MINUS ONE!!!!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!
    necro_intern_string(&intern, "let");
    necro_intern_string(&intern, "where");
    necro_intern_string(&intern, "of");
    necro_intern_string(&intern, "do");
    necro_intern_string(&intern, "case");
    necro_intern_string(&intern, "class");
    necro_intern_string(&intern, "data");
    necro_intern_string(&intern, "deriving");
    necro_intern_string(&intern, "forall");
    necro_intern_string(&intern, "if");
    necro_intern_string(&intern, "else");
    necro_intern_string(&intern, "then");
    necro_intern_string(&intern, "import");
    necro_intern_string(&intern, "instance");
    necro_intern_string(&intern, "in");
    necro_intern_string(&intern, "module");
    necro_intern_string(&intern, "newtype");
    necro_intern_string(&intern, "type");
    necro_intern_string(&intern, "for");
    necro_intern_string(&intern, "loop");
    necro_intern_string(&intern, "while");
    return intern;
}

void necro_intern_destroy(NecroIntern* intern)
{
    if (intern->entries != NULL)
        free(intern->entries);
    necro_paged_arena_destroy(&intern->arena);
    necro_snapshot_arena_destroy(&intern->snapshot_arena);
    *intern = necro_intern_empty();
}

size_t necro_hash_string(const char* str, size_t* out_string_length)
{
    uint8_t* u_str = (uint8_t*) str;
    size_t   hash  = NECRO_INTERN_HASH_MAGIC;
    size_t   len   = 0;
    while (*u_str != '\0')
    {
        hash = hash * NECRO_INTERN_HASH_MULTIPLIER + *u_str;
        u_str++;
        len++;
    }
    *out_string_length = len;
    return hash;
}

size_t necro_hash_string_slice(NecroStringSlice slice)
{
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
    if (symbol == NULL)
        return false;
    for (size_t probe = (symbol->hash & (intern->size - 1)); intern->entries[probe].data != NULL; probe = (probe + 1) & (intern->size - 1))
    {
        if (intern->entries[probe].data == symbol)
            return true;
    }
    return false;
}

void necro_intern_grow(NecroIntern* intern)
{
    size_t            old_size    = intern->size;
    NecroInternEntry* old_entries = intern->entries;
    intern->size                  = intern->size * 2;
    intern->entries               = emalloc(intern->size * sizeof(NecroInternEntry));
    size_t            new_count   = 0;
    // initialize new block of memory
    for (size_t i = 0; i < intern->size; ++i)
    {
        intern->entries[i] = (NecroInternEntry) { .hash = 0, .data = NULL };
    }
    // re-insert all data from old block of memory
    for (size_t i = 0; i < old_size; ++i)
    {
        if (old_entries[i].data != NULL)
        {
            size_t hash  = old_entries[i].hash;
            size_t probe = hash & (intern->size - 1);
            while (intern->entries[probe].data != NULL)
            {
                probe = (probe + 1) & (intern->size - 1);
            }
            assert(intern->entries[probe].data == NULL);
            intern->entries[probe] = old_entries[i];
            new_count++;
        }
    }
    assert(new_count == intern->count);
    free(old_entries);
}

NecroSymbol necro_intern_create_type_class_instance_symbol(NecroIntern* intern, NecroSymbol symbol, NecroSymbol type_class_name)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&intern->snapshot_arena);
    const char* string1 = symbol->str;
    const char* div1    = "<";
    const char* string2 = type_class_name->str;
    const char* div2    = ">";
    size_t      len1    = strlen(string1);
    size_t      lend    = strlen(div1);
    size_t      len2    = strlen(string2);
    size_t      lend2   = strlen(div2);
    char*       str     = necro_snapshot_arena_alloc(&intern->snapshot_arena, (len1 + lend + len2 + lend2 + 1) * sizeof(char));
    // Copy str1
    for (size_t i = 0; i < len1; ++i)
        str[i] = string1[i];
    // Copy div1
    for (size_t i = 0; i < lend; ++i)
        str[len1 + i] = div1[i];
    // Copy str2
    for (size_t i = 0; i < len2; ++i)
        str[len1 + lend + i] = string2[i];
    // Copy div2
    for (size_t i = 0; i < lend; ++i)
        str[len1 + lend + len2 + i] = div2[i];
    str[len1 + lend + len2 + lend2]   = '\0';
    NecroSymbol new_symbol = necro_intern_string(intern, str);
    necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    return new_symbol;
}

NecroSymbol necro_intern_get_type_class_member_symbol_from_instance_symbol(NecroIntern* intern, NecroSymbol symbol)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&intern->snapshot_arena);
    const char* string1 = symbol->str;
    size_t      len1    = 0;
    for (size_t i = 0; string1[i] != '<'; ++i)
        len1++;
    char* str = necro_snapshot_arena_alloc(&intern->snapshot_arena, (len1 + 1) * sizeof(char));
    // Copy str1
    for (size_t i = 0; i < len1; ++i)
        str[i] = string1[i];
    str[len1] = '\0';
    NecroSymbol new_symbol = necro_intern_string(intern, str);
    necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    return new_symbol;
}

NecroSymbol necro_intern_concat_symbols(NecroIntern* intern, NecroSymbol symbol1, NecroSymbol symbol2)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&intern->snapshot_arena);
    const char* string1 = symbol1->str;
    const char* string2 = symbol2->str;
    size_t      len1    = symbol1->length;
    size_t      len2    = symbol2->length;
    char*       str     = necro_snapshot_arena_alloc(&intern->snapshot_arena, (len1 + len2 + 1) * sizeof(char));
    for (size_t i = 0; i < len1; ++i)
        str[i] = string1[i];
    for (size_t i = 0; i < len2; ++i)
        str[len1 + i] = string2[i];
    str[len1 + len2]   = '\0';
    NecroSymbol symbol = necro_intern_string(intern, str);
    necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    return symbol;
}

typedef struct
{
    size_t      hash;
    size_t      length;
    size_t      probe;
    NecroSymbol symbol;
} NecroInternProbeResult;

NecroInternProbeResult necro_intern_prob(NecroIntern* intern, const char* str)
{
    assert(intern          != NULL);
    assert(intern->entries != NULL);

    // Early exit on NULL strings
    if (str == NULL)
        return (NecroInternProbeResult) { .hash = 0, .length = 0, .probe = 0, .symbol = NULL};

    // Grow if we're over 50% load
    if (intern->count >= (intern->size / 2))
        necro_intern_grow(intern);

    // Do linear probe
    size_t length = 0;
    size_t hash   = necro_hash_string(str, &length);
    size_t probe  = hash & (intern->size - 1);
    while (intern->entries[probe].data != NULL)
    {
        if (intern->entries[probe].hash == hash)
        {
            const char* str_at_probe = intern->entries[probe].data->str;
            if (str_at_probe != NULL && strcmp(str_at_probe, str) == 0)
                return (NecroInternProbeResult) { .hash = hash, .length = length, .probe = probe, .symbol = intern->entries[probe].data };
        }
        probe = (probe + 1) & (intern->size - 1);
    }

    // Assert that this entry is in fact empty
    assert(intern->entries[probe].data == NULL);

    return (NecroInternProbeResult) { .hash = hash, .length = length, .probe = probe, .symbol = NULL};
}

NecroSymbol necro_intern_string(NecroIntern* intern, const char* str)
{
    // Probe
    NecroInternProbeResult probe_result = necro_intern_prob(intern, str);
    if (probe_result.symbol != NULL)
        return probe_result.symbol;

    const size_t length          = probe_result.length;
    const size_t hash            = probe_result.hash;
    const size_t probe           = probe_result.probe;

    // Alloc and insert
    intern->entries[probe].data  = necro_paged_arena_alloc(&intern->arena, sizeof(struct NecroSymbolData));
    char*  new_str               = necro_paged_arena_alloc(&intern->arena, length + 1);
    strcpy(new_str, str);
    *intern->entries[probe].data = (struct NecroSymbolData) { .hash = hash, .symbol_num = intern->count + 1, .str = new_str, .length = length, .global_string_value = NULL, .program = NULL };
    intern->entries[probe].hash  = hash;

    // Increase count, return symbol
    intern->count += 1;
    return intern->entries[probe].data;
}

NecroSymbol necro_intern_unique_string(NecroIntern* intern, const char* str)
{
    // Probe
    const size_t           str_len    = strlen(str);
    const size_t           buf_size   = str_len + 32;
    NecroArenaSnapshot     snapshot   = necro_snapshot_arena_get(&intern->snapshot_arena);
    char*                  unique_str = necro_snapshot_arena_alloc(&intern->snapshot_arena, buf_size);
    NecroInternProbeResult probe_result;
    char                   itoa_buf[NECRO_ITOA_BUF_LENGTH];
    do
    {
        char* itoa_result = necro_itoa((uint32_t)intern->clash_suffix, itoa_buf, NECRO_ITOA_BUF_LENGTH, 24);
        // char* itoa_result = necro_itoa((uint32_t)intern->clash_suffix, itoa_buf, NECRO_ITOA_BUF_LENGTH, 10);
        assert(itoa_result != NULL);
        UNUSED(itoa_result);
        intern->clash_suffix++;
        snprintf(unique_str, buf_size, "%s%s", str, itoa_buf);
        probe_result = necro_intern_prob(intern, unique_str);
        memset(itoa_buf, '\0', 16 * sizeof(char));
    }
    while (probe_result.symbol != NULL);

    const size_t length          = probe_result.length;
    const size_t hash            = probe_result.hash;
    const size_t probe           = probe_result.probe;

    // Alloc and insert
    intern->entries[probe].data  = necro_paged_arena_alloc(&intern->arena, sizeof(struct NecroSymbolData));
    char*  new_str               = necro_paged_arena_alloc(&intern->arena, length + 1);
    strcpy(new_str, unique_str);
    *intern->entries[probe].data = (struct NecroSymbolData) { .hash = hash, .symbol_num = intern->count + 1, .str = new_str, .length = length, .global_string_value = NULL, .program = NULL };
    intern->entries[probe].hash  = hash;

    // Increase count, return symbol
    intern->count += 1;
    necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    return intern->entries[probe].data;
}

NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice)
{
    assert(intern          != NULL);
    assert(intern->entries != NULL);

    // Early exit on NULL strings
    if (slice.data == NULL)
        return NULL;

    // Grow if we're over 50% load
    if (intern->count >= (intern->size / 2))
        necro_intern_grow(intern);

    // Do Linear probe
    size_t hash  = necro_hash_string_slice(slice);
    size_t probe = hash & (intern->size - 1);
    while (intern->entries[probe].data != NULL)
    {
        if (intern->entries[probe].hash == hash)
        {
            const char* str_at_probe = intern->entries[probe].data->str;
            if (str_at_probe != NULL && strncmp(str_at_probe, slice.data, slice.length) == 0)
                return intern->entries[probe].data;
        }
        probe = (probe + 1) & (intern->size - 1);
    }

    // Assert that this is in fact en empty entry
    assert(intern->entries[probe].data == NULL);

    // Alloc and insert
    size_t length                = slice.length;
    intern->entries[probe].data  = necro_paged_arena_alloc(&intern->arena, sizeof(struct NecroSymbolData));
    char*  new_str               = necro_paged_arena_alloc(&intern->arena, length + 1);
    strncpy(new_str, slice.data, slice.length);
    new_str[slice.length]        = '\0';
    *intern->entries[probe].data = (struct NecroSymbolData) { .hash = hash, .symbol_num = intern->count + 1, .str = new_str, .length = length, .global_string_value = NULL, .program = NULL };
    intern->entries[probe].hash  = hash;

    // Increase count and return symbol
    intern->count += 1;
    return intern->entries[probe].data;
}

size_t necro_type_mangle_subs_recursive(char* suffix_buffer, size_t offset, const NecroInstSub* sub)
{
    if (sub == NULL)
        return offset;
    offset = necro_type_mangle_subs_recursive(suffix_buffer, offset, sub->next);
    if (sub->next != NULL)
        offset += sprintf(suffix_buffer + offset, ",");
    offset = necro_type_mangled_sprintf(suffix_buffer, offset, sub->new_name);
    return offset;
}

NecroSymbol necro_intern_append_suffix_from_subs(NecroIntern* intern, NecroSymbol prefix, const NecroInstSub* subs)
{
    // Count length
    const size_t        prefix_length = strlen(prefix->str);
    size_t              length        = 2 + prefix_length;
    const NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        length += necro_type_mangled_string_length(curr_sub->new_name);
        if (curr_sub->next != NULL)
            length += 1;
        curr_sub = curr_sub->next;
    }

    // Alloc suffix buffer
    NecroArenaSnapshot snapshot      = necro_snapshot_arena_get(&intern->snapshot_arena);
    char*              suffix_buffer = necro_snapshot_arena_alloc(&intern->snapshot_arena, length);

    // Write type strings
    size_t offset = 0;
    if (prefix->str[prefix_length - 1] == '>')
    {
        // Name has already been overloaded for a type class, instead append to type list.
        offset = sprintf(suffix_buffer, "%s", prefix->str);
        suffix_buffer[prefix_length - 1] = ',';
        length -= 1;
    }
    else
    {
        offset = sprintf(suffix_buffer, "%s<", prefix->str);
    }
    offset        = necro_type_mangle_subs_recursive (suffix_buffer, offset, subs);
    offset       += sprintf(suffix_buffer + offset, ">");
    assert(offset == length);

    // Intern, clean up, return
    NecroSymbol string_symbol = necro_intern_string(intern, suffix_buffer);
    necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    return string_symbol;
}

void necro_intern_print(NecroIntern* intern)
{
    printf("NecroIntern\n{\n");
    printf("    size:    %zu,\n", intern->size);
    printf("    count:   %zu,\n", intern->count);
    printf("    data:\n    [\n");
    for (size_t i = 0; i < intern->size; ++i)
    {
        if (intern->entries[i].data == NULL)
            continue;
        printf("        hash: %zu, symbol_num: %zu, value: %s\n", intern->entries[i].hash, intern->entries[i].data->symbol_num, intern->entries[i].data->str);
    }
    printf("    ]\n");
    printf("}\n\n");
}

//=====================================================
// Testing
//=====================================================
void necro_test_intern_id(NecroIntern* intern, NecroSymbol symbol, const char* compare_str)
{
    UNUSED(intern);
    UNUSED(symbol);
    UNUSED(compare_str);
    assert(symbol->str != NULL);
    puts("Intern id test:         passed");

    size_t length = 0;
    UNUSED(length);
    assert(symbol->hash == necro_hash_string(compare_str, &length));
    puts("Intern hash test:       passed");

    assert(necro_intern_contains_symbol(intern, symbol));
    puts("Intern contains test:   passed");

    assert(strcmp(symbol->str, compare_str) == 0);
    puts("Intern get test:        passed");
}

void necro_intern_test()
{
    necro_announce_phase("NecroIntern");

    NecroIntern intern  = necro_intern_create();

    NecroSymbol symbol1 = necro_intern_string(&intern, "test");
    necro_test_intern_id(&intern, symbol1, "test");

    NecroSymbol symbol2 = necro_intern_string(&intern, "This is not a test");
    necro_test_intern_id(&intern, symbol2, "This is not a test");

    NecroSymbol symbol3 = necro_intern_string_slice(&intern, (NecroStringSlice) { "fuck yeah", 4 });
    necro_test_intern_id(&intern, symbol3, "fuck");

    NecroSymbol symbol4 = necro_intern_string_slice(&intern, (NecroStringSlice) { "please work?", 6 });
    necro_test_intern_id(&intern, symbol4, "please");

    // Destroy test
    necro_intern_destroy(&intern);
    assert(intern.entries == NULL);
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
    assert(intern.entries == NULL);
    puts("Intern g destroy test:  passed");
}

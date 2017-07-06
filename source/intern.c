/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "intern.h"

// Constants
#define NECRO_INITIAL_INTERN_SIZE    256
#define NECRO_INTERN_HASH_MAGIC      5381
#define NECRO_INTERN_HASH_MULTIPLIER 33
#define NECRO_INTERN_NULL_ID         0

NecroIntern necro_create_intern()
{
	NecroInternEntry* data = malloc(NECRO_INITIAL_INTERN_SIZE * sizeof(NecroInternEntry));
	for (int32_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
	{
		data[i] = (NecroInternEntry) { { 0, NECRO_INTERN_NULL_ID }, NULL };
	}
	return (NecroIntern)
	{
		data,
		NECRO_INITIAL_INTERN_SIZE,
		0,
	};
}

void necro_destroy_intern(NecroIntern* intern)
{
	for (size_t i = 0; i < intern->size; ++i)
	{
		if (intern->data[i].value != NULL)
			free(intern->data[i].value);
	}
	intern->count      = 0;
	intern->size       = 0;
	free(intern->data);
	intern->data       = NULL;
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
	for (size_t probe = symbol.hash % intern->size; intern->data[probe].symbol.id != NECRO_INTERN_NULL_ID; probe = (probe + 1) % intern->size)
	{
		if (intern->data[probe].symbol.id == symbol.id)
			return true;
	}
	return false;
}

const char* necro_intern_get_string(NecroIntern* intern, NecroSymbol symbol)
{
	if (symbol.id == NECRO_INTERN_NULL_ID)
		return NULL;
	for (size_t probe = symbol.hash % intern->size; intern->data[probe].symbol.id != NECRO_INTERN_NULL_ID; probe = (probe + 1) % intern->size)
	{
		if (intern->data[probe].symbol.id == symbol.id)
			return intern->data[probe].value;
	}
	return NULL;
}

void necro_intern_grow(NecroIntern* intern)
{
	size_t            old_size = intern->size;
	NecroInternEntry* old_data = intern->data;
	intern->size               = intern->size * 2;
	intern->data               = malloc(intern->size * sizeof(NecroInternEntry));
	// initialize new block of memory
	for (size_t i = 0; i < intern->size; ++i)
	{
		intern->data[i] = (NecroInternEntry) { { 0, NECRO_INTERN_NULL_ID }, NULL };
	}
	// re-insert all entries from old block of memory
	for (size_t i = 0; i < old_size; ++i)
	{
		if (old_data[i].symbol.id != NECRO_INTERN_NULL_ID)
		{
			size_t hash  = old_data[i].symbol.hash;
			size_t probe = hash % intern->size;
			while (intern->data[probe].symbol.id != NECRO_INTERN_NULL_ID)
			{
				probe = (probe + 1) % intern->size;
			}
			assert(intern->data[probe].symbol.id == NECRO_INTERN_NULL_ID);
			assert(intern->data[probe].value     == NULL);
			intern->data[probe] = old_data[i];
		}
	}
	free(old_data);
}

NecroSymbol necro_intern_string(NecroIntern* intern, const char* str)
{
	if (str == NULL)
		return (NecroSymbol) { 0, NECRO_INTERN_NULL_ID };
	if (intern->count >= intern->size / 2)
		necro_intern_grow(intern);
	size_t hash  = necro_hash_string(str);
	size_t probe = hash % intern->size;
	while (intern->data[probe].symbol.id != NECRO_INTERN_NULL_ID)
	{
		if (intern->data[probe].symbol.hash == hash && strcmp(intern->data[probe].value, str) == 0)
			return intern->data[probe].symbol;
		probe = (probe + 1) % intern->size;
	}
	assert(intern->data[probe].symbol.id == NECRO_INTERN_NULL_ID);
	assert(intern->data[probe].value     == NULL);
	intern->data[probe].symbol = (NecroSymbol) { hash, intern->count + 1 };
	intern->data[probe].value  = _strdup(str);
	intern->count += 1;
	return intern->data[probe].symbol;
}

NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice)
{
	// if (slice.data == NULL)
	// 	return NECRO_INTERN_NULL_ID;
	// if (intern->count >= intern->size / 2)
	// 	necro_intern_grow(intern);
	// size_t id    = necro_hash_string_slice(slice);
	// size_t probe = id % intern->size;
	// while (intern->data[probe].value != NULL)
	// {
	// 	if (intern->data[probe].key == id)
	// 		return id;
	// 	probe = (probe + 1) % intern->size;
	// }
	// assert(intern->data[probe].symbol.id == NECRO_INTERN_NULL_ID);
	// assert(intern->data[probe].value == NULL);
	// intern->data[probe].key   = id;
	// intern->data[probe].value = necro_dup_string_slice(slice);
	// intern->count += 1;
	// return id;

	if (slice.data == NULL)
		return (NecroSymbol) { 0, NECRO_INTERN_NULL_ID };
	if (intern->count >= intern->size / 2)
		necro_intern_grow(intern);
	size_t hash  = necro_hash_string_slice(slice);
	size_t probe = hash % intern->size;
	while (intern->data[probe].symbol.id != NECRO_INTERN_NULL_ID)
	{
		if (intern->data[probe].symbol.hash == hash && strncmp(intern->data[probe].value, slice.data, slice.length) == 0)
			return intern->data[probe].symbol;
		probe = (probe + 1) % intern->size;
	}
	assert(intern->data[probe].symbol.id == NECRO_INTERN_NULL_ID);
	assert(intern->data[probe].value     == NULL);
	intern->data[probe].symbol = (NecroSymbol) { hash, intern->count + 1 };
	intern->data[probe].value  = necro_dup_string_slice(slice);
	intern->count += 1;
	return intern->data[probe].symbol;
}

void necro_print_intern(NecroIntern* intern)
{
	printf("NecroIntern\n{\n");
	printf("    size:    %d,\n", intern->size);
	printf("    count:   %d,\n", intern->count);
	printf("    entries:\n    [\n");
	for (size_t i = 0; i < intern->size; ++i)
	{
		if (intern->data[i].value == NULL)
			continue;
		printf("        hash: %d, id: %d, value: %s\n", intern->data[i].symbol.hash, intern->data[i].symbol.id, intern->data[i].value);
	}
	printf("    ]\n");
	printf("}\n\n");
}

//=====================================================
// Testing
//=====================================================
void necro_test_intern_id(NecroIntern* intern, NecroSymbol symbol, const char* compare_str)
{
	printf("----\nstring: %s, hash: %d, symbol: %d\n", compare_str, symbol.hash, symbol.id);
	// ID Test
	if (symbol.id != NECRO_INTERN_NULL_ID)
		puts("NecroIntern id test:         passed");
	else
		puts("NecroIntern id test:         failed");

	// Hash Test
	if (symbol.hash == necro_hash_string(compare_str))
		puts("NecroIntern hash test:       passed");
	else
		puts("NecroIntern hash test:       failed");

	// Contains test
	if (necro_intern_contains_symbol(intern, symbol))
		puts("NecroIntern contains test:   passed");
	else
		puts("NecroIntern contains test:   failed");

	// Get test
	if (strcmp(necro_intern_get_string(intern, symbol), compare_str) == 0)
		puts("NecroIntern get test:        passed");
	else
		puts("NecroIntern get test:        failed");
}

void necro_test_intern()
{
	puts("Testing NecroIntern...");

	NecroIntern intern = necro_create_intern();

	// Test ID1
	NecroSymbol  id1    = necro_intern_string(&intern, "test");
	necro_test_intern_id(&intern, id1, "test");

	// Test ID2
	NecroSymbol  id2    = necro_intern_string(&intern, "This is not a test");
	necro_test_intern_id(&intern, id2, "This is not a test");

	// Test ID3
	const char*      test2 = "fuck yeah";
	NecroStringSlice slice = { test2, 4 };
	NecroSymbol      id3   = necro_intern_string_slice(&intern, slice);
	necro_test_intern_id(&intern, id3, "fuck");

	puts("\n---");
	necro_print_intern(&intern);
	puts("");

	// Destroy test
	necro_destroy_intern(&intern);
	if (intern.data == NULL)
		puts("NecroIntern destroy test:    passed");
	else
		puts("NecroIntern destroy test:    failed");

	// Grow test
	puts("---\nGrow test");
	intern = necro_create_intern();
	NecroSymbol symbols[NECRO_INITIAL_INTERN_SIZE];
	for (size_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
	{
		char num = '0' + (char) i;
		symbols[i] = necro_intern_string_slice(&intern, (NecroStringSlice) { &num, 1 });
	}

	bool grow_test_passed = true;
	for (size_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
	{
		grow_test_passed = grow_test_passed && necro_intern_contains_symbol(&intern, symbols[i]);
	}

	if (grow_test_passed)
		puts("NecroIntern grow test:       passed");
	else
		puts("NecroIntern grow test:       failed");

	// Grow Destroy test
	necro_destroy_intern(&intern);
	if (intern.data == NULL)
		puts("NecroIntern g destroy test:  passed");
	else
		puts("NecroIntern g destroy test:  failed");
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <assert.h>
#include "intern.h"

#define NECRO_INTERN_HASH_MULTIPLIER 33
#define NECRO_INTERN_NULL_ID         (size_t)-1

NecroIntern necro_create_intern()
{
	NecroInternEntry* data = malloc(NECRO_INITIAL_INTERN_SIZE * sizeof(NecroInternEntry));
	for (int32_t i = 0; i < NECRO_INITIAL_INTERN_SIZE; ++i)
	{
		data[i] = (NecroInternEntry) { NECRO_INTERN_NULL_ID, NULL };
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
	intern->count = 0;
	intern->size  = 0;
	free(intern->data);
	intern->data  = NULL;
}

size_t necro_hash_string(const char* str)
{
	// Assuming that char is 8 bytes...
	uint8_t* u_str = (uint8_t*) str;
	size_t   hash  = 0;
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
	size_t   hash  = 0;
	for (size_t i = 0; i < slice.length; ++i)
	{
		hash = hash * NECRO_INTERN_HASH_MULTIPLIER + *u_str;
		u_str++;
	}
	return hash;
}

bool necro_intern_contains_string(NecroIntern* intern, const char* str)
{
	if (str == NULL)
		return false;
	return necro_intern_contains_id(intern, necro_hash_string(str));
}

bool necro_intern_contains_id(NecroIntern* intern, size_t id)
{
	if (id == NECRO_INTERN_NULL_ID)
		return false;
	for (size_t probe = id % intern->size; intern->data[probe].value != NULL; probe = (probe + 1) % intern->size)
	{
		if (intern->data[probe].key == id)
			return true;
	}
	return false;
}

const char* necro_intern_get_string(NecroIntern* intern, size_t id)
{
	if (id == NECRO_INTERN_NULL_ID)
		return NULL;
	for (size_t probe = id % intern->size; intern->data[probe].value != NULL; probe = (probe + 1) % intern->size)
	{
		if (intern->data[probe].key == id)
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
		intern->data[i] = (NecroInternEntry) { NECRO_INTERN_NULL_ID, NULL };
	}
	// re-insert all entries from old block of memory
	for (size_t i = 0; i < old_size; ++i)
	{
		if (old_data[i].value != NULL)
		{
			size_t id    = old_data[i].key;
			size_t probe = id % intern->size;
			while (intern->data[probe].value != NULL)
			{
				probe = (probe + 1) % intern->size;
			}
			assert(intern->data[probe].value == NULL);
			intern->data[probe].key   = id;
			intern->data[probe].value = old_data[i].value;
		}
	}
	free(old_data);
}

size_t necro_intern_string(NecroIntern* intern, const char* str)
{
	if (str == NULL)
		return NECRO_INTERN_NULL_ID;
	if (intern->count >= intern->size / 2)
		necro_intern_grow(intern);
	size_t id    = necro_hash_string(str);
	size_t probe = id % intern->size;
	while (intern->data[probe].value != NULL)
	{
		if (intern->data[probe].key == id)
			return id;
		probe = (probe + 1) % intern->size;
	}
	assert(intern->data[probe].value == NULL);
	intern->data[probe].key   = id;
	intern->data[probe].value = _strdup(str);
	intern->count += 1;
	return id;
}

size_t necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice)
{
	if (slice.data == NULL)
		return NECRO_INTERN_NULL_ID;
	if (intern->count >= intern->size / 2)
		necro_intern_grow(intern);
	size_t id    = necro_hash_string_slice(slice);
	size_t probe = id % intern->size;
	while (intern->data[probe].value != NULL)
	{
		if (intern->data[probe].key == id)
			return id;
		probe = (probe + 1) % intern->size;
	}
	assert(intern->data[probe].value == NULL);
	intern->data[probe].key   = id;
	intern->data[probe].value = necro_dup_string_slice(slice);
	intern->count += 1;
	return id;
}

//=====================================================
// Testing
//=====================================================
void necro_test_intern_id(NecroIntern* intern, size_t id, const char* compare_str)
{
	printf("----\nstring: %s, id: %d\n", compare_str, id);
	// ID Test
	if (id != NECRO_INTERN_NULL_ID)
		puts("NecroIntern id test:         passed");
	else
		puts("NecroIntern id test:         failed");

	// Hash Test
	if (id == necro_hash_string(compare_str))
		puts("NecroIntern hash test:       passed");
	else
		puts("NecroIntern hash test:       failed");

	// Contains test
	if (necro_intern_contains_id(intern, id))
		puts("NecroIntern contains test:   passed");
	else
		puts("NecroIntern contains test:   failed");

	// Get test
	if (strcmp(necro_intern_get_string(intern, id), compare_str) == 0)
		puts("NecroIntern get test:        passed");
	else
		puts("NecroIntern get test:        failed");
}

void necro_test_intern()
{
	puts("Testing NecroIntern...");

	NecroIntern intern = necro_create_intern();

	// Test ID1
	size_t      id1    = necro_intern_string(&intern, "test");
	necro_test_intern_id(&intern, id1, "test");

	// Test ID2
	size_t      id2    = necro_intern_string(&intern, "This is not a test");
	necro_test_intern_id(&intern, id2, "This is not a test");

	// Test ID3
	const char*      test2 = "fuck yeah";
	NecroStringSlice slice = { test2, 4 };
	size_t           id3   = necro_intern_string_slice(&intern, slice);
	necro_test_intern_id(&intern, id3, "fuck");

	// Destroy test
	necro_destroy_intern(&intern);
	if (intern.data == NULL)
		puts("NecroIntern destroy test:    passed");
	else
		puts("NecroIntern destroy test:    failed");

	// Grow test
	puts("---\nGrow test");
	intern = necro_create_intern();
	for (size_t i = 0; i < intern.size; ++i)
	{
		char num = '0' + (char) i;
		necro_intern_string_slice(&intern, (NecroStringSlice) { &num, 1 });
	}

	bool grow_test_passed = true;
	for (size_t i = 0; i < intern.size; ++i)
	{
		char             num   = '0' + (char) i;
		NecroStringSlice slice = { &num, 1 };
		size_t           id    = necro_hash_string_slice(slice);
		grow_test_passed       = grow_test_passed && necro_intern_contains_id(&intern, id);
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

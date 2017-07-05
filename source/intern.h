/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef INTERN_H
#define INTERN_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utility.h"

//=====================================================
// NecroIntern:
//     * A simple string interning container
//     * Based on open addressing via linear probing
//     * All strings passed to this are duplicated,
//       thus it does not own, nor will it free any
//       strings which are passed into the api by the user.
//     * NULL strings will not be interned and are never considered interned.
//=====================================================
#define NECRO_INITIAL_INTERN_SIZE 256
typedef struct
{
	size_t key;
	char*  value;
} NecroInternEntry;

typedef struct
{
	NecroInternEntry* data;
	size_t            size;
	size_t            count;
} NecroIntern;

// API
NecroIntern necro_create_intern();
void        necro_destroy_intern(NecroIntern* intern);
bool        necro_intern_contains_string(NecroIntern* intern, const char* str);
bool        necro_intern_contains_id(NecroIntern* intern, size_t id);
size_t      necro_intern_string(NecroIntern* intern, const char* str);
size_t      necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice);
const char* necro_intern_get_string(NecroIntern* intern, size_t id);
void        necro_test_intern();

#endif // INTERN_H

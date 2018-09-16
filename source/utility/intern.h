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
#include "arena.h"

//=====================================================
// NecroIntern:
//     * A simple string interning container
//     * Based on open addressing via linear probing
//     * All strings passed to this are duplicated,
//       thus it does not own, nor will it free any
//       strings which are passed into the api by the user.
//     * NULL strings will not be interned and are never considered interned.
//     * The strings which are returned with NecroSymbol structs
//       are stable and can be compared directly with pointer comparisons.
//     * Still, it is considered best to store them as NecroSymbols
//       to make the distinction clear that these are interned strings
//=====================================================

NECRO_DECLARE_VECTOR(char, Char, char)

typedef struct
{
    // TODO: Keep hash and str, but not id
    size_t      hash;
    size_t      id;
    const char* str;
} NecroSymbol;

typedef struct NecroIntern
{
    NecroPagedArena arena;
    NecroSymbol*    symbols;
    size_t          size;
    size_t          count;
} NecroIntern;

// API
NecroIntern necro_intern_empty();
NecroIntern necro_intern_create();
void        necro_intern_destroy(NecroIntern* intern);
NecroSymbol necro_intern_string(NecroIntern* intern, const char* str);
NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice);
bool        necro_intern_contains_symbol(NecroIntern* intern, NecroSymbol symbol);
NecroSymbol necro_intern_concat_symbols(NecroIntern* intern, NecroSymbol symbol1, NecroSymbol symbol2);
NecroSymbol necro_intern_create_type_class_instance_symbol(NecroIntern* intern, NecroSymbol symbol, NecroSymbol type_class_name);
NecroSymbol necro_intern_get_type_class_member_symbol_from_instance_symbol(NecroIntern* intern, NecroSymbol symbol);
void        necro_intern_print(NecroIntern* intern);
void        necro_intern_test();

#define NULL_SYMBOL ((NecroSymbol){.id = 0, .hash = 0, .str = NULL})

#endif // INTERN_H

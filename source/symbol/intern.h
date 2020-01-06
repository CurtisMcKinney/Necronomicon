/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_INTERN_H
#define NECRO_INTERN_H 1

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

struct NecroInstSub;
struct NecroMachAst;
struct NecroMachProgram;

typedef enum
{
    NECRO_MANGLE_NAME,
    NECRO_DONT_MANGLE,
} NECRO_MANGLE_TYPE;

struct NecroSymbolData
{
    size_t                   hash;
    size_t                   symbol_num;
    size_t                   length;
    const char*              str;
    struct NecroMachAst*     global_string_value;
    struct NecroMachProgram* program;
};

typedef struct NecroSymbolData* NecroSymbol;

typedef struct NecroInternEntry
{
    size_t                  hash;
    struct NecroSymbolData* data;
} NecroInternEntry;

typedef struct NecroIntern
{
    NecroPagedArena    arena;
    NecroSnapshotArena snapshot_arena;
    NecroInternEntry*  entries;
    size_t             size;
    size_t             count;
    size_t             clash_suffix;
} NecroIntern;

NecroIntern necro_intern_empty();
NecroIntern necro_intern_create();
void        necro_intern_destroy(NecroIntern* intern);
void        necro_intern_test();

NecroSymbol necro_intern_string(NecroIntern* intern, const char* str);
NecroSymbol necro_intern_unique_string(NecroIntern* intern, const char* str);
NecroSymbol necro_intern_string_slice(NecroIntern* intern, NecroStringSlice slice);
bool        necro_intern_contains_symbol(NecroIntern* intern, NecroSymbol symbol);
NecroSymbol necro_intern_concat_symbols(NecroIntern* intern, NecroSymbol symbol1, NecroSymbol symbol2);
NecroSymbol necro_intern_create_type_class_instance_symbol(NecroIntern* intern, NecroSymbol symbol, NecroSymbol type_class_name);
NecroSymbol necro_intern_get_type_class_member_symbol_from_instance_symbol(NecroIntern* intern, NecroSymbol symbol);
void        necro_intern_print(NecroIntern* intern);
NecroSymbol necro_intern_append_suffix_from_subs(NecroIntern* intern, NecroSymbol prefix, const struct NecroInstSub* subs);

#endif // NECRO_INTERN_H

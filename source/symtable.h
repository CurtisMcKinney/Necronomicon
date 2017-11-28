/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "utility.h"
#include "intern.h"
#include "arena.h"

//=====================================================
// NecroSymTable, Contains info for a particular ID:
//         * ID
//         * Unique Name
//         * Human Readable Name
//         * Type
//         * Misc Attributes
//=====================================================

typedef struct
{
    size_t id;
} NecroID;

typedef struct
{
    size_t dummy;
} NecroType;

typedef struct
{
    NecroType   type;
    NecroSymbol unique_name;
    NecroSymbol reader_name;
    NecroID     id;
} NecroSymbolInfo;

typedef struct
{
    NecroSymbolInfo* data;
    size_t           size;
    size_t           count;
    NecroIntern*     intern;
} NecroSymTable;

// API
NecroSymTable necro_create_symtable(NecroIntern* intern);
void          necro_destroy_symtable(NecroSymTable* table);
NecroID       necro_symbtable_insert(NecroSymTable* table, NecroSymbolInfo info);
bool          necro_symbtable_find(NecroSymTable* table, NecroID id, NecroSymbolInfo* out_info);
void          necro_symtable_print(NecroSymTable* table);
void          necro_symtable_test();

// Scope symbol table
typedef struct
{
    NecroArena*          pool;
    size_t               parent_id;
    NecroSymTable*       global_table;
} NecroScopedSymTable;

#endif // SYMTABLE_H

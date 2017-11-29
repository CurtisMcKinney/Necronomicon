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
    uint32_t id;
} NecroID;

typedef struct
{
    size_t dummy;
} NecroType;

typedef struct
{
    size_t line;
    size_t character;
    size_t raw_index;
} NecroSourceLoc;

typedef struct
{
    NecroSymbol    name;
    NecroID        id;
    size_t         data_size;
    NecroType      type;
    size_t         local_var_num;
    NecroSourceLoc source_loc;
} NecroSymbolInfo;

typedef struct
{
    NecroSymbolInfo* data;
    size_t           size;
    size_t           count;
    NecroIntern*     intern;
} NecroSymTable;

// API
NecroSymTable    necro_create_symtable(NecroIntern* intern);
void             necro_destroy_symtable(NecroSymTable* table);
NecroID          necro_symtable_insert(NecroSymTable* table, NecroSymbolInfo info);
NecroSymbolInfo* necro_symtable_get(NecroSymTable* table, NecroID id);
void             necro_symtable_print(NecroSymTable* table);
void             necro_symtable_test();
NecroSymbolInfo  necro_create_initial_symbol_info(NecroSymbol symbol);

//=====================================================
// NecroScopedSymTable
//=====================================================
typedef struct NecroScopedHashtableNode
{
    NecroSymbol symbol;
    NecroID     id;
} NecroScopedHashtableNode;

typedef struct NecroScopedHashtable
{
    struct NecroScopedHashtable* parent;
    NecroScopedHashtableNode*    buckets;
    size_t                       size;
    size_t                       count;
} NecroScopedHashtable;

typedef struct
{
    NecroPagedArena       arena;
    NecroSymTable*        global_table;
    NecroScopedHashtable* current_scope;
} NecroScopedSymTable;

NecroScopedSymTable necro_create_scoped_symtable(NecroSymTable* global_table);
void                necro_destroy_scoped_symtable(NecroScopedSymTable* table);
void                necro_scoped_symtable_new_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_pop_scope(NecroScopedSymTable* table);
NecroID             necro_scoped_symtable_find(NecroScopedSymTable* table, NecroSymbol symbol);
NecroID             necro_scoped_symtable_new_symbol_info(NecroScopedSymTable* table, NecroSymbolInfo info);
void                necro_scoped_symtable_print(NecroScopedSymTable* table);
void                necro_scoped_symtable_test();

#endif // SYMTABLE_H
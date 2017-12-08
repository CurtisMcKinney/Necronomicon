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
#include "ast.h"
#include "arena.h"
#include "type.h"

//=====================================================
// NecroSymTable, Contains info for a particular ID:
//         * ID
//         * Unique Name
//         * Human Readable Name
//         * Type
//         * Misc Attributes
//=====================================================
struct NecroScope;

typedef struct
{
    NecroSymbol            name;
    NecroID                id;
    size_t                 data_size;
    size_t                 local_var_num;
    NecroSourceLoc         source_loc;
    struct NecroScope*     scope;
    NecroAST_Node_Reified* optional_type_signature;
    NecroType*             type;
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
NecroSymbolInfo  necro_create_initial_symbol_info(NecroSymbol symbol, NecroSourceLoc source_loc, struct NecroScope* scope);

//=====================================================
// NecroScopedSymTable
//=====================================================
typedef struct NecroScopedHashtableNode
{
    NecroSymbol symbol;
    NecroID     id;
} NecroScopeNode;

typedef struct NecroScope
{
    struct NecroScope* parent;
    NecroScopeNode*    buckets;
    size_t             size;
    size_t             count;
    NecroID            last_introduced_id;
} NecroScope;

typedef struct
{
    NecroPagedArena arena;
    NecroSymTable*  global_table;
    NecroScope*     top_scope;
    NecroScope*     current_scope;
    NecroScope*     top_type_scope;
    NecroScope*     current_type_scope;
    NecroError      error;
} NecroScopedSymTable;

NecroScopedSymTable necro_create_scoped_symtable(NecroSymTable* global_table);
void                necro_destroy_scoped_symtable(NecroScopedSymTable* table);

// Build API
void                necro_scoped_symtable_new_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_pop_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_new_type_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_pop_type_scope(NecroScopedSymTable* table);

// Names API
NECRO_RETURN_CODE   necro_build_scopes(NecroScopedSymTable* table, NecroAST_Reified* ast);
NecroID             necro_this_scope_find(NecroScope* scope, NecroSymbol symbol);
NecroID             necro_scope_find(NecroScope* scope, NecroSymbol symbol);
NecroID             necro_scoped_symtable_new_symbol_info(NecroScopedSymTable* table, NecroScope* scope, NecroSymbolInfo info);
void                necro_scope_set_last_introduced_id(NecroScope* scope, NecroID id);

// Test
void                necro_scoped_symtable_print_type_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_print(NecroScopedSymTable* table);
void                necro_scoped_symtable_test();
#endif // SYMTABLE_H
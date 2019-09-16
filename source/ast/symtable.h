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

// TODO: Clean up LOTS of shit in this.

//=====================================================
// NecroSymTable, Contains info for a particular ID:
//         * ID
//         * Unique Name
//         * Human Readable Name
//         * Type
//         * Misc Attributes
//=====================================================
struct NecroScope;
struct NecroTypeClass;
struct NecroTypeClassInstance;
struct NecroNodePrototype;
struct NecroCoreAST_Expression;
struct NecroMachineAST;
struct NecroDeclarationGroup;

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO (Curtis, 2-5-18):
// REMOVE THIS ONCE NecroAstSymbol SYSTEM
// IS FINISHED!!!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
typedef struct
{
    NecroSymbol                     name;
    size_t                          con_num;
    bool                            is_enum;
    NecroSourceLoc                  source_loc;
    struct NecroScope*              scope;
    NecroAst*                       ast;
    struct NecroCoreAST_Expression* core_ast;
    struct NecroDeclarationGroup*   declaration_group;
    NecroAst*                       optional_type_signature;
    NecroType*                      type;
    NecroType*                      closure_type;
    NECRO_TYPE_STATUS               type_status;
    struct NecroTypeClass*          method_type_class;
    struct NecroTypeClass*          type_class;
    struct NecroTypeClassInstance*  type_class_instance;
    int32_t                         arity; // -1 indicates arity has not been set
    uint32_t                        persistent_slot; // 0 indicates no persistence
    struct NecroMachineAST*         necro_machine_ast;
    bool                            is_constructor;
    bool                            is_recursive;
    NECRO_STATE_TYPE                state_type;
} NecroSymbolInfo;

typedef struct NecroSymTable
{
    NecroSymbolInfo* data;
    size_t           size;
    size_t           count;
    NecroIntern*     intern;
} NecroSymTable;

// API
NecroSymTable    necro_symtable_empty();
NecroSymTable    necro_symtable_create(NecroIntern* intern);
void             necro_symtable_destroy(NecroSymTable* table);
NecroSymbolInfo  necro_symtable_create_initial_symbol_info(NecroSymbol symbol, NecroSourceLoc source_loc, struct NecroScope* scope);

//=====================================================
// NecroScopedSymTable
//=====================================================
typedef struct NecroScopeNode
{
    NecroSymbol     symbol;
    NecroAstSymbol* ast_symbol;
} NecroScopeNode;

typedef struct NecroScope
{
    struct NecroScope* parent;
    NecroScopeNode*    buckets;
    size_t             size;
    size_t             count;
    NecroSymbol        last_introduced_symbol;
} NecroScope;

typedef struct NecroScopedSymTable
{
    NecroPagedArena  arena;
    NecroSymTable*   global_table;
    NecroScope*      top_scope;
    NecroScope*      current_scope;
    NecroScope*      top_type_scope;
    NecroScope*      current_type_scope;
} NecroScopedSymTable;

extern NecroScope necro_global_scope;

NecroScopedSymTable necro_scoped_symtable_empty();
NecroScopedSymTable necro_scoped_symtable_create(NecroSymTable* global_table);
void                necro_scoped_symtable_destroy(NecroScopedSymTable* table);

// Build API
void                necro_scoped_symtable_new_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_pop_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_new_type_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_pop_type_scope(NecroScopedSymTable* table);

// Names API
void                necro_build_scopes(NecroCompileInfo info, NecroScopedSymTable* table, NecroAstArena* ast);
void                necro_build_scopes_go(NecroScopedSymTable* scoped_symtable, NecroAst* input_node);
NecroScope*         necro_scope_create(NecroPagedArena* arena, NecroScope* parent);

void                necro_scope_print(NecroScope* scope, size_t whitespace, NecroIntern* intern);
bool                necro_scope_contains(NecroScope* scope, NecroSymbol symbol);
bool                necro_scope_is_subscope_of(NecroScope* super_scope, NecroScope* maybe_sub_scope);
void                necro_scope_insert_ast_symbol(NecroPagedArena* arena, NecroScope* scope, NecroAstSymbol* ast_symbol);
NecroAstSymbol*     necro_scope_find_ast_symbol(NecroScope* scope, NecroSymbol symbol);
NecroAstSymbol*     necro_scope_find_in_this_scope_ast_symbol(NecroScope* scope, NecroSymbol symbol);
NecroAstSymbol*     necro_symtable_get_top_level_ast_symbol(NecroScopedSymTable* scoped_symtable, NecroSymbol symbol);
NecroAstSymbol*     necro_symtable_get_type_ast_symbol(NecroScopedSymTable* scoped_symtable, NecroSymbol symbol);

void                necro_scoped_symtable_print_top_scopes(NecroScopedSymTable* table);
void                necro_scoped_symtable_print_type_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_print(NecroScopedSymTable* table);
void                necro_scoped_symtable_test();

#endif // SYMTABLE_H
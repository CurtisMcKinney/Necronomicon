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

#define NECRO_SYMTABLE_NULL_ID      ((NecroID) {0})

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

typedef struct
{
    NecroSymbol                     name;
    NecroID                         id;
    size_t                          con_num;
    bool                            is_enum;
    NecroSourceLoc                  source_loc;
    struct NecroScope*              scope;
    NecroASTNode*                   ast;
    struct NecroCoreAST_Expression* core_ast;
    NecroDeclarationGroup*          declaration_group;
    NecroAST_Node_Reified*          optional_type_signature;
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
NecroSymTable    necro_create_symtable(NecroIntern* intern);
void             necro_destroy_symtable(NecroSymTable* table);
NecroID          necro_symtable_insert(NecroSymTable* table, NecroSymbolInfo info);
NecroSymbolInfo* necro_symtable_get(NecroSymTable* table, NecroID id);
void             necro_symtable_print(NecroSymTable* table);
void             necro_symtable_test();
NecroSymbolInfo  necro_create_initial_symbol_info(NecroSymbol symbol, NecroSourceLoc source_loc, struct NecroScope* scope, NecroIntern* intern);
void             necro_print_env_with_symtable(NecroSymTable* table, NecroInfer* infer);

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
    NecroScopeNode*    id_buckets;
    size_t             size;
    size_t             count;
    NecroID            last_introduced_id;
} NecroScope;

typedef struct NecroScopedSymTable
{
    NecroPagedArena  arena;
    NecroSymTable*   global_table;
    NecroScope*      top_scope;
    NecroScope*      current_scope;
    NecroScope*      top_type_scope;
    NecroScope*      current_type_scope;
    NecroError       error;
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
void                necro_build_scopes_go(NecroScopedSymTable* scoped_symtable, NecroAST_Node_Reified* input_node);
NecroID             necro_this_scope_find(NecroScope* scope, NecroSymbol symbol);
NecroID             necro_scope_find(NecroScope* scope, NecroSymbol symbol);
NecroID             necro_scoped_symtable_new_symbol_info(NecroScopedSymTable* table, NecroScope* scope, NecroSymbolInfo info);
void                necro_scope_set_last_introduced_id(NecroScope* scope, NecroID id);
NecroID             necro_symtable_manual_new_symbol(NecroSymTable* symtable, NecroSymbol symbol);

NecroSymbolInfo*    necro_symtable_get_type_class_declaration_info(NecroSymTable* symtable, NecroAST_Node_Reified* ast);
NecroSymbolInfo*    necro_symtable_get_type_class_instance_info(NecroSymTable* symtable, NecroAST_Node_Reified* ast);
NecroVar            necro_get_top_level_symbol_var(NecroScopedSymTable* scoped_symtable, const char* name);
NecroVar            necro_get_type_symbol_var(NecroScopedSymTable* scoped_symtable, const char* name);

// Test
void                necro_scoped_symtable_print_type_scope(NecroScopedSymTable* table);
void                necro_scoped_symtable_print(NecroScopedSymTable* table);
void                necro_scoped_symtable_test();
#endif // SYMTABLE_H
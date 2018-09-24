/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_AST_SYMBOL_H
#define NECRO_AST_SYMBOL_H 1

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "intern.h"

struct NecroAst;
struct NecroScope;
struct NecroDeclarationGroup;
struct NecroType;
struct NecroCoreAst;
struct NecroMachineAST;
struct NecroMachineType;
struct NecroTypeClass;

typedef enum
{
    NECRO_TYPE_UNCHECKED,
    NECRO_TYPE_CHECKING,
    NECRO_TYPE_DONE
} NECRO_TYPE_STATUS;

typedef struct
{
    struct NecroAst*               ast;
    struct NecroAst*               optional_type_signature;
    struct NecroDeclarationGroup*  declaration_group;
    struct NecroType*              type;
    size_t                         con_num;
    bool                           is_constructor;
    NECRO_TYPE_STATUS              type_status;
    bool                           is_recursive;
    struct NecroTypeClass*         method_type_class;
    struct NecroTypeClass*         type_class;
    struct NecroTypeClassInstance* type_class_instance;
    struct NecroMachineAST*        necro_machine_ast;
} NecroAstSymbolData;

// name -> unique_name
// name += module_name
// Collapse NecroAstSymbolData into NecroAstSymbol and make it a pointer!
// Make NecroAstSymbols unique to that identified object and compare NecroAstSymbol pointers directly
typedef struct
{
    NecroSymbol         name;
    NecroSymbol         source_name;
    NecroSymbol         module_name;
    NecroAstSymbolData* ast_data;
} NecroAstSymbol;

static const NecroAstSymbol necro_ast_symbol_empty = { .name = NULL, .source_name = NULL, .module_name = NULL, .ast_data = NULL };

const char*         necro_ast_symbol_most_qualified_name(NecroAstSymbol ast_symbol);
NecroAstSymbolData* necro_ast_symbol_data_create(NecroPagedArena* arena, struct NecroAst* ast, struct NecroAst* optional_type_signature, struct NecroDeclarationGroup* declaration_group);

#endif // NECRO_AST_SYMBOL_H

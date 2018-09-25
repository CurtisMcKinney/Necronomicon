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

// Make NecroAstSymbols unique to that identified object and compare NecroAstSymbol pointers directly
typedef struct NecroAstSymbol
{
    NecroSymbol                    name;
    NecroSymbol                    source_name;
    NecroSymbol                    module_name;
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
} NecroAstSymbol;

NecroAstSymbol* necro_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, struct NecroAst* ast);
const char*     necro_ast_symbol_most_qualified_name(NecroAstSymbol* ast_symbol);

#endif // NECRO_AST_SYMBOL_H

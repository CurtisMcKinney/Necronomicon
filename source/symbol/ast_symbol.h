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
#include "list.h"

struct NecroAst;
struct NecroScope;
struct NecroDeclarationGroup;
struct NecroType;
struct NecroCoreAst;
struct NecroMachineAST;
struct NecroMachineType;
struct NecroTypeClass;
struct NecroTypeClassInstance;
struct NecroInstanceList;
struct NecroMachAstSymbol;
struct NecroUsage;

typedef enum
{
    NECRO_TYPE_UNCHECKED,
    NECRO_TYPE_CHECKING,
    NECRO_TYPE_DONE
} NECRO_TYPE_STATUS;

// TODO: Figure some way of partitioning this data. We need to get the size of this down.
///////////////////////////////////////////////////////
// NecroAstSymbol
//------------------
// Unique identifier and information container for any "Symbol" (i.e. identified object, such as Data Constructor, variables, Type Classes, etc)
// contained in the program being compiled.
///////////////////////////////////////////////////////
typedef struct NecroAstSymbol
{
    NecroSymbol                    name;                    // Most fully qualified version of the name of the NecroAstSymbol, which should be unique to the entire project and all included modules. takes the form: ModuleName.sourceName_clashSuffix
    NecroSymbol                    source_name;             // The name of the NecroAstSymbol as it appears in the source code.
    NecroSymbol                    module_name;             // The name of the module that contains the NecroAstSymbol.
    struct NecroAst*               ast;                     // Pointer to the actual ast node that this symbol identifies.
    struct NecroAst*               optional_type_signature; // Type signature of the symbol in NecroAst form, if present. Resolved after reification phase.
    struct NecroAst*               declaration_group;       // Declaration group of the symbol, if present. Resolved after d_analysis phase.
    struct NecroType*              type;                    // Type of the symbol, if present. Resolved after inference phase.
    size_t                         con_num;                 // Constructor Number, if present. This is the order in the constructor list of a data object a constructor sits at. Resolved after inference phase and used in code generation phase.
    bool                           is_enum;                 // Whether or not this type is an enum type. Resolved in necro_infer.
    NECRO_TYPE_STATUS              type_status;             // Type checking status of the symbol. Useful for detecting recursion in the ast.
    bool                           is_constructor;          // Whether or not the symbol is a constructor (HACK?)
    bool                           is_recursive;            // Whether or not symbol is recursive. Create an enum for this?
    struct NecroTypeClass*         method_type_class;       // Type class for a class method, if present. Resolved at inference phase.
    struct NecroTypeClass*         type_class;              // Type class, if present. Resolved at inference phase.
    struct NecroTypeClassInstance* type_class_instance;     // Class instance, if present. Resolved at inference phase.
    struct NecroInstanceList*      instance_list;           // List of type classes this symbol is an instance of. Resolved at inference phase.
    struct NecroMachAstSymbol*     mach_symbol;             // Resolved at necro_mach_translate.
    struct NecroUsage*             usage;                   // Conflicting usages (In the sharing sense) gathered during alias analysis phase.
    struct NecroMachineAST*        necro_machine_ast;       // NecroMachineAST that this symbol was compiled into. Generated at NecroMachine compilation phase.
} NecroAstSymbol;

NecroAstSymbol* necro_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, struct NecroAst* ast);
const char*     necro_ast_symbol_most_qualified_name(NecroAstSymbol* ast_symbol);
void            necro_ast_symbol_print_type_and_kind(NecroAstSymbol* ast_symbol, size_t num_white_spaces);
NecroAstSymbol* necro_ast_symbol_deep_copy(NecroPagedArena* arena, NecroAstSymbol* ast_symbol);

#endif // NECRO_AST_SYMBOL_H

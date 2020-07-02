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

//--------------------
// Forward Declarations
//--------------------
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
struct NecroCoreAst;
struct NecroCoreAstSymbol;
struct NecroCoreScope;
struct NecroStaticValue;
struct NecroConstraintList;

typedef enum
{
    NECRO_TYPE_UNCHECKED,
    NECRO_TYPE_CHECKING,
    NECRO_TYPE_DONE
} NECRO_TYPE_STATUS;

typedef enum
{
    NECRO_STATE_CONSTANT  = 0,
    NECRO_STATE_POLY      = 1,
    NECRO_STATE_POINTWISE = 2,
    NECRO_STATE_STATEFUL  = 3,
} NECRO_STATE_TYPE; // Used for state analysis and in necromachine

typedef enum
{
    NECRO_PRIMOP_NONE    = 0,
    NECRO_PRIMOP_PRIM_FN = 1,
    NECRO_PRIMOP_PRIM_VAL,

    NECRO_PRIMOP_UNBOXED_CON,
    NECRO_PRIMOP_PROJ,

    NECRO_PRIMOP_UOP_IABS,
    NECRO_PRIMOP_UOP_UABS,
    NECRO_PRIMOP_UOP_ISGN,
    NECRO_PRIMOP_UOP_USGN,
    NECRO_PRIMOP_UOP_ITOI,
    NECRO_PRIMOP_UOP_ITOU,
    NECRO_PRIMOP_UOP_ITOF,
    NECRO_PRIMOP_UOP_ITOFV,
    NECRO_PRIMOP_UOP_UTOI,
    NECRO_PRIMOP_UOP_FTRI,
    NECRO_PRIMOP_UOP_FRNI,
    NECRO_PRIMOP_UOP_FTRU,
    NECRO_PRIMOP_UOP_FTOF,
    NECRO_PRIMOP_UOP_FTOFV,
    NECRO_PRIMOP_UOP_FFLR,
    NECRO_PRIMOP_UOP_FFLR_TO_INT,
    NECRO_PRIMOP_UOP_FCEIL_TO_INT,
    NECRO_PRIMOP_UOP_FTRNC_TO_INT,
    NECRO_PRIMOP_UOP_FRND_TO_INT,
    NECRO_PRIMOP_UOP_FBREV,
    NECRO_PRIMOP_UOP_FTOB,
    NECRO_PRIMOP_UOP_FFRB,
    NECRO_PRIMOP_UOP_NOT,
    NECRO_PRIMOP_UOP_FNOT,
    NECRO_PRIMOP_UOP_BIT_CAST,

    NECRO_PRIMOP_BINOP_IADD,
    NECRO_PRIMOP_BINOP_ISUB,
    NECRO_PRIMOP_BINOP_IMUL,
    NECRO_PRIMOP_BINOP_IDIV,
    NECRO_PRIMOP_BINOP_IREM,
    NECRO_PRIMOP_BINOP_UADD,
    NECRO_PRIMOP_BINOP_USUB,
    NECRO_PRIMOP_BINOP_UMUL,
    NECRO_PRIMOP_BINOP_UDIV,
    NECRO_PRIMOP_BINOP_UREM,
    NECRO_PRIMOP_BINOP_FADD,
    NECRO_PRIMOP_BINOP_FSUB,
    NECRO_PRIMOP_BINOP_FMUL,
    NECRO_PRIMOP_BINOP_FDIV,
    NECRO_PRIMOP_BINOP_FREM,
    NECRO_PRIMOP_BINOP_FVADD,
    NECRO_PRIMOP_BINOP_FVSUB,
    NECRO_PRIMOP_BINOP_FVMUL,
    NECRO_PRIMOP_BINOP_FVDIV,
    NECRO_PRIMOP_BINOP_FVREM,
    NECRO_PRIMOP_BINOP_AND,
    NECRO_PRIMOP_BINOP_OR,
    NECRO_PRIMOP_BINOP_SHL,
    NECRO_PRIMOP_BINOP_SHR,
    NECRO_PRIMOP_BINOP_SHRA,
    NECRO_PRIMOP_BINOP_XOR,
    NECRO_PRIMOP_BINOP_FAND,
    NECRO_PRIMOP_BINOP_FOR,
    NECRO_PRIMOP_BINOP_FSHL,
    NECRO_PRIMOP_BINOP_FSHR,
    NECRO_PRIMOP_BINOP_FSHRA,
    NECRO_PRIMOP_BINOP_FXOR,

    NECRO_PRIMOP_CMP_EQ,
    NECRO_PRIMOP_CMP_NE,
    NECRO_PRIMOP_CMP_GT,
    NECRO_PRIMOP_CMP_GE,
    NECRO_PRIMOP_CMP_LT,
    NECRO_PRIMOP_CMP_LE,

    NECRO_PRIMOP_ARRAY_EMPTY,
    NECRO_PRIMOP_ARRAY_FREEZE,
    NECRO_PRIMOP_ARRAY_READ,
    NECRO_PRIMOP_ARRAY_READU,
    NECRO_PRIMOP_ARRAY_WRITE,
    NECRO_PRIMOP_ARRAY_TO_FV,
    NECRO_PRIMOP_ARRAY_FROM_FV,

    // TODO: Replace TO_FV and FROM_FV with BIT_CAST

    // NECRO_PRIMOP_FV_EMPTY,
    // NECRO_PRIMOP_FV_FREEZE,
    NECRO_PRIMOP_FV_INSERT,
    NECRO_PRIMOP_FV_EXTRACT,

    NECRO_PRIMOP_PTR_ALLOC,
    NECRO_PRIMOP_PTR_REALLOC,
    NECRO_PRIMOP_PTR_FREE,
    NECRO_PRIMOP_PTR_POKE,
    NECRO_PRIMOP_PTR_SWAP,
    NECRO_PRIMOP_PTR_PEEK,
    NECRO_PRIMOP_PTR_CAST,
    NECRO_PRIMOP_PTR_EQ,

    NECRO_PRIMOP_MREF,
    NECRO_PRIMOP_NAT_VAL,

    NECRO_PRIMOP_PTHUNK_EVAL,
    NECRO_PRIMOP_DYN_DCOPY,
    NECRO_PRIMOP_DYN_DCOPY_INTO,

    NECRO_PRIMOP_INTR_FMA,
    NECRO_PRIMOP_INTR_BREV,
    NECRO_PRIMOP_INTR_FABS,
    NECRO_PRIMOP_INTR_SIN,
    NECRO_PRIMOP_INTR_COS,
    NECRO_PRIMOP_INTR_EXP,
    NECRO_PRIMOP_INTR_EXP2,
    NECRO_PRIMOP_INTR_LOG,
    NECRO_PRIMOP_INTR_LOG10,
    NECRO_PRIMOP_INTR_LOG2,
    NECRO_PRIMOP_INTR_POW,
    NECRO_PRIMOP_INTR_SQRT,
    NECRO_PRIMOP_INTR_FFLR,
    NECRO_PRIMOP_INTR_FCEIL,
    NECRO_PRIMOP_INTR_FTRNC,
    NECRO_PRIMOP_INTR_FRND,
    NECRO_PRIMOP_INTR_FCPYSGN,
    NECRO_PRIMOP_INTR_FMIN,
    NECRO_PRIMOP_INTR_FMAX,

} NECRO_PRIMOP_TYPE;


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
    struct NecroType*              sig_type_var_ulist;      // List of uniqueness types for the specified type variable that were generated during the creation of a type signature.
    struct NecroTypeClass*         method_type_class;       // Type class for a class method, if present. Resolved at inference phase.
    struct NecroTypeClass*         type_class;              // Type class, if present. Resolved at inference phase.
    struct NecroTypeClassInstance* type_class_instance;     // Class instance, if present. Resolved at inference phase.
    struct NecroInstanceList*      instance_list;           // List of type classes this symbol is an instance of. Resolved at inference phase.
    struct NecroCoreAstSymbol*     core_ast_symbol;         // Resolved at necro_ast_transform_to_core
    struct NecroMachAstSymbol*     mach_symbol;             // Resolved at necro_mach_translate.
    struct NecroUsage*             usage;                   // Conflicting usages (In the sharing sense) gathered during alias analysis phase.
    struct NecroConstraintList*    constraints;             // List of constraints placed on this type variable
    struct NecroMachineAST*        necro_machine_ast;       // NecroMachineAST that this symbol was compiled into. Generated at NecroMachine compilation phase.
    size_t                         con_num;                 // Constructor Number, if present. This is the order in the constructor list of a data object a constructor sits at. Resolved after inference phase and used in code generation phase.
    NECRO_TYPE_STATUS              type_status;             // Type checking status of the symbol. Useful for detecting recursion in the ast.
    NECRO_PRIMOP_TYPE              primop_type;             // Defines a primop for this symbol, set in base.c
    bool                           is_top_level;            // Whether this is a local binding or a global top level binding
    bool                           is_class_head_var;       // Used for instancing class signatures
    bool                           is_enum;                 // Whether or not this type is an enum type. Resolved in necro_infer.
    bool                           is_constructor;          // Whether or not the symbol is a constructor (HACK?)
    bool                           is_recursive;            // Whether or not symbol is recursive. Create an enum for this?
    bool                           is_primitive;            // Whether or not a symbol is primitive.
    bool                           is_unboxed;
    bool                           is_wrapper;              // Equivalant to newtype
    bool                           never_inline;
} NecroAstSymbol;

NecroAstSymbol* necro_ast_symbol_create(NecroPagedArena* arena, NecroSymbol name, NecroSymbol source_name, NecroSymbol module_name, struct NecroAst* ast);
const char*     necro_ast_symbol_most_qualified_name(NecroAstSymbol* ast_symbol);
void            necro_ast_symbol_print_type_and_kind(NecroAstSymbol* ast_symbol, size_t num_white_spaces);
NecroAstSymbol* necro_ast_symbol_deep_copy(NecroPagedArena* arena, NecroAstSymbol* ast_symbol);

typedef struct NecroCoreAstSymbol
{
    NecroSymbol                name;
    NecroSymbol                source_name;
    NecroSymbol                module_name;
    struct NecroCoreAst*       ast;
    struct NecroCoreAst*       inline_ast;
    struct NecroType*          type;
    struct NecroCoreScope*     free_vars;
    struct NecroStaticValue*   static_value;
    struct NecroMachAstSymbol* mach_symbol;
    struct NecroCoreAstSymbol* outer;
    struct NecroCoreAstSymbol* deep_copy_fn;
    NECRO_STATE_TYPE           state_type;
    NECRO_PRIMOP_TYPE          primop_type;
    size_t                     arity;
    size_t                     con_num;
    bool                       is_constructor;
    bool                       is_enum;
    bool                       is_recursive;
    bool                       is_primitive;
    bool                       is_unboxed;
    bool                       is_wrapper;
    bool                       is_deep_copy_fn;
    bool                       is_wildcard;
    bool                       never_inline;
} NecroCoreAstSymbol;

NecroCoreAstSymbol* necro_core_ast_symbol_create(NecroPagedArena* core_ast_arena, NecroSymbol name, struct NecroType* type);
NecroCoreAstSymbol* necro_core_ast_symbol_create_from_ast_symbol(NecroPagedArena* core_ast_arena, NecroAstSymbol* ast_symbol); // NOTE: This deep copies all information, and thus does not depend on any memory from the ast_symbol afterwards.
NecroCoreAstSymbol* necro_core_ast_symbol_create_by_renaming(NecroPagedArena* core_ast_arena, NecroSymbol new_name, NecroCoreAstSymbol* ast_symbol); // NOTE: This deep copies all information, and thus does not depend on any memory from the ast_symbol afterwards.
const char*         necro_core_ast_symbol_most_qualified_name(NecroCoreAstSymbol* core_ast_symbol);

#endif // NECRO_AST_SYMBOL_H

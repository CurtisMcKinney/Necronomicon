/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_H
#define CORE_H 1

#include <stdio.h>

#include "ast.h"
#include "infer.h"
#include "parser.h"
#include "prim.h"

/*
///////////////////////////////////////////////////////
// TODO / NOTE for Chad!!! (2-23-19)
///////////////////////////////////////////////////////

Core translation requires a large overhaul after all the changes in the frontend:

    * Phase changes:
      On the lage scale, compiler phases now have a more uniform way of behaving,
      and the Core Translation phase needs to come into line with this.
      Consult necro_compile_go found in driver.c to see how all the phases before Core are being handled.
      Broadly, the phase should be thought of as taking inputs, doing some transformation, and producing outputs.
      The state involved in the process should be split into two camps.
      The first camp is state which is internal to the transformation itself, and can be discarded afterward.
      This should be handled completely internally by the phase and should not be visible nor accessible outside of the phase.
      The second camp is state which will be required by phases downstream.
      This state should be introduced in an empty state at the beginning of necro_compile,
      then it will be initialized and mutated by the core translation phase,
      then cleaned up and freed at the end of necro_compile.

    * Naming Conventions:
      Also naming has become more unified in the other phases.
      The current convention is that functions should be name as:
        necro_system_function_name
      Where system is whatever system the function belongs to, such as ast, intern, or lex.
      In core things should be generally named necro_core_function_name.
      Furthermore the Ast data types have more unified naming.
      Specifically the NecroParseAst and NecroAst are the name for the recursive
      AST type itself, while NecroParseAstArena and NecroAstArena is a struct which contains
      The root Ast node, the memory handling NecroPagedArena, and whatever else is required.

    * Symbols:
      The way symbols are being handled has changed dramatically.
      There are two kinds of "symbols":
        - NecroSymbol:    This is essentially a unique immutable string with some extra info, created by NecroIntern
        - NecroAstSymbol: This is an identifier and storehold for named objects in the AST (Data types, functions, etc).
      Core needs to use this new system. However, NecroAstSymbol has a lot of NecroAst specific stuff,
      I'm unsure if this warrants the creation of a new AstSymbol type to handle core specifically named something like NecroCoreAstSymbol.
      I will leave this up to you to suss out.

    * DeclarationGroupLists and DeclarationGroups:
      NecroAst has changed and by the time it reaches Core all declarations use a "list of lists" system.
      DeclarationGroupLists is a list node structure who's item member is a
      Declaration list node structure, whose item member is an Ast node.
      Look at a print out of some of the asts to get a more concrete idea of what I mean.
      If there's only one Declaration list node then it should be transformed into a simple Bind CoreAst node.
      If there's more than one Declaration list node in the declaration list that means it's a recursive group
      and should be bound with a BindRec CoreAst node.

    * Pruning
      The Ast that arrives is going to have a lot of noise that needs to be filtered out.
      TypeSignatures are no longer necessary and should be ignored.
      Class declarations are no longer necessary and should be ignored.
      Instance declarations themselves are no longer necessary, but the
      declaration group list that they contain needs to be translated and moved into the top most decaration group list.
      The ast is monomorphized before arriving to core translation, so
      polymorphic values and functions (but not Data declarations) need to be dropped, as they are vestigial
      You can use necro_type_is_polymorphic to test the type of an Ast node.
      There's probably more I'm forgetting. Ask if you have any questions.

    * Types
        Core should now be fully typed. The language will be monomorphic by the time it reaches core translation,
        so there will be no need for "Big lambda" as is used in Haskell's core translation.
        I believe you should be able to keep using NecroType as the main type struct.
        These types should be preserved by the core translation.
        If a more complex translation occures which maps nodes to a different node structure then types
        for this new node structure should be derived programmatically.
        At the end of translation a very simple sanity checking type checking should be done on the Core Ast to
        make sure the translation worked properly.
        Also make sure that the NecroAstSymbol or NecroCoreAstSymbol structs have proper types associated with them as well.
        Every Ast node except for list type nodes should have a concrete type associated with it.

    * NecroBase
        The way things are working currently is that there really is spearate compilation of modules.
        This is somewhat by design as the new monomorphization phase which replaced typeclass dictionary passing translation
        requires whole program analysis.
        That said currently there are basically two "modules", Main and Base.
        Main is the user's code that's being compiled and Base is the base set of types and functions
        provided by the compiler itself. Currently there are no separate modules, or prelude.
        At Core Translation these actually need to merge into one mega NecroCoreAst which contains everything.
        I'll leave you to figure out how to best do this.

    * Optimizations / Inlining
        Don't worry about this at all currently as it will need to be handled with care to preserve the semantics of the language.
        As the semantics of the language itself are still shifting it's best not to get started on this.

    * Downstream
        All of these changes will likely break phases further down stream (LambdaLifting, ClosureConverion, NecroMachine translation, JIT).
        How to handle this properly? I'll let you decide, but IMHO, the easy solution would be to refactor the current set of structs and functions
        to names with DEPRECATED appended to the end, then to create a new batch of structs and functions upon which nothing downstream relies upon.
        Once you're done we can then work on retrofitting downstream phses to operate given the new reality.

*/

#if 0
//////////////////////
// Types
//////////////////////

typedef struct
{
    struct NecroCoreAST_Type* typeA;
    struct NecroCoreAST_Type* typeB;
} NecroCoreAST_AppType;

typedef struct
{
    NecroVar id;
} NecroCoreAST_TypeCon;

typedef struct
{
    NecroCoreAST_TypeCon* typeCon;
    struct NecroCoreAST_TypeList* types;
} NecroCoreAST_TyConApp;

typedef struct
{
    union
    {
        NecroVar tyvarTy;
        NecroAST_ConstantType liftTy;
        NecroCoreAST_AppType* appTy;
        NecroCoreAST_TyConApp* tyConApp;
    };

    enum
    {
        NECRO_CORE_AST_TYPE_VAR,
        NECRO_CORE_AST_TYPE_LIT,
        NECRO_CORE_AST_TYPE_APP,
        NECRO_CORE_AST_TYPE_TYCON_APP,
    } type;
} NecroCoreAST_Type;

typedef struct
{
    NecroCoreAST_Type* type;
    NecroCoreAST_Type* next;
} NecroCoreAST_TypeList;
#endif

//////////////////////
// Expressions
//////////////////////
struct NecroCoreAST_Expression;

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO (Curtis, 2/5/18): Port core over to using new NecroAstSymbol system !!
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

typedef struct NecroCoreAST_Bind
{
    NecroVar var;
    struct NecroCoreAST_Expression* expr;
    bool                            is_recursive; // Curtis: Metadata for codegen
} NecroCoreAST_Bind;

typedef struct NecroCoreAST_BindRec
{
    struct NecroCoreAST_Expression* binds;
} NecroCoreAST_BindRec;

typedef struct NecroCoreAST_Application
{
    struct NecroCoreAST_Expression* exprA;
    struct NecroCoreAST_Expression* exprB;
    uint32_t                        persistent_slot; // Curtis: Metadata for codegen
} NecroCoreAST_Application;

typedef struct NecroCoreAST_Lambda
{
    struct NecroCoreAST_Expression* arg;
    struct NecroCoreAST_Expression* expr;
} NecroCoreAST_Lambda;

typedef struct NecroCoreAST_Let
{
    struct NecroCoreAST_Expression* bind;
    struct NecroCoreAST_Expression* expr;
} NecroCoreAST_Let;

typedef struct NecroCoreAST_DataCon
{
    NecroVar condid;
    struct NecroCoreAST_Expression* arg_list;
    struct NecroCoreAST_DataCon* next;
} NecroCoreAST_DataCon;

typedef struct NecroCoreAST_DataDecl
{
    NecroVar data_id;
    struct NecroCoreAST_DataCon* con_list;
} NecroCoreAST_DataDecl;

typedef struct NecroCoreAST_CaseAlt
{
    struct NecroCoreAST_Expression* altCon; // NULL altCon is a wild card
    struct NecroCoreAST_Expression* expr;
    struct NecroCoreAST_CaseAlt* next;
} NecroCoreAST_CaseAlt;

typedef struct
{
    struct NecroCoreAST_Expression* expr;
    //NecroVar var; // Don't know what to do with var and type atm
    //struct NecroCoreAST_Type* type;
    NecroCoreAST_CaseAlt* alts;
    NecroType*            type;
} NecroCoreAST_Case;

typedef struct
{
    NecroType* type;
} NecroCoreAST_Type;

typedef struct
{
    struct NecroCoreAST_Expression* expr;
    struct NecroCoreAST_Expression* next;
} NecroCoreAST_List;

typedef enum
{
    NECRO_CORE_EXPR_VAR,
    NECRO_CORE_EXPR_BIND,
    NECRO_CORE_EXPR_LIT,
    NECRO_CORE_EXPR_APP,
    NECRO_CORE_EXPR_LAM,
    NECRO_CORE_EXPR_LET,
    NECRO_CORE_EXPR_CASE,
    NECRO_CORE_EXPR_TYPE,
    NECRO_CORE_EXPR_LIST, // used for top decls not language lists
    NECRO_CORE_EXPR_DATA_DECL,
    NECRO_CORE_EXPR_DATA_CON,
    NECRO_CORE_EXPR_COUNT,
    NECRO_CORE_EXPR_UNIMPLEMENTED,
} NECRO_CORE_EXPR;

typedef struct NecroCoreAST_Expression
{
    union
    {
        NecroVar var;
        NecroCoreAST_Bind bind;
        NecroAstConstant lit;
        NecroCoreAST_Application app;
        NecroCoreAST_Lambda lambda;
        NecroCoreAST_Let let;
        NecroCoreAST_Case case_expr;
        NecroCoreAST_Type type;
        NecroCoreAST_List list;
        NecroCoreAST_DataDecl data_decl;
        NecroCoreAST_DataCon data_con;
    };
    NECRO_CORE_EXPR expr_type;
    NecroType*      necro_type;
} NecroCoreAST_Expression;

#if 0 // unused currently
static const char* core_ast_names[] =
{
    "NECRO_CORE_EXPR_VAR",
    "NECRO_CORE_EXPR_BIND",
    "NECRO_CORE_EXPR_LIT",
    "NECRO_CORE_EXPR_APP",
    "NECRO_CORE_EXPR_LAM",
    "NECRO_CORE_EXPR_LET",
    "NECRO_CORE_EXPR_CASE",
    "NECRO_CORE_EXPR_TYPE",
    "NECRO_CORE_EXPR_LIST",
    "NECRO_CORE_EXPR_DATA_DECL",
    "NECRO_CORE_EXPR_DATA_CON",
    "NECRO_CORE_EXPR_COUNT",
    "NECRO_CORE_EXPR_UNIMPLEMENTED"
};
#endif

typedef enum
{
    NECRO_CORE_TRANSFORMING,
    NECRO_CORE_TRANSFORM_ERROR,
    NECRO_CORE_TRANSFORM_DONE
} NecroParse_CoreTransformState;

typedef struct NecroCoreAST
{
    NecroPagedArena arena;
    NecroCoreAST_Expression* root;
} NecroCoreAST;

typedef struct
{
    NecroVar twoTuple;
    NecroVar threeTuple;
    NecroVar fourTuple;
    NecroVar fiveTuple;
    NecroVar sixTuple;
    NecroVar sevenTuple;
    NecroVar eightTuple;
    NecroVar nineTuple;
    NecroVar tenTuple;
} NecroCoreConstructors;

typedef struct
{
    NecroCoreAST* core_ast;
    NecroAstArena* necro_ast;
    NecroIntern* intern;
    NecroPrimTypes* prim_types;
    NecroParse_CoreTransformState transform_state;
    NecroSymTable* symtable;
    NecroCoreConstructors constructors;
} NecroTransformToCore;

void necro_transform_to_core(NecroTransformToCore* core_transform);
void necro_print_core(NecroCoreAST* ast, NecroIntern* intern);

NecroTransformToCore necro_empty_core_transform();
void necro_construct_core_transform(
    NecroTransformToCore* core_transform,
    NecroCoreAST* core_ast,
    NecroAstArena* necro_ast,
    NecroIntern* intern,
    NecroPrimTypes* prim_types,
    NecroSymTable* symtable,
    NecroScopedSymTable* scoped_symtable);

static inline void necro_destruct_core_transform(NecroTransformToCore* core_transform)
{
    // Ownership of CoreAST is passed out of NecroCoreTransform
    // if (core_transform->core_ast != NULL)
    //     necro_destroy_paged_arena(&core_transform->core_ast->arena);
    core_transform->core_ast = NULL;
    core_transform->necro_ast = NULL;
    core_transform->intern = NULL;
    core_transform->transform_state = NECRO_CORE_TRANSFORM_DONE;
}

NecroCoreAST necro_empty_core_ast();
void         necro_destroy_core_ast(NecroCoreAST* ast);

#endif // CORE_H

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_AST_H
#define CORE_AST_H 1

#include <stdio.h>
#include "ast.h"
#include "infer.h"
#include "base.h"

/*
///////////////////////////////////////////////////////
// TODO / NOTE for Chad!!! (2-23-19)
///////////////////////////////////////////////////////

    TODO
        * BIND_REC

Core translation requires a large overhaul after all the changes in the frontend:


    * TODO / NOTE: Perform Deep copy of Ast + Types + AstSymbols needed info and translation

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

    * One last Error:
        There is one last user facing error that can occur here: Incomplete case expressions. Thus we use NecroResult(NecroCoreAst) during transformation

*/

//--------------------
// NecroCoreAst
//--------------------
struct NecroCoreAst;
NECRO_DECLARE_ARENA_LIST(struct NecroCoreAst*, CoreAst, core_ast);
typedef struct NecroCoreAstLiteral
{
    union
    {
        double            float_literal;
        int64_t           int_literal;
        uint64_t          uint_literal;
        NecroSymbol       string_literal;
        uint32_t          char_literal;
        NecroCoreAstList* array_literal_elements;
    };
    NECRO_CONSTANT_TYPE type;
    size_t              persistent_slot;
} NecroCoreAstLiteral;

typedef struct NecroCoreAstVar
{
    NecroCoreAstSymbol* ast_symbol;
    size_t              persistent_slot;
} NecroCoreAstVar;

typedef struct NecroCoreAstBind
{
    NecroCoreAstSymbol*  ast_symbol;
    struct NecroCoreAst* expr;
    // bool                 is_recursive; // TODO / NOTE: Do we still need this?
    // TODO: How to handle initializers?
} NecroCoreAstBind;

// TODO: Make recursive binds use this
typedef struct NecroCoreAstBindRec
{
    struct NecroCoreAstList* binds;
} NecroCoreAstBindRec;

typedef struct NecroCoreAstApplication
{
    struct NecroCoreAst* expr1;
    struct NecroCoreAst* expr2;
    size_t               persistent_slot; // Curtis: Metadata for codegen
} NecroCoreAstApplication;

typedef struct NecroCoreAstLambda
{
    struct NecroCoreAst* arg;
    struct NecroCoreAst* expr;
} NecroCoreAstLambda;

typedef struct NecroCoreAstLet
{
    struct NecroCoreAst* bind;
    struct NecroCoreAst* expr;
} NecroCoreAstLet;

typedef struct NecroCoreAstDataCon
{
    NecroCoreAstSymbol* ast_symbol;
    NecroType*          type;
    NecroType*          data_type_type;
} NecroCoreAstDataCon;

typedef struct NecroCoreAstDataDecl
{
    NecroCoreAstSymbol*  ast_symbol;
    NecroCoreAstList*    con_list;
} NecroCoreAstDataDecl;

typedef struct NecroCoreAstCaseAlt
{
    struct NecroCoreAst* pat; // NULL if wild card
    struct NecroCoreAst* expr;
} NecroCoreAstCaseAlt;

typedef struct NecroCoreAstCase
{
    struct NecroCoreAst* expr;
    NecroCoreAstList*    alts;
} NecroCoreAstCase;

typedef struct NecroCoreAstForLoop
{
    struct NecroCoreAst* range_init;
    struct NecroCoreAst* value_init;
    struct NecroCoreAst* index_arg;
    struct NecroCoreAst* value_arg;
    struct NecroCoreAst* expression;
    size_t               max_loops;
    size_t               persistent_slot;
} NecroCoreAstForLoop;

typedef enum
{
    NECRO_CORE_AST_VAR,
    NECRO_CORE_AST_BIND,
    NECRO_CORE_AST_BIND_REC,
    NECRO_CORE_AST_LIT,
    NECRO_CORE_AST_APP,
    NECRO_CORE_AST_LAM,
    NECRO_CORE_AST_LET,
    NECRO_CORE_AST_CASE,
    NECRO_CORE_AST_CASE_ALT,
    NECRO_CORE_AST_FOR,
    NECRO_CORE_AST_DATA_DECL,
    NECRO_CORE_AST_DATA_CON,
} NECRO_CORE_AST_TYPE;

typedef struct NecroCoreAst
{
    union
    {
        NecroCoreAstVar         var;
        NecroCoreAstBind        bind;
        NecroCoreAstBindRec     bind_rec;
        NecroCoreAstLiteral     lit;
        NecroCoreAstApplication app;
        NecroCoreAstLambda      lambda;
        NecroCoreAstLet         let;
        NecroCoreAstCase        case_expr;
        NecroCoreAstCaseAlt     case_alt;
        NecroCoreAstForLoop     for_loop;
        NecroCoreAstDataDecl    data_decl;
        NecroCoreAstDataCon     data_con;
    };
    NECRO_CORE_AST_TYPE ast_type;
    NecroType*          necro_type;
} NecroCoreAst;

//--------------------
// NecroCoreAstArena
//--------------------
typedef struct NecroCoreAstArena
{
    NecroPagedArena arena;
    NecroCoreAst*   root;
} NecroCoreAstArena;

NecroCoreAstArena necro_core_ast_arena_empty();
NecroCoreAstArena necro_core_ast_arena_create();
void              necro_core_ast_arena_destroy(NecroCoreAstArena* ast_arena);

//--------------------
// Core Ast
//--------------------
NecroCoreAst* necro_core_ast_alloc(NecroPagedArena* arena, NECRO_CORE_AST_TYPE ast_type);
NecroCoreAst* necro_core_ast_create_lit(NecroPagedArena* arena, NecroAstConstant constant);
NecroCoreAst* necro_core_ast_create_var(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroType* necro_type);
NecroCoreAst* necro_core_ast_create_bind(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroCoreAst* expr);
NecroCoreAst* necro_core_ast_create_let(NecroPagedArena* arena, NecroCoreAst* bind, NecroCoreAst* expr);
NecroCoreAst* necro_core_ast_create_lam(NecroPagedArena* arena, NecroCoreAst* arg, NecroCoreAst* expr);
// NecroCoreAst* necro_core_ast_create_app(NecroPagedArena* arena, NecroCoreAst* expr1, NecroCoreAst* expr2, NecroType* necro_type);
NecroCoreAst* necro_core_ast_create_app(NecroPagedArena* arena, NecroCoreAst* expr1, NecroCoreAst* expr2);
NecroCoreAst* necro_core_ast_create_data_con(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroType* type, NecroType* data_type_type);
NecroCoreAst* necro_core_ast_create_data_decl(NecroPagedArena* arena, NecroCoreAstSymbol* ast_symbol, NecroCoreAstList* con_list);
NecroCoreAst* necro_core_ast_create_case(NecroPagedArena* arena, NecroCoreAst* expr, NecroCoreAstList* alts);
NecroCoreAst* necro_core_ast_create_case_alt(NecroPagedArena* arena, NecroCoreAst* pat, NecroCoreAst* expr);
NecroCoreAst* necro_core_ast_create_for_loop(NecroPagedArena* arena, size_t max_loops, NecroCoreAst* range_init, NecroCoreAst* value_init, NecroCoreAst* index_arg, NecroCoreAst* value_arg, NecroCoreAst* expression);
void          necro_core_ast_swap(NecroCoreAst* ast1, NecroCoreAst* ast2);
void          necro_core_ast_pretty_print(NecroCoreAst* ast);
// TODO: Finish deep copy
NecroCoreAst* necro_core_ast_deep_copy(NecroPagedArena* arena, NecroCoreAst* ast);
size_t        necro_core_ast_num_args(NecroCoreAst* bind_ast);

//--------------------
// Transformation
//--------------------
NecroResult(void) necro_ast_transform_to_core(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroAstArena* ast_arena, NecroCoreAstArena* core_ast_arena);
void              necro_core_ast_test();

#endif // CORE_AST_H

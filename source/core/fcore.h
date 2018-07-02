/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef CORE_FINAL_PASS_H
#define CORE_FINAL_PASS_H 1

// #include <stdlib.h>
// #include <stdint.h>
// #include <assert.h>
// #include <stdbool.h>

// #include "core.h"
// #include "type.h"
// #include "utility/arena.h"

// ///////////////////////////////////////////////////////
// // FCore
// //-----------
// // * Closure conversion
// // * Constant hoisting / folding
// ///////////////////////////////////////////////////////

// ///////////////////////////////////////////////////////
// // Forward Declarations
// ///////////////////////////////////////////////////////
// struct NecroFCoreAST;

// ///////////////////////////////////////////////////////
// // Types
// ///////////////////////////////////////////////////////

// typedef enum
// {
//     NECRO_STATE_STATIC,
//     NECRO_STATE_CONSTANT,
//     NECRO_STATE_STATELESS,
//     NECRO_STATE_STATEFUL,
// } NECRO_STATE_TYPE;

// typedef struct
// {
//     NecroPagedArena       arena;
//     struct NecroFCoreAST* ast;
// } NecroFCore;

// typedef struct NecroFCoreBind
// {
//     NecroVar              var;
//     struct NecroFCoreAST* expr;
// } NecroFCoreBind;

// typedef struct NecroFCoreBindRec
// {
//     NecroFCoreBind* binds;
//     size_t          num_binds;
// } NecroFCoreBindRec;

// typedef enum
// {
//     NECRO_FCORE_LET_BIND,
//     NECRO_FCORE_LET_BIND_REC,
// } NECRO_FCORE_LET_TYPE;

// typedef struct NecroFCoreLet
// {
//     union
//     {
//         NecroFCoreBind*    bind;
//         NecroFCoreBindRec* bind_rec;
//     };
//     NECRO_FCORE_LET_TYPE  bind_type;
//     struct NecroFCoreAST* body;
// } NecroFCoreLet;

// typedef struct
// {
//     union
//     {
//         double      double_literal;
//         int64_t     int_literal;
//         char        char_literal;
//         NecroSymbol string_literal;
//     };
//     NecroAST_ConstantType type;
// } NecroFCoreLit;

// typedef struct NecroFCoreDataCon
// {
//     NecroVar                  condid;
//     struct NecroFCoreAST*     args;
//     size_t                    num_args;
//     struct NecroFCoreDataCon* next;
// } NecroFCoreDataCon;

// typedef struct NecroFCoreDataDecl
// {
//     NecroVar           data_id;
//     NecroFCoreDataCon* cons;
//     size_t             num_cons;
// } NecroFCoreDataDecl;

// typedef struct NecroFCoreApp
// {
//     struct NecroFCoreAST* fun;
//     struct NecroFCoreAST* args;
//     size_t                num_args;
// } NecroFCoreApp;

// typedef struct NecroFCoreCaseAlt
// {
//     struct NecroFCoreAST* altCon; // NULL altCon is a wild card
//     struct NecroFCoreAST* expr;
// } NecroFCoreCaseAlt;

// typedef struct NecroFCoreCase
// {
//     struct NecroFCoreAST* expr;
//     NecroFCoreCaseAlt*    alts;
//     size_t                num_alts;
// } NecroFCoreCase;

// typedef struct NecroFCoreClosure
// {
//     NecroVar               fun;
//     struct NecroFCoreBind* env_binds;
//     size_t                 num_env_binds;
// } NecroFCoreClosure;

// typedef struct NecroFCoreProject
// {
//     struct NecroFCoreAST* closure;
//     NecroVar              var;
// } NecroFCoreProject;

// typedef struct NecroFCoreFunction
// {
//     NecroVar              name;
//     struct NecroFCoreAST* parameters;
//     size_t                num_parameters;
//     struct NecroFCoreAST* body;
//     NecroVar*             free_vars;
//     size_t                num_free_vars;
//     NecroVar*             free_global_vars;
//     size_t                num_free_global_vars;
// } NecroFCoreFunction;

// typedef enum
// {
//     NECRO_FCORE_LIT,
//     NECRO_FCORE_VAR,
//     NECRO_FCORE_LET,
//     NECRO_FCORE_APP,
//     NECRO_FCORE_CASE,
//     NECRO_FCORE_DATA_DECL,
//     NECRO_FCORE_DATA_CON,
//     NECRO_FCORE_FUN,
//     NECRO_FCORE_CLOSURE,
//     NECRO_FCORE_PROJECT,
// } NECRO_FCORE_AST_TYPE;

// typedef struct NecroFCoreAST
// {
//     union
//     {
//         NecroVar           var;
//         NecroFCoreLet      let;
//         NecroFCoreLit      lit;
//         NecroFCoreDataDecl data_decl;
//         NecroFCoreDataCon  data_con;
//         NecroFCoreApp      app;
//         NecroFCoreCase     case_expr;
//         NecroFCoreClosure  closure;
//         NecroFCoreProject  proj;
//         NecroFCoreFunction fun;
//     };
//     NECRO_FCORE_AST_TYPE type;
//     //--------------------
//     // Metadata
//     //--------------------
//     NecroType*       necro_type;
//     int32_t          persistent_slot;
//     NECRO_STATE_TYPE state_type;
//     bool             is_recursive;
// } NecroFCoreAST;

// NecroFCore necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroPrimTypes* prim_types);

#endif // CORE_FINAL_PASS
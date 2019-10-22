/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#ifndef NECRO_INFER_H
#define NECRO_INFER_H 1

#include <stdlib.h>
#include <stdbool.h>
#include "utility.h"
#include "type.h"
#include "type_class.h"
#include "ast.h"
#include "d_analyzer.h"
#include "result.h"
#include "dequeue.h"

//--------------------
// Constraints
//--------------------
struct NecroConstraint;
typedef enum
{
    NECRO_CONSTRAINT_AND,
    NECRO_CONSTRAINT_EQL,
    NECRO_CONSTRAINT_FOR,
    NECRO_CONSTRAINT_UNI,
} NECRO_CONSTRAINT_TYPE;

typedef struct
{
    struct NecroConstraint* con1;
    struct NecroConstraint* con2;
} NecroConstraintAnd;

typedef struct
{
    NecroType*     u1;
    NecroType*     u2;
    NecroSourceLoc source_loc;
} NecroConstraintUniquenessCoercion;

typedef struct
{
    NecroType* type1;
    NecroType* type2;
} NecroConstraintEqual;

typedef struct
{
    NecroType*              var;
    struct NecroConstraint* con;
} NecroConstraintForAll;

typedef struct NecroConstraint
{
    union
    {
        NecroConstraintAnd                and;
        NecroConstraintEqual              eql;
        NecroConstraintUniquenessCoercion uni;
        NecroConstraintForAll             for_all;
    };
    NECRO_CONSTRAINT_TYPE type;
} NecroConstraint;

NECRO_DECLARE_ARENA_LIST(struct NecroConstraint*, Constraint, constraint);
NECRO_DECLARE_DEQUEUE(struct NecroConstraint*, Constraint, constraint);
typedef struct NecroConstraintEnv
{
    NecroConstraintDequeue constraints;
    NecroSourceLoc         curr_loc;
} NecroConstraintEnv;
NecroConstraintList* necro_constraint_append_uniqueness_coercion_without_queue_push(NecroPagedArena* arena, NecroConstraintEnv* env, NecroType* u1, NecroType* u2, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_uniqueness_coercion_and_queue_push_back(NecroPagedArena* arena, NecroConstraintEnv* env, NecroType* u1, NecroType* u2, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_uniqueness_coercion_and_queue_push_front(NecroPagedArena* arena, NecroConstraintEnv* env, NecroType* u1, NecroType* u2, NecroConstraintList* next);
NecroResult(void)    necro_constraint_simplify(NecroPagedArena* arena, NecroConstraintEnv* env, struct NecroBase* base);
NecroConstraintEnv   necro_constraint_env_empty();
NecroConstraintEnv   necro_constraint_env_create();
void                 necro_constraint_env_destroy(NecroConstraintEnv* env);

//--------------------
// Infer
//--------------------
typedef struct NecroInfer
{
    struct NecroScopedSymTable* scoped_symtable;
    struct NecroBase*           base;
    NecroIntern*                intern;
    NecroPagedArena*            arena;
    NecroSnapshotArena          snapshot_arena;
    NecroAstArena*              ast_arena;
    NecroConstraintEnv          con_env;
} NecroInfer;

NecroResult(void)      necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);
NecroResult(NecroType) necro_infer_go(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_type_sig(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast, NECRO_TYPE_ORDER variable_type_order, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type);
void                   necro_test_infer();

#endif // NECRO_INFER_H
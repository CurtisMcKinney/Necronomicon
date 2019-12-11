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
#include "alias_analysis.h"

//--------------------
// Constraints
//--------------------
struct NecroConstraint;
typedef enum
{
    // NECRO_CONSTRAINT_AND,
    NECRO_CONSTRAINT_EQL,
    NECRO_CONSTRAINT_UNI,
    NECRO_CONSTRAINT_CLASS,
} NECRO_CONSTRAINT_TYPE;

// typedef struct
// {
//     struct NecroConstraint* con1;
//     struct NecroConstraint* con2;
// } NecroConstraintAnd;

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
    NecroTypeClass* type_class;
    NecroType*      type1;
} NecroConstraintClass;

typedef struct NecroConstraint
{
    union
    {
        // NecroConstraintAnd                and;
        NecroConstraintEqual              eql;
        NecroConstraintUniquenessCoercion uni;
        NecroConstraintClass              cls;
    };
    NECRO_CONSTRAINT_TYPE type;
    NecroSourceLoc        source_loc;
    NecroSourceLoc        end_loc;
} NecroConstraint;

NECRO_DECLARE_ARENA_LIST(struct NecroConstraint*, Constraint, constraint);
NECRO_DECLARE_DEQUEUE(struct NecroConstraint*, Constraint, constraint);
typedef struct NecroConstraintEnv
{
    NecroConstraintDequeue constraints;
    NecroSourceLoc         curr_loc;
} NecroConstraintEnv;
bool                 necro_constraint_is_equivalant(NecroConstraint* con1, NecroConstraint* con2);
bool                 necro_constraint_is_equal(NecroConstraint* con1, NecroConstraint* con2);
NecroConstraintList* necro_constraint_append_constraint_with_new_subject_and_queue_push_back(NecroPagedArena* arena, NecroConstraintEnv* env, NecroConstraint* con, NecroType* subject, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_constraint_with_new_subject(NecroPagedArena* arena, NecroConstraint* con, NecroType* subject, NecroConstraintList* next);
NecroConstraint*     necro_constraint_with_new_object(NecroPagedArena* arena, NecroConstraint* con, NecroType* object);
NecroConstraintList* necro_constraint_append_uniqueness_coercion_without_queue_push(NecroPagedArena* arena, NecroType* u1, NecroType* u2, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_uniqueness_coercion_and_queue_push_back(NecroPagedArena* arena, NecroConstraintEnv* env, NecroType* u1, NecroType* u2, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_uniqueness_coercion_and_queue_push_front(NecroPagedArena* arena, NecroConstraintEnv* env, NecroType* u1, NecroType* u2, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_class_constraint_and_queue_push_back(NecroPagedArena* arena, NecroConstraintEnv* env, NecroTypeClass* type_class, NecroType* type1, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next);
NecroConstraintList* necro_constraint_append_class_constraint(NecroPagedArena* arena, NecroTypeClass* type_class, NecroType* type1, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next);
NecroResult(void)    necro_constraint_simplify(NecroPagedArena* arena, NecroConstraintEnv* env, struct NecroBase* base, struct NecroIntern* intern);
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
    struct NecroFScope*         f_scope;
} NecroInfer;

NecroResult(void)      necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena);
NecroResult(NecroType) necro_infer_go(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_type_sig(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type);
void                   necro_test_infer();

#endif // NECRO_INFER_H
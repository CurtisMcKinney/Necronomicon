/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include "symtable.h"
#include "type_class.h"
#include "kind.h"
#include "infer.h"
#include "base.h"
#include "alias_analysis.h"
#include "utility.h"

///////////////////////////////////////////////////////
// NecroConstraint
// ------------------
// Somewhat based on "OutsideIn(X)" paper.
///////////////////////////////////////////////////////
NecroConstraintEnv necro_constraint_env_empty()
{
    return (NecroConstraintEnv)
    {
        .constraints = necro_constraint_dequeue_empty(),
        .source_loc  = NULL_LOC,
        .end_loc     = NULL_LOC,
    };
}

NecroConstraintEnv necro_constraint_env_create()
{
    return (NecroConstraintEnv)
    {
        .constraints = necro_constraint_dequeue_create(128),
        .source_loc  = NULL_LOC,
        .end_loc     = NULL_LOC,
    };
}

void necro_constraint_env_destroy(NecroConstraintEnv* env)
{
    assert(env != NULL);
    necro_constraint_dequeue_destroy(&env->constraints);
    *env = necro_constraint_env_empty();
}

void necro_constraint_set_loc(NecroConstraintEnv* env, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    env->source_loc = source_loc;
    env->end_loc    = end_loc;
}

// 'is_equivalant' meaning that they constrain the same way such that the "type1" (or object of the constraint) may differ but the "type2" (the subject of the constraint) is the same.
bool necro_constraint_is_equivalant(NecroConstraint* con1, NecroConstraint* con2)
{
    if (con1->type != con2->type)
        return false;
    switch (con1->type)
    {
    case NECRO_CONSTRAINT_EQUAL:       return necro_type_exact_unify(con1->equal.type2, con2->equal.type2);
    case NECRO_CONSTRAINT_UCONSTRAINT: return necro_type_exact_unify(con1->uconstraint.u2, con2->uconstraint.u2);
    case NECRO_CONSTRAINT_UCOERCE:     return necro_type_exact_unify(con1->ucoerce.type2, con2->ucoerce.type2);
    case NECRO_CONSTRAINT_CLASS:       return con1->cls.type_class->type_class_name == con2->cls.type_class->type_class_name;
    }
    assert(false);
    return false;
}

// Tests for exact equality
bool necro_constraint_is_equal(NecroConstraint* con1, NecroConstraint* con2)
{
    if (con1->type != con2->type)
        return false;
    switch (con1->type)
    {
    case NECRO_CONSTRAINT_EQUAL:       return necro_type_exact_unify(con1->equal.type1, con2->equal.type1) &&  necro_type_exact_unify(con1->equal.type2, con2->equal.type2);
    case NECRO_CONSTRAINT_UCONSTRAINT: return necro_type_exact_unify(con1->uconstraint.u1, con2->uconstraint.u1) && necro_type_exact_unify(con1->uconstraint.u2, con2->uconstraint.u2);
    case NECRO_CONSTRAINT_UCOERCE:     return necro_type_exact_unify(con1->ucoerce.type1, con2->ucoerce.type1) && necro_type_exact_unify(con1->ucoerce.type2, con2->ucoerce.type2);
    case NECRO_CONSTRAINT_CLASS:       return con1->cls.type_class->type_class_name == con2->cls.type_class->type_class_name && necro_type_exact_unify(con1->cls.type1, con2->cls.type1);
    }
    assert(false);
    return false;
}

bool necro_constraint_list_contains_constraints(NecroConstraintList* constraints, NecroConstraint* constraint)
{
    while (constraints != NULL)
    {
        if (necro_constraint_is_equal(constraints->data, constraint))
            return true;
        constraints = constraints->next;
    }
    return false;
}

// Sometimes we want to change rigid variables, sometimes we don't...
// Note: u1 <= u2
NecroResult(void) necro_constraint_simplify_uniqueness_constraint(NecroPagedArena* arena, NecroBase* base, NecroIntern* intern, NecroConstraint* con)
{
    assert(con->type == NECRO_CONSTRAINT_UCONSTRAINT);
    NecroType* u1 = necro_type_find(con->uconstraint.u1);
    NecroType* u2 = necro_type_find(con->uconstraint.u2);
    assert(u1 != NULL);
    assert(u2 != NULL);
    assert(necro_kind_is_ownership(base, u1));
    assert(necro_kind_is_ownership(base, u2));
    if (u1 == u2)
        return ok_void();
    if (necro_type_is_ownership_share(base, u1))
    {
        if (u2->type == NECRO_TYPE_VAR)
        {
            return necro_type_ownership_bind_uvar(u2, u1, con->source_loc, con->end_loc);
        }
        else if (necro_type_is_ownership_share(base, u2))
        {
            return ok_void();
        }
        else if (necro_type_is_ownership_owned(base, u2))
        {
            // TODO: Custom u1 <= u2 error (with 2 source locations?)
            return necro_error_map(NecroType, void, necro_type_mismatched_type_error(u1, u2, u1, u2, con->source_loc, con->end_loc));
        }
        necro_unreachable(void);
    }
    else if (u1->type == NECRO_TYPE_VAR)
    {
        if (u2->type == NECRO_TYPE_VAR)
        {
            if (intern != NULL)
            {
                necro_type_name_if_anon_type(base, intern, u1);
                necro_type_name_if_anon_type(base, intern, u2);
            }
            if (!necro_constraint_list_contains_constraints(u1->var.var_symbol->constraints, con))
                u1->var.var_symbol->constraints = necro_cons_constraint_list(arena, con, u1->var.var_symbol->constraints);
            return ok_void();
        }
        else if (necro_type_is_ownership_share(base, u2))
        {
            return ok_void();
        }
        else if (necro_type_is_ownership_owned(base, u2))
        {
            return necro_type_ownership_bind_uvar(u1, u2, con->source_loc, con->end_loc);
        }
        necro_unreachable(void);
    }
    else if (necro_type_is_ownership_owned(base, u1))
    {
        if (u2->type == NECRO_TYPE_VAR)
            return ok_void();
        else if (necro_type_is_ownership_share(base, u2))
            return ok_void();
        else if (necro_type_is_ownership_owned(base, u2))
            return ok_void();
        necro_unreachable(void);
    }
    necro_unreachable(void);
}

NecroResult(void) necro_constraint_simplify_uniqueness_coerce(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroIntern* intern, NecroConstraint* con)
{
    UNUSED(intern);
    assert(con->type == NECRO_CONSTRAINT_UCOERCE);
    NecroType* t1 = necro_type_find(con->ucoerce.type1);
    NecroType* u1 = necro_type_find(t1->ownership);
    NecroType* t2 = necro_type_find(con->ucoerce.type2);
    NecroType* u2 = necro_type_find(t2->ownership);
    assert(u1 != NULL);
    assert(u2 != NULL);
    assert(necro_kind_is_ownership(base, u1));
    assert(necro_kind_is_ownership(base, u2));
    if (u1 == u2)
        return ok_void();
    if (necro_type_is_ownership_share(base, u1))
    {
        if (u2->type == NECRO_TYPE_VAR)
        {
            t1->ownership = necro_type_ownership_fresh_var(arena, base, NULL);
            return necro_constraint_simplify_uniqueness_coerce(arena, con_env, base, intern, con);
        }
        else if (necro_type_is_ownership_share(base, u2))
        {
            return ok_void();
        }
        else if (necro_type_is_ownership_owned(base, u2))
        {
            t1->ownership = u2;
            return ok_void();
        }
        necro_unreachable(void);
    }
    else if (u1->type == NECRO_TYPE_VAR)
    {
        if (u2->type == NECRO_TYPE_VAR)
        {
            // Add constraint
            con->type           = NECRO_CONSTRAINT_UCONSTRAINT;
            con->uconstraint.u1 = u1;
            con->uconstraint.u2 = u2;
            return necro_constraint_simplify_uniqueness_constraint(arena, base, intern, con);
            // necro_constraint_dequeue_push_back(&con_env->constraints, con);
            // return ok_void();
        }
        else if (necro_type_is_ownership_share(base, u2))
        {
            return ok_void();
        }
        else if (necro_type_is_ownership_owned(base, u2))
        {
            // return necro_type_ownership_bind_uvar(u1, u2, con->source_loc, con->end_loc);
            u1->var.bound = u2;
            return ok_void();
        }
        necro_unreachable(void);
    }
    else if (necro_type_is_ownership_owned(base, u1))
    {
        if (u2->type == NECRO_TYPE_VAR)
            return ok_void();
        else if (necro_type_is_ownership_share(base, u2))
            return ok_void();
        else if (necro_type_is_ownership_owned(base, u2))
            return ok_void();
        necro_unreachable(void);
    }
    necro_unreachable(void);
}

// NecroResult(void) necro_constraint_simplify_uniqueness_coerce_2(NecroPagedArena* arena, NecroConstraintEnv* con_env, NecroBase* base, NecroIntern* intern, NecroConstraint* con)
// {
//     UNUSED(intern);
//     assert(con->type == NECRO_CONSTRAINT_UCOERCE);
//     NecroType* t1 = necro_type_find(con->ucoerce.type1);
//     NecroType* u1 = necro_type_find(t1->ownership);
//     NecroType* t2 = necro_type_find(con->ucoerce.type2);
//     NecroType* u2 = necro_type_find(t2->ownership);
//     assert(u1 != NULL);
//     assert(u2 != NULL);
//     assert(necro_kind_is_ownership(base, u1));
//     assert(necro_kind_is_ownership(base, u2));
//     if (u1 == u2)
//         return ok_void();
//     if (necro_type_is_ownership_share(base, u1))
//     {
//         if (u2->type == NECRO_TYPE_VAR)
//         {
//             // t1->ownership = necro_type_ownership_fresh_var(arena, base, NULL);
//             // return necro_constraint_simplify_uniqueness_coerce(arena, con_env, base, intern, con);
//             // Mutate into Type var and then add constraint
//             u1->type                 = NECRO_TYPE_VAR;
//             u1->var.bound            = NULL;
//             u1->var.is_rigid         = false;
//             u1->var.scope            = NULL;
//             u1->var.var_symbol       = necro_ast_symbol_create(arena, necro_intern_unique_string(intern, "q"), NULL, NULL, NULL);
//             u1->var.var_symbol->type = u1;
//             con->type                = NECRO_CONSTRAINT_UCONSTRAINT;
//             con->uconstraint.u1      = u1;
//             con->uconstraint.u2      = u2;
//             necro_constraint_dequeue_push_back(&con_env->constraints, con);
//             return ok_void();
//         }
//         else if (necro_type_is_ownership_share(base, u2))
//         {
//             return ok_void();
//         }
//         else if (necro_type_is_ownership_owned(base, u2))
//         {
//             // t1->ownership = u2;
//             u1->type                 = NECRO_TYPE_VAR;
//             u1->var.bound            = u2;
//             u1->var.is_rigid         = false;
//             u1->var.scope            = NULL;
//             u1->var.var_symbol       = necro_ast_symbol_create(arena, necro_intern_unique_string(intern, "q"), NULL, NULL, NULL);
//             u1->var.var_symbol->type = u1;
//             return ok_void();
//         }
//         necro_unreachable(void);
//     }
//     else if (u1->type == NECRO_TYPE_VAR)
//     {
//         if (u2->type == NECRO_TYPE_VAR)
//         {
//             // return necro_constraint_simplify_uniqueness_constraint(arena, base, intern, con);
//             // Add constraint
//             con->type           = NECRO_CONSTRAINT_UCONSTRAINT;
//             con->uconstraint.u1 = u1;
//             con->uconstraint.u2 = u2;
//             necro_constraint_dequeue_push_back(&con_env->constraints, con);
//             return ok_void();
//         }
//         else if (necro_type_is_ownership_share(base, u2))
//         {
//             return ok_void();
//         }
//         else if (necro_type_is_ownership_owned(base, u2))
//         {
//             // return necro_type_ownership_bind_uvar(u1, u2, con->source_loc, con->end_loc);
//             u1->var.bound = u2;
//             return ok_void();
//         }
//         necro_unreachable(void);
//     }
//     else if (necro_type_is_ownership_owned(base, u1))
//     {
//         if (u2->type == NECRO_TYPE_VAR)
//             return ok_void();
//         else if (necro_type_is_ownership_share(base, u2))
//             return ok_void();
//         else if (necro_type_is_ownership_owned(base, u2))
//             return ok_void();
//         necro_unreachable(void);
//     }
//     necro_unreachable(void);
// }

NecroResult(void) necro_constraint_simplify(NecroPagedArena* arena, NecroConstraintEnv* env, NecroBase* base, NecroIntern* intern)
{
    NecroConstraint* wanted = NULL;
    while (necro_constraint_dequeue_pop_front(&env->constraints, &wanted))
    {
        switch (wanted->type)
        {
        case NECRO_CONSTRAINT_UCONSTRAINT: necro_try(void, necro_constraint_simplify_uniqueness_constraint(arena, base, intern, wanted)); break;
        case NECRO_CONSTRAINT_UCOERCE:     necro_try(void, necro_constraint_simplify_uniqueness_coerce(arena, env, base, intern, wanted)); break;
        case NECRO_CONSTRAINT_CLASS:       necro_try(void, necro_constraint_simplify_class_constraint(arena, env, base, wanted)); break;
        default:
            necro_unreachable(void);
        }
    }
    return ok_void();
}

NecroConstraintList* necro_constraint_append_uniqueness_constraint_and_queue_push_back(NecroPagedArena* arena, NecroConstraintEnv* env, NecroType* u1, NecroType* u2, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next)
{
    NecroConstraint* constraint = necro_paged_arena_alloc(arena, sizeof(NecroConstraint));
    constraint->type            = NECRO_CONSTRAINT_UCONSTRAINT;
    constraint->uconstraint.u1  = u1;
    constraint->uconstraint.u2  = u2;
    constraint->source_loc      = source_loc;
    constraint->end_loc         = end_loc;
    necro_constraint_dequeue_push_back(&env->constraints, constraint);
    return necro_cons_constraint_list(arena, constraint, next);
}

void necro_constraint_push_back_uniqueness_constraint(NecroPagedArena* arena, NecroConstraintEnv* env, NecroBase* base, NecroIntern* intern, NecroType* u1, NecroType* u2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    assert(u1 != NULL);
    assert(u2 != NULL);
    if (intern != NULL)
    {
        necro_type_name_if_anon_type(base, intern, u1);
        necro_type_name_if_anon_type(base, intern, u2);
    }
    NecroConstraint* constraint = necro_paged_arena_alloc(arena, sizeof(NecroConstraint));
    constraint->type            = NECRO_CONSTRAINT_UCONSTRAINT;
    constraint->uconstraint.u1  = u1;
    constraint->uconstraint.u2  = u2;
    constraint->source_loc      = source_loc;
    constraint->end_loc         = end_loc;
    necro_constraint_dequeue_push_back(&env->constraints, constraint);
}

void necro_constraint_push_back_uniqueness_coercion(NecroPagedArena* arena, NecroConstraintEnv* env, NecroBase* base, NecroIntern* intern, NecroType* type1, NecroType* type2, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    assert(type1 != NULL);
    assert(type2 != NULL);
    assert(type1->ownership != NULL);
    if (intern != NULL)
    {
        necro_type_name_if_anon_type(base, intern, type1);
        necro_type_name_if_anon_type(base, intern, type1->ownership);
        necro_type_name_if_anon_type(base, intern, type2);
        necro_type_name_if_anon_type(base, intern, type2->ownership);
    }
    NecroConstraint* constraint = necro_paged_arena_alloc(arena, sizeof(NecroConstraint));
    constraint->type            = NECRO_CONSTRAINT_UCOERCE;
    constraint->ucoerce.type1   = type1;
    constraint->ucoerce.type2   = type2;
    constraint->source_loc      = source_loc;
    constraint->end_loc         = end_loc;
    necro_constraint_dequeue_push_back(&env->constraints, constraint);
}

NecroConstraintList* necro_constraint_append_class_constraint_and_queue_push_back(NecroPagedArena* arena, NecroConstraintEnv* env, NecroTypeClass* type_class, NecroType* type1, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next)
{
    NecroConstraint* constraint = necro_paged_arena_alloc(arena, sizeof(NecroConstraint));
    constraint->type            = NECRO_CONSTRAINT_CLASS;
    constraint->cls.type_class  = type_class;
    constraint->cls.type1       = type1;
    constraint->source_loc      = source_loc;
    constraint->end_loc         = end_loc;
    necro_constraint_dequeue_push_back(&env->constraints, constraint);
    return necro_cons_constraint_list(arena, constraint, next);
}

NecroConstraintList* necro_constraint_append_class_constraint(NecroPagedArena* arena, NecroTypeClass* type_class, NecroType* type1, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroConstraintList* next)
{
    NecroConstraint* constraint = necro_paged_arena_alloc(arena, sizeof(NecroConstraint));
    constraint->type            = NECRO_CONSTRAINT_CLASS;
    constraint->cls.type_class  = type_class;
    constraint->cls.type1       = type1;
    constraint->source_loc      = source_loc;
    constraint->end_loc         = end_loc;
    return necro_cons_constraint_list(arena, constraint, next);
}


///////////////////////////////////////////////////////
// Infer
///////////////////////////////////////////////////////
NecroInfer necro_infer_empty()
{
    return (NecroInfer)
    {
        .scoped_symtable = NULL,
            .snapshot_arena = necro_snapshot_arena_empty(),
            .arena = NULL,
            .intern = NULL,
            .base = NULL,
            .con_env = necro_constraint_env_empty(),
            .f_scope = NULL,
    };
}

NecroInfer necro_infer_create(NecroPagedArena* arena, NecroIntern* intern, struct NecroScopedSymTable* scoped_symtable, struct NecroBase* base, NecroAstArena* ast_arena)
{
    NecroInfer infer = (NecroInfer)
    {
        .intern          = intern,
        .arena           = arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
        .con_env         = necro_constraint_env_create(),
        .f_scope         = NULL,
    };
    return infer;
}

void necro_infer_destroy(NecroInfer* infer)
{
    if (infer == NULL)
        return;
    necro_snapshot_arena_destroy(&infer->snapshot_arena);
    necro_constraint_env_destroy(&infer->con_env);
    *infer = necro_infer_empty();
}

///////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_infer_apat(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_pattern(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_var(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_ast_symbol_uniqueness(NecroInfer* infer, NecroAstSymbol* ast_symbol, NecroType* prev_uniqueness, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc);
NecroResult(NecroType) necro_infer_tuple_type(NecroInfer* infer, NecroAst* ast, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type);
NecroResult(NecroType) necro_infer_simple_assignment(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_pat_assignment(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_apats_assignment(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_gen_pat_go(NecroInfer* infer, NecroAst* ast);
NecroType*             necro_infer_constant(NecroInfer* infer, NecroAst* ast);
void                   necro_pat_new_name_go(NecroInfer* infer, NecroAst* ast);

//=====================================================
// TypeSig
//=====================================================
NecroResult(NecroType) necro_ast_get_type_sig_ownership(NecroInfer* infer, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type, NECRO_TYPE necro_type, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    switch (attribute_type)
    {
    case NECRO_TYPE_ATTRIBUTE_VOID:        return ok(NecroType, infer->base->ownership_share->type);
    case NECRO_TYPE_ATTRIBUTE_NONE:        return ok(NecroType, infer->base->ownership_share->type);
    case NECRO_TYPE_ATTRIBUTE_STAR:        return ok(NecroType, infer->base->ownership_steal->type);
    case NECRO_TYPE_ATTRIBUTE_DOT:         return ok(NecroType, necro_type_ownership_fresh_var(infer->arena, infer->base, NULL));
    case NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR:
        if (necro_type == NECRO_TYPE_VAR)
            return ok(NecroType, necro_type_ownership_fresh_var(infer->arena, infer->base, NULL));
        else
            return ok(NecroType, NULL);
    case NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR_DOT:
        if (necro_type == NECRO_TYPE_APP || necro_type == NECRO_TYPE_CON)
            return ok(NecroType, necro_type_ownership_fresh_var(infer->arena, infer->base, NULL));
        else
            return necro_type_malformed_uniqueness_attribute_error(NULL, NULL, source_loc, end_loc);
    default:
        assert(false);
        return ok(NecroType, NULL);
    }
}

NECRO_TYPE_ATTRIBUTE_TYPE necro_type_attribute_get_arg_attribute(NECRO_TYPE_ATTRIBUTE_TYPE attribute_type)
{
    switch (attribute_type)
    {
    case NECRO_TYPE_ATTRIBUTE_VOID:            return NECRO_TYPE_ATTRIBUTE_VOID;
    case NECRO_TYPE_ATTRIBUTE_NONE:            return NECRO_TYPE_ATTRIBUTE_NONE;
    case NECRO_TYPE_ATTRIBUTE_STAR:            return NECRO_TYPE_ATTRIBUTE_NONE;
    case NECRO_TYPE_ATTRIBUTE_DOT:             return NECRO_TYPE_ATTRIBUTE_NONE;
    case NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR:     return NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR;
    case NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR_DOT: return NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR;
    default:
        assert(false);
        return NECRO_TYPE_ATTRIBUTE_NONE;
    }
}

NecroResult(NecroType) necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_TUPLE:    return necro_infer_tuple_type(infer, ast, attribute_type);
    case NECRO_AST_CONSTANT:
    {
        NecroType* necro_type = necro_infer_constant(infer, ast);
        necro_type->ownership = infer->base->ownership_share->type;
        assert(ast->necro_type != NULL);
        return ok(NecroType, necro_type);
    }

    case NECRO_AST_VARIABLE:
        if (ast->variable.ast_symbol->type == NULL)
        {
            // New Var
            NecroType* type_var            = necro_type_var_create(infer->arena, ast->variable.ast_symbol, NULL);
            ast->variable.ast_symbol->type = type_var;
            ast->necro_type                = type_var;
            type_var->ownership            = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, type_var->type, ast->source_loc, ast->end_loc));
            assert(ast->necro_type != NULL);
            return ok(NecroType, type_var);
        }
        else
        {
            // Existing Var
            assert(ast->variable.ast_symbol->type->ownership != NULL);
            ast->necro_type = ast->variable.ast_symbol->type;
            if (attribute_type != NECRO_TYPE_ATTRIBUTE_VOID)
            {
                NecroType* utype                             = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
                ast->variable.ast_symbol->sig_type_var_ulist = necro_type_list_create(infer->arena, utype, ast->variable.ast_symbol->sig_type_var_ulist);
            }
            assert(ast->necro_type != NULL);
            return ok(NecroType, ast->necro_type);
        }

    case NECRO_AST_FUNCTION_TYPE:
    {
        NECRO_TYPE_ATTRIBUTE_TYPE arg_attribute_type = necro_type_attribute_get_arg_attribute(attribute_type);
        NecroType* left                              = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, ast->function_type.type, arg_attribute_type));
        NecroType* right                             = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, ast->function_type.next_on_arrow, arg_attribute_type));
        ast->necro_type                              = necro_type_fn_create(infer->arena, left, right);
        ast->necro_type->ownership                   = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_CONID:
        ast->necro_type            = necro_type_con_create(infer->arena, ast->conid.ast_symbol->type->con.con_symbol, NULL);
        ast->necro_type->ownership = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
        // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->necro_type, ast->source_loc, ast->end_loc));
        assert(ast->necro_type != NULL);
        return ok(NecroType, ast->necro_type);

    case NECRO_AST_CONSTRUCTOR:
    {
        NecroType*                con_args           = NULL;
        NecroAst*                 arg_list           = ast->constructor.arg_list;
        size_t                    arity              = 0;
        NECRO_TYPE_ATTRIBUTE_TYPE arg_attribute_type = necro_type_attribute_get_arg_attribute(attribute_type);
        while (arg_list != NULL)
        {
            NecroType* arg_type = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, arg_list->list.item, arg_attribute_type));
            // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, arg_type, arg_list->list.item->source_loc, arg_list->list.item->end_loc));
            if (con_args == NULL)
                con_args = necro_type_list_create(infer->arena, arg_type, NULL);
            else
                con_args->list.next = necro_type_list_create(infer->arena, arg_type, NULL);
            arg_list = arg_list->list.next_item;
            arity++;
        }
        NecroType* env_con_type = ast->constructor.conid->conid.ast_symbol->type;
        ast->necro_type         = necro_type_con_create(infer->arena, env_con_type->con.con_symbol, con_args);
        // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->necro_type, ast->source_loc, ast->end_loc));
        ast->necro_type->ownership = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
        assert(ast->necro_type != NULL);
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_TYPE_APP:
    {
        NecroType* left   = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, ast->type_app.ty, attribute_type));

        // Handle Share type
        NecroAst*  left_fn = ast->type_app.ty;
        while (left_fn->type == NECRO_AST_TYPE_APP)
            left_fn = left_fn->type_app.ty;
        // NECRO_TYPE_ATTRIBUTE_TYPE arg_attribute_type = (left_fn->type == NECRO_AST_CONID && left_fn->conid.ast_symbol == infer->base->share_type) ? NECRO_TYPE_ATTRIBUTE_NONE : attribute_type;
        // NECRO_TYPE_ATTRIBUTE_TYPE arg_attribute_type = (attribute_type == NECRO_TYPE_ATTRIBUTE_VOID) ? NECRO_TYPE_ATTRIBUTE_VOID : NECRO_TYPE_ATTRIBUTE_NONE;
        NECRO_TYPE_ATTRIBUTE_TYPE arg_attribute_type = necro_type_attribute_get_arg_attribute(attribute_type);

        NecroType* right  = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, ast->type_app.next_ty, arg_attribute_type));
        // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, left, ast->type_app.ty->source_loc, ast->type_app.ty->end_loc));
        // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, right, ast->type_app.next_ty->source_loc, ast->type_app.next_ty->end_loc));

        if (left->type == NECRO_TYPE_CON)
        {
            if (left->con.args == NULL)
            {
                left->con.args = necro_type_list_create(infer->arena, right, NULL);
            }
            else if (left->con.args != NULL)
            {
                NecroType* current_arg = left->con.args;
                while (current_arg->list.next != NULL)
                    current_arg = current_arg->list.next;
                current_arg->list.next = necro_type_list_create(infer->arena, right, NULL);
            }
            ast->necro_type = necro_type_find(left);
            ast->necro_type->kind = NULL;
            // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->necro_type, ast->source_loc, ast->end_loc));
            // TODO: Look at this!
            ast->necro_type->ownership = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
            assert(ast->necro_type != NULL);
            return ok(NecroType, ast->necro_type);
        }
        else
        {
            ast->necro_type = necro_type_app_create(infer->arena, left, right);
            // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, ast->necro_type, ast->source_loc, ast->end_loc));
            ast->necro_type->ownership = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
            assert(ast->necro_type != NULL);
            return ok(NecroType, ast->necro_type);
        }
    }

    case NECRO_AST_TYPE_ATTRIBUTE:
    {
        if (attribute_type != NECRO_TYPE_ATTRIBUTE_NONE)
        {
            if (attribute_type == NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR && ast->attribute.type == NECRO_TYPE_ATTRIBUTE_DOT)
            {
                return necro_ast_to_type_sig_go(infer, ast->attribute.attribute_type, NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR_DOT);
            }
            else
            {
                return necro_type_malformed_uniqueness_attribute_error(NULL, NULL, ast->source_loc, ast->end_loc);
            }
            // necro_unreachable(NecroType);
        }
        else if (ast->attribute.type == NECRO_TYPE_ATTRIBUTE_STAR)
        {
            return necro_ast_to_type_sig_go(infer, ast->attribute.attribute_type, NECRO_TYPE_ATTRIBUTE_STAR);
        }
        else if (ast->attribute.type == NECRO_TYPE_ATTRIBUTE_DOT)
        {
            return necro_ast_to_type_sig_go(infer, ast->attribute.attribute_type, NECRO_TYPE_ATTRIBUTE_DOT);
        }
        necro_unreachable(NecroType);
    }

    default: necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_infer_type_sig(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_SIGNATURE);

    NecroType*           type_sig    = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, ast->type_signature.type, NECRO_TYPE_ATTRIBUTE_NONE));
    NecroConstraintList* constraints = necro_try_map_result(NecroConstraintList, NecroType, necro_constraint_list_from_ast(infer, ast->type_signature.context));

    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type_sig, ast->scope, ast->type_signature.type->source_loc, ast->type_signature.type->end_loc));
    necro_try(NecroType, necro_uniqueness_propagate(infer->arena, &infer->con_env, infer->base, infer->intern, type_sig, ast->scope, NULL, true, ast->source_loc, ast->end_loc, NECRO_CONSTRAINT_UCOERCE));

    necro_try(NecroType, necro_constraint_list_kinds_check(infer->arena, infer->base, constraints, ast->scope));
    necro_try(NecroType, necro_constraint_ambiguous_type_class_check(ast->type_signature.var->variable.ast_symbol, constraints, type_sig));

    type_sig = necro_try_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, type_sig, ast->type_signature.type->scope));

    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, type_sig, ast->scope, ast->type_signature.type->source_loc, ast->type_signature.type->end_loc));

    type_sig->pre_supplied                             = true;
    ast->type_signature.var->variable.ast_symbol->type = type_sig;
    ast->necro_type                                    = type_sig;
    necro_constraint_list_apply(infer->arena, type_sig, constraints);

    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_ast_to_kind_sig_go(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        NecroType* kind_var = necro_type_var_create(infer->arena, ast->variable.ast_symbol, NULL);
        kind_var->kind      = infer->base->kind_kind->type;
        return ok(NecroType, kind_var);
    }
    case NECRO_AST_TUPLE:
        return necro_kind_unify_with_info(infer->base->kind_kind->type, infer->base->star_kind->type, NULL, ast->source_loc, ast->end_loc); // Will produce necessary unification error
    case NECRO_AST_FUNCTION_TYPE:
    {
        NecroType* left  = necro_try_result(NecroType, necro_ast_to_kind_sig_go(infer, ast->function_type.type));
        necro_try(NecroType, necro_kind_unify_with_info(infer->base->kind_kind->type, left->kind, NULL, ast->source_loc, ast->end_loc));
        NecroType* right = necro_try_result(NecroType, necro_ast_to_kind_sig_go(infer, ast->function_type.next_on_arrow));
        necro_try(NecroType, necro_kind_unify_with_info(infer->base->kind_kind->type, right->kind, NULL, ast->source_loc, ast->end_loc));
        // TODO: Error if kind has non-sharing arrow!
        ast->necro_type  = necro_type_fn_create(infer->arena, left, right);
        ast->necro_type->kind = infer->base->kind_kind->type;
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_CONID:
        ast->necro_type       = necro_type_con_create(infer->arena, ast->conid.ast_symbol->type->con.con_symbol, NULL);
        ast->necro_type->kind = ast->conid.ast_symbol->type->con.con_symbol->type->kind;
        necro_try(NecroType, necro_kind_unify_with_info(infer->base->kind_kind->type, ast->necro_type->kind, NULL, ast->source_loc, ast->end_loc));
        return ok(NecroType, ast->necro_type);

    case NECRO_AST_CONSTRUCTOR:
    {
        if (ast->constructor.arg_list != NULL)
            return necro_kind_unify_with_info(infer->base->kind_kind->type, infer->base->higher_kind->type, NULL, ast->source_loc, ast->end_loc); // Will produce necessary unification error
        NecroType* env_con_type = ast->constructor.conid->conid.ast_symbol->type;
        ast->necro_type         = necro_type_con_create(infer->arena, env_con_type->con.con_symbol, NULL);
        ast->necro_type->kind   = env_con_type->con.con_symbol->type->kind;
        necro_try(NecroType, necro_kind_unify_with_info(infer->base->kind_kind->type, ast->necro_type->kind, NULL, ast->source_loc, ast->end_loc));
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_TYPE_APP:
        return necro_kind_unify_with_info(infer->base->kind_kind->type, infer->base->higher_kind->type, NULL, ast->source_loc, ast->end_loc); // Will produce necessary unification error

    default: necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_infer_kind_sig(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_SIGNATURE);
    NecroType* kind_sig = necro_try_result(NecroType, necro_ast_to_kind_sig_go(infer, ast->type_signature.type));
    necro_try(NecroType, necro_kind_unify_with_info(infer->base->kind_kind->type, kind_sig->kind, NULL, ast->source_loc, ast->end_loc));
    // TODO: Throw error if kind signatures has a context!!!!!!!
    NecroType* tyvar_type                              = necro_type_var_create(infer->arena, ast->type_signature.var->variable.ast_symbol, NULL);
    tyvar_type->kind                                   = kind_sig;
    kind_sig->pre_supplied                             = true;
    ast->type_signature.var->variable.ast_symbol->type = tyvar_type;
    ast->necro_type                                    = tyvar_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Data Declaration
//=====================================================
size_t necro_count_ty_vars(NecroAst* ty_vars)
{
    size_t count = 0;
    while (ty_vars != NULL)
    {
        count++;
        ty_vars = ty_vars->list.next_item;
    }
    return count;
}

NecroResult(NecroType) necro_ty_vars_to_args(NecroInfer* infer, NecroAst* ty_vars, NecroType* type_uvar)
{
    NecroType* type = NULL;
    NecroType* head = NULL;
    while (ty_vars != NULL)
    {
        assert(ty_vars->type == NECRO_AST_LIST_NODE);
        NecroType* tyvar_type = NULL;
        if (ty_vars->list.item->type == NECRO_AST_VARIABLE)
        {
            if (ty_vars->list.item->variable.ast_symbol->type == NULL)
            {
                ty_vars->list.item->variable.ast_symbol->type            = necro_type_var_create(infer->arena, ty_vars->list.item->variable.ast_symbol, NULL);
                ty_vars->list.item->variable.ast_symbol->type->kind      = necro_kind_fresh_kind_var(infer->arena, infer->base, ty_vars->list.item->scope);
                ty_vars->list.item->variable.ast_symbol->type->ownership = necro_type_ownership_fresh_var(infer->arena, infer->base, NULL);
            }
            tyvar_type = ty_vars->list.item->variable.ast_symbol->type;
        }
        else if (ty_vars->list.item->type == NECRO_AST_TYPE_SIGNATURE)
        {
            tyvar_type            = necro_try_result(NecroType, necro_infer_kind_sig(infer, ty_vars->list.item));
            tyvar_type->ownership = type_uvar;
        }
        else
        {
            assert(false);
        }
        if (type == NULL)
        {

            head = necro_type_list_create(infer->arena, tyvar_type, NULL);
            type = head;
        }
        else
        {
            type->list.next = necro_type_list_create(infer->arena, tyvar_type, NULL);
            type = type->list.next;
        }
        ty_vars = ty_vars->list.next_item;
    }
    return ok(NecroType, head);
}

// TODO: Different uvars for each argument!
NecroResult(NecroType) necro_create_data_constructor(NecroInfer* infer, NecroAst* ast, NecroType* data_type, size_t con_num)
{
    if (data_type->ownership == NULL)
        data_type->ownership = necro_type_ownership_fresh_var(infer->arena, infer->base, NULL);
    NecroType* con_head  = NULL;
    NecroType* con_args  = NULL;
    NecroAst*  args_ast  = ast->constructor.arg_list;
    size_t     count     = 0;
    if (args_ast != NULL)
        data_type->con.con_symbol->is_enum = false;
    while (args_ast != NULL)
    {
        NecroType* arg = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, args_ast->list.item, NECRO_TYPE_ATTRIBUTE_CONSTRUCTOR));
        if (con_args == NULL)
        {
            con_args            = necro_type_fn_create(infer->arena, arg, NULL);
            con_head            = con_args;
            con_args->ownership = infer->base->ownership_share->type;
        }
        else
        {
            con_args->fun.type2 = necro_type_fn_create(infer->arena, arg, NULL);
            con_args            = con_args->fun.type2;
            con_args->ownership = infer->base->ownership_share->type;
        }
        args_ast             = args_ast->list.next_item;
        if (args_ast != NULL)
            args_ast->necro_type = necro_type_find(arg); // arg type
        count++;
    }
    NecroType* con_type = NULL;
    if (con_args == NULL)
    {
        con_type = data_type;
    }
    else
    {
        con_args->fun.type2 = data_type;
        con_type            = con_head;
    }

    necro_try_map(void, NecroType, necro_kind_infer_default(infer->arena, infer->base, con_type, ast->constructor.conid->source_loc, ast->constructor.conid->end_loc));
    necro_try(NecroType, necro_uniqueness_propagate_data_con(infer->arena, &infer->con_env, infer->base, infer->intern, con_type, ast->scope, NULL, ast->source_loc, ast->end_loc, data_type->con.con_symbol, data_type->ownership));

    con_type                                                 = necro_try_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, con_type, ast->scope));
    con_type->pre_supplied                                   = true;
    ast->constructor.conid->conid.ast_symbol->type           = con_type;
    ast->constructor.conid->conid.ast_symbol->is_constructor = true;
    ast->constructor.conid->conid.ast_symbol->con_num        = con_num;

    necro_try_map(void, NecroType, necro_kind_infer_default(infer->arena, infer->base, con_type, ast->constructor.conid->source_loc, ast->constructor.conid->end_loc));

    ast->necro_type = necro_type_find(con_type);

    // necro_type_print(ast->necro_type);
    // puts("");

    return ok(NecroType, ast->necro_type);
}

// Set is_wrapper flag, type must be of this general shape: data Wrapper a = Wrapper a (or monomorphic equivalant)
void necro_infer_set_is_wrapper_type(NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DATA_DECLARATION);
    NecroAstSymbol* symbol = ast->data_declaration.ast_symbol;
    if (symbol->is_primitive)
        return;
    NecroAst*       data_con        = ast->data_declaration.constructor_list->list.item;
    NecroAstSymbol* data_con_symbol = data_con->constructor.conid->conid.ast_symbol;
    if (data_con->type == NECRO_AST_CONID)
        return;
    if (ast->data_declaration.constructor_list->list.next_item != NULL)
    {
        if (data_con_symbol->source_name->str[data_con_symbol->source_name->length - 1] == '#')
            assert(false && "TODO: Unboxed types cannot be sum types error!");
        else
            return;
    }
    assert(data_con->type == NECRO_AST_CONSTRUCTOR);
    if (data_con->constructor.arg_list == NULL || data_con->constructor.arg_list->list.next_item != NULL)
    {
        if (data_con_symbol->source_name->str[data_con_symbol->source_name->length - 1] == '#')
        {
            // unboxed type
            data_con_symbol->is_unboxed = true;
            symbol->is_unboxed          = true;
        }
        return;
    }
    else
    {
        // wrapper type
        symbol->is_wrapper                                        = true;
        data_con->constructor.conid->conid.ast_symbol->is_wrapper = true;
    }
}

NecroResult(NecroType) necro_infer_simple_type(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_TYPE);
    assert(ast->simple_type.type_con->type == NECRO_AST_CONID);

    // Constructor type
    NecroType*   simple_type_type                 = necro_type_con_create(infer->arena, ast->simple_type.type_con->conid.ast_symbol, NULL);
    NecroType*   simple_type_result_kind          = infer->base->star_kind->type;

    // Constructor uvar
    NecroType*   simple_type_uvar                 = necro_type_ownership_fresh_var(infer->arena, infer->base, ast->scope);
    simple_type_uvar->var.is_rigid                = true;
    simple_type_uvar->var.var_symbol->source_name = necro_intern_string(infer->intern, "u");
    simple_type_uvar->var.var_symbol->name        = simple_type_uvar->var.var_symbol->source_name;
    simple_type_uvar->var.var_symbol->module_name = infer->ast_arena->module_name;
    necro_get_unique_name(infer->ast_arena, infer->intern, NECRO_VALUE_NAMESPACE, NECRO_MANGLE_NAME, simple_type_uvar->var.var_symbol);
    simple_type_type->ownership                   = simple_type_uvar;

    // Args type list with kinds
    NecroType*   args_type_list          = necro_try_result(NecroType, necro_ty_vars_to_args(infer, ast->simple_type.type_var_list, simple_type_uvar));

    // Derive simple type kind
    NecroType*   simple_type_kind        = NULL;
    NecroType*   simple_type_kind_curr   = NULL;
    NecroType*   curr_arg                = args_type_list;
    while (curr_arg != NULL)
    {
        assert(curr_arg->list.item->kind != NULL);
        if (simple_type_kind == NULL)
        {
            simple_type_kind      = necro_type_fn_create(infer->arena, curr_arg->list.item->kind, NULL);
            simple_type_kind_curr = simple_type_kind;
        }
        else
        {
            simple_type_kind_curr->fun.type2 = necro_type_fn_create(infer->arena, curr_arg->list.item->kind, NULL);
            simple_type_kind_curr            = simple_type_kind_curr->fun.type2;
        }
        curr_arg = curr_arg->list.next;
    }
    if (simple_type_kind != NULL)
        simple_type_kind_curr->fun.type2              = simple_type_result_kind;
    else
        simple_type_kind                              = simple_type_result_kind;
    simple_type_type->kind                            = simple_type_kind;
    simple_type_type->pre_supplied                    = true;
    ast->simple_type.type_con->conid.ast_symbol->type = simple_type_type;

    // Fully applied type
    NecroType*   fully_applied_type                   = necro_type_con_create(infer->arena, ast->simple_type.type_con->conid.ast_symbol, args_type_list);
    fully_applied_type->pre_supplied                  = true;
    ast->necro_type                                   = fully_applied_type;
    fully_applied_type->ownership                     = simple_type_uvar;

    unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, ast->necro_type, ast->source_loc, ast->end_loc));
    unwrap(NecroType, necro_kind_unify(ast->necro_type->kind, simple_type_result_kind, NULL));

    return ok(NecroType, ast->necro_type);
}

//=====================================================
// FreeVars
//=====================================================
NecroFScope* necro_infer_f_scope_push(NecroInfer* infer, NecroScope* scope)
{
    NecroFScope* f_scope   = necro_paged_arena_alloc(infer->arena, sizeof(NecroFScope));
    f_scope->parent        = infer->f_scope;
    f_scope->scope         = scope;
    f_scope->free_vars     = NULL;
    f_scope->is_loop_scope = false;
    infer->f_scope         = f_scope;
    return f_scope;
}

NecroFScope* necro_infer_f_scope_push_loop(NecroInfer* infer, NecroScope* scope)
{
    NecroFScope* f_scope   = necro_infer_f_scope_push(infer, scope);
    f_scope->is_loop_scope = true;
    return f_scope;
}

void necro_infer_f_scope_pop(NecroInfer* infer)
{
    assert(infer != NULL);
    assert(infer->f_scope != NULL);
    infer->f_scope = infer->f_scope->parent;
}

void necro_infer_f_scope_add_free_var(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_VARIABLE);
    assert(ast->variable.ast_symbol != NULL);
    assert(ast->variable.ast_symbol->ast != NULL);
    NecroType* utype = necro_type_find(ast->necro_type)->ownership;
    if (ast->variable.var_type != NECRO_VAR_VAR || ast->variable.ast_symbol == infer->base->prim_undefined || necro_type_is_ownership_share(infer->base, utype))
        return;
    NecroFScope* f_scope = infer->f_scope;
    while (f_scope != NULL)
    {
        bool        is_free_var = true;
        NecroScope* var_scope   = ast->variable.ast_symbol->ast->scope;
        while (var_scope != NULL)
        {
            if (var_scope == f_scope->scope)
            {
                is_free_var = false;
                break;
            }
            var_scope = var_scope->parent;
        }
        if (is_free_var)
        {
            f_scope->free_vars = necro_cons_free_var_list(infer->arena, (NecroFreeVar) { .symbol = ast->variable.ast_symbol, .type = necro_type_find(ast->necro_type), .ownership_type = utype, .source_loc = ast->source_loc, .end_loc = ast->end_loc }, f_scope->free_vars);
        }
        f_scope = f_scope->parent;
    }
}


//=====================================================
// Lambda
//=====================================================
NecroResult(NecroType) necro_infer_lambda(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LAMBDA);
    NecroFScope*      f_scope        = necro_infer_f_scope_push(infer, ast->lambda.apats->scope);
    NecroAst*         apats          = ast->lambda.apats;
    NecroType*        f_type         = NULL;
    NecroType*        f_head         = NULL;
    assert(apats != NULL);
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        NecroType* apat_type = necro_try_result(NecroType, necro_infer_apat(infer, apats->apats.apat));
        if (f_type == NULL)
        {
            f_type                = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_head                = f_type;
        }
        else
        {
            f_type->fun.type2     = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_type                = f_type->fun.type2;
        }
        apats     = apats->apats.next_apat;
    }
    assert(f_type != NULL);
    f_type->fun.type2 = necro_try_result(NecroType, necro_infer_go(infer, ast->lambda.expression));
    ast->necro_type   = f_head;
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    necro_try(NecroType, necro_uniqueness_propagate(infer->arena, &infer->con_env, infer->base, infer->intern, ast->necro_type, ast->scope, f_scope->free_vars, true, ast->source_loc, ast->end_loc, NECRO_CONSTRAINT_UCONSTRAINT));
    necro_infer_f_scope_pop(infer);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Assignment
//=====================================================
NecroResult(NecroType) necro_infer_apats_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_APATS_ASSIGNMENT);

    NecroFScope* f_scope    = necro_infer_f_scope_push(infer, ast->apats_assignment.apats->scope);
    NecroType*   proxy_type = ast->apats_assignment.ast_symbol->type;
    NecroScope*  uscope     = (ast->scope->parent == NULL) ? &necro_global_scope : ast->scope;
    proxy_type->ownership   = necro_try_result(NecroType, necro_infer_ast_symbol_uniqueness(infer, ast->apats_assignment.ast_symbol, proxy_type->ownership, uscope, ast->source_loc, ast->end_loc));

    // Unify args (long winded version for better error messaging)
    NecroAst*  apats  = ast->apats_assignment.apats;
    NecroType* f_type = NULL;
    NecroType* f_head = NULL;
    assert(apats != NULL);
    while (apats != NULL)
    {
        NecroType* apat_type = necro_try_result(NecroType, necro_infer_apat(infer, apats->apats.apat));
        if (f_type == NULL)
        {
            f_type                = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_head                = f_type;
        }
        else
        {
            f_type->fun.type2     = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_type                = f_type->fun.type2;
        }
        apats     = apats->apats.next_apat;
    }
    assert(f_type != NULL);

    // Unify rhs
    NecroType* rhs        = necro_try_result(NecroType, necro_infer_go(infer, ast->apats_assignment.rhs));
    NecroType* rhs_proxy  = necro_type_fresh_var(infer->arena,  ast->apats_assignment.rhs->scope);
    rhs_proxy->kind       = infer->base->star_kind->type;
    f_type->fun.type2     = rhs_proxy;

    // Unify args
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, f_head, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, proxy_type, f_head, ast->scope, ast->source_loc, ast->apats_assignment.apats->end_loc));

    // Unify rhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, rhs_proxy, rhs, ast->scope, ast->apats_assignment.rhs->source_loc, ast->apats_assignment.rhs->end_loc));
    ast->necro_type = necro_type_find(f_head);
    necro_try(NecroType, necro_uniqueness_propagate(infer->arena, &infer->con_env, infer->base, infer->intern, ast->necro_type, ast->scope, f_scope->free_vars, true, ast->source_loc, ast->end_loc, NECRO_CONSTRAINT_UCONSTRAINT));
    necro_infer_f_scope_pop(infer);
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_simple_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    NecroType* proxy_type = ast->simple_assignment.ast_symbol->type;
    NecroScope* uscope    = (ast->scope->parent == NULL) ? &necro_global_scope : ast->scope;
    proxy_type->ownership = necro_try_result(NecroType, necro_infer_ast_symbol_uniqueness(infer, ast->simple_assignment.ast_symbol, proxy_type->ownership, uscope, ast->source_loc, ast->end_loc));
    NecroType* init_type  = necro_try_result(NecroType, necro_infer_go(infer, ast->simple_assignment.initializer));
    NecroType* rhs_type   = necro_try_result(NecroType, necro_infer_go(infer, ast->simple_assignment.rhs));
    if (ast->simple_assignment.declaration_group != NULL && ast->simple_assignment.declaration_group->declaration.next_declaration != NULL)
        ast->simple_assignment.is_recursive = true;
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, rhs_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, proxy_type, rhs_type, ast->scope, ast->simple_assignment.rhs->source_loc, ast->simple_assignment.rhs->end_loc));
    if (init_type != NULL)
    {
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, proxy_type, init_type, ast->scope, ast->simple_assignment.initializer->source_loc, ast->simple_assignment.initializer->end_loc));
    }
    ast->necro_type = necro_type_find(rhs_type);
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_pat_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_PAT_ASSIGNMENT);
    assert(ast->pat_assignment.pat != NULL);
    NecroType* pat_type = necro_try_result(NecroType, necro_infer_apat(infer, ast->pat_assignment.pat));
    NecroType* rhs_type = necro_try_result(NecroType, necro_infer_go(infer, ast->pat_assignment.rhs));
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, pat_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, rhs_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, pat_type, rhs_type, ast->scope, ast->pat_assignment.rhs->source_loc, ast->pat_assignment.rhs->end_loc));
    ast->necro_type = necro_type_find(pat_type);
    return ok(NecroType, ast->necro_type);
}

void necro_pat_new_name_go(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        NecroAstSymbol* data = ast->variable.ast_symbol;
        NecroType*      type = data->type;
        if (type == NULL)
        {
            NecroType* new_name  = necro_type_fresh_var(infer->arena, data->ast->scope);
            new_name->kind       = infer->base->star_kind->type;
            data->type           = new_name;
            data->type_status    = NECRO_TYPE_CHECKING;
        }
        else
        {
            type->pre_supplied = true;
        }
        if (data->ast == NULL)
            data->ast = ast;
        return;
    }
    case NECRO_AST_CONSTANT: return;
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_pat_new_name_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_pat_new_name_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    case NECRO_AST_WILDCARD: return;
    case NECRO_AST_BIN_OP_SYM:
    {
        necro_pat_new_name_go(infer, ast->bin_op_sym.left);
        necro_pat_new_name_go(infer, ast->bin_op_sym.right);
        return;
    }
    case NECRO_AST_CONID: return;
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_pat_new_name_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    default: assert(false); return;
    }
}

NecroResult(NecroType) necro_gen_pat_go(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        NecroAstSymbol* var_symbol = ast->variable.ast_symbol;
        if (var_symbol->type->pre_supplied || var_symbol->type_status == NECRO_TYPE_DONE)
        {
            var_symbol->type_status = NECRO_TYPE_DONE;
            ast->necro_type   = var_symbol->type;
            return ok(NecroType, ast->necro_type);
        }
        // Monomorphism restriction applies to pattern assignment
        // Is the scoping right on this!?
        // var_symbol->type        = necro_try_result(NecroType, necro_type_generalize(infer->arena, infer->base, var_symbol->type, ast->scope->parent));
        var_symbol->type_status = NECRO_TYPE_DONE;
        // necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, var_symbol->type, ast->source_loc, ast->end_loc));
        // // var_symbol->type->kind = necro_kind_gen(infer->arena, infer->base, var_symbol->type->kind);
        // necro_kind_default_type_kinds(infer->arena, infer->base, var_symbol->type);
        // necro_try(NecroType, necro_kind_unify_with_info(infer->base->star_kind->type, var_symbol->type->kind, NULL, ast->source_loc, ast->end_loc));
        necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, var_symbol->type, NULL, ast->source_loc, ast->end_loc));
        ast->necro_type = necro_type_find(var_symbol->type);
        // rec check
        if (ast->variable.is_recursive)
        {
        }
        return ok(NecroType, ast->necro_type);
    }
    case NECRO_AST_CONSTANT:
        return ok(NecroType, NULL);
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_try(NecroType, necro_gen_pat_go(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_try(NecroType, necro_gen_pat_go(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_AST_WILDCARD:
        return ok(NecroType, NULL);
    case NECRO_AST_BIN_OP_SYM:
        necro_try(NecroType, necro_gen_pat_go(infer, ast->bin_op_sym.left));
        return necro_gen_pat_go(infer, ast->bin_op_sym.right);
    case NECRO_AST_CONID:
        return ok(NecroType, NULL);
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_try(NecroType, necro_gen_pat_go(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    default:
        necro_unreachable(NecroType);
    }
}

//=====================================================
// Variable
//=====================================================
NecroResult(NecroType) necro_infer_var_initializer(NecroInfer* infer, NecroAst* ast)
{
    if (ast->variable.initializer == NULL)
        return ok(NecroType, NULL);
    NecroType* init_type = necro_try_result(NecroType, necro_infer_go(infer, ast->variable.initializer));
    return necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, ast->necro_type, init_type, ast->scope, ast->variable.initializer->source_loc, ast->variable.initializer->end_loc);
}

NecroResult(NecroType) necro_infer_ast_symbol_uniqueness(NecroInfer* infer, NecroAstSymbol* ast_symbol, NecroType* prev_uniqueness, NecroScope* scope, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    if (ast_symbol == infer->base->prim_undefined)
    {
        // return ok(NecroType, necro_type_ownership_fresh_var(infer->arena, infer->base, scope));
        return ok(NecroType, prev_uniqueness);
    }
    else if (prev_uniqueness == NULL)
    {
        if (!necro_usage_is_unshared(ast_symbol->usage))
        {
            return ok(NecroType, infer->base->ownership_share->type);
        }
        else
        {
            NecroType* var = necro_type_ownership_fresh_var(infer->arena, infer->base, scope);
            return ok(NecroType, var);
        }
    }
    else if (!necro_usage_is_unshared(ast_symbol->usage))
    {
        necro_try(NecroType, necro_type_ownership_unify_with_info(infer->arena, &infer->con_env, infer->base, infer->base->ownership_share->type, prev_uniqueness, scope, source_loc, end_loc));
        return ok(NecroType, prev_uniqueness);
    }
    else
    {
        return ok(NecroType, prev_uniqueness);
    }
}

NecroResult(NecroType) necro_infer_var(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_VARIABLE);
    NecroAstSymbol* data = ast->variable.ast_symbol;
    assert(data != NULL);

    // assert?
    if (data->declaration_group != NULL)
        assert(data->type != NULL);

    if (ast->variable.var_type == NECRO_VAR_VAR && data->declaration_group != NULL && data->type_status != NECRO_TYPE_DONE && data->ast != NULL)
    {
        if (data->ast->type == NECRO_AST_SIMPLE_ASSIGNMENT && data->ast->simple_assignment.initializer == NULL && data->ast->simple_assignment.ast_symbol != infer->base->prim_undefined)
        {
            return necro_type_uninitialized_recursive_value_error(data, data->type, data->ast->source_loc, data->ast->end_loc);
        }
        else if (data->ast->type == NECRO_AST_VARIABLE && data->ast->variable.var_type == NECRO_VAR_DECLARATION)
        {
            if (data->ast->variable.initializer == NULL)
            {
                return necro_type_uninitialized_recursive_value_error(data, data->type, data->ast->source_loc, data->ast->end_loc);
            }
            else
            {
                data->ast->variable.is_recursive = true;
                data->is_recursive               = true;
            }
        }
    }

    if (data->type == NULL)
    {
        assert(ast->variable.var_type == NECRO_VAR_DECLARATION);
        data->type            = necro_type_fresh_var(infer->arena, ast->scope);
        ast->necro_type       = data->type;
        ast->necro_type->kind = infer->base->star_kind->type;
        necro_try(NecroType, necro_infer_var_initializer(infer, ast));
        ast->necro_type->ownership = necro_try_result(NecroType, necro_infer_ast_symbol_uniqueness(infer, ast->variable.ast_symbol, ast->necro_type->ownership, ast->scope, ast->source_loc, ast->end_loc));
        // necro_try(NecroType, necro_infer_var_uniqueness(infer, ast));
    }
    else if (necro_type_is_bound_in_scope(data->type, ast->scope))
    {
        ast->necro_type = necro_type_find(data->type);
        necro_try(NecroType, necro_infer_var_initializer(infer, ast));
        ast->necro_type->ownership = necro_try_result(NecroType, necro_infer_ast_symbol_uniqueness(infer, ast->variable.ast_symbol, ast->necro_type->ownership, ast->scope, ast->source_loc, ast->end_loc));
        // necro_try(NecroType, necro_infer_var_uniqueness(infer, ast));
    }
    else
    {
        ast->variable.inst_subs = NULL;
        NecroType* inst_type    = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, data->type, ast->scope, &ast->variable.inst_subs, ast->source_loc, ast->end_loc);
        ast->necro_type         = necro_type_find(inst_type);
        necro_try(NecroType, necro_infer_var_initializer(infer, ast));
        ast->necro_type->ownership = necro_try_result(NecroType, necro_infer_ast_symbol_uniqueness(infer, ast->variable.ast_symbol, ast->necro_type->ownership, ast->scope, ast->source_loc, ast->end_loc));
        // necro_try(NecroType, necro_infer_var_uniqueness(infer, ast));
    }

    // Add free vars
    necro_infer_f_scope_add_free_var(infer, ast);

    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Constant
//=====================================================
NecroType* necro_infer_constant(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONSTANT);
    switch (ast->constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:
        ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->float_con->type, ast->scope, ast->source_loc, ast->end_loc);
        return ast->necro_type;
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
        // TODO: Overloaded numeric literal patterns
        ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->float_con->type, ast->scope, ast->source_loc, ast->end_loc);
        return ast->necro_type;
    case NECRO_AST_CONSTANT_INTEGER:
        ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->int_con->type, ast->scope, ast->source_loc, ast->end_loc);
        return ast->necro_type;
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
        // TODO: Overloaded numeric literal patterns
        ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->int_con->type, ast->scope, ast->source_loc, ast->end_loc);
        return ast->necro_type;
    case NECRO_AST_CONSTANT_CHAR:
        ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->char_con->type, ast->scope, ast->source_loc, ast->end_loc);
        return ast->necro_type;
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
        ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->char_con->type, ast->scope, ast->source_loc, ast->end_loc);
        return ast->necro_type;
    case NECRO_AST_CONSTANT_STRING:
    {
        NecroType* arity_type = necro_type_nat_create(infer->arena, strlen(ast->constant.symbol->str));
        arity_type->kind      = infer->base->nat_kind->type;
        NecroType* char_type  = necro_type_con_create(infer->arena, infer->base->char_type, NULL);
        char_type->ownership  = infer->base->ownership_share->type;
        NecroType* array_type = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, char_type);
        ast->necro_type       = array_type;
        unwrap(void, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        ast->necro_type->ownership = infer->base->ownership_share->type;
        return ast->necro_type;
    }
    case NECRO_AST_CONSTANT_TYPE_INT:
        ast->necro_type       = necro_type_nat_create(infer->arena, (size_t) ast->constant.int_literal);
        ast->necro_type->kind = infer->base->nat_kind->type;
        return ast->necro_type;
    default:
        assert(false);
        return NULL;
    }
}

//=====================================================
// ConID
//=====================================================
NecroResult(NecroType) necro_infer_conid(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONID);
    ast->necro_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, ast->conid.ast_symbol->type, ast->scope, ast->source_loc, ast->end_loc);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Wildcard
//=====================================================
NecroType* necro_infer_wildcard(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_WILDCARD);
    ast->necro_type       = necro_type_fresh_var(infer->arena, NULL);
    ast->necro_type->kind = infer->base->star_kind->type;
    return ast->necro_type;
}

//=====================================================
// Tuple
//=====================================================
NecroResult(NecroType) necro_infer_tuple(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        NecroType* item_type = necro_try_result(NecroType, necro_infer_go(infer, current_expression->list.item));
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, item_type, NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, item_type, NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    if (ast->tuple.is_unboxed)
        ast->necro_type = necro_type_unboxed_tuple_con_create(infer->arena, infer->base, types_head);
    else
        ast->necro_type = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_tuple_pattern(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        NecroType* item_type = necro_try_result(NecroType, necro_infer_pattern(infer, current_expression->list.item));
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, item_type, NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, item_type, NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    if (ast->tuple.is_unboxed)
        ast->necro_type = necro_type_unboxed_tuple_con_create(infer->arena, infer->base, types_head);
    else
        ast->necro_type = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_tuple_type(NecroInfer* infer, NecroAst* ast, NECRO_TYPE_ATTRIBUTE_TYPE attribute_type)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        NECRO_TYPE_ATTRIBUTE_TYPE arg_attribute_type = necro_type_attribute_get_arg_attribute(attribute_type);
        NecroType*                item_type          = necro_try_result(NecroType, necro_ast_to_type_sig_go(infer, current_expression->list.item, arg_attribute_type));
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, item_type, NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, item_type, NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    if (ast->tuple.is_unboxed)
        ast->necro_type = necro_type_unboxed_tuple_con_create(infer->arena, infer->base, types_head);
    else
        ast->necro_type = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    // necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    // ast->necro_type->ownership = infer->base->ownership_share->type;
    ast->necro_type->ownership = necro_try_result(NecroType, necro_ast_get_type_sig_ownership(infer, attribute_type, ast->necro_type->type, ast->source_loc, ast->end_loc));
    return ok(NecroType, ast->necro_type);
}

///////////////////////////////////////////////////////
// Expression Array
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_infer_expression_array(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_ARRAY);
    NecroAst*  current_cell  = ast->expression_array.expressions;
    NecroType* element_type  = necro_type_fresh_var(infer->arena, NULL);
    element_type->kind       = infer->base->star_kind->type;
    size_t    arity          = 0;
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try_result(NecroType, necro_infer_go(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, item_type, element_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
        arity++;
        current_cell = current_cell->list.next_item;
    }
    NecroType* arity_type   = necro_type_nat_create(infer->arena, arity);
    arity_type->kind        = infer->base->nat_kind->type;
    ast->necro_type         = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, element_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Expression List
//=====================================================
NecroResult(NecroType) necro_infer_expression_list(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    // NOTE: Changing Lists to be arrays given language changes.

    // NecroAst*  current_cell = ast->expression_list.expressions;
    // NecroType* list_type    = necro_type_fresh_var(infer->arena);
    // list_type->kind         = infer->base->star_kind->type;
    // while (current_cell != NULL)
    // {
    //     NecroType* item_type = necro_try_result(NecroType, necro_infer_go(infer, current_cell->list.item));
    //     necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, list_type, item_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
    //     current_cell = current_cell->list.next_item;
    // }
    // ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, list_type);
    // necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    // return ok(NecroType, ast->necro_type);

    NecroAst*  current_cell  = ast->expression_list.expressions;
    NecroType* element_type  = necro_type_fresh_var(infer->arena, NULL);
    element_type->kind       = infer->base->star_kind->type;
    size_t    arity          = 0;
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try_result(NecroType, necro_infer_go(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, item_type, element_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
        arity++;
        current_cell = current_cell->list.next_item;
    }
    NecroType* arity_type   = necro_type_nat_create(infer->arena, arity);
    arity_type->kind        = infer->base->nat_kind->type;
    ast->necro_type         = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, element_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_expression_list_pattern(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    // NOTE: Changing Lists to be arrays given language changes

    // NecroAst*  current_cell = ast->expression_list.expressions;
    // NecroType* list_type    = necro_type_fresh_var(infer->arena);
    // list_type->kind         = infer->base->star_kind->type;
    // while (current_cell != NULL)
    // {
    //     NecroType* item_type = necro_try_result(NecroType, necro_infer_pattern(infer, current_cell->list.item));
    //     necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, list_type, item_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
    //     current_cell = current_cell->list.next_item;
    // }
    // ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, list_type);
    // necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    // return ok(NecroType, ast->necro_type);

    NecroAst*  current_cell  = ast->expression_list.expressions;
    NecroType* element_type  = necro_type_fresh_var(infer->arena, NULL);
    element_type->kind       = infer->base->star_kind->type;
    size_t    arity          = 0;
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try_result(NecroType, necro_infer_pattern(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, item_type, element_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
        arity++;
        current_cell = current_cell->list.next_item;
    }
    NecroType* arity_type   = necro_type_nat_create(infer->arena, arity);
    arity_type->kind        = infer->base->nat_kind->type;
    ast->necro_type         = necro_type_con2_create(infer->arena, infer->base->array_type, arity_type, element_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Pat Expression
//=====================================================
NecroResult(NecroType) necro_infer_seq_expression(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SEQ_EXPRESSION);
    // Infer types
    NecroAst*  expressions    = ast->sequence_expression.expressions;
    NecroType* element_type   = necro_type_fresh_var(infer->arena, NULL);
    element_type->kind        = necro_type_fresh_var(infer->arena, NULL);
    NecroType* seq_type       = necro_type_con1_create(infer->arena, infer->base->seq_type, element_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, seq_type, ast->scope, ast->source_loc, ast->end_loc));
    while (expressions != NULL)
    {
        NecroType* item_type = necro_try_result(NecroType, necro_infer_go(infer, expressions->list.item));
        necro_try(NecroType, necro_type_unify(infer->arena, &infer->con_env, infer->base, seq_type, item_type, ast->scope));
        expressions = expressions->list.next_item;
    }
    // Tick / Run
    switch (ast->sequence_expression.sequence_type)
    {
    case NECRO_SEQUENCE_SEQUENCE:   ast->sequence_expression.tick_symbol = infer->base->seq_tick;        break;
    case NECRO_SEQUENCE_TUPLE:      ast->sequence_expression.tick_symbol = infer->base->tuple_tick;      break;
    case NECRO_SEQUENCE_INTERLEAVE: ast->sequence_expression.tick_symbol = infer->base->interleave_tick; break;
    }
    assert(ast->sequence_expression.tick_symbol->type != NULL);
    ast->sequence_expression.run_seq_symbol    = infer->base->run_seq;
    ast->sequence_expression.tick_inst_subs    = NULL;
    NecroType* inst_tick_type                  = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, ast->sequence_expression.tick_symbol->type, ast->scope, &ast->sequence_expression.tick_inst_subs, ast->source_loc, ast->end_loc);
    necro_try(NecroType, necro_type_unify(infer->arena, &infer->con_env, infer->base, inst_tick_type->fun.type2->fun.type2->fun.type1->con.args->list.item, element_type, ast->scope));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, inst_tick_type, ast->source_loc, ast->end_loc));
    ast->sequence_expression.run_seq_inst_subs = NULL;
    NecroType* run_seq_inst_type               = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, ast->sequence_expression.run_seq_symbol->type, ast->scope, &ast->sequence_expression.run_seq_inst_subs, ast->source_loc, ast->end_loc);
    necro_try(NecroType, necro_type_unify(infer->arena, &infer->con_env, infer->base, run_seq_inst_type->fun.type1, seq_type, ast->scope));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, run_seq_inst_type, ast->source_loc, ast->end_loc));
    // Finish up
    ast->necro_type = seq_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Function Expression
//=====================================================
NecroResult(NecroType) necro_infer_fexpr(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    NecroType* e1_type     = necro_try_result(NecroType, necro_infer_go(infer, ast->fexpression.next_fexpression));
    NecroType* e0_type     = necro_try_result(NecroType, necro_infer_go(infer, ast->fexpression.aexp));

    NecroType* arg_type    = necro_type_fresh_var(infer->arena, NULL);
    arg_type->kind         = infer->base->star_kind->type;
    NecroType* result_type = necro_type_fresh_var(infer->arena, NULL);
    result_type->kind      = infer->base->star_kind->type;
    NecroType* f_type      = necro_type_fn_create(infer->arena, arg_type, result_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, f_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify f (in f x)
    NecroResult(NecroType) f_result = necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, f_type, e0_type, ast->scope, ast->fexpression.aexp->source_loc, ast->fexpression.aexp->end_loc);
    if (f_result.type == NECRO_RESULT_ERROR)
    {
        // Does this break things?
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, arg_type, e1_type, ast->scope, ast->fexpression.next_fexpression->source_loc, ast->fexpression.next_fexpression->end_loc));
        return f_result;
    }

    // Unify x (in f x)
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, arg_type, e1_type, ast->scope, ast->fexpression.next_fexpression->source_loc, ast->fexpression.next_fexpression->end_loc));

    ast->necro_type        = result_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// If then else
//=====================================================
NecroResult(NecroType) necro_infer_if_then_else(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_IF_THEN_ELSE);
    NecroType* if_type   = necro_try_result(NecroType, necro_infer_go(infer, ast->if_then_else.if_expr));
    NecroType* then_type = necro_try_result(NecroType, necro_infer_go(infer, ast->if_then_else.then_expr));
    NecroType* else_type = necro_try_result(NecroType, necro_infer_go(infer, ast->if_then_else.else_expr));
    NecroType* bool_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->true_con->type, ast->scope, ast->source_loc, ast->end_loc);
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, bool_type, if_type, ast->scope, ast->if_then_else.if_expr->source_loc, ast->if_then_else.if_expr->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, then_type, else_type, ast->scope, ast->if_then_else.else_expr->source_loc, ast->if_then_else.else_expr->end_loc));
    ast->necro_type = necro_type_find(then_type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// BinOp
//=====================================================
NecroResult(NecroType) necro_infer_bin_op(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_BIN_OP);

    // HACK FOR base.necro!!!! Figure out a better solution
    if (ast->bin_op.ast_symbol->type == NULL)
    {
        ast->bin_op.ast_symbol->type = necro_type_fresh_var(infer->arena, NULL);
    }

    ast->bin_op.inst_subs    = NULL;
    NecroType* op_type       = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, ast->bin_op.ast_symbol->type, ast->scope, &ast->bin_op.inst_subs, ast->source_loc, ast->end_loc);
    NecroType* y_type        = necro_try_result(NecroType, necro_infer_go(infer, ast->bin_op.rhs));
    NecroType* x_type        = necro_try_result(NecroType, necro_infer_go(infer, ast->bin_op.lhs));

    NecroType* left_type     = necro_type_fresh_var(infer->arena, NULL);
    left_type->kind          = infer->base->star_kind->type;
    NecroType* right_type    = necro_type_fresh_var(infer->arena, NULL);
    right_type->kind         = infer->base->star_kind->type;
    NecroType* result_type   = necro_type_fresh_var(infer->arena, NULL);
    result_type->kind        = infer->base->star_kind->type;
    NecroType* bin_op_type   = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, result_type));
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, bin_op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify op
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, bin_op_type, op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify rhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, right_type, y_type, ast->scope, ast->bin_op.rhs->source_loc, ast->bin_op.rhs->end_loc));

    // Unify lhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, left_type, x_type, ast->scope, ast->bin_op.lhs->source_loc, ast->bin_op.lhs->end_loc));

    ast->necro_type     = result_type;
    ast->bin_op.op_type = op_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Operator Left Section
//=====================================================
NecroResult(NecroType) necro_infer_op_left_section(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_OP_LEFT_SECTION);
    NecroType* x_type                  = necro_try_result(NecroType, necro_infer_go(infer, ast->op_left_section.left));
    ast->op_left_section.inst_subs     = NULL;
    NecroType* op_type                 = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, ast->op_left_section.ast_symbol->type, ast->scope, &ast->op_left_section.inst_subs, ast->source_loc, ast->end_loc);
    ast->op_left_section.op_necro_type = op_type;

    NecroType* left_type               = necro_type_fresh_var(infer->arena, NULL);
    left_type->kind                    = infer->base->star_kind->type;
    NecroType* result_type             = necro_type_fresh_var(infer->arena, NULL);
    result_type->kind                  = infer->base->star_kind->type;
    NecroType* section_type            = necro_type_fn_create(infer->arena, left_type, result_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, section_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify op
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, section_type, op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify lhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, left_type, x_type, ast->scope, ast->op_left_section.left->source_loc, ast->op_left_section.left->end_loc));

    ast->necro_type = necro_type_find(result_type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Operator Right Section
//=====================================================
NecroResult(NecroType) necro_infer_op_right_section(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_OP_RIGHT_SECTION);
    ast->op_right_section.inst_subs     = NULL;
    NecroType* op_type                  = necro_type_instantiate_with_subs(infer->arena, &infer->con_env, infer->base, ast->op_right_section.ast_symbol->type, ast->scope, &ast->op_right_section.inst_subs, ast->source_loc, ast->end_loc);
    ast->op_right_section.op_necro_type = op_type;
    NecroType* y_type                   = necro_try_result(NecroType, necro_infer_go(infer, ast->op_right_section.right));

    NecroType* left_type                = necro_type_fresh_var(infer->arena, NULL);
    left_type->kind                     = infer->base->star_kind->type;
    NecroType* right_type               = necro_type_fresh_var(infer->arena, NULL);
    right_type->kind                    = infer->base->star_kind->type;
    NecroType* result_type              = necro_type_fresh_var(infer->arena, NULL);
    result_type->kind                   = infer->base->star_kind->type;
    NecroType* bin_op_type              = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, result_type));
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, bin_op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify op
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, bin_op_type, op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify rhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, right_type, y_type, ast->scope, ast->op_right_section.right->source_loc, ast->op_right_section.right->end_loc));

    ast->necro_type = necro_type_fn_create(infer->arena, left_type, result_type);
    necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Apat
//=====================================================
NecroResult(NecroType) necro_infer_apat(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:        return necro_infer_var(infer, ast);
    case NECRO_AST_TUPLE:           return necro_infer_tuple_pattern(infer, ast);
    case NECRO_AST_EXPRESSION_LIST: return necro_infer_expression_list_pattern(infer, ast);
    case NECRO_AST_CONSTANT:        return ok(NecroType, necro_infer_constant(infer, ast));
    case NECRO_AST_WILDCARD:
    {
        NecroType* wildcard_type = necro_type_fresh_var(infer->arena, ast->scope);
        wildcard_type->kind      = infer->base->star_kind->type;
        ast->necro_type          = wildcard_type;
        return ok(NecroType, wildcard_type);
    }

    case NECRO_AST_BIN_OP_SYM:
    {
        assert(ast->bin_op_sym.op->type == NECRO_AST_CONID);
        NecroType* constructor_type = ast->bin_op_sym.op->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type            = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, constructor_type, ast->scope, ast->source_loc, ast->end_loc);
        NecroType* left_type        = necro_try_result(NecroType, necro_infer_apat(infer, ast->bin_op_sym.left));
        NecroType* right_type       = necro_try_result(NecroType, necro_infer_apat(infer, ast->bin_op_sym.right));
        NecroType* data_type        = necro_type_fresh_var(infer->arena, ast->scope);
        data_type->kind             = infer->base->star_kind->type;
        NecroType* f_type           = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, data_type));
        necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, f_type, ast->scope, ast->source_loc, ast->end_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, constructor_type, f_type, ast->scope, ast->source_loc, ast->end_loc));
        ast->necro_type             = f_type;
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_CONID:
    {
        NecroType* constructor_type  = ast->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, constructor_type, ast->scope, ast->source_loc, ast->end_loc);
        constructor_type = necro_type_get_fully_applied_fun_type(constructor_type);
        NecroType* type  = necro_try_result(NecroType, necro_infer_conid(infer, ast));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, constructor_type, type, ast->scope, ast->source_loc, ast->end_loc));
        ast->necro_type = necro_type_find(constructor_type);
        return ok(NecroType, ast->necro_type);
    }

    // TODO: unify against constructor function for correct uniqueness handling!
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroType* constructor_type        = necro_type_find(ast->constructor.conid->conid.ast_symbol->type);
        assert(constructor_type != NULL);
        constructor_type                   = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, constructor_type, ast->scope, ast->source_loc, ast->end_loc);
        ast->constructor.conid->necro_type = constructor_type;
        NecroType* pattern_type_head       = NULL;
        NecroType* pattern_type            = NULL;
        NecroAst*  ast_args                = ast->constructor.arg_list;
        size_t     arg_count               = 0;
        while (ast_args != NULL)
        {
            NecroType* item_type = necro_try_result(NecroType, necro_infer_apat(infer, ast_args->list.item));
            if (pattern_type_head == NULL)
            {
                pattern_type_head = necro_type_fn_create(infer->arena, item_type, NULL);
                pattern_type      = pattern_type_head;
            }
            else
            {
                pattern_type->fun.type2 = necro_type_fn_create(infer->arena, item_type, NULL);
                pattern_type            = pattern_type->fun.type2;
            }
            arg_count++;
            ast_args = ast_args->list.next_item;
        }

        NecroType* result_type = necro_type_fresh_var(infer->arena, ast->scope);
        result_type->kind      = infer->base->star_kind->type;
        if (pattern_type_head == NULL)
        {
            pattern_type_head = result_type;
        }
        else
        {
            pattern_type->fun.type2 = result_type;
        }

        necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, pattern_type_head, ast->scope, ast->source_loc, ast->end_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, constructor_type, pattern_type_head, ast->scope, ast->source_loc, ast->end_loc));

        // NOTE: Double check, but I believe we can remove this as we do a kinds check up above
        // Ensure it's fully_applied
        // NecroType* fully_applied_constructor = necro_try_result(NecroType, necro_type_instantiate(infer->arena, &infer->con_env, infer->base, ast->constructor.conid->conid.ast_symbol->type, ast->scope, ast->source_loc, ast->end_loc));
        // fully_applied_constructor            = necro_type_get_fully_applied_fun_type(fully_applied_constructor);
        // necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, fully_applied_constructor, result_type, ast->scope, ast->source_loc, ast->end_loc));

        ast->necro_type = necro_type_find(result_type);
        return ok(NecroType, ast->necro_type);
    }

    default:
        necro_unreachable(NecroType);
    }
}

//=====================================================
// Pattern
//=====================================================
NecroResult(NecroType) necro_infer_pattern(NecroInfer* infer, NecroAst* ast)
{
    return necro_infer_apat(infer, ast);
}

//=====================================================
// Case
//=====================================================
NecroResult(NecroType) necro_infer_case(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CASE);
    NecroType* expression_type = necro_try_result(NecroType, necro_infer_go(infer, ast->case_expression.expression));
    NecroType* result_type     = necro_type_fresh_var(infer->arena, NULL);
    result_type->kind          = infer->base->star_kind->type;
    NecroAst*  alternatives    = ast->case_expression.alternatives;
    while (alternatives != NULL)
    {
        NecroAst*  alternative = alternatives->list.item;
        NecroType* alt_type    = necro_try_result(NecroType, necro_infer_pattern(infer, alternative->case_alternative.pat))
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, expression_type, alt_type, alternatives->scope, alternative->case_alternative.pat->source_loc, alternative->case_alternative.pat->end_loc));
        NecroType* body_type   = necro_try_result(NecroType, necro_infer_go(infer, alternative->case_alternative.body));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, result_type, body_type, alternatives->scope, alternative->case_alternative.body->source_loc, alternative->case_alternative.body->end_loc));
        alternatives = alternatives->list.next_item;
    }
    ast->necro_type = necro_type_find(result_type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Loop Free Vars
//=====================================================
NecroResult(NecroType) necro_infer_share_loop_free_vars(NecroInfer* infer, NecroFreeVarList* free_vars, NecroScope* scope)
{
    while (free_vars != NULL)
    {
        necro_try(NecroType, necro_type_unify_with_full_info(infer->arena, &infer->con_env, infer->base, infer->base->ownership_share->type, free_vars->data.ownership_type, scope, free_vars->data.source_loc, free_vars->data.end_loc, infer->base->ownership_share->type, free_vars->data.ownership_type));
        free_vars = free_vars->next;
    }
    return ok(NecroType, NULL);
}

//=====================================================
// For Loop
//=====================================================
NecroResult(NecroType) necro_infer_for_loop(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_FOR_LOOP);
    NecroType*   range_init_type = necro_try_result(NecroType, necro_infer_go(infer, ast->for_loop.range_init));
    NecroType*   value_init_type = necro_try_result(NecroType, necro_infer_go(infer, ast->for_loop.value_init));
    NecroFScope* f_scope         = necro_infer_f_scope_push(infer, ast->for_loop.value_apat->scope);
    NecroType*   index_apat_type = necro_try_result(NecroType, necro_infer_pattern(infer, ast->for_loop.index_apat));
    NecroType*   value_apat_type = necro_try_result(NecroType, necro_infer_pattern(infer, ast->for_loop.value_apat));
    NecroType*   expression_type = necro_try_result(NecroType, necro_infer_go(infer, ast->for_loop.expression));
    NecroType*   n_type          = necro_type_fresh_var(infer->arena, NULL);
    NecroType*   range_type      = necro_type_con1_create(infer->arena, infer->base->range_type, n_type);
    NecroType*   index_type      = necro_type_con1_create(infer->arena, infer->base->index_type, n_type);
    ast->necro_type              = necro_type_find(expression_type);
    unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, range_type, ast->source_loc, ast->end_loc));
    unwrap(NecroType, necro_kind_infer(infer->arena, infer->base, index_type, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, range_type, range_init_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, index_type, index_apat_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, value_init_type, value_apat_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, value_apat_type, expression_type, ast->scope, ast->source_loc, ast->end_loc));
    necro_try(NecroType, necro_infer_share_loop_free_vars(infer, f_scope->free_vars, ast->for_loop.value_apat->scope));
    necro_infer_f_scope_pop(infer);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// While Loop
//=====================================================
NecroResult(NecroType) necro_infer_while_loop(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_WHILE_LOOP);
    NecroType*   value_init_type  = necro_try_result(NecroType, necro_infer_go(infer, ast->while_loop.value_init));
    NecroFScope* f_scope         = necro_infer_f_scope_push(infer, ast->while_loop.value_apat->scope);
    NecroType*   value_apat_type  = necro_try_result(NecroType, necro_infer_pattern(infer, ast->while_loop.value_apat));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, value_init_type, value_apat_type, ast->scope, ast->source_loc, ast->end_loc));
    NecroType* while_type         = necro_try_result(NecroType, necro_infer_go(infer, ast->while_loop.while_expression));
    NecroType* bool_type          = necro_type_instantiate(infer->arena, &infer->con_env, infer->base, infer->base->true_con->type, ast->scope, ast->source_loc, ast->end_loc);
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, bool_type, while_type, ast->scope, ast->source_loc, ast->end_loc));
    NecroType* do_expression_type = necro_try_result(NecroType, necro_infer_go(infer, ast->while_loop.do_expression));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, value_apat_type, do_expression_type, ast->scope, ast->source_loc, ast->end_loc));
    ast->necro_type               = necro_type_find(do_expression_type);
    necro_try(NecroType, necro_infer_share_loop_free_vars(infer, f_scope->free_vars, ast->while_loop.value_apat->scope));
    necro_infer_f_scope_pop(infer);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// RightHandSide
//=====================================================
NecroResult(NecroType) necro_infer_right_hand_side(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    necro_try(NecroType, necro_infer_go(infer, ast->right_hand_side.declarations));
    ast->necro_type = necro_try_result(NecroType, necro_infer_go(infer, ast->right_hand_side.expression));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Let
//=====================================================
NecroResult(NecroType) necro_infer_let_expression(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LET_EXPRESSION);
    necro_try(NecroType, necro_infer_go(infer, ast->let_expression.declarations));
    ast->necro_type = necro_try_result(NecroType, necro_infer_go(infer, ast->let_expression.expression));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Arithmetic Sequence
//=====================================================
NecroResult(NecroType) necro_infer_arithmetic_sequence(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    assert(false && "Arithmetic Sequences currently not supported given the current list changes");
    return ok(NecroType, NULL);
    // NecroType* type = infer->base->int_type->type;
    // if (ast->arithmetic_sequence.from != NULL)
    // {
    //     NecroType* from_type = necro_try_result(NecroType, necro_infer_go(infer, ast->arithmetic_sequence.from));
    //     necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, type, from_type, ast->scope, ast->arithmetic_sequence.from->source_loc, ast->arithmetic_sequence.from->end_loc));
    // }
    // if (ast->arithmetic_sequence.then != NULL)
    // {
    //     NecroType* then_type = necro_try_result(NecroType, necro_infer_go(infer, ast->arithmetic_sequence.then));
    //     necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, type, then_type, ast->scope, ast->arithmetic_sequence.then->source_loc, ast->arithmetic_sequence.then->end_loc));
    // }
    // if (ast->arithmetic_sequence.to != NULL)
    // {
    //     NecroType* to_type = necro_try_result(NecroType, necro_infer_go(infer, ast->arithmetic_sequence.to));
    //     necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, type, to_type, ast->scope, ast->arithmetic_sequence.to->source_loc, ast->arithmetic_sequence.to->end_loc));
    // }
    // ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, type);
    // necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    // return ok(NecroType, ast->necro_type);
}

//=====================================================
// Do
//=====================================================
NecroResult(NecroType) necro_infer_do_statement(NecroInfer* infer, NecroAst* ast, NecroType* monad_var)
{
    assert(ast != NULL);
    switch(ast->type)
    {

    case NECRO_AST_LET_EXPRESSION:
        ast->necro_type = necro_try_result(NecroType, necro_infer_let_expression(infer, ast));
        return ok(NecroType, NULL);

    case NECRO_BIND_ASSIGNMENT:
    {
        NecroType* var_name                   = necro_type_fresh_var(infer->arena, ast->scope);
        var_name->kind                        = infer->base->star_kind->type;
        ast->bind_assignment.ast_symbol->type = var_name;
        NecroType* rhs_type                   = necro_try_result(NecroType, necro_infer_go(infer, ast->bind_assignment.expression));
        ast->necro_type                       = necro_type_app_create(infer->arena, monad_var, var_name);
        necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, rhs_type, ast->necro_type, ast->scope, ast->bind_assignment.expression->source_loc, ast->bind_assignment.expression->end_loc));
        return ok(NecroType, NULL);
    }

    case NECRO_PAT_BIND_ASSIGNMENT:
    {
        NecroType* pat_type = necro_try_result(NecroType, necro_infer_pattern(infer, ast->pat_bind_assignment.pat));
        NecroType* rhs_type = necro_try_result(NecroType, necro_infer_go(infer, ast->pat_bind_assignment.expression));
        ast->necro_type     = necro_type_app_create(infer->arena, monad_var, pat_type);
        necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, rhs_type, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        return ok(NecroType, NULL);
    }

    default:
    {
        NecroType* statement_type = necro_try_result(NecroType, necro_infer_go(infer, ast));
        ast->necro_type           = necro_type_app_create(infer->arena, monad_var, necro_type_fresh_var(infer->arena, NULL));
        necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, &infer->con_env, infer->base, statement_type, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        return ok(NecroType, ast->necro_type);
    }

    }
}

NecroResult(NecroType) necro_infer_do(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DO);
    // TODO: If we want to support do notation again we'll have to retrofit it with the new constraint system
    return ok(NecroType, NULL);
    // NecroType* monad_var      = necro_type_fresh_var(infer->arena, NULL);
    // monad_var->kind           = infer->base->monad_type_class->type_class->type_var->type->kind;
    // NecroAst*  statements     = ast->do_statement.statement_list;
    // NecroType* statement_type = NULL;
    // necro_apply_constraints(infer->arena, monad_var, necro_create_type_class_context(infer->arena, infer->base->monad_type_class->type_class, infer->base->monad_type_class, monad_var->var.var_symbol, NULL));
    // while (statements != NULL)
    // {
    //     statement_type = necro_try_result(NecroType, necro_infer_do_statement(infer, statements->list.item, monad_var));
    //     if (statements->list.next_item == NULL && statement_type == NULL)
    //         return necro_type_final_do_statement_error(NULL, monad_var, statements->list.item->source_loc, statements->list.item->end_loc);
    //     statements = statements->list.next_item;
    // }
    // ast->necro_type             = statement_type;
    // ast->do_statement.monad_var = monad_var;
    // necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, ast->scope, ast->simple_type.type_con->source_loc, ast->simple_type.type_con->end_loc));
    // return ok(NecroType, ast->necro_type);
}

//=====================================================
// Declaration groups
//=====================================================
NecroResult(NecroType) necro_infer_declaration(NecroInfer* infer, NecroAst* declaration_group)
{
    assert(declaration_group != NULL);
    assert(declaration_group->type == NECRO_AST_DECL);
    NecroAst*       ast  = NULL;
    NecroAstSymbol* data = NULL;

    //-----------------------------
    // Pass 1, new names
    NecroAst* curr = declaration_group;
    while (curr != NULL)
    {
        assert(curr->type == NECRO_AST_DECL);
        if (curr->declaration.type_checked) { curr = curr->declaration.next_declaration; continue; }
        ast = curr->declaration.declaration_impl;
        switch(ast->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            data = ast->simple_assignment.ast_symbol;
            if (data->type == NULL)
            {
                NecroType* new_name = necro_type_fresh_var(infer->arena, ast->scope);
                new_name->kind      = infer->base->star_kind->type;
                data->type          = new_name;
            }
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            if (ast->apats_assignment.is_recursive)
                return necro_type_recursive_function_error(ast->apats_assignment.ast_symbol, NULL, ast->source_loc, ast->end_loc);
            data = ast->apats_assignment.ast_symbol;
            if (data->type == NULL)
            {
                NecroType* new_name = necro_type_fresh_var(infer->arena, ast->scope);
                new_name->kind      = infer->base->star_kind->type;
                data->type          = new_name;
            }
            break;
        case NECRO_AST_PAT_ASSIGNMENT:
            necro_pat_new_name_go(infer, ast->pat_assignment.pat);
            break;
        case NECRO_AST_DATA_DECLARATION:
            if (ast->data_declaration.is_recursive)
                return necro_type_recursive_data_type_error(ast->data_declaration.ast_symbol, NULL, ast->data_declaration.simpletype->source_loc, ast->data_declaration.simpletype->end_loc);
            necro_try(NecroType, necro_infer_simple_type(infer, ast->data_declaration.simpletype));
            break;
        case NECRO_AST_TYPE_SIGNATURE:
            necro_try(NecroType, necro_infer_type_sig(infer, ast));
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            necro_try(NecroType, necro_create_type_class(infer, ast));
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            necro_try(NecroType, necro_create_type_class_instance(infer, ast));
            break;
        default:
            necro_unreachable(NecroType);
        }
        curr = curr->declaration.next_declaration;
    }

    //-----------------------------
    // Pass 2, infer types
    curr = declaration_group;
    while (curr != NULL)
    {
        if (curr->declaration.type_checked) { curr = curr->declaration.next_declaration; continue; }
        ast = curr->declaration.declaration_impl;
        switch(ast->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            necro_try(NecroType, necro_infer_simple_assignment(infer, ast));
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            necro_try(NecroType, necro_infer_apats_assignment(infer, ast));
            break;
        case NECRO_AST_PAT_ASSIGNMENT:
            necro_try(NecroType, necro_infer_pat_assignment(infer, ast));
            break;
        case NECRO_AST_DATA_DECLARATION:
        {
            NecroAst*  constructor_list = ast->data_declaration.constructor_list;
            size_t     con_num          = 0;
            while (constructor_list != NULL)
            {
                necro_try(NecroType, necro_create_data_constructor(infer, constructor_list->list.item, ast->data_declaration.simpletype->necro_type, con_num));
                constructor_list = constructor_list->list.next_item;
                con_num++;
            }
            ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol->con_num = con_num;
            necro_infer_set_is_wrapper_type(ast);
            break;
        }
        case NECRO_AST_TYPE_SIGNATURE:
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            break;
        default:
            necro_unreachable(NecroType);
        }
        curr = curr->declaration.next_declaration;
    }

    //-----------------------------
    // Pass 3, generalize
    curr = declaration_group;
    while (curr != NULL)
    {
        ast = curr->declaration.declaration_impl;
        if (ast->scope->parent == NULL) // Only simplify at top level
        {
            necro_try_map(void, NecroType, necro_constraint_simplify(infer->arena, &infer->con_env, infer->base, infer->intern));
        }
        if (curr->declaration.type_checked) { curr = curr->declaration.next_declaration; continue; }
        switch(ast->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            data = ast->simple_assignment.ast_symbol;
            if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE) { data->type_status = NECRO_TYPE_DONE; curr->declaration.type_checked = true; continue; }
            if (ast->scope->parent == NULL) // Only generalize top level
                data->type = necro_try_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, data->type, ast->scope->parent));
            necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, data->type, NULL, ast->source_loc, ast->end_loc));
            data->type_status = NECRO_TYPE_DONE;
            break;
        }
        case NECRO_AST_APATS_ASSIGNMENT:
        {
            data = ast->apats_assignment.ast_symbol;
            if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE) { data->type_status = NECRO_TYPE_DONE; curr->declaration.type_checked = true; continue; }
            if (ast->scope->parent == NULL) // Only generalize top level
                data->type = necro_try_result(NecroType, necro_type_generalize(infer->arena, &infer->con_env, infer->base, infer->intern, data->type, ast->scope->parent));
            necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, data->type, NULL, ast->source_loc, ast->end_loc));
            data->type_status = NECRO_TYPE_DONE;
            break;
        }
        case NECRO_AST_PAT_ASSIGNMENT:
        {
            necro_try(NecroType, necro_gen_pat_go(infer, ast->pat_assignment.pat));
            necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->necro_type, NULL, ast->source_loc, ast->end_loc));
            break;
        }
        case NECRO_AST_DATA_DECLARATION:
        {
            necro_try_map(void, NecroType, necro_kind_infer_default_unify_with_star(infer->arena, infer->base, ast->data_declaration.simpletype->necro_type, NULL, ast->data_declaration.simpletype->simple_type.type_con->source_loc, ast->data_declaration.simpletype->simple_type.type_con->end_loc));
            necro_kind_default_type_kinds(infer->arena, infer->base, ast->data_declaration.simpletype->necro_type);
            break;
        }
        case NECRO_AST_TYPE_SIGNATURE:
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            break;
        default:
            necro_unreachable(NecroType);
        }
        curr->declaration.type_checked = true;
        curr                           = curr->declaration.next_declaration;
    }

    return ok(NecroType, NULL);
}

//=====================================================
// Declaration Group List
//=====================================================
NecroResult(NecroType) necro_infer_declaration_group_list(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DECLARATION_GROUP_LIST);
    NecroAst* groups = ast;
    while (groups != NULL)
    {
        if (groups->declaration_group_list.declaration_group != NULL)
        {
            necro_try(NecroType, necro_infer_declaration(infer, groups->declaration_group_list.declaration_group));
        }
        groups = groups->declaration_group_list.next;
    }
    return ok(NecroType, NULL);
}

//=====================================================
// Infer Go
//=====================================================
NecroResult(NecroType) necro_infer_go(NecroInfer* infer, NecroAst* ast)
{
    if (ast == NULL)
        return ok(NecroType, NULL);
    switch (ast->type)
    {

    case NECRO_AST_CONSTANT:               return ok(NecroType, necro_infer_constant(infer, ast));
    case NECRO_AST_WILDCARD:               return ok(NecroType, necro_infer_wildcard(infer, ast));
    case NECRO_AST_VARIABLE:               return necro_infer_var(infer, ast);
    case NECRO_AST_CONID:                  return necro_infer_conid(infer, ast);
    case NECRO_AST_FUNCTION_EXPRESSION:    return necro_infer_fexpr(infer, ast);
    case NECRO_AST_BIN_OP:                 return necro_infer_bin_op(infer, ast);
    case NECRO_AST_OP_LEFT_SECTION:        return necro_infer_op_left_section(infer, ast);
    case NECRO_AST_OP_RIGHT_SECTION:       return necro_infer_op_right_section(infer, ast);
    case NECRO_AST_IF_THEN_ELSE:           return necro_infer_if_then_else(infer, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:        return necro_infer_right_hand_side(infer, ast);
    case NECRO_AST_LAMBDA:                 return necro_infer_lambda(infer, ast);
    case NECRO_AST_LET_EXPRESSION:         return necro_infer_let_expression(infer, ast);
    case NECRO_AST_DECL:                   return necro_infer_declaration(infer, ast);
    case NECRO_AST_DECLARATION_GROUP_LIST: return necro_infer_declaration_group_list(infer, ast);
    case NECRO_AST_TUPLE:                  return necro_infer_tuple(infer, ast);
    case NECRO_AST_EXPRESSION_LIST:        return necro_infer_expression_list(infer, ast);
    case NECRO_AST_EXPRESSION_ARRAY:       return necro_infer_expression_array(infer, ast);
    case NECRO_AST_SEQ_EXPRESSION:         return necro_infer_seq_expression(infer, ast);
    case NECRO_AST_CASE:                   return necro_infer_case(infer, ast);
    case NECRO_AST_FOR_LOOP:               return necro_infer_for_loop(infer, ast);
    case NECRO_AST_WHILE_LOOP:             return necro_infer_while_loop(infer, ast);
    case NECRO_AST_ARITHMETIC_SEQUENCE:    return necro_infer_arithmetic_sequence(infer, ast);
    case NECRO_AST_DO:                     return necro_infer_do(infer, ast);
    case NECRO_AST_TYPE_SIGNATURE:         return necro_infer_type_sig(infer, ast);

    case NECRO_AST_TOP_DECL:               /* FALLTHROUGH */
    case NECRO_AST_SIMPLE_ASSIGNMENT:      /* FALLTHROUGH */
    case NECRO_AST_APATS_ASSIGNMENT:       /* FALLTHROUGH */
    case NECRO_AST_PAT_ASSIGNMENT:         /* FALLTHROUGH */
    case NECRO_AST_TYPE_CLASS_DECLARATION: /* FALLTHROUGH */
    case NECRO_AST_TYPE_CLASS_INSTANCE:    /* FALLTHROUGH */
    case NECRO_AST_DATA_DECLARATION:       /* FALLTHROUGH */
    default:                               necro_unreachable(NecroType);
    }
}

NecroResult(void) necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroInfer             infer  = necro_infer_create(&ast_arena->arena, intern, scoped_symtable, base, ast_arena);
    NecroResult(NecroType) result = necro_infer_go(&infer, ast_arena->root);
    if (result.type == NECRO_RESULT_OK)
    {
        NecroResult(void) result2 = necro_constraint_simplify(infer.arena, &infer.con_env, base, intern);
        if (result2.type == NECRO_RESULT_ERROR)
        {
            necro_infer_destroy(&infer);
            return result2;
        }
        necro_infer_destroy(&infer);
        if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_INFER && info.verbosity > 0))
            necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    necro_infer_destroy(&infer);
    return necro_error_map(NecroType, void, result);
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////

#define INFER_TEST_VERBOSE 0

void necro_infer_test_impl(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type, NecroAstArena* ast2)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &base, &ast);
    necro_alias_analysis(info, &ast); // NOTE: Consider merging alias_analysis into RENAME_VAR phase?
    NecroResult(void) result = necro_infer(info, &intern, &scoped_symtable, &base, &ast);

    // Assert
    if (result.type != expected_result)
    {
        necro_ast_arena_print(&ast);
        necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    }
    else if (ast2)
    {
        assert(expected_result == NECRO_RESULT_OK);
        assert(error_type == NULL);
// #if INFER_TEST_VERBOSE
//         puts("//////////////////////////////");
//         puts("// necro_infer_test ast");
//         puts("//////////////////////////////");
//         necro_ast_arena_print(&ast);
//         puts("//////////////////////////////");
//         puts("// necro_infer_test ast2");
//         puts("//////////////////////////////");
//         necro_ast_arena_print(ast2);
//         puts("//////////////////////////////");
// #endif
        necro_ast_assert_eq(ast.root, ast2->root);
    }

    assert(result.type == expected_result);
    bool passed = result.type == expected_result;
    if (expected_result == NECRO_RESULT_ERROR)
    {
        assert(error_type != NULL);
        if (result.error != NULL && error_type != NULL)
        {
            assert(result.error->type == *error_type);
            passed &= result.error->type == *error_type;
        }
        else
        {
            passed = false;
        }
    }

    const char* result_string = passed ? "Passed" : "Failed";
    printf("Infer %s test: %s\n", test_name, result_string);
    fflush(stdout);

    // Clean up
#if INFER_TEST_VERBOSE
    if (result.type == NECRO_RESULT_ERROR)
        necro_result_error_print(result.error, str, "Test");
    else if (result.error)
        necro_result_error_destroy(result.type, result.error);
#else
    necro_result_error_destroy(result.type, result.error);
#endif

    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}

void necro_infer_test_result(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
{
    NecroAstArena* no_ast_comparison = NULL;
    necro_infer_test_impl(test_name, str, expected_result, error_type, no_ast_comparison);
}

void necro_infer_test_comparison(const char* test_name, const char* str, NecroIntern* intern, NecroAstArena* ast2)
{
    const NECRO_RESULT_TYPE ok_result_expected = NECRO_RESULT_OK;
    const NECRO_RESULT_ERROR_TYPE* no_error_expected = NULL;
    necro_infer_test_impl(test_name, str, ok_result_expected, no_error_expected, ast2);
    necro_intern_destroy(intern);
    necro_ast_arena_destroy(ast2);
}

void necro_test_infer()
{
    necro_announce_phase("NecroInfer");
    fflush(stdout);

    ///////////////////////////////////////////////////////
    // Error tests
    ///////////////////////////////////////////////////////

    {
        const char* test_name   = "Constraints Test";
        const char* test_source = ""
            "xPlusx x = x + x where x' = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Constraints Test";
        const char* test_source = ""
            "xPlusx x = let x' = x in x + x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Constraints Test";
        const char* test_source = ""
            "xPlusx x = x + x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Array is Not Ok 1";
        const char* test_source = ""
            "dropOne :: Array n a -> Array n a -> Array n a\n"
            "dropOne x _ = x\n"
            "arr1 = { 7, 6, 5, 4, 3, 2, 1, 0 }\n"
            "arr2 = { 3, 2, 1, 0 }\n"
            "one' = dropOne arr1 arr2\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // // TODO (Curtis, 2-8-19): Do statements rely heavily on type class machinery behaving. Revisit once it's sorted.
    // {
    //     const char* test_name = "MistmatchedType: do1";
    //     const char* test_source = ""
    //         "wrongnad :: Monad m => m Bool\n"
    //         "wrongnad = do\n"
    //         "  x <- pure ()\n"
    //         "  pure x\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    // {
    //     const char* test_name = "MistmatchedType: do2";
    //     const char* test_source = ""
    //         "wrongnad2 :: Monad m => m Bool\n"
    //         "wrongnad2 = do\n"
    //         "  pure ()\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    {
        const char* test_name = "Rigid Type Variable 3";
        const char* test_source = ""
            "notSoftMachine :: Maybe a\n"
            "notSoftMachine = n where\n"
            "  n :: Maybe b\n"
            "  n = Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }


    {
        const char* test_name = "Rigid Type Variable 1";
        const char* test_source = ""
            "notSoftMachine :: a\n"
            "notSoftMachine = ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_RIGID_TYPE_VARIABLE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "Rigid Type Variable 2";
        const char* test_source = ""
            "notSoftMachine :: () -> Maybe a\n"
            "notSoftMachine x = Just x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_RIGID_TYPE_VARIABLE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: SimpleAssignment";
        const char* test_source = ""
            "data Book = Pages\n"
            "data NotBook = EmptyPages\n"
            "notcronomicon :: Maybe Book\n"
            "notcronomicon = Just EmptyPages\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char*                   test_name           = "Uninitialized Recursive Value";
        const char*                   test_source         = "impossible = impossible\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "MistmatchedType: Initializer";
        const char* test_source = "looper ~ () = looper && True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: ApatsAssignment";
        const char* test_source = ""
            "zeroIsNotNothing :: a -> Int\n"
            "zeroIsNotNothing z = Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: PatAssignment";
        const char* test_source = ""
            "earth :: Bool\n"
            "earth = toTheMoon where\n"
            "  toTheMoon :: Bool\n"
            "  (toTheSun, toTheMoon) = ((), Just True)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Array";
        const char* test_source = ""
            "notLikeTheOthers :: Array 3 Bool\n"
            "notLikeTheOthers = { True, False, () }\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Array 2";
        const char* test_source = ""
            "neverEnough :: Array 4 Int\n"
            "neverEnough = { 0, 1, 2 }\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: List";
        const char* test_source = ""
            // "notLikeTheOthers :: [()]\n"
            "notLikeTheOthers :: Array 3 ()\n"
            "notLikeTheOthers = { (), False, () }\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: fexpr";
        const char* test_source = ""
            "unitsForDays :: () -> ()\n"
            "unitsForDays x = x\n"
            "notAUnit :: Bool\n"
            "notAUnit = unitsForDays True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: fexpr2";
        const char* test_source = ""
            "unitsForDays :: () -> Maybe Bool -> ()\n"
            "unitsForDays x _ = x\n"
            "notAUnit :: Bool\n"
            "notAUnit = unitsForDays () True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: fexpr3";
        const char* test_source = ""
            "notAFunction :: ()\n"
            "notAFunction = ()\n"
            "unity :: ()\n"
            "unity = notAFunction () 0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: if1";
        const char* test_source = ""
            "logicDied :: Maybe ()\n"
            "logicDied = if () then Just () else Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: if2";
        const char* test_source = ""
            "logicDiedAgain :: Maybe ()\n"
            "logicDiedAgain = if False then Just True else Just ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: BinOp";
        const char* test_source = ""
            "andIsPretend :: Bool\n"
            "andIsPretend = True && ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: LeftSection";
        const char* test_source = ""
            "chopItOff :: Bool -> Bool\n"
            "chopItOff = (() ||)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: RightSection";
        const char* test_source = ""
            "noTheOtherOne :: Bool -> Bool\n"
            "noTheOtherOne = (|| ())\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats1";
        const char* test_source = ""
            "atTheApatsOfMadness :: Bool -> Bool\n"
            "atTheApatsOfMadness () = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats2";
        const char* test_source = ""
            "theCallOfWrongthulhu :: Int -> Int -> Bool\n"
            "theCallOfWrongthulhu x () = False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats3";
        const char* test_source = ""
            "data OnlyLogic = OnlyLogic Bool\n"
            "cantDoAnythingRight :: OnlyLogic -> Bool\n"
            "cantDoAnythingRight OnlyLogic = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats4";
        const char* test_source = ""
            "data DeathMayDie = DeathMayDie Bool ()\n"
            "eternalLies :: DeathMayDie -> Bool\n"
            "eternalLies (DeathMayDie x) = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats5";
        const char* test_source = ""
            "wrongzathoth :: Int -> Int -> Bool\n"
            "wrongzathoth x = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats6";
        const char* test_source = ""
            "wrongzathoth2 :: Int -> Bool\n"
            "wrongzathoth2 x y z = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Case1";
        const char* test_source = ""
            "notAGoodCase :: Bool\n"
            "notAGoodCase = case True of\n"
            "  True  -> ()\n"
            "  False -> True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Case2";
        const char* test_source = ""
            "fubar :: Bool\n"
            "fubar = case True of\n"
            "  ()    -> False\n"
            "  False -> True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Case3";
        const char* test_source = ""
            "rofl :: Bool\n"
            "rofl = case True of\n"
            "  True  -> False\n"
            "  False -> ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // {
    //     const char* test_name = "MistmatchedType: ArithmeticSequence1";
    //     const char* test_source = ""
    //         "sequencer1 :: [Int]\n"
    //         "sequencer1 = [True..10]\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    // TODO (Curtis, 2-8-19): Something wonky with this. However arithmetic sequences are a "nice to have"...
    // {
    //     const char* test_name = "MistmatchedType: ArithmeticSequence3";
    //     const char* test_source = ""
    //         "sequencer3 :: [Int]\n"
    //         "sequencer3 = [1, True..10]\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    // {
    //     const char* test_name = "MistmatchedType: ArithmeticSequence2";
    //     const char* test_source = ""
    //         "sequencer2 :: [Int]\n"
    //         "sequencer2 = [1..()]\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    {
        const char* test_name = "Occurs1";
        const char* test_source = ""
            "caughtInATimeLoop ~ Nothing = Just caughtInATimeLoop\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_OCCURS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // {
    //     const char* test_name = "Occurs1";
    //     const char* test_source = ""
    //         "finalCountdown = do\n"
    //         "    x <- pure ()\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_FINAL_DO_STATEMENT;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    {
        const char* test_name = "MismatchedKinds: TypeSig";
        const char* test_source = ""
            "thankYouKindly :: Maybe\n"
            "thankYouKindly = Just ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MismatchedKinds: Context";
        const char* test_source = ""
            "noThankYouKindly :: Monad m => m () -> m () ()\n"
            "noThankYouKindly = pure ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MismatchedKinds: TypeSig2";
        const char* test_source = ""
            "thankYouKindly1 :: () -> Maybe Maybe\n"
            "thankYouKindly1 x = Just ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MismatchedKinds: Instance";
        const char* test_source = ""
            "instance Monad () where\n"
            "  bind g f = ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "Not A Type Class 1";
        const char* test_source = ""
            "maybeNever :: Maybe a => a\n"
            "maybeNever = Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_A_CLASS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "Not A Type Class 2";
        const char* test_source = ""
            "noClass :: (Monad m, Int m) => m ()\n"
            "noClass = Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_A_CLASS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "Not A Type Class 3";
        const char* test_source = ""
            "instance World Int where\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_A_CLASS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "Not A Visible Method";
        const char* test_source = ""
            "data Invisible = Invisible\n"
            "instance Semiring Invisible where\n"
            "  nothingToSeeHere x y = Invisible\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_A_VISIBLE_METHOD;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Rigid Type Variable: Instance Method";
        const char* test_source = ""
            "class OgreMagi a where\n"
            "  twoHeads :: OgreMagi b => b -> (a, b)\n"
            "instance OgreMagi (Maybe a) where\n"
            "  twoHeads x = (Just x, x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_RIGID_TYPE_VARIABLE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Rigid Type Variable: Instance Method";
        const char* test_source = ""
            "class OgreMagi a where\n"
            "  twoHeads :: OgreMagi b => b -> (a, b)\n"
            "instance OgreMagi c => OgreMagi (Maybe c) where\n"
            "  twoHeads x = (Just x, x)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_RIGID_TYPE_VARIABLE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "RigidTypeVariable, Fixed Crash";
        const char* test_source = ""
            "data Book = Pages\n"
            "class Fail a where result :: a -> a\n"
            "x :: Fail a => a\n"
            "x = Pages\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error = NECRO_TYPE_RIGID_TYPE_VARIABLE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "No Explicit Implementation";
        const char* test_source = ""
            "data HalfMeasures = HalfMeasures\n"
            "instance Semiring HalfMeasures where\n"
            "  add x y = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NO_EXPLICIT_IMPLEMENTATION;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Ambiguous Type Class 1";
        const char* test_source = ""
            "class Ambiguity a where\n"
            "  amb :: Int -> Int\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_CLASS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Ambiguous Type Class 2";
        const char* test_source = ""
            "class Bad b where\n"
            "badzathoth :: Bad z => Int -> Int\n"
            "badzathoth x = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_CLASS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Constrains Only Class Var";
        const char* test_source = ""
            "class Prison b where\n"
            "  cell :: Prison b => b -> b\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_CONSTRAINS_ONLY_CLASS_VAR;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // TODO (Curtis 2-12-19): Error highlighting is off on this. Putting the error arrows in the wrong place.
    {
        const char* test_name   = "Not An Instance 1";
        const char* test_source = ""
            "data Illogical = Illogical Bool\n"
            "logical :: Illogical\n"
            "logical = Illogical True + Illogical False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_AN_INSTANCE_OF;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Not An Instance 2";
        const char* test_source = ""
            "data Illogical a = Illogical a\n"
            "logical :: Num a => a -> Illogical a\n"
            "logical x = Illogical x + Illogical x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_AN_INSTANCE_OF;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Not An Instance 3";
        const char* test_source = ""
            "rigidNotAnInstanceOfError :: a -> a\n"
            "rigidNotAnInstanceOfError x = x * x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NOT_AN_INSTANCE_OF;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Does not implement Super Class";
        const char* test_source = ""
            "data Imaginary = Imaginary\n"
            "instance DivisionRing Imaginary where\n"
            "  recip _ = Imaginary\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_DOES_NOT_IMPLEMENT_SUPER_CLASS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Multiple Instance Declarations";
        const char* test_source = ""
            "instance Semiring Int where\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MULTIPLE_INSTANCE_DECLARATIONS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Kind is not a type";
        const char* test_source = ""
            "notKind :: Type -> Int\n"
            "notKind _ = 0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Kind Signature is Ok 1";
        const char* test_source = ""
            "data Phantom (a :: Nat) = Phantom\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Kind Signature is Ok 2";
        const char* test_source = ""
            "data Phantom (a :: Type -> Type) = Phantom\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Wrong Kind Signature 1";
        const char* test_source = ""
            "data Phantom (a :: Bool) = Phantom\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Wrong Kind Signature 2";
        const char* test_source = ""
            "data Phantom (a :: Maybe) = Phantom\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Kinds 1";
        const char* test_source = ""
            "data Phantom (a :: Nat) = Phantom\n"
            "solitaryPhantom :: Phantom ()\n"
            "solitaryPhantom = Phantom\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Kinds 2";
        const char* test_source = ""
            "data Apply (a :: Type) b = Apply (a b)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Kinds 3";
        const char* test_source = ""
            "data Apply (a :: Type -> Type) = Apply a\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Higher Kinds Ok 1";
        const char* test_source = ""
            "data Apply (a :: Type -> Type) b = Apply (a b)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Higher Kinds Ok 2";
        const char* test_source = ""
            "data DontApply (a :: Type -> Type) b = DontApply b\n"
            "dontApply :: DontApply Maybe Int\n"
            "dontApply = DontApply 22\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Mismatched Kinds 4";
        const char* test_source = ""
            "data DontApply (a :: Type -> Type) b = DontApply b\n"
            "dontApply :: DontApply () Int\n"
            "dontApply = DontApply 22\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Higher Kinds Ok 3";
        const char* test_source = ""
            "data DoubleApply m a b = DoubleApply (m a) (m b)\n"
            "doubleApplyMaybe :: DoubleApply Maybe () Bool\n"
            "doubleApplyMaybe = DoubleApply Nothing (Just False)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Mismatched Kinds 5";
        const char* test_source = ""
            "data WrongApply m a b = WrongApply (m a) (m a b)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Higher Kinds Ok 4";
        const char* test_source = ""
            "data ArrayN (n :: Nat) a = ArrayN a\n"
            "data UseArray (a :: Nat -> Type -> Type) b = UseArray b\n"
            "useArray :: UseArray ArrayN Int\n"
            "useArray = UseArray 22\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Mismatched Kinds 6";
        const char* test_source = ""
            "data ArrayN (n :: Nat) a = ArrayN a\n"
            "data UseArray (a :: Nat -> Type -> Type) b = UseArray b\n"
            "useArray :: UseArray Maybe Int\n"
            "useArray = UseArray 22\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Mismatched Kinds 7";
        const char* test_source = ""
            "class LitNum n where\n"
            "  intArr :: Array n Int\n"
            "instance LitNum Int where\n"
            "  intArr = { 0 }\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // Currently parsing and type class implementation doesn't support this...do we want to?
    // {
    //     const char* test_name   = "Nat Class";
    //     const char* test_source = ""
    //         "class LitNum n where\n"
    //         "  intArr :: Array n Int\n"
    //         "instance LitNum 1 where\n"
    //         "  intArr = { 0 }\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // TODO: Better type signature inference. Use what we have, kind signatures in Constructor AstSymbols to enforce more accurate and local kinds inference. Treat with the same care we treat type inference with!
    // TODO: Test kind inference with kind signatures
    // TODO: Test infer_generalize with different kinds
    // TODO: Create type literals in parsing, etc
    // TODO: Bang this around a lot.
    {
        const char* test_name   = "Nat Kind 1";
        const char* test_source = ""
            "data ArrayN (n :: Nat) a = ArrayN a\n"
            "arrayN :: ArrayN 8 Int\n"
            "arrayN = ArrayN 22\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Array is Ok 1";
        const char* test_source = ""
            "gimme :: Array 8 Float\n"
            "gimme = { 7, 6, 5, 4, 3, 2, 1, 0 }\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Array is Ok 2";
        const char* test_source = ""
            "dropOne :: Array n a -> Array n a -> Array n a\n"
            "dropOne x _ = x\n"
            "arr1 = { 0, 1, 2, 3 }\n"
            "arr2 = { 3, 2, 1, 0 }\n"
            "one' = dropOne arr1 arr2\n"
            "arr3 = { 0, 1, 2, 3, 4 }\n"
            "arr4 = { 4, 3, 2, 1, 0 }\n"
            "three = dropOne arr3 arr4\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Array is Not Ok 2";
        const char* test_source = ""
            "dropOne :: a -> a -> a\n"
            "dropOne x _ = x\n"
            "arr1 = { 7, 6, 5, 4, 3, 2, 1, 0 }\n"
            "arr2 = { 3, 2, 1, 0 }\n"
            "one' = dropOne arr1 arr2\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // NOTE: Recursive functions are now a type error
    {
        const char* test_name   = "Mutual Recursion";
        const char* test_source = ""
            "mutantRec1 :: Int -> Int\n"
            "mutantRec1 x = mutantRec2 x\n"
            "mutantRec2 :: Int -> Int\n"
            "mutantRec2 x = mutantRec1 x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_RECURSIVE_FUNCTION;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // NOTE: Recursive data types are now an error
    {
        const char* test_name   = "Recursive Data Types";
        const char* test_source = ""
            "data RecList a = RecList a (RecList a) | RecNil";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_RECURSIVE_DATA_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "TypesPlease";
        const char* test_source = ""
            "data TypesOnly (n :: Nat) = TypesOnly n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_KIND_MISMATCHED_KIND;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Dont Generalize Let";
        const char* test_source = ""
            "data Alive = Alive\n"
            "data Dead  = Dead\n"
            "cat :: (Maybe Alive, Maybe Dead)\n"
            "cat = (alive, dead) where\n"
            "  schroedingersMaybe = Nothing\n"
            "  alive              = schroedingersMaybe\n"
            "  dead               = schroedingersMaybe\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }


    ///////////////////////////////////////////////////////
    // OK tests
    ///////////////////////////////////////////////////////

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        NecroAst* book = necro_ast_create_data_declaration_with_ast_symbol(
            &ast.arena,
            necro_ast_symbol_create(&ast.arena,
                necro_intern_string(&intern, "Test.Book"),
                necro_intern_string(&intern, "Book"),
                necro_intern_string(&intern, "Test"),
                NULL
            ),
            necro_ast_create_simple_type_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena,
                    necro_intern_string(&intern, "Test.Book"),
                    necro_intern_string(&intern, "Book"),
                    necro_intern_string(&intern, "Test"),
                    NULL
                ),
                NULL
            ),
            necro_ast_create_list(
                &ast.arena,
                necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena,
                        necro_intern_string(&intern, "Test.Pages"),
                        necro_intern_string(&intern, "Pages"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    NULL
                ),
                NULL
            )
        );

        NecroAst* not_book = necro_ast_create_data_declaration_with_ast_symbol(
            &ast.arena,
            necro_ast_symbol_create(&ast.arena,
                necro_intern_string(&intern, "Test.NotBook"),
                necro_intern_string(&intern, "NotBook"),
                necro_intern_string(&intern, "Test"),
                NULL
            ),
            necro_ast_create_simple_type_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena,
                    necro_intern_string(&intern, "Test.NotBook"),
                    necro_intern_string(&intern, "NotBook"),
                    necro_intern_string(&intern, "Test"),
                    NULL
                ),
                NULL
            ),
            necro_ast_create_list(
                &ast.arena,
                necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena,
                        necro_intern_string(&intern, "Test.EmptyPages"),
                        necro_intern_string(&intern, "EmptyPages"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    NULL
                ),
                NULL
            )
        );

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    book,
                    NULL
                ),
                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                    necro_ast_create_decl(&ast.arena,
                        not_book,
                        NULL
                    ),
                    necro_ast_create_declaration_group_list_with_next(&ast.arena,
                        necro_ast_create_decl(&ast.arena,
                            necro_ast_create_type_signature(&ast.arena,
                                NECRO_SIG_DECLARATION,
                                necro_ast_create_var_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(
                                        &ast.arena,
                                        necro_intern_string(&intern, "Test.notcronomicon"),
                                        necro_intern_string(&intern, "notcronomicon"),
                                        necro_intern_string(&intern, "Test"),
                                        NULL
                                    ),
                                    NECRO_VAR_SIG
                                ),
                                NULL,
                                necro_ast_create_type_app(
                                    &ast.arena,
                                    necro_ast_create_conid_with_ast_symbol(
                                        &ast.arena,
                                        necro_ast_symbol_create(
                                            &ast.arena,
                                            necro_intern_string(&intern, "Necro.Base.Maybe"),
                                            necro_intern_string(&intern, "Maybe"),
                                            necro_intern_string(&intern, "Necro.Base"),
                                            NULL
                                        ),
                                        NECRO_CON_TYPE_VAR
                                    ),
                                    necro_ast_create_conid_with_ast_symbol(
                                        &ast.arena,
                                        necro_ast_symbol_create(
                                            &ast.arena,
                                            necro_intern_string(&intern, "Test.Book"),
                                            necro_intern_string(&intern, "Book"),
                                            necro_intern_string(&intern, "Test"),
                                            NULL
                                        ),
                                        NECRO_CON_TYPE_VAR
                                    )
                                )
                            ),
                            NULL
                        ),
                        necro_ast_create_declaration_group_list_with_next(&ast.arena,
                            necro_ast_create_decl(&ast.arena,
                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(
                                        &ast.arena,
                                        necro_intern_string(&intern, "Test.notcronomicon"),
                                        necro_intern_string(&intern, "notcronomicon"),
                                        necro_intern_string(&intern, "Test"),
                                        NULL
                                    ),
                                    NULL,
                                    necro_ast_create_rhs(
                                        &ast.arena,
                                        necro_ast_create_fexpr(
                                            &ast.arena,
                                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                                necro_ast_symbol_create(&ast.arena,
                                                    necro_intern_string(&intern, "Necro.Base.Just"),
                                                    necro_intern_string(&intern, "Just"),
                                                    necro_intern_string(&intern, "Necro.Base"),
                                                    NULL
                                                ),
                                                NECRO_CON_VAR
                                            ),
                                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                                necro_ast_symbol_create(&ast.arena,
                                                    necro_intern_string(&intern, "Test.Pages"),
                                                    necro_intern_string(&intern, "Pages"),
                                                    necro_intern_string(&intern, "Test"),
                                                    NULL
                                                ),
                                                NECRO_CON_VAR
                                            )
                                        ),
                                        NULL
                                    )
                                ),
                                NULL
                            ),
                            NULL
                        )
                    )
                )
            );

        const char* test_name = "Ok: SimpleAssignment";
        const char* test_source = ""
            "data Book = Pages\n"
            "data NotBook = EmptyPages\n"
            "notcronomicon :: Maybe Book\n"
            "notcronomicon = Just Pages\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(
                            &ast.arena,
                            necro_intern_string(&intern, "Test.notcronomicon"),
                            necro_intern_string(&intern, "notcronomicon"),
                            necro_intern_string(&intern, "Test"),
                            NULL
                        ),
                        NULL,
                        necro_ast_create_rhs(
                            &ast.arena,
                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Necro.Base.Nothing"),
                                    necro_intern_string(&intern, "Nothing"),
                                    necro_intern_string(&intern, "Necro.Base"),
                                    NULL
                                ),
                                NECRO_CON_VAR
                            ),
                            NULL
                        )
                    ),
                    NULL
                ),
                NULL
            );

        const char* test_name = "NothingType";
        const char* test_source = ""
            "notcronomicon = Nothing\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        NecroAst* book = necro_ast_create_data_declaration_with_ast_symbol(
            &ast.arena,
            necro_ast_symbol_create(&ast.arena,
                necro_intern_string(&intern, "Test.Book"),
                necro_intern_string(&intern, "Book"),
                necro_intern_string(&intern, "Test"),
                NULL
            ),
            necro_ast_create_simple_type_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena,
                    necro_intern_string(&intern, "Test.Book"),
                    necro_intern_string(&intern, "Book"),
                    necro_intern_string(&intern, "Test"),
                    NULL
                ),
                NULL
            ),
            necro_ast_create_list(
                &ast.arena,
                necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena,
                        necro_intern_string(&intern, "Test.Pages"),
                        necro_intern_string(&intern, "Pages"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    NULL
                ),
                NULL
            )
        );

        NecroAst* not_book = necro_ast_create_data_declaration_with_ast_symbol(
            &ast.arena,
            necro_ast_symbol_create(&ast.arena,
                necro_intern_string(&intern, "Test.NotBook"),
                necro_intern_string(&intern, "NotBook"),
                necro_intern_string(&intern, "Test"),
                NULL
            ),
            necro_ast_create_simple_type_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena,
                    necro_intern_string(&intern, "Test.NotBook"),
                    necro_intern_string(&intern, "NotBook"),
                    necro_intern_string(&intern, "Test"),
                    NULL
                ),
                NULL
            ),
            necro_ast_create_list(
                &ast.arena,
                necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena,
                        necro_intern_string(&intern, "Test.EmptyPages"),
                        necro_intern_string(&intern, "EmptyPages"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    NULL
                ),
                NULL
            )
        );

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    book,
                    NULL
                ),
                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                    necro_ast_create_decl(&ast.arena,
                        not_book,
                        NULL
                    ),
                    NULL
                )
            );

        const char* test_name = "DataDeclarations";
        const char* test_source = ""
            "data Book = Pages\n"
            "data NotBook = EmptyPages\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(
                            &ast.arena,
                            necro_intern_string(&intern, "Test.atTheMountainsOfOK"),
                            necro_intern_string(&intern, "atTheMountainsOfOK"),
                            necro_intern_string(&intern, "Test"),
                            NULL
                        ),
                        NULL,
                        necro_ast_create_rhs(
                            &ast.arena,
                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Necro.Base.False"),
                                    necro_intern_string(&intern, "False"),
                                    necro_intern_string(&intern, "Necro.Base"),
                                    NULL
                                ),
                                NECRO_CON_VAR
                            ),
                            NULL
                        )
                    ),
                    NULL
                ),
                NULL
            );

        const char* test_name = "GeneralizationIsOk1";
        const char* test_source = ""
            "atTheMountainsOfOK = False\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(
                            &ast.arena,
                            necro_intern_string(&intern, "Test.atTheMountainsOfOK2"),
                            necro_intern_string(&intern, "atTheMountainsOfOK2"),
                            necro_intern_string(&intern, "Test"),
                            NULL
                        ),
                        necro_ast_create_apats(
                            &ast.arena,
                            necro_ast_create_var_with_ast_symbol(
                                &ast.arena,
                                necro_ast_symbol_create(
                                    &ast.arena,
                                    clash_x,
                                    necro_intern_string(&intern, "x"),
                                    necro_intern_string(&intern, "Test"),
                                    NULL
                                ),
                                NECRO_VAR_DECLARATION
                            ),
                            NULL
                        ),
                        necro_ast_create_rhs(
                            &ast.arena,
                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Necro.Base.False"),
                                    necro_intern_string(&intern, "False"),
                                    necro_intern_string(&intern, "Necro.Base"),
                                    NULL
                                ),
                                NECRO_CON_VAR
                            ),
                            NULL
                        )
                    ),
                    NULL
                ),
                NULL
            );

        const char* test_name = "GeneralizationIsOk2";
        const char* test_source = ""
            "atTheMountainsOfOK2 x = False\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(
                            &ast.arena,
                            necro_intern_string(&intern, "Test.atTheMountainsOfOK3"),
                            necro_intern_string(&intern, "atTheMountainsOfOK3"),
                            necro_intern_string(&intern, "Test"),
                            NULL
                        ),
                        necro_ast_create_apats(
                            &ast.arena,
                            necro_ast_create_var_with_ast_symbol(
                                &ast.arena,
                                necro_ast_symbol_create(
                                    &ast.arena,
                                    clash_x,
                                    necro_intern_string(&intern, "x"),
                                    necro_intern_string(&intern, "Test"),
                                    NULL
                                ),
                                NECRO_VAR_DECLARATION
                            ),
                            necro_ast_create_apats(
                                &ast.arena,
                                necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena,
                                        necro_intern_string(&intern, "Necro.Base.()"),
                                        necro_intern_string(&intern, "()"),
                                        necro_intern_string(&intern, "Necro.Base"),
                                        NULL
                                    ),
                                    NECRO_CON_VAR
                                ),
                                NULL
                            )
                        ),
                        necro_ast_create_rhs(
                            &ast.arena,
                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Necro.Base.False"),
                                    necro_intern_string(&intern, "False"),
                                    necro_intern_string(&intern, "Necro.Base"),
                                    NULL
                                ),
                                NECRO_CON_VAR
                            ),
                            NULL
                        )
                    ),
                    NULL
                ),
                NULL
            );

        const char* test_name = "GeneralizationIsOk3";
        const char* test_source = ""
            "atTheMountainsOfOK3 x () = False\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        NecroAst* invisible = necro_ast_create_data_declaration_with_ast_symbol(
            &ast.arena,
            necro_ast_symbol_create(&ast.arena,
                necro_intern_string(&intern, "Test.Invisible"),
                necro_intern_string(&intern, "Invisible"),
                necro_intern_string(&intern, "Test"),
                NULL
            ),
            necro_ast_create_simple_type_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena,
                    necro_intern_string(&intern, "Test.Invisible"),
                    necro_intern_string(&intern, "Invisible"),
                    necro_intern_string(&intern, "Test"),
                    NULL
                ),
                NULL
            ),
            necro_ast_create_list(
                &ast.arena,
                necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena,
                        necro_intern_string(&intern, "Test.Invisible"),
                        necro_intern_string(&intern, "Invisible"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    NULL
                ),
                NULL
            )
        );

        NecroAst* eq_instance = NULL;
        {
            NecroSymbol   clash_a = necro_append_clash_suffix_to_name(&ast, &intern, "Test.a");
            NecroSymbol   clash_b = necro_append_clash_suffix_to_name(&ast, &intern, "Test.b");
            eq_instance = necro_ast_create_apats_assignment_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.eq<Invisible>"), necro_intern_string(&intern, "eq<Invisible>"), necro_intern_string(&intern, "Test"), NULL),
                necro_ast_create_apats(
                    &ast.arena,
                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                        NECRO_VAR_DECLARATION
                    ),
                    necro_ast_create_apats(
                        &ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, clash_b, necro_intern_string(&intern, "b"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_DECLARATION
                        ),
                        NULL
                    )
                ),
                necro_ast_create_rhs(
                    &ast.arena,
                    necro_ast_create_conid_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena,
                            necro_intern_string(&intern, "Necro.Base.True"),
                            necro_intern_string(&intern, "True"),
                            necro_intern_string(&intern, "Necro.Base"),
                            NULL
                        ),
                        NECRO_CON_VAR
                    ),
                    NULL
                )
            );
        }

        NecroAst* neq_instance = NULL;
        {
            NecroSymbol   clash_a = necro_append_clash_suffix_to_name(&ast, &intern, "Test.a");
            NecroSymbol   clash_b = necro_append_clash_suffix_to_name(&ast, &intern, "Test.b");
            neq_instance = necro_ast_create_apats_assignment_with_ast_symbol(
                &ast.arena,
                necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.neq<Invisible>"), necro_intern_string(&intern, "neq<Invisible>"), necro_intern_string(&intern, "Test"), NULL),
                necro_ast_create_apats(
                    &ast.arena,
                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                        NECRO_VAR_DECLARATION
                    ),
                    necro_ast_create_apats(
                        &ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, clash_b, necro_intern_string(&intern, "b"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_DECLARATION
                        ),
                        NULL
                    )
                ),
                necro_ast_create_rhs(
                    &ast.arena,
                    necro_ast_create_conid_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena,
                            necro_intern_string(&intern, "Necro.Base.False"),
                            necro_intern_string(&intern, "False"),
                            necro_intern_string(&intern, "Necro.Base"),
                            NULL
                        ),
                        NECRO_CON_VAR
                    ),
                    NULL
                )
            );
        }

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    invisible,
                    NULL
                ),
                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                    necro_ast_create_decl(&ast.arena,
                        necro_ast_create_instance_with_symbol(
                            &ast.arena,
                            necro_ast_symbol_create(
                                &ast.arena,
                                necro_intern_string(&intern, "Necro.Base.Eq"),
                                necro_intern_string(&intern, "Eq"),
                                necro_intern_string(&intern, "Necro.Base"),
                                NULL
                            ),
                            necro_ast_create_conid_with_ast_symbol(
                                &ast.arena,
                                necro_ast_symbol_create(
                                    &ast.arena,
                                    necro_intern_string(&intern, "Test.Invisible"),
                                    necro_intern_string(&intern, "Invisible"),
                                    necro_intern_string(&intern, "Test"),
                                    NULL
                                ),
                                NECRO_CON_TYPE_VAR
                            ),
                            NULL,
                            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                necro_ast_create_decl(&ast.arena,
                                    eq_instance,
                                    NULL
                                ),
                                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                    necro_ast_create_decl(&ast.arena,
                                        neq_instance,
                                        NULL
                                    ),
                                    NULL
                                )
                            )
                        ),
                        NULL
                    ),
                    NULL
                )
            );

        const char* test_name = "Instance Methods";
        const char* test_source = ""
            "data Invisible = Invisible\n"
            "instance Eq Invisible where\n"
            "  eq  a b = True\n"
            "  neq a b = False\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_a = necro_append_clash_suffix_to_name(&ast, &intern, "Test.a");
        NecroSymbol   clash_b = necro_append_clash_suffix_to_name(&ast, &intern, "Test.b");
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_type_signature(&ast.arena,
                        NECRO_SIG_DECLARATION,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.threeOfAPerfectTuple"), necro_intern_string(&intern, "threeOfAPerfectTuple"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_SIG
                        ),
                        NULL,
                        necro_ast_create_type_fn(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_TYPE_FREE_VAR
                            ),
                            necro_ast_create_type_fn(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena, clash_b, necro_intern_string(&intern, "b"), necro_intern_string(&intern, "Test"), NULL),
                                    NECRO_VAR_TYPE_FREE_VAR
                                ),
                                necro_ast_create_tuple(
                                    &ast.arena,
                                    necro_ast_create_list(
                                        &ast.arena,
                                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                                            necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                                            NECRO_VAR_TYPE_FREE_VAR
                                        ),
                                        necro_ast_create_list(
                                            &ast.arena,
                                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                                necro_ast_symbol_create(&ast.arena, clash_b, necro_intern_string(&intern, "b"), necro_intern_string(&intern, "Test"), NULL),
                                                NECRO_VAR_TYPE_FREE_VAR
                                            ),
                                            NULL
                                        )
                                    )
                                )
                            )
                        )
                    ),
                    NULL
                ),
                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                    necro_ast_create_decl(&ast.arena,
                        necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.threeOfAPerfectTuple"), necro_intern_string(&intern, "threeOfAPerfectTuple"), necro_intern_string(&intern, "Test"), NULL),
                            necro_ast_create_apats(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                                    NECRO_VAR_DECLARATION),
                                necro_ast_create_apats(&ast.arena,
                                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                                        necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                        NECRO_VAR_DECLARATION),
                                    NULL)),
                            necro_ast_create_rhs(
                                &ast.arena,
                                necro_ast_create_tuple(
                                    &ast.arena,
                                    necro_ast_create_list(
                                        &ast.arena,
                                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                                            necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                                            NECRO_VAR_TYPE_FREE_VAR
                                        ),
                                        necro_ast_create_list(
                                            &ast.arena,
                                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                                necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                                NECRO_VAR_TYPE_FREE_VAR
                                            ),
                                            NULL
                                        )
                                    )
                                ),
                                NULL
                            )
                        ),
                        NULL
                    ),
                    NULL
                )
            );

        const char* test_name = "Tuple Types";
        const char* test_source = ""
            "threeOfAPerfectTuple :: a -> b -> (a, b)\n"
            "threeOfAPerfectTuple x y = (x, y)\n";

        necro_infer_test_comparison(test_name, test_source, &intern, &ast);
    }

    {
        const char* test_name   = "Two Class Type Variables";
        const char* test_source = ""
            "class OgreMagi a where\n"
            "  twoHeads :: OgreMagi b => b -> (a, b)\n"
            "instance OgreMagi (Maybe a) where\n"
            "  twoHeads x = (Nothing, x)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

// TODO: Put back
/*
    {
        puts("Infer {{{ child process inferTest:  starting...");
        assert(NECRO_COMPILE_IN_CHILD_PROCESS("inferTest.necro", "infer") == 0);
        puts("Infer }}} child process inferTest:  passed\n");
    }

    {
        puts("Infer {{{ child process typeClassTest:  starting...");
        assert(NECRO_COMPILE_IN_CHILD_PROCESS("typeClassTest.necro", "infer") == 0);
        puts("Infer }}} child process typeClassTest:  passed\n");
    }

    {
        puts("Infer {{{ child process typeClassTranslateTest:  starting...");
        assert(NECRO_COMPILE_IN_CHILD_PROCESS("typeClassTranslateTest.necro", "infer") == 0);
        puts("Infer }}} child process typeClassTranslateTest:  passed\n");
    }

    {
        puts("Infer {{{ child process pathologicalType:  starting...");
        assert(NECRO_COMPILE_IN_CHILD_PROCESS("pathologicalType.necro", "infer") == 0);
        puts("Infer }}} child process pathologicalType:  passed\n");
    }
*/

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.constant1"), necro_intern_string(&intern, "constant1"), necro_intern_string(&intern, "Test"), NULL), NULL,
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_fexpr(&ast.arena, necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.fromInt"), necro_intern_string(&intern, "fromInt"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_VAR_VAR),
                                necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER })),
                            NULL)
                    ),
                    NULL
                ),
                NULL);
        necro_infer_test_comparison("Constant1", "constant1 = 10\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   inner_name = necro_append_clash_suffix_to_name(&ast, &intern, "Test.inner");
        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.top"), necro_intern_string(&intern, "top"), necro_intern_string(&intern, "Test"), NULL), NULL,
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_let(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                    necro_ast_create_decl(&ast.arena,
                                        necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                                            necro_ast_symbol_create(&ast.arena,
                                                inner_name,
                                                necro_intern_string(&intern, "inner"),
                                                necro_intern_string(&intern, "Test"),
                                                NULL
                                            ),
                                            NULL,
                                            necro_ast_create_rhs(&ast.arena,
                                                necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                                    necro_ast_symbol_create(&ast.arena,
                                                        necro_intern_string(&intern, "Necro.Base.Nothing"),
                                                        necro_intern_string(&intern, "Nothing"),
                                                        necro_intern_string(&intern, "Necro.Base"),
                                                        NULL
                                                    ),
                                                    NECRO_CON_VAR
                                                ),
                                                NULL
                                            )
                                        ),
                                        NULL
                                    ),
                                    NULL
                                )
                            ),
                            NULL
                        )
                    ),
                    NULL
                ),
                NULL
            );
        necro_infer_test_comparison("Let", "top = let inner = Nothing in inner\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   inner_name = necro_append_clash_suffix_to_name(&ast, &intern, "Test.inner");
        NecroSymbol   other_name = necro_append_clash_suffix_to_name(&ast, &intern, "Test.other");
        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.top"), necro_intern_string(&intern, "top"), necro_intern_string(&intern, "Test"), NULL), NULL,
                        necro_ast_create_rhs(&ast.arena,

                            necro_ast_create_let(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                // declaration1
                                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                    necro_ast_create_decl(&ast.arena,
                                        necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                            necro_ast_create_rhs(&ast.arena,
                                                necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                                    NECRO_CON_VAR),
                                                NULL)),
                                        NULL),
                                    necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                        // declaration2
                                        necro_ast_create_decl(&ast.arena,
                                            necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, other_name, necro_intern_string(&intern, "other"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                                necro_ast_create_rhs(&ast.arena,
                                                    necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.False"), necro_intern_string(&intern, "False"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                                        NECRO_CON_VAR),
                                                    NULL)),
                                            NULL),
                                        NULL
                                    )
                                )
                            ),
                            NULL)
                    ),
                    NULL
                ),
                NULL
            );

        const char* test_code = ""
            "top =\n"
            "  let\n"
            "    inner = Nothing\n"
            "    other = False\n"
            "  in inner\n";

        necro_infer_test_comparison("Let2", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_let(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                    necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                        necro_ast_create_decl(&ast.arena, // CHANGE THIS TO A GROUP LIST!
                                            necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                                necro_ast_create_rhs(&ast.arena,
                                                    necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                                        NECRO_CON_VAR), NULL)
                                            ),
                                            NULL),
                                        NULL)
                                ),
                        NULL)
                    ),
                    NULL
                ),
                NULL
            );

        necro_infer_test_comparison("LetClash", "x = let x = Nothing in x\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        NecroSymbol   clash_x2 = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_let(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                    necro_ast_create_decl(&ast.arena,
                                        necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                            necro_ast_create_rhs(&ast.arena,
                                                necro_ast_create_let(&ast.arena,
                                                    necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x2, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                                        necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                                            necro_ast_create_decl(&ast.arena,
                                                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x2, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                                                    necro_ast_create_rhs(&ast.arena,
                                                                        necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                                                            NECRO_CON_VAR), NULL)
                                                                ),
                                                                NULL),
                                                            NULL
                                                        )
                                                    ),
                                                NULL)
                                        ),
                                        NULL),
                                    NULL)
                                ),
                            NULL)
                    ),
                    NULL),
                NULL);

        necro_infer_test_comparison("LetClash2", "x = let x = (let x = Nothing in x) in x\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                    necro_ast_create_decl(&ast.arena,
                                        necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                            necro_ast_create_rhs(&ast.arena,
                                                necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_CON_VAR)
                                                , NULL)),
                                        NULL),
                                    NULL)
                            )),
                    NULL
                ),
                NULL
            );

        necro_infer_test_comparison("WhereClash", "x = x where x = Nothing\n", &intern, &ast);
    }

//  #if 0 // Throws an error about Type Not An Instance Of, though I'm not sure that's expected
//     {
//         NecroIntern   intern = necro_intern_create();
//         NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
//         NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
//         ast.root =
//                 necro_ast_create_declaration_group_list_with_next(&ast.arena,
//                     necro_ast_create_decl(&ast.arena,
//                     necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
//                         necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
//                         necro_ast_create_rhs(&ast.arena,
//                             necro_ast_create_do(&ast.arena,
//                                 necro_ast_create_list(&ast.arena,
//                                     necro_ast_create_bind_assignment(&ast.arena,
//                                         necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
//                                         necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_CON_VAR)
//                                     ),
//                                     necro_ast_create_list(&ast.arena,
//                                         necro_ast_create_fexpr(&ast.arena,
//                                             necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.pure"), necro_intern_string(&intern, "pure"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_VAR_VAR),
//                                             necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_VAR)
//                                         ),
//                                         NULL)
//                                 )
//                             ),
//                             NULL)),
//                     NULL
//                 ),
//                 NULL
//             );
//
//         // necro_ast_print(ast.root);
//         necro_infer_test_comparison("BindClash", "x = do\n  x <- Nothing\n  pure x\n", &intern, &ast);
//     }
// #endif

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                        necro_ast_create_apats(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_DECLARATION),
                            NULL),
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_VAR),
                            NULL)),
                    NULL
                 ),
                 NULL
            );
        // necro_ast_print(ast.root);

        necro_infer_test_comparison("ApatsAssignment1", "x y = y\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");
        NecroSymbol   clash_y2 = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                        // apat assignment var
                        necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                        // apat assignment apats
                        necro_ast_create_apats(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_DECLARATION),
                            NULL),
                        necro_ast_create_rhs(&ast.arena,
                            // RHS expression
                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_y2, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_VAR),
                            // RHS declarations
                            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                                necro_ast_create_decl(&ast.arena,
                                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                                        necro_ast_symbol_create(&ast.arena,
                                            clash_y2,
                                            necro_intern_string(&intern, "y"),
                                            necro_intern_string(&intern, "Test"),
                                            NULL
                                        ),
                                        necro_ast_create_fexpr(&ast.arena,
                                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                                necro_ast_symbol_create(&ast.arena,
                                                    necro_intern_string(&intern, "Necro.Base.fromInt"),
                                                    necro_intern_string(&intern, "fromInt"),
                                                    necro_intern_string(&intern, "Necro.Base"),
                                                    NULL
                                                ),
                                                NECRO_VAR_VAR
                                            ),
                                            necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 0, .type = NECRO_AST_CONSTANT_INTEGER })
                                        ),
                                        necro_ast_create_rhs(&ast.arena,
                                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_y2, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_VAR), NULL)),
                                    NULL),
                                NULL
                            )
                        )),
                    NULL
                ),
                NULL
            );

        necro_infer_test_comparison("ApatsAssignmentYs", "x y = y where y ~ 0 = y\n", &intern, &ast);
    }

    {
        NecroIntern			intern = necro_intern_create();
        NecroAstArena		ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol			clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                        necro_ast_create_apats(&ast.arena,
                            necro_ast_create_constructor_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Just"), necro_intern_string(&intern, "Just"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                necro_ast_create_apats(&ast.arena,
                                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                                        necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                        NECRO_VAR_DECLARATION),
                                    NULL
                                )
                            ),
                            NULL
                        ),
                        necro_ast_create_rhs(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_VAR
                            ),
                            NULL
                        )
                    ),
                    NULL
                ),
                NULL
            );

        necro_infer_test_comparison("ApatsAssignmentPatBind", "x (Just y) = y\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_a = necro_append_clash_suffix_to_name(&ast, &intern, "Test.a");
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");
        NecroSymbol   clash_z = necro_append_clash_suffix_to_name(&ast, &intern, "Test.z");

        NecroAst* bin_op = necro_ast_create_bin_op_with_ast_symbol(&ast.arena,
            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.+"), necro_intern_string(&intern, "+"), necro_intern_string(&intern, "Necro.Base"), NULL),
            necro_ast_create_var_with_ast_symbol(&ast.arena,
                necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                NECRO_VAR_VAR
            ),
            necro_ast_create_var_with_ast_symbol(&ast.arena,
                necro_ast_symbol_create(&ast.arena, clash_z, necro_intern_string(&intern, "z"), necro_intern_string(&intern, "Test"), NULL),
                NECRO_VAR_VAR
            ),
            necro_type_fresh_var(&ast.arena, NULL)
        );

        bin_op->bin_op.type = NECRO_BIN_OP_ADD;

        ast.root =
            necro_ast_create_declaration_group_list_with_next(&ast.arena,
                necro_ast_create_decl(&ast.arena,
                    necro_ast_create_type_signature(&ast.arena,
                        NECRO_SIG_DECLARATION,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_SIG
                        ),
                        necro_ast_create_context_with_ast_symbols(&ast.arena,
                            necro_ast_symbol_create(&ast.arena,
                                necro_intern_string(&intern, "Necro.Base.Num"), necro_intern_string(&intern, "Num"), necro_intern_string(&intern, "Necro.Base"),
                                NULL
                            ),
                            necro_ast_symbol_create(&ast.arena,
                                clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"),
                                NULL
                            )
                        ),
                        necro_ast_create_type_fn(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_TYPE_FREE_VAR
                            ),
                            necro_ast_create_type_fn(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                                    NECRO_VAR_TYPE_FREE_VAR
                                ),
                                necro_ast_create_var_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                                    NECRO_VAR_TYPE_FREE_VAR
                                )
                            )
                        )
                    ),
                    NULL
                ),
                necro_ast_create_declaration_group_list_with_next(&ast.arena,
                    necro_ast_create_decl(&ast.arena,
                        necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                            necro_ast_create_apats(&ast.arena,
                                necro_ast_create_var_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                    NECRO_VAR_DECLARATION),
                                necro_ast_create_apats(&ast.arena,
                                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                                        necro_ast_symbol_create(&ast.arena, clash_z, necro_intern_string(&intern, "z"), necro_intern_string(&intern, "Test"), NULL),
                                        NECRO_VAR_DECLARATION),
                                    NULL)),
                            necro_ast_create_rhs(&ast.arena, bin_op, NULL)
                        ),
                        NULL
                    ),
                    NULL
                )
            );

        const char* test_code = ""
            "x :: Num a => a -> a -> a\n"
            "x y z = y + z\n";

        necro_infer_test_comparison("TypeClassContext", test_code, &intern, &ast);
    }

    {
        const char* test_name   = "For Loop 1";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes = loop x = 0 for i <- tenTimes do\n"
            "  x * 2\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "For Loop 2";
        const char* test_source = ""
            "tenTimes :: Range 10\n"
            "tenTimes = each\n"
            "loopTenTimes = loop (x, y) = (True, False) for i <- tenTimes do\n"
            "  (y, x)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unboxed Tuple 1";
        const char* test_source = ""
            "unboxedTuple = (#True, False#)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unboxed Tuple 2";
        const char* test_source = ""
            "unboxedTuple :: (#Bool, Int#)\n"
            "unboxedTuple = (#True, 0#)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unboxed Tuple 3";
        const char* test_source = ""
            "unboxedTuple :: (Bool, Int)\n"
            "unboxedTuple = (#True, 0#)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Unboxed Tuple 4";
        const char* test_source = ""
            "unboxedTuple :: (#Bool, Int, Float#)\n"
            "unboxedTuple = (#True, 0#)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Unboxed Tuple 5";
        const char* test_source = ""
            "unboxedTuple :: (#Bool, Int, Maybe ()#) -> Bool\n"
            "unboxedTuple (#b, _, _#) = b\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Unboxed Tuple 6";
        const char* test_source = ""
            "data TripleThreat a = TripleThreat (#a, a, a#)\n"
            "tripleThreat :: TripleThreat (Maybe Bool)\n"
            "tripleThreat = TripleThreat (#Nothing, Nothing, Just True#)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 1";
        const char* test_source = ""
            "justNothing :: Float\n"
            "justNothing = x where\n"
            "  Just x = Nothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 2";
        const char* test_source = ""
            "justNothing :: Float\n"
            "justNothing = x where\n"
            "  Just (x ~ 0.0) = Just (x + 1)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 3";
        const char* test_source = ""
            "unboxedTuple :: Int\n"
            "unboxedTuple = x where\n"
            "  Just (x, acc) = Just (0, 1)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 4";
        const char* test_source = ""
            "unboxedTuple :: Int\n"
            "unboxedTuple = x where\n"
            "  Just (#x, acc#) = Just (#0, 1#)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 5";
        const char* test_source = ""
            "unboxedTuple :: Int\n"
            "unboxedTuple = x where\n"
            "  (x, acc) = (0, 1)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 6";
        const char* test_source = ""
            "unboxedTuple :: Int\n"
            "unboxedTuple = x where\n"
            "  (#x, acc#) = (#0, 1#)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pat Assignment 7";
        const char* test_source = ""
            "unboxedTuple :: Bool\n"
            "unboxedTuple = x where\n"
            "  (#x, (acc ~ 0)#) = (#True, acc + 1#)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // TODO: Parsing needs to be extended with better oppat support if we want this
    // {
    //     const char* test_name   = "Pat Assignment 8";
    //     const char* test_source = ""
    //         "unboxedTuple :: Int\n"
    //         "unboxedTuple = acc where\n"
    //         "  (#x, acc ~ 0#) = (#True, acc + 1#)\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    // }

/*
    {
        const char* test_name   = "For Loop 3";
        const char* test_source = ""
            "lotsOfStuff :: Array 3 Float\n"
            "lotsOfStuff = { 1, 2, 3 }\n"
            "loopTenTimes =\n"
            "  for (range lotsOfStuff) 0 loop i _ ->\n"
            "    index lotsOfStuff i\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }
*/

    // TODO: Parsing broke on initialization stuff for som reason...
    // {
    //     const char* test_name   = "For Loop 3";
    //     const char* test_source = ""
    //         "tenTimes :: Range 10 (Index 10)\n"
    //         "tenTimes = each\n"
    //         "loopTest = a1 where\n"
    //         "  (feed ~ 0, a1) = for tenTimes (feed, 0) loop i (f, a) -> (f + 1, a)\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    // }

}

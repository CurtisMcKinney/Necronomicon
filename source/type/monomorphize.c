/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "monomorphize.h"
#include "base.h"
#include "infer.h"
#include "result.h"
#include "kind.h"
#include "alias_analysis.h"

/*
    TODO:
        * Look into Pattern Assignment + Initializers
        * Check Numeric patterns
        * Pattern Literals
        * do statements
        * Prune pass after to cull all unneeded stuff and put it in a direct form to make Chad's job for Core translation easier?

*/

///////////////////////////////////////////////////////
// NecroMonomorphize
///////////////////////////////////////////////////////

typedef struct
{
    NecroIntern*         intern;
    NecroScopedSymTable* scoped_symtable;
    NecroBase*           base;
    NecroAstArena*       ast_arena;
    NecroPagedArena*     arena;
    NecroSnapshotArena   snapshot_arena;
    NecroConstraintEnv   con_env;
    NecroAst*            lift_point;
} NecroMonomorphize;

NecroMonomorphize necro_monomorphize_empty()
{
    NecroMonomorphize monomorphize = (NecroMonomorphize)
    {
        .intern          = NULL,
        .arena           = NULL,
        .snapshot_arena  = necro_snapshot_arena_empty(),
        .scoped_symtable = NULL,
        .base            = NULL,
        .ast_arena       = NULL,
        .con_env         = necro_constraint_env_empty(),
        .lift_point      = NULL,
    };
    return monomorphize;
}

NecroMonomorphize necro_monomorphize_create(NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroMonomorphize monomorphize = (NecroMonomorphize)
    {
        .intern          = intern,
        .arena           = &ast_arena->arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
        .con_env         = necro_constraint_env_create(),
        .lift_point      = NULL,
    };
    return monomorphize;
}

void necro_monomorphize_destroy(NecroMonomorphize* monomorphize)
{
    assert(monomorphize != NULL);
    necro_snapshot_arena_destroy(&monomorphize->snapshot_arena);
    necro_constraint_env_destroy(&monomorphize->con_env);
    *monomorphize = necro_monomorphize_empty();
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroResult(void) necro_monomorphize_go(NecroMonomorphize* monomorphize, NecroAst* ast, NecroInstSub* subs);
// void              necro_monomorphize_type_go(NecroMonomorphize* monomorphize, NecroAst* ast);
NecroAstSymbol*   necro_ast_specialize(NecroMonomorphize* monomorphize, NecroAstSymbol* ast_symbol, NecroInstSub* subs);

NecroResult(void) necro_monomorphize(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroMonomorphize monomorphize = necro_monomorphize_create(intern, scoped_symtable, base, ast_arena);
    NecroResult(void) result       = necro_monomorphize_go(&monomorphize, ast_arena->root, NULL);
    if (result.type == NECRO_RESULT_OK)
    {
        // TODO: Fully specialize types at the end of core instead of before core.
        // necro_monomorphize_type_go(&monomorphize, ast_arena->root);
        necro_monomorphize_destroy(&monomorphize);
        if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_MONOMORPHIZE && info.verbosity > 0))
            necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    necro_monomorphize_destroy(&monomorphize);
    return result;
}

///////////////////////////////////////////////////////
// SpecializeAst
///////////////////////////////////////////////////////
NecroInstSub* necro_type_create_instance_subs(NecroMonomorphize* monomorphize, NecroTypeClass* type_class, NecroInstSub* subs)
{
    // Find Sub
    NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        NecroType* sub_type = necro_type_find(curr_sub->new_name);
        if (curr_sub->var_to_replace->source_name == type_class->type_var->source_name && curr_sub->var_to_replace->is_class_head_var)
        {
            // Find Instance
            assert(sub_type->type == NECRO_TYPE_CON);
            NecroInstanceList* instance_list = sub_type->con.con_symbol->instance_list;
            while (instance_list != NULL)
            {
                if (instance_list->data->type_class_name == type_class->type_class_name)
                {
                    // Create InstanceSubs
                    NecroTypeClassInstance* instance      = instance_list->data;
                    NecroInstSub*           instance_subs = NULL;
                    NecroType*              instance_type = necro_type_instantiate_with_subs(monomorphize->arena, &monomorphize->con_env, monomorphize->base, instance->data_type, NULL, &instance_subs, zero_loc, zero_loc);
                    unwrap(NecroType, necro_type_unify(monomorphize->arena, &monomorphize->con_env, monomorphize->base, instance_type, sub_type, NULL));
                    NecroInstSub*           new_subs      = necro_type_filter_and_deep_copy_subs(monomorphize->arena, subs, curr_sub->var_to_replace, instance_type);
                    // NecroInstSub*           new_subs      = necro_type_filter_and_deep_copy_subs(monomorphize->arena, subs, type_class->type_class_name, instance_type);
                    new_subs                              = necro_type_union_subs(monomorphize->arena, monomorphize->base, new_subs, instance_subs);
                    if (new_subs != NULL)
                    {
                        NecroInstSub* curr_new_sub = new_subs;
                        while (curr_new_sub->next != NULL)
                        {
                            curr_new_sub = curr_new_sub->next;
                        }
                        curr_new_sub->next = instance_subs;
                        // new_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, new_subs, type_class->type_var, instance_type);
                        new_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, new_subs, curr_sub->var_to_replace, instance_type);
                        return new_subs;
                    }
                    else
                    {
                        // instance_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, instance_subs, type_class->type_class_name, instance_type);
                        instance_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, instance_subs, curr_sub->var_to_replace, instance_type);
                        return instance_subs;
                    }
                }
                instance_list = instance_list->next;
            }
            assert(false);
        }
        curr_sub = curr_sub->next;
    }
    assert(false);
    return NULL;
}

NecroAstSymbol* necro_ast_specialize_method(NecroMonomorphize* monomorphize, NecroAstSymbol* ast_symbol, NecroInstSub* subs)
{
    assert(monomorphize != NULL);
    assert(ast_symbol != NULL);
    assert(subs != NULL);

    NecroTypeClass* type_class     = ast_symbol->method_type_class;
    NecroAstSymbol* type_class_var = type_class->type_var;
    NecroInstSub*   curr_sub       = subs;
    while (curr_sub != NULL)
    {
        // TODO: Figure out system for this...fuuuuck.
        // if (curr_sub->var_to_replace->name == type_class_var->name)
        if (curr_sub->var_to_replace->source_name == type_class_var->source_name && curr_sub->var_to_replace->is_class_head_var)
        {
            NecroType* sub_con = necro_type_find(curr_sub->new_name);
            necro_type_collapse_app_cons(monomorphize->arena, monomorphize->base, sub_con);
            assert(sub_con->type == NECRO_TYPE_CON);
            NecroSymbol     instance_method_name       = necro_intern_create_type_class_instance_symbol(monomorphize->intern, ast_symbol->source_name, sub_con->con.con_symbol->source_name);
            NecroAstSymbol* instance_method_ast_symbol = necro_scope_find_ast_symbol(monomorphize->scoped_symtable->top_scope, instance_method_name);
            assert(instance_method_ast_symbol != NULL);
            if (necro_type_is_polymorphic(instance_method_ast_symbol->type))
            {
                NecroInstSub* instance_subs = necro_type_create_instance_subs(monomorphize, type_class, subs);
                // TODO / HACK / NOTE: This isn't playing nice with uvars. Take another look at this when that is sorted!
                if (instance_subs == NULL)
                    return instance_method_ast_symbol;
                else
                    return necro_ast_specialize(monomorphize, instance_method_ast_symbol, instance_subs);
            }
            else
            {
                return instance_method_ast_symbol;
            }
        }
        curr_sub = curr_sub->next;
    }
    assert(false);
    return NULL;
}

// TODO: Add usage point and lift specialized ast up into global scope right before usage point
NecroAstSymbol* necro_ast_specialize(NecroMonomorphize* monomorphize, NecroAstSymbol* ast_symbol, NecroInstSub* subs)
{
    assert(monomorphize != NULL);
    assert(ast_symbol != NULL);
    assert(ast_symbol->ast != NULL);
    assert(ast_symbol->ast->scope != NULL);
    assert(subs != NULL);

    // TODO: Assert sub size == for_all size
    if (ast_symbol->method_type_class)
        return necro_ast_specialize_method(monomorphize, ast_symbol, subs);

    //--------------------
    // HACK: This really shouldn't be necessary here. Perhaps the better solution is to unify TypeApp and TypeCon
    // Collapse TypeApps -> TypeCons
    //--------------------
    NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        necro_type_collapse_app_cons(monomorphize->arena, monomorphize->base, curr_sub->new_name);
        curr_sub = curr_sub->next;
    }

    //--------------------
    // Find specialized ast
    //--------------------
    NecroSymbol     specialized_name       = necro_intern_append_suffix_from_subs(monomorphize->intern, ast_symbol->source_name, subs);
    NecroScope*     scope                  = ast_symbol->ast->scope;
    NecroAstSymbol* specialized_ast_symbol = necro_scope_find_ast_symbol(scope, specialized_name);
    if (specialized_ast_symbol != NULL)
    {
        assert(specialized_ast_symbol->ast != NULL);
        return specialized_ast_symbol;
    }

    //--------------------
    // Find and create specialized ast insertion point
    // This is handled differently for global bindings and local bindings
    // This is necessitated by the semantics of local bindings and the nature of specializing class methods defined in different modules (such as base)
    //--------------------
    bool      is_lift_method1            = true;
    NecroAst* new_declaration            = NULL;
    NecroAst* new_declaration_group_list = NULL;
    if (monomorphize->lift_point == NULL || ast_symbol->ast->scope->parent != NULL)
    {
        // Monomorphize local bindings
        NecroAst* declaration_group = ast_symbol->declaration_group;
        assert(declaration_group != NULL);
        assert(declaration_group->type == NECRO_AST_DECL);
        while (declaration_group->declaration.next_declaration != NULL) // Append
            declaration_group = declaration_group->declaration.next_declaration;
        new_declaration                                     = necro_ast_create_decl(monomorphize->arena, ast_symbol->ast, declaration_group->declaration.next_declaration);
        declaration_group->declaration.next_declaration     = new_declaration;
        new_declaration->declaration.declaration_group_list = declaration_group->declaration.declaration_group_list;
        new_declaration->declaration.declaration_impl       = NULL; // Removing dummy argument
    }
    else
    {
        // Monomorphize global bindings (potentially out of order global bindings, due to type classes, thus the difference in handling).
        // Lift global bindings at right before the USE point (instead of in the original declaration group).
        is_lift_method1 = false;
        assert(monomorphize->lift_point != NULL);
        assert(monomorphize->lift_point->type == NECRO_AST_DECLARATION_GROUP_LIST);
        NecroAst* prev_declaration_group_list                    = monomorphize->lift_point;
        NecroAst* next_declaration_group_list                    = prev_declaration_group_list->declaration_group_list.next;
        new_declaration                                          = necro_ast_create_decl(monomorphize->arena, ast_symbol->ast, NULL);
        new_declaration_group_list                               = necro_ast_create_declaration_group_list_with_next(monomorphize->arena, new_declaration, next_declaration_group_list);
        prev_declaration_group_list->declaration_group_list.next = new_declaration_group_list;
        new_declaration->declaration.declaration_group_list      = new_declaration_group_list;
        new_declaration->declaration.declaration_impl            = NULL;
    }

    //--------------------
    // Deep Copy Ast
    //--------------------
    // TODO: Account for NOT mangling original name....how?
    // TODO: Actually, how to handle this whole renaming business???
    // Create new scope up here, insert named thing up here, percolates to bottom?
    // TODO: More principled deep copy
    specialized_ast_symbol                        = necro_ast_symbol_create(monomorphize->arena, specialized_name, ast_symbol->name, monomorphize->ast_arena->module_name, NULL);
    specialized_ast_symbol->is_constructor        = ast_symbol->is_constructor;
    specialized_ast_symbol->is_wrapper            = ast_symbol->is_wrapper;
    specialized_ast_symbol->is_enum               = ast_symbol->is_enum;
    specialized_ast_symbol->is_primitive          = ast_symbol->is_primitive;
    specialized_ast_symbol->is_recursive          = ast_symbol->is_recursive;
    specialized_ast_symbol->is_unboxed            = ast_symbol->is_unboxed;
    specialized_ast_symbol->never_inline          = ast_symbol->never_inline;
    specialized_ast_symbol->primop_type           = ast_symbol->primop_type;
    specialized_ast_symbol->declaration_group     = new_declaration;

    // NEW COPYING SCHEME
    // specialized_ast_symbol->ast                   = necro_ast_deep_copy_go(monomorphize->arena, new_declaration, ast_symbol->ast);
    NecroScope* copy_scope                        = necro_scope_create(monomorphize->arena, NULL);
    necro_scope_insert_ast_symbol(monomorphize->arena, copy_scope, specialized_ast_symbol);
    specialized_ast_symbol->ast                   = necro_ast_deep_copy_with_new_names(monomorphize->arena, monomorphize->intern, copy_scope, new_declaration, ast_symbol->ast);
    specialized_ast_symbol->source_name           = specialized_ast_symbol->name;
    necro_scope_insert_ast_symbol(monomorphize->arena, scope, specialized_ast_symbol);

    new_declaration->declaration.declaration_impl = specialized_ast_symbol->ast;

    assert(ast_symbol->ast != specialized_ast_symbol->ast);
    switch (specialized_ast_symbol->ast->type)
    {
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        specialized_ast_symbol->ast->simple_assignment.ast_symbol = specialized_ast_symbol;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        specialized_ast_symbol->ast->apats_assignment.ast_symbol = specialized_ast_symbol;
        break;
    default:
        assert(false);
        break;
    }
    NecroScope* prev_scope                            = monomorphize->scoped_symtable->current_scope;
    NecroScope* prev_type_scope                       = monomorphize->scoped_symtable->current_type_scope;
    monomorphize->scoped_symtable->current_scope      = scope;

    // TODO: Not mangling specialized recursive simple assignment acc value in runSeq2 for some reason!?
    // necro_rename_internal_scope_and_rename(monomorphize->ast_arena, monomorphize->scoped_symtable, monomorphize->intern, specialized_ast_symbol->ast);

    //--------------------
    // Specialize Ast
    //--------------------
    specialized_ast_symbol->type = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast_symbol->type, subs);
    unwrap(void, necro_monomorphize_go(monomorphize, specialized_ast_symbol->ast, subs));
    if (!is_lift_method1)
        monomorphize->lift_point = new_declaration_group_list; // Move lift point to the newly created Declaration Group for the specialized ast.

    monomorphize->scoped_symtable->current_scope      = prev_scope;
    monomorphize->scoped_symtable->current_type_scope = prev_type_scope;

    // necro_ast_print(specialized_ast_symbol->ast);

    return specialized_ast_symbol;
}

/*
    Note: To specialize a value or function:
        * The value or function must be polymorphic.
        * The usage of the value or function must be completely concrete.
        * If we encounter a monomorphic type variable we try to default it, if we can't it's a type error.
*/
NecroResult(bool) necro_ast_should_specialize(NecroPagedArena* arena, NecroBase* base, NecroAstSymbol* ast_symbol, NecroAst* ast, NecroInstSub* inst_subs)
{
    const bool is_symbol_poly    = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, ast_symbol->type, ast_symbol->type, ast->source_loc, ast->end_loc));
    const bool is_ast_poly       = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
    bool       should_specialize = is_symbol_poly && !is_ast_poly;
    NecroInstSub* curr_sub    = inst_subs;
    while (curr_sub != NULL)
    {
        const bool is_curr_sub_poly = necro_try_result(bool, necro_type_is_unambiguous_polymorphic(arena, base, curr_sub->new_name, curr_sub->new_name, ast->source_loc, ast->end_loc));
        should_specialize           = should_specialize && !is_curr_sub_poly;
        curr_sub                    = curr_sub->next;
    }
    return ok(bool, should_specialize);
}

NecroResult(void) necro_rec_check_pat_assignment(NecroBase* base, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
        // if (ast->variable.initializer != NULL && !necro_is_fully_concrete(base, ast->variable.ast_symbol->type))
        // if (ast->variable.initializer != NULL && !necro_type_is_zero_order(ast->variable.ast_symbol->type))
        if (ast->variable.initializer != NULL)
        {
            // necro_try_map(NecroType, void, necro_type_set_zero_order(ast->variable.ast_symbol->type, &ast->source_loc, &ast->end_loc));
            // return necro_type_non_concrete_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        if (ast->variable.initializer != NULL && !ast->variable.is_recursive)
        {
            // TODO: This is being thrown when it shouldn't!
            // return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        return ok_void();
    case NECRO_AST_CONSTANT:
        return ok_void();
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_try(void, necro_rec_check_pat_assignment(base, args->list.item));
            args = args->list.next_item;
        }
        return ok_void();
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_try(void, necro_rec_check_pat_assignment(base, args->list.item));
            args = args->list.next_item;
        }
        return ok_void();
    }
    case NECRO_AST_WILDCARD:
        return ok_void();
    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_rec_check_pat_assignment(base, ast->bin_op_sym.left));
        return necro_rec_check_pat_assignment(base, ast->bin_op_sym.right);
    case NECRO_AST_CONID:
        return ok_void();
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_try(void, necro_rec_check_pat_assignment(base, args->list.item));
            args = args->list.next_item;
        }
        return ok_void();
    }
    default:
        necro_unreachable(void);
    }
}

///////////////////////////////////////////////////////
// Monomorphize Go
///////////////////////////////////////////////////////
NecroResult(void) necro_monomorphize_go(NecroMonomorphize* monomorphize, NecroAst* ast, NecroInstSub* subs)
{
    if (ast == NULL)
        return ok_void();
    // Maybe deep copy type here, instead of at ast deep copy?
    // ast->necro_type = necro_try_map_result(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->necro_type, subs));
    ast->necro_type = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast->necro_type, subs);
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_TOP_DECL:
        necro_unreachable(void);

    case NECRO_AST_DECLARATION_GROUP_LIST:
    {
        const bool is_top_level = monomorphize->lift_point == NULL;
        NecroAst* prev_group_list = NULL;
        NecroAst* group_list      = ast;
        while (group_list != NULL)
        {
            // We're caching this here since the "next" point will likely be altered during the course of monomorphization, which lifts ast's to the top level
            NecroAst* next_group_list = group_list->declaration_group_list.next;
            if (is_top_level)
                monomorphize->lift_point = prev_group_list;
            necro_try(void, necro_monomorphize_go(monomorphize, group_list->declaration_group_list.declaration_group, subs));
            // group_list = group_list->declaration_group_list.next;
            prev_group_list = group_list;
            group_list      = next_group_list;
        }
        return ok_void();
    }

    case NECRO_AST_DECL:
    {
        NecroAst* declaration_group = ast;
        while (declaration_group != NULL)
        {
            necro_try(void, necro_monomorphize_go(monomorphize, declaration_group->declaration.declaration_impl, subs));
            declaration_group = declaration_group->declaration.next_declaration;
        }
        return ok_void();
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        return necro_monomorphize_go(monomorphize, ast->type_class_instance.declarations, subs);

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        // ast->simple_assignment.ast_symbol->type = necro_try_map_result(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->simple_assignment.ast_symbol->type, subs));
        ast->simple_assignment.ast_symbol->type = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast->simple_assignment.ast_symbol->type, subs);
        if (ast->simple_assignment.initializer != NULL)
        {
            // necro_try_map(NecroType, void, necro_type_set_zero_order(ast->necro_type, &ast->source_loc, &ast->end_loc));
        }
        if (ast->simple_assignment.initializer != NULL && !ast->simple_assignment.is_recursive)
        {
            return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->simple_assignment.initializer, subs));
        return necro_monomorphize_go(monomorphize, ast->simple_assignment.rhs, subs);

    case NECRO_AST_APATS_ASSIGNMENT:
        // necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type->ownership, ast->necro_type->ownership, ast->source_loc, ast->end_loc));
        // ast->apats_assignment.ast_symbol->type = necro_try_map_result(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->apats_assignment.ast_symbol->type, subs));
        ast->apats_assignment.ast_symbol->type = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast->apats_assignment.ast_symbol->type, subs);
        necro_try(void, necro_monomorphize_go(monomorphize, ast->apats_assignment.apats, subs));
        return necro_monomorphize_go(monomorphize, ast->apats_assignment.rhs, subs);

    case NECRO_AST_PAT_ASSIGNMENT:
        necro_try(void, necro_rec_check_pat_assignment(monomorphize->base, ast->pat_assignment.pat));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->pat_assignment.pat, subs));
        return necro_monomorphize_go(monomorphize, ast->pat_assignment.rhs, subs);

    case NECRO_AST_DATA_DECLARATION:
        return ok_void();

    case NECRO_AST_VARIABLE:
        if (ast->variable.ast_symbol == monomorphize->base->prim_undefined)
            return ok_void();
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
        {
            ast->variable.inst_subs      = necro_type_union_subs(monomorphize->arena, monomorphize->base, ast->variable.inst_subs, subs);
            const bool should_specialize = necro_try_map_result(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->variable.ast_symbol, ast, ast->variable.inst_subs));
            if (!should_specialize)
                return ok_void();
            NecroAstSymbol* specialized_ast_symbol = necro_ast_specialize(monomorphize, ast->variable.ast_symbol, ast->variable.inst_subs);
            NecroAst*       specialized_var        = necro_ast_create_var_full(monomorphize->arena, specialized_ast_symbol, NECRO_VAR_VAR, NULL, NULL, ast->variable.order);
            specialized_var->necro_type            = specialized_ast_symbol->type;
            *ast = *specialized_var;
            return ok_void();
        }

        case NECRO_VAR_DECLARATION:
            if (ast->variable.ast_symbol != NULL)
            {
                // ast->variable.ast_symbol->type = necro_try_map_result(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->variable.ast_symbol->type, subs));
                ast->variable.ast_symbol->type = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast->variable.ast_symbol->type, subs);
            }
            if (ast->variable.initializer != NULL)
                necro_try(void, necro_monomorphize_go(monomorphize, ast->variable.initializer, subs));
            return ok_void();
        case NECRO_VAR_SIG:                  return ok_void();
        case NECRO_VAR_TYPE_VAR_DECLARATION: return ok_void();
        case NECRO_VAR_TYPE_FREE_VAR:        return ok_void();
        case NECRO_VAR_CLASS_SIG:            return ok_void();
        default:
            necro_unreachable(void);
        }

    case NECRO_AST_CONID:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        return ok_void();

    case NECRO_AST_UNDEFINED:
        return ok_void();

    case NECRO_AST_CONSTANT:
        // TODO (Curtis, 2-14-19): Handle this for overloaded numeric literals in patterns
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        return ok_void();

    case NECRO_AST_UN_OP:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        return ok_void();

    case NECRO_AST_BIN_OP:
    {
        ast->bin_op.op_type          = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast->bin_op.op_type, subs);
        ast->bin_op.inst_subs        = necro_type_union_subs(monomorphize->arena, monomorphize->base, ast->bin_op.inst_subs, subs);
        const bool should_specialize = necro_try_map_result(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->bin_op.ast_symbol, ast, ast->bin_op.inst_subs));
        if (should_specialize)
        {
            ast->bin_op.ast_symbol = necro_ast_specialize(monomorphize, ast->bin_op.ast_symbol, ast->bin_op.inst_subs);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op.lhs, subs));
        return necro_monomorphize_go(monomorphize, ast->bin_op.rhs, subs);
    }

    case NECRO_AST_OP_LEFT_SECTION:
    {
        ast->op_left_section.inst_subs = necro_type_union_subs(monomorphize->arena, monomorphize->base, ast->op_left_section.inst_subs, subs);
        const bool should_specialize   = necro_try_map_result(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->op_left_section.ast_symbol, ast, ast->op_left_section.inst_subs));
        if (should_specialize)
        {
            ast->op_left_section.ast_symbol = necro_ast_specialize(monomorphize, ast->op_left_section.ast_symbol, ast->op_left_section.inst_subs);
        }
        return necro_monomorphize_go(monomorphize, ast->op_left_section.left, subs);
    }

    case NECRO_AST_OP_RIGHT_SECTION:
    {
        ast->op_right_section.inst_subs = necro_type_union_subs(monomorphize->arena, monomorphize->base, ast->op_right_section.inst_subs, subs);
        const bool should_specialize    = necro_try_map_result(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->op_right_section.ast_symbol, ast, ast->op_right_section.inst_subs));
        if (should_specialize)
        {
            ast->op_right_section.ast_symbol = necro_ast_specialize(monomorphize, ast->op_right_section.ast_symbol, ast->op_right_section.inst_subs);
        }
        return necro_monomorphize_go(monomorphize, ast->op_right_section.right, subs);
    }

    case NECRO_AST_IF_THEN_ELSE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->if_then_else.if_expr, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->if_then_else.then_expr, subs));
        return necro_monomorphize_go(monomorphize, ast->if_then_else.else_expr, subs);

    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->right_hand_side.declarations, subs));
        return necro_monomorphize_go(monomorphize, ast->right_hand_side.expression, subs);

    case NECRO_AST_LET_EXPRESSION:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->let_expression.declarations, subs));
        return necro_monomorphize_go(monomorphize, ast->let_expression.expression, subs);

    case NECRO_AST_FUNCTION_EXPRESSION:
    {
        // HACK / Quick / Dirty fromInt<Int>, fromInt<Float> optimizations
        NecroType* f_type = necro_type_strip_for_all(necro_type_find(ast->fexpression.aexp->necro_type));
        if (ast->fexpression.aexp->type == NECRO_AST_VARIABLE && ast->fexpression.aexp->variable.ast_symbol == monomorphize->base->from_int && f_type->type == NECRO_TYPE_FUN && necro_type_find(ast->necro_type)->type == NECRO_TYPE_CON && necro_type_find(ast->necro_type)->con.con_symbol == monomorphize->base->int_type)
        {
            *ast = *ast->fexpression.next_fexpression;
            return necro_monomorphize_go(monomorphize, ast, subs);
        }
        else if (ast->fexpression.aexp->type == NECRO_AST_VARIABLE && ast->fexpression.aexp->variable.ast_symbol == monomorphize->base->from_int && f_type->type == NECRO_TYPE_FUN && necro_type_find(ast->necro_type)->type == NECRO_TYPE_CON && necro_type_find(ast->necro_type)->con.con_symbol == monomorphize->base->float_type && ast->fexpression.next_fexpression->type == NECRO_AST_CONSTANT)
        {
            *ast            = *necro_ast_create_constant(monomorphize->arena, (NecroParseAstConstant) { .double_literal = (double) ast->fexpression.next_fexpression->constant.int_literal, .type = NECRO_AST_CONSTANT_FLOAT });
            ast->necro_type = monomorphize->base->float_type->type;
            return necro_monomorphize_go(monomorphize, ast, subs);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->fexpression.aexp, subs));
        return necro_monomorphize_go(monomorphize, ast->fexpression.next_fexpression, subs);
    }

    case NECRO_AST_APATS:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->apats.apat, subs));
        return necro_monomorphize_go(monomorphize, ast->apats.next_apat, subs);

    case NECRO_AST_WILDCARD:
        return ok_void();

    case NECRO_AST_LAMBDA:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->lambda.apats, subs));
        return necro_monomorphize_go(monomorphize, ast->lambda.expression, subs);

    case NECRO_AST_DO:
        assert(false && "Not Implemented");
        return ok_void();

    case NECRO_AST_LIST_NODE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->list.item, subs));
        return necro_monomorphize_go(monomorphize, ast->list.next_item, subs);

    case NECRO_AST_EXPRESSION_LIST:
        return necro_monomorphize_go(monomorphize, ast->expression_list.expressions, subs);

    case NECRO_AST_EXPRESSION_ARRAY:
        return necro_monomorphize_go(monomorphize, ast->expression_array.expressions, subs);

    case NECRO_AST_SEQ_EXPRESSION:
    {
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        // tick
        ast->sequence_expression.tick_inst_subs = necro_type_union_subs(monomorphize->arena, monomorphize->base, ast->sequence_expression.tick_inst_subs, subs);
        const bool should_specialize_tick       = necro_try_map_result(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->sequence_expression.tick_symbol, ast, ast->sequence_expression.tick_inst_subs));
        if (should_specialize_tick)
            ast->sequence_expression.tick_symbol = necro_ast_specialize(monomorphize, ast->sequence_expression.tick_symbol, ast->sequence_expression.tick_inst_subs);
        // run
        ast->sequence_expression.run_seq_inst_subs = necro_type_union_subs(monomorphize->arena, monomorphize->base, ast->sequence_expression.run_seq_inst_subs, subs);
        const bool should_specialize_run           = necro_try_map_result(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->sequence_expression.run_seq_symbol, ast, ast->sequence_expression.run_seq_inst_subs));
        if (should_specialize_run)
            ast->sequence_expression.run_seq_symbol = necro_ast_specialize(monomorphize, ast->sequence_expression.run_seq_symbol, ast->sequence_expression.run_seq_inst_subs);
        return necro_monomorphize_go(monomorphize, ast->sequence_expression.expressions, subs);
    }

    case NECRO_AST_TUPLE:
        return necro_monomorphize_go(monomorphize, ast->tuple.expressions, subs);

    case NECRO_BIND_ASSIGNMENT:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        // ast->bind_assignment.ast_symbol->type = necro_try_map_result(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->bind_assignment.ast_symbol->type, subs));
        ast->bind_assignment.ast_symbol->type = necro_type_replace_with_subs_deep_copy(monomorphize->arena, &monomorphize->con_env, monomorphize->base, ast->bind_assignment.ast_symbol->type, subs);
        return necro_monomorphize_go(monomorphize, ast->bind_assignment.expression, subs);

    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->pat_bind_assignment.pat, subs));
        return necro_monomorphize_go(monomorphize, ast->pat_bind_assignment.expression, subs);

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->arithmetic_sequence.from, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->arithmetic_sequence.then, subs));
        return necro_monomorphize_go(monomorphize, ast->arithmetic_sequence.to, subs);

    case NECRO_AST_CASE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->case_expression.expression, subs));
        return necro_monomorphize_go(monomorphize, ast->case_expression.alternatives, subs);

    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->case_alternative.pat, subs));
        return necro_monomorphize_go(monomorphize, ast->case_alternative.body, subs);

    case NECRO_AST_FOR_LOOP:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->for_loop.range_init, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->for_loop.value_init, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->for_loop.index_apat, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->for_loop.value_apat, subs));
        return necro_monomorphize_go(monomorphize, ast->for_loop.expression, subs);

    case NECRO_AST_WHILE_LOOP:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->while_loop.value_init, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->while_loop.value_apat, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->while_loop.while_expression, subs));
        return necro_monomorphize_go(monomorphize, ast->while_loop.do_expression, subs);

    case NECRO_AST_TYPE_APP:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->type_app.ty, subs));
        return necro_monomorphize_go(monomorphize, ast->type_app.next_ty, subs);

    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op_sym.left, subs));
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op_sym.op, subs));
        return necro_monomorphize_go(monomorphize, ast->bin_op_sym.right, subs);

    case NECRO_AST_CONSTRUCTOR:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->constructor.conid, subs));
        return necro_monomorphize_go(monomorphize, ast->constructor.arg_list, subs);

    case NECRO_AST_SIMPLE_TYPE:
        necro_try(void, necro_monomorphize_go(monomorphize, ast->simple_type.type_con, subs));
        return necro_monomorphize_go(monomorphize, ast->simple_type.type_var_list, subs);

    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_FUNCTION_TYPE:
    case NECRO_AST_TYPE_ATTRIBUTE:
        return ok_void();
    default:
        necro_unreachable(void);
    }
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
#define NECRO_MONOMORPHIZE_TEST_VERBOSE 0
void necro_monomorphize_test_result(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
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
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    NecroResult(void) result = necro_monomorphize(info, &intern, &scoped_symtable, &base, &ast);

    // Assert
    if (result.type != expected_result)
    {
        necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    }
#if NECRO_MONOMORPHIZE_TEST_VERBOSE
    if (result.type == expected_result)
        necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    // necro_ast_arena_print(&base.ast);
    necro_ast_arena_print(&ast);
#endif
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
    printf("Monomorphize %s test: %s\n", test_name, result_string);
    fflush(stdout);

    // Clean up
#if NECRO_MONOMORPHIZE_TEST_VERBOSE
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

void necro_monomorphize_test()
{
    necro_announce_phase("Monomorphize");
    // necro_monomorphize_test_suffix();

    // TODO: Compiler breaks down when a Non-Type kinded type is used in a class declaration!
    // {
    //     const char* test_name   = "Instance Declarations 2";
    //     const char* test_source = ""
    //         "class NumCollection c where\n"
    //         "  checkOutMyCollection :: Num a => a -> c a -> c a\n"
    //         "instance NumCollection (Array n) where\n"
    //         "  checkOutMyCollection x c = c\n"
    //         "rationals1 :: Array 1 Float\n"
    //         "rationals1 = checkOutMyCollection 22 {33}\n"
    //         "rationals2 :: Array 2 Float\n"
    //         "rationals2 = checkOutMyCollection 22 {33, 44}\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    {
        const char* test_name   = "WhereGen";
        const char* test_source = ""
            "cat :: Maybe Int\n"
            "cat = nothing where\n"
            "  nothing = Nothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimpleUserPoly";
        const char* test_source = ""
            "data Neither a b = Either a b | Neither\n"
            "polyNeither :: Neither a b\n"
            "polyNeither = Neither\n"
            "concreteEither :: Neither () Bool\n"
            "concreteEither = Either () True\n"
            "concreteNeither :: Neither Int Float\n"
            "concreteNeither = polyNeither\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly1";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteNothing :: Maybe Int\n"
            "concreteNothing = polyNothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly2";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteNothing :: Maybe Int\n"
            "concreteNothing = polyNothing\n"
            "concreteNothing2 :: Maybe Float\n"
            "concreteNothing2 = polyNothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly3";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteTuple :: (Maybe Bool, Maybe Float)\n"
            "concreteTuple = (polyNothing, polyNothing)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoublePoly1";
        const char* test_source = ""
            "polyTuple :: a -> b -> (a, b) \n"
            "polyTuple x y = (x, y)\n"
            "concreteTuple :: ((), Bool)\n"
            "concreteTuple = polyTuple () False\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method1";
        const char* test_source = ""
            "getReal :: Float\n"
            "getReal = 0\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method2";
        const char* test_source = ""
            "unreal :: Float\n"
            "unreal = 10 * 440 / 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context1";
        const char* test_source = ""
            "inContext :: Num a => a -> a -> a\n"
            "inContext x y = x + y * 1000\n"
            "outOfContext :: Int\n"
            "outOfContext = inContext 10 99\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Left Section";
        const char* test_source = ""
            "left' :: Int -> Int \n"
            "left' = (10+)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Right Section";
        const char* test_source = ""
            "right' :: Float -> Float \n"
            "right' = (*33.3)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context2";
        const char* test_source = ""
            "numbernomicon :: Num a => a -> a -> a\n"
            "numbernomicon x y = x + y * 1000\n"
            "atTheIntOfMadness :: Int\n"
            "atTheIntOfMadness = numbernomicon 0 300\n"
            "audioOutOfSpace :: Float\n"
            "audioOutOfSpace = numbernomicon 10 99\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context3";
        const char* test_source = ""
            "equalizer :: (Num a, Eq a) => a -> a -> Bool\n"
            "equalizer x y = x + y == x\n"
            "zero' :: Int\n"
            "zero' = 0\n"
            "oneHundred :: Int\n"
            "oneHundred = 1000\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer zero' oneHundred\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoubleContext";
        const char* test_source = ""
            "doubleTrouble :: (Num a, Num b) => a -> b -> (a, b)\n"
            "doubleTrouble x y = (x - 1000, y + 1000)\n"
            "doubleEdge :: (Float, Float)\n"
            "doubleEdge = doubleTrouble 22 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "HalfContext";
        const char* test_source = ""
            "halfTrouble :: Num a => a -> Int -> (a, Int)\n"
            "halfTrouble x y = (x - 1000, y + 1000)\n"
            "halfEdge :: (Float, Int)\n"
            "halfEdge = halfTrouble 22.2 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DeepContext";
        const char* test_source = ""
            "deep :: Floating a => a -> a\n"
            "deep x = x / x\n"
            "shallow :: (Floating a, Floating b) => a -> b -> (a, b)\n"
            "shallow y z = (deep y, deep z)\n"
            "top :: (Float, Float)\n"
            "top = shallow 22.2 33.3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Dont Generalize Let";
        const char* test_source = ""
            "data Alive = Alive\n"
            "data Dead  = Dead\n"
            "cat :: (Maybe Alive, Maybe Dead)\n"
            "cat = (alive, dead) where\n"
            "  schroedingersMaybe :: Maybe a\n"
            "  schroedingersMaybe = Nothing\n"
            "  alive              = schroedingersMaybe\n"
            "  dead               = schroedingersMaybe\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "No Monomorphism Restriction 2";
        const char* test_source = ""
            "notMonoFucker = add\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic Data Type 1";
        const char* test_source = ""
            "data Thing a = Thing a\n"
            "addThing :: Num a => Thing a -> Thing a -> Thing a\n"
            "addThing (Thing x) (Thing y) = Thing (x + y)\n"
            "thingInt :: Num a => Int -> Thing a\n"
            "thingInt x = Thing (fromInt x)\n"
            "intThing :: Thing Int\n"
            "intThing = addThing (thingInt 1) (thingInt 2)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic Data Type 2";
        const char* test_source = ""
            "tupleAdd :: (Num a, Num b) => (a, b) -> (a, b) -> (a, b)\n"
            "tupleAdd (xa, xb) (ya, yb) = (xa + ya, xb + yb)\n"
            "tupleFromInt :: (Num a, Num b) => Int -> (a, b)\n"
            "tupleFromInt x = (fromInt x, fromInt x)\n"
            "tupleIntFloat :: (Int, Float)\n"
            "tupleIntFloat = tupleAdd (tupleFromInt 1) (tupleFromInt 2)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic methods 1";
        const char* test_source = ""
            "data Thing a = Thing a\n"
            "instance Semiring a => Semiring (Thing a) where\n"
            "  zero                    = Thing zero\n"
            "  one                     = Thing one\n"
            "  add (Thing x) (Thing y) = Thing (x + y)\n"
            "  mul (Thing x) (Thing y) = Thing (x * y)\n"
            "instance Ring a => Ring (Thing a) where\n"
            "  sub (Thing x) (Thing y) = Thing (x - y)\n"
            "  fromInt i               = Thing (fromInt i)\n"
            "intThing :: Thing Int\n"
            "intThing = 10 + 33 * 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

// TODO: Replace once Seq is working
/*
    {
        const char* test_name   = "Polymorphic methods 1";
        const char* test_source = ""
            "audioPat :: Seq Float\n"
            "audioPat = 3.3 / 4 * 22.2\n"
            "floatPat :: Seq Float\n"
            "floatPat = 3.3 / 4 * 22.2\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }
*/

    {
        const char* test_name   = "Instance Chains 1";
        const char* test_source = ""
            "class UberClass u where\n"
            "  uberMensch :: Num n => u -> n\n"
            "class (UberClass s) => SuperClass s where\n"
            "  superMensch :: Num n => s -> n\n"
            "class (SuperClass c) => NoClass c where\n"
            "  mensch :: Num n => c -> n\n"
            "instance UberClass Int where\n"
            "  uberMensch x = fromInt x\n"
            "instance SuperClass Int where\n"
            "  superMensch x = uberMensch x\n"
            "instance NoClass Int where\n"
            "  mensch x = superMensch x\n"
            "iAmNotAMonster :: Float\n"
            "iAmNotAMonster = mensch 1\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 1";
        const char* test_source = ""
            "amb :: Float\n"
            "amb = const 22 33.0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 2";
        const char* test_source = ""
            "eqconst :: Eq b => a -> Maybe b -> a\n"
            "eqconst x y = x\n"
            "amb :: Bool\n"
            "amb = eqconst True Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 3";
        const char* test_source = ""
            "equalizer :: (Eq a) => a -> a -> Bool\n"
            "equalizer x y = x == x\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer 0.5 100\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defaulting 4";
        const char* test_source = ""
            "class NotUnit a where\n"
            "equalizer :: (Eq a) => Maybe a -> Maybe a -> Bool\n"
            "equalizer x y = True\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer Nothing Nothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Ambiguous Type Var 1";
        const char* test_source = ""
            "maybeNothing :: Maybe c\n"
            "maybeNothing = Nothing\n"
            "amb :: Int\n"
            "amb = const 22 Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_TYPE_VAR;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Ambiguous Type Var 2";
        const char* test_source = ""
            "class BS a where\n"
            "maybeNothing :: Maybe c\n"
            "maybeNothing = Nothing\n"
            "constBS :: (Num b, BS b) => a -> b -> a\n"
            "constBS x y = x\n"
            "amb :: Int\n"
            "amb = constBS 22 0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_TYPE_VAR;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "Zero Order Recursive Value";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing ~ Nothing = polyNothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Initialized Non-Recursive Value";
        const char* test_source = ""
            "nonRec ~ True = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // NOTE: Recrusive functions are currently a type error
    // {
    //     const char* test_name   = "Mutual Recursion 2";
    //     const char* test_source = ""
    //         "rec1 :: a -> b -> b\n"
    //         "rec1 x y = rec2 True y\n"
    //         "rec2 :: a -> b -> b\n"
    //         "rec2 x y = rec1 x y\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // {
    //     const char* test_name   = "Mutual Recursion 3";
    //     const char* test_source = ""
    //         "rec1 :: a -> b -> b\n"
    //         "rec1 x y = rec2 True y\n"
    //         "rec2 :: a -> b -> b\n"
    //         "rec2 x y = rec1 () y\n"
    //         "go :: Int\n"
    //         "go = rec1 (Just ()) 2\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    {
        const char* test_name   = "Pattern Assignment 1";
        const char* test_source = ""
            "(left', right') = (1, 2.0)\n"
            "leftPlus :: Float -> Float\n"
            "leftPlus x = left'\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 2";
        const char* test_source = ""
            "fstP:: (a, b) -> a\n"
            "fstP x = left' where\n"
            "  (left', _) = x\n"
            "unitFirst :: ()\n"
            "unitFirst = fstP ((), True)\n"
            "intFirst :: Int\n"
            "intFirst = fstP (0, ())\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 2";
        const char* test_source = ""
            "leftRight = (left', right') where\n"
            "  (left', right') = (1, 2.0)\n"
            "intFloat :: (Int, Float)\n"
            "intFloat = leftRight\n"
            "rationalFloat :: (Float, Float)\n"
            "rationalFloat = leftRight\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Recursive Pattern Assignment 1";
        const char* test_source = ""
            "data Pair a b = Pair a b\n"
            "Pair (leftOsc ~ 0) _ = Pair (leftOsc + 1) True\n"
            "leftPlus :: Int\n"
            "leftPlus = leftOsc + 1\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Recursive Value 1";
        const char* test_source = ""
            "coolOsc :: Float\n"
            "coolOsc ~ 0 = coolOsc + 1\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Recursive Value 2";
        const char* test_source = ""
            "coolOsc :: (Default a, Num a) => a\n"
            "coolOsc ~ default = coolOsc + 1\n"
            "coolInt :: Int\n"
            "coolInt = coolOsc\n"
            "coolFloat :: Float\n"
            "coolFloat = coolOsc\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Array is Ok 2";
        const char* test_source = ""
            "dropOne :: Array n a -> Array n a -> Array n a\n"
            "dropOne x _ = x\n"
            "arr1 = { 0, 1, 2, 3 }\n"
            "arr2 = { 3, 2, 1, 0 }\n"
            "one' :: Array 4 Int\n"
            "one' = dropOne arr1 arr2\n"
            "arr3 = { 0, 1, 2, 3, 4 }\n"
            "arr4 = { 4, 3, 2, 1, 0 }\n"
            "three :: Array 5 Int\n"
            "three = dropOne arr3 arr4\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defunctionalization 1";
        const char* test_source = ""
            "pipe :: a -> (a -> b) -> b\n"
            "pipe x f = f x\n"
            "dopeSmoker :: Float\n"
            "dopeSmoker = pipe 100 (mul 100)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

// TODO: Remove 'order' from TypeVars!
/*
    {
        const char* test_name   = "Defunctionalization 2";
        const char* test_source = ""
            "fst :: (^a, b) -> ^a\n"
            "fst (x, _) = x\n"
            "fstOfItsKind :: Int -> Int -> Int\n"
            "fstOfItsKind = fst (mul, 0)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }
*/

    {
        const char* test_name   = "String Test 1";
        const char* test_source = ""
            "helloWorld :: Array 11 Char\n"
            "helloWorld = \"Hello World\"\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // TODO: Look into Tuples with initializers in parser
    // {
    //     const char* test_name   = "Recursive Pattern Assignment 1";
    //     const char* test_source = ""
    //         "(leftOsc ~ 0, _) = (leftOsc + 1, True)\n"
    //         "leftPlus :: Int\n"
    //         "leftPlus = leftOsc + 1\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // // TODO (Curtis 2-22-19): Pattern Assignment with Initializers with Numeric literals seems a little strange...
    // // TODO: check that default method is constant somehow...
    // {
    //     const char* test_name   = "Recursive Pattern Assignment 1";
    //     const char* test_source = ""
    //         "data Wrapper a = Wrapper a\n"
    //         "Wrapper (wrappedOsc ~ default) = Wrapper (wrappedOsc + 1)\n"
    //         "plusOne :: Float\n"
    //         "plusOne = wrappedOsc + 1\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // NOTE: Multi-line Definitions are currently removed.
    // {
    //     const char* test_name   = "Multi-line Function";
    //     const char* test_source = ""
    //         "skateOrDie (Just _) = True\n"
    //         "skateOrDie _        = False\n"
    //         "die                 = skateOrDie (Just ())\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    // }
}

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
    };
    return monomorphize;
}

void necro_monomorphize_destroy(NecroMonomorphize* monomorphize)
{
    assert(monomorphize != NULL);
    necro_snapshot_arena_destroy(&monomorphize->snapshot_arena);
    *monomorphize = necro_monomorphize_empty();
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroResult(void) necro_monomorphize_go(NecroMonomorphize* monomorphize, NecroAst* ast, NecroInstSub* subs);
void              necro_monomorphize_type_go(NecroMonomorphize* monomorphize, NecroAst* ast);
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
        if (info.compilation_phase == NECRO_PHASE_MONOMORPHIZE && info.verbosity > 0)
            necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    necro_monomorphize_destroy(&monomorphize);
    return result;
}

///////////////////////////////////////////////////////
// SpecializeAst
///////////////////////////////////////////////////////
size_t necro_type_mangle_subs_recursive(char* suffix_buffer, size_t offset, const NecroInstSub* sub)
{
    if (sub == NULL)
        return offset;
    offset = necro_type_mangle_subs_recursive(suffix_buffer, offset, sub->next);
    if (sub->next != NULL)
        offset += sprintf(suffix_buffer + offset, ",");
    offset = necro_type_mangled_sprintf(suffix_buffer, offset, sub->new_name);
    return offset;
}

NecroSymbol necro_create_suffix_from_subs(NecroMonomorphize* monomorphize, NecroSymbol prefix, const NecroInstSub* subs)
{
    // Count length
    const size_t        prefix_length = strlen(prefix->str);
    size_t              length        = 2 + prefix_length;
    const NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        length += necro_type_mangled_string_length(curr_sub->new_name);
        if (curr_sub->next != NULL)
            length += 1;
        curr_sub = curr_sub->next;
    }

    // Alloc suffix buffer
    NecroArenaSnapshot snapshot      = necro_snapshot_arena_get(&monomorphize->snapshot_arena);
    char*              suffix_buffer = necro_snapshot_arena_alloc(&monomorphize->snapshot_arena, length);

    // Write type strings
    size_t offset = 0;
    if (prefix->str[prefix_length - 1] == '>')
    {
        // Name has already been overloaded for a type class, instead append to type list.
        offset = sprintf(suffix_buffer, "%s", prefix->str);
        suffix_buffer[prefix_length - 1] = ',';
        length -= 1;
    }
    else
    {
        offset = sprintf(suffix_buffer, "%s<", prefix->str);
    }
    offset        = necro_type_mangle_subs_recursive (suffix_buffer, offset, subs);
    offset       += sprintf(suffix_buffer + offset, ">");
    assert(offset == length);

    // Intern, clean up, return
    NecroSymbol string_symbol = necro_intern_string(monomorphize->intern, suffix_buffer);
    necro_snapshot_arena_rewind(&monomorphize->snapshot_arena, snapshot);
    return string_symbol;
}

NecroInstSub* necro_type_create_instance_subs(NecroMonomorphize* monomorphize, NecroTypeClass* type_class, NecroInstSub* subs)
{
    // Find Sub
    NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        NecroType* sub_type = necro_type_find(curr_sub->new_name);
        if (curr_sub->var_to_replace == type_class->type_var)
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
                    NecroType*              instance_type = unwrap(NecroType, necro_type_instantiate_with_subs(monomorphize->arena, monomorphize->base, instance->data_type, NULL, &instance_subs));
                    unwrap(NecroType, necro_type_unify(monomorphize->arena, monomorphize->base, instance_type, sub_type, NULL));
                    NecroInstSub*           new_subs      = necro_type_filter_and_deep_copy_subs(monomorphize->arena, subs, type_class->type_class_name, instance_type);
                    new_subs                              = necro_type_union_subs(new_subs, instance_subs);
                    if (new_subs != NULL)
                    {
                        NecroInstSub* curr_new_sub = new_subs;
                        while (curr_new_sub->next != NULL)
                        {
                            curr_new_sub = curr_new_sub->next;
                        }
                        curr_new_sub->next = instance_subs;
                        new_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, new_subs, type_class->type_var, instance_type);
                        return new_subs;
                    }
                    else
                    {
                        instance_subs = necro_type_filter_and_deep_copy_subs(monomorphize->arena, instance_subs, type_class->type_class_name, instance_type);
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
        if (curr_sub->var_to_replace->name == type_class_var->name)
        {
            NecroType* sub_con = necro_type_find(curr_sub->new_name);
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
    // Find specialized ast
    //--------------------
    NecroSymbol     specialized_name       = necro_create_suffix_from_subs(monomorphize, ast_symbol->source_name, subs);
    NecroScope*     scope                  = ast_symbol->ast->scope;
    NecroAstSymbol* specialized_ast_symbol = necro_scope_find_ast_symbol(scope, specialized_name);
    if (specialized_ast_symbol != NULL)
    {
        assert(specialized_ast_symbol->ast != NULL);
        return specialized_ast_symbol;
    }

    //--------------------
    // Find DeclarationGroupList, Make new DeclarationGroup
    //--------------------
    NecroAst* declaration_group = ast_symbol->declaration_group;

    assert(declaration_group != NULL);
    assert(declaration_group->type == NECRO_AST_DECL);
    NecroAst* new_declaration                           = necro_ast_create_decl(monomorphize->arena, ast_symbol->ast, declaration_group->declaration.next_declaration);
    declaration_group->declaration.next_declaration     = new_declaration;
    new_declaration->declaration.declaration_group_list = declaration_group->declaration.declaration_group_list;
    new_declaration->declaration.declaration_impl       = NULL; // Removing dummy argument

    //--------------------
    // Deep Copy Ast
    //--------------------
    specialized_ast_symbol                        = necro_ast_symbol_create(monomorphize->arena, specialized_name, specialized_name, monomorphize->ast_arena->module_name, NULL);
    specialized_ast_symbol->declaration_group     = new_declaration;
    specialized_ast_symbol->ast                   = necro_ast_deep_copy_go(monomorphize->arena, new_declaration, ast_symbol->ast);
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
    NecroScope* prev_scope                                    = monomorphize->scoped_symtable->current_scope;
    NecroScope* prev_type_scope                               = monomorphize->scoped_symtable->current_type_scope;
    monomorphize->scoped_symtable->current_scope      = scope;
    necro_rename_internal_scope_and_rename(monomorphize->ast_arena, monomorphize->scoped_symtable, monomorphize->intern, specialized_ast_symbol->ast);
    monomorphize->scoped_symtable->current_scope      = prev_scope;
    monomorphize->scoped_symtable->current_type_scope = prev_type_scope;

    //--------------------
    // Specialize Ast
    //--------------------
    specialized_ast_symbol->type = unwrap(NecroType, necro_type_replace_with_subs_deep_copy(monomorphize->arena, monomorphize->base, ast_symbol->type, subs));
    unwrap(void, necro_monomorphize_go(monomorphize, specialized_ast_symbol->ast, subs));

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
    const bool is_symbol_poly    = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, ast_symbol->type, ast_symbol->type, ast->source_loc, ast->end_loc));
    const bool is_ast_poly       = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
    bool       should_specialize = is_symbol_poly && !is_ast_poly;
    NecroInstSub* curr_sub    = inst_subs;
    while (curr_sub != NULL)
    {
        const bool is_curr_sub_poly = necro_try(bool, necro_type_is_unambiguous_polymorphic(arena, base, curr_sub->new_name, curr_sub->new_name, ast->source_loc, ast->end_loc));
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
            necro_try_map(NecroType, void, necro_type_set_zero_order(ast->variable.ast_symbol->type, &ast->source_loc, &ast->end_loc));
            // return necro_type_non_concrete_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        if (ast->variable.initializer != NULL && !ast->variable.is_recursive)
        {
            return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
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
    ast->necro_type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->necro_type, subs));
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_TOP_DECL:
        necro_unreachable(void);

    case NECRO_AST_DECLARATION_GROUP_LIST:
    {
        NecroAst* group_list = ast;
        while (group_list != NULL)
        {
            necro_try(void, necro_monomorphize_go(monomorphize, group_list->declaration_group_list.declaration_group, subs));
            group_list = group_list->declaration_group_list.next;
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
        ast->simple_assignment.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->simple_assignment.ast_symbol->type, subs));
        if (ast->simple_assignment.initializer != NULL)
        {
            necro_try_map(NecroType, void, necro_type_set_zero_order(ast->necro_type, &ast->source_loc, &ast->end_loc));
        }
        if (ast->simple_assignment.initializer != NULL && !ast->simple_assignment.is_recursive)
        {
            return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->simple_assignment.initializer, subs));
        return necro_monomorphize_go(monomorphize, ast->simple_assignment.rhs, subs);

    case NECRO_AST_APATS_ASSIGNMENT:
        ast->apats_assignment.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->apats_assignment.ast_symbol->type, subs));
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
            ast->variable.inst_subs      = necro_type_union_subs(ast->variable.inst_subs, subs);
            const bool should_specialize = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->variable.ast_symbol, ast, ast->variable.inst_subs));
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
                ast->variable.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->variable.ast_symbol->type, subs));
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
        ast->bin_op.inst_subs        = necro_type_union_subs(ast->bin_op.inst_subs, subs);
        const bool should_specialize = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->bin_op.ast_symbol, ast, ast->bin_op.inst_subs));
        if (should_specialize)
        {
            ast->bin_op.ast_symbol = necro_ast_specialize(monomorphize, ast->bin_op.ast_symbol, ast->bin_op.inst_subs);
        }
        necro_try(void, necro_monomorphize_go(monomorphize, ast->bin_op.lhs, subs));
        return necro_monomorphize_go(monomorphize, ast->bin_op.rhs, subs);
    }

    case NECRO_AST_OP_LEFT_SECTION:
    {
        ast->op_left_section.inst_subs = necro_type_union_subs(ast->op_left_section.inst_subs, subs);
        const bool should_specialize   = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->op_left_section.ast_symbol, ast, ast->op_left_section.inst_subs));
        if (should_specialize)
        {
            ast->op_left_section.ast_symbol = necro_ast_specialize(monomorphize, ast->op_left_section.ast_symbol, ast->op_left_section.inst_subs);
        }
        return necro_monomorphize_go(monomorphize, ast->op_left_section.left, subs);
    }

    case NECRO_AST_OP_RIGHT_SECTION:
    {
        ast->op_right_section.inst_subs = necro_type_union_subs(ast->op_right_section.inst_subs, subs);
        const bool should_specialize    = necro_try_map(bool, void, necro_ast_should_specialize(monomorphize->arena, monomorphize->base, ast->op_right_section.ast_symbol, ast, ast->op_right_section.inst_subs));
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
        necro_try(void, necro_monomorphize_go(monomorphize, ast->fexpression.next_fexpression, subs));
        return necro_monomorphize_go(monomorphize, ast->fexpression.aexp, subs);

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

    case NECRO_AST_PAT_EXPRESSION:
        return necro_monomorphize_go(monomorphize, ast->pattern_expression.expressions, subs);

    case NECRO_AST_TUPLE:
        return necro_monomorphize_go(monomorphize, ast->tuple.expressions, subs);

    case NECRO_BIND_ASSIGNMENT:
        necro_try(void, necro_type_ambiguous_type_variable_check(monomorphize->arena, monomorphize->base, ast->necro_type, ast->necro_type, ast->source_loc, ast->end_loc));
        ast->bind_assignment.ast_symbol->type = necro_try_map(NecroType, void, necro_type_replace_with_subs(monomorphize->arena, monomorphize->base, ast->bind_assignment.ast_symbol->type, subs));
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
// Specialize Type
///////////////////////////////////////////////////////
// NOTE: Should we be doing in this in core before NecroMach translation instead?
NecroType* necro_specialize_type(NecroMonomorphize* monomorphize, NecroType* type)
{
    if (type == NULL || necro_type_is_polymorphic(type))
        return type;
    type = necro_type_find(type);
    switch (type->type)
    {

    case NECRO_TYPE_CON:
    {

        //--------------------
        // If there's no args, we don't need to be specialized
        //--------------------
        if (type->con.args == NULL)
            return type;

        //--------------------
        // Create Name and Lookup (probably a faster way of doing this, such as hashing types directly, instead of their names?)
        //--------------------
        // TODO: Name's should use fully qualified name, not source name!
        NecroArenaSnapshot snapshot                     = necro_snapshot_arena_get(&monomorphize->snapshot_arena);
        const size_t       specialized_type_name_length = necro_type_mangled_string_length(type);
        char*              specialized_type_name_buffer = necro_snapshot_arena_alloc(&monomorphize->snapshot_arena, specialized_type_name_length * sizeof(char));
        necro_type_mangled_sprintf(specialized_type_name_buffer, 0, type);
        const NecroSymbol  specialized_type_source_name = necro_intern_string(monomorphize->intern, specialized_type_name_buffer);
        NecroAstSymbol*    specialized_type_ast_symbol  = necro_scope_find_ast_symbol(monomorphize->scoped_symtable->top_type_scope, specialized_type_source_name);
        if (specialized_type_ast_symbol != NULL)
        {
            assert(specialized_type_ast_symbol->type != NULL);
            assert(specialized_type_ast_symbol->type->type == NECRO_TYPE_CON);
            assert(specialized_type_ast_symbol->type->con.args == NULL);
            return specialized_type_ast_symbol->type;
        }

        //--------------------
        // Create Specialized Type's NecroAstSymbol
        //--------------------
        // const NecroSymbol specialized_type_name = necro_intern_concat_symbols(monomorphize->intern, monomorphize->ast_arena->module_name, specialized_type_source_name);
        specialized_type_ast_symbol             = necro_ast_symbol_create(monomorphize->arena, specialized_type_source_name, specialized_type_source_name, monomorphize->ast_arena->module_name, NULL);
        specialized_type_ast_symbol->type       = necro_type_con_create(monomorphize->arena, specialized_type_ast_symbol, NULL);
        specialized_type_ast_symbol->type->kind = monomorphize->base->star_kind->type;
        necro_scope_insert_ast_symbol(monomorphize->arena, monomorphize->scoped_symtable->top_type_scope, specialized_type_ast_symbol);

        //--------------------
        // Specialize args
        //--------------------
        NecroType* args = type->con.args;
        while (args != NULL)
        {
            necro_specialize_type(monomorphize, args->list.item);
            args = args->list.next;
        }

        //--------------------
        // New Declaration in old DeclarationGroupList
        //--------------------
        NecroAstSymbol* original_type_ast_symbol            = type->con.con_symbol;
        NecroAst*       declaration_group                   = original_type_ast_symbol->ast->data_declaration.declaration_group;
        NecroAst*       new_declaration                     = necro_ast_create_decl(monomorphize->arena, original_type_ast_symbol->ast, declaration_group->declaration.next_declaration);
        declaration_group->declaration.next_declaration     = new_declaration;
        new_declaration->declaration.declaration_group_list = declaration_group->declaration.declaration_group_list;
        new_declaration->declaration.declaration_impl       = NULL; // Removing dummy argument

        //--------------------
        // Create specialized type suffix
        //--------------------
        char* specialized_type_suffix_buffer = specialized_type_name_buffer;
        while (*specialized_type_suffix_buffer != '<')
            specialized_type_suffix_buffer++;
        NecroSymbol specialized_type_suffix_symbol = necro_intern_string(monomorphize->intern, specialized_type_suffix_buffer);

        //--------------------
        // Specialize Data Declaration
        //--------------------
        NecroAst* specialized_type_ast       = necro_ast_deep_copy_go(monomorphize->arena, new_declaration, original_type_ast_symbol->ast);
        NecroAst* specialized_type_data_cons = specialized_type_ast->data_declaration.constructor_list;
        while (specialized_type_data_cons != NULL)
        {
            // New AstSymbol
            NecroAst*       data_con                      = specialized_type_data_cons->list.item;
            NecroAstSymbol* old_data_con_symbol           = data_con->constructor.conid->conid.ast_symbol;
            NecroSymbol     new_data_con_source_name      = necro_intern_concat_symbols(monomorphize->intern, old_data_con_symbol->source_name, specialized_type_suffix_symbol);
            // NecroSymbol     new_data_con_name             = necro_intern_concat_symbols(monomorphize->intern, monomorphize->ast_arena->module_name, new_data_con_source_name);
            NecroAstSymbol* new_data_con_symbol           = necro_ast_symbol_create(monomorphize->arena, new_data_con_source_name, new_data_con_source_name, monomorphize->ast_arena->module_name, data_con);
            data_con->constructor.conid->conid.ast_symbol = new_data_con_symbol;
            data_con->constructor.arg_list                = NULL;
            // Specialize Type
            NecroType*      data_con_type                 = unwrap(NecroType, necro_type_instantiate(monomorphize->arena, monomorphize->base, data_con->necro_type, NULL));
            NecroType*      data_con_result               = data_con_type;
            while (data_con_result->type == NECRO_TYPE_FUN)
                data_con_result = data_con_result->fun.type2;
            unwrap(NecroType, necro_type_unify(monomorphize->arena, monomorphize->base, data_con_result, type, NULL));
            NecroType*      specialized_data_con_type     = necro_specialize_type(monomorphize, data_con_type);
            unwrap(void, necro_kind_infer_default_unify_with_star(monomorphize->arena, monomorphize->base, specialized_data_con_type, NULL, NULL_LOC, NULL_LOC));
            new_data_con_symbol->type                     = specialized_data_con_type;
            data_con->necro_type                          = specialized_data_con_type;
            necro_scope_insert_ast_symbol(monomorphize->arena, monomorphize->scoped_symtable->top_scope, new_data_con_symbol);
            specialized_type_data_cons                    = specialized_type_data_cons->list.next_item;
        }
        specialized_type_ast->data_declaration.simpletype->simple_type.type_var_list = NULL;

        //--------------------
        // Clean up and return
        //--------------------
        necro_snapshot_arena_rewind(&monomorphize->snapshot_arena, snapshot);
        return specialized_type_ast_symbol->type;
    }

    case NECRO_TYPE_FUN:
    {
        NecroType* type1 = necro_specialize_type(monomorphize, type->fun.type1);
        NecroType* type2 = necro_specialize_type(monomorphize, type->fun.type2);
        if (type1 == type->fun.type1 && type2 == type->fun.type2)
            return type;
        else
            return necro_type_fn_create(monomorphize->arena, type1, type2);
    }

    case NECRO_TYPE_APP:
        return necro_specialize_type(monomorphize, necro_type_uncurry_app(monomorphize->arena, monomorphize->base, type));

    // Ignore
    case NECRO_TYPE_VAR:
        return type;
    case NECRO_TYPE_FOR:
        return type;
    case NECRO_TYPE_NAT:
        return type;
    case NECRO_TYPE_SYM:
        return type;

    case NECRO_TYPE_LIST: /* FALL THROUGH */
    default:
        assert(false);
        return NULL;
    }
}

///////////////////////////////////////////////////////
// Monomorphize Type Go
///////////////////////////////////////////////////////
void necro_monomorphize_type_go(NecroMonomorphize* monomorphize, NecroAst* ast)
{
    if (ast == NULL)
        return;
    NecroType* original_type = ast->necro_type;
    ast->necro_type          = necro_specialize_type(monomorphize, ast->necro_type);
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_DECLARATION_GROUP_LIST:
    {
        NecroAst* group_list = ast;
        while (group_list != NULL)
        {
            necro_monomorphize_type_go(monomorphize, group_list->declaration_group_list.declaration_group);
            group_list = group_list->declaration_group_list.next;
        }
        return;
    }

    case NECRO_AST_DECL:
    {
        NecroAst* declaration_group = ast;
        while (declaration_group != NULL)
        {
            necro_monomorphize_type_go(monomorphize, declaration_group->declaration.declaration_impl);
            declaration_group = declaration_group->declaration.next_declaration;
        }
        return;
    }

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        ast->simple_assignment.ast_symbol->type = necro_specialize_type(monomorphize, ast->simple_assignment.ast_symbol->type);
        necro_monomorphize_type_go(monomorphize, ast->simple_assignment.initializer);
        necro_monomorphize_type_go(monomorphize, ast->simple_assignment.rhs);
        return;

    case NECRO_AST_APATS_ASSIGNMENT:
        ast->apats_assignment.ast_symbol->type = necro_specialize_type(monomorphize, ast->apats_assignment.ast_symbol->type);
        necro_monomorphize_type_go(monomorphize, ast->apats_assignment.apats);
        necro_monomorphize_type_go(monomorphize, ast->apats_assignment.rhs);
        return;

    case NECRO_BIND_ASSIGNMENT:
        ast->bind_assignment.ast_symbol->type = necro_specialize_type(monomorphize, ast->bind_assignment.ast_symbol->type);
        necro_monomorphize_type_go(monomorphize, ast->bind_assignment.expression);
        return;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_DECLARATION:
            if (ast->variable.ast_symbol != NULL)
                ast->variable.ast_symbol->type = necro_specialize_type(monomorphize, ast->variable.ast_symbol->type);
            if (ast->variable.initializer != NULL)
                necro_monomorphize_type_go(monomorphize, ast->variable.initializer);
            return;
        case NECRO_VAR_VAR:                  return;
        case NECRO_VAR_SIG:                  return;
        case NECRO_VAR_TYPE_VAR_DECLARATION: return;
        case NECRO_VAR_TYPE_FREE_VAR:        return;
        case NECRO_VAR_CLASS_SIG:            return;
        default:
            assert(false);
            return;
        }

    case NECRO_AST_CONID:
        if (necro_type_find(ast->necro_type) != necro_type_find(original_type))
        {
            // Specialize Type
            NecroType* curr_type = ast->necro_type;
            while (curr_type->type == NECRO_TYPE_FUN)
                curr_type = necro_type_find(curr_type->fun.type2);
            assert(curr_type->type == NECRO_TYPE_CON);
            //--------------------
            // Create specialized type suffix
            //--------------------
            // TODO: Name's should use fully qualified name, not source name!
            const char* specialized_type_suffix_buffer = curr_type->con.con_symbol->name->str;
            while (*specialized_type_suffix_buffer != '<')
                specialized_type_suffix_buffer++;
            NecroSymbol     specialized_type_suffix_symbol = necro_intern_string(monomorphize->intern, specialized_type_suffix_buffer);
            NecroSymbol     specialized_type_con_name      = necro_intern_concat_symbols(monomorphize->intern, ast->conid.ast_symbol->source_name, specialized_type_suffix_symbol);
            NecroAstSymbol* specialized_type_con_symbol    = necro_scope_find_ast_symbol(monomorphize->scoped_symtable->top_scope, specialized_type_con_name);
            ast->conid.ast_symbol                          = specialized_type_con_symbol;
            assert(specialized_type_con_symbol != NULL);
        }
        return;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        necro_monomorphize_type_go(monomorphize, ast->type_class_instance.declarations);
        return;
    case NECRO_AST_PAT_ASSIGNMENT:
        necro_monomorphize_type_go(monomorphize, ast->pat_assignment.pat);
        necro_monomorphize_type_go(monomorphize, ast->pat_assignment.rhs);
        return;
    case NECRO_AST_BIN_OP:
        necro_monomorphize_type_go(monomorphize, ast->bin_op.lhs);
        necro_monomorphize_type_go(monomorphize, ast->bin_op.rhs);
        return;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_monomorphize_type_go(monomorphize, ast->op_left_section.left);
        return;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_monomorphize_type_go(monomorphize, ast->op_right_section.right);
        return;
    case NECRO_AST_IF_THEN_ELSE:
        necro_monomorphize_type_go(monomorphize, ast->if_then_else.if_expr);
        necro_monomorphize_type_go(monomorphize, ast->if_then_else.then_expr);
        necro_monomorphize_type_go(monomorphize, ast->if_then_else.else_expr);
        return;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_monomorphize_type_go(monomorphize, ast->right_hand_side.declarations);
        necro_monomorphize_type_go(monomorphize, ast->right_hand_side.expression);
        return;
    case NECRO_AST_LET_EXPRESSION:
        necro_monomorphize_type_go(monomorphize, ast->let_expression.declarations);
        necro_monomorphize_type_go(monomorphize, ast->let_expression.expression);
        return;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_monomorphize_type_go(monomorphize, ast->fexpression.next_fexpression);
        necro_monomorphize_type_go(monomorphize, ast->fexpression.aexp);
        return;
    case NECRO_AST_APATS:
        necro_monomorphize_type_go(monomorphize, ast->apats.apat);
        necro_monomorphize_type_go(monomorphize, ast->apats.next_apat);
        return;
    case NECRO_AST_WILDCARD:
        return;
    case NECRO_AST_LAMBDA:
        necro_monomorphize_type_go(monomorphize, ast->lambda.apats);
        necro_monomorphize_type_go(monomorphize, ast->lambda.expression);
        return;
    case NECRO_AST_DO:
        assert(false && "Not Implemented");
        return;
    case NECRO_AST_LIST_NODE:
        necro_monomorphize_type_go(monomorphize, ast->list.item);
        necro_monomorphize_type_go(monomorphize, ast->list.next_item);
        return;
    case NECRO_AST_EXPRESSION_LIST:
        necro_monomorphize_type_go(monomorphize, ast->expression_list.expressions);
        return;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_monomorphize_type_go(monomorphize, ast->expression_array.expressions);
        return;
    case NECRO_AST_PAT_EXPRESSION:
        necro_monomorphize_type_go(monomorphize, ast->pattern_expression.expressions);
        return;
    case NECRO_AST_TUPLE:
        necro_monomorphize_type_go(monomorphize, ast->tuple.expressions);
        return;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_monomorphize_type_go(monomorphize, ast->pat_bind_assignment.pat);
        necro_monomorphize_type_go(monomorphize, ast->pat_bind_assignment.expression);
        return;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_monomorphize_type_go(monomorphize, ast->arithmetic_sequence.from);
        necro_monomorphize_type_go(monomorphize, ast->arithmetic_sequence.then);
        necro_monomorphize_type_go(monomorphize, ast->arithmetic_sequence.to);
        return;
    case NECRO_AST_CASE:
        necro_monomorphize_type_go(monomorphize, ast->case_expression.expression);
        necro_monomorphize_type_go(monomorphize, ast->case_expression.alternatives);
        return;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_monomorphize_type_go(monomorphize, ast->case_alternative.pat);
        necro_monomorphize_type_go(monomorphize, ast->case_alternative.body);
        return;
    case NECRO_AST_TYPE_APP:
        necro_monomorphize_type_go(monomorphize, ast->type_app.ty);
        necro_monomorphize_type_go(monomorphize, ast->type_app.next_ty);
        return;
    case NECRO_AST_BIN_OP_SYM:
        necro_monomorphize_type_go(monomorphize, ast->bin_op_sym.left);
        necro_monomorphize_type_go(monomorphize, ast->bin_op_sym.op);
        necro_monomorphize_type_go(monomorphize, ast->bin_op_sym.right);
        return;
    case NECRO_AST_CONSTRUCTOR:
        necro_monomorphize_type_go(monomorphize, ast->constructor.conid);
        necro_monomorphize_type_go(monomorphize, ast->constructor.arg_list);
        return;
    case NECRO_AST_SIMPLE_TYPE:
        necro_monomorphize_type_go(monomorphize, ast->simple_type.type_con);
        necro_monomorphize_type_go(monomorphize, ast->simple_type.type_var_list);
        return;

    case NECRO_AST_TOP_DECL:
    case NECRO_AST_UNDEFINED:
    case NECRO_AST_CONSTANT:
    case NECRO_AST_UN_OP:
    case NECRO_AST_DATA_DECLARATION:
    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_FUNCTION_TYPE:
        return;
    default:
        assert(false);
        return;
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
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
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
    necro_dependency_analyze(info, &intern, &ast);
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
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_monomorphize_test_suffix()
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroMonomorphize   translate = necro_monomorphize_create(&intern, &scoped_symtable, &base, &base.ast);
    NecroSymbol         prefix    = necro_intern_string(&intern, "superCool");

    NecroInstSub* subs =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            base.unit_type->type,
                necro_create_inst_sub_manual(&base.ast.arena, NULL,
                    base.unit_type->type, NULL));
    NecroSymbol suffix_symbol = necro_create_suffix_from_subs(&translate, prefix, subs);
    printf("suffix1: %s\n", suffix_symbol->str);


    NecroInstSub* subs2 =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            base.mouse_x_fn->type,
                necro_create_inst_sub_manual(&base.ast.arena, NULL,
                    base.unit_type->type, NULL));
    NecroSymbol suffix_symbol2 = necro_create_suffix_from_subs(&translate, prefix, subs2);
    printf("suffix2: %s\n", suffix_symbol2->str);

    NecroInstSub* subs3 =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            necro_type_fn_create(&base.ast.arena,
                necro_type_con_create(&base.ast.arena, base.maybe_type, necro_type_list_create(&base.ast.arena, base.int_type->type, NULL)),
                necro_type_con_create(&base.ast.arena, base.maybe_type, necro_type_list_create(&base.ast.arena, base.float_type->type, NULL))),
            NULL);
    NecroSymbol suffix_symbol3 = necro_create_suffix_from_subs(&translate, prefix, subs3);
    printf("suffix3: %s\n", suffix_symbol3->str);

    NecroInstSub* subs4 =
        necro_create_inst_sub_manual(&base.ast.arena, NULL,
            necro_type_con_create(&base.ast.arena, base.tuple2_type, necro_type_list_create(&base.ast.arena, base.int_type->type, necro_type_list_create(&base.ast.arena, base.float_type->type, NULL))),
            NULL);
    NecroSymbol suffix_symbol4 = necro_create_suffix_from_subs(&translate, prefix, subs4);
    printf("suffix4: %s\n", suffix_symbol4->str);

}

void necro_monomorphize_test()
{
    necro_announce_phase("Monomorphize");
    // necro_monomorphize_test_suffix();

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
            "concreteNothing2 :: Maybe Audio\n"
            "concreteNothing2 = polyNothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly3";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteTuple :: (Maybe Bool, Maybe Audio)\n"
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
            "unreal :: Audio\n"
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
            "left :: Int -> Int \n"
            "left = (10+)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Right Section";
        const char* test_source = ""
            "right :: Rational -> Rational \n"
            "right = (*33.3)\n";
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
            "audioOutOfSpace :: Audio\n"
            "audioOutOfSpace = numbernomicon 10 99\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context3";
        const char* test_source = ""
            "equalizer :: (Num a, Eq a) => a -> a -> Bool\n"
            "equalizer x y = x + y == x\n"
            "zero :: Int\n"
            "zero = 0\n"
            "oneHundred :: Int\n"
            "oneHundred = 1000\n"
            "notTheSame :: Bool\n"
            "notTheSame = equalizer zero oneHundred\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoubleContext";
        const char* test_source = ""
            "doubleTrouble :: (Num a, Num b) => a -> b -> (a, b)\n"
            "doubleTrouble x y = (x - 1000, y + 1000)\n"
            "doubleEdge :: (Rational, Float)\n"
            "doubleEdge = doubleTrouble 22 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "HalfContext";
        const char* test_source = ""
            "halfTrouble :: Num a => a -> Int -> (a, Int)\n"
            "halfTrouble x y = (x - 1000, y + 1000)\n"
            "halfEdge :: (Audio, Int)\n"
            "halfEdge = halfTrouble 22.2 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DeepContext";
        const char* test_source = ""
            "deep :: Fractional a => a -> a\n"
            "deep x = x / x\n"
            "shallow :: (Fractional a, Fractional b) => a -> b -> (a, b)\n"
            "shallow y z = (deep y, deep z)\n"
            "top :: (Rational, Float)\n"
            "top = shallow 22.2 33.3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "No Monomorphism Restriction";
        const char* test_source = ""
            "data Alive = Alive\n"
            "data Dead  = Dead\n"
            "cat :: (Maybe Alive, Maybe Dead)\n"
            "cat = (alive, dead) where\n"
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
            "instance Num a => Num (Thing a) where\n"
            "  add (Thing x) (Thing y) = Thing (x + y)\n"
            "  sub (Thing x) (Thing y) = Thing (x - y)\n"
            "  mul (Thing x) (Thing y) = Thing (x * y)\n"
            "  abs (Thing x)           = Thing (abs x)\n"
            "  signum (Thing x)        = Thing (signum x)\n"
            "  fromInt x               = Thing (fromInt x)\n"
            "intThing :: Thing Int\n"
            "intThing = 10 + 33 * 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Polymorphic methods 1";
        const char* test_source = ""
            "audioPat :: Pattern Audio\n"
            "audioPat = 3.3 / 4 * 22.2\n"
            "floatPat :: Pattern Float\n"
            "floatPat = 3.3 / 4 * 22.2\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }
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
            "const :: a -> b -> a\n"
            "const x y = x\n"
            "amb :: Audio\n"
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
            "const :: a -> b -> a\n"
            "const x y = x\n"
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
            "(left, right) = (1, 2.0)\n"
            "leftPlus :: Rational -> Rational\n"
            "leftPlus x = left\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 2";
        const char* test_source = ""
            "fst:: (a, b) -> a\n"
            "fst x = left where\n"
            "  (left, _) = x\n"
            "unitFirst :: ()\n"
            "unitFirst = fst ((), True)\n"
            "intFirst :: Int\n"
            "intFirst = fst (0, ())\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Pattern Assignment 2";
        const char* test_source = ""
            "leftRight = (left, right) where\n"
            "  (left, right) = (1, 2.0)\n"
            "intFloat :: (Int, Float)\n"
            "intFloat = leftRight\n"
            "rationalAudio :: (Rational, Audio)\n"
            "rationalAudio = leftRight\n";
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
            "coolOsc :: Audio\n"
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
            "coolAudio :: Audio\n"
            "coolAudio = coolOsc\n";
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
            "one :: Array 4 Int\n"
            "one = dropOne arr1 arr2\n"
            "arr3 = { 0, 1, 2, 3, 4 }\n"
            "arr4 = { 4, 3, 2, 1, 0 }\n"
            "three :: Array 5 Int\n"
            "three = dropOne arr3 arr4\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Instance Declarations 2";
        const char* test_source = ""
            "class NumCollection c where\n"
            "  checkOutMyCollection :: Num a => a -> c a -> c a\n"
            "instance NumCollection (Array n) where\n"
            "  checkOutMyCollection x c = c\n"
            "rationals1 :: Array 1 Rational\n"
            "rationals1 = checkOutMyCollection 22 {33}\n"
            "rationals2 :: Array 2 Rational\n"
            "rationals2 = checkOutMyCollection 22 {33, 44}\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Defunctionalization 1";
        const char* test_source = ""
            "pipe :: a -> (a -> b) -> b\n"
            "pipe x f = f x\n"
            "dopeSmoker :: Audio\n"
            "dopeSmoker = pipe 100 (mul 100)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_monomorphize_test_result(test_name, test_source, expect_error_result, NULL);
    }

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

    {
        const char* test_name   = "String Test 1";
        const char* test_source = ""
            "helloWorld :: Array 12 Char\n"
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
    //         "plusOne :: Audio\n"
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


/*
Thoughts on Futhark style Defunctionalization:
    * Maybe Demand type not rely upon closures.
    * Implement "necro_is_zero_order" to test for functional types
    * Functional Types can't be returned from branches (if statements, case statements, etc)
    * Functional Types can't be stored in data types


///////////////////////////////////////////////////////
// Demand Streams Ideas
///////////////////////////////////////////////////////

-----------------------
* Idea 0: Simple Closures

    * Pros:
        - Simple semantics and implementation
        - No surprises if you understand how normal closures work
        - No additional overhead to understanding beyond understanding how closures work.
        - No actual "Translation" phase is required.
        - Wide variety of pattern manipulating combinators are possible and easy to use.
        - Can easily create user defined pattern combinators
        - Allows for unbounded "demands" while still maintaining a static/fixed memory footprint.
    * Cons:
        - Closures currently have some big restrictions on them that are more painful when put on something like Patterns.
        - Due to the semantics of Necrolang, no real way to memoize.

    * Mitigations:
        - Switch combinator is probably a better fit for "scanning" through patterns anyways, since it can use the same "pattern" syntax that loop and seq do.
        - If really worried about expensive patterns we can create an explicit memo function which will memoize the value manually.
        - Pattern
        - Might as well look into some of the restrictions for futhark style defunctionalization and see if we might be able to loosen them somewhat.

    data PVal a    = PVal Time a | PRest Time | PConst a | PInterval | PEnd
    data Pattern a = Pattern (Time -> PVal a)
    seq     :: Int -> Pattern a -> Pattern a
    rswitch :: Int -> Pattern a -> Pattern a
    switch  :: Pattern Int -> Pattern a -> Pattern a
    tempo   :: Tempo -> Pattern a -> Pattern a
    poly    :: Num a => (a -> a) -> Pattern a -> a
    mouse   :: Pattern (Int, Int)
    pmemo   :: Pattern a -> Pattern a
    every   :: Int -> (Pattern a -> Pattern a) -> Pattern a
    rev     :: Pattern a -> Pattern a

-----------------------
* Idea !: Bang Type Attributes

    * Pros:
        - Simple semantics and implementation
        - No surprises if you understand how normal closures work
        - No additional overhead to understanding beyond understanding how closures work.
        - No actual "Translation" phase is required.
        - Wide variety of pattern manipulating combinators are possible and easy to use.
        - Can easily create user defined pattern combinators
        - Allows for unbounded "demands" while still maintaining a static/fixed memory footprint.
    * Cons:
        - Closures currently have some big restrictions on them that are more painful when put on something like Patterns.
        - Due to the semantics of Necrolang, no real way to memoize.

    * Mitigations:
        - Switch combinator is probably a better fit for "scanning" through patterns anyways, since it can use the same "pattern" syntax that loop and seq do.
        - If really worried about expensive patterns we can create an explicit memo function which will memoize the value manually.
        - Pattern
        - Might as well look into some of the restrictions for futhark style defunctionalization and see if we might be able to loosen them somewhat.

    data PVal a    = PVal Time a | PRest Time | PConst a | PInterval | PEnd
    // data Pattern a = Pattern (Time -> PVal a)

    seq     :: Int -> !a -> !a
    rswitch :: Int -> !a -> !a
    switch  :: !Int -> !a -> !a
    tempo   :: Tempo -> !a -> !a
    poly    :: Num a => (a -> a) -> !a -> a
    mouse   :: !(Int, Int)
    pmemo   :: Pattern a -> Pattern a
    every   :: Int -> (!a -> !a) -> !a
    rev     :: !a -> !a


-----------------------
* Idea Syntax: Special 'pat' and 'with' syntax, similar to 'do' syntax in Haskell (i.e. enter into a sublanguage)
    - Everything is still strict, but the time scoping semantics are different.

    data PVal a    = PVal Time a | PRest Time | PConst a | PInterval | PEnd
    data Pattern a = Pattern (Time -> PVal a)
    seq     :: Int -> Pattern a -> Pattern a
    rswitch :: Int -> Pattern a -> Pattern a
    switch  :: Pattern Int -> Pattern a -> Pattern a
    tempo   :: Tempo -> Pattern a -> Pattern a
    poly    :: Num a => (a -> a) -> Pattern a -> a
    mouse   :: Pattern (Int, Int)
    pmemo   :: Pattern a -> Pattern a

-----------------------
* Idea 0.5: Lazy Pattern Type with forcing behavior

    * All types that have "demand" like characteristics use the Pattern Types (lazy list like things, input event type things, musical patterns, etc).
    * Certain constructs "force" the value: case expressions evaluations (force), case branches (force, box), arrays (force, box), recursive values (lift, force, box), strictness annotations?

    data Pattern a = PVal Time a | PRest Time | PConst a | PInterval | PEnd
    seq     :: Int -> Pattern a -> Pattern a
    rswitch :: Int -> Pattern a -> Pattern a
    switch  :: Pattern Int -> Pattern a -> Pattern a
    tempo   :: Tempo -> Pattern a -> Pattern a
    poly    :: Num a => (a -> a) -> Pattern a -> a
    mouse   :: Pattern (Int, Int)


-----------------------
* Idea 1: Demand Types

    data Demand a
        = Demand a -- A delayed / demanded value. Will be translated in later pass.
        | Sample a -- Sampled value, simply represents whatever the value was when it was last seen.
        | Interval -- A Nil value which indicates that it may come back at some point.
        | End      -- End of stream, will never come back.

    Translation:
        * Rule 1 (Abstraction):
            Demand values (Demand a) are translated to lambdas of type: () -> Demand a.
            NOTE: This prevents sharing.

            top :: Demand Int
            top = expr

            ==>

            top :: () -> Demand Int
            top _ = expr

        * Rule 2 (Usage):
            Demand value usage is translated to an application of () to the created lambdas

            doubleTop :: Demand Int
            doubleTop = top + top

            ==>

            doubleTop :: () -> Demand Int
            doubleTop _ = top () + top ()

        * Rule 3 (Recursion):
            Recursive demand values are are translated to have a recursive subexpression value (not function) with the outer value translated as per rule 1.
            Usages of the recursive value inside its definition are untranslated, usages outside of its definition are translated as per rule 1.
            NOTE: This preserves self-sharing.

            rec :: Demand Int
            rec ~ 0 = rec + 1

            ==>

            rec :: () -> Demand Int
            rec _ = rec' ()
              where
                rec' ~ 0 = rec' + 1

        * Rule 4 (Elimination):
            Calls to delay are removed.  Translations of the form: (\() -> dmdValue ()), are reduced to: dmdValue

        * All non-top level demand values are left in tact
        * Recursive non-top level demand values work as normal values

    noisey :: Audio
    noisey = poly coolSynth (play coolPat)
      where
        a       = ...
        b       = ...
        coolPat = seq [a b _ [2 a] (sample mouseX)]

    * Pattern sequence notation: [a b] translates to pat0 :: Time -> Demand (Time, a)
    * seq :: (Time -> Demand (Time, a)) -> Demand (Time, a)

    * Common sub-expression elimination for wasteful re-evaluation in same time scope.
      Or translate into let binding?
      No sharing in other time scopes (as intended). Best of both worlds.

    * sample :: a -> Demand a

    * delay  :: (() -> a) -> Demand a (Needs to be keyword, not function, since it can't be curried)
    * demand :: Demand a -> Maybe a

    // Lists as demand streams?
    * (:) :: a -> Demand a -> Demand a
    * [a] ==> Demand a (translated by

-----------------------
* Idea 2: Demand rate Bindings:
    * Instead of accomplishing this through types, use it variable bindings (much like how Haskell has strictness annotations on variable bindings)
    * Demand annotations on variable bindings.
    * Variables annotated as demand are not evaluated strictly.

    // * Implicitly evaluated at the call site where they are used instead, using that call site's "Time scope".
    * To be evaluated strictly is must be demanded via: demand ~: a -> a
    * A strict value can be used in a demand via: sample :: a ~> a

    * Can be used with certain built-in constructs that call demand values multiple times, such as poly, take, drop, etc.
    * Eliminates some corner cases (such as nested demand types), should make usage easier and semantics clearer.
    * Uses a simple translation process which turns demand bindings into functions bindings
        x ~: Int
        x = ...
        ==>
        x' :: () -> Int
        x' _ = ...
    * Use sites translate into let bindings and applications.
        y = x + x
        ==>
        y = x + x where x = x' ()
    * This preserves sharing within the time scope but still prevents sharing different time scope (exactly what we want).
    * Less noise in type signatures

        seq  ~: Int -> PatternSteps a ~> Pattern a
        play ~: Tempo -> Pattern a ~> Event a
        poly  : (Default a, Num a) => (a -> a) -> Event a ~> a

    * does prevent demand type inference, and places the burden on the programmer to remember to explicitly annotate demand-ness every it is required.
    * Also sub declarations which aren't explicitly declared as demand rate would implicitly evaluate strictly, which would cause some weirdness...
    * Need additional translation rules, hmmm.....

    data Pattern a
        = PVal Time a
        | PRest Time
        | PInterval
        | PEnd

    data Event a
        = EVal Time a
        | EInterval


-----------------------
* Idea 3: Demand Attributes (similar to clean uniqueness attributes)

    * Type signatures
        demand :: *a -> a
        sample :: a -> *a
        seq    :: Int -> *a -> *a
        tempo  :: Tempo -> *a -> *a
        pause  :: Bool -> *a -> *a
        poly   :: Num a => (a -> a) -> *a -> a
        mouse  :: *(Int, Int)
        mouseX :: *Int

    * Pros:
        - Doesn't need to follow function restrictions
        - In theory allows for sharing in the local context (but not global sharing).

    * Rules:
        * Demand Types are types decorated by a Demand Attribute
        * Attribute propagation means that "Demandedness" can't nest.
        * Implicit demand and delay (Force which attaches "Strict ConsumedDemand" attribute), like Idris.
        * DemandTypes are translated into the type: () -> a. // data Demand a = Demand (() -> a), *a ==> Demand a
        * DemandTypes are demanded when used as the expression portion of a case expression, similar to Haskell lazy evaluation.
        * DemandTypes are demanded and reboxed when used in the branch portion of a case expression.
        * Recursive DemandTypes lift a value into a sub declaration which is recursive where it is demanded, then it is reboxed and assigned to the original value which is turned into a function of type: () -> Demand a
        * (More advanced) Memo values placed at each "Time scope" which memoizes results to recover sharing where we need it?
        * requires deltaTime :: Time combinator which is updated with local delta time to function.
        * Some combinators ignore negative time and treat it as positive time, or ignore time manipulation altogether (events, audio combinators, etc).
        * Don't generalize local declaration groups automatically.

    * Demand Attribute:
        * Strict
        * Demand
        * StrictConsumedDemand

    * Translation:
        delay  :: a -> *a
        demand :: *a -> a

        x :: *Int
        x = ...

        y :: *Int
        y = x + x + nonDemandInt

        z :: Int
        z = y

        c :: Bool -> *Int
        c b = if b then x else drop x y + y

        drop :: *a -> *b -> *a
        drop x _ = x

        pat :: *Int
        pat = [0 x _ [y (delay z)] mouseX _]

        ==>

        delay :: a -> () -> a
        delay x _ = x

        demand :: (() -> a) -> a
        demand x = x ()

        x :: () -> Int
        x _ = ...

        y :: () -> Int
        y _ =
          let x' = x ()
          in  x' + x' + nonDemandInt -- NOTE: this should translate naively should be: x' + x' + demand (sample nonDemandInt), but demand + sample next to eachother in translation should cancel eachother out.

        z :: Int
        z = y ()

        c :: Bool -> () -> Int
        c b _ = if b
          then let x' = x () in x'
          else
            let y' = y ()
            let d' = drop x y () in
            d' + y'

    * Attribute propagation ensures that we can't have nested demand types and other weirdness.

-----------------------
* Idea 4: Demand Kinds

    * Type signatures
        data Pattern a :: DemandType = PVal Time a | PRest Time | PInterval | PEnd
        data Event   a :: DemandType = Event Time a | EInterval
        data Demand  a :: DemandType = Demand a

        -- no need for sample, use pure instead
        demand :: (DemandClass c) => c a -> a
        seq    :: Int -> (Int -> Pattern a) -> Pattern a
        play   :: Tempo -> Pattern a -> Event a
        poly   :: (Default a, Num a) => (a -> a) -> Event a -> a
        mouse  :: Event (Int, Int)

    * Demand Kind rules / restrictions:
        * Non-Nesting: DemandType cannot be arguments of other type's kind signatures
        * Must implement "DemandClass" (need better name).
        * Only built-in DemandTypes for now. No syntax for user built ones.

    * Translation Rules:
        * DemandTypes are translated into a function of type: (DemandClass c) => () -> c a
        * DemandTypes are demanded when used as the expression portion of a case expression, similar to Haskell lazy evaluation.
        * DemandTypes are demanded and reboxed when used in the branch portion of a case expression.
        * Recursive DemandTypes lift a value into a sub declaration which is recursive where it is demanded, then it is reboxed and assigned to the original value which is turned into a function of type: () -> Demand a
        * (More advanced) Memo values placed at each "Time scope" which memoizes results to recover sharing where we need it?

    * Translation:
        data Demand a = Demand a

        pure :: a -> Demand a
        pure x = Demand x

        x :: Demand Int
        x = ...

        y :: Demand Int
        y = x + x + pure nonDemandInt

        z :: Int
        z = demand y

        c :: Bool -> Demand Int
        c b = if b then x else y

        demand :: Demand a -> a
        demand x = case x of
          Demand x' -> x'

        instance Num a => Num (Demand a) where
          add x y = pure (demand x + demand y)

        ==>

        pure :: a -> (() -> Demand a)
        pure x = \_ -> Demand x

        x :: () -> Demand Int
        x = ...

        y :: () -> Demand Int
        y = x + x + pure nonDemandInt -- Num class instance of Demand does the heavy lifting, but this prevents sharing. Rely on common sub-expression elimination?

        z :: Int
        z = demand y

        c :: Bool -> () -> Demand Int
        c b = pure <| if b then demand x else demand y

        demand :: (() -> Demand a) -> a
        demand x = case x () of
          Demand x' -> x'

        instance Num a => Num (Demand a) where
          add x y = pure (demand x + demand y)

    * Translation occurs before, after, or during monomorphization?


-----------------------
* Idea 3: Internal Attributes on special Pattern type (similar to clean uniqueness attributes, but only applied internally.)

    * Type signatures
        data Pattern a = PVal Time a | PRest Time a | PInterval | PConst a | PEnd a

        seq    :: Int -> Pattern a -> Pattern a
        play   :: Tempo -> Pattern a -> Event a
        pause  :: Bool -> Pattern a -> Pattern a
        poly   :: Num a => (a -> a) -> Event a -> a
        mouse  :: Event (Int, Int)
        mouseX :: Event Int

    * Pattern Attribute:
        * Strict
        * Lazy
        * StrictConsumedLazy

    * Pros:
        - Doesn't need to follow function restrictions
        - In theory allows for sharing in the local context (but not global sharing).

    * Rules:
        * Demand Types are types decorated by a Demand Attribute
        * Attribute propagation means that "Demandedness" can't nest.
        * Implicit demand and delay (Force which attaches "Strict ConsumedDemand" attribute), like Idris.
        * DemandTypes are translated into the type: () -> a. // data Demand a = Demand (() -> a), *a ==> Demand a
        * DemandTypes are demanded when used as the expression portion of a case expression, similar to Haskell lazy evaluation.
        * DemandTypes are demanded and reboxed when used in the branch portion of a case expression.
        * Recursive DemandTypes lift a value into a sub declaration which is recursive where it is demanded, then it is reboxed and assigned to the original value which is turned into a function of type: () -> Demand a
        * (More advanced) Memo values placed at each "Time scope" which memoizes results to recover sharing where we need it?
        * requires deltaTime :: Time combinator which is updated with local delta time to function.
        * Some combinators ignore negative time and treat it as positive time, or ignore time manipulation altogether (events, audio combinators, etc).
        * Don't generalize local declaration groups automatically.

    * Translation:
        x :: Pattern Int
        x = ...

        y :: Pattern Int
        y = x + x + pure nonDemandInt

        z :: Int
        z = sample y

        c :: Bool -> Pattern Int
        c b = if b then x else drop x y + y

        drop :: Pattern a -> Pattern b -> Pattern a
        drop x _ = x

        pat :: Pattern Int
        pat = [0 x _ [y (pure z)] mouseX _]

        ==>

        x :: () -> Pattern Int
        x _ = ...

        y :: () -> Pattern Int
        y _ =
          let x' = x ()
          in  x' + x' + pure nonDemandInt -- NOTE: this should translate naively should be: x' + x' + demand (sample nonDemandInt), but demand + sample next to eachother in translation should cancel eachother out.

        z :: Int
        z = sample (y ())

        c :: Bool -> () -> Pattern Int
        c b _ = if b
          then let x' = x () in x'
          else
            let y' = y ()
            let d' = drop x y () in
            d' + y'

        drop :: Pattern a -> Pattern b -> () -> Pattern a
        drop x y _ = x

        pat :: () -> Pattern Int
        pat _ = ...more intense translation...

    * Attribute propagation ensures that we can't have nested demand types and other weirdness.


*/

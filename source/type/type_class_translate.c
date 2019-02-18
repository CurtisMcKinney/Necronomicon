/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "type_class_translate.h"
#include "base.h"
#include "infer.h"
#include "result.h"

/*
    Notes (Curtis, 2-8-19):
        * Healthy defaulting scheme, including a --> ()
        * Default type class. Use this for polymorphic recursion
        * Given the above we could do an even more drastic memory scheme, such as large scale Region based memory management.
        * Rename pass to necro_type_specialize ???
        * Implement Patterns (The musical ones...) similarly to Iterator trait in Rust, via TypeClasses?

    TODO:
        * Defaulting / Ambiguous Type Class error
        * Prune pass after to cull all unneeded stuff and put it in a direct form to make Chad's job for Core translation easier?
        * Pattern Literals
        * Mutually recursive bindings
        * Pattern bindings
        * do statements
*/

///////////////////////////////////////////////////////
// NecroTypeClassTranslate
///////////////////////////////////////////////////////

typedef struct
{
    NecroIntern*         intern;
    NecroScopedSymTable* scoped_symtable;
    NecroBase*           base;
    NecroAstArena*       ast_arena;
    NecroPagedArena*     arena;
    NecroSnapshotArena   snapshot_arena;
    char*                suffix_buffer;
    size_t               suffix_size;
} NecroTypeClassTranslate;

NecroTypeClassTranslate necro_type_class_translate_empty()
{
    NecroTypeClassTranslate type_class_translate = (NecroTypeClassTranslate)
    {
        .intern          = NULL,
        .arena           = NULL,
        .snapshot_arena  = necro_snapshot_arena_empty(),
        .scoped_symtable = NULL,
        .base            = NULL,
        .ast_arena       = NULL,
    };
    return type_class_translate;
}

NecroTypeClassTranslate necro_type_class_translate_create(NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroTypeClassTranslate type_class_translate = (NecroTypeClassTranslate)
    {
        .intern          = intern,
        .arena           = &ast_arena->arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
        .suffix_buffer   = malloc(256 * sizeof(char)),
        .suffix_size     = 256,
    };
    return type_class_translate;
}

void necro_type_class_translate_destroy(NecroTypeClassTranslate* type_class_translate)
{
    assert(type_class_translate != NULL);
    necro_snapshot_arena_destroy(&type_class_translate->snapshot_arena);
    free(type_class_translate->suffix_buffer);
    *type_class_translate = necro_type_class_translate_empty();
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroResult(void) necro_type_class_translate_go(NecroTypeClassTranslate* type_class_translate, NecroAst* ast, NecroInstSub* subs);
NecroAstSymbol*   necro_ast_specialize(NecroTypeClassTranslate* type_class_translate, NecroAstSymbol* ast_symbol, NecroInstSub* subs);


NecroResult(void) necro_type_class_translate(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroTypeClassTranslate type_class_translate = necro_type_class_translate_create(intern, scoped_symtable, base, ast_arena);
    NecroResult(void)       result               = necro_type_class_translate_go(&type_class_translate, ast_arena->root, NULL);
    necro_type_class_translate_destroy(&type_class_translate);
    if (result.type == NECRO_RESULT_OK)
    {
        if (info.compilation_phase == NECRO_PHASE_TYPE_CLASS_TRANSLATE && info.verbosity > 0)
            necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    return result;
}

///////////////////////////////////////////////////////
// SpecializeAst
///////////////////////////////////////////////////////
NecroSymbol necro_create_suffix_from_subs(NecroTypeClassTranslate* type_class_translate, NecroSymbol prefix, const NecroInstSub* subs)
{
    // Count length
    size_t              length   = 2 + strlen(prefix->str);
    const NecroInstSub* curr_sub = subs;
    while (curr_sub != NULL)
    {
        length += necro_type_mangled_string_length(curr_sub->new_name);
        if (curr_sub->next != NULL)
            length += 1;
        curr_sub = curr_sub->next;
    }

    // Resize buffer
    if (type_class_translate->suffix_size < length)
    {
        free(type_class_translate->suffix_buffer);
        while (type_class_translate->suffix_size < length)
            type_class_translate->suffix_size *= 2;
        type_class_translate->suffix_buffer = malloc(type_class_translate->suffix_size * sizeof(char));
    }

    // Write type strings
    size_t offset = sprintf(type_class_translate->suffix_buffer, "%s<", prefix->str);
    curr_sub      = subs;
    while (curr_sub != NULL)
    {
        offset  = necro_type_mangled_sprintf(type_class_translate->suffix_buffer, offset, curr_sub->new_name);
        if (curr_sub->next != NULL)
            offset += sprintf(type_class_translate->suffix_buffer + offset, ",");
        curr_sub = curr_sub->next;
    }
    offset += sprintf(type_class_translate->suffix_buffer + offset, ">");
    assert(offset == length);

    return necro_intern_string(type_class_translate->intern, type_class_translate->suffix_buffer);
}

NecroAstSymbol* necro_ast_specialize_method(NecroTypeClassTranslate* type_class_translate, NecroAstSymbol* ast_symbol, NecroInstSub* subs)
{
    assert(type_class_translate != NULL);
    assert(ast_symbol != NULL);
    assert(subs != NULL);

    // TODO: Fuuuuuuuck....how to handle multi-line apat assignments and mututally recursive bindings!? Cornercases hate your life.
    NecroTypeClass* type_class        = ast_symbol->method_type_class;
    NecroAstSymbol* type_class_var    = type_class->type_var;
    NecroInstSub*   curr_sub          = subs;
    while (curr_sub != NULL)
    {
        if (curr_sub->var_to_replace->name == type_class_var->name)
        {
            NecroType* sub_con = necro_type_find(curr_sub->new_name);
            assert(sub_con->type == NECRO_TYPE_CON);
            NecroSymbol     instance_method_name       = necro_intern_create_type_class_instance_symbol(type_class_translate->intern, ast_symbol->source_name, sub_con->con.con_symbol->source_name);
            NecroAstSymbol* instance_method_ast_symbol = necro_scope_find_ast_symbol(type_class_translate->scoped_symtable->top_scope, instance_method_name);
            assert(instance_method_ast_symbol != NULL);
            if (necro_type_is_polymorphic(instance_method_ast_symbol->type))
            {
                return necro_ast_specialize(type_class_translate, instance_method_ast_symbol, subs);
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

NecroAstSymbol* necro_ast_specialize(NecroTypeClassTranslate* type_class_translate, NecroAstSymbol* ast_symbol, NecroInstSub* subs)
{
    assert(type_class_translate != NULL);
    assert(ast_symbol != NULL);
    assert(ast_symbol->ast != NULL);
    assert(ast_symbol->ast->scope != NULL);
    assert(subs != NULL);
    if (ast_symbol->method_type_class)
        return necro_ast_specialize_method(type_class_translate, ast_symbol, subs);

    // TODO: Fuuuuuuuck....how to handle multi-line apat assignments and mututally recursive bindings!? Cornercases hate your life.

    // Find specialized ast
    NecroSymbol     specialized_name       = necro_create_suffix_from_subs(type_class_translate, ast_symbol->source_name, subs);
    NecroScope*     scope                  = ast_symbol->ast->scope;
    NecroAstSymbol* specialized_ast_symbol = necro_scope_find_ast_symbol(scope, specialized_name);
    if (specialized_ast_symbol != NULL)
    {
        assert(specialized_ast_symbol->ast != NULL);
        return specialized_ast_symbol;
    }

    // NecroAst* declaration_group_list = ast_symbol->declaration_group->declaration.declaration_group_list;
    NecroAst* declaration_group_list = NULL;
    switch (ast_symbol->ast->type)
    {
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        declaration_group_list = ast_symbol->ast->simple_assignment.declaration_group->declaration.declaration_group_list;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        declaration_group_list = ast_symbol->ast->apats_assignment.declaration_group->declaration.declaration_group_list;
        break;
    // TODO: Other named things?
    default:
        assert(false);
        break;
    }

    assert(declaration_group_list != NULL);
    assert(declaration_group_list->type == NECRO_AST_DECLARATION_GROUP_LIST);

    NecroAst* next_group_list                           = declaration_group_list->declaration_group_list.next;
    NecroAst* new_declaration                           = necro_ast_create_decl(type_class_translate->arena, ast_symbol->ast, NULL);
    NecroAst* new_group_list                            = necro_ast_create_declaration_group_list_with_next(type_class_translate->arena, new_declaration, next_group_list);
    declaration_group_list->declaration_group_list.next = new_group_list;
    new_declaration->declaration.declaration_group_list = new_group_list;
    new_declaration->declaration.declaration_impl       = NULL; // Removing dummy argument

    // Create specialized ast
    specialized_ast_symbol                        = necro_ast_symbol_create(type_class_translate->arena, specialized_name, specialized_name, type_class_translate->ast_arena->module_name, NULL);
    specialized_ast_symbol->declaration_group     = new_declaration;
    specialized_ast_symbol->ast                   = necro_ast_deep_copy_go(type_class_translate->arena, new_declaration, ast_symbol->ast);
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
    // TODO: Other named things?
    default:
        assert(false);
        break;
    }
    NecroScope* prev_scope                                    = type_class_translate->scoped_symtable->current_scope;
    NecroScope* prev_type_scope                               = type_class_translate->scoped_symtable->current_type_scope;
    type_class_translate->scoped_symtable->current_scope      = scope;
    necro_rename_internal_scope_and_rename(type_class_translate->ast_arena, type_class_translate->scoped_symtable, type_class_translate->intern, specialized_ast_symbol->ast);
    type_class_translate->scoped_symtable->current_scope      = prev_scope;
    type_class_translate->scoped_symtable->current_type_scope = prev_type_scope;

    specialized_ast_symbol->type                              = necro_type_replace_with_subs_deep_copy(type_class_translate->arena, ast_symbol->type, subs);
    unwrap(void, necro_type_class_translate_go(type_class_translate, specialized_ast_symbol->ast, subs));
    return specialized_ast_symbol;
}

/*
    Note: To specialize a value or function:
        * The value or function must be polymorphic
        * The usage of the value or function must be completely concrete
    TODO: Figure out ambigous type variable error and defaulting....rigid type variable?
*/
NecroResult(bool) necro_ast_should_specialize(NecroAstSymbol* ast_symbol, NecroAst* ast, NecroInstSub* inst_subs, NecroInstSub* subs)
{
    UNUSED(subs);
    if (!necro_type_is_polymorphic(ast_symbol->type))
        return ok(bool, false);
    if (necro_type_is_polymorphic(ast->necro_type))
    {
        // if (subs != NULL)
            return ok(bool, false);
        // else
        //     return necro_error_map(void, bool, necro_type_ambiguous_type_var_error(ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc));
    }
    NecroInstSub* curr_sub  = inst_subs;
    while (curr_sub != NULL)
    {
        if (necro_type_is_polymorphic(curr_sub->new_name))
        {
            return ok(bool, false);
            // return necro_error_map(void, bool, necro_type_ambiguous_type_var_error(ast_symbol, curr_sub->new_name, ast->source_loc, ast->end_loc));
        }
        curr_sub = curr_sub->next;
    }
    return ok(bool, true);
}

NecroResult(void) necro_type_ambiguous_type_var_check(NecroTypeClassTranslate* type_class_translate, NecroAst* ast)
{
    UNUSED(type_class_translate);
    UNUSED(ast);
    return ok_void();
    // necro_type_ambiguous_type_var_error
}

///////////////////////////////////////////////////////
// Translate Go
///////////////////////////////////////////////////////
NecroResult(void) necro_type_class_translate_go(NecroTypeClassTranslate* type_class_translate, NecroAst* ast, NecroInstSub* subs)
{
    if (ast == NULL)
        return ok_void();
    ast->necro_type = necro_type_replace_with_subs(type_class_translate->arena, ast->necro_type, subs);
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
            necro_try(void, necro_type_class_translate_go(type_class_translate, group_list->declaration_group_list.declaration_group, subs));
            group_list = group_list->declaration_group_list.next;
        }
        return ok_void();
    }

    case NECRO_AST_DECL:
    {
        NecroAst* declaration_group = ast;
        while (declaration_group != NULL)
        {
            necro_try(void, necro_type_class_translate_go(type_class_translate, declaration_group->declaration.declaration_impl, subs));
            declaration_group = declaration_group->declaration.next_declaration;
        }
        return ok_void();
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        return necro_type_class_translate_go(type_class_translate, ast->type_class_instance.declarations, subs);

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        ast->simple_assignment.ast_symbol->type = necro_type_replace_with_subs(type_class_translate->arena, ast->simple_assignment.ast_symbol->type, subs);
        if (ast->simple_assignment.initializer != NULL && !necro_is_fully_concrete(ast->necro_type))
        {
            return necro_type_non_concrete_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        if (ast->simple_assignment.initializer != NULL && !ast->simple_assignment.is_recursive)
        {
            return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
        }
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->simple_assignment.initializer, subs));
        return necro_type_class_translate_go(type_class_translate, ast->simple_assignment.rhs, subs);

    case NECRO_AST_APATS_ASSIGNMENT:
        ast->apats_assignment.ast_symbol->type = necro_type_replace_with_subs(type_class_translate->arena, ast->apats_assignment.ast_symbol->type, subs);
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->apats_assignment.apats, subs));
        return necro_type_class_translate_go(type_class_translate, ast->apats_assignment.rhs, subs);

//     // NOTE: We are making pattern bindings fully monomorphic now (even if a type signature is given. Haskell was at least like this at one point)
//     // If we REALLY want this (for some reason!?!?) we can look into putting more effort into getting polymorphic pattern bindings in.
//     // For now they have a poor weight to power ration.
//     case NECRO_AST_PAT_ASSIGNMENT:
//         necro_try(NecroType, necro_rec_check_pat_assignment(infer, ast->pat_assignment.pat));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->pat_assignment.rhs);

    case NECRO_AST_DATA_DECLARATION:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->data_declaration.simpletype, subs));
        return necro_type_class_translate_go(type_class_translate, ast->data_declaration.constructor_list, subs);

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
        {
            ast->variable.inst_subs      = necro_type_union_subs(ast->variable.inst_subs, subs);
            const bool should_specialize = necro_try_map(bool, void, necro_ast_should_specialize(ast->variable.ast_symbol, ast, ast->variable.inst_subs, subs));
            if (!should_specialize)
                return ok_void();
            NecroAstSymbol* specialized_ast_symbol = necro_ast_specialize(type_class_translate, ast->variable.ast_symbol, ast->variable.inst_subs);
            NecroAst*       specialized_var        = necro_ast_create_var_full(type_class_translate->arena, specialized_ast_symbol, NECRO_VAR_VAR, NULL, NULL);
            specialized_var->necro_type            = specialized_ast_symbol->type;
            *ast = *specialized_var;
            return ok_void();
        }

        case NECRO_VAR_DECLARATION:
            if (ast->variable.ast_symbol != NULL)
                ast->variable.ast_symbol->type = necro_type_replace_with_subs(type_class_translate->arena, ast->variable.ast_symbol->type, subs);
            return ok_void();
        case NECRO_VAR_SIG:                  return ok_void();
        case NECRO_VAR_TYPE_VAR_DECLARATION: return ok_void();
        case NECRO_VAR_TYPE_FREE_VAR:        return ok_void();
        case NECRO_VAR_CLASS_SIG:            return ok_void();
        default:
            necro_unreachable(void);
        }

    case NECRO_AST_CONID:
        return ok_void();

    case NECRO_AST_UNDEFINED:
        return ok_void();

    case NECRO_AST_CONSTANT:
        // TODO (Curtis, 2-14-19): Handle this for overloaded numeric literals in patterns
        // necro_type_class_translate_constant(infer, ast, dictionary_context);
        return ok_void();

    case NECRO_AST_UN_OP:
        return ok_void();

    case NECRO_AST_BIN_OP:
    {
        ast->bin_op.inst_subs        = necro_type_union_subs(ast->bin_op.inst_subs, subs);
        const bool should_specialize = necro_try_map(bool, void, necro_ast_should_specialize(ast->bin_op.ast_symbol, ast, ast->bin_op.inst_subs, subs));
        if (should_specialize)
        {
            ast->bin_op.ast_symbol = necro_ast_specialize(type_class_translate, ast->bin_op.ast_symbol, ast->bin_op.inst_subs);
        }
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->bin_op.lhs, subs));
        return necro_type_class_translate_go(type_class_translate, ast->bin_op.rhs, subs);
    }

    case NECRO_AST_OP_LEFT_SECTION:
    {
        ast->op_left_section.inst_subs = necro_type_union_subs(ast->op_left_section.inst_subs, subs);
        const bool should_specialize   = necro_try_map(bool, void, necro_ast_should_specialize(ast->op_left_section.ast_symbol, ast, ast->op_left_section.inst_subs, subs));
        if (should_specialize)
        {
            ast->op_left_section.ast_symbol = necro_ast_specialize(type_class_translate, ast->op_left_section.ast_symbol, ast->op_left_section.inst_subs);
        }
        return necro_type_class_translate_go(type_class_translate, ast->op_left_section.left, subs);
    }

    case NECRO_AST_OP_RIGHT_SECTION:
    {
        ast->op_right_section.inst_subs = necro_type_union_subs(ast->op_right_section.inst_subs, subs);
        const bool should_specialize    = necro_try_map(bool, void, necro_ast_should_specialize(ast->op_right_section.ast_symbol, ast, ast->op_right_section.inst_subs, subs));
        if (should_specialize)
        {
            ast->op_right_section.ast_symbol = necro_ast_specialize(type_class_translate, ast->op_right_section.ast_symbol, ast->op_right_section.inst_subs);
        }
        return necro_type_class_translate_go(type_class_translate, ast->op_right_section.right, subs);
    }

    case NECRO_AST_IF_THEN_ELSE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->if_then_else.if_expr, subs));
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->if_then_else.then_expr, subs));
        return necro_type_class_translate_go(type_class_translate, ast->if_then_else.else_expr, subs);

    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->right_hand_side.declarations, subs));
        return necro_type_class_translate_go(type_class_translate, ast->right_hand_side.expression, subs);

    case NECRO_AST_LET_EXPRESSION:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->let_expression.declarations, subs));
        return necro_type_class_translate_go(type_class_translate, ast->let_expression.expression, subs);

    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->fexpression.aexp, subs));
        return necro_type_class_translate_go(type_class_translate, ast->fexpression.next_fexpression, subs);

    case NECRO_AST_APATS:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->apats.apat, subs));
        return necro_type_class_translate_go(type_class_translate, ast->apats.next_apat, subs);

    case NECRO_AST_WILDCARD:
        return ok_void();

    case NECRO_AST_LAMBDA:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->lambda.apats, subs));
        return necro_type_class_translate_go(type_class_translate, ast->lambda.expression, subs);

//     case NECRO_AST_DO:
//     {
           // // TODO: update ast_symbol type for bind!
           // // ast->bin_op.ast_symbol->type  = necro_type_replace_with_subs(type_class_translate->arena, ast->bin_op.ast_symbol->type, subs);
//         // DO statements NOT FULLY IMPLEMENTED currently
//         necro_unreachable(NecroType);
//         // TODO: REDO!
//         // NecroAst* bind_node = necro_ast_create_var(infer->arena, infer->intern, "bind", NECRO_VAR_VAR);
//         // NecroScope* scope = ast->scope;
//         // while (scope->parent != NULL)
//         //     scope = scope->parent;
//         // bind_node->scope = bind_node->scope = scope;
//         // // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//         // // necro_rename_var_pass(infer->renamer, infer->arena, bind_node);
//         // bind_node->necro_type = necro_symtable_get(infer->symtable, bind_node->variable.id)->type;
//         // NecroTypeClassContext* bind_context     = necro_symtable_get(infer->symtable, bind_node->variable.id)->type->for_all.context;
//         // NecroTypeClassContext* monad_context    = ast->do_statement.monad_var->var.context;
//         // while (monad_context->type_class_name.id.id != bind_context->type_class_name.id.id)
//         //     monad_context = monad_context->next;
//         // // assert(monad_context->type_class_name.id.id != bind_context->type_class_name.id.id);
//         // NecroAst*          bind_method_inst = necro_resolve_method(infer, monad_context->type_class, monad_context, bind_node, dictionary_context);
//         // necro_ast_print(bind_method_inst);
//         // necro_type_class_translate_go(type_class_translate, ast->do_statement.statement_list);
//         // break;
//     }

    case NECRO_AST_LIST_NODE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->list.item, subs));
        return necro_type_class_translate_go(type_class_translate, ast->list.next_item, subs);

    case NECRO_AST_EXPRESSION_LIST:
        return necro_type_class_translate_go(type_class_translate, ast->expression_list.expressions, subs);

    case NECRO_AST_EXPRESSION_ARRAY:
        return necro_type_class_translate_go(type_class_translate, ast->expression_array.expressions, subs);

    case NECRO_AST_PAT_EXPRESSION:
        return necro_type_class_translate_go(type_class_translate, ast->pattern_expression.expressions, subs);

    case NECRO_AST_TUPLE:
        return necro_type_class_translate_go(type_class_translate, ast->tuple.expressions, subs);

    case NECRO_BIND_ASSIGNMENT:
        ast->bind_assignment.ast_symbol->type = necro_type_replace_with_subs(type_class_translate->arena, ast->bind_assignment.ast_symbol->type, subs);
        return necro_type_class_translate_go(type_class_translate, ast->bind_assignment.expression, subs);

    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->pat_bind_assignment.pat, subs));
        return necro_type_class_translate_go(type_class_translate, ast->pat_bind_assignment.expression, subs);

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->arithmetic_sequence.from, subs));
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->arithmetic_sequence.then, subs));
        return necro_type_class_translate_go(type_class_translate, ast->arithmetic_sequence.to, subs);

    case NECRO_AST_CASE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->case_expression.expression, subs));
        return necro_type_class_translate_go(type_class_translate, ast->case_expression.alternatives, subs);

    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->case_alternative.pat, subs));
        return necro_type_class_translate_go(type_class_translate, ast->case_alternative.body, subs);

    case NECRO_AST_TYPE_APP:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->type_app.ty, subs));
        return necro_type_class_translate_go(type_class_translate, ast->type_app.next_ty, subs);

    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->bin_op_sym.left, subs));
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->bin_op_sym.op, subs));
        return necro_type_class_translate_go(type_class_translate, ast->bin_op_sym.right, subs);

    case NECRO_AST_CONSTRUCTOR:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->constructor.conid, subs));
        return necro_type_class_translate_go(type_class_translate, ast->constructor.arg_list, subs);

    case NECRO_AST_SIMPLE_TYPE:
        necro_try(void, necro_type_class_translate_go(type_class_translate, ast->simple_type.type_con, subs));
        return necro_type_class_translate_go(type_class_translate, ast->simple_type.type_var_list, subs);

    case NECRO_AST_TYPE_CLASS_DECLARATION:
    case NECRO_AST_TYPE_SIGNATURE:
    case NECRO_AST_TYPE_CLASS_CONTEXT:
    case NECRO_AST_FUNCTION_TYPE:
        return ok_void();
    default:
        necro_unreachable(void);
    }
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_type_class_translate_test_result(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
{
    UNUSED(test_name);
    UNUSED(str);
    UNUSED(expected_result);
    UNUSED(error_type);

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
    unwrap(void, necro_infer(info, &intern, &scoped_symtable, &base, &ast));
    NecroResult(void) result = necro_type_class_translate(info, &intern, &scoped_symtable, &base, &ast);

    // Assert
    // if (result.type != expected_result)
    // {
        // necro_ast_arena_print(&base.ast);
        necro_ast_arena_print(&ast);
        // necro_scoped_symtable_print_top_scopes(&scoped_symtable);
    // }
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
    if (result.type == NECRO_RESULT_ERROR)
        necro_result_error_print(result.error, str, "Test");
    else if (result.error)
        free(result.error);

    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_type_class_translate_test_suffix()
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);

    NecroTypeClassTranslate translate = necro_type_class_translate_create(&intern, &scoped_symtable, &base, &base.ast);
    NecroSymbol             prefix    = necro_intern_string(&intern, "superCool");

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

void necro_type_class_translate_test()
{
    // necro_type_class_translate_test_suffix();

    {
        const char* test_name   = "SimplePoly1";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteNothing :: Maybe Int\n"
            "concreteNothing = polyNothing\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
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
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "SimplePoly3";
        const char* test_source = ""
            "polyNothing :: Maybe a\n"
            "polyNothing = Nothing\n"
            "concreteTuple :: (Maybe Bool, Maybe Audio)\n"
            "concreteTuple = (polyNothing, polyNothing)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoublePoly1";
        const char* test_source = ""
            "polyTuple :: a -> b -> (a, b) \n"
            "polyTuple x y = (x, y)\n"
            "concreteTuple :: ((), Bool)\n"
            "concreteTuple = polyTuple () False\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method1";
        const char* test_source = ""
            "getReal :: Float\n"
            "getReal = 0\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method2";
        const char* test_source = ""
            "unreal :: Audio\n"
            "unreal = 10 * 440 / 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Method2";
        const char* test_source = ""
            "unreal :: Audio\n"
            "unreal = 10 * 440 / 3\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Context1";
        const char* test_source = ""
            "inContext :: Num a => a -> a -> a\n"
            "inContext x y = x + y * 1000\n"
            "outOfContext :: Int\n"
            "outOfContext = inContext 10 99\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Left Section";
        const char* test_source = ""
            "left :: Int -> Int \n"
            "left = (10+)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "Right Section";
        const char* test_source = ""
            "right :: Rational -> Rational \n"
            "right = (*33.3)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
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
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
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
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "DoubleContext";
        const char* test_source = ""
            "doubleTrouble :: (Num a, Num b) => a -> b -> (a, b)\n"
            "doubleTrouble x y = (x - 1000, y + 1000)\n"
            "doubleEdge :: (Rational, Float)\n"
            "doubleEdge = doubleTrouble 22 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "HalfContext";
        const char* test_source = ""
            "halfTrouble :: Num a => a -> Int -> (a, Int)\n"
            "halfTrouble x y = (x - 1000, y + 1000)\n"
            "halfEdge :: (Audio, Int)\n"
            "halfEdge = halfTrouble 22.2 33\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
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
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // // BUG / TODO: This should proc defaulting / ambiguous Type class error!
    // {
    //     const char* test_name   = "Context3";
    //     const char* test_source = ""
    //         // "equalizer :: (Num a, Eq a) => a -> a -> Bool\n"
    //         // "equalizer x y = x + y == x\n"
    //         "equalizer :: (Eq a) => a -> a -> Bool\n"
    //         "equalizer x y = x == x\n"
    //         "notTheSame :: Bool\n"
    //         "notTheSame = equalizer 0 100\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    {
        const char* test_name   = "No Monomorphism Restriction 1";
        const char* test_source = ""
            "data Alive = Alive\n"
            "data Dead  = Dead\n"
            "schroedingersMaybe = Nothing\n"
            "alive = schroedingersMaybe\n"
            "dead = schroedingersMaybe\n"
            "cat :: (Maybe Alive, Maybe Dead)\n"
            "cat = (alive, dead)\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    {
        const char* test_name   = "No Monomorphism Restriction 2";
        const char* test_source = ""
            "data Alive = Alive\n"
            "data Dead  = Dead\n"
            "cat :: (Maybe Alive, Maybe Dead)\n"
            "cat = (alive, dead) where\n"
            "  schroedingersMaybe = Nothing\n"
            "  alive :: Maybe Alive\n"
            "  alive = schroedingersMaybe\n"
            "  dead :: Maybe Dead\n"
            "  dead = schroedingersMaybe\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // // BUG / TODO: schroedingersMaybe specialized asts aren't appearing in declaration list?
    // {
    //     const char* test_name   = "No Monomorphism Restriction 3";
    //     const char* test_source = ""
    //         "data Alive = Alive\n"
    //         "data Dead  = Dead\n"
    //         "cat :: (Maybe Alive, Maybe Dead)\n"
    //         "cat = (alive, dead) where\n"
    //         "  schroedingersMaybe = Nothing\n"
    //         "  alive              = schroedingersMaybe\n"
    //         "  dead               = schroedingersMaybe\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    {
        const char* test_name   = "No Monomorphism Restriction 5";
        const char* test_source = ""
            "notMonoFucker = add\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // // BUG / TODO: div<Pattern><Pattern<Float>> is getting dropped!
    // {
    //     const char* test_name   = "Polymorphic methods 1";
    //     const char* test_source = ""
    //         "floatPat :: Pattern Float\n"
    //         "floatPat = 3.3 / 2.2\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    {
        const char* test_name   = "Instance Declarations 1";
        const char* test_source = ""
            "class NumCollection c where\n"
            "  checkOutMyIntCollection :: Int -> c Int\n"
            "instance NumCollection [] where\n"
            "  checkOutMyIntCollection x = [x + 1]\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // // BUG / TODO: div<Pattern><Pattern<Float>> is getting dropped!
    // {
    //     const char* test_name   = "Instance Declarations 2";
    //     const char* test_source = ""
    //         "class NumCollection c where\n"
    //         "  checkOutMyCollection :: Num a => a -> c a\n"
    //         "instance NumCollection [] where\n"
    //         "  checkOutMyCollection x = [x + 1]\n"
    //         "rationalCollection :: [Rational]\n"
    //         "rationalCollection = checkOutMyCollection 22\n";
    //     const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
    //     necro_type_class_translate_test_result(test_name, test_source, expect_error_result, NULL);
    // }

    // // TODO: Finish ambigous type variable error
    // // TODO: Finish defaulting!!!
    // {
    //     const char* test_name   = "Ambiguous Type Var 1";
    //     const char* test_source = ""
    //         "const :: a -> b -> a\n"
    //         "const x y = x\n"
    //         "amb :: Int\n"
    //         "amb = const 22 33\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_AMBIGUOUS_TYPE_VAR;
    //     necro_type_class_translate_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

}


/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "parse/parser.h"
#include "type_class.h"
#include "renamer.h"
#include "base.h"
#include "d_analyzer.h"
#include "driver.h"
#include "itoa.h"
#include "utility/result.h"

typedef enum
{
    NECRO_RENAME_NORMAL,
    NECRO_RENAME_DATA,
    NECRO_RENAME_PAT_ASSIGNMENT,
    NECRO_RENAME_TYPECLASS_CONTEXT,
} NECRO_RENAME_STATE;

typedef struct NecroRenamer
{
    NecroIntern*           intern;
    NecroScopedSymTable*   scoped_symtable;
    NecroSymbol            current_class_instance_symbol;
    NecroSymbol            prev_class_instance_symbol;
    NecroAst*              current_declaration_group;
    NecroAst*              current_type_sig_ast;
    NecroAstArena*         ast_arena;
    NECRO_RENAME_STATE     state;
    bool                   is_internal_rename;
} NecroRenamer;

/*
    TODO: Redo setting declaration_group to be similar to necro_ast_deep_copy? Or would that break d_analysis?
*/

//=====================================================
// Logic
//=====================================================
void necro_swap_renamer_class_symbol(NecroRenamer* renamer)
{
    NecroSymbol class_symbol = renamer->prev_class_instance_symbol;
    renamer->prev_class_instance_symbol = renamer->current_class_instance_symbol;
    renamer->current_class_instance_symbol = class_symbol;

}

void necro_prepend_module_name_to_name(NecroIntern* intern, NecroAstSymbol* ast_symbol)
{
    NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&intern->snapshot_arena);
    ast_symbol->name = necro_intern_string(intern, necro_snapshot_arena_concat_strings(&intern->snapshot_arena, 3, (const char*[]) { ast_symbol->module_name->str, ".", ast_symbol->name->str }));
    necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
}

// Bump the clash suffix counter and append the name with a string version of the counter.
NecroSymbol necro_append_clash_suffix_to_name(NecroAstArena* ast_arena, NecroIntern* intern, const char* name)
{
    ast_arena->clash_suffix++;
    char itoa_buf[NECRO_ITOA_BUF_LENGTH];
    assert(ast_arena->clash_suffix <= UINT32_MAX);
    char* itoa_buf_result = necro_itoa((uint32_t)ast_arena->clash_suffix, itoa_buf, NECRO_ITOA_BUF_LENGTH, 36);
    assert(itoa_buf_result != NULL);
    return necro_intern_string(intern, necro_snapshot_arena_concat_strings(&intern->snapshot_arena, 3u, (const char*[]) { name, "_", itoa_buf_result }));
}

// Take a name and makes it unique if need be.
// If it need to mangle a name it will recursively apply necro_append_clash_suffix_to_name until the name is unique to the module.
// It will the prepend the module name, making the name unique across the whole project.
NecroAstSymbol* necro_get_unique_name(NecroAstArena* ast_arena, NecroIntern* intern, NECRO_NAMESPACE_TYPE namespace_type, NECRO_MANGLE_TYPE mangle_type, NecroAstSymbol* ast_symbol)
{
    UNUSED(ast_arena);
    UNUSED(namespace_type);
    // NecroScope* namespace_scope = (namespace_type == NECRO_VALUE_NAMESPACE) ? ast_arena->module_names : ast_arena->module_type_names;
    necro_prepend_module_name_to_name(intern, ast_symbol);
    // if (necro_scope_contains(namespace_scope, ast_symbol->name) || mangle_type == NECRO_MANGLE_NAME)
    if (mangle_type == NECRO_MANGLE_NAME)
    {
        ast_symbol->name = necro_intern_unique_string(intern, ast_symbol->name->str);
    }
    // while (necro_scope_contains(namespace_scope, ast_symbol->name) || mangle_type == NECRO_MANGLE_NAME)
    // {
    //     NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&intern->snapshot_arena);
    //     mangle_type = NECRO_DONT_MANGLE; // Toggling this simply ensures that we keep mangling until the name is unique.
    //     ast_symbol->name = necro_append_clash_suffix_to_name(ast_arena, intern, ast_symbol->name->str);
    //     necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    // }
    // necro_scope_insert_ast_symbol(&ast_arena->arena, namespace_scope, ast_symbol);
    return ast_symbol;
}

NecroResult(NecroAstSymbol) necro_create_name(NecroAstArena* ast_arena, NecroIntern* intern, NECRO_NAMESPACE_TYPE namespace_type, NECRO_MANGLE_TYPE mangle_type, NecroScope* scope, NecroAstSymbol* ast_symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    assert(ast_symbol != NULL);
    assert(ast_symbol->name != NULL);
    assert(ast_symbol->source_name != NULL);
    assert(ast_symbol->module_name != NULL);
    NecroAstSymbol* out_ast_symbol = necro_scope_find_in_this_scope_ast_symbol(scope, ast_symbol->source_name);
    if (out_ast_symbol != NULL)
    {
        // assert(out_ast_symbol->ast != NULL);
        if (out_ast_symbol->ast != NULL)
            return necro_multiple_definitions_error(ast_symbol, source_loc, end_loc, out_ast_symbol, out_ast_symbol->ast->source_loc, out_ast_symbol->ast->end_loc);
        else
            return necro_multiple_definitions_error(ast_symbol, source_loc, end_loc, out_ast_symbol, zero_loc, zero_loc);
    }
    else
    {
        out_ast_symbol = necro_get_unique_name(ast_arena, intern, namespace_type, mangle_type, ast_symbol);
        necro_scope_insert_ast_symbol(&ast_arena->arena, scope, out_ast_symbol);
        return ok_NecroAstSymbol(out_ast_symbol);
    }
}

NecroResult(NecroAstSymbol) necro_find_name(NecroPagedArena* arena, NecroScope* scope, NecroSymbol source_name, NecroSourceLoc source_loc, NecroSourceLoc end_loc)
{
    assert(source_name != NULL);
    NecroAstSymbol* out_ast_symbol = necro_scope_find_ast_symbol(scope, source_name);
    if (out_ast_symbol != NULL)
    {
        return ok_NecroAstSymbol(out_ast_symbol);
    }
    else
    {
        NecroAstSymbol* missing_name_symbol = necro_ast_symbol_create(arena, source_name, source_name, NULL, NULL);
        return necro_not_in_scope_error(missing_name_symbol, source_loc, end_loc);
        // return necro_not_in_scope_error(ast_symbol, source_loc, end_loc);
    }
}

///////////////////////////////////////////////////////
// Declare pass
///////////////////////////////////////////////////////

NecroResult(NecroAstSymbol) necro_rename_declare(NecroRenamer* renamer, NecroAst* ast)
{
    if (ast == NULL)
        return ok_NecroAstSymbol(NULL);
    switch (ast->type)
    {
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->bin_op.lhs));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->bin_op.rhs));
        break;
    case NECRO_AST_IF_THEN_ELSE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->if_then_else.if_expr));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->if_then_else.then_expr));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->if_then_else.else_expr));
        break;
    case NECRO_AST_TOP_DECL:
        while (ast != NULL)
        {
            necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->top_declaration.declaration));
            ast = ast->top_declaration.next_top_decl;
        }
        break;
    case NECRO_AST_DECL:
        while (ast != NULL)
        {
            necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->declaration.declaration_impl));
            ast = ast->declaration.next_declaration;
        }
        break;
    case NECRO_AST_DECLARATION_GROUP_LIST:
        while (ast != NULL)
        {
            necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->declaration_group_list.declaration_group));
            ast = ast->declaration_group_list.next;
        }
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        if (renamer->current_class_instance_symbol != NULL)
        {
            ast->simple_assignment.ast_symbol->name        = necro_intern_create_type_class_instance_symbol(renamer->intern, ast->simple_assignment.ast_symbol->source_name, renamer->current_class_instance_symbol);
            ast->simple_assignment.ast_symbol->source_name = necro_intern_create_type_class_instance_symbol(renamer->intern, ast->simple_assignment.ast_symbol->source_name, renamer->current_class_instance_symbol);
        }
        else
        {
            ast->simple_assignment.ast_symbol->name = ast->simple_assignment.ast_symbol->source_name;
        }
        ast->simple_assignment.ast_symbol->ast = ast;
        ast->simple_assignment.ast_symbol->module_name = renamer->ast_arena->module_name;
        ast->scope->last_introduced_symbol = NULL;
        NECRO_MANGLE_TYPE mangle_type = (ast->scope->parent == NULL) ? NECRO_DONT_MANGLE : NECRO_MANGLE_NAME; // NECRO_DONT_MANGLE vs NECRO_MANGLE is based on if its on the top level or not
        ast->simple_assignment.ast_symbol = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, mangle_type, ast->scope, ast->simple_assignment.ast_symbol, ast->source_loc, ast->end_loc));
        necro_swap_renamer_class_symbol(renamer);
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->simple_assignment.initializer));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->simple_assignment.rhs));
        necro_swap_renamer_class_symbol(renamer);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        if (renamer->current_class_instance_symbol != NULL)
        {
            ast->apats_assignment.ast_symbol->name        = necro_intern_create_type_class_instance_symbol(renamer->intern, ast->apats_assignment.ast_symbol->source_name, renamer->current_class_instance_symbol);
            ast->apats_assignment.ast_symbol->source_name = necro_intern_create_type_class_instance_symbol(renamer->intern, ast->apats_assignment.ast_symbol->source_name, renamer->current_class_instance_symbol);
        }
        else
        {
            ast->apats_assignment.ast_symbol->name = ast->apats_assignment.ast_symbol->source_name;
        }
        ast->apats_assignment.ast_symbol->ast = ast;
        ast->apats_assignment.ast_symbol->module_name = renamer->ast_arena->module_name;
        necro_prepend_module_name_to_name(renamer->intern, ast->apats_assignment.ast_symbol);
        NecroAstSymbol* ast_symbol2 = necro_scope_find_in_this_scope_ast_symbol(ast->scope, ast->apats_assignment.ast_symbol->source_name);
        if (ast_symbol2 != NULL)
        {
            // NOTE (Curtis 2-22-19): Removing Multi-line definitions for now...
            // // Multi-line definition
            // if (ast_symbol2->name == ast->scope->last_introduced_symbol)
            // {
            //     ast->apats_assignment.ast_symbol = ast_symbol2;
            // }
            // Multiple Definitions error
            // else
            // {
                assert(ast_symbol2->ast != NULL);
                return necro_multiple_definitions_error(ast->apats_assignment.ast_symbol, ast->source_loc, ast->end_loc, ast_symbol2, ast_symbol2->ast->source_loc, ast_symbol2->ast->end_loc);
            // }
        }
        else
        {
            necro_scope_insert_ast_symbol(&renamer->ast_arena->arena, ast->scope, ast->apats_assignment.ast_symbol);
            ast->scope->last_introduced_symbol = ast->apats_assignment.ast_symbol->name;
        }
        necro_swap_renamer_class_symbol(renamer);
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->apats_assignment.apats));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->apats_assignment.rhs));
        necro_swap_renamer_class_symbol(renamer);
        break;
    }

    case NECRO_AST_PAT_ASSIGNMENT:
        if (ast->scope->parent == NULL)
            renamer->state = NECRO_RENAME_PAT_ASSIGNMENT;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->pat_assignment.pat));
        if (ast->scope->parent == NULL)
            renamer->state = NECRO_RENAME_NORMAL;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->pat_assignment.rhs));
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->right_hand_side.declarations));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->right_hand_side.expression));
        break;
    case NECRO_AST_LET_EXPRESSION:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->let_expression.declarations));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->let_expression.expression));
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->fexpression.aexp));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->fexpression.next_fexpression));
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR: break;
        case NECRO_VAR_SIG: ast->scope->last_introduced_symbol = NULL; break;
        case NECRO_VAR_DECLARATION:
            ast->variable.ast_symbol->name = ast->variable.ast_symbol->source_name;
            ast->variable.ast_symbol->module_name = renamer->ast_arena->module_name;
            ast->variable.ast_symbol = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, (renamer->state == NECRO_RENAME_PAT_ASSIGNMENT) ? NECRO_DONT_MANGLE : NECRO_MANGLE_NAME, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc));
            ast->variable.ast_symbol->ast = ast;
            break;
        case NECRO_VAR_TYPE_VAR_DECLARATION:
            ast->variable.ast_symbol->name = ast->variable.ast_symbol->source_name;
            ast->variable.ast_symbol->module_name = renamer->ast_arena->module_name;
            ast->variable.ast_symbol = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_MANGLE_NAME, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc));
            ast->variable.ast_symbol->ast = ast;
            break;
        case NECRO_VAR_CLASS_SIG:
            ast->scope->last_introduced_symbol = NULL;
            ast->variable.ast_symbol->name = ast->variable.ast_symbol->source_name;
            ast->variable.ast_symbol->module_name = renamer->ast_arena->module_name;
            ast->variable.ast_symbol = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_DONT_MANGLE, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc));
            ast->variable.ast_symbol->ast = ast;
            if (ast->variable.ast_symbol->optional_type_signature != NULL)
                return necro_duplicate_type_signatures_error(ast->variable.ast_symbol, renamer->current_type_sig_ast->source_loc, renamer->current_type_sig_ast->end_loc, ast->variable.ast_symbol, ast->variable.ast_symbol->optional_type_signature->source_loc, ast->variable.ast_symbol->optional_type_signature->end_loc);
            else
                ast->variable.ast_symbol->optional_type_signature = renamer->current_type_sig_ast;
            break;
        case NECRO_VAR_TYPE_FREE_VAR:
        {
            NecroAstSymbol* var_ast_symbol = ast->variable.ast_symbol;
            ast->variable.ast_symbol       = necro_scope_find_ast_symbol(ast->scope, ast->variable.ast_symbol->source_name);
            if (ast->variable.ast_symbol == NULL)
            {
                if (renamer->state == NECRO_RENAME_NORMAL)
                {
                    // ast->variable.ast_symbol = necro_ast_symbol_create(&renamer->ast_arena->arena, ast->variable.symbol, ast->variable.symbol, renamer->ast_arena->module_name, ast);
                    ast->variable.ast_symbol = var_ast_symbol;
                    ast->variable.ast_symbol->module_name = renamer->ast_arena->module_name;
                    ast->variable.ast_symbol = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_MANGLE_NAME, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc));
                }
                else
                {
                    return necro_not_in_scope_error(ast->variable.ast_symbol, ast->source_loc, ast->end_loc);
                }
            }
            break;
        }

        default:
            assert(false);
        }
        break;

    case NECRO_AST_APATS:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->apats.apat));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->apats.next_apat));
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->lambda.apats));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->lambda.expression));
        break;
    case NECRO_AST_DO:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->do_statement.statement_list));
        break;
    case NECRO_AST_LIST_NODE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->list.item));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->list.next_item));
        break;
    case NECRO_AST_EXPRESSION_LIST:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->expression_list.expressions));
        break;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->expression_array.expressions));
        break;
    case NECRO_AST_SEQ_EXPRESSION:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->sequence_expression.expressions));
        break;
    case NECRO_AST_TUPLE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->tuple.expressions));
        break;
    case NECRO_BIND_ASSIGNMENT:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->bind_assignment.expression));
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->pat_bind_assignment.pat));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->pat_bind_assignment.expression));
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->arithmetic_sequence.from));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->arithmetic_sequence.then));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->arithmetic_sequence.to));
        break;
    case NECRO_AST_CASE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->case_expression.expression));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->case_expression.alternatives));
        break;
    case NECRO_AST_FOR_LOOP:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->for_loop.range_init));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->for_loop.value_init));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->for_loop.index_apat));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->for_loop.value_apat));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->for_loop.expression));
        break;
    case NECRO_AST_WHILE_LOOP:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->while_loop.value_init));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->while_loop.value_apat));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->while_loop.while_expression));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->while_loop.do_expression));
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->case_alternative.pat));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->case_alternative.body));
        break;

    case NECRO_AST_CONID:
        switch (ast->conid.con_type)
        {
        case NECRO_CON_VAR:
            break;
        case NECRO_CON_TYPE_VAR:
            break;
        case NECRO_CON_TYPE_DECLARATION:
            ast->scope->last_introduced_symbol = NULL;
            ast->conid.ast_symbol->ast         = ast;
            ast->conid.ast_symbol->name        = ast->conid.ast_symbol->source_name;
            ast->conid.ast_symbol->module_name = renamer->ast_arena->module_name;
            ast->conid.ast_symbol              = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_DONT_MANGLE, renamer->scoped_symtable->top_type_scope, ast->conid.ast_symbol, ast->source_loc, ast->end_loc));
            break;
        case NECRO_CON_DATA_DECLARATION:
            ast->scope->last_introduced_symbol = NULL;
            ast->conid.ast_symbol->ast         = ast;
            ast->conid.ast_symbol->name        = ast->conid.ast_symbol->source_name;
            ast->conid.ast_symbol->module_name = renamer->ast_arena->module_name;
            ast->conid.ast_symbol              = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_DONT_MANGLE, renamer->scoped_symtable->top_scope, ast->conid.ast_symbol, ast->source_loc, ast->end_loc));
            break;
        }
        break;

    case NECRO_AST_TYPE_APP:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_app.ty));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_app.next_ty));
        break;
    case NECRO_AST_BIN_OP_SYM:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->bin_op_sym.left));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->bin_op_sym.op));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->bin_op_sym.right));
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->op_left_section.left));
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->op_right_section.right));
        break;
    case NECRO_AST_CONSTRUCTOR:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->constructor.conid));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->constructor.arg_list));
        break;
    case NECRO_AST_SIMPLE_TYPE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->simple_type.type_con));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->simple_type.type_var_list));
        break;

    case NECRO_AST_DATA_DECLARATION:
        ast->scope->last_introduced_symbol = NULL;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->data_declaration.simpletype));
        // TODO: Better distinctions between var types. I.e. be really specific about the usage.
        renamer->state = NECRO_RENAME_DATA;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->data_declaration.constructor_list));
        renamer->state = NECRO_RENAME_NORMAL;
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        ast->scope->last_introduced_symbol = NULL;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_declaration.tycls));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_declaration.tyvar));
        renamer->state = NECRO_RENAME_TYPECLASS_CONTEXT;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_declaration.context));
        renamer->state = NECRO_RENAME_NORMAL;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_declaration.declarations));
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        ast->scope->last_introduced_symbol = NULL;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_instance.qtycls));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_instance.inst));
        renamer->state = NECRO_RENAME_TYPECLASS_CONTEXT;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_instance.context));
        renamer->state = NECRO_RENAME_NORMAL;
        if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
            renamer->current_class_instance_symbol = ast->type_class_instance.inst->conid.ast_symbol->name;
        else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
            renamer->current_class_instance_symbol = ast->type_class_instance.inst->constructor.conid->conid.ast_symbol->name;
        else
            assert(false);
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_instance.declarations));
        renamer->current_class_instance_symbol = NULL;
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        renamer->current_type_sig_ast = ast;
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_signature.var));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_signature.context));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_signature.type));
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_context.conid));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->type_class_context.varid));
        break;
    case NECRO_AST_FUNCTION_TYPE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->function_type.type));
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->function_type.next_on_arrow));
        break;
    case NECRO_AST_TYPE_ATTRIBUTE:
        necro_try(NecroAstSymbol, necro_rename_declare(renamer, ast->attribute.attribute_type));
        break;
    default:
        assert(false);
        break;
    }
    return ok_NecroAstSymbol(NULL);
}

///////////////////////////////////////////////////////
// Var pass
///////////////////////////////////////////////////////

NecroResult(NecroAstSymbol) necro_rename_var(NecroRenamer* renamer, NecroAst* ast)
{
    if (ast == NULL)
        return ok_NecroAstSymbol(NULL);
    switch (ast->type)
    {
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        ast->bin_op.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, ast->scope, ast->bin_op.ast_symbol->source_name, ast->source_loc, ast->end_loc));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->bin_op.lhs));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->bin_op.rhs));
        break;
    case NECRO_AST_IF_THEN_ELSE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->if_then_else.if_expr));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->if_then_else.then_expr));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->if_then_else.else_expr));
        break;
    case NECRO_AST_TOP_DECL:
        while (ast != NULL)
        {
            necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->top_declaration.declaration));
            ast = ast->top_declaration.next_top_decl;
        }
        break;
    case NECRO_AST_DECL:
        while (ast != NULL)
        {
            necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->declaration.declaration_impl));
            ast = ast->declaration.next_declaration;
        }
        break;
    case NECRO_AST_DECLARATION_GROUP_LIST:
        while (ast != NULL)
        {
            necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->declaration_group_list.declaration_group));
            ast = ast->declaration_group_list.next;
        }
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->simple_assignment.initializer));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->simple_assignment.rhs));
        // if (ast->simple_assignment.ast_symbol->declaration_group == NULL)
        if (!renamer->is_internal_rename)
        {
            ast->simple_assignment.ast_symbol->declaration_group = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, ast->simple_assignment.ast_symbol->declaration_group);
            ast->simple_assignment.declaration_group             = ast->simple_assignment.ast_symbol->declaration_group;
        }
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->apats_assignment.apats));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->apats_assignment.rhs));
        // if (ast->apats_assignment.ast_symbol->declaration_group == NULL)
        if (!renamer->is_internal_rename)
        {
            ast->apats_assignment.ast_symbol->declaration_group = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, ast->apats_assignment.ast_symbol->declaration_group);
            ast->apats_assignment.declaration_group             = ast->apats_assignment.ast_symbol->declaration_group;
        }
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        if (ast->pat_assignment.declaration_group == NULL)
        {
            renamer->current_declaration_group    = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
            ast->pat_assignment.declaration_group = renamer->current_declaration_group;
        }
        renamer->state = NECRO_RENAME_PAT_ASSIGNMENT;
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->pat_assignment.pat));
        renamer->current_declaration_group = NULL;
        renamer->state = NECRO_RENAME_NORMAL;
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->pat_assignment.rhs));
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->right_hand_side.declarations));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->right_hand_side.expression));
        break;
    case NECRO_AST_LET_EXPRESSION:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->let_expression.declarations));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->let_expression.expression));
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->fexpression.aexp));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->fexpression.next_fexpression));
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
            ast->variable.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, ast->scope, ast->variable.ast_symbol->source_name, ast->source_loc, ast->end_loc));
            break;
        case NECRO_VAR_SIG:
            ast->variable.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, ast->scope, ast->variable.ast_symbol->source_name, ast->source_loc, ast->end_loc));
            if (ast->variable.ast_symbol->optional_type_signature != NULL)
                return necro_duplicate_type_signatures_error(ast->variable.ast_symbol, renamer->current_type_sig_ast->source_loc, renamer->current_type_sig_ast->end_loc, ast->variable.ast_symbol, ast->variable.ast_symbol->optional_type_signature->source_loc, ast->variable.ast_symbol->optional_type_signature->end_loc);
            else
                ast->variable.ast_symbol->optional_type_signature = renamer->current_type_sig_ast;
            break;
        case NECRO_VAR_DECLARATION:
            // if we are in a pat_assignment, set our declaration_ast
            if (renamer->current_declaration_group != NULL && ast->variable.ast_symbol->declaration_group == NULL)
                ast->variable.ast_symbol->declaration_group = renamer->current_declaration_group;
            break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->variable.initializer));
        break;

    case NECRO_AST_APATS:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->apats.apat));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->apats.next_apat));
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->lambda.apats));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->lambda.expression));
        break;
    case NECRO_AST_DO:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->do_statement.statement_list));
        break;
    case NECRO_AST_LIST_NODE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->list.item));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->list.next_item));
        break;
    case NECRO_AST_EXPRESSION_LIST:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->expression_list.expressions));
        break;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->expression_array.expressions));
        break;
    case NECRO_AST_SEQ_EXPRESSION:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->sequence_expression.expressions));
        break;
    case NECRO_AST_TUPLE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->tuple.expressions));
        break;
    case NECRO_BIND_ASSIGNMENT:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->bind_assignment.expression));
        ast->bind_assignment.ast_symbol = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_MANGLE_NAME, ast->scope, ast->bind_assignment.ast_symbol, ast->source_loc, ast->end_loc));
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->pat_bind_assignment.pat));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->pat_bind_assignment.expression));
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->arithmetic_sequence.from));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->arithmetic_sequence.then));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->arithmetic_sequence.to));
        break;
    case NECRO_AST_CASE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->case_expression.expression));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->case_expression.alternatives));
        break;
    case NECRO_AST_FOR_LOOP:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->for_loop.range_init));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->for_loop.value_init));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->for_loop.index_apat));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->for_loop.value_apat));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->for_loop.expression));
        break;
    case NECRO_AST_WHILE_LOOP:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->while_loop.value_init));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->while_loop.value_apat));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->while_loop.while_expression));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->while_loop.do_expression));
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->case_alternative.pat));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->case_alternative.body));
        break;

    case NECRO_AST_CONID:
        switch (ast->conid.con_type)
        {
        case NECRO_CON_VAR:
            ast->conid.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, ast->scope, ast->conid.ast_symbol->source_name, ast->source_loc, ast->end_loc));
            break;
        case NECRO_CON_TYPE_VAR:
            ast->conid.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, renamer->scoped_symtable->top_type_scope, ast->conid.ast_symbol->source_name, ast->source_loc, ast->end_loc));
            break;
        case NECRO_CON_TYPE_DECLARATION: break; //try_find_name(renamer, ast, renamer->scoped_symtable->top_type_scope, &ast->conid.id, ast->conid.symbol); break;
        case NECRO_CON_DATA_DECLARATION: break;
        }
        break;

    case NECRO_AST_TYPE_APP:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_app.ty));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_app.next_ty));
        break;
    case NECRO_AST_BIN_OP_SYM:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->bin_op_sym.left));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->bin_op_sym.op));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->bin_op_sym.right));
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->op_left_section.left));
        ast->op_left_section.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, ast->scope, ast->op_left_section.ast_symbol->source_name, ast->source_loc, ast->end_loc));
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        ast->op_right_section.ast_symbol = necro_try_result(NecroAstSymbol, necro_find_name(&renamer->ast_arena->arena, ast->scope, ast->op_right_section.ast_symbol->source_name, ast->source_loc, ast->end_loc));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->op_right_section.right));
        break;

    case NECRO_AST_CONSTRUCTOR:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->constructor.conid));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->constructor.arg_list));
        break;

    case NECRO_AST_SIMPLE_TYPE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->simple_type.type_con));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->simple_type.type_var_list));
        break;

    case NECRO_AST_DATA_DECLARATION:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->data_declaration.simpletype));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->data_declaration.constructor_list));
        ast->data_declaration.ast_symbol      = ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol;
        ast->data_declaration.ast_symbol->ast = ast;
        if (ast->data_declaration.ast_symbol->declaration_group == NULL)
        {
            ast->data_declaration.ast_symbol->declaration_group = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
            ast->data_declaration.declaration_group             = ast->data_declaration.ast_symbol->declaration_group;
        }
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_declaration.context));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_declaration.tycls));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_declaration.tyvar));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_declaration.declarations));
        ast->type_class_declaration.ast_symbol      = ast->type_class_declaration.tycls->conid.ast_symbol;
        ast->type_class_declaration.ast_symbol->ast = ast;
        if (ast->type_class_declaration.ast_symbol->declaration_group == NULL)
        {
            ast->type_class_declaration.ast_symbol->declaration_group = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
            ast->type_class_declaration.declaration_group             = ast->type_class_declaration.ast_symbol->declaration_group;
        }
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
    {
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_instance.context));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_instance.qtycls));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_instance.inst));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_instance.declarations));
        NecroAstSymbol* type_class_name = ast->type_class_instance.qtycls->conid.ast_symbol;
        NecroAstSymbol* data_type_name  = NULL;
        if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
            data_type_name = ast->type_class_instance.inst->conid.ast_symbol;
        else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
            data_type_name = ast->type_class_instance.inst->constructor.conid->conid.ast_symbol;
        else
            assert(false);
        NecroSymbol instance_source_name    = necro_intern_create_type_class_instance_symbol(renamer->intern, type_class_name->source_name, data_type_name->source_name);
        NecroSymbol instance_name           = necro_intern_concat_symbols(renamer->intern, renamer->ast_arena->module_name, instance_source_name);
        ast->type_class_instance.ast_symbol = necro_ast_symbol_create(&renamer->ast_arena->arena, instance_name, instance_source_name, renamer->ast_arena->module_name, ast);
        if (necro_scope_find_in_this_scope_ast_symbol(ast->scope, ast->type_class_instance.ast_symbol->source_name) == NULL)
        {
            ast->type_class_instance.ast_symbol      = necro_try_result(NecroAstSymbol, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_DONT_MANGLE, ast->scope, ast->type_class_instance.ast_symbol, ast->source_loc, ast->end_loc));
            ast->type_class_instance.ast_symbol->ast = ast;
        }
        if (ast->type_class_instance.ast_symbol->declaration_group == NULL)
        {
            ast->type_class_instance.ast_symbol->declaration_group = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
            ast->type_class_instance.declaration_group             = ast->type_class_instance.ast_symbol->declaration_group;
        }
        break;
    }

    case NECRO_AST_TYPE_SIGNATURE:
        // TODO / HACK: Hack with global state. Type signatures are precarious and are being abused by primitives.
        // This should get around some of the hackery. Clean up with proper solution later.
        renamer->current_type_sig_ast = ast;
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_signature.var));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_signature.context));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_signature.type));
        if (ast->type_signature.declaration_group == NULL)
            ast->type_signature.declaration_group = necro_ast_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_context.conid));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->type_class_context.varid));
        break;

    case NECRO_AST_FUNCTION_TYPE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->function_type.type));
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->function_type.next_on_arrow));
        break;
    case NECRO_AST_TYPE_ATTRIBUTE:
        necro_try(NecroAstSymbol, necro_rename_var(renamer, ast->attribute.attribute_type));
        break;
    default:
        assert(false);
        break;
    }
    return ok_NecroAstSymbol(NULL);
}

NecroResult(void) necro_rename(NecroCompileInfo info, NecroScopedSymTable* scoped_symtable, NecroIntern* intern, NecroAstArena* ast_arena)
{
    NecroRenamer renamer = (NecroRenamer)
    {
        .intern                    = intern,
        .scoped_symtable           = scoped_symtable,
        .current_declaration_group = NULL,
        .ast_arena                 = ast_arena,
        .state                     = NECRO_RENAME_NORMAL,
        .is_internal_rename        = false,
    };

    // Declare pass
    renamer.current_class_instance_symbol = NULL;
    renamer.prev_class_instance_symbol = NULL;
    renamer.current_declaration_group = NULL;
    renamer.current_type_sig_ast = NULL;
    renamer.state = NECRO_RENAME_NORMAL;
    necro_try_map(NecroAstSymbol, void, necro_rename_declare(&renamer, ast_arena->root));

    // Var pass
    renamer.current_class_instance_symbol = NULL;
    renamer.prev_class_instance_symbol = NULL;
    renamer.current_declaration_group = NULL;
    renamer.current_type_sig_ast = NULL;
    renamer.state = NECRO_RENAME_NORMAL;
    necro_try_map(NecroAstSymbol, void, necro_rename_var(&renamer, ast_arena->root));

    if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_RENAME && info.verbosity > 0))
        necro_ast_arena_print(ast_arena);
    return ok_void();
}

void necro_rename_internal_scope_and_rename(NecroAstArena* ast_arena, NecroScopedSymTable* scoped_symtable, NecroIntern* intern, NecroAst* ast)
{
    necro_build_scopes_go(scoped_symtable, ast);

    NecroRenamer renamer = (NecroRenamer)
    {
        .intern                    = intern,
        .scoped_symtable           = scoped_symtable,
        .current_declaration_group = NULL,
        .ast_arena                 = ast_arena,
        .state                     = NECRO_RENAME_NORMAL,
        .is_internal_rename        = true,
    };

    // Declare pass
    renamer.current_class_instance_symbol = NULL;
    renamer.prev_class_instance_symbol    = NULL;
    renamer.current_declaration_group     = NULL;
    renamer.current_type_sig_ast          = NULL;
    renamer.state                         = NECRO_RENAME_NORMAL;
    unwrap(NecroAstSymbol, necro_rename_declare(&renamer, ast));

    // Var pass
    renamer.current_class_instance_symbol = NULL;
    renamer.prev_class_instance_symbol    = NULL;
    renamer.current_declaration_group     = NULL;
    renamer.current_type_sig_ast          = NULL;
    renamer.state                         = NECRO_RENAME_NORMAL;
    unwrap(NecroAstSymbol, necro_rename_var(&renamer, ast));
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_rename_test_error(const char* test_name, const char* str, NECRO_RESULT_ERROR_TYPE error_type)
{
    // Set up
    NecroIntern         intern = necro_intern_create();
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base = necro_base_compile(&intern, &scoped_symtable);

    NecroLexTokenVector tokens = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast = necro_parse_ast_arena_empty();
    NecroAstArena       ast = necro_ast_arena_empty();
    NecroCompileInfo    info = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    NecroResult(void) result = necro_rename(info, &scoped_symtable, &intern, &ast);

    // Assert
    if (result.type != NECRO_RESULT_ERROR)
    {
        necro_ast_print(ast.root);
    }
    assert(result.type == NECRO_RESULT_ERROR);
    assert(result.error->type == error_type);
    printf("Rename %s test: Passed\n", test_name);

    // Clean up
    necro_result_error_destroy(result.type, result.error);
    necro_ast_arena_destroy(&ast);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(&intern);
}

#define RENAME_TEST_VERBOSE 0

void necro_rename_test_case(const char* test_name, const char* str, NecroIntern* intern, NecroAstArena* ast2)
{
    // Set up
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create();
    NecroBase           base = necro_base_compile(intern, &scoped_symtable);

    NecroLexTokenVector tokens = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast = necro_parse_ast_arena_empty();
    NecroAstArena       ast = necro_ast_arena_empty();
    NecroCompileInfo    info = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, intern, &tokens, necro_intern_string(intern, "Test"), &parse_ast));
    ast = necro_reify(info, intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, intern, &ast));

#if RENAME_TEST_VERBOSE
    necro_ast_print(ast.root);
#endif
    // Assert
    necro_ast_assert_eq(ast.root, ast2->root);
    printf("Rename %s test: Passed\n", test_name);

    // Clean up
    necro_ast_arena_destroy(&ast);
    necro_ast_arena_destroy(ast2);
    necro_base_destroy(&base);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_intern_destroy(intern);
}

void necro_rename_test()
{
    necro_announce_phase("NecroRename");

    // Error tests
    necro_rename_test_error("NotInScope", "whereIsIt = notInScope\n", NECRO_RENAME_NOT_IN_SCOPE);
    necro_rename_test_error("MultipleDeclarations", "multi = 0\nsomethingElse = 1\nmulti = 2\n", NECRO_RENAME_MULTIPLE_DEFINITIONS);
    necro_rename_test_error("MultipleTypeSigs", "multiSig :: Int\nmultiSig :: Float\nmultiSig = 20\n", NECRO_RENAME_MULTIPLE_TYPE_SIGNATURES);

    {
        const char* test_name = "NotInScope: SimpleAssignment";
        const char* test_source = ""
            "data Book = Pages\n"
            "data NotBook = EmptyPages\n"
            "notcronomicon :: Maybe Book\n"
            "notcronomicon = Just Book\n";
        necro_rename_test_error(test_name, test_source, NECRO_RENAME_NOT_IN_SCOPE);
    }

    //--------------------
    // Note:
    // Consult ast_symbol.h / .c for more info on NecroAstSymbols names.
    // We're testing that compiled strings are renamed correctly.
    // To do this we need to hand construct a NecroAST with hand mangled names.
    // Hence the usage of necro_append_clash_suffix_to_name which appends a clash suffix.
    //--------------------
    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.constant1"), necro_intern_string(&intern, "constant1"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_fexpr(&ast.arena, necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.fromInt"), necro_intern_string(&intern, "fromInt"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_VAR_VAR),
                            necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER })),
                        NULL)
                ),
                NULL);
        necro_rename_test_case("Constant1", "constant1 = 10\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   inner_name = necro_append_clash_suffix_to_name(&ast, &intern, "Test.inner");
        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.top"), necro_intern_string(&intern, "top"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_let(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                            necro_ast_create_decl(&ast.arena,
                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                    necro_ast_create_rhs(&ast.arena,
                                        necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                            NECRO_CON_VAR), NULL)
                                ),
                                NULL)),
                        NULL)
                ),
                NULL);
        necro_rename_test_case("Let", "top = let inner = Nothing in inner\n", &intern, &ast);
    }


    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   inner_name = necro_append_clash_suffix_to_name(&ast, &intern, "Test.inner");
        NecroSymbol   other_name = necro_append_clash_suffix_to_name(&ast, &intern, "Test.other");
        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.top"), necro_intern_string(&intern, "top"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,

                        necro_ast_create_let(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                            // declaration1
                            necro_ast_create_decl(&ast.arena,
                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, inner_name, necro_intern_string(&intern, "inner"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                    necro_ast_create_rhs(&ast.arena,
                                        necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                            NECRO_CON_VAR),
                                        NULL)),
                                // declaration2
                                necro_ast_create_decl(&ast.arena,
                                    necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, other_name, necro_intern_string(&intern, "other"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                        necro_ast_create_rhs(&ast.arena,
                                            necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.False"), necro_intern_string(&intern, "False"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                                NECRO_CON_VAR),
                                            NULL)),
                                    NULL))
                        ),
                        NULL)
                ),
                NULL);
        necro_rename_test_case("Let2", "top =\n  let\n    inner = Nothing\n    other = False\n  in inner\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_let(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                            necro_ast_create_decl(&ast.arena,
                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                    necro_ast_create_rhs(&ast.arena,
                                        necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                            NECRO_CON_VAR), NULL)
                                ),
                                NULL)),

                        NULL)
                ),
                NULL);
        necro_rename_test_case("LetClash", "x = let x = Nothing in x\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        NecroSymbol   clash_x2 = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,

                        necro_ast_create_let(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                            necro_ast_create_decl(&ast.arena,
                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                    necro_ast_create_rhs(&ast.arena,
                                        necro_ast_create_let(&ast.arena,
                                            necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x2, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                                            necro_ast_create_decl(&ast.arena,
                                                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x2, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                                    necro_ast_create_rhs(&ast.arena,
                                                        necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL),
                                                            NECRO_CON_VAR), NULL)
                                                ),
                                                NULL)),
                                        NULL)
                                ),
                                NULL)),

                        NULL)
                ),
                NULL);
        necro_rename_test_case("LetClash2", "x = let x = (let x = Nothing in x) in x\n", &intern, &ast);
    }


    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_DECLARATION),
                        necro_ast_create_decl(&ast.arena,
                            necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                necro_ast_create_rhs(&ast.arena,
                                    necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_CON_VAR)
                                    , NULL)),
                            NULL))),
                NULL);
        necro_rename_test_case("WhereClash", "x = x where x = Nothing\n", &intern, &ast);
    }

    // {
    //     NecroIntern   intern = necro_intern_create();
    //     NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
    //     NecroSymbol   clash_x = necro_append_clash_suffix_to_name(&ast, &intern, "Test.x");
    //     ast.root =
    //         necro_ast_create_top_decl(&ast.arena,
    //             necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
    //                 necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NULL,
    //                 necro_ast_create_rhs(&ast.arena,
    //                     necro_ast_create_do(&ast.arena,
    //                         necro_ast_create_list(&ast.arena,
    //                             necro_ast_create_bind_assignment(&ast.arena,
    //                                 necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
    //                                 necro_ast_create_conid_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.Nothing"), necro_intern_string(&intern, "Nothing"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_CON_VAR)
    //                             ),
    //                             necro_ast_create_list(&ast.arena,
    //                                 necro_ast_create_fexpr(&ast.arena,
    //                                     necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.pure"), necro_intern_string(&intern, "pure"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_VAR_VAR),
    //                                     necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_x, necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_VAR)
    //                                 ),
    //                                 NULL)
    //                         )
    //                     ),
    //                     NULL)),
    //             NULL);
    //     // necro_ast_print(ast.root);
    //     necro_rename_test_case("BindClash", "x = do\n  x <- Nothing\n  pure x\n", &intern, &ast);
    // }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
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
                NULL);
        // necro_ast_print(ast.root);

        necro_rename_test_case("ApatsAssignment1", "x y = y\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");
        NecroSymbol   clash_y2 = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
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
                        necro_ast_create_decl(&ast.arena,
                            necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_y2, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                necro_ast_create_rhs(&ast.arena,
                                    necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, clash_y2, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL), NECRO_VAR_VAR), NULL)),
                            NULL)
                    )),
                NULL);
#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif
        necro_rename_test_case("ApatsAssignmentYs", "x y = y where y = y\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");
        NecroSymbol   clash_z = necro_append_clash_suffix_to_name(&ast, &intern, "Test.z");
        NecroSymbol   clash_w = necro_append_clash_suffix_to_name(&ast, &intern, "Test.w");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
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
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, clash_w, necro_intern_string(&intern, "w"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_VAR),
                        necro_ast_create_decl(
                            &ast.arena,
                            necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_w, necro_intern_string(&intern, "w"), necro_intern_string(&intern, "Test"), NULL), NULL,
                                necro_ast_create_rhs(&ast.arena,
                                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                                        necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                                        NECRO_VAR_VAR
                                    ),
                                    NULL
                                )
                            ),
                            NULL
                        )
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        necro_rename_test_case("ApatsAssignment2", "x y z = w where w = y\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                    necro_ast_create_apats(&ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_DECLARATION),
                        necro_ast_create_apats(&ast.arena,
                            necro_ast_create_wildcard(&ast.arena),
                            NULL)
                    ),
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, clash_y, necro_intern_string(&intern, "y"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_VAR),
                        NULL)),
                NULL);

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        necro_rename_test_case("ApatsAssignmentWildCard", "x y _ = y\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                    necro_ast_create_apats(&ast.arena,
                        necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
                        NULL
                    ),
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_fexpr(&ast.arena, necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.fromInt"), necro_intern_string(&intern, "fromInt"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_VAR_VAR),
                            necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER })),
                        NULL
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        necro_rename_test_case("ApatsAssignmentInitializedVar", "x (10) = 10\n", &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                    necro_ast_create_apats(&ast.arena,
                        necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER_PATTERN }),
                        NULL
                    ),
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_fexpr(&ast.arena, necro_ast_create_var_with_ast_symbol(&ast.arena, necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.fromInt"), necro_intern_string(&intern, "fromInt"), necro_intern_string(&intern, "Necro.Base"), NULL), NECRO_VAR_VAR),
                            necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 10, .type = NECRO_AST_CONSTANT_INTEGER })),
                        NULL
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        necro_rename_test_case("ApatsAssignmentNumericLiteral", "x 10 = 10\n", &intern, &ast);
    }

    {
        NecroIntern			intern = necro_intern_create();
        NecroAstArena		ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol			clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
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
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        necro_rename_test_case("ApatsAssignmentPatBind", "x (Just y) = y\n", &intern, &ast);
    }

    {
        NecroIntern			intern = necro_intern_create();
        NecroAstArena		ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol			clash_jz = necro_append_clash_suffix_to_name(&ast, &intern, "Test.jz");
        NecroSymbol			clash_y = necro_append_clash_suffix_to_name(&ast, &intern, "Test.y");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                    necro_ast_create_apats(&ast.arena,
                        necro_ast_create_var_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, clash_jz, necro_intern_string(&intern, "jz"), necro_intern_string(&intern, "Test"), NULL),
                            NECRO_VAR_VAR
                        ),
                        NULL
                    ),
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_fexpr(&ast.arena,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.z"), necro_intern_string(&intern, "z"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_VAR
                            ),
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, clash_jz, necro_intern_string(&intern, "jz"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_VAR
                            )
                        ),
                        necro_ast_create_decl(
                            &ast.arena,
                            necro_ast_create_apats_assignment_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.z"), necro_intern_string(&intern, "z"), necro_intern_string(&intern, "Test"), NULL),
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
                        )
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = ""
            "x jz = z jz\n"
            "   where z (Just y) = y\n";

        necro_rename_test_case("ApatsAssignmentWherePatBind", test_code, &intern, &ast);
    }

#if 0 // Renamer fails this right now :(
    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root = NULL; // not-implemented, just to demo issue below

        const char* test_code = ""
            "add :: Num a => a -> a -> a\n"
            "add a b = a + b\n";

        necro_rename_test_case("TypeClassContext", test_code, &intern, &ast);
    }
#endif

#if 0 // This causes a the renamer to fail the try call but with no user error
    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        ast.root = NULL;

        const char* test_code = ""
            "z :: Num a => a -> a -> a\n" // having this be the type for something not found simply fails in the renamer
            "x y z = y + z\n";

        necro_rename_test_case("TypeClassContext", test_code, &intern, &ast);
    }
#endif

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
            necro_ast_create_top_decl(&ast.arena,
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
                necro_ast_create_top_decl(&ast.arena,
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
                )
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = ""
            "x :: Num a => a -> a -> a\n"
            "x y z = y + z\n";

        necro_rename_test_case("TypeClassContext", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
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
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_type_signature(&ast.arena,
                    NECRO_SIG_DECLARATION,
                    necro_ast_create_var_with_ast_symbol(&ast.arena,
                        necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.x"), necro_intern_string(&intern, "x"), necro_intern_string(&intern, "Test"), NULL),
                        NECRO_VAR_SIG
                    ),
                    NULL,
                    necro_ast_create_type_fn(&ast.arena,
                        necro_ast_create_conid_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena,
                                necro_intern_string(&intern, "Necro.Base.Int"),
                                necro_intern_string(&intern, "Int"),
                                necro_intern_string(&intern, "Necro.Base"),
                                NULL
                            ),
                            NECRO_CON_TYPE_VAR
                        ),
                        necro_ast_create_type_fn(&ast.arena,
                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Necro.Base.Int"),
                                    necro_intern_string(&intern, "Int"),
                                    necro_intern_string(&intern, "Necro.Base"),
                                    NULL
                                ),
                                NECRO_CON_TYPE_VAR
                            ),
                            necro_ast_create_conid_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Necro.Base.Int"),
                                    necro_intern_string(&intern, "Int"),
                                    necro_intern_string(&intern, "Necro.Base"),
                                    NULL
                                ),
                                NECRO_CON_TYPE_VAR
                            )
                        )
                    )
                ),
                necro_ast_create_top_decl(&ast.arena,
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
                )
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = ""
            "x :: Int -> Int -> Int\n"
            "x y z = y + z\n";

        necro_rename_test_case("BinOp", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_a = necro_append_clash_suffix_to_name(&ast, &intern, "Test.a");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_type_class_with_ast_symbols(
                    &ast.arena,
                    necro_ast_symbol_create(
                        &ast.arena,
                        necro_intern_string(&intern, "Test.Doom"),
                        necro_intern_string(&intern, "Doom"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    necro_ast_symbol_create(&ast.arena, clash_a, necro_intern_string(&intern, "a"), necro_intern_string(&intern, "Test"), NULL),
                    NULL, // Context
                    necro_ast_create_decl(
                        &ast.arena,
                        necro_ast_create_type_signature(&ast.arena,
                            NECRO_SIG_TYPE_CLASS,
                            necro_ast_create_var_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.yourDoom"), necro_intern_string(&intern, "yourDoom"), necro_intern_string(&intern, "Test"), NULL),
                                NECRO_VAR_SIG
                            ),
                            NULL,
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
                        ),
                        NULL
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = ""
            "class Doom a where\n"
            "   yourDoom :: a -> a\n";

        necro_rename_test_case("TypeClassDeclaration", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));
        NecroSymbol   clash_a = necro_append_clash_suffix_to_name(&ast, &intern, "Test.a");
        NecroSymbol   clash_b = necro_append_clash_suffix_to_name(&ast, &intern, "Test.b");

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_instance_with_symbol(
                    &ast.arena,
                    necro_ast_symbol_create(
                        &ast.arena,
                        necro_intern_string(&intern, "Necro.Base.Num"),
                        necro_intern_string(&intern, "Num"),
                        necro_intern_string(&intern, "Necro.Base"),
                        NULL
                    ),
                    necro_ast_create_conid_with_ast_symbol(
                        &ast.arena,
                        necro_ast_symbol_create(
                            &ast.arena,
                            necro_intern_string(&intern, "Necro.Base.()"),
                            necro_intern_string(&intern, "()"),
                            necro_intern_string(&intern, "Necro.Base"),
                            NULL
                        ),
                        NECRO_CON_TYPE_VAR
                    ),
                    NULL,
                    necro_ast_create_decl(
                        &ast.arena,
                        necro_ast_create_apats_assignment_with_ast_symbol(
                            &ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.add<()>"), necro_intern_string(&intern, "add<()>"), necro_intern_string(&intern, "Test"), NULL),
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
                        NULL
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = ""
            "instance Num () where\n"
            "   add a b = ()\n";

        necro_rename_test_case("TypeClassInstance", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_data_declaration_with_ast_symbol(
                    &ast.arena,
                    necro_ast_symbol_create(&ast.arena,
                        necro_intern_string(&intern, "Test.MFDoom"),
                        necro_intern_string(&intern, "MFDoom"),
                        necro_intern_string(&intern, "Test"),
                        NULL
                    ),
                    necro_ast_create_simple_type_with_ast_symbol(
                        &ast.arena,
                        necro_ast_symbol_create(&ast.arena,
                            necro_intern_string(&intern, "Test.MFDoom"),
                            necro_intern_string(&intern, "MFDoom"),
                            necro_intern_string(&intern, "Test"),
                            NULL
                        ),
                        NULL
                    ),
                    necro_ast_create_list(
                        &ast.arena,
                        necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                            necro_ast_symbol_create(&ast.arena,
                                necro_intern_string(&intern, "Test.Doom"),
                                necro_intern_string(&intern, "Doom"),
                                necro_intern_string(&intern, "Test"),
                                NULL
                            ),
                            NULL
                        ),
                        necro_ast_create_list(
                            &ast.arena,
                            necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                                necro_ast_symbol_create(&ast.arena,
                                    necro_intern_string(&intern, "Test.DoomII"),
                                    necro_intern_string(&intern, "DoomII"),
                                    necro_intern_string(&intern, "Test"),
                                    NULL
                                ),
                                NULL
                            ),
                            necro_ast_create_list(
                                &ast.arena,
                                necro_ast_create_data_con_with_ast_symbol(&ast.arena,
                                    necro_ast_symbol_create(&ast.arena,
                                        necro_intern_string(&intern, "Test.DoomIII"),
                                        necro_intern_string(&intern, "DoomIII"),
                                        necro_intern_string(&intern, "Test"),
                                        NULL
                                    ),
                                    NULL
                                ),
                                NULL
                            )
                        )
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = "data MFDoom = Doom | DoomII | DoomIII\n";
        necro_rename_test_case("DataDeclaration", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.addOne"), necro_intern_string(&intern, "addOne"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_right_section(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base.+"), necro_intern_string(&intern, "+"), necro_intern_string(&intern, "Necro.Base"), NULL),
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
                                necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER })
                            )
                        ),
                        NULL
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = "addOne = (+1)\n";
        necro_rename_test_case("RightSection", test_code, &intern, &ast);
    }

    {
        NecroIntern   intern = necro_intern_create();
        NecroAstArena ast = necro_ast_arena_create(necro_intern_string(&intern, "Test"));

        ast.root =
            necro_ast_create_top_decl(&ast.arena,
                necro_ast_create_simple_assignment_with_ast_symbol(&ast.arena,
                    necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Test.reciprocal"), necro_intern_string(&intern, "reciprocal"), necro_intern_string(&intern, "Test"), NULL), NULL,
                    necro_ast_create_rhs(&ast.arena,
                        necro_ast_create_left_section(&ast.arena,
                            necro_ast_symbol_create(&ast.arena, necro_intern_string(&intern, "Necro.Base./"), necro_intern_string(&intern, "/"), necro_intern_string(&intern, "Necro.Base"), NULL),
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
                                necro_ast_create_constant(&ast.arena, (NecroParseAstConstant) { .int_literal = 1, .type = NECRO_AST_CONSTANT_INTEGER })
                            )
                        ),
                        NULL
                    )
                ),
                NULL
            );

#if RENAME_TEST_VERBOSE
        necro_ast_print(ast.root);
#endif

        const char* test_code = "reciprocal = (1/)\n";
        necro_rename_test_case("LeftSection", test_code, &intern, &ast);
    }

    // {
    //     puts("Rename {{{ child process rename_test:  starting...");
    //     assert(NECRO_COMPILE_IN_CHILD_PROCESS("rename_test.txt", "rename") == 0);
    //     puts("Rename }}} child process rename_test:  passed\n");
    // }

    // {
    //     puts("Rename {{{ child process parseTest:  starting...");
    //     assert(NECRO_COMPILE_IN_CHILD_PROCESS("parseTest.necro", "rename") == 0);
    //     puts("Rename }}} child process parseTest:  passed\n");
    // }
}

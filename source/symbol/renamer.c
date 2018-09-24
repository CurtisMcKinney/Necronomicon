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
    NecroDeclarationGroup* current_declaration_group;
    NecroAst*              current_type_sig_ast;
    NecroAstArena*         ast_arena;
    NECRO_RENAME_STATE     state;
} NecroRenamer;

//=====================================================
// Logic
//=====================================================
void necro_swap_renamer_class_symbol(NecroRenamer* renamer)
{
    NecroSymbol class_symbol = renamer->prev_class_instance_symbol;
    renamer->prev_class_instance_symbol    = renamer->current_class_instance_symbol;
    renamer->current_class_instance_symbol = class_symbol;

}

NecroAstSymbol necro_get_unique_name(NecroAstArena* ast_arena, NecroIntern* intern, NECRO_NAMESPACE_TYPE namespace_type, NECRO_MANGLE_TYPE mangle_type, NecroAstSymbol ast_symbol)
{
    struct NecroScope* namespace_scope = (namespace_type == NECRO_VALUE_NAMESPACE) ? ast_arena->module_names : ast_arena->module_type_names;
    while (necro_scope_contains(namespace_scope, ast_symbol.name) || mangle_type == NECRO_MANGLE_NAME)
    {
        mangle_type = NECRO_DONT_MANGLE;
        NecroArenaSnapshot snapshot = necro_snapshot_arena_get(&intern->snapshot_arena);
        ast_arena->clash_suffix++;
        char itoa_buf[16];
        const char* new_name   = necro_snapshot_arena_concat_strings(&intern->snapshot_arena, 3, (const char*[]) { ast_symbol.name->str, "_", itoa(ast_arena->clash_suffix, itoa_buf, 36) });
        ast_symbol.name        = necro_intern_string(intern, new_name);
        necro_snapshot_arena_rewind(&intern->snapshot_arena, snapshot);
    }
    necro_scope_insert_ast_symbol(&ast_arena->arena, namespace_scope, ast_symbol);
    return ast_symbol;
}

NecroResult(void) necro_create_name(NecroAstArena* ast_arena, NecroIntern* intern, NECRO_NAMESPACE_TYPE namespace_type, NECRO_MANGLE_TYPE mangle_type, NecroScope* scope, NecroAstSymbol ast_symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAstSymbol* out_ast_symbol)
{
    assert(ast_symbol.name        != NULL);
    assert(ast_symbol.source_name != NULL);
    assert(ast_symbol.module_name != NULL);
    assert(ast_symbol.ast_data    != NULL);
    if (necro_scope_find_in_this_scope_ast_symbol(scope, ast_symbol.source_name, out_ast_symbol))
    {
        assert(out_ast_symbol->ast_data != NULL);
        assert(out_ast_symbol->ast_data->ast != NULL);
        return necro_multiple_definitions_error(ast_symbol, source_loc, end_loc, *out_ast_symbol, out_ast_symbol->ast_data->ast->source_loc, out_ast_symbol->ast_data->ast->end_loc);
    }
    else
    {
        *out_ast_symbol = necro_get_unique_name(ast_arena, intern, namespace_type, mangle_type, ast_symbol);
        necro_scope_insert_ast_symbol(&ast_arena->arena, scope, *out_ast_symbol);
        return ok_void();
    }
}

NecroResult(void) necro_find_name(NecroScope* scope, NecroAstSymbol ast_symbol, NecroSourceLoc source_loc, NecroSourceLoc end_loc, NecroAstSymbol* out_ast_symbol)
{
    assert(ast_symbol.source_name != NULL);
    if (necro_scope_find_ast_symbol(scope, ast_symbol.source_name, out_ast_symbol))
    {
        assert(out_ast_symbol->ast_data != NULL);
        return ok_void();
    }
    else
    {
        return necro_not_in_scope_error(ast_symbol, source_loc, end_loc);
    }
}

///////////////////////////////////////////////////////
// Declare pass
///////////////////////////////////////////////////////

// TODO: Error cons
NecroResult(void) necro_rename_declare(NecroRenamer* renamer, NecroAst* ast)
{
    if (ast == NULL)
        return ok_void();
    switch (ast->type)
    {
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        necro_try(void, necro_rename_declare(renamer, ast->bin_op.lhs));
        necro_try(void, necro_rename_declare(renamer, ast->bin_op.rhs));
        break;
    case NECRO_AST_IF_THEN_ELSE:
        necro_try(void, necro_rename_declare(renamer, ast->if_then_else.if_expr));
        necro_try(void, necro_rename_declare(renamer, ast->if_then_else.then_expr));
        necro_try(void, necro_rename_declare(renamer, ast->if_then_else.else_expr));
        break;
    case NECRO_AST_TOP_DECL:
        necro_try(void, necro_rename_declare(renamer, ast->top_declaration.declaration));
        necro_try(void, necro_rename_declare(renamer, ast->top_declaration.next_top_decl));
        break;
    case NECRO_AST_DECL:
        necro_try(void, necro_rename_declare(renamer, ast->declaration.declaration_impl));
        necro_try(void, necro_rename_declare(renamer, ast->declaration.next_declaration));
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        if (renamer->current_class_instance_symbol != NULL)
            ast->simple_assignment.ast_symbol.name = necro_intern_create_type_class_instance_symbol(renamer->intern, ast->simple_assignment.ast_symbol.source_name, renamer->current_class_instance_symbol);
        else
            ast->simple_assignment.ast_symbol.name = ast->simple_assignment.ast_symbol.source_name;
        ast->simple_assignment.ast_symbol.ast_data->ast = ast;
        ast->simple_assignment.ast_symbol.module_name   = renamer->ast_arena->module_name;
        ast->scope->last_introduced_symbol = NULL;
        necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_DONT_MANGLE, ast->scope, ast->simple_assignment.ast_symbol, ast->source_loc, ast->end_loc, &ast->simple_assignment.ast_symbol));
        necro_swap_renamer_class_symbol(renamer);
        necro_try(void, necro_rename_declare(renamer, ast->simple_assignment.initializer));
        necro_try(void, necro_rename_declare(renamer, ast->simple_assignment.rhs));
        necro_swap_renamer_class_symbol(renamer);
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
        if (renamer->current_class_instance_symbol != NULL)
            ast->apats_assignment.ast_symbol.name = necro_intern_create_type_class_instance_symbol(renamer->intern, ast->apats_assignment.ast_symbol.source_name, renamer->current_class_instance_symbol);
        else
            ast->apats_assignment.ast_symbol.name = ast->apats_assignment.ast_symbol.source_name;
        ast->apats_assignment.ast_symbol.ast_data->ast = ast;
        ast->apats_assignment.ast_symbol.module_name   = renamer->ast_arena->module_name;
        NecroAstSymbol ast_symbol2;
        if (necro_scope_find_in_this_scope_ast_symbol(ast->scope, ast->apats_assignment.ast_symbol.source_name, &ast_symbol2))
        {
            // Multi-line definition
            if (ast_symbol2.name == ast->scope->last_introduced_symbol)
            {
                ast->apats_assignment.ast_symbol = ast_symbol2;
            }
            // Multiple Definitions error
            else
            {
                assert(ast_symbol2.ast_data != NULL);
                assert(ast_symbol2.ast_data->ast != NULL);
                return necro_multiple_definitions_error(ast->apats_assignment.ast_symbol, ast->source_loc, ast->end_loc, ast_symbol2, ast_symbol2.ast_data->ast->source_loc, ast_symbol2.ast_data->ast->end_loc);
            }
        }
        else
        {
            // ast->apats_assignment.ast_symbol   = necro_get_unique_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, ast->apats_assignment.ast_symbol);
            necro_scope_insert_ast_symbol(&renamer->ast_arena->arena, ast->scope, ast->apats_assignment.ast_symbol);
            ast->scope->last_introduced_symbol = ast->apats_assignment.ast_symbol.name;
        }
        necro_swap_renamer_class_symbol(renamer);
        necro_try(void, necro_rename_declare(renamer, ast->apats_assignment.apats));
        necro_try(void, necro_rename_declare(renamer, ast->apats_assignment.rhs));
        necro_swap_renamer_class_symbol(renamer);
        break;

    case NECRO_AST_PAT_ASSIGNMENT:
        renamer->state = NECRO_RENAME_PAT_ASSIGNMENT;
        necro_try(void, necro_rename_declare(renamer, ast->pat_assignment.pat));
        renamer->state = NECRO_RENAME_NORMAL;
        necro_try(void, necro_rename_declare(renamer, ast->pat_assignment.rhs));
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(void, necro_rename_declare(renamer, ast->right_hand_side.declarations));
        necro_try(void, necro_rename_declare(renamer, ast->right_hand_side.expression));
        break;
    case NECRO_AST_LET_EXPRESSION:
        necro_try(void, necro_rename_declare(renamer, ast->let_expression.declarations));
        necro_try(void, necro_rename_declare(renamer, ast->let_expression.expression));
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_try(void, necro_rename_declare(renamer, ast->fexpression.aexp));
        necro_try(void, necro_rename_declare(renamer, ast->fexpression.next_fexpression));
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR: break;
        case NECRO_VAR_SIG: ast->scope->last_introduced_symbol = NULL; break;
        case NECRO_VAR_DECLARATION:
            ast->variable.ast_symbol.ast_data->ast = ast;
            ast->variable.ast_symbol.name          = ast->variable.ast_symbol.source_name;
            ast->variable.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, (renamer->state == NECRO_RENAME_PAT_ASSIGNMENT) ? NECRO_DONT_MANGLE : NECRO_MANGLE_NAME, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc, &ast->variable.ast_symbol));
            break;
        case NECRO_VAR_TYPE_VAR_DECLARATION:
            ast->variable.ast_symbol.ast_data->ast = ast;
            ast->variable.ast_symbol.name          = ast->variable.ast_symbol.source_name;
            ast->variable.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_MANGLE_NAME, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc, &ast->variable.ast_symbol));
            break;
        case NECRO_VAR_CLASS_SIG:
            ast->scope->last_introduced_symbol     = NULL;
            ast->variable.ast_symbol.ast_data->ast = ast;
            ast->variable.ast_symbol.name          = ast->variable.ast_symbol.source_name;
            ast->variable.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_DONT_MANGLE, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc, &ast->variable.ast_symbol));
            if (ast->variable.ast_symbol.ast_data->optional_type_signature != NULL)
                return necro_duplicate_type_signatures_error(ast->variable.ast_symbol, renamer->current_type_sig_ast->source_loc, renamer->current_type_sig_ast->end_loc, ast->variable.ast_symbol, ast->variable.ast_symbol.ast_data->optional_type_signature->source_loc, ast->variable.ast_symbol.ast_data->optional_type_signature->end_loc);
            else
                ast->variable.ast_symbol.ast_data->optional_type_signature = renamer->current_type_sig_ast;
            break;
        case NECRO_VAR_TYPE_FREE_VAR:
            if (!necro_scope_find_ast_symbol(ast->scope, ast->variable.ast_symbol.source_name, &ast->variable.ast_symbol))
            {
                if (renamer->state == NECRO_RENAME_NORMAL)
                {
                    ast->variable.ast_symbol.ast_data->ast = ast;
                    ast->variable.ast_symbol.name          = ast->variable.ast_symbol.source_name;
                    ast->variable.ast_symbol.module_name   = renamer->ast_arena->module_name;
                    necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_MANGLE_NAME, ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc, &ast->variable.ast_symbol));
                }
                else
                {
                    return necro_not_in_scope_error(ast->variable.ast_symbol, ast->source_loc, ast->end_loc);
                }
            }
            break;
        default:
            assert(false);
        }
        break;

    case NECRO_AST_APATS:
        necro_try(void, necro_rename_declare(renamer, ast->apats.apat));
        necro_try(void, necro_rename_declare(renamer, ast->apats.next_apat));
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        necro_try(void, necro_rename_declare(renamer, ast->lambda.apats));
        necro_try(void, necro_rename_declare(renamer, ast->lambda.expression));
        break;
    case NECRO_AST_DO:
        necro_try(void, necro_rename_declare(renamer, ast->do_statement.statement_list));
        break;
    case NECRO_AST_LIST_NODE:
        necro_try(void, necro_rename_declare(renamer, ast->list.item));
        necro_try(void, necro_rename_declare(renamer, ast->list.next_item));
        break;
    case NECRO_AST_EXPRESSION_LIST:
        necro_try(void, necro_rename_declare(renamer, ast->expression_list.expressions));
        break;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_try(void, necro_rename_declare(renamer, ast->expression_array.expressions));
        break;
    case NECRO_AST_PAT_EXPRESSION:
        necro_try(void, necro_rename_declare(renamer, ast->pattern_expression.expressions));
        break;
    case NECRO_AST_TUPLE:
        necro_try(void, necro_rename_declare(renamer, ast->tuple.expressions));
        break;
    case NECRO_BIND_ASSIGNMENT:
        necro_try(void, necro_rename_declare(renamer, ast->bind_assignment.expression));
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(void, necro_rename_declare(renamer, ast->pat_bind_assignment.pat));
        necro_try(void, necro_rename_declare(renamer, ast->pat_bind_assignment.expression));
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(void, necro_rename_declare(renamer, ast->arithmetic_sequence.from));
        necro_try(void, necro_rename_declare(renamer, ast->arithmetic_sequence.then));
        necro_try(void, necro_rename_declare(renamer, ast->arithmetic_sequence.to));
        break;
    case NECRO_AST_CASE:
        necro_try(void, necro_rename_declare(renamer, ast->case_expression.expression));
        necro_try(void, necro_rename_declare(renamer, ast->case_expression.alternatives));
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(void, necro_rename_declare(renamer, ast->case_alternative.pat));
        necro_try(void, necro_rename_declare(renamer, ast->case_alternative.body));
        break;

    case NECRO_AST_CONID:
        switch (ast->conid.con_type)
        {
        case NECRO_CON_VAR:
            break;
        case NECRO_CON_TYPE_VAR:
            break;
        case NECRO_CON_TYPE_DECLARATION:
            ast->scope->last_introduced_symbol  = NULL;
            ast->conid.ast_symbol.ast_data->ast = ast;
            ast->conid.ast_symbol.name          = ast->conid.ast_symbol.source_name;
            ast->conid.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_TYPE_NAMESPACE, NECRO_DONT_MANGLE, renamer->scoped_symtable->top_type_scope, ast->conid.ast_symbol, ast->source_loc, ast->end_loc, &ast->conid.ast_symbol));
            break;
        case NECRO_CON_DATA_DECLARATION:
            ast->scope->last_introduced_symbol  = NULL;
            ast->conid.ast_symbol.ast_data->ast = ast;
            ast->conid.ast_symbol.name          = ast->conid.ast_symbol.source_name;
            ast->conid.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_DONT_MANGLE, renamer->scoped_symtable->top_scope, ast->conid.ast_symbol, ast->source_loc, ast->end_loc, &ast->conid.ast_symbol));
            break;
        }
        break;

    case NECRO_AST_TYPE_APP:
        necro_try(void, necro_rename_declare(renamer, ast->type_app.ty));
        necro_try(void, necro_rename_declare(renamer, ast->type_app.next_ty));
        break;
    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_rename_declare(renamer, ast->bin_op_sym.left));
        necro_try(void, necro_rename_declare(renamer, ast->bin_op_sym.op));
        necro_try(void, necro_rename_declare(renamer, ast->bin_op_sym.right));
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_try(void, necro_rename_declare(renamer, ast->op_left_section.left));
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_try(void, necro_rename_declare(renamer, ast->op_right_section.right));
        break;
    case NECRO_AST_CONSTRUCTOR:
        necro_try(void, necro_rename_declare(renamer, ast->constructor.conid));
        necro_try(void, necro_rename_declare(renamer, ast->constructor.arg_list));
        break;
    case NECRO_AST_SIMPLE_TYPE:
        necro_try(void, necro_rename_declare(renamer, ast->simple_type.type_con));
        necro_try(void, necro_rename_declare(renamer, ast->simple_type.type_var_list));
        break;

    case NECRO_AST_DATA_DECLARATION:
        ast->scope->last_introduced_symbol = NULL;
        necro_try(void, necro_rename_declare(renamer, ast->data_declaration.simpletype));
        // TODO: Better distinctions between var types. I.e. be really specific about the usage.
        renamer->state = NECRO_RENAME_DATA;
        necro_try(void, necro_rename_declare(renamer, ast->data_declaration.constructor_list));
        renamer->state = NECRO_RENAME_NORMAL;
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        ast->scope->last_introduced_symbol = NULL;
        necro_try(void, necro_rename_declare(renamer, ast->type_class_declaration.tycls));
        necro_try(void, necro_rename_declare(renamer, ast->type_class_declaration.tyvar));
        renamer->state = NECRO_RENAME_TYPECLASS_CONTEXT;
        necro_try(void, necro_rename_declare(renamer, ast->type_class_declaration.context));
        renamer->state = NECRO_RENAME_NORMAL;
        necro_try(void, necro_rename_declare(renamer, ast->type_class_declaration.declarations));
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        ast->scope->last_introduced_symbol = NULL;
        necro_try(void, necro_rename_declare(renamer, ast->type_class_instance.qtycls));
        necro_try(void, necro_rename_declare(renamer, ast->type_class_instance.inst));
        renamer->state = NECRO_RENAME_TYPECLASS_CONTEXT;
        necro_try(void, necro_rename_declare(renamer, ast->type_class_instance.context));
        renamer->state = NECRO_RENAME_NORMAL;
        if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
            renamer->current_class_instance_symbol = ast->type_class_instance.inst->conid.ast_symbol.source_name;
        else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
            renamer->current_class_instance_symbol = ast->type_class_instance.inst->constructor.conid->conid.ast_symbol.source_name;
        else
            assert(false);
        necro_try(void, necro_rename_declare(renamer, ast->type_class_instance.declarations));
        renamer->current_class_instance_symbol = NULL;
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        renamer->current_type_sig_ast = ast;
        necro_try(void, necro_rename_declare(renamer, ast->type_signature.var));
        necro_try(void, necro_rename_declare(renamer, ast->type_signature.context));
        necro_try(void, necro_rename_declare(renamer, ast->type_signature.type));
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        necro_try(void, necro_rename_declare(renamer, ast->type_class_context.conid));
        necro_try(void, necro_rename_declare(renamer, ast->type_class_context.varid));
        break;
    case NECRO_AST_FUNCTION_TYPE:
        necro_try(void, necro_rename_declare(renamer, ast->function_type.type));
        necro_try(void, necro_rename_declare(renamer, ast->function_type.next_on_arrow));
        break;
    default:
        assert(false);
        break;
    }
    return ok_void();
}

///////////////////////////////////////////////////////
// Var pass
///////////////////////////////////////////////////////

NecroResult(void) necro_rename_var(NecroRenamer* renamer, NecroAst* ast)
{
    if (ast == NULL)
        return ok_void();
    switch (ast->type)
    {
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        necro_try(void, necro_find_name(ast->scope, ast->bin_op.ast_symbol, ast->source_loc, ast->end_loc, &ast->bin_op.ast_symbol));
        necro_try(void, necro_rename_var(renamer, ast->bin_op.lhs));
        necro_try(void, necro_rename_var(renamer, ast->bin_op.rhs));
        break;
    case NECRO_AST_IF_THEN_ELSE:
        necro_try(void, necro_rename_var(renamer, ast->if_then_else.if_expr));
        necro_try(void, necro_rename_var(renamer, ast->if_then_else.then_expr));
        necro_try(void, necro_rename_var(renamer, ast->if_then_else.else_expr));
        break;
    case NECRO_AST_TOP_DECL:
        necro_try(void, necro_rename_var(renamer, ast->top_declaration.declaration));
        necro_try(void, necro_rename_var(renamer, ast->top_declaration.next_top_decl));
        break;
    case NECRO_AST_DECL:
        necro_try(void, necro_rename_var(renamer, ast->declaration.declaration_impl));
        necro_try(void, necro_rename_var(renamer, ast->declaration.next_declaration));
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        necro_try(void, necro_rename_var(renamer, ast->simple_assignment.initializer));
        necro_try(void, necro_rename_var(renamer, ast->simple_assignment.rhs));
        ast->simple_assignment.ast_symbol.ast_data->declaration_group = necro_declaration_group_append(&renamer->ast_arena->arena, ast, ast->simple_assignment.ast_symbol.ast_data->declaration_group);
        ast->simple_assignment.declaration_group                      = ast->simple_assignment.ast_symbol.ast_data->declaration_group;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        necro_try(void, necro_rename_var(renamer, ast->apats_assignment.apats));
        necro_try(void, necro_rename_var(renamer, ast->apats_assignment.rhs));
        ast->apats_assignment.ast_symbol.ast_data->declaration_group = necro_declaration_group_append(&renamer->ast_arena->arena, ast, ast->apats_assignment.ast_symbol.ast_data->declaration_group);
        ast->apats_assignment.declaration_group                      = ast->apats_assignment.ast_symbol.ast_data->declaration_group;
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        renamer->current_declaration_group    = necro_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
        ast->pat_assignment.declaration_group = renamer->current_declaration_group;
        renamer->state                        = NECRO_RENAME_PAT_ASSIGNMENT;
        necro_try(void, necro_rename_var(renamer, ast->pat_assignment.pat));
        renamer->current_declaration_group    = NULL;
        renamer->state                        = NECRO_RENAME_NORMAL;
        necro_try(void, necro_rename_var(renamer, ast->pat_assignment.rhs));
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        necro_try(void, necro_rename_var(renamer, ast->right_hand_side.declarations));
        necro_try(void, necro_rename_var(renamer, ast->right_hand_side.expression));
        break;
    case NECRO_AST_LET_EXPRESSION:
        necro_try(void, necro_rename_var(renamer, ast->let_expression.declarations));
        necro_try(void, necro_rename_var(renamer, ast->let_expression.expression));
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        necro_try(void, necro_rename_var(renamer, ast->fexpression.aexp));
        necro_try(void, necro_rename_var(renamer, ast->fexpression.next_fexpression));
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
            ast->variable.ast_symbol.ast_data->ast = ast;
            ast->variable.ast_symbol.name          = ast->variable.ast_symbol.source_name;
            ast->variable.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_find_name(ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc, &ast->variable.ast_symbol));
            break;
        case NECRO_VAR_SIG:
            ast->variable.ast_symbol.ast_data->ast = ast;
            ast->variable.ast_symbol.name          = ast->variable.ast_symbol.source_name;
            ast->variable.ast_symbol.module_name   = renamer->ast_arena->module_name;
            necro_try(void, necro_find_name(ast->scope, ast->variable.ast_symbol, ast->source_loc, ast->end_loc, &ast->variable.ast_symbol));
            if (ast->variable.ast_symbol.ast_data->optional_type_signature != NULL)
                return necro_duplicate_type_signatures_error(ast->variable.ast_symbol, renamer->current_type_sig_ast->source_loc, renamer->current_type_sig_ast->end_loc, ast->variable.ast_symbol, ast->variable.ast_symbol.ast_data->optional_type_signature->source_loc, ast->variable.ast_symbol.ast_data->optional_type_signature->end_loc);
            else
                ast->variable.ast_symbol.ast_data->optional_type_signature = renamer->current_type_sig_ast;
            break;
        case NECRO_VAR_DECLARATION:
            // if we are in a pat_assignment, set our declaration_ast
            if (renamer->current_declaration_group != NULL)
                ast->variable.ast_symbol.ast_data->declaration_group = renamer->current_declaration_group;
            break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        necro_try(void, necro_rename_var(renamer, ast->variable.initializer));
        break;

    case NECRO_AST_APATS:
        necro_try(void, necro_rename_var(renamer, ast->apats.apat));
        necro_try(void, necro_rename_var(renamer, ast->apats.next_apat));
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        necro_try(void, necro_rename_var(renamer, ast->lambda.apats));
        necro_try(void, necro_rename_var(renamer, ast->lambda.expression));
        break;
    case NECRO_AST_DO:
        necro_try(void, necro_rename_var(renamer, ast->do_statement.statement_list));
        break;
    case NECRO_AST_LIST_NODE:
        necro_try(void, necro_rename_var(renamer, ast->list.item));
        necro_try(void, necro_rename_var(renamer, ast->list.next_item));
        break;
    case NECRO_AST_EXPRESSION_LIST:
        necro_try(void, necro_rename_var(renamer, ast->expression_list.expressions));
        break;
    case NECRO_AST_EXPRESSION_ARRAY:
        necro_try(void, necro_rename_var(renamer, ast->expression_array.expressions));
        break;
    case NECRO_AST_PAT_EXPRESSION:
        necro_try(void, necro_rename_var(renamer, ast->pattern_expression.expressions));
        break;
    case NECRO_AST_TUPLE:
        necro_try(void, necro_rename_var(renamer, ast->tuple.expressions));
        break;
    case NECRO_BIND_ASSIGNMENT:
        necro_try(void, necro_rename_var(renamer, ast->bind_assignment.expression));
        necro_try(void, necro_create_name(renamer->ast_arena, renamer->intern, NECRO_VALUE_NAMESPACE, NECRO_MANGLE_NAME, ast->scope, ast->bind_assignment.ast_symbol, ast->source_loc, ast->end_loc, &ast->bind_assignment.ast_symbol));
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        necro_try(void, necro_rename_var(renamer, ast->pat_bind_assignment.pat));
        necro_try(void, necro_rename_var(renamer, ast->pat_bind_assignment.expression));
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        necro_try(void, necro_rename_var(renamer, ast->arithmetic_sequence.from));
        necro_try(void, necro_rename_var(renamer, ast->arithmetic_sequence.then));
        necro_try(void, necro_rename_var(renamer, ast->arithmetic_sequence.to));
        break;
    case NECRO_AST_CASE:
        necro_try(void, necro_rename_var(renamer, ast->case_expression.expression));
        necro_try(void, necro_rename_var(renamer, ast->case_expression.alternatives));
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        necro_try(void, necro_rename_var(renamer, ast->case_alternative.pat));
        necro_try(void, necro_rename_var(renamer, ast->case_alternative.body));
        break;

    case NECRO_AST_CONID:
        switch (ast->conid.con_type)
        {
        case NECRO_CON_VAR:
            necro_try(void, necro_find_name(ast->scope, ast->conid.ast_symbol, ast->source_loc, ast->end_loc, &ast->conid.ast_symbol));
            break;
        case NECRO_CON_TYPE_VAR:
            necro_try(void, necro_find_name(renamer->scoped_symtable->top_type_scope, ast->conid.ast_symbol, ast->source_loc, ast->end_loc, &ast->conid.ast_symbol));
            break;
        case NECRO_CON_TYPE_DECLARATION: break; //try_find_name(renamer, ast, renamer->scoped_symtable->top_type_scope, &ast->conid.id, ast->conid.symbol); break;
        case NECRO_CON_DATA_DECLARATION: break;
        }
        break;

    case NECRO_AST_TYPE_APP:
        necro_try(void, necro_rename_var(renamer, ast->type_app.ty));
        necro_try(void, necro_rename_var(renamer, ast->type_app.next_ty));
        break;
    case NECRO_AST_BIN_OP_SYM:
        necro_try(void, necro_rename_var(renamer, ast->bin_op_sym.left));
        necro_try(void, necro_rename_var(renamer, ast->bin_op_sym.op));
        necro_try(void, necro_rename_var(renamer, ast->bin_op_sym.right));
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        necro_try(void, necro_rename_var(renamer, ast->op_left_section.left));
        necro_try(void, necro_find_name(ast->scope, ast->op_left_section.ast_symbol, ast->source_loc, ast->end_loc, &ast->op_left_section.ast_symbol));
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_try(void, necro_find_name(ast->scope, ast->op_right_section.ast_symbol, ast->source_loc, ast->end_loc, &ast->op_right_section.ast_symbol));
        necro_try(void, necro_rename_var(renamer, ast->op_right_section.right));
        break;

    case NECRO_AST_CONSTRUCTOR:
    {
        necro_try(void, necro_rename_var(renamer, ast->constructor.conid));
        necro_try(void, necro_rename_var(renamer, ast->constructor.arg_list));
        NecroAstSymbol ast_symbol = ast->constructor.conid->conid.ast_symbol;
        ast_symbol.ast_data->ast  = ast;
        break;
    }

    case NECRO_AST_SIMPLE_TYPE:
    {
        necro_try(void, necro_rename_var(renamer, ast->simple_type.type_con));
        necro_try(void, necro_rename_var(renamer, ast->simple_type.type_var_list));
        NecroAstSymbol ast_symbol = ast->simple_type.type_con->conid.ast_symbol;
        ast_symbol.ast_data->ast  = ast;
        break;
    }

    case NECRO_AST_DATA_DECLARATION:
    {
        necro_try(void, necro_rename_var(renamer, ast->data_declaration.simpletype));
        necro_try(void, necro_rename_var(renamer, ast->data_declaration.constructor_list));
        NecroAstSymbol ast_symbol               = ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol;
        ast_symbol.ast_data->ast                = ast;
        ast_symbol.ast_data->declaration_group  = necro_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
        ast->data_declaration.declaration_group = ast_symbol.ast_data->declaration_group;
        break;
    }

    case NECRO_AST_TYPE_CLASS_DECLARATION:
    {
        necro_try(void, necro_rename_var(renamer, ast->type_class_declaration.context));
        necro_try(void, necro_rename_var(renamer, ast->type_class_declaration.tycls));
        necro_try(void, necro_rename_var(renamer, ast->type_class_declaration.tyvar));
        necro_try(void, necro_rename_var(renamer, ast->type_class_declaration.declarations));
        NecroAstSymbol ast_symbol                     = ast->type_class_declaration.tycls->conid.ast_symbol;
        ast_symbol.ast_data->ast                      = ast;
        ast_symbol.ast_data->declaration_group        = necro_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
        ast->type_class_declaration.declaration_group = ast_symbol.ast_data->declaration_group;
        break;
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
    {
        necro_try(void, necro_rename_var(renamer, ast->type_class_instance.context));
        necro_try(void, necro_rename_var(renamer, ast->type_class_instance.qtycls));
        necro_try(void, necro_rename_var(renamer, ast->type_class_instance.inst));
        necro_try(void, necro_rename_var(renamer, ast->type_class_instance.declarations));
        NecroAstSymbol ast_symbol                  = ast->type_class_instance.ast_symbol;
        ast_symbol.ast_data->ast                   = ast;
        ast_symbol.ast_data->declaration_group     = necro_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
        ast->type_class_instance.declaration_group = ast_symbol.ast_data->declaration_group;
        break;
    }

    case NECRO_AST_TYPE_SIGNATURE:
        // TODO / HACK: Hack with global state. Type signatures are precarious and are being abused by primitives.
        // This should get around some of the hackery. Clean up with proper solution later.
        renamer->current_type_sig_ast = ast;
        necro_try(void, necro_rename_var(renamer, ast->type_signature.var));
        necro_try(void, necro_rename_var(renamer, ast->type_signature.context));
        necro_try(void, necro_rename_var(renamer, ast->type_signature.type));
        ast->type_signature.declaration_group = necro_declaration_group_append(&renamer->ast_arena->arena, ast, NULL);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        necro_try(void, necro_rename_var(renamer, ast->type_class_context.conid));
        necro_try(void, necro_rename_var(renamer, ast->type_class_context.varid));
        break;

    case NECRO_AST_FUNCTION_TYPE:
        necro_try(void, necro_rename_var(renamer, ast->function_type.type));
        necro_try(void, necro_rename_var(renamer, ast->function_type.next_on_arrow));
        break;

    default:
        assert(false);
        break;
    }
    return ok_void();
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
    };

    // Declare pass
    renamer.current_class_instance_symbol = NULL;
    renamer.prev_class_instance_symbol    = NULL;
    renamer.current_declaration_group     = NULL;
    renamer.current_type_sig_ast          = NULL;
    renamer.state                         = NECRO_RENAME_NORMAL;
    necro_try(void, necro_rename_declare(&renamer, ast_arena->root));

    // Var pass
    renamer.current_class_instance_symbol = NULL;
    renamer.prev_class_instance_symbol    = NULL;
    renamer.current_declaration_group     = NULL;
    renamer.current_type_sig_ast          = NULL;
    renamer.state                         = NECRO_RENAME_NORMAL;
    necro_try(void, necro_rename_var(&renamer, ast_arena->root));

    if (info.compilation_phase == NECRO_PHASE_RENAME && info.verbosity > 0)
        necro_ast_arena_print(ast_arena);
    return ok_void();
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_rename_test_error(const char* test_name, const char* str, NECRO_RESULT_ERROR_TYPE error_type)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);
    UNUSED(base);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    NecroResult(void) result = necro_rename(info, &scoped_symtable, &intern, &ast);

    // Assert
    assert(result.type == NECRO_RESULT_ERROR);
    assert(result.error->type == error_type);
    printf("Parse %s test: Passed\n", test_name);

    // Clean up
    necro_ast_arena_destroy(&ast);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}


void necro_rename_test()
{
    necro_announce_phase("NecroRename");
    necro_rename_test_error("NotInScope", "whereIsIt = notInScope\n", NECRO_RENAME_NOT_IN_SCOPE);
    necro_rename_test_error("MultipleDeclarations", "multi = 0\nsomethingElse = 1\nmulti = 2\n", NECRO_RENAME_MULTIPLE_DEFINITIONS);
    necro_rename_test_error("MultipleTypeSigs", "multiSig :: Int\nmultiSig :: Float\nmultiSig = 20\n", NECRO_RENAME_MULTIPLE_TYPE_SIGNATURES);
}
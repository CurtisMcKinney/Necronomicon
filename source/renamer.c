/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "parser.h"
#include "renamer.h"

//=====================================================
// Logic
//=====================================================
void swap_renamer_class_symbol(NecroRenamer* renamer)
{
    NecroSymbol class_symbol = renamer->prev_class_instance_symbol;
    renamer->prev_class_instance_symbol    = renamer->current_class_instance_symbol;
    renamer->current_class_instance_symbol = class_symbol;

}

bool try_create_name(NecroRenamer* renamer, NecroAST_Node_Reified* node, NecroScope* scope, NecroID* id_to_set, NecroSymbol symbol)
{
    NecroID id = necro_this_scope_find(scope, symbol);
    if (id.id != 0)
    {
        NecroSourceLoc original_source_loc = renamer->scoped_symtable->global_table->data[id.id].source_loc;
        necro_error(&renamer->error, node->source_loc, "Multiple definitions for \'%s\'.\n Original definition found at line: %d", necro_intern_get_string(renamer->scoped_symtable->global_table->intern, symbol), original_source_loc.line);
        return false;
    }
    else
    {
        node->scope->last_introduced_id = id;
        *id_to_set = necro_scoped_symtable_new_symbol_info(renamer->scoped_symtable, scope, necro_create_initial_symbol_info(symbol, node->source_loc, scope));
        return true;
    }
}

void rename_declare_go(NecroAST_Node_Reified* input_node, NecroRenamer* renamer)
{
    if (input_node == NULL || renamer->error.return_code != NECRO_SUCCESS)
        return;
    switch (input_node->type)
    {
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
    {
        rename_declare_go(input_node->bin_op.lhs, renamer);
        rename_declare_go(input_node->bin_op.rhs, renamer);
        break;
    }
    case NECRO_AST_IF_THEN_ELSE:
    {
        rename_declare_go(input_node->if_then_else.if_expr, renamer);
        rename_declare_go(input_node->if_then_else.then_expr, renamer);
        rename_declare_go(input_node->if_then_else.else_expr, renamer);
        break;
    }
    case NECRO_AST_TOP_DECL:
    {
        rename_declare_go(input_node->top_declaration.declaration, renamer);
        rename_declare_go(input_node->top_declaration.next_top_decl, renamer);
        break;
    }
    case NECRO_AST_DECL:
    {
        rename_declare_go(input_node->declaration.declaration_impl, renamer);
        rename_declare_go(input_node->declaration.next_declaration, renamer);
        break;
    }

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        if (renamer->current_class_instance_symbol.id != 0)
            input_node->simple_assignment.variable_name = necro_intern_create_type_class_instance_symbol(renamer->scoped_symtable->global_table->intern, input_node->simple_assignment.variable_name, renamer->current_class_instance_symbol);
        if (!try_create_name(renamer, input_node, input_node->scope, &input_node->simple_assignment.id, input_node->simple_assignment.variable_name))
            return;
        swap_renamer_class_symbol(renamer);
        rename_declare_go(input_node->simple_assignment.rhs, renamer);
        swap_renamer_class_symbol(renamer);
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        if (renamer->current_class_instance_symbol.id != 0)
            input_node->apats_assignment.variable_name = necro_intern_create_type_class_instance_symbol(renamer->scoped_symtable->global_table->intern, input_node->apats_assignment.variable_name, renamer->current_class_instance_symbol);
        NecroID id = necro_this_scope_find(input_node->scope, input_node->apats_assignment.variable_name);
        if (id.id != 0 && id.id != input_node->scope->last_introduced_id.id)
        {
            necro_error(&renamer->error, input_node->source_loc, "Multiple definitions for \'%s\'", necro_intern_get_string(renamer->scoped_symtable->global_table->intern, input_node->apats_assignment.variable_name));
            return;
        }
        else if (id.id != 0 && id.id == input_node->scope->last_introduced_id.id)
        {
            input_node->apats_assignment.id = id;
        }
        else
        {
            input_node->apats_assignment.id       = necro_scoped_symtable_new_symbol_info(renamer->scoped_symtable, input_node->scope, necro_create_initial_symbol_info(input_node->apats_assignment.variable_name, input_node->source_loc, input_node->scope));
            input_node->scope->last_introduced_id = input_node->apats_assignment.id;
        }
        swap_renamer_class_symbol(renamer);
        rename_declare_go(input_node->apats_assignment.apats, renamer);
        rename_declare_go(input_node->apats_assignment.rhs, renamer);
        swap_renamer_class_symbol(renamer);
        break;
    }

    case NECRO_AST_PAT_ASSIGNMENT:
    {
        // input_node->pat_assignment.id.id = 0;
        // if (renamer->current_class_instance_symbol.id != 0)
        // {
        //     input_node->pat_assignment.variable_name = necro_intern_create_type_class_instance_symbol(renamer->scoped_symtable->global_table->intern, NULL_SYMBOL, renamer->current_class_instance_symbol);
        // }
        // else
        // {
        //     input_node->pat_assignment.variable_name = NULL_SYMBOL;
        // }

        // NecroID id = necro_this_scope_find(input_node->scope, input_node->pat_assignment.variable_name);
        // if (id.id != 0 && id.id != input_node->scope->last_introduced_id.id)
        // {
        //     necro_error(&renamer->error, input_node->source_loc, "Multiple definitions for \'%s\'", necro_intern_get_string(renamer->scoped_symtable->global_table->intern, input_node->pat_assignment.variable_name));
        //     return;
        // }
        // else if (id.id != 0 && id.id == input_node->scope->last_introduced_id.id)
        // {
        //     input_node->pat_assignment.id = id;
        // }
        // else
        // {
        //     input_node->pat_assignment.id = necro_scoped_symtable_new_symbol_info(renamer->scoped_symtable, input_node->scope, necro_create_initial_symbol_info(input_node->pat_assignment.variable_name, input_node->source_loc, input_node->scope));
        //     input_node->scope->last_introduced_id = input_node->pat_assignment.id;
        // }

        // swap_renamer_class_symbol(renamer);
        rename_declare_go(input_node->pat_assignment.pat, renamer);
        rename_declare_go(input_node->pat_assignment.rhs, renamer);
        // swap_renamer_class_symbol(renamer);
        break;
    }

    case NECRO_AST_RIGHT_HAND_SIDE:
        rename_declare_go(input_node->right_hand_side.declarations, renamer);
        rename_declare_go(input_node->right_hand_side.expression, renamer);
        break;
    case NECRO_AST_LET_EXPRESSION:
        rename_declare_go(input_node->let_expression.declarations, renamer);
        rename_declare_go(input_node->let_expression.expression, renamer);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        rename_declare_go(input_node->fexpression.aexp, renamer);
        rename_declare_go(input_node->fexpression.next_fexpression, renamer);
        break;

    case NECRO_AST_VARIABLE:
        switch (input_node->variable.var_type)
        {
        case NECRO_VAR_VAR:                  break;
        case NECRO_VAR_SIG:                  input_node->scope->last_introduced_id = (NecroID) { 0 }; break;
        case NECRO_VAR_DECLARATION:          try_create_name(renamer, input_node, input_node->scope, &input_node->variable.id, input_node->variable.symbol); break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: try_create_name(renamer, input_node, input_node->scope, &input_node->variable.id, input_node->variable.symbol); break;
        case NECRO_VAR_CLASS_SIG:
            if (try_create_name(renamer, input_node, input_node->scope, &input_node->variable.id, input_node->variable.symbol))
            {
                NecroID                 var_id         = input_node->variable.id;
                NecroAST_Node_Reified** type_signature = &necro_symtable_get(renamer->scoped_symtable->global_table, var_id)->optional_type_signature;
                if (*type_signature != NULL)
                {
                    necro_error(&renamer->error, input_node->source_loc, "Duplicate type signature for: \'%s\'.\n Original found at: %d",
                                necro_intern_get_string(renamer->scoped_symtable->global_table->intern, input_node->variable.symbol), (*type_signature)->source_loc);
                }
                else
                {
                    (*type_signature) = input_node;
                }
            }
            break;
        // FIIIIIXXXXXXXX
        case NECRO_VAR_TYPE_FREE_VAR:
        {
            NecroID id = necro_scope_find(input_node->scope, input_node->variable.symbol);
            if (id.id != 0)
                input_node->variable.id = id;
            else if (renamer->should_free_type_declare)
                input_node->variable.id = necro_scoped_symtable_new_symbol_info(renamer->scoped_symtable, input_node->scope, necro_create_initial_symbol_info(input_node->variable.symbol, input_node->source_loc, input_node->scope));
            else
                necro_error(&renamer->error, input_node->source_loc, "Not in scope: \'%s\'", necro_intern_get_string(renamer->scoped_symtable->global_table->intern, input_node->variable.symbol));
            break;
        }
        default: assert(false);
        }
        break;

    case NECRO_AST_APATS:
        rename_declare_go(input_node->apats.apat, renamer);
        rename_declare_go(input_node->apats.next_apat, renamer);
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        rename_declare_go(input_node->lambda.apats, renamer);
        rename_declare_go(input_node->lambda.expression, renamer);
        break;
    case NECRO_AST_DO:
        rename_declare_go(input_node->do_statement.statement_list, renamer);
        break;
    case NECRO_AST_LIST_NODE:
        rename_declare_go(input_node->list.item, renamer);
        rename_declare_go(input_node->list.next_item, renamer);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        rename_declare_go(input_node->expression_list.expressions, renamer);
        break;
    case NECRO_AST_TUPLE:
        rename_declare_go(input_node->tuple.expressions, renamer);
        break;
    case NECRO_BIND_ASSIGNMENT:
        rename_declare_go(input_node->bind_assignment.expression, renamer);
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        rename_declare_go(input_node->pat_bind_assignment.expression, renamer);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        rename_declare_go(input_node->arithmetic_sequence.from, renamer);
        rename_declare_go(input_node->arithmetic_sequence.then, renamer);
        rename_declare_go(input_node->arithmetic_sequence.to, renamer);
        break;
    case NECRO_AST_CASE:
        rename_declare_go(input_node->case_expression.expression, renamer);
        rename_declare_go(input_node->case_expression.alternatives, renamer);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        rename_declare_go(input_node->case_alternative.pat, renamer);
        rename_declare_go(input_node->case_alternative.body, renamer);
        break;

    case NECRO_AST_CONID:
        switch (input_node->conid.con_type)
        {
        case NECRO_CON_VAR:              break;
        case NECRO_CON_TYPE_VAR:         break;
        case NECRO_CON_TYPE_DECLARATION: try_create_name(renamer, input_node, renamer->scoped_symtable->top_type_scope, &input_node->conid.id, input_node->conid.symbol); break;
        case NECRO_CON_DATA_DECLARATION: try_create_name(renamer, input_node, renamer->scoped_symtable->top_scope, &input_node->conid.id, input_node->conid.symbol); break;
        }
        break;

    case NECRO_AST_TYPE_APP:
        rename_declare_go(input_node->type_app.ty, renamer);
        rename_declare_go(input_node->type_app.next_ty, renamer);
        break;
    case NECRO_AST_BIN_OP_SYM:
        rename_declare_go(input_node->bin_op_sym.left, renamer);
        rename_declare_go(input_node->bin_op_sym.op, renamer);
        rename_declare_go(input_node->bin_op_sym.right, renamer);
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        rename_declare_go(input_node->op_left_section.left, renamer);
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        rename_declare_go(input_node->op_right_section.right, renamer);
        break;
    case NECRO_AST_CONSTRUCTOR:
        rename_declare_go(input_node->constructor.conid, renamer);
        rename_declare_go(input_node->constructor.arg_list, renamer);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        rename_declare_go(input_node->simple_type.type_con, renamer);
        rename_declare_go(input_node->simple_type.type_var_list, renamer);
        break;
    case NECRO_AST_DATA_DECLARATION:
        rename_declare_go(input_node->data_declaration.simpletype, renamer);
        renamer->should_free_type_declare = false;
        rename_declare_go(input_node->data_declaration.constructor_list, renamer);
        renamer->should_free_type_declare = true;
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        rename_declare_go(input_node->type_class_declaration.tycls, renamer);
        rename_declare_go(input_node->type_class_declaration.tyvar, renamer);
        renamer->should_free_type_declare = false;
        rename_declare_go(input_node->type_class_declaration.context, renamer);
        renamer->should_free_type_declare = true;
        rename_declare_go(input_node->type_class_declaration.declarations, renamer);
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        rename_declare_go(input_node->type_class_instance.qtycls, renamer);
        rename_declare_go(input_node->type_class_instance.inst, renamer);
        renamer->should_free_type_declare = false;
        rename_declare_go(input_node->type_class_instance.context, renamer);
        renamer->should_free_type_declare = true;
        if (input_node->type_class_instance.inst->type == NECRO_AST_CONID)
            renamer->current_class_instance_symbol = input_node->type_class_instance.inst->conid.symbol;
        else if (input_node->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
            renamer->current_class_instance_symbol = input_node->type_class_instance.inst->constructor.conid->conid.symbol;
        else
            assert(false);
        rename_declare_go(input_node->type_class_instance.declarations, renamer);
        renamer->current_class_instance_symbol = (NecroSymbol) { 0 };
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        rename_declare_go(input_node->type_signature.var, renamer);
        rename_declare_go(input_node->type_signature.context, renamer);
        rename_declare_go(input_node->type_signature.type, renamer);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        rename_declare_go(input_node->type_class_context.conid, renamer);
        rename_declare_go(input_node->type_class_context.varid, renamer);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        rename_declare_go(input_node->function_type.type, renamer);
        rename_declare_go(input_node->function_type.next_on_arrow, renamer);
        break;
    default:
        necro_error(&renamer->error, input_node->source_loc, "Unrecognized AST Node type found: %d", input_node->type);
        break;
    }
}

bool try_find_name(NecroRenamer* renamer, NecroAST_Node_Reified* node, NecroScope* scope, NecroID* id_to_set, NecroSymbol symbol)
{
    NecroID id = necro_scope_find(scope, symbol);
    if (id.id == 0)
    {
        necro_error(&renamer->error, node->source_loc, "Not in scope: \'%s\'", necro_intern_get_string(renamer->scoped_symtable->global_table->intern, symbol));
        return false;
    }
    else
    {
        *id_to_set = id;
        return true;
    }
}


void rename_var_go(NecroAST_Node_Reified* input_node, NecroRenamer* renamer)
{
    if (input_node == NULL || renamer->error.return_code != NECRO_SUCCESS)
        return;
    switch (input_node->type)
    {
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        if (!try_find_name(renamer, input_node, input_node->scope, &input_node->bin_op.id, input_node->bin_op.symbol))
            return;
        rename_var_go(input_node->bin_op.lhs, renamer);
        rename_var_go(input_node->bin_op.rhs, renamer);
        break;
    case NECRO_AST_IF_THEN_ELSE:
        rename_var_go(input_node->if_then_else.if_expr, renamer);
        rename_var_go(input_node->if_then_else.then_expr, renamer);
        rename_var_go(input_node->if_then_else.else_expr, renamer);
        break;
    case NECRO_AST_TOP_DECL:
        rename_var_go(input_node->top_declaration.declaration, renamer);
        rename_var_go(input_node->top_declaration.next_top_decl, renamer);
        break;
    case NECRO_AST_DECL:
        rename_var_go(input_node->declaration.declaration_impl, renamer);
        rename_var_go(input_node->declaration.next_declaration, renamer);
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        rename_var_go(input_node->simple_assignment.rhs, renamer);
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        rename_var_go(input_node->apats_assignment.apats, renamer);
        rename_var_go(input_node->apats_assignment.rhs, renamer);
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        rename_var_go(input_node->pat_assignment.pat, renamer);
        rename_var_go(input_node->pat_assignment.rhs, renamer);
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        rename_var_go(input_node->right_hand_side.declarations, renamer);
        rename_var_go(input_node->right_hand_side.expression, renamer);
        break;
    case NECRO_AST_LET_EXPRESSION:
        rename_var_go(input_node->let_expression.declarations, renamer);
        rename_var_go(input_node->let_expression.expression, renamer);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        rename_var_go(input_node->fexpression.aexp, renamer);
        rename_var_go(input_node->fexpression.next_fexpression, renamer);
        break;

    case NECRO_AST_VARIABLE:
        switch (input_node->variable.var_type)
        {
        case NECRO_VAR_VAR:
            try_find_name(renamer, input_node, input_node->scope, &input_node->variable.id, input_node->variable.symbol);
            break;
        case NECRO_VAR_SIG:
            if (try_find_name(renamer, input_node, input_node->scope, &input_node->variable.id, input_node->variable.symbol))
            {
                NecroID                 var_id         = input_node->variable.id;
                NecroAST_Node_Reified** type_signature = &necro_symtable_get(renamer->scoped_symtable->global_table, var_id)->optional_type_signature;
                if (*type_signature != NULL)
                {
                    necro_error(&renamer->error, input_node->source_loc, "Duplicate type signature for: \'%s\'.\n Original found at: %d",
                                necro_intern_get_string(renamer->scoped_symtable->global_table->intern, input_node->variable.symbol), (*type_signature)->source_loc);
                }
                else
                {
                    (*type_signature) = input_node;
                }
            }
            break;
        case NECRO_VAR_DECLARATION:          break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        break;

    case NECRO_AST_APATS:
        rename_var_go(input_node->apats.apat, renamer);
        rename_var_go(input_node->apats.next_apat, renamer);
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        rename_var_go(input_node->lambda.apats, renamer);
        rename_var_go(input_node->lambda.expression, renamer);
        break;
    case NECRO_AST_DO:
        rename_var_go(input_node->do_statement.statement_list, renamer);
        break;
    case NECRO_AST_LIST_NODE:
        rename_var_go(input_node->list.item, renamer);
        rename_var_go(input_node->list.next_item, renamer);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        rename_var_go(input_node->expression_list.expressions, renamer);
        break;
    case NECRO_AST_TUPLE:
        rename_var_go(input_node->tuple.expressions, renamer);
        break;
    case NECRO_BIND_ASSIGNMENT:
        rename_var_go(input_node->bind_assignment.expression, renamer);
        if (!try_create_name(renamer, input_node, input_node->scope, &input_node->bind_assignment.id, input_node->bind_assignment.variable_name))
            return;
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        rename_var_go(input_node->pat_bind_assignment.pat, renamer);
        rename_var_go(input_node->pat_bind_assignment.expression, renamer);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        rename_var_go(input_node->arithmetic_sequence.from, renamer);
        rename_var_go(input_node->arithmetic_sequence.then, renamer);
        rename_var_go(input_node->arithmetic_sequence.to, renamer);
        break;
    case NECRO_AST_CASE:
        rename_var_go(input_node->case_expression.expression, renamer);
        rename_var_go(input_node->case_expression.alternatives, renamer);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        rename_var_go(input_node->case_alternative.pat, renamer);
        rename_var_go(input_node->case_alternative.body, renamer);
        break;

    case NECRO_AST_CONID:
        switch (input_node->conid.con_type)
        {
        case NECRO_CON_VAR:              try_find_name(renamer, input_node, input_node->scope, &input_node->conid.id, input_node->conid.symbol); break;
        case NECRO_CON_TYPE_VAR:         try_find_name(renamer, input_node, renamer->scoped_symtable->top_type_scope, &input_node->conid.id, input_node->conid.symbol); break;
        case NECRO_CON_TYPE_DECLARATION: break; //try_find_name(renamer, input_node, renamer->scoped_symtable->top_type_scope, &input_node->conid.id, input_node->conid.symbol); break;
        case NECRO_CON_DATA_DECLARATION: break;
        }
        break;

    case NECRO_AST_TYPE_APP:
        rename_var_go(input_node->type_app.ty, renamer);
        rename_var_go(input_node->type_app.next_ty, renamer);
        break;
    case NECRO_AST_BIN_OP_SYM:
        rename_var_go(input_node->bin_op_sym.left, renamer);
        rename_var_go(input_node->bin_op_sym.op, renamer);
        rename_var_go(input_node->bin_op_sym.right, renamer);
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        rename_var_go(input_node->op_left_section.left, renamer);
        if (!try_find_name(renamer, input_node, input_node->scope, &input_node->op_left_section.id, input_node->op_left_section.symbol))
            return;
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        if (!try_find_name(renamer, input_node, input_node->scope, &input_node->op_right_section.id, input_node->op_right_section.symbol))
            return;
        rename_var_go(input_node->op_right_section.right, renamer);
        break;
    case NECRO_AST_CONSTRUCTOR:
        rename_var_go(input_node->constructor.conid, renamer);
        rename_var_go(input_node->constructor.arg_list, renamer);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        rename_var_go(input_node->simple_type.type_con, renamer);
        rename_var_go(input_node->simple_type.type_var_list, renamer);
        break;
    case NECRO_AST_DATA_DECLARATION:
        rename_var_go(input_node->data_declaration.simpletype, renamer);
        rename_var_go(input_node->data_declaration.constructor_list, renamer);
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        rename_var_go(input_node->type_class_declaration.context, renamer);
        rename_var_go(input_node->type_class_declaration.tycls, renamer);
        rename_var_go(input_node->type_class_declaration.tyvar, renamer);
        rename_var_go(input_node->type_class_declaration.declarations, renamer);
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        rename_var_go(input_node->type_class_instance.context, renamer);
        rename_var_go(input_node->type_class_instance.qtycls, renamer);
        rename_var_go(input_node->type_class_instance.inst, renamer);
        rename_var_go(input_node->type_class_instance.declarations, renamer);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        rename_var_go(input_node->type_signature.var, renamer);
        rename_var_go(input_node->type_signature.context, renamer);
        rename_var_go(input_node->type_signature.type, renamer);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        rename_var_go(input_node->type_class_context.conid, renamer);
        rename_var_go(input_node->type_class_context.varid, renamer);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        rename_var_go(input_node->function_type.type, renamer);
        rename_var_go(input_node->function_type.next_on_arrow, renamer);
        break;
    default:
        necro_error(&renamer->error, input_node->source_loc, "Unrecognized AST Node type found: %d", input_node->type);
        break;
    }
}

NecroRenamer necro_create_renamer(NecroScopedSymTable* scoped_symtable)
{
    return (NecroRenamer)
    {
        .scoped_symtable          = scoped_symtable,
        .should_free_type_declare = true,
    };
}

void necro_destroy_renamer(NecroRenamer* renamer)
{
}

NECRO_RETURN_CODE necro_rename_declare_pass(NecroRenamer* renamer, NecroAST_Node_Reified* input_ast)
{
    renamer->error.return_code             = NECRO_SUCCESS;
    renamer->current_class_instance_symbol = (NecroSymbol) { 0 };
    renamer->prev_class_instance_symbol    = (NecroSymbol) { 0 };
    rename_declare_go(input_ast, renamer);
    return renamer->error.return_code;
}

NECRO_RETURN_CODE necro_rename_var_pass(NecroRenamer* renamer, NecroAST_Node_Reified* input_ast)
{
    renamer->error.return_code             = NECRO_SUCCESS;
    renamer->current_class_instance_symbol = (NecroSymbol) { 0 };
    renamer->prev_class_instance_symbol    = (NecroSymbol) { 0 };
    rename_var_go(input_ast, renamer);
    return renamer->error.return_code;
}
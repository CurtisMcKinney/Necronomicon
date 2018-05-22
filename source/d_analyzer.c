/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "type_class.h"
#include "d_analyzer.h"

void necro_strong_connect1(NecroDeclarationGroup* group)
{
    group->index    = group->info->index;
    group->low_link = group->info->index;
    group->info->index++;
    necro_push_declaration_group_vector(&group->info->stack, &group);
    group->on_stack            = true;
    group->info->current_group = group;
}

void necro_strong_connect2(NecroPagedArena* arena, NecroDeclarationGroup* group)
{
    // This might actually just construct things how we require them in dependency order?
    // If we're a root node
    if (group->low_link == group->index)
    {
        // group->info->group_lists                                = necro_prepend_declaration_group_list(arena, group, group->info->group_lists);
        if (group->info->group_lists->declaration_group != NULL)
            necro_append_declaration_group_list(arena, NULL, group->info->group_lists);
        NecroDeclarationGroupList* strongly_connected_component = group->info->group_lists;
        while (strongly_connected_component->next != NULL) strongly_connected_component = strongly_connected_component->next;
        NecroDeclarationGroup* w = NULL;
        do
        {
            w = necro_pop_declaration_group_vector(&group->info->stack);
            assert(w != NULL);
            w->on_stack = false;
            // necro_append_declaration_group_to_group_in_group_list(arena, strongly_connected_component, w);
            necro_prepend_declaration_group_to_group_in_group_list(arena, strongly_connected_component, w);
        } while (w != group);
        assert(strongly_connected_component->declaration_group != NULL);
        assert(strongly_connected_component->declaration_group == group);
    }
}

void d_analyze_go(NecroDependencyAnalyzer* d_analyzer, NecroASTNode* ast);
void d_analyze_var(NecroDependencyAnalyzer* d_analyzer, NecroID id)
{
    NecroSymbolInfo* symbol_info = necro_symtable_get(d_analyzer->symtable, id);
    assert(symbol_info != NULL);
    if (symbol_info->declaration_group == NULL) return;
    NecroDeclarationGroup* w = necro_symtable_get(d_analyzer->symtable, id)->declaration_group;
    assert(w->info != NULL);
    if (w->info->current_group == NULL)
        w->info->current_group = w;
    NecroDeclarationGroup* v = w->info->current_group;
    assert(v != NULL);
    if (w->index == -1)
    {
        symbol_info->declaration_group->info->current_group = w;
        d_analyze_go(d_analyzer, w->declaration_ast);
        v->low_link = min(w->low_link, v->low_link);
    }
    else if (w->on_stack)
    {
        v->low_link = min(w->low_link, v->low_link);
    }
    symbol_info->declaration_group->info->current_group = v;
}

void d_analyze_go(NecroDependencyAnalyzer* d_analyzer, NecroASTNode* ast)
{
    if (ast == NULL || d_analyzer->error.return_code != NECRO_SUCCESS)
        return;
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_TOP_DECL:
    {
        NecroASTNode*          curr = ast;
        NecroDeclarationsInfo* info = necro_create_declarations_info(d_analyzer->arena);
        NecroDeclarationGroup* temp = NULL;
        //-----------------------------------------
        // Pass 1, assign groups and info
        while (curr != NULL)
        {
            switch (curr->top_declaration.declaration->type)
            {
            case NECRO_AST_SIMPLE_ASSIGNMENT:
                curr->top_declaration.declaration->simple_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, curr->top_declaration.declaration->simple_assignment.id)->declaration_group;
                temp = curr->top_declaration.declaration->simple_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->info = info;
                    temp = temp->next;
                }
                break;
            case NECRO_AST_APATS_ASSIGNMENT:
                curr->top_declaration.declaration->apats_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, curr->top_declaration.declaration->apats_assignment.id)->declaration_group;
                temp = curr->top_declaration.declaration->apats_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->info = info;
                    temp = temp->next;
                }
                break;
            case NECRO_AST_PAT_ASSIGNMENT:
                curr->top_declaration.declaration->pat_assignment.declaration_group->info = info;
                break;
            case NECRO_AST_DATA_DECLARATION:
                curr->top_declaration.declaration->data_declaration.declaration_group->info = info;
                break;
            case NECRO_AST_TYPE_CLASS_DECLARATION:
                curr->top_declaration.declaration->type_class_declaration.declaration_group->info = info;
                break;
            case NECRO_AST_TYPE_CLASS_INSTANCE:
                curr->top_declaration.declaration->type_class_instance.declaration_group->info = info;
                break;
            case NECRO_AST_TYPE_SIGNATURE:
                curr->top_declaration.declaration->type_signature.declaration_group->info = info;
                break;
            // Note: Should never reach here
            default: assert(false);  break;
            }
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 2, analyze Data Declarations
        curr = ast;
        while (curr != NULL)
        {
            NecroAST_NodeType type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_DATA_DECLARATION)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 3, TypeClass Declarations, TypeClass Instances
        curr = ast;
        while (curr != NULL)
        {
            NecroAST_NodeType type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_TYPE_CLASS_DECLARATION ||
                type == NECRO_AST_TYPE_CLASS_INSTANCE)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 4, analyze Type Signatures
        curr = ast;
        while (curr != NULL)
        {
            NecroAST_NodeType type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_TYPE_SIGNATURE)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 5, analyze Terms
        curr = ast;
        while (curr != NULL)
        {
            NecroAST_NodeType type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_SIMPLE_ASSIGNMENT ||
                type == NECRO_AST_APATS_ASSIGNMENT  ||
                type == NECRO_AST_PAT_ASSIGNMENT)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        ast->top_declaration.group_list = info->group_lists;
        necro_destroy_declaration_group_vector(&info->stack);
        break;
    }

    case NECRO_AST_DECL:
    {
        NecroASTNode*          curr = ast;
        NecroDeclarationsInfo* info = necro_create_declarations_info(d_analyzer->arena);
        NecroDeclarationGroup* temp = NULL;
        //-----------------------------------------
        // Pass 1, assign groups and info
        while (curr != NULL)
        {
            switch (curr->declaration.declaration_impl->type)
            {
            case NECRO_AST_SIMPLE_ASSIGNMENT:
                curr->declaration.declaration_impl->simple_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, curr->declaration.declaration_impl->simple_assignment.id)->declaration_group;
                temp = curr->declaration.declaration_impl->simple_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->info = info;
                    temp = temp->next;
                }
                break;
            case NECRO_AST_APATS_ASSIGNMENT:
                curr->declaration.declaration_impl->apats_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, curr->declaration.declaration_impl->apats_assignment.id)->declaration_group;
                temp = curr->declaration.declaration_impl->apats_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->info = info;
                    temp = temp->next;
                }
                break;
            case NECRO_AST_PAT_ASSIGNMENT:
                curr->declaration.declaration_impl->pat_assignment.declaration_group->info = info;
                break;
            case NECRO_AST_TYPE_SIGNATURE:
                curr->top_declaration.declaration->type_signature.declaration_group->info = info;
                break;
            // NOTE: Should never reach
            default: assert(false); break;
            }
            curr = curr->declaration.next_declaration;
        }
        //-----------------------------------------
        // Pass 2, analyze TypeSignatures
        curr = ast;
        while (curr != NULL)
        {
            NecroAST_NodeType type = curr->declaration.declaration_impl->type;
            if (type == NECRO_AST_TYPE_SIGNATURE)
                d_analyze_go(d_analyzer, curr->declaration.declaration_impl);
            curr = curr->declaration.next_declaration;
        }
        //-----------------------------------------
        // Pass 3, analyze everything else
        curr = ast;
        while (curr != NULL)
        {
            NecroAST_NodeType type = curr->declaration.declaration_impl->type;
            if (type == NECRO_AST_SIMPLE_ASSIGNMENT ||
                type == NECRO_AST_APATS_ASSIGNMENT  ||
                type == NECRO_AST_PAT_ASSIGNMENT)
                d_analyze_go(d_analyzer, curr->declaration.declaration_impl);
            curr = curr->declaration.next_declaration;
        }
        ast->declaration.group_list = info->group_lists;
        necro_destroy_declaration_group_vector(&info->stack);
        break;
    }

    //=====================================================
    // Assignment type things
    //=====================================================
    // TODO: Force type signatures first!
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        if (ast->simple_assignment.declaration_group->index != -1) return;
        assert(ast->simple_assignment.declaration_group != NULL);
        if (necro_symtable_get(d_analyzer->symtable, ast->simple_assignment.id)->optional_type_signature != NULL)
            d_analyze_go(d_analyzer, necro_symtable_get(d_analyzer->symtable, ast->simple_assignment.id)->optional_type_signature);
        NecroDeclarationGroup* initial_group = ast->simple_assignment.declaration_group;
        NecroDeclarationGroup* current_group = initial_group;
        while (current_group != NULL)
        {
            // current_group->info = initial_group->info;
            necro_strong_connect1(current_group);
            if (current_group->declaration_ast->type != NECRO_AST_SIMPLE_ASSIGNMENT)
            {
                necro_error(&d_analyzer->error, ast->source_loc, "Multiple declarations of: %s", necro_intern_get_string(d_analyzer->intern, ast->simple_assignment.variable_name));
                return;
            }
            d_analyze_go(d_analyzer, current_group->declaration_ast->simple_assignment.rhs);
            initial_group->low_link = min(initial_group->low_link, current_group->low_link);
            current_group = current_group->next;
        }
        current_group = initial_group;
        NecroDeclarationGroup* prev_group = NULL;
        while (current_group != NULL)
        {
            current_group->low_link = initial_group->low_link;
            prev_group              = current_group;
            current_group           = current_group->next;
            prev_group->next        = NULL;
        }
        ast->simple_assignment.declaration_group->info->current_group = ast->simple_assignment.declaration_group;
        necro_strong_connect2(d_analyzer->arena, ast->simple_assignment.declaration_group);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        if (ast->apats_assignment.declaration_group->index != -1) return;
        assert(ast->apats_assignment.declaration_group != NULL);
        if (necro_symtable_get(d_analyzer->symtable, ast->apats_assignment.id)->optional_type_signature != NULL)
            d_analyze_go(d_analyzer, necro_symtable_get(d_analyzer->symtable, ast->apats_assignment.id)->optional_type_signature);
        NecroDeclarationGroup* initial_group = ast->apats_assignment.declaration_group;
        NecroDeclarationGroup* current_group = initial_group;
        while (current_group != NULL)
        {
            // current_group->info = initial_group->info;
            necro_strong_connect1(current_group);
            if (current_group->declaration_ast->type != NECRO_AST_APATS_ASSIGNMENT)
            {
                necro_error(&d_analyzer->error, ast->source_loc, "Multiple declarations of: %s", necro_intern_get_string(d_analyzer->intern, ast->apats_assignment.variable_name));
                return;
            }
            d_analyze_go(d_analyzer, current_group->declaration_ast->apats_assignment.apats);
            d_analyze_go(d_analyzer, current_group->declaration_ast->apats_assignment.rhs);
            initial_group->low_link = min(initial_group->low_link, current_group->low_link);
            current_group = current_group->next;
        }
        current_group = initial_group;
        NecroDeclarationGroup* prev_group = NULL;
        while (current_group != NULL)
        {
            current_group->low_link = initial_group->low_link;
            prev_group              = current_group;
            current_group           = current_group->next;
            prev_group->next        = NULL;
        }
        ast->apats_assignment.declaration_group->info->current_group = ast->apats_assignment.declaration_group;
        necro_strong_connect2(d_analyzer->arena, ast->apats_assignment.declaration_group);
        break;
    }

    case NECRO_AST_PAT_ASSIGNMENT:
        if (ast->pat_assignment.declaration_group->index != -1) return;
        assert(ast->pat_assignment.declaration_group != NULL);
        assert(ast->pat_assignment.declaration_group->next == NULL);
        necro_strong_connect1(ast->pat_assignment.declaration_group);
        d_analyze_go(d_analyzer, ast->pat_assignment.rhs);
        necro_strong_connect2(d_analyzer->arena, ast->pat_assignment.declaration_group);
        break;

    case NECRO_AST_DATA_DECLARATION:
        if (ast->data_declaration.declaration_group->index != -1) return;
        assert(ast->data_declaration.declaration_group != NULL);
        assert(ast->data_declaration.declaration_group->next == NULL);
        necro_strong_connect1(ast->data_declaration.declaration_group);
        d_analyze_go(d_analyzer, ast->data_declaration.simpletype);
        d_analyze_go(d_analyzer, ast->data_declaration.constructor_list);
        necro_strong_connect2(d_analyzer->arena, ast->data_declaration.declaration_group);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        // TODO: Circular reference detection
        if (ast->type_class_declaration.declaration_group->index != -1) return;
        assert(ast->type_class_declaration.declaration_group != NULL);
        assert(ast->type_class_declaration.declaration_group->next == NULL);
        necro_strong_connect1(ast->type_class_declaration.declaration_group);
        d_analyze_go(d_analyzer, ast->type_class_declaration.context);
        // d_analyze_go(d_analyzer, ast->type_class_declaration.tycls);
        // d_analyze_go(d_analyzer, ast->type_class_declaration.tyvar);
        d_analyze_go(d_analyzer, ast->type_class_declaration.declarations);
        necro_strong_connect2(d_analyzer->arena, ast->type_class_declaration.declaration_group);
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        // TODO: Circular reference detection
        if (ast->type_class_instance.declaration_group->index != -1) return;
        assert(ast->type_class_instance.declaration_group != NULL);
        assert(ast->type_class_instance.declaration_group->next == NULL);
        necro_strong_connect1(ast->type_class_instance.declaration_group);
        d_analyze_go(d_analyzer, ast->type_class_instance.context);
        // d_analyze_go(d_analyzer, ast->type_class_instance.qtycls);
        // d_analyze_go(d_analyzer, ast->type_class_instance.inst);
        d_analyze_go(d_analyzer, ast->type_class_instance.declarations);
        necro_strong_connect2(d_analyzer->arena, ast->type_class_instance.declaration_group);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        if (ast->type_signature.declaration_group->index != -1) return;
        assert(ast->type_signature.declaration_group != NULL);
        assert(ast->type_signature.declaration_group->next == NULL);
        necro_strong_connect1(ast->type_signature.declaration_group);
        // d_analyze_go(d_analyzer, ast->type_signature.var);
        d_analyze_go(d_analyzer, ast->type_signature.context);
        d_analyze_go(d_analyzer, ast->type_signature.type);
        necro_strong_connect2(d_analyzer->arena, ast->type_signature.declaration_group);
        break;

    //=====================================================
    // Variable type things
    //=====================================================
    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
            d_analyze_var(d_analyzer, ast->variable.id);
            break;
        case NECRO_VAR_SIG:                  break;
        case NECRO_VAR_DECLARATION:          break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        break;

    case NECRO_AST_CONID:
        if (ast->conid.con_type == NECRO_CON_TYPE_VAR)
            d_analyze_var(d_analyzer, ast->conid.id);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        d_analyze_go(d_analyzer, ast->type_class_context.conid);
        // d_analyze_go(d_analyzer, ast->type_class_context.varid);
        break;

    //=====================================================
    // Other Stuff
    //=====================================================
    case NECRO_AST_UNDEFINED:
        break;
    case NECRO_AST_CONSTANT:
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        d_analyze_go(d_analyzer, ast->bin_op.lhs);
        d_analyze_go(d_analyzer, ast->bin_op.rhs);
        break;
    case NECRO_AST_IF_THEN_ELSE:
        d_analyze_go(d_analyzer, ast->if_then_else.if_expr);
        d_analyze_go(d_analyzer, ast->if_then_else.then_expr);
        d_analyze_go(d_analyzer, ast->if_then_else.else_expr);
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        d_analyze_go(d_analyzer, ast->op_left_section.left);
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        d_analyze_go(d_analyzer, ast->op_right_section.right);
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        d_analyze_go(d_analyzer, ast->right_hand_side.declarations);
        d_analyze_go(d_analyzer, ast->right_hand_side.expression);
        break;
    case NECRO_AST_LET_EXPRESSION:
        d_analyze_go(d_analyzer, ast->let_expression.declarations);
        d_analyze_go(d_analyzer, ast->let_expression.expression);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        d_analyze_go(d_analyzer, ast->fexpression.aexp);
        d_analyze_go(d_analyzer, ast->fexpression.next_fexpression);
        break;
    case NECRO_AST_APATS:
        d_analyze_go(d_analyzer, ast->apats.apat);
        d_analyze_go(d_analyzer, ast->apats.next_apat);
        break;
    case NECRO_AST_WILDCARD:
        break;
    case NECRO_AST_LAMBDA:
        d_analyze_go(d_analyzer, ast->lambda.apats);
        d_analyze_go(d_analyzer, ast->lambda.expression);
        break;
    case NECRO_AST_DO:
        d_analyze_go(d_analyzer, ast->do_statement.statement_list);
        break;
    case NECRO_AST_LIST_NODE:
        d_analyze_go(d_analyzer, ast->list.item);
        d_analyze_go(d_analyzer, ast->list.next_item);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        d_analyze_go(d_analyzer, ast->expression_list.expressions);
        break;
    case NECRO_AST_EXPRESSION_SEQUENCE:
        d_analyze_go(d_analyzer, ast->expression_sequence.expressions);
        break;
    case NECRO_AST_TUPLE:
        d_analyze_go(d_analyzer, ast->tuple.expressions);
        break;
    case NECRO_BIND_ASSIGNMENT:
        d_analyze_go(d_analyzer, ast->bind_assignment.expression);
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        d_analyze_go(d_analyzer, ast->pat_bind_assignment.pat);
        d_analyze_go(d_analyzer, ast->pat_bind_assignment.expression);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        d_analyze_go(d_analyzer, ast->arithmetic_sequence.from);
        d_analyze_go(d_analyzer, ast->arithmetic_sequence.then);
        d_analyze_go(d_analyzer, ast->arithmetic_sequence.to);
        break;
    case NECRO_AST_CASE:
        d_analyze_go(d_analyzer, ast->case_expression.expression);
        d_analyze_go(d_analyzer, ast->case_expression.alternatives);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        d_analyze_go(d_analyzer, ast->case_alternative.pat);
        d_analyze_go(d_analyzer, ast->case_alternative.body);
        break;
    case NECRO_AST_TYPE_APP:
        d_analyze_go(d_analyzer, ast->type_app.ty);
        d_analyze_go(d_analyzer, ast->type_app.next_ty);
        break;
    case NECRO_AST_BIN_OP_SYM:
        d_analyze_go(d_analyzer, ast->bin_op_sym.left);
        d_analyze_go(d_analyzer, ast->bin_op_sym.op);
        d_analyze_go(d_analyzer, ast->bin_op_sym.right);
        break;
    case NECRO_AST_CONSTRUCTOR:
        d_analyze_go(d_analyzer, ast->constructor.conid);
        d_analyze_go(d_analyzer, ast->constructor.arg_list);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        d_analyze_go(d_analyzer, ast->simple_type.type_con);
        d_analyze_go(d_analyzer, ast->simple_type.type_var_list);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        d_analyze_go(d_analyzer, ast->function_type.type);
        d_analyze_go(d_analyzer, ast->function_type.next_on_arrow);
        break;

    default:
        necro_error(&d_analyzer->error, ast->source_loc, "Unrecognized AST Node type found in dependency analysis: %d", ast->type);
        break;
    }
}

NecroDependencyAnalyzer necro_create_dependency_analyzer(NecroSymTable* symtable, NecroIntern* intern)
{
    return (NecroDependencyAnalyzer)
    {
        .symtable = symtable,
        .intern   = intern,
    };
}

void necro_destroy_dependency_analyzer(NecroDependencyAnalyzer* d_analyzer)
{
}

NECRO_RETURN_CODE necro_dependency_analyze_ast(NecroDependencyAnalyzer* d_analyzer, NecroPagedArena* ast_arena, NecroASTNode* ast)
{
    d_analyzer->arena             = ast_arena;
    d_analyzer->error.return_code = NECRO_SUCCESS;
    d_analyze_go(d_analyzer, ast);
    d_analyzer->arena             = NULL;
    return d_analyzer->error.return_code;
}
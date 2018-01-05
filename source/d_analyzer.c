/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "d_analyzer.h"

// bool necro_is_repeat_declaration(NecroASTNode* ast, NecroID* prev_id)
// {
//     if (ast->type == NECRO_AST_SIMPLE_ASSIGNMENT)
//     {
//         if (ast->simple_assignment.id.id == prev_id->id)
//         {
//             return true;
//         }
//         else
//         {
//             prev_id->id = ast->simple_assignment.id.id;
//             return false;
//         }
//     }
//     else if (ast->type == NECRO_AST_APATS_ASSIGNMENT)
//     {
//         if (ast->apats_assignment.id.id == prev_id->id)
//         {
//             return true;
//         }
//         else
//         {
//             prev_id->id = ast->apats_assignment.id.id;
//             return false;
//         }
//     }
//     else
//     {
//         prev_id->id = 0;
//         return false;
//     }
// }

// typedef enum
// {
//     NECRO_DEPENDENCY_NONE,
//     NECRO_DEPENDENCY_LEFT_ON_RIGHT,
//     NECRO_DEPENDENCY_RIGHT_ON_LEFT,
//     NECRO_DEPENDENCY_MUTUAL
// } NECRO_DEPENDENCY_RESULT;

// // intermediate bindings in dependency_list.....
// // Can't do it here, lists won't be complete!
// bool necro_does_group_depend_on_other(NecroDeclarationGroup* group, NecroDeclarationGroup* other)
// {
//     assert(group != NULL);
//     assert(other != NULL);
//     NecroID prev_id = (NecroID) { .id = 0 };
//     while (group != NULL)
//     {
//         NecroDependencyList* list = group->dependency_list;
//         while (list != NULL)
//         {
//             while (other != NULL)
//             {
//                 if (necro_is_repeat_declaration(other->declaration_ast, &prev_id)) continue;
//                 // take into account type signature?
//                 if (list->dependency_group == other) return true;
//                 other = other->next;
//             }
//             // This would simply go infinite for cyclical references yes? how to handle....
//             if (necro_does_group_depend_on_other(list->dependency_group, other)) return true;
//             list = list->next;
//         }
//         group = group->next;
//     }
//     return false;
// }

// NECRO_DEPENDENCY_RESULT necro_determine_dependency(NecroDeclarationGroup* group1, NecroDeclarationGroup* group2)
// {
//     bool left_depends_on_right = necro_does_group_depend_on_other(group1, group2);
//     bool right_depends_on_left = necro_does_group_depend_on_other(group2, group1);
//     if (left_depends_on_right && right_depends_on_left)       return NECRO_DEPENDENCY_MUTUAL;
//     else if (left_depends_on_right && !right_depends_on_left) return NECRO_DEPENDENCY_LEFT_ON_RIGHT;
//     else if (!left_depends_on_right && right_depends_on_left) return NECRO_DEPENDENCY_RIGHT_ON_LEFT;
//     else                                                      return NECRO_DEPENDENCY_NONE;
// }

// void necro_insert_declaration_group_into_group_list(NecroPagedArena* arena, NecroDeclarationGroup* group, NecroDeclarationGroupList* group_list)
// {
//     NecroDeclarationGroupList* curr_group_list = group_list;
//     NecroDeclarationGroupList* prev_group_list = group_list;
//     while (curr_group_list != NULL)
//     {
//         switch (necro_determine_dependency(group, curr_group_list->declaration_group))
//         {
//         case NECRO_DEPENDENCY_NONE:          break;
//         case NECRO_DEPENDENCY_LEFT_ON_RIGHT: return;
//         case NECRO_DEPENDENCY_RIGHT_ON_LEFT: return;
//         case NECRO_DEPENDENCY_MUTUAL:        return;
//         }
//         prev_group_list = curr_group_list;
//         curr_group_list = curr_group_list->next;
//     }
//     // Handle no groups depend on this group
// }

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
        group->info->group_lists                                = necro_append_declaration_group_list(arena, NULL, group->info->group_lists);
        NecroDeclarationGroupList* strongly_connected_component = group->info->group_lists;
        NecroDeclarationGroup*     w                            = NULL;
        do
        {
            w           = necro_pop_declaration_group_vector(&group->info->stack);
            w->on_stack = false;
            necro_append_declaration_group_to_group_in_group_list(arena, strongly_connected_component, w);
        } while (w != group);
        assert(strongly_connected_component->declaration_group != NULL);
        assert(strongly_connected_component->declaration_group == group);
    }
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
        NecroASTNode*              current_decl = ast;
        NecroDeclarationsInfo*     info         = necro_create_declarations_info(d_analyzer->arena);
        //-----------------------------------------
        // Pass 1, assign groups and info
        while (current_decl != NULL)
        {
            switch (ast->top_declaration.declaration->type)
            {
            case NECRO_AST_SIMPLE_ASSIGNMENT:
                ast->top_declaration.declaration->simple_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, ast->top_declaration.declaration->simple_assignment.id)->declaration_group;
                ast->top_declaration.declaration->simple_assignment.declaration_group->info = info;
                break;
            case NECRO_AST_APATS_ASSIGNMENT:
                ast->top_declaration.declaration->apats_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, ast->top_declaration.declaration->apats_assignment.id)->declaration_group;
                ast->top_declaration.declaration->apats_assignment.declaration_group->info = info;
                break;
            case NECRO_AST_PAT_ASSIGNMENT:
                ast->top_declaration.declaration->pat_assignment.declaration_group->info = info;
                break;
            default: break;
            }
            current_decl = current_decl->top_declaration.next_top_decl;
        }
        //-----------------------------------------
        // Pass 2, analyze
        current_decl = ast;
        while (current_decl != NULL)
        {
            d_analyze_go(d_analyzer, ast->top_declaration.declaration);
            current_decl = current_decl->top_declaration.next_top_decl;
        }
        necro_destroy_declaration_group_vector(&info->stack);
        break;
    }

    case NECRO_AST_DECL:
    {
        NecroASTNode*              current_decl = ast;
        NecroDeclarationsInfo*     info         = necro_create_declarations_info(d_analyzer->arena);
        //-----------------------------------------
        // Pass 1, assign groups and info
        while (current_decl != NULL)
        {
            switch (ast->top_declaration.declaration->type)
            {
            case NECRO_AST_SIMPLE_ASSIGNMENT:
                ast->declaration.declaration_impl->simple_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, ast->declaration.declaration_impl->simple_assignment.id)->declaration_group;
                ast->declaration.declaration_impl->simple_assignment.declaration_group->info = info;
                break;
            case NECRO_AST_APATS_ASSIGNMENT:
                ast->declaration.declaration_impl->apats_assignment.declaration_group = necro_symtable_get(d_analyzer->symtable, ast->declaration.declaration_impl->apats_assignment.id)->declaration_group;
                ast->declaration.declaration_impl->apats_assignment.declaration_group->info = info;
                break;
            case NECRO_AST_PAT_ASSIGNMENT:
                ast->declaration.declaration_impl->pat_assignment.declaration_group->info = info;
                break;
            default: break;
            }
            current_decl = current_decl->declaration.next_declaration;
        }
        //-----------------------------------------
        // Pass 2, analyze
        current_decl = ast;
        while (current_decl != NULL)
        {
            d_analyze_go(d_analyzer, ast->declaration.declaration_impl);
            current_decl = current_decl->declaration.next_declaration;
        }
        necro_destroy_declaration_group_vector(&info->stack);
        break;
    }

    case NECRO_AST_TYPE_CLASS_INSTANCE:
    {
        d_analyze_go(d_analyzer, ast->type_class_instance.context);
        d_analyze_go(d_analyzer, ast->type_class_instance.qtycls);
        d_analyze_go(d_analyzer, ast->type_class_instance.inst);
        d_analyze_go(d_analyzer, ast->type_class_instance.declarations);
        break;
    }

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        if (ast->simple_assignment.declaration_group->index != -1) return;
        assert(ast->simple_assignment.declaration_group != NULL);
        NecroDeclarationGroup* initial_group = ast->simple_assignment.declaration_group;
        NecroDeclarationGroup* current_group = initial_group;
        while (current_group != NULL)
        {
            initial_group->low_link = min(initial_group->low_link, current_group->low_link);
            necro_strong_connect1(current_group);
            if (current_group->declaration_ast->type != NECRO_AST_SIMPLE_ASSIGNMENT)
            {
                necro_error(&d_analyzer->error, ast->source_loc, "Multiple declarations of: %s", necro_intern_get_string(d_analyzer->intern, ast->simple_assignment.variable_name));
            }
            d_analyze_go(d_analyzer, current_group->declaration_ast->simple_assignment.rhs);
            current_group = current_group->next;
        }
        current_group = initial_group;
        while (current_group != NULL)
        {
            current_group->low_link = initial_group->low_link;
            current_group           = current_group->next;
        }
        necro_strong_connect2(d_analyzer->arena, ast->simple_assignment.declaration_group);
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        if (ast->apats_assignment.declaration_group->index != -1) return;
        assert(ast->apats_assignment.declaration_group != NULL);
        NecroDeclarationGroup* initial_group = ast->apats_assignment.declaration_group;
        NecroDeclarationGroup* current_group = initial_group;
        while (current_group != NULL)
        {
            initial_group->low_link = min(initial_group->low_link, current_group->low_link);
            necro_strong_connect1(current_group);
            d_analyze_go(d_analyzer, current_group->declaration_ast->apats_assignment.apats);
            d_analyze_go(d_analyzer, current_group->declaration_ast->apats_assignment.rhs);
            current_group = current_group->next;
        }
        current_group = initial_group;
        while (current_group != NULL)
        {
            current_group->low_link = initial_group->low_link;
            current_group           = current_group->next;
        }
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

    //=====================================================
    // Variable
    //=====================================================
    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
        {
            NecroSymbolInfo* symbol_info = necro_symtable_get(d_analyzer->symtable, ast->variable.id);
            if (symbol_info->declaration_group == NULL) return;
            // necro_create_dependency_list(d_analyzer->arena, symbol_info->declaration_group, symbol_info->declaration_group->info->current_group->dependency_list);
            NecroDeclarationGroup* w = necro_symtable_get(d_analyzer->symtable, ast->variable.id)->declaration_group;
            NecroDeclarationGroup* v = w->info->current_group;
            if (w->index == -1)
            {
                symbol_info->declaration_group->info->current_group = w;
                // Fuuuuuuuuuuuuck......Need to visit each node in a declaration_group, not just the head one
                // This is important for multi-line declarations!!!!!!
                d_analyze_go(d_analyzer, w->declaration_ast);
                v->low_link = min(w->low_link, v->low_link);
            }
            else if (w->on_stack)
            {
                v->low_link = min(w->low_link, v->low_link);
            }
            symbol_info->declaration_group->info->current_group = v;
            break;
        }
        case NECRO_VAR_DECLARATION:          break;
        case NECRO_VAR_SIG:                  break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
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
    case NECRO_AST_CONID:
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
    case NECRO_AST_DATA_DECLARATION:
        d_analyze_go(d_analyzer, ast->data_declaration.simpletype);
        d_analyze_go(d_analyzer, ast->data_declaration.constructor_list);
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        d_analyze_go(d_analyzer, ast->type_class_declaration.context);
        d_analyze_go(d_analyzer, ast->type_class_declaration.tycls);
        d_analyze_go(d_analyzer, ast->type_class_declaration.tyvar);
        d_analyze_go(d_analyzer, ast->type_class_declaration.declarations);
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        d_analyze_go(d_analyzer, ast->type_signature.var);
        d_analyze_go(d_analyzer, ast->type_signature.context);
        d_analyze_go(d_analyzer, ast->type_signature.type);
        break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:
        d_analyze_go(d_analyzer, ast->type_class_context.conid);
        d_analyze_go(d_analyzer, ast->type_class_context.varid);
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
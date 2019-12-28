/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "type_class.h"
#include "d_analyzer.h"
#include "utility/math_utility.h"
#include "base.h"

NECRO_DECLARE_VECTOR(NecroAst*, NecroDeclarationGroup, declaration_group)

typedef struct NecroDeclarationsInfo
{
    int32_t                     index;
    NecroDeclarationGroupVector stack;
    NecroAst*                   group_lists;
    NecroAst*                   current_group;
} NecroDeclarationsInfo;

typedef struct
{
    NecroAst*            current_declaration_group;
    NecroPagedArena*     arena;
    NecroIntern*         intern;
    NecroBase*           base;
    NecroScopedSymTable* scoped_symtable;
} NecroDependencyAnalyzer;

NecroDeclarationsInfo* necro_declaration_info_create(NecroPagedArena* arena)
{
    NecroDeclarationsInfo* info = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationsInfo));
    info->group_lists           = necro_ast_create_declaration_group_list(arena, NULL, NULL);
    info->current_group         = NULL;
    info->stack                 = necro_create_declaration_group_vector();
    info->index                 = 0;
    return info;
}

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
void d_analyze_go(NecroDependencyAnalyzer* d_analyzer, NecroAst* ast);

///////////////////////////////////////////////////////
// Dependency Analysis
///////////////////////////////////////////////////////
void necro_strong_connect1(NecroAst* group)
{
    assert(group->type == NECRO_AST_DECL);
    group->declaration.index    = group->declaration.info->index;
    group->declaration.low_link = group->declaration.info->index;
    group->declaration.info->index++;
    necro_push_declaration_group_vector(&group->declaration.info->stack, &group);
    group->declaration.on_stack            = true;
    group->declaration.info->current_group = group;
}

void necro_strong_connect2(NecroPagedArena* arena, NecroAst* group)
{
    assert(group->type == NECRO_AST_DECL);
    // If we're a root node
    if (group->declaration.low_link == group->declaration.index)
    {
        if (group->declaration.info->group_lists->declaration_group_list.declaration_group != NULL)
            necro_ast_declaration_group_list_append(arena, NULL, group->declaration.info->group_lists);
        NecroAst* strongly_connected_component = group->declaration.info->group_lists;
        assert(strongly_connected_component->type == NECRO_AST_DECLARATION_GROUP_LIST);
        while (strongly_connected_component->declaration_group_list.next != NULL) strongly_connected_component = strongly_connected_component->declaration_group_list.next;
        NecroAst* w = NULL;
        do
        {
            w = necro_pop_declaration_group_vector(&group->declaration.info->stack);
            assert(w != NULL);
            w->declaration.on_stack = false;
            necro_ast_declaration_group_prepend_to_group_in_group_list(strongly_connected_component, w);
        } while (w != group);
        assert(strongly_connected_component->declaration_group_list.declaration_group != NULL);
        assert(strongly_connected_component->declaration_group_list.declaration_group == group);
    }
}

void d_analyze_var(NecroDependencyAnalyzer* d_analyzer, NecroAstSymbol* ast_symbol)
{
    assert(ast_symbol != NULL);
    if (ast_symbol->declaration_group == NULL) return;
    NecroAst* w = ast_symbol->declaration_group;
    assert(w->type == NECRO_AST_DECL);
    assert(w->declaration.info != NULL);
    if (w->declaration.info->current_group == NULL)
        w->declaration.info->current_group = w;
    NecroAst* v = w->declaration.info->current_group;
    assert(v != NULL);
    assert(v->type == NECRO_AST_DECL);
    if (w->declaration.index == -1)
    {
        ast_symbol->declaration_group->declaration.info->current_group = w;
        d_analyze_go(d_analyzer, w->declaration.declaration_impl);
        v->declaration.low_link = MIN(w->declaration.low_link, v->declaration.low_link);
    }
    else if (w->declaration.on_stack)
    {
        v->declaration.low_link = MIN(w->declaration.low_link, v->declaration.low_link);
        if (w->declaration.info->current_group->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT)
        {
            w->declaration.info->current_group->declaration.declaration_impl->simple_assignment.is_recursive = true;
        }
        else if (w->declaration.info->current_group->declaration.declaration_impl->type == NECRO_AST_DATA_DECLARATION)
        {
            w->declaration.info->current_group->declaration.declaration_impl->data_declaration.is_recursive = true;
        }
        else if (w->declaration.info->current_group->declaration.declaration_impl->type == NECRO_AST_APATS_ASSIGNMENT)
        {
            w->declaration.info->current_group->declaration.declaration_impl->apats_assignment.is_recursive = true;
        }
    }
    ast_symbol->declaration_group->declaration.info->current_group = v;
}

void d_analyze_go(NecroDependencyAnalyzer* d_analyzer, NecroAst* ast)
{
    if (ast == NULL)
        return;
    switch (ast->type)
    {

    //=====================================================
    // Declaration type things
    //=====================================================
    case NECRO_AST_TOP_DECL:
    {
        NecroAst*              curr = ast;
        NecroDeclarationsInfo* info = necro_declaration_info_create(d_analyzer->arena);
        NecroAst*              temp = NULL; // DeclarationGroup
        //-----------------------------------------
        // Pass 1, assign groups and info
        while (curr != NULL)
        {
            switch (curr->top_declaration.declaration->type)
            {
            case NECRO_AST_SIMPLE_ASSIGNMENT:
                curr->top_declaration.declaration->simple_assignment.declaration_group = curr->top_declaration.declaration->simple_assignment.ast_symbol->declaration_group;
                temp = curr->top_declaration.declaration->simple_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->declaration.info = info;
                    temp = temp->declaration.next_declaration;
                }
                break;
            case NECRO_AST_APATS_ASSIGNMENT:
                curr->top_declaration.declaration->apats_assignment.declaration_group = curr->top_declaration.declaration->apats_assignment.ast_symbol->declaration_group;
                temp = curr->top_declaration.declaration->apats_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->declaration.info = info;
                    temp = temp->declaration.next_declaration;
                }
                break;
            case NECRO_AST_PAT_ASSIGNMENT:
                curr->top_declaration.declaration->pat_assignment.declaration_group->declaration.info = info;
                break;
            case NECRO_AST_DATA_DECLARATION:
                curr->top_declaration.declaration->data_declaration.declaration_group->declaration.info = info;
                break;
            case NECRO_AST_TYPE_CLASS_DECLARATION:
                curr->top_declaration.declaration->type_class_declaration.declaration_group->declaration.info = info;
                break;
            case NECRO_AST_TYPE_CLASS_INSTANCE:
                curr->top_declaration.declaration->type_class_instance.declaration_group->declaration.info = info;
                break;
            case NECRO_AST_TYPE_SIGNATURE:
                curr->top_declaration.declaration->type_signature.declaration_group->declaration.info = info;
                break;
            default:
                assert(false);
                break;
            }
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 2, analyze Data Declarations
        curr = ast;
        while (curr != NULL)
        {
            NECRO_AST_TYPE type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_DATA_DECLARATION)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 3, TypeClass Declarations, TypeClass Instances
        curr = ast;
        while (curr != NULL)
        {
            NECRO_AST_TYPE type = curr->top_declaration.declaration->type;
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
            NECRO_AST_TYPE type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_TYPE_SIGNATURE)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        //-----------------------------------------
        // Pass 5, analyze Terms
        curr = ast;
        while (curr != NULL)
        {
            NECRO_AST_TYPE type = curr->top_declaration.declaration->type;
            if (type == NECRO_AST_SIMPLE_ASSIGNMENT ||
                type == NECRO_AST_APATS_ASSIGNMENT  ||
                type == NECRO_AST_PAT_ASSIGNMENT)
                d_analyze_go(d_analyzer, curr->top_declaration.declaration);
            curr = curr->top_declaration.next_top_decl;
        }

        // Overwrite ast with group lists
        *ast = *info->group_lists;

        //-----------------------------------------
        // Pass 6, Set back pointers from DeclarationGroup to this DeclarationGroupList
        curr = ast;
        while (curr != NULL)
        {
            assert(curr->type == NECRO_AST_DECLARATION_GROUP_LIST);
            NecroAst* group = curr->declaration_group_list.declaration_group;
            while (group != NULL)
            {
                assert(group->type == NECRO_AST_DECL);
                group->declaration.declaration_group_list = curr;
                group                                     = group->declaration.next_declaration;
            }
            curr = curr->declaration_group_list.next;
        }

        necro_destroy_declaration_group_vector(&info->stack);
        break;
    }

    case NECRO_AST_DECL:
    {
        NecroAst*              curr = ast;
        NecroDeclarationsInfo* info = necro_declaration_info_create(d_analyzer->arena);
        NecroAst*              temp = NULL; // DeclarationGroup
        //-----------------------------------------
        // Pass 1, assign groups and info
        while (curr != NULL)
        {
            switch (curr->declaration.declaration_impl->type)
            {
            case NECRO_AST_SIMPLE_ASSIGNMENT:
                curr->declaration.declaration_impl->simple_assignment.declaration_group = curr->declaration.declaration_impl->simple_assignment.ast_symbol->declaration_group;
                temp = curr->declaration.declaration_impl->simple_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->declaration.info = info;
                    temp                   = temp->declaration.next_declaration;
                }
                break;
            case NECRO_AST_APATS_ASSIGNMENT:
                curr->declaration.declaration_impl->apats_assignment.declaration_group = curr->declaration.declaration_impl->apats_assignment.ast_symbol->declaration_group;
                temp = curr->declaration.declaration_impl->apats_assignment.declaration_group;
                while (temp != NULL)
                {
                    temp->declaration.info = info;
                    temp                   = temp->declaration.next_declaration;
                }
                break;
            case NECRO_AST_PAT_ASSIGNMENT:
                curr->declaration.declaration_impl->pat_assignment.declaration_group->declaration.info = info;
                break;
            case NECRO_AST_TYPE_SIGNATURE:
                curr->top_declaration.declaration->type_signature.declaration_group->declaration.info = info;
                break;
            default: assert(false); break;
            }
            curr = curr->declaration.next_declaration;
        }
        //-----------------------------------------
        // Pass 2, analyze TypeSignatures
        curr = ast;
        while (curr != NULL)
        {
            NECRO_AST_TYPE type = curr->declaration.declaration_impl->type;
            if (type == NECRO_AST_TYPE_SIGNATURE)
                d_analyze_go(d_analyzer, curr->declaration.declaration_impl);
            curr = curr->declaration.next_declaration;
        }
        //-----------------------------------------
        // Pass 3, analyze everything else
        curr = ast;
        while (curr != NULL)
        {
            NECRO_AST_TYPE type = curr->declaration.declaration_impl->type;
            if (type == NECRO_AST_SIMPLE_ASSIGNMENT ||
                type == NECRO_AST_APATS_ASSIGNMENT  ||
                type == NECRO_AST_PAT_ASSIGNMENT)
                d_analyze_go(d_analyzer, curr->declaration.declaration_impl);
            curr = curr->declaration.next_declaration;
        }
        // Overwrite ast with group lists
        *ast = *info->group_lists;
        //-----------------------------------------
        // Pass 4, Set back pointers from DeclarationGroup to this DeclarationGroupList
        curr = ast;
        while (curr != NULL)
        {
            assert(curr->type == NECRO_AST_DECLARATION_GROUP_LIST);
            NecroAst* group = curr->declaration_group_list.declaration_group;
            while (group != NULL)
            {
                assert(group->type == NECRO_AST_DECL);
                group->declaration.declaration_group_list = curr;
                group                                     = group->declaration.next_declaration;
            }
            curr = curr->declaration_group_list.next;
        }
        necro_destroy_declaration_group_vector(&info->stack);
        break;
    }

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        if (ast->simple_assignment.declaration_group->declaration.index != -1) return;
        assert(ast->simple_assignment.declaration_group != NULL);
        if (ast->simple_assignment.ast_symbol->optional_type_signature != NULL)
            d_analyze_go(d_analyzer, ast->simple_assignment.ast_symbol->optional_type_signature);
        NecroAst* initial_group = ast->simple_assignment.declaration_group;
        NecroAst* current_group = initial_group;
        while (current_group != NULL)
        {
            // current_group->info = initial_group->info;
            necro_strong_connect1(current_group);
            assert(current_group->declaration.declaration_impl->type == NECRO_AST_SIMPLE_ASSIGNMENT);
            d_analyze_go(d_analyzer, current_group->declaration.declaration_impl->simple_assignment.initializer);
            d_analyze_go(d_analyzer, current_group->declaration.declaration_impl->simple_assignment.rhs);
            initial_group->declaration.low_link = MIN(initial_group->declaration.low_link, current_group->declaration.low_link);
            current_group = current_group->declaration.next_declaration;
        }
        current_group = initial_group;
        NecroAst* prev_group = NULL;
        while (current_group != NULL)
        {
            current_group->declaration.low_link      = initial_group->declaration.low_link;
            prev_group                               = current_group;
            current_group                            = current_group->declaration.next_declaration;
            prev_group->declaration.next_declaration = NULL;
        }
        ast->simple_assignment.declaration_group->declaration.info->current_group = ast->simple_assignment.declaration_group;
        necro_strong_connect2(d_analyzer->arena, ast->simple_assignment.declaration_group);
        // necro_symtable_get(d_analyzer->symtable, ast->simple_assignment.id)->ast = ast;
        break;
    }

    case NECRO_AST_APATS_ASSIGNMENT:
    {
        if (ast->apats_assignment.declaration_group->declaration.index != -1) return;
        assert(ast->apats_assignment.declaration_group != NULL);
        if (ast->apats_assignment.ast_symbol->optional_type_signature != NULL)
            d_analyze_go(d_analyzer, ast->apats_assignment.ast_symbol->optional_type_signature);
        NecroAst* initial_group = ast->apats_assignment.declaration_group;
        NecroAst* current_group = initial_group;
        while (current_group != NULL)
        {
            // current_group->info = initial_group->info;
            necro_strong_connect1(current_group);
            assert(current_group->declaration.declaration_impl->type == NECRO_AST_APATS_ASSIGNMENT);
            d_analyze_go(d_analyzer, current_group->declaration.declaration_impl->apats_assignment.apats);
            d_analyze_go(d_analyzer, current_group->declaration.declaration_impl->apats_assignment.rhs);
            initial_group->declaration.low_link = MIN(initial_group->declaration.low_link, current_group->declaration.low_link);
            current_group = current_group->declaration.next_declaration;
        }
        current_group = initial_group;
        NecroAst* prev_group = NULL;
        while (current_group != NULL)
        {
            current_group->declaration.low_link      = initial_group->declaration.low_link;
            prev_group                               = current_group;
            current_group                            = current_group->declaration.next_declaration;
            prev_group->declaration.next_declaration = NULL;
        }
        ast->apats_assignment.declaration_group->declaration.info->current_group = ast->apats_assignment.declaration_group;
        necro_strong_connect2(d_analyzer->arena, ast->apats_assignment.declaration_group);
        break;
    }

    case NECRO_AST_PAT_ASSIGNMENT:
        if (ast->pat_assignment.declaration_group->declaration.index != -1) return;
        assert(ast->pat_assignment.declaration_group != NULL);
        assert(ast->pat_assignment.declaration_group->declaration.next_declaration == NULL);
        necro_strong_connect1(ast->pat_assignment.declaration_group);
        d_analyze_go(d_analyzer, ast->pat_assignment.rhs);
        necro_strong_connect2(d_analyzer->arena, ast->pat_assignment.declaration_group);
        break;

    case NECRO_AST_DATA_DECLARATION:
        if (ast->data_declaration.declaration_group->declaration.index != -1) return;
        assert(ast->data_declaration.declaration_group != NULL);
        assert(ast->data_declaration.declaration_group->declaration.next_declaration == NULL);
        necro_strong_connect1(ast->data_declaration.declaration_group);
        d_analyze_go(d_analyzer, ast->data_declaration.simpletype);
        d_analyze_go(d_analyzer, ast->data_declaration.constructor_list);
        necro_strong_connect2(d_analyzer->arena, ast->data_declaration.declaration_group);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        if (ast->type_class_declaration.declaration_group->declaration.index != -1) return;
        assert(ast->type_class_declaration.declaration_group != NULL);
        assert(ast->type_class_declaration.declaration_group->declaration.next_declaration == NULL);
        necro_strong_connect1(ast->type_class_declaration.declaration_group);
        d_analyze_go(d_analyzer, ast->type_class_declaration.context);
        // d_analyze_go(d_analyzer, ast->type_class_declaration.tycls);
        // d_analyze_go(d_analyzer, ast->type_class_declaration.tyvar);
        d_analyze_go(d_analyzer, ast->type_class_declaration.declarations);
        necro_strong_connect2(d_analyzer->arena, ast->type_class_declaration.declaration_group);
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        // TODO: Circular reference detection
        if (ast->type_class_instance.declaration_group->declaration.index != -1) return;
        assert(ast->type_class_instance.declaration_group != NULL);
        assert(ast->type_class_instance.declaration_group->declaration.next_declaration == NULL);
        necro_strong_connect1(ast->type_class_instance.declaration_group);
        d_analyze_go(d_analyzer, ast->type_class_instance.qtycls);
        // Super class instances!
        NecroAst* super_classes = ast->type_class_instance.qtycls->conid.ast_symbol->ast->type_class_declaration.context;
        if (super_classes != NULL)
        {
            NecroSymbol data_type_name  = NULL;
            if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
                data_type_name = ast->type_class_instance.inst->conid.ast_symbol->source_name;
            else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
                data_type_name = ast->type_class_instance.inst->constructor.conid->conid.ast_symbol->source_name;
            if (super_classes->type != NECRO_AST_LIST_NODE)
            {
                NecroSymbol     type_class_name                 = super_classes->type_class_context.conid->conid.ast_symbol->source_name;
                NecroSymbol     super_class_instance_symbol     = necro_intern_create_type_class_instance_symbol(d_analyzer->intern, type_class_name, data_type_name);
                NecroAstSymbol* super_class_instance_ast_symbol = necro_scope_find_ast_symbol(d_analyzer->scoped_symtable->top_type_scope, super_class_instance_symbol);
                if (super_class_instance_ast_symbol != NULL)
                    d_analyze_var(d_analyzer, super_class_instance_ast_symbol);
            }
            else
            {
                while (super_classes != NULL)
                {
                    NecroSymbol     type_class_name                 = super_classes->list.item->type_class_context.conid->conid.ast_symbol->source_name;
                    NecroSymbol     super_class_instance_symbol     = necro_intern_create_type_class_instance_symbol(d_analyzer->intern, type_class_name, data_type_name);
                    NecroAstSymbol* super_class_instance_ast_symbol = necro_scope_find_ast_symbol(d_analyzer->scoped_symtable->top_type_scope, super_class_instance_symbol);
                    if (super_class_instance_ast_symbol != NULL)
                        d_analyze_var(d_analyzer, super_class_instance_ast_symbol);
                    super_classes = super_classes->list.next_item;
                }
            }
        }
        d_analyze_go(d_analyzer, ast->type_class_instance.context);
        d_analyze_go(d_analyzer, ast->type_class_instance.inst);
        d_analyze_go(d_analyzer, ast->type_class_instance.declarations);
        necro_strong_connect2(d_analyzer->arena, ast->type_class_instance.declaration_group);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        if (ast->type_signature.declaration_group->declaration.index != -1) return;
        assert(ast->type_signature.declaration_group != NULL);
        assert(ast->type_signature.declaration_group->declaration.next_declaration == NULL);
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
            d_analyze_var(d_analyzer, ast->variable.ast_symbol);
            break;
        case NECRO_VAR_SIG:                  break;
        case NECRO_VAR_DECLARATION:          break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        if (ast->variable.initializer != NULL)
            d_analyze_go(d_analyzer, ast->variable.initializer);
        break;

    case NECRO_AST_CONID:
        if (ast->conid.con_type == NECRO_CON_TYPE_VAR)
            d_analyze_var(d_analyzer, ast->conid.ast_symbol);
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
        d_analyze_var(d_analyzer, d_analyzer->base->eq_type_class);
        d_analyze_var(d_analyzer, d_analyzer->base->ord_type_class);
        d_analyze_var(d_analyzer, d_analyzer->base->num_type_class);
        d_analyze_var(d_analyzer, d_analyzer->base->floating_class);
        d_analyze_var(d_analyzer, d_analyzer->base->integral_class);
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        d_analyze_var(d_analyzer, ast->bin_op.ast_symbol);
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
    case NECRO_AST_EXPRESSION_ARRAY:
        d_analyze_go(d_analyzer, ast->expression_array.expressions);
        break;
    case NECRO_AST_SEQ_EXPRESSION:
        d_analyze_var(d_analyzer, d_analyzer->base->seq_tick);
        d_analyze_var(d_analyzer, d_analyzer->base->tuple_tick);
        d_analyze_var(d_analyzer, d_analyzer->base->interleave_tick);
        d_analyze_var(d_analyzer, d_analyzer->base->run_seq);
        d_analyze_go(d_analyzer, ast->sequence_expression.expressions);
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
    case NECRO_AST_FOR_LOOP:
        d_analyze_go(d_analyzer, ast->for_loop.range_init);
        d_analyze_go(d_analyzer, ast->for_loop.value_init);
        d_analyze_go(d_analyzer, ast->for_loop.index_apat);
        d_analyze_go(d_analyzer, ast->for_loop.value_apat);
        d_analyze_go(d_analyzer, ast->for_loop.expression);
        break;
    case NECRO_AST_WHILE_LOOP:
        d_analyze_go(d_analyzer, ast->while_loop.value_init);
        d_analyze_go(d_analyzer, ast->while_loop.value_apat);
        d_analyze_go(d_analyzer, ast->while_loop.while_expression);
        d_analyze_go(d_analyzer, ast->while_loop.do_expression);
        break;
    case NECRO_AST_TYPE_APP:
        d_analyze_go(d_analyzer, ast->type_app.ty);
        d_analyze_go(d_analyzer, ast->type_app.next_ty);
        break;
    case NECRO_AST_BIN_OP_SYM:
        d_analyze_go(d_analyzer, ast->bin_op_sym.op);
        d_analyze_go(d_analyzer, ast->bin_op_sym.left);
        d_analyze_go(d_analyzer, ast->bin_op_sym.right);
        break;
    case NECRO_AST_CONSTRUCTOR:
        d_analyze_go(d_analyzer, ast->constructor.conid);
        d_analyze_go(d_analyzer, ast->constructor.arg_list);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        d_analyze_go(d_analyzer, ast->simple_type.type_con);
        // TODO / HACK: Don't run d_analyze on kind signatures?....
        // d_analyze_go(d_analyzer, ast->simple_type.type_var_list);
        break;
    case NECRO_AST_FUNCTION_TYPE:
        d_analyze_go(d_analyzer, ast->function_type.type);
        d_analyze_go(d_analyzer, ast->function_type.next_on_arrow);
        break;
    case NECRO_AST_TYPE_ATTRIBUTE:
        d_analyze_go(d_analyzer, ast->attribute.attribute_type);
        break;

    default:
        assert(false);
        break;
    }
}

void necro_dependency_analyze(NecroCompileInfo info, NecroIntern* intern, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroDependencyAnalyzer d_analyzer =
    {
        .intern          = intern,
        .arena           = &ast_arena->arena,
        .base            = base,
        .scoped_symtable = base->scoped_symtable,
    };
    d_analyze_go(&d_analyzer, ast_arena->root);
    if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_DEPENDENCY_ANALYSIS && info.verbosity > 0))
        necro_ast_arena_print(ast_arena);
}

/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "d_analyzer.h"

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
        d_analyze_go(d_analyzer, ast->top_declaration.declaration);
        d_analyze_go(d_analyzer, ast->top_declaration.next_top_decl);
        break;
    case NECRO_AST_DECL:
        d_analyze_go(d_analyzer, ast->declaration.declaration_impl);
        d_analyze_go(d_analyzer, ast->declaration.next_declaration);
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        d_analyze_go(d_analyzer, ast->type_class_instance.context);
        d_analyze_go(d_analyzer, ast->type_class_instance.qtycls);
        d_analyze_go(d_analyzer, ast->type_class_instance.inst);
        d_analyze_go(d_analyzer, ast->type_class_instance.declarations);
        break;

    //=====================================================
    // Assignment type things
    //=====================================================
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        d_analyze_go(d_analyzer, ast->simple_assignment.rhs);
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        d_analyze_go(d_analyzer, ast->apats_assignment.apats);
        d_analyze_go(d_analyzer, ast->apats_assignment.rhs);
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        d_analyze_go(d_analyzer, ast->pat_assignment.rhs);
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_VAR:
            // try_find_name(renamer, ast, ast->scope, &ast->variable.id, ast->variable.symbol);
            break;
        case NECRO_VAR_DECLARATION:
            // // if we are in a pat_assignment, set our declaration_ast
            // if (renamer->current_declaration_group != NULL)
            //     renamer->scoped_symtable->global_table->data[ast->variable.id.id].declaration_group = renamer->current_declaration_group;
            break;
        case NECRO_VAR_SIG: break;
        case NECRO_VAR_TYPE_VAR_DECLARATION: break;
        case NECRO_VAR_TYPE_FREE_VAR:        break;
        case NECRO_VAR_CLASS_SIG:            break;
        default: assert(false);
        }
        break;

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

NecroDependencyAnalyzer necro_create_dependency_analyzer(NecroSymTable* symtable)
{
    return (NecroDependencyAnalyzer)
    {
        .symtable = symtable,
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
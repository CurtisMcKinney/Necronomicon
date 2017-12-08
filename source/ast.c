/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "symtable.h"
#include "ast.h"

#define AST_TAB "  "

void print_reified_ast_impl(NecroAST_Node_Reified* ast_node, NecroIntern* intern, uint32_t depth)
{
    assert(ast_node != NULL);
    assert(intern != NULL);
    for (uint32_t i = 0;  i < depth; ++i)
    {
        printf(AST_TAB);
    }

    switch(ast_node->type)
    {
    case NECRO_AST_BIN_OP:
        switch(ast_node->bin_op.type)
        {
        case NECRO_BIN_OP_ADD:
            puts("(+)");
            break;
        case NECRO_BIN_OP_SUB:
            puts("(-)");
            break;
        case NECRO_BIN_OP_MUL:
            puts("(*)");
            break;
        case NECRO_BIN_OP_DIV:
            puts("(/)");
            break;
        case NECRO_BIN_OP_MOD:
            puts("(%)");
            break;
        case NECRO_BIN_OP_GT:
            puts("(>)");
            break;
        case NECRO_BIN_OP_LT:
            puts("(<)");
            break;
        case NECRO_BIN_OP_GTE:
            puts("(>=)");
            break;
        case NECRO_BIN_OP_LTE:
            puts("(<=)");
            break;
        case NECRO_BIN_OP_COLON:
            puts("(:)");
            break;
        case NECRO_BIN_OP_DOUBLE_COLON:
            puts("(::)");
            break;
        case NECRO_BIN_OP_LEFT_SHIFT:
            puts("(<<)");
            break;
        case NECRO_BIN_OP_RIGHT_SHIFT:
            puts("(>>)");
            break;
        case NECRO_BIN_OP_PIPE:
            puts("(|)");
            break;
        case NECRO_BIN_OP_FORWARD_PIPE:
            puts("(|>)");
            break;
        case NECRO_BIN_OP_BACK_PIPE:
            puts("(<|)");
            break;
        case NECRO_BIN_OP_EQUALS:
            puts("(=)");
            break;
        case NECRO_BIN_OP_NOT_EQUALS:
            puts("(/=)");
            break;
        case NECRO_BIN_OP_AND:
            puts("(&&)");
            break;
        case NECRO_BIN_OP_OR:
            puts("(||)");
            break;
        case NECRO_BIN_OP_DOT:
            puts("(.)");
            break;
        case NECRO_BIN_OP_DOLLAR:
            puts("($)");
            break;
        case NECRO_BIN_OP_BIND_RIGHT:
            puts("(>>=)");
            break;
        case NECRO_BIN_OP_BIND_LEFT:
            puts("(=<<)");
            break;
        case NECRO_BIN_OP_DOUBLE_EXCLAMATION:
            puts("(!!)");
            break;
        case NECRO_BIN_OP_APPEND:
            puts("(++)");
            break;
        default:
            puts("(Undefined Binary Operator)");
            break;
        }
        print_reified_ast_impl(ast_node->bin_op.lhs, intern, depth + 1);
        print_reified_ast_impl(ast_node->bin_op.rhs, intern, depth + 1);
        break;

    case NECRO_AST_CONSTANT:
        switch(ast_node->constant.type)
        {
        case NECRO_AST_CONSTANT_FLOAT:
            printf("(%f)\n", ast_node->constant.double_literal);
            break;
        case NECRO_AST_CONSTANT_INTEGER:
#if WIN32
            printf("(%lli)\n", ast_node->constant.int_literal);
#else
            printf("(%li)\n", ast_node->constant.int_literal);
#endif
            break;
        case NECRO_AST_CONSTANT_STRING:
            {
                const char* string = necro_intern_get_string(intern, ast_node->constant.symbol);
                if (string)
                    printf("(\"%s\")\n", string);
            }
            break;
        case NECRO_AST_CONSTANT_CHAR:
            printf("(\'%c\')\n", ast_node->constant.char_literal);
            break;
        case NECRO_AST_CONSTANT_BOOL:
            printf("(%s)\n", ast_node->constant.boolean_literal ? " True" : "False");
            break;
        }
        break;

    case NECRO_AST_IF_THEN_ELSE:
        puts("(If then else)");
        print_reified_ast_impl(ast_node->if_then_else.if_expr, intern, depth + 1);
        print_reified_ast_impl(ast_node->if_then_else.then_expr, intern, depth + 1);
        print_reified_ast_impl(ast_node->if_then_else.else_expr, intern, depth + 1);
        break;

    case NECRO_AST_TOP_DECL:
        puts("(Top Declaration)");
        print_reified_ast_impl(ast_node->top_declaration.declaration, intern, depth + 1);
        if (ast_node->top_declaration.next_top_decl != NULL)
        {
            print_reified_ast_impl(ast_node->top_declaration.next_top_decl, intern, depth);
        }
        break;

    case NECRO_AST_DECL:
        puts("(Declaration)");
        print_reified_ast_impl(ast_node->declaration.declaration_impl, intern, depth + 1);
        if (ast_node->declaration.next_declaration != NULL)
        {
            print_reified_ast_impl(ast_node->declaration.next_declaration, intern, depth);
        }
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        printf("(Assignment: %s, id: %d)\n", necro_intern_get_string(intern, ast_node->simple_assignment.variable_name), ast_node->simple_assignment.id.id);
        print_reified_ast_impl(ast_node->simple_assignment.rhs, intern, depth + 1);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        puts("(Right Hand Side)");
        print_reified_ast_impl(ast_node->right_hand_side.expression, intern, depth + 1);
        if (ast_node->right_hand_side.declarations != NULL)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "where");
            print_reified_ast_impl(ast_node->right_hand_side.declarations, intern, depth + 3);
        }
        break;

    case NECRO_AST_LET_EXPRESSION:
        puts("(Let)");
        print_reified_ast_impl(ast_node->let_expression.declarations, intern, depth + 1);
        if (ast_node->right_hand_side.declarations != NULL)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "in");
            print_reified_ast_impl(ast_node->let_expression.expression, intern, depth + 3);
        }
        break;


    case NECRO_AST_FUNCTION_EXPRESSION:
        puts("(fexp)");
        print_reified_ast_impl(ast_node->fexpression.aexp, intern, depth + 1);
        if (ast_node->fexpression.next_fexpression != NULL)
        {
            print_reified_ast_impl(ast_node->fexpression.next_fexpression, intern, depth + 1);
        }
        break;

    case NECRO_AST_VARIABLE:
    {
        const char* variable_string = necro_intern_get_string(intern, ast_node->variable.symbol);
        if (variable_string)
        {
            printf("(varid: %s, id: %d, vtype: %s)\n", variable_string, ast_node->variable.id.id, var_type_string(ast_node->variable.var_type));
        }
        else
        {
            puts("(varid: ???");
        }
        break;
    }

    case NECRO_AST_APATS:
        puts("(Apat)");
        print_reified_ast_impl(ast_node->apats.apat, intern, depth + 1);
        if (ast_node->apats.next_apat != NULL)
        {
            print_reified_ast_impl(ast_node->apats.next_apat, intern, depth);
        }
        break;

    case NECRO_AST_WILDCARD:
        puts("(_)");
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
        printf("(Apats Assignment: %s, id: %d)\n", necro_intern_get_string(intern, ast_node->apats_assignment.variable_name), ast_node->apats_assignment.id.id);
        print_reified_ast_impl(ast_node->apats_assignment.apats, intern, depth + 1);
        print_reified_ast_impl(ast_node->apats_assignment.rhs, intern, depth + 1);
        break;

    case NECRO_AST_LAMBDA:
        puts("\\(lambda)");
        print_reified_ast_impl(ast_node->lambda.apats, intern, depth + 1);
        for (uint32_t i = 0;  i < (depth + 1); ++i)
        {
            printf(AST_TAB);
        }
        puts("->");
        print_reified_ast_impl(ast_node->lambda.expression, intern, depth + 2);
        break;

    case NECRO_AST_DO:
        puts("(do)");
        print_reified_ast_impl(ast_node->do_statement.statement_list, intern, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        puts("([])");
        if (ast_node->expression_list.expressions != NULL)
            print_reified_ast_impl(ast_node->expression_list.expressions, intern, depth + 1);
        break;

    case NECRO_AST_TUPLE:
        puts("(tuple)");
        print_reified_ast_impl(ast_node->expression_list.expressions, intern, depth + 1);
        break;

    case NECRO_AST_LIST_NODE:
        printf("\r"); // clear current line
        print_reified_ast_impl(ast_node->list.item, intern, depth);
        if (ast_node->list.next_item != NULL)
        {
            print_reified_ast_impl(ast_node->list.next_item, intern, depth);
        }
        break;

    case NECRO_BIND_ASSIGNMENT:
        printf("(Bind: %s, id: %d)\n", necro_intern_get_string(intern, ast_node->bind_assignment.variable_name), ast_node->bind_assignment.id.id);
        print_reified_ast_impl(ast_node->bind_assignment.expression, intern, depth + 1);
        break;

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        {
            switch(ast_node->arithmetic_sequence.type)
            {
            case NECRO_ARITHMETIC_ENUM_FROM:
                puts("(EnumFrom)");
                print_reified_ast_impl(ast_node->arithmetic_sequence.from, intern, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_TO:
                puts("(EnumFromTo)");
                print_reified_ast_impl(ast_node->arithmetic_sequence.from, intern, depth + 1);
                print_reified_ast_impl(ast_node->arithmetic_sequence.to, intern, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_THEN_TO:
                puts("(EnumFromThenTo)");
                print_reified_ast_impl(ast_node->arithmetic_sequence.from, intern, depth + 1);
                print_reified_ast_impl(ast_node->arithmetic_sequence.then, intern, depth + 1);
                print_reified_ast_impl(ast_node->arithmetic_sequence.to, intern, depth + 1);
                break;
            default:
                assert(false);
                break;
            }
        }
        break;

    case NECRO_AST_CASE:
        puts("case");
        print_reified_ast_impl(ast_node->case_expression.expression, intern, depth + 1);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("of");
        print_reified_ast_impl(ast_node->case_expression.alternatives, intern, depth + 1);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        printf("\r"); // clear current line
        print_reified_ast_impl(ast_node->case_alternative.pat, intern, depth + 0);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("->");
        print_reified_ast_impl(ast_node->case_alternative.body, intern, depth + 1);
        break;

    case NECRO_AST_CONID:
    {
        const char* con_string = necro_intern_get_string(intern, ast_node->conid.symbol);
        if (con_string)
            printf("(conid: %s, id: %d, ctype: %s)\n", con_string, ast_node->conid.id.id, con_type_string(ast_node->conid.con_type));
        else
            puts("(conid: ???)");
        break;
    }

    case NECRO_AST_DATA_DECLARATION:
        puts("(data)");
        print_reified_ast_impl(ast_node->data_declaration.simpletype, intern, depth + 1);
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts(" = ");
        print_reified_ast_impl(ast_node->data_declaration.constructor_list, intern, depth + 1);
        break;

    case NECRO_AST_SIMPLE_TYPE:
        puts("(simple type)");
        print_reified_ast_impl(ast_node->simple_type.type_con, intern, depth + 1);
        if (ast_node->simple_type.type_var_list != NULL)
            print_reified_ast_impl(ast_node->simple_type.type_var_list, intern, depth + 2);
        break;

    case NECRO_AST_CONSTRUCTOR:
        puts("(constructor)");
        print_reified_ast_impl(ast_node->constructor.conid, intern, depth + 1);
        if (ast_node->constructor.arg_list != NULL)
            print_reified_ast_impl(ast_node->constructor.arg_list, intern, depth + 1);
        break;

    case NECRO_AST_TYPE_APP:
        puts("(type app)");
        print_reified_ast_impl(ast_node->type_app.ty, intern, depth + 1);
        if (ast_node->type_app.next_ty != NULL)
            print_reified_ast_impl(ast_node->type_app.next_ty, intern, depth + 1);
        break;

    case NECRO_AST_BIN_OP_SYM:
        printf("(%s)\n", necro_intern_get_string(intern, ast_node->bin_op_sym.op->conid.symbol));
        print_reified_ast_impl(ast_node->bin_op_sym.left, intern, depth + 1);
        print_reified_ast_impl(ast_node->bin_op_sym.right, intern, depth + 1);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        printf("\r");
        print_reified_ast_impl(ast_node->type_signature.var, intern, depth + 0);
        for (uint32_t i = 0;  i < depth + 1; ++i) printf(AST_TAB);
        puts("::");
        if (ast_node->type_signature.context != NULL)
        {
            print_reified_ast_impl(ast_node->type_signature.context, intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        print_reified_ast_impl(ast_node->type_signature.type, intern, depth + 1);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        printf("\r");
        print_reified_ast_impl(ast_node->type_class_context.conid, intern, depth + 0);
        print_reified_ast_impl(ast_node->type_class_context.varid, intern, depth + 0);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        puts("(class)");
        if (ast_node->type_class_declaration.context != NULL)
        {
            print_reified_ast_impl(ast_node->type_class_declaration.context, intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        print_reified_ast_impl(ast_node->type_class_declaration.tycls, intern, depth + 1);
        print_reified_ast_impl(ast_node->type_class_declaration.tyvar, intern, depth + 1);
        if (ast_node->type_class_declaration.declarations != NULL)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            print_reified_ast_impl(ast_node->type_class_declaration.declarations, intern, depth + 2);
        }
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        puts("(instance)");
        if (ast_node->type_class_instance.context != NULL)
        {
            print_reified_ast_impl(ast_node->type_class_instance.context, intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        print_reified_ast_impl(ast_node->type_class_instance.qtycls, intern, depth + 1);
        print_reified_ast_impl(ast_node->type_class_instance.inst, intern, depth + 1);
        if (ast_node->type_class_declaration.declarations != NULL)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            print_reified_ast_impl(ast_node->type_class_declaration.declarations, intern, depth + 2);
        }
        break;

    case NECRO_AST_FUNCTION_TYPE:
        puts("(");
        print_reified_ast_impl(ast_node->function_type.type, intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts("->");
        print_reified_ast_impl(ast_node->function_type.next_on_arrow, intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts(")");

        // print_ast_impl(ast, ast_get_node(ast, ast_node->function_type.type), intern, depth + 1);
        // for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        // puts("->");
        // print_ast_impl(ast, ast_get_node(ast, ast_node->function_type.next_on_arrow), intern, depth + 1);
        // for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        // puts(")");
        break;

    default:
        puts("(Undefined)");
        break;
    }
}

void necro_print_reified_ast(NecroAST_Reified* ast, NecroIntern* intern)
{
    if (ast->root == NULL)
    {
        puts("(Empty AST)");
    }
    else
    {
        print_reified_ast_impl(ast->root, intern, 0);
    }
}


NecroAST_Node_Reified* necro_reify(NecroAST* a_ast, NecroAST_LocalPtr a_ptr, NecroPagedArena* arena)
{
    if (a_ptr == null_local_ptr)
        return NULL;
    NecroAST_Node* node = ast_get_node(a_ast, a_ptr);
    if (node == NULL)
        return NULL;
    NecroAST_Node_Reified* reified_node = necro_paged_arena_alloc(arena, sizeof(NecroAST_Node_Reified));
    reified_node->type       = node->type;
    reified_node->source_loc = node->source_loc;
    reified_node->scope      = NULL;
    switch (node->type)
    {
    case NECRO_AST_UNDEFINED:
        reified_node->undefined._pad = node->undefined._pad;
        break;
    case NECRO_AST_CONSTANT:
        reified_node->constant.symbol = node->constant.symbol;
        reified_node->constant.type   = node->constant.type;
        break;
    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        reified_node->bin_op.lhs  = necro_reify(a_ast, node->bin_op.lhs, arena);
        reified_node->bin_op.rhs  = necro_reify(a_ast, node->bin_op.rhs, arena);
        reified_node->bin_op.type = node->bin_op.type;
        break;
    case NECRO_AST_IF_THEN_ELSE:
        reified_node->if_then_else.if_expr   = necro_reify(a_ast, node->if_then_else.if_expr, arena);
        reified_node->if_then_else.then_expr = necro_reify(a_ast, node->if_then_else.then_expr, arena);
        reified_node->if_then_else.else_expr = necro_reify(a_ast, node->if_then_else.else_expr, arena);
        break;
    case NECRO_AST_TOP_DECL:
        reified_node->top_declaration.declaration   = necro_reify(a_ast, node->top_declaration.declaration, arena);
        reified_node->top_declaration.next_top_decl = necro_reify(a_ast, node->top_declaration.next_top_decl, arena);
        break;
    case NECRO_AST_DECL:
        reified_node->declaration.declaration_impl = necro_reify(a_ast, node->declaration.declaration_impl, arena);
        reified_node->declaration.next_declaration = necro_reify(a_ast, node->declaration.next_declaration, arena);
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        reified_node->simple_assignment.rhs           = necro_reify(a_ast, node->simple_assignment.rhs, arena);
        reified_node->simple_assignment.variable_name = node->simple_assignment.variable_name;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        reified_node->apats_assignment.variable_name = node->apats_assignment.variable_name;
        reified_node->apats_assignment.apats         = necro_reify(a_ast, node->apats_assignment.apats, arena);
        reified_node->apats_assignment.rhs           = necro_reify(a_ast, node->apats_assignment.rhs, arena);
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        reified_node->right_hand_side.expression   = necro_reify(a_ast, node->right_hand_side.expression, arena);
        reified_node->right_hand_side.declarations = necro_reify(a_ast, node->right_hand_side.declarations, arena);
        break;
    case NECRO_AST_LET_EXPRESSION:
        reified_node->let_expression.expression   = necro_reify(a_ast, node->let_expression.expression, arena);
        reified_node->let_expression.declarations = necro_reify(a_ast, node->let_expression.declarations, arena);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        reified_node->fexpression.aexp             = necro_reify(a_ast, node->fexpression.aexp, arena);
        reified_node->fexpression.next_fexpression = necro_reify(a_ast, node->fexpression.next_fexpression, arena);
        break;
    case NECRO_AST_VARIABLE:
        reified_node->variable.symbol   = node->variable.symbol;
        reified_node->variable.var_type = node->variable.var_type;
        break;
    case NECRO_AST_APATS:
        reified_node->apats.apat      = necro_reify(a_ast, node->apats.apat, arena);
        reified_node->apats.next_apat = necro_reify(a_ast, node->apats.next_apat, arena);
        break;
    case NECRO_AST_WILDCARD:
        reified_node->undefined._pad = node->undefined._pad;
        break;
    case NECRO_AST_LAMBDA:
        reified_node->lambda.apats      = necro_reify(a_ast, node->lambda.apats, arena);
        reified_node->lambda.expression = necro_reify(a_ast, node->lambda.expression, arena);
        break;
    case NECRO_AST_DO:
        reified_node->do_statement.statement_list = necro_reify(a_ast, node->do_statement.statement_list, arena);
        break;
    case NECRO_AST_LIST_NODE:
        reified_node->list.item      = necro_reify(a_ast, node->list.item, arena);
        reified_node->list.next_item = necro_reify(a_ast, node->list.next_item, arena);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        reified_node->expression_list.expressions = necro_reify(a_ast, node->expression_list.expressions, arena);
        break;
    case NECRO_AST_TUPLE:
        reified_node->tuple.expressions = necro_reify(a_ast, node->tuple.expressions, arena);
        break;
    case NECRO_BIND_ASSIGNMENT:
        reified_node->bind_assignment.variable_name = node->bind_assignment.variable_name;
        reified_node->bind_assignment.expression    = necro_reify(a_ast, node->bind_assignment.expression, arena);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        reified_node->arithmetic_sequence.from = necro_reify(a_ast, node->arithmetic_sequence.from, arena);
        reified_node->arithmetic_sequence.then = necro_reify(a_ast, node->arithmetic_sequence.then, arena);
        reified_node->arithmetic_sequence.to   = necro_reify(a_ast, node->arithmetic_sequence.to, arena);
        reified_node->arithmetic_sequence.type = node->arithmetic_sequence.type;
        break;
    case NECRO_AST_CASE:
        reified_node->case_expression.expression   = necro_reify(a_ast, node->case_expression.expression, arena);
        reified_node->case_expression.alternatives = necro_reify(a_ast, node->case_expression.alternatives, arena);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        reified_node->case_alternative.body = necro_reify(a_ast, node->case_alternative.body, arena);
        reified_node->case_alternative.pat  = necro_reify(a_ast, node->case_alternative.pat, arena);
        break;
    case NECRO_AST_CONID:
        reified_node->conid.symbol   = node->conid.symbol;
        reified_node->conid.con_type = node->conid.con_type;
        break;
    case NECRO_AST_TYPE_APP:
        reified_node->type_app.ty      = necro_reify(a_ast, node->type_app.ty, arena);
        reified_node->type_app.next_ty = necro_reify(a_ast, node->type_app.next_ty, arena);
        break;
    case NECRO_AST_BIN_OP_SYM:
        reified_node->bin_op_sym.left  = necro_reify(a_ast, node->bin_op_sym.left, arena);
        reified_node->bin_op_sym.op    = necro_reify(a_ast, node->bin_op_sym.op, arena);
        reified_node->bin_op_sym.right = necro_reify(a_ast, node->bin_op_sym.right, arena);
        break;
    case NECRO_AST_CONSTRUCTOR:
        reified_node->constructor.conid    = necro_reify(a_ast, node->constructor.conid, arena);
        reified_node->constructor.arg_list = necro_reify(a_ast, node->constructor.arg_list, arena);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        reified_node->simple_type.type_con      = necro_reify(a_ast, node->simple_type.type_con, arena);
        reified_node->simple_type.type_var_list = necro_reify(a_ast, node->simple_type.type_var_list, arena);
        break;
    case NECRO_AST_DATA_DECLARATION:
        reified_node->data_declaration.simpletype       = necro_reify(a_ast, node->data_declaration.simpletype, arena);
        reified_node->data_declaration.constructor_list = necro_reify(a_ast, node->data_declaration.constructor_list, arena);
        break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:
        reified_node->type_class_context.conid = necro_reify(a_ast, node->type_class_context.conid, arena);
        reified_node->type_class_context.varid = necro_reify(a_ast, node->type_class_context.varid, arena);
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        reified_node->type_class_declaration.context      = necro_reify(a_ast, node->type_class_declaration.context, arena);
        reified_node->type_class_declaration.tycls        = necro_reify(a_ast, node->type_class_declaration.tycls, arena);
        reified_node->type_class_declaration.tyvar        = necro_reify(a_ast, node->type_class_declaration.tyvar, arena);
        reified_node->type_class_declaration.declarations = necro_reify(a_ast, node->type_class_declaration.declarations, arena);
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        reified_node->type_class_instance.context      = necro_reify(a_ast, node->type_class_instance.context, arena);
        reified_node->type_class_instance.qtycls       = necro_reify(a_ast, node->type_class_instance.qtycls, arena);
        reified_node->type_class_instance.inst         = necro_reify(a_ast, node->type_class_instance.inst, arena);
        reified_node->type_class_instance.declarations = necro_reify(a_ast, node->type_class_instance.declarations, arena);
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        reified_node->type_signature.var      = necro_reify(a_ast, node->type_signature.var, arena);
        reified_node->type_signature.context  = necro_reify(a_ast, node->type_signature.context, arena);
        reified_node->type_signature.type     = necro_reify(a_ast, node->type_signature.type, arena);
        reified_node->type_signature.sig_type = node->type_signature.sig_type;
        break;
    case NECRO_AST_FUNCTION_TYPE:
        reified_node->function_type.type          = necro_reify(a_ast, node->function_type.type, arena);
        reified_node->function_type.next_on_arrow = necro_reify(a_ast, node->function_type.next_on_arrow, arena);
        break;
    default:
        fprintf(stderr, "Unrecognized type during reification: %d", node->type);
        exit(1);
        break;
    }
    return reified_node;
}

NecroAST_Reified necro_create_reified_ast()
{
    NecroPagedArena        arena = necro_create_paged_arena();
    NecroAST_Node_Reified* root  = NULL;
    return (NecroAST_Reified)
    {
        .arena = arena,
        .root = root,
    };
}

NecroAST_Reified necro_reify_ast(NecroAST* a_ast, NecroAST_LocalPtr a_root)
{
    NecroPagedArena        arena = necro_create_paged_arena();
    NecroAST_Node_Reified* root  = necro_reify(a_ast, a_root, &arena);
    return (NecroAST_Reified)
    {
        .arena = arena,
        .root = root,
    };
}

void necro_destroy_reified_ast(NecroAST_Reified* ast)
{
    necro_destroy_paged_arena(&ast->arena);
}
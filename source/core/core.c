/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core.h"

#define NECRO_CORE_DEBUG 0
#if NECRO_CORE_DEBUG
#define TRACE_CORE(...) printf(__VA_ARGS__)
#else
#define TRACE_CORE(...)
#endif

static inline print_tabs(uint32_t num_tabs)
{
    for (uint32_t i = 0; i < num_tabs; ++i)
    {
        printf(STRING_TAB);
    }
}

void necro_print_data_con(NecroCoreAST_DataCon* data_con, NecroIntern* intern, uint32_t depth);
void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern, uint32_t depth);

void necro_print_data_con_arg(NecroCoreAST_DataExpr* data_con_arg, NecroIntern* intern, uint32_t depth)
{
    switch (data_con_arg->dataExpr_type)
    {
    case NECRO_CORE_DATAEXPR_DATACON:
        necro_print_data_con(data_con_arg->dataCon, intern, depth);
        break;

    case NECRO_CORE_DATAEXPR_EXPR:
        necro_print_core_node(data_con_arg->expr, intern, depth);
        break;

    default:
        assert(false && "Unexpected data expr type!?");
        break;
    }
}

void necro_print_data_con(NecroCoreAST_DataCon* data_con, NecroIntern* intern, uint32_t depth)
{
    print_tabs(depth);
    printf("(DataCon: %s)\n", necro_intern_get_string(intern, data_con->condid.symbol));
    NecroCoreAST_DataExpr* data_con_arg = data_con->arg_list;
    while (data_con_arg)
    {
        necro_print_data_con_arg(data_con_arg, intern, depth + 1);
        data_con_arg = data_con_arg->next;
    }
}

void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern, uint32_t depth)
{
    assert(ast_node);
    assert(intern);
    print_tabs(depth);

    switch (ast_node->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:
        printf("(Var: %s, %d)\n", necro_intern_get_string(intern, ast_node->var.symbol), ast_node->var.id.id);
        break;

    case NECRO_CORE_EXPR_BIND:
        printf("(Bind: %s, %d)\n", necro_intern_get_string(intern, ast_node->bind.var.symbol), ast_node->bind.var.id.id);
        necro_print_core_node(ast_node->bind.expr, intern, depth + 1);
        break;

    case NECRO_CORE_EXPR_LIT:
        {
            switch (ast_node->lit.type)
            {
            case NECRO_AST_CONSTANT_FLOAT:
                printf("(%f)\n", ast_node->lit.double_literal);
                break;
            case NECRO_AST_CONSTANT_INTEGER:
    #if WIN32
                printf("(%lli)\n", ast_node->lit.int_literal);
    #else
                printf("(%li)\n", ast_node->constant.int_literal);
    #endif
                break;
            case NECRO_AST_CONSTANT_STRING:
                {
                    const char* string = necro_intern_get_string(intern, ast_node->lit.symbol);
                    if (string)
                        printf("(\"%s\")\n", string);
                }
                break;
            case NECRO_AST_CONSTANT_CHAR:
                printf("(\'%c\')\n", ast_node->lit.char_literal);
                break;
            case NECRO_AST_CONSTANT_BOOL:
                printf("(%s)\n", ast_node->lit.boolean_literal ? " True" : "False");
                break;
            }
        }
        break;

    case NECRO_CORE_EXPR_APP:
        puts("(App)");
        necro_print_core_node(ast_node->app.exprA, intern, depth + 1);
        necro_print_core_node(ast_node->app.exprB, intern, depth + 1);
        break;

    case NECRO_CORE_EXPR_LAM:
        puts("(\\ ->)");
        necro_print_core_node(ast_node->lambda.arg, intern, depth + 1);
        necro_print_core_node(ast_node->lambda.expr, intern, depth + 1);
        break;

    case NECRO_CORE_EXPR_LET:
        {
            puts("(Let)");
            necro_print_core_node(ast_node->let.bind, intern, depth + 1);
            necro_print_core_node(ast_node->let.expr, intern, depth + 1);
        }
        break;

    case NECRO_CORE_EXPR_CASE:
        {
            puts("(Case)");
            necro_print_core_node(ast_node->case_expr.expr, intern, depth + 1);
            NecroCoreAST_CaseAlt* alt = ast_node->case_expr.alts;

            printf("\r");
            print_tabs(depth + 1);
            puts("(Of)");

            while (alt)
            {
                print_tabs(depth + 2);
                printf("(AltCon)\n");

                char s_alt_con[512];

                if (alt->altCon.altCon_type != NECRO_CORE_CASE_ALT_DATA)
                    print_tabs(depth + 3);

                switch (alt->altCon.altCon_type)
                {
                case NECRO_CORE_CASE_ALT_DATA:
                    necro_print_data_con(alt->altCon.dataCon, intern, depth + 3);
                    break;

                case NECRO_CORE_CASE_ALT_LITERAL:
                    switch (alt->altCon.lit.type)
                    {
                    case NECRO_AST_CONSTANT_FLOAT:
                        printf("(%f)\n", alt->altCon.lit.double_literal);
                        break;
                    case NECRO_AST_CONSTANT_INTEGER:
#if WIN32
                        printf("(%lli)\n", alt->altCon.lit.int_literal);
#else
                        printf("(%li)\n", alt->altCon.lit.int_literal);
#endif
                        break;
                    case NECRO_AST_CONSTANT_STRING:
                    {
                        const char* string = necro_intern_get_string(intern, alt->altCon.lit.symbol);
                        if (string)
                            printf("(\"%s\")\n", string);
                        else
                            assert(false);
                    }
                    break;
                    case NECRO_AST_CONSTANT_CHAR:
                        printf("(\'%c\')\n", alt->altCon.lit.char_literal);
                        break;
                    case NECRO_AST_CONSTANT_BOOL:
                        printf("(%s)\n", alt->altCon.lit.boolean_literal ? "True" : "False");
                        break;
                    default:
                        assert(false);
                        break;
                    }
                    break;

                case NECRO_CORE_CASE_ALT_DEFAULT:
                    printf("_\n");
                    break;

                default:
                    assert(false);
                    break;
                }

                necro_print_core_node(alt->expr, intern, depth + 3);
                alt = alt->next;
            }
        }
        break;

    case NECRO_CORE_EXPR_LIST:
        {
            puts("(CORE_EXPR_LIST)");
            NecroCoreAST_List* list_expr = &ast_node->list;
            while (list_expr)
            {
                necro_print_core_node(list_expr->expr, intern, depth + 1);
                list_expr = list_expr->next;
            }
        }
        break;

    case NECRO_CORE_EXPR_DATA:
        {
            printf("(Data %s)\n", necro_intern_get_string(intern, ast_node->data_decl.data_id.symbol));
            NecroCoreAST_DataCon* con = ast_node->data_decl.con_list;
            while (con)
            {
                necro_print_data_con(con, intern, depth + 1);
                con = con->next;
            }
        }
        break;

    default:
        printf("necro_print_core_node printing expression type unimplemented!: %s\n", core_ast_names[ast_node->expr_type]);
        assert(false);
        break;
    }
}

void necro_print_core(NecroCoreAST* ast, NecroIntern* intern)
{
    assert(ast != NULL);
    necro_print_core_node(ast->root, intern, 0);
}

NecroCoreAST_Expression* necro_transform_to_core_impl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node);

NecroCoreAST_Expression* necro_transform_bin_op(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_BIN_OP);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    // 1 + 2 --> (+) 1 2 --> ((+) 1) 2 --> App (App (+) 1) 2
    NecroAST_BinOp_Reified* bin_op = &necro_ast_node->bin_op;

    NecroCoreAST_Expression* inner_core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    inner_core_expr->expr_type = NECRO_CORE_EXPR_APP;
    NecroCoreAST_Application* inner_core_app = &inner_core_expr->app;

    NecroCoreAST_Expression* var_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    var_expr->expr_type = NECRO_CORE_EXPR_VAR;
    var_expr->var.symbol = bin_op->symbol;
    var_expr->var.id = bin_op->id;

    inner_core_app->exprA = var_expr;
    inner_core_app->exprB = necro_transform_to_core_impl(core_transform, bin_op->lhs);

    NecroCoreAST_Expression* outer_core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    outer_core_expr->expr_type = NECRO_CORE_EXPR_APP;
    NecroCoreAST_Application* outer_core_app = &outer_core_expr->app;
    outer_core_app->exprA = inner_core_expr;
    outer_core_app->exprB = necro_transform_to_core_impl(core_transform, bin_op->rhs);
    return outer_core_expr;
}

NecroCoreAST_Expression* necro_transform_if_then_else(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_IF_THEN_ELSE);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_IfThenElse_Reified* ast_if_then_else = &necro_ast_node->if_then_else;
    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_CASE;

    NecroCoreAST_Case* core_case = &core_expr->case_expr;
    core_case->expr = necro_transform_to_core_impl(core_transform, ast_if_then_else->if_expr);

    NecroCoreAST_CaseAlt* true_alt = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_CaseAlt));
    true_alt->expr = necro_transform_to_core_impl(core_transform, ast_if_then_else->then_expr);
    true_alt->altCon.lit.boolean_literal = true;
    true_alt->altCon.lit.type = NECRO_AST_CONSTANT_BOOL;
    true_alt->altCon.altCon_type = NECRO_CORE_CASE_ALT_LITERAL;
    true_alt->next = NULL;

    NecroCoreAST_CaseAlt* false_alt = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_CaseAlt));
    false_alt->expr = necro_transform_to_core_impl(core_transform, ast_if_then_else->else_expr);
    false_alt->altCon.lit.boolean_literal = false;
    false_alt->altCon.lit.type = NECRO_AST_CONSTANT_BOOL;
    false_alt->altCon.altCon_type = NECRO_CORE_CASE_ALT_LITERAL;
    false_alt->next = NULL;
    true_alt->next = false_alt;

    core_case->alts = true_alt;
    return core_expr;
}

NecroCoreAST_Expression* necro_transform_simple_assignment(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_SimpleAssignment_Reified* simple_assignment = &necro_ast_node->simple_assignment;
    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_BIND;
    NecroCoreAST_Bind* core_bind = &core_expr->bind;
    core_bind->var.symbol = simple_assignment->variable_name;
    core_bind->var.id = simple_assignment->id;
    core_bind->expr = necro_transform_to_core_impl(core_transform, simple_assignment->rhs);
    return core_expr;
}

NecroCoreAST_Expression* necro_transform_apats_assignment(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_APATS_ASSIGNMENT);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_ApatsAssignment_Reified* apats_assignment = &necro_ast_node->apats_assignment;
    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_BIND;
    NecroCoreAST_Bind* core_bind = &core_expr->bind;
    core_bind->var.symbol = apats_assignment->variable_name;
    core_bind->var.id = apats_assignment->id;

    // How to handle apats...rhs...?

    core_bind->expr = necro_transform_to_core_impl(core_transform, apats_assignment->rhs);
    return core_expr;
}


NecroCoreAST_Expression* necro_transform_right_hand_side(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_RIGHT_HAND_SIDE);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_RightHandSide_Reified* right_hand_side = &necro_ast_node->right_hand_side;
    if (right_hand_side->declarations)
    {
        assert(right_hand_side->declarations->type == NECRO_AST_DECL);
        NecroDeclarationGroupList* group_list = right_hand_side->declarations->declaration.group_list;
        NecroCoreAST_Expression* core_let_expr = NULL;
        NecroCoreAST_Let* core_let = NULL;
        NecroCoreAST_Expression* rhs_expression = necro_transform_to_core_impl(core_transform, right_hand_side->expression);
        while (group_list)
        {
            NecroDeclarationGroup* group = group_list->declaration_group;
            assert(group);
            assert(group->declaration_ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
            NecroAST_SimpleAssignment_Reified* simple_assignment = &group->declaration_ast->simple_assignment;
            NecroCoreAST_Expression* let_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
            let_expr->expr_type = NECRO_CORE_EXPR_LET;
            NecroCoreAST_Let* let = &let_expr->let;
            let->bind = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
            let->bind->expr_type = NECRO_CORE_EXPR_BIND;
            let->bind->bind.var.id = simple_assignment->id;
            let->bind->bind.var.symbol = simple_assignment->variable_name;
            let->bind->bind.expr = necro_transform_to_core_impl(core_transform, simple_assignment->rhs);
            if (core_let)
            {
                core_let->expr = let_expr;
            }
            else
            {
                core_let_expr = let_expr;
            }

            core_let = let;
            core_let->expr = rhs_expression;
            group_list = group_list->next;
        }

        return core_let_expr;
    }
    else
    {
        return necro_transform_to_core_impl(core_transform, right_hand_side->expression);
    }
}

NecroCoreAST_Expression* necro_transform_let(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_LET_EXPRESSION);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_LetExpression_Reified* reified_ast_let = &necro_ast_node->let_expression;
    if (reified_ast_let->declarations)
    {
        assert(reified_ast_let->declarations->type == NECRO_AST_DECL);
        NecroDeclarationGroupList* group_list = reified_ast_let->declarations->declaration.group_list;
        NecroCoreAST_Expression* core_let_expr = NULL;
        NecroCoreAST_Let* core_let = NULL;
        NecroCoreAST_Expression* rhs_expression = necro_transform_to_core_impl(core_transform, reified_ast_let->expression);
        while (group_list)
        {
            NecroDeclarationGroup* group = group_list->declaration_group;
            assert(group);
            assert(group->declaration_ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
            NecroAST_SimpleAssignment_Reified* simple_assignment = &group->declaration_ast->simple_assignment;
            NecroCoreAST_Expression* let_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
            let_expr->expr_type = NECRO_CORE_EXPR_LET;
            NecroCoreAST_Let* let = &let_expr->let;
            let->bind = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
            let->bind->expr_type = NECRO_CORE_EXPR_BIND;
            let->bind->bind.var.id = simple_assignment->id;
            let->bind->bind.var.symbol = simple_assignment->variable_name;
            let->bind->bind.expr = necro_transform_to_core_impl(core_transform, simple_assignment->rhs);
            if (core_let)
            {
                core_let->expr = let_expr;
            }
            else
            {
                core_let_expr = let_expr;
            }

            core_let = let;
            core_let->expr = rhs_expression;
            group_list = group_list->next;
        }

        return core_let_expr;
    }
    else
    {
        assert(false && "Let requires a binding!");
    }
}

NecroCoreAST_Expression* necro_transform_function_expression(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_FUNCTION_EXPRESSION);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_APP;
    core_expr->app.exprA = necro_transform_to_core_impl(core_transform, necro_ast_node->fexpression.aexp);
    core_expr->app.exprB = necro_transform_to_core_impl(core_transform, necro_ast_node->fexpression.next_fexpression);
    return core_expr;
}

NecroCoreAST_Expression* necro_transform_constant(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_CONSTANT);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_LIT;
    core_expr->lit = necro_ast_node->constant;
    return core_expr;
}

NecroCoreAST_DataCon* necro_transform_data_constructor(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_CONSTRUCTOR);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_Constructor_Reified* ast_constructor = &necro_ast_node->constructor;
    NecroCoreAST_DataCon* core_datacon = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_DataCon));
    core_datacon->condid.id = ast_constructor->conid->conid.id;
    core_datacon->condid.symbol = ast_constructor->conid->conid.symbol;
    core_datacon->next = NULL;
    core_datacon->arg_list = NULL;

    NecroCoreAST_DataExpr* current_core_arg = NULL;
    NecroAST_Node_Reified* arg_list = ast_constructor->arg_list;

    while (arg_list)
    {
        NecroAST_Node_Reified* ast_item = arg_list->list.item;
        NecroCoreAST_DataExpr* next_core_arg_data_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_DataExpr));

        switch (ast_item->type)
        {
        case NECRO_AST_VARIABLE:
            {
                NecroCoreAST_Expression* next_core_arg = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
                next_core_arg->expr_type = NECRO_CORE_EXPR_VAR;
                next_core_arg->var.id = ast_item->variable.id;
                next_core_arg->var.symbol = ast_item->variable.symbol;

                next_core_arg_data_expr->dataExpr_type = NECRO_CORE_DATAEXPR_EXPR;
                next_core_arg_data_expr->expr = next_core_arg;
                next_core_arg_data_expr->next = NULL;
            }
            break;

        case NECRO_AST_TYPE_APP:
            {
                NecroAST_TypeApp_Reified* type_app = &ast_item->type_app;

                NecroCoreAST_Expression* next_core_arg = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
                next_core_arg->expr_type = NECRO_CORE_EXPR_APP;
                next_core_arg->app.exprA = necro_transform_to_core_impl(core_transform, type_app->ty);
                next_core_arg->app.exprB = necro_transform_to_core_impl(core_transform, type_app->next_ty);
                next_core_arg_data_expr->dataExpr_type = NECRO_CORE_DATAEXPR_EXPR;
                next_core_arg_data_expr->expr = next_core_arg;
                next_core_arg_data_expr->next = NULL;
            }
            break;

        default:
            assert(false && "Unexpected node type when transforming data constructor arg!");
        }

        if (current_core_arg)
        {
            current_core_arg->next = next_core_arg_data_expr;
        }
        else
        {
            core_datacon->arg_list = next_core_arg_data_expr;
        }

        current_core_arg = next_core_arg_data_expr;
        arg_list = arg_list->list.next_item;
    }

    return core_datacon;
}

NecroCoreAST_Expression* necro_transform_data_decl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_DATA_DECLARATION);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_DataDeclaration_Reified* data_decl = &necro_ast_node->data_declaration;
    assert(data_decl->simpletype->type == NECRO_AST_SIMPLE_TYPE);
    NecroAST_SimpleType_Reified* simple_type = &data_decl->simpletype->simple_type;
    assert(simple_type->type_con->type == NECRO_AST_CONID);
    NecroAST_ConID_Reified* conid = &simple_type->type_con->conid;

    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_DATA;
    core_expr->data_decl.data_id.id = conid->id;
    core_expr->data_decl.data_id.symbol = conid->symbol;
    core_expr->data_decl.con_list = NULL;

    NecroCoreAST_DataCon* current_core_data_con = NULL;

    assert(data_decl->constructor_list->type == NECRO_AST_LIST_NODE);
    NecroAST_ListNode_Reified* list_node = &data_decl->constructor_list->list;
    assert(list_node);
    while (list_node)
    {
        assert(list_node->item->type == NECRO_AST_CONSTRUCTOR);
        NecroAST_Constructor_Reified* con = &list_node->item->constructor;

        NecroCoreAST_DataCon* next_core_data_con = NULL;
        switch (list_node->item->type)
        {
        case NECRO_AST_CONSTRUCTOR:
            next_core_data_con = necro_transform_data_constructor(core_transform, list_node->item);
            break;
        default:
            assert(false && "Unexpected type during data declaration core transformation!");
            break;
        }

        if (core_expr->data_decl.con_list == NULL)
        {
            core_expr->data_decl.con_list = next_core_data_con;
        }
        else
        {
            current_core_data_con->next = next_core_data_con;
        }

        current_core_data_con = next_core_data_con;
        list_node = list_node->next_item;
    }

    return core_expr;
}

NecroCoreAST_Expression* necro_transform_conid(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_CONID);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_VAR;
    core_expr->var.id = necro_ast_node->conid.id;
    core_expr->var.symbol = necro_ast_node->conid.symbol;
    return core_expr;
}

NecroCoreAST_Expression* necro_transform_top_decl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_TOP_DECL);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroCoreAST_Expression* top_expression = NULL;
    NecroCoreAST_Expression* next_node = NULL;
    NecroDeclarationGroupList* group_list = necro_ast_node->top_declaration.group_list;
    assert(group_list);
    while (group_list)
    {
        NecroDeclarationGroup* group = group_list->declaration_group;
        assert(group);
        NecroCoreAST_Expression* last_node = next_node;
        NecroCoreAST_Expression* expression = necro_transform_to_core_impl(core_transform, (NecroAST_Node_Reified*) group->declaration_ast);
        next_node = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
        next_node->expr_type = NECRO_CORE_EXPR_LIST;
        next_node->list.expr = expression;
        next_node->list.next = NULL;
        if (top_expression == NULL)
        {
            top_expression = next_node;
        }
        else
        {
            last_node->list.next = next_node;
        }

        group_list = group_list->next;
    }

    assert(top_expression);
    return top_expression;
}

NecroCoreAST_Expression* necro_transform_lambda(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_LAMBDA);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_Lambda_Reified* lambda = &necro_ast_node->lambda;
    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_LAM;

    NecroCoreAST_Expression* last_lambda = NULL;
    NecroCoreAST_Expression* current_lambda = core_expr;

    NecroAST_Apats_Reified* apats = &lambda->apats->apats;
    while (apats)
    {
        NecroAST_Node_Reified* apat = apats->apat;
        current_lambda->lambda.arg = necro_transform_to_core_impl(core_transform, apats->apat);
        current_lambda->lambda.expr = NULL;

        if (apats->next_apat)
        {
            last_lambda = current_lambda;
            current_lambda = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
            current_lambda->expr_type = NECRO_CORE_EXPR_LAM;
            current_lambda->lambda.arg = NULL;
            current_lambda->lambda.expr = NULL;
        }

        if (last_lambda)
        {
            last_lambda->lambda.expr = current_lambda;
        }

        apats = apats->next_apat ? &apats->next_apat->apats : NULL;
    }

    current_lambda->lambda.expr = necro_transform_to_core_impl(core_transform, lambda->expression);
    return core_expr;
}

NecroCoreAST_Expression* necro_transform_case(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_CASE);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_Case_Reified* case_ast = &necro_ast_node->case_expression;
    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_CASE;
    NecroCoreAST_Case* core_case = &core_expr->case_expr;
    core_case->expr = necro_transform_to_core_impl(core_transform, case_ast->expression);
    core_case->alts = NULL;

    NecroAST_Node_Reified* alt_list_node = case_ast->alternatives;
    NecroAST_ListNode_Reified* list_node = &alt_list_node->list;
    NecroCoreAST_CaseAlt* case_alt = NULL;

    while (list_node)
    {
        NecroAST_Node_Reified* alt_node = list_node->item;
        assert(list_node->item->type == NECRO_AST_CASE_ALTERNATIVE);
        NecroAST_CaseAlternative_Reified* alt = &alt_node->case_alternative;

        NecroCoreAST_CaseAlt* last_case_alt = case_alt;
        case_alt = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_CaseAlt));
        case_alt->expr = necro_transform_to_core_impl(core_transform, alt->body);
        case_alt->next = NULL;

        switch (alt->pat->type)
        {

        case NECRO_AST_CONSTANT:
            case_alt->altCon.altCon_type = NECRO_CORE_CASE_ALT_LITERAL;
            case_alt->altCon.lit = alt->pat->constant;
            break;

        case NECRO_AST_WILDCARD:
            case_alt->altCon.altCon_type = NECRO_CORE_CASE_ALT_DEFAULT;
            case_alt->altCon._defaultPadding = 0;
            break;

        case NECRO_AST_CONSTRUCTOR:
            case_alt->altCon.altCon_type = NECRO_CORE_CASE_ALT_DATA;
            case_alt->altCon.dataCon = necro_transform_data_constructor(core_transform, alt->pat);
            break;

        default:
            printf("necro_transform_case pattern type not implemented!: %d\n", alt->pat->type);
            assert(false);
            return NULL;
        }

        if (last_case_alt)
        {
            last_case_alt->next = case_alt;
        }
        else
        {
            core_case->alts = case_alt;
        }

        list_node = list_node->next_item;
    }

    return core_expr;
}

NecroCoreAST_Expression* necro_transform_variable(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_VARIABLE);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_VAR;
    core_expr->var.id = necro_ast_node->variable.id;
    core_expr->var.symbol = necro_ast_node->variable.symbol;
    return core_expr;
}

NecroCoreAST_Expression* necro_transform_to_core_impl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(necro_ast_node);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    switch (necro_ast_node->type)
    {
    case NECRO_AST_BIN_OP:
        return necro_transform_bin_op(core_transform, necro_ast_node);

    case NECRO_AST_IF_THEN_ELSE:
        return necro_transform_if_then_else(core_transform, necro_ast_node);

    case NECRO_AST_TOP_DECL:
        return necro_transform_top_decl(core_transform, necro_ast_node);

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        return necro_transform_simple_assignment(core_transform, necro_ast_node);

    case NECRO_AST_APATS_ASSIGNMENT:
        return necro_transform_apats_assignment(core_transform, necro_ast_node);

    case NECRO_AST_RIGHT_HAND_SIDE:
        return necro_transform_right_hand_side(core_transform, necro_ast_node);

    case NECRO_AST_LET_EXPRESSION:
        return necro_transform_let(core_transform, necro_ast_node);

    case NECRO_AST_FUNCTION_EXPRESSION:
        return necro_transform_function_expression(core_transform, necro_ast_node);

    case NECRO_AST_CONSTANT:
        return necro_transform_constant(core_transform, necro_ast_node);

    case NECRO_AST_LAMBDA:
        return necro_transform_lambda(core_transform, necro_ast_node);

    case NECRO_AST_CASE:
        return necro_transform_case(core_transform, necro_ast_node);

    case NECRO_AST_VARIABLE:
        return necro_transform_variable(core_transform, necro_ast_node);

    case NECRO_AST_DATA_DECLARATION:
        return necro_transform_data_decl(core_transform, necro_ast_node);

    case NECRO_AST_CONID:
        return necro_transform_conid(core_transform, necro_ast_node);

    default:
        printf("necro_transform_to_core transforming AST type unimplemented!: %d\n", necro_ast_node->type);
        assert(false);
        break;
    }

    assert(false);
    return NULL;
}

void necro_transform_to_core(NecroTransformToCore* core_transform)
{
    assert(core_transform);
    assert(core_transform->necro_ast);
    assert(core_transform->necro_ast->root);
    core_transform->transform_state = NECRO_SUCCESS;
    core_transform->core_ast->root = necro_transform_to_core_impl(core_transform, core_transform->necro_ast->root);
}

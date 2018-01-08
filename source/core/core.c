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

void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern, uint32_t depth)
{
    assert(ast_node);
    assert(intern);
    for (uint32_t i = 0; i < depth; ++i)
    {
        printf(STRING_TAB);
    }

    switch (ast_node->expr_type)
    {
    case NECRO_CORE_EXPR_VAR:
        printf("(Var: %s, %d)\n", necro_intern_get_string(intern, ast_node->var.symbol), ast_node->var.id);
        break;
    
    case NECRO_CORE_EXPR_LIT:
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
        break;
    
    case NECRO_CORE_EXPR_LET:
        puts("(Let)");
        NecroCoreAST_Expression bind_expr;
        bind_expr.expr_type = NECRO_CORE_EXPR_VAR;
        bind_expr.var = ast_node->var;
        necro_print_core_node(&bind_expr, intern, depth + 1);
        necro_print_core_node(ast_node->let.expr, intern, depth + 1);
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

NecroCoreAST_Expression* necro_transform_simple_assignment(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroAST_SimpleAssignment_Reified* simple_assignment = &necro_ast_node->simple_assignment;
    NecroCoreAST_Expression* core_expr = necro_paged_arena_alloc(&core_transform->core_ast->arena, sizeof(NecroCoreAST_Expression));
    core_expr->expr_type = NECRO_CORE_EXPR_LET;
    NecroCoreAST_Let* core_let = &core_expr->let;
    core_let->bind.symbol = simple_assignment->variable_name;
    core_let->bind.id = simple_assignment->id;
    core_let->expr = necro_transform_to_core_impl(core_transform, simple_assignment->rhs);
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
    NecroAST_Declaration_Reified* decl = right_hand_side->declarations ? &right_hand_side->declarations->declaration : NULL;
    if (decl)
    {
        assert(right_hand_side->declarations->type == NECRO_AST_DECL);
        NecroCoreAST_Expression* core_let_expr = NULL;
        NecroCoreAST_Let* core_let = NULL;
        while (decl)
        {
            NecroCoreAST_Expression* let_expr = necro_transform_to_core_impl(core_transform, decl->declaration_impl);
            assert(let_expr->expr_type == NECRO_CORE_EXPR_LET);
            if (core_let)
            {
                core_let->expr = let_expr;
            }
            else
            {
                core_let_expr = let_expr;
            }

            core_let = &let_expr->let;
            if (decl->next_declaration)
            {
                assert(decl->next_declaration->type == NECRO_AST_DECL);
                decl = &decl->next_declaration->declaration;
            }
        }

        return core_let_expr; // finish this!
    }
    else
    {
        return necro_transform_to_core_impl(core_transform, right_hand_side->expression);
    }
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

NecroCoreAST_Expression* necro_transform_top_decl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(core_transform);
    assert(necro_ast_node);
    assert(necro_ast_node->type == NECRO_AST_TOP_DECL);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    NecroDeclarationGroupList* top_decl_group_list = necro_ast_node->top_declaration.group_list;
    assert(top_decl_group_list);
    NecroDeclarationGroup* group = top_decl_group_list->declaration_group;
    NecroCoreAST_Expression* top_expression = NULL;
    NecroCoreAST_Expression* next_node = NULL;
    while (group)
    {
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
        group = group->next;
    }

    assert(top_expression);
    return top_expression;
}

NecroCoreAST_Expression* necro_transform_to_core_impl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node)
{
    assert(necro_ast_node);
    if (core_transform->transform_state != NECRO_CORE_TRANSFORMING)
        return NULL;

    switch (necro_ast_node->type)
    {
    case NECRO_AST_TOP_DECL:
        return necro_transform_top_decl(core_transform, necro_ast_node);

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        return necro_transform_simple_assignment(core_transform, necro_ast_node);

    case NECRO_AST_RIGHT_HAND_SIDE:
        return necro_transform_right_hand_side(core_transform, necro_ast_node);

    case NECRO_AST_CONSTANT:
        return necro_transform_constant(core_transform, necro_ast_node);

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

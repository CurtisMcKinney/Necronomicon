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

void necro_print_core_node(NecroCoreAST_Expression* ast_node, NecroIntern* intern)
{
    assert(ast_node != NULL);
    switch (ast_node->expr_type)
    {
    default:
        printf("necro_print_core_node printing expression type unimplemented!: %s\n", core_ast_names[ast_node->expr_type]);
        //assert(false);
        break;
    }
}

void necro_print_core(NecroCoreAST* ast, NecroIntern* intern)
{
    assert(ast != NULL);
    necro_print_core_node(ast->root, intern);
}

NecroCoreAST_Expression* necro_transform_to_core_impl(NecroTransformToCore* core_transform, NecroAST_Node_Reified* necro_ast_node);

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
    NecroCoreAST_Expression* expression = NULL;
    NecroCoreAST_Expression* top_expression = NULL;
    while (group)
    {
        NecroCoreAST_Expression* last_expression = expression;
        NecroCoreAST_Expression* expression = necro_transform_to_core_impl(core_transform, (NecroAST_Node_Reified*) group->declaration_ast);
        if (top_expression == NULL)
        {
            top_expression = expression;
        }
        else
        {
            assert(last_expression);
            assert(last_expression->expr_type == NECRO_CORE_EXPR_LET);
            last_expression->let.expr = expression;
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

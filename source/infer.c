/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"

typedef NecroAST_Node_Reified NecroNode;

#define NECRO_INFER_DEBUG           0
#if NECRO_INFER_DEBUG
#define TRACE_INFER(...) printf(__VA_ARGS__)
#else
#define TRACE_INFER(...)
#endif

NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroNode* ast, const char* error_message, ...);
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast);

/*
DONE:
    NECRO_AST_UN_OP,
    NECRO_AST_UNDEFINED,
    NECRO_AST_CONSTANT,
    NECRO_AST_BIN_OP,
    NECRO_AST_IF_THEN_ELSE,
    NECRO_AST_SIMPLE_ASSIGNMENT,
    NECRO_AST_RIGHT_HAND_SIDE,
    NECRO_AST_FUNCTION_EXPRESSION,
    NECRO_AST_VARIABLE,
    NECRO_AST_LET_EXPRESSION,
    NECRO_AST_TUPLE,

IN PROGRESS:
    NECRO_AST_TOP_DECL,
    NECRO_AST_DECL,
    NECRO_AST_EXPRESSION_LIST,

TODO:
    NECRO_AST_APATS_ASSIGNMENT,
    NECRO_AST_APATS,
    NECRO_AST_WILDCARD,
    NECRO_AST_LAMBDA,
    NECRO_AST_DO,
    NECRO_AST_LIST_NODE,
    NECRO_BIND_ASSIGNMENT,
    NECRO_AST_ARITHMETIC_SEQUENCE,
    NECRO_AST_CASE,
    NECRO_AST_CASE_ALTERNATIVE,
    NECRO_AST_CONID,
    NECRO_AST_TYPE_APP,
    NECRO_AST_BIN_OP_SYM,
    NECRO_AST_CONSTRUCTOR,
    NECRO_AST_SIMPLE_TYPE,
    NECRO_AST_DATA_DECLARATION,
    NECRO_AST_TYPE_CLASS_CONTEXT,
    NECRO_AST_TYPE_CLASS_DECLARATION,
    NECRO_AST_TYPE_CLASS_INSTANCE,
    NECRO_AST_TYPE_SIGNATURE,
    NECRO_AST_FUNCTION_TYPE,
    // NECRO_AST_MODULE,
*/

//=====================================================
// Constant
//=====================================================
NecroType* necro_infer_constant(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONSTANT);
    if (necro_is_infer_error(infer)) return NULL;
    switch (ast->constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:   return infer->prim_types.float_type;
    case NECRO_AST_CONSTANT_INTEGER: return infer->prim_types.int_type;
    case NECRO_AST_CONSTANT_BOOL:    return infer->prim_types.bool_type;
    case NECRO_AST_CONSTANT_CHAR:    return infer->prim_types.char_type;
    case NECRO_AST_CONSTANT_STRING:  return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: String not implemented....");
    default:                         return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unrecognized constant type: %d", ast->constant.type);
    }
}

//=====================================================
// Variable
//=====================================================
NecroType* necro_infer_var(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_VARIABLE);
    if (necro_is_infer_error(infer)) return NULL;
    if (ast->variable.var_type == NECRO_VAR_DECLARATION)
    {
        NecroType* new_name = necro_new_name(infer);
        necro_env_set(infer, (NecroVar) { ast->variable.id }, new_name);
        return new_name;
    }
    else if (ast->variable.var_type == NECRO_VAR_VAR)
    {
        NecroType* type = necro_env_get(infer, (NecroVar) { ast->variable.id });
        if (type == NULL)
            return necro_infer_ast_error(infer, NULL, ast, "Type not found for variable: %s", necro_intern_get_string(infer->intern, ast->variable.symbol));
        return necro_inst(infer, type);
    }
    else
    {
        assert(false);
        return NULL;
    }
}

//=====================================================
// Wildcard
//=====================================================
NecroType* necro_infer_wildcard(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_WILDCARD);
    if (necro_is_infer_error(infer)) return NULL;
    return necro_new_name(infer);
}

//=====================================================
// Function Expression
//=====================================================
NecroType* necro_infer_fexpr(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* e0_type     = necro_infer_go(infer, ast->fexpression.aexp);
    NecroType* e1_type     = necro_infer_go(infer, ast->fexpression.next_fexpression);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* result_type = necro_new_name(infer);
    NecroType* f_type      = necro_create_type_fun(infer, e1_type, result_type);
    necro_unify(infer, e0_type, f_type);
    return result_type;
}

//=====================================================
// If then else
//=====================================================
NecroType* necro_infer_if_then_else(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_IF_THEN_ELSE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* if_type   = necro_infer_go(infer, ast->if_then_else.if_expr);
    NecroType* then_type = necro_infer_go(infer, ast->if_then_else.then_expr);
    NecroType* else_type = necro_infer_go(infer, ast->if_then_else.else_expr);
    if (necro_is_infer_error(infer)) return NULL;
    necro_unify(infer, if_type, infer->prim_types.bool_type);
    necro_unify(infer, then_type, else_type);
    if (necro_is_infer_error(infer)) return NULL;
    return then_type;
}

//=====================================================
// BinOp
//=====================================================
NecroType* necro_infer_bin_op(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_BIN_OP);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* x_type      = necro_infer_go(infer, ast->bin_op.lhs);
    NecroType* op_type     = necro_get_bin_op_type(infer, ast->bin_op.type);
    NecroType* y_type      = necro_infer_go(infer, ast->bin_op.rhs);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* result_type = necro_new_name(infer);
    NecroType* bin_op_type = necro_create_type_fun(infer, x_type, necro_create_type_fun(infer, y_type, result_type));
    necro_unify(infer, op_type, bin_op_type);
    if (necro_is_infer_error(infer)) return NULL;
    return result_type;
}

//=====================================================
// Apat
//=====================================================
NecroType* necro_infer_apat(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_APATS);
    if (necro_is_infer_error(infer)) return NULL;
    return NULL;
}

//=====================================================
// Apats Assignment
//=====================================================
NecroType* necro_infer_apats_assignment(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_APATS_ASSIGNMENT);
    if (necro_is_infer_error(infer)) return NULL;

    NecroType* proxy_type = necro_env_get(infer, (NecroVar){ast->apats_assignment.id});
    NecroNode* apats      = ast->apats_assignment.apats;
    NecroType* f_type     = NULL;
    NecroType* f_head     = NULL;
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        if (necro_is_infer_error(infer)) return NULL;
        if (f_type == NULL)
        {
            f_type = necro_create_type_fun(infer, necro_infer_go(infer, apats->apats.apat), NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_create_type_fun(infer, necro_infer_go(infer, apats->apats.apat), NULL);
            f_type = f_type->fun.type2;
        }
        apats = apats->apats.next_apat;
    }
    f_type->fun.type2 = necro_infer_go(infer, ast->apats_assignment.rhs);
    if (necro_is_infer_error(infer)) return NULL;
    necro_unify(infer, proxy_type, f_head);
    necro_env_set(infer, (NecroVar){ast->apats_assignment.id}, necro_gen(infer, proxy_type));
    return NULL;
}

//=====================================================
// Simple Assignment
//=====================================================
NecroType* necro_infer_simple_assignment(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    if (necro_is_infer_error(infer)) return NULL;

    // How the fuck to handle this now....
    NecroType* proxy_type = necro_env_get(infer, (NecroVar){ast->simple_assignment.id});
    NecroType* rhs_type   = necro_infer_go(infer, ast->simple_assignment.rhs);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    necro_unify(infer, proxy_type, rhs_type);
    necro_env_set(infer, (NecroVar){ast->simple_assignment.id}, necro_gen(infer, proxy_type));

    return NULL;
}

//=====================================================
// RightHandSide
//=====================================================
NecroType* necro_infer_right_hand_side(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    if (necro_is_infer_error(infer)) return NULL;
    necro_infer_go(infer, ast->right_hand_side.declarations);
    return necro_infer_go(infer, ast->right_hand_side.expression);
}

//=====================================================
// Declaration
//=====================================================
NecroType* necro_infer_declaration(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DECL);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_decl = ast;

    //----------------------------------------------------
    // Pass 1, create new names to serve as proxy types (This is the easy solution to recursive let, but produces less general types)
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        switch (current_decl->declaration.declaration_impl->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            NecroType* new_name = necro_new_name(infer);
            necro_env_set(infer, (NecroVar) { current_decl->declaration.declaration_impl->simple_assignment.id }, new_name);
            break;
        }
        case NECRO_AST_APATS_ASSIGNMENT:
        {
            NecroType* new_name = necro_new_name(infer);
            necro_env_set(infer, (NecroVar) { current_decl->declaration.declaration_impl->apats_assignment.id }, new_name);
            break;
        }
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->declaration.declaration_impl->type);
        }
        current_decl = current_decl->declaration.next_declaration;
    }
    if (necro_is_infer_error(infer)) return NULL;
    current_decl = ast;

    //----------------------------------------------------
    // Pass 2, Infer types for declarations, then unify assignments with their proxy types
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        necro_infer_go(infer, current_decl->declaration.declaration_impl);
        current_decl = current_decl->declaration.next_declaration;
    }
    if (necro_is_infer_error(infer)) return NULL;

    // Declarations themselves have no types
    return NULL;
}

//=====================================================
// Top Declaration
//=====================================================
NecroType* necro_infer_top_declaration(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TOP_DECL);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_decl = ast;

    //----------------------------------------------------
    // Pass 1, create new names to serve as proxy types (This is the easy solution to recursive let, but produces less general types)
    while (current_decl != NULL)
    {
        TRACE_INFER("top while 2\n");
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        switch (current_decl->top_declaration.declaration->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            NecroType* new_name = necro_new_name(infer);
            necro_env_set(infer, (NecroVar) { current_decl->top_declaration.declaration->simple_assignment.id }, new_name);
            break;
        }
        case NECRO_AST_APATS_ASSIGNMENT:
        {
            NecroType* new_name = necro_new_name(infer);
            necro_env_set(infer, (NecroVar) { current_decl->declaration.declaration_impl->apats_assignment.id }, new_name);
            break;
        }
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->top_declaration.declaration->type);
        }
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return NULL;

    //----------------------------------------------------
    // Pass 2, Infer types for rhs of declarations
    current_decl = ast;
    while (current_decl != NULL)
    {
        TRACE_INFER("top while 2\n");
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        necro_infer_go(infer, current_decl->declaration.declaration_impl);
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return NULL;

    // Declarations themselves have no types
    return NULL;
}

//=====================================================
// Let
//=====================================================
NecroType* necro_infer_let_expression(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LET_EXPRESSION);
    if (necro_is_infer_error(infer)) return NULL;
    necro_infer_go(infer, ast->let_expression.declarations);
    return necro_infer_go(infer, ast->let_expression.expression);
}

//=====================================================
// Expression List
//=====================================================
NecroType* necro_infer_expression_list(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_cell = ast->expression_list.expressions;
    NecroType* list_type    = necro_new_name(infer);
    while (current_cell != NULL)
    {
        necro_unify(infer, list_type, necro_infer_go(infer, current_cell->list.item));
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    return necro_make_con_1(infer, infer->prim_types.list_symbol, list_type);
}

//=====================================================
// Tuple
//=====================================================
NecroType* necro_infer_tuple(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    if (necro_is_infer_error(infer)) return NULL;
    size_t     tuple_count        = 0;
    NecroNode* current_expression = ast->tuple.expressions;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        tuple_count++;
        current_expression = current_expression->list.next_item;
    }
    current_expression = ast->tuple.expressions;
    switch (tuple_count)
    {
    case 2:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_2(infer, infer->prim_types.tuple_symbols.two, e1, e2);
    }
    case 3:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_3(infer, infer->prim_types.tuple_symbols.three, e1, e2, e3);
    }
    case 4:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_4(infer, infer->prim_types.tuple_symbols.four, e1, e2, e3, e4);
    }
    case 5:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e5 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_5(infer, infer->prim_types.tuple_symbols.five, e1, e2, e3, e4, e5);
    }
    case 6:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e5 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e6 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_6(infer, infer->prim_types.tuple_symbols.six, e1, e2, e3, e4, e5, e6);
    }
    case 7:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e5 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e6 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e7 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_7(infer, infer->prim_types.tuple_symbols.seven, e1, e2, e3, e4, e5, e6, e7);
    }
    case 8:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e5 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e6 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e7 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e8 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_8(infer, infer->prim_types.tuple_symbols.eight, e1, e2, e3, e4, e5, e6, e7, e8);
    }
    case 9:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e5 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e6 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e7 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e8 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e9 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_9(infer, infer->prim_types.tuple_symbols.nine, e1, e2, e3, e4, e5, e6, e7, e8, e9);
    }
    case 10:
    {
        NecroType* e1 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e2 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e3 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e4 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e5 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e6 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e7 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e8 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e9 = necro_infer_go(infer, current_expression->list.item);
        current_expression = current_expression->list.next_item;
        NecroType* e10 = necro_infer_go(infer, current_expression->list.item);
        if (necro_is_infer_error(infer)) return NULL;
        return necro_make_con_10(infer, infer->prim_types.tuple_symbols.ten, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10);
    }
    default: return necro_infer_ast_error(infer, NULL, ast, "Illegal tuple construction");
    }
}


//=====================================================
// Infer Go
//=====================================================
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast)
{
    TRACE_INFER("necro_infer_go\n");
    assert(infer != NULL);
    if (infer->error.return_code != NECRO_SUCCESS)
        return NULL;
    if (ast == NULL)
        return NULL;
    switch (ast->type)
    {
    case NECRO_AST_CONSTANT:            return necro_infer_constant(infer, ast);
    case NECRO_AST_VARIABLE:            return necro_infer_var(infer, ast);
    case NECRO_AST_FUNCTION_EXPRESSION: return necro_infer_fexpr(infer, ast);
    case NECRO_AST_BIN_OP:              return necro_infer_bin_op(infer, ast);
    case NECRO_AST_IF_THEN_ELSE:        return necro_infer_if_then_else(infer, ast);
    case NECRO_AST_SIMPLE_ASSIGNMENT:   return necro_infer_simple_assignment(infer, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:     return necro_infer_right_hand_side(infer, ast);
    case NECRO_AST_LET_EXPRESSION:      return necro_infer_let_expression(infer, ast);
    case NECRO_AST_DECL:                return necro_infer_declaration(infer, ast);
    case NECRO_AST_TOP_DECL:            return necro_infer_top_declaration(infer, ast);
    case NECRO_AST_TUPLE:               return necro_infer_tuple(infer, ast);
    case NECRO_AST_EXPRESSION_LIST:     return necro_infer_expression_list(infer, ast);
    case NECRO_AST_APATS_ASSIGNMENT:    return necro_infer_apats_assignment(infer, ast);
    case NECRO_AST_WILDCARD:            return necro_infer_wildcard(infer, ast);
    default:                            return necro_infer_ast_error(infer, NULL, ast, "AST type %d has not been implemented for type inference", ast->type);
    }
    return NULL;
}

NecroType* necro_infer(NecroInfer* infer, NecroNode* ast)
{
    TRACE_INFER("necro_infer\n");
    NecroType* result = necro_infer_go(infer, ast);
    necro_check_type_sanity(infer, result);
    if (infer->error.return_code != NECRO_SUCCESS)
    {
        printf("\n");
        printf("====================================================\n");
        printf("Type error:\n    %s\n", infer->error.error_message);
        // printf("during inference for ast:\n");
        // necro_print_reified_ast_node(ast, infer->intern);
        printf("====================================================\n");
        printf("\n");
        return NULL;
    }
    return result;
}

NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroNode* ast, const char* error_message, ...)
{
    if (infer->error.return_code != NECRO_SUCCESS)
        return NULL;
    va_list args;
	va_start(args, error_message);

    size_t count1 = necro_verror(&infer->error, ast->source_loc, error_message, args);
    if (type != NULL)
    {
        size_t count2 = snprintf(infer->error.error_message + count1, NECRO_MAX_ERROR_MESSAGE_LENGTH, "\n    Found in type:\n        ");
        necro_snprintf_type_sig(type, infer->intern, infer->error.error_message + count1 + count2, NECRO_MAX_ERROR_MESSAGE_LENGTH);
    }

    va_end(args);

    return NULL;
}

//=====================================================
// Testing
//=====================================================
void necro_test_infer()
{
    necro_announce_phase("NecroInfer");

    NecroIntern      intern = necro_create_intern();
    NecroInfer       infer  = necro_create_infer(&intern, 0);

    // TODO / NOTE: Better error messaging!
    // TODO / NOTE: Should new_name and/or unify be setting things in the env?!?!
    // Test Var 1
    {
        NecroType* avar           = necro_new_name(&infer);
        NecroType* icon           = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
        necro_env_set(&infer, avar->var.var, icon);

        NecroNode* var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        var_node->type            = NECRO_AST_VARIABLE;
        var_node->variable.symbol = necro_intern_string(&intern, "a");
        var_node->variable.id.id  = avar->var.var.id.id;

        NecroType* result         = necro_infer(&infer, var_node);
        necro_print_type_test_result("expectEnv1", icon, "resultEnv1", result, &intern);
    }

    // Test Fexpr1
    {
        necro_reset_infer(&infer);
        NecroType* avar = necro_new_name(&infer);
        NecroType* bvar = necro_new_name(&infer);
        NecroType* icon = infer.prim_types.int_type;
        NecroType* bcon = infer.prim_types.bool_type;
        NecroType* afun = necro_create_type_fun(&infer, icon, bcon);
        necro_env_set(&infer, avar->var.var, afun);
        necro_env_set(&infer, bvar->var.var, icon);

        NecroNode* a_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        a_var_node->type            = NECRO_AST_VARIABLE;
        a_var_node->variable.symbol = necro_intern_string(&intern, "a");
        a_var_node->variable.id.id  = avar->var.var.id.id;

        NecroNode* b_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        b_var_node->type            = NECRO_AST_VARIABLE;
        b_var_node->variable.symbol = necro_intern_string(&intern, "b");
        b_var_node->variable.id.id  = bvar->var.var.id.id;

        NecroNode* f_node                    = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        f_node->type                         = NECRO_AST_FUNCTION_EXPRESSION;
        f_node->fexpression.aexp             = a_var_node;
        f_node->fexpression.next_fexpression = b_var_node;

        NecroType* result = necro_infer(&infer, f_node);
        NecroType* expect = bcon;

        necro_print_type_test_result("expectFexp1", expect, "resultFexp1", result, &intern);
        puts("");
        necro_print_env(&infer);
    }

    // Test BinOp1
    {
        necro_reset_infer(&infer);
        NecroType* avar = necro_new_name(&infer);
        NecroType* bvar = necro_new_name(&infer);
        NecroType* icon = infer.prim_types.int_type;
        necro_env_set(&infer, avar->var.var, icon);
        necro_env_set(&infer, bvar->var.var, icon);

        NecroNode* a_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        a_var_node->type            = NECRO_AST_VARIABLE;
        a_var_node->variable.symbol = necro_intern_string(&intern, "a");
        a_var_node->variable.id.id  = avar->var.var.id.id;

        NecroNode* b_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        b_var_node->type            = NECRO_AST_VARIABLE;
        b_var_node->variable.symbol = necro_intern_string(&intern, "b");
        b_var_node->variable.id.id  = bvar->var.var.id.id;

        NecroNode* bin_op_node   = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        bin_op_node->type        = NECRO_AST_BIN_OP;
        bin_op_node->bin_op.lhs  = a_var_node;
        bin_op_node->bin_op.type = NECRO_BIN_OP_ADD;
        bin_op_node->bin_op.rhs  = b_var_node;

        NecroType* result = necro_infer(&infer, bin_op_node);
        NecroType* expect = icon;

        necro_print_type_test_result("expectBinOp1", expect, "resultBinOp1", result, &intern);
        puts("");
        necro_print_env(&infer);
    }

    // Test BinOp2
    {
        necro_reset_infer(&infer);
        NecroType* icon = infer.prim_types.int_type;

        NecroNode* x_node     = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        x_node->type          = NECRO_AST_CONSTANT;
        x_node->constant.type = NECRO_AST_CONSTANT_INTEGER;

        NecroNode* y_node     = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        y_node->type          = NECRO_AST_CONSTANT;
        y_node->constant.type = NECRO_AST_CONSTANT_INTEGER;

        NecroNode* bin_op_node   = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        bin_op_node->type        = NECRO_AST_BIN_OP;
        bin_op_node->bin_op.lhs  = x_node;
        bin_op_node->bin_op.type = NECRO_BIN_OP_ADD;
        bin_op_node->bin_op.rhs  = y_node;

        NecroType* result = necro_infer(&infer, bin_op_node);
        NecroType* expect = icon;

        necro_print_type_test_result("expectBinOp2", expect, "resultBinOp2", result, &intern);
        // necro_print_env(&infer);
    }

    // Test SimpleAssignment
    {
        necro_reset_infer(&infer);
        NecroType* avar = necro_new_name(&infer);
        NecroType* bvar = necro_new_name(&infer);
        NecroType* cvar = necro_new_name(&infer);
        NecroType* bcon = infer.prim_types.bool_type;
        NecroType* bfun = necro_create_type_fun(&infer, cvar, bcon);
        necro_env_set(&infer, bvar->var.var, bfun);

        NecroNode* b_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        b_var_node->type            = NECRO_AST_VARIABLE;
        b_var_node->variable.symbol = necro_intern_string(&intern, "b");
        b_var_node->variable.id.id  = bvar->var.var.id.id;

        NecroNode* assign_node                       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
        assign_node->type                            = NECRO_AST_SIMPLE_ASSIGNMENT;
        assign_node->simple_assignment.id            = avar->var.var.id;
        assign_node->simple_assignment.variable_name = necro_intern_string(&intern, "a");
        assign_node->simple_assignment.rhs           = b_var_node;

        NecroType* result = necro_infer(&infer, assign_node);
        // result            = necro_env_get(&infer, avar->var.var);
        NecroType* expect = necro_gen(&infer, necro_create_type_fun(&infer, cvar, bcon));
        expect            = necro_rename_var_for_testing_only(&infer, (NecroVar) { 5 }, necro_create_type_var(&infer, (NecroVar) { 4 }), expect);

        necro_print_type_test_result("expectSimpleAssignment1", expect, "resultSimpleAssignment2", result, &intern);
        puts("");
        necro_print_env(&infer);
    }

}
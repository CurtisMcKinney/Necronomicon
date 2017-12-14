/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "prim.h"
#include "infer.h"

typedef NecroAST_Node_Reified NecroNode;

#define NECRO_INFER_DEBUG           0
#if NECRO_INFER_DEBUG
#define TRACE_INFER(...) printf(__VA_ARGS__)
#else
#define TRACE_INFER(...)
#endif

// Forward declarations
NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroNode* ast, const char* error_message, ...);
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer_apat(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer_pattern(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer_constant(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer_var(NecroInfer* infer, NecroNode* ast);
NecroType* necro_infer_tuple_type(NecroInfer* infer, NecroNode* ast);

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
    NECRO_AST_EXPRESSION_LIST,
    NECRO_AST_APATS_ASSIGNMENT,
    NECRO_AST_WILDCARD,
    NECRO_AST_LIST_NODE,
    NECRO_AST_LAMBDA,
    NECRO_AST_ARITHMETIC_SEQUENCE,
    NECRO_AST_CASE,
    NECRO_AST_CASE_ALTERNATIVE,

IN PROGRESS:
    NECRO_AST_TOP_DECL,
    NECRO_AST_DECL,
    Pattern
    Apat

    User supplied type sigs aren't unify-ing correctly against types
    The order of quantified ty_vars is getting clobbered or isn't mattering.
    NECRO_AST_TYPE_SIGNATURE,
    NECRO_AST_FUNCTION_TYPE,
    NECRO_AST_CONID
    NECRO_AST_TYPE_APP,

TODO:
    MULTIPLE assignments with same name!

    NECRO_AST_DO, (Think we need type classes to check this??)
    NECRO_BIND_ASSIGNMENT,

    NECRO_AST_BIN_OP_SYM,
    NECRO_AST_CONSTRUCTOR,
    NECRO_AST_SIMPLE_TYPE,
    NECRO_AST_DATA_DECLARATION,

    NECRO_AST_TYPE_CLASS_CONTEXT,
    NECRO_AST_TYPE_CLASS_DECLARATION,
    NECRO_AST_TYPE_CLASS_INSTANCE,

    // NECRO_AST_MODULE,
*/

//=====================================================
// TypeSig
//=====================================================
NecroType* necro_ast_to_type_sig_go(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    switch (ast->type)
    {
    // TODO: List and Tuple
    case NECRO_AST_VARIABLE: return necro_create_type_var(infer, (NecroVar) { ast->variable.id });
    case NECRO_AST_CONID:    return necro_create_type_con(infer, (NecroCon) { .symbol = ast->conid.symbol, .id = ast->conid.id }, NULL, 0);
    case NECRO_AST_TUPLE:    return necro_infer_tuple_type(infer, ast);
    case NECRO_AST_TYPE_APP:
    {
        NecroType* left  = necro_ast_to_type_sig_go(infer, ast->type_app.ty);
        NecroType* right = necro_ast_to_type_sig_go(infer, ast->type_app.next_ty);
        if (left->type == NECRO_TYPE_CON)
        {
            if (left->con.args == NULL)
            {
                left->con.args   = necro_create_type_list(infer, right, NULL);
                left->con.arity += 1;
            }
            else if (left->con.args != NULL)
            {
                NecroType* current_arg = left->con.args;
                while (current_arg->list.next != NULL)
                    current_arg = current_arg->list.next;
                current_arg->list.next = necro_create_type_list(infer, right, NULL);
                left->con.arity += 1;
            }
            return left;
        }
        else
        {
            return necro_create_type_app(infer, left, right);
        }
    }
    case NECRO_AST_FUNCTION_TYPE: return necro_create_type_fun(infer, necro_ast_to_type_sig_go(infer, ast->function_type.type), necro_ast_to_type_sig_go(infer, ast->function_type.next_on_arrow));
    default:                      return necro_infer_ast_error(infer, NULL, ast, "Unimplemented type signature case: %d", ast->type);
    }
}

NecroType* necro_ast_to_type_sig(NecroInfer* infer, NecroNode* ast)
{
    NecroType* type_sig = necro_gen(infer, necro_ast_to_type_sig_go(infer, ast), ast->scope);
    if (necro_is_infer_error(infer)) return NULL;
    necro_check_type_sanity(infer, type_sig);
    if (necro_is_infer_error(infer)) return NULL;
    // printf("ast to type_sig: ");
    // necro_print_type_sig(type_sig, infer->intern);
    return type_sig;
}

NecroType* necro_infer_type_sig(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_SIGNATURE);
    NecroType* type = necro_ast_to_type_sig(infer, ast->type_signature.type);
    if (type == NULL || necro_is_infer_error(infer)) return NULL;
    type->pre_supplied = true;
    necro_symtable_get(infer->symtable, ast->type_signature.var->variable.id)->type = type;
    return NULL;
}

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
    case NECRO_AST_CONSTANT_FLOAT:   return necro_symtable_get(infer->symtable, infer->prim_types.float_type.id)->type;
    case NECRO_AST_CONSTANT_INTEGER: return necro_symtable_get(infer->symtable, infer->prim_types.int_type.id)->type;
    case NECRO_AST_CONSTANT_BOOL:    return necro_symtable_get(infer->symtable, infer->prim_types.bool_type.id)->type;
    case NECRO_AST_CONSTANT_CHAR:    return necro_symtable_get(infer->symtable, infer->prim_types.char_type.id)->type;
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
    NecroType* var_type = infer->symtable->data[ast->variable.id.id].type;
    if (var_type == NULL)
    {
        var_type = necro_new_name(infer);
        var_type->var.var.scope = infer->symtable->data[ast->variable.id.id].scope;
        infer->symtable->data[ast->variable.id.id].type = var_type;
        return var_type;
    }
    else
    {
        return necro_inst(infer, var_type, ast->scope);
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
// Tuple
//=====================================================
NecroType* necro_infer_tuple(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        if (tuple_types == NULL)
        {
            tuple_types = necro_create_type_list(infer, necro_infer_go(infer, current_expression->list.item), NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_create_type_list(infer, necro_infer_go(infer, current_expression->list.item), NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    return necro_make_tuple_con(infer, types_head);
}

NecroType* necro_infer_tuple_pattern(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        if (tuple_types == NULL)
        {
            tuple_types = necro_create_type_list(infer, necro_infer_pattern(infer, current_expression->list.item), NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_create_type_list(infer, necro_infer_pattern(infer, current_expression->list.item), NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    return necro_make_tuple_con(infer, types_head);
}


NecroType* necro_infer_tuple_type(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        if (tuple_types == NULL)
        {
            tuple_types = necro_create_type_list(infer, necro_ast_to_type_sig_go(infer, current_expression->list.item), NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_create_type_list(infer, necro_ast_to_type_sig_go(infer, current_expression->list.item), NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    return necro_make_tuple_con(infer, types_head);
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
        necro_unify(infer, list_type, necro_infer_go(infer, current_cell->list.item), ast->scope);
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    return necro_make_con_1(infer, infer->prim_types.list_type.symbol, list_type);
}

NecroType* necro_infer_expression_list_pattern(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* current_cell = ast->expression_list.expressions;
    NecroType* list_type    = necro_new_name(infer);
    while (current_cell != NULL)
    {
        necro_unify(infer, list_type, necro_infer_pattern(infer, current_cell->list.item), ast->scope);
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    return necro_make_con_1(infer, infer->prim_types.list_type.symbol, list_type);
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
    necro_unify(infer, e0_type, f_type, ast->scope);
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
    necro_unify(infer, if_type, necro_symtable_get(infer->symtable, infer->prim_types.bool_type.id)->type, ast->scope);
    necro_unify(infer, then_type, else_type, ast->scope);
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
    necro_unify(infer, op_type, bin_op_type, ast->scope);
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
    if (necro_is_infer_error(infer)) return NULL;
    switch (ast->type)
    {
    case NECRO_AST_CONSTANT:        return necro_infer_constant(infer, ast);
    case NECRO_AST_VARIABLE:        return necro_infer_var(infer, ast);
    case NECRO_AST_TUPLE:           return necro_infer_tuple_pattern(infer, ast);
    case NECRO_AST_EXPRESSION_LIST: return necro_infer_expression_list_pattern(infer, ast);
    case NECRO_AST_WILDCARD:        return necro_new_name(infer);
    default:                        return necro_infer_ast_error(infer, NULL, ast, "Unimplemented pattern in type inference: %d", ast->type);
    }
}

//=====================================================
// Pattern
//=====================================================
NecroType* necro_infer_pattern(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    return necro_infer_apat(infer, ast);
}

//=====================================================
// Cast
//=====================================================
NecroType* necro_infer_case(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CASE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* expression_type = necro_infer_go(infer, ast->case_expression.expression);
    NecroType* result_type     = necro_new_name(infer);
    NecroNode* alternatives    = ast->case_expression.alternatives;
    while (alternatives != NULL)
    {
        NecroNode* alternative = alternatives->list.item;
        necro_unify(infer, expression_type, necro_infer_pattern(infer, alternative->case_alternative.pat), alternatives->scope);
        necro_unify(infer, result_type, necro_infer_go(infer, alternative->case_alternative.body), alternatives->scope);
        alternatives = alternatives->list.next_item;
    }
    return result_type;
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
    NecroNode* apats      = ast->apats_assignment.apats;
    NecroType* f_type     = NULL;
    NecroType* f_head     = NULL;
    if (necro_is_infer_error(infer)) return NULL;
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        if (necro_is_infer_error(infer)) return NULL;
        if (f_type == NULL)
        {
            f_type = necro_create_type_fun(infer, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_create_type_fun(infer, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_type = f_type->fun.type2;
        }
        apats = apats->apats.next_apat;
    }
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* proxy_type = infer->symtable->data[ast->apats_assignment.id.id].type;
    if (proxy_type->pre_supplied)
    {
        f_type->fun.type2 = necro_new_name(infer);
        necro_unify(infer, proxy_type, f_head, ast->scope);
    }
    NecroType* rhs = necro_infer_go(infer, ast->apats_assignment.rhs);
    f_type->fun.type2 = rhs;
    // if (proxy_type->pre_supplied)
    //     proxy_type = necro_inst(infer, proxy_type, infer->symtable->data[ast->apats_assignment.id.id].scope->parent);
    necro_unify(infer, proxy_type, f_head, ast->scope);
    return NULL;
}

//=====================================================
// Lambda
//=====================================================
NecroType* necro_infer_lambda(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LAMBDA);
    if (necro_is_infer_error(infer)) return NULL;
    NecroNode* apats  = ast->lambda.apats;
    NecroType* f_type = NULL;
    NecroType* f_head = NULL;
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        if (necro_is_infer_error(infer)) return NULL;
        if (f_type == NULL)
        {
            f_type = necro_create_type_fun(infer, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_create_type_fun(infer, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_type = f_type->fun.type2;
        }
        apats = apats->apats.next_apat;
    }
    if (necro_is_infer_error(infer)) return NULL;
    f_type->fun.type2 = necro_infer_go(infer, ast->lambda.expression);
    return f_head;
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
    NecroType* proxy_type = infer->symtable->data[ast->simple_assignment.id.id].type;
    NecroType* rhs_type   = necro_infer_go(infer, ast->simple_assignment.rhs);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    necro_unify(infer, proxy_type, rhs_type, ast->scope);
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
    // Pass 0, add type signatures
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        switch (current_decl->declaration.declaration_impl->type)
        {
        case NECRO_AST_TYPE_SIGNATURE: necro_infer_type_sig(infer, current_decl->declaration.declaration_impl); break;
        default: break;
        };
        current_decl = current_decl->declaration.next_declaration;
    }
    if (necro_is_infer_error(infer)) return NULL;

    //----------------------------------------------------
    // Pass 1, create new names to serve as proxy types (This is the easy solution to recursive let, but produces less general types)
    current_decl = ast;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        switch (current_decl->declaration.declaration_impl->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            if (necro_symtable_get(infer->symtable, current_decl->declaration.declaration_impl->simple_assignment.id)->type == NULL)
            {
                NecroType* new_name = necro_new_name(infer);
                new_name->var.var.scope = infer->symtable->data[current_decl->declaration.declaration_impl->simple_assignment.id.id].scope;
                infer->symtable->data[current_decl->declaration.declaration_impl->simple_assignment.id.id].type = new_name;
            }
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            if (necro_symtable_get(infer->symtable, current_decl->declaration.declaration_impl->apats_assignment.id)->type == NULL)
            {
                NecroType* new_name = necro_new_name(infer);
                new_name->var.var.scope = infer->symtable->data[current_decl->declaration.declaration_impl->apats_assignment.id.id].scope;
                infer->symtable->data[current_decl->declaration.declaration_impl->apats_assignment.id.id].type = new_name;
            }
            break;
        case NECRO_AST_TYPE_SIGNATURE: break; // Do Nothing
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->declaration.declaration_impl->type);
        }
        if (necro_is_infer_error(infer)) return NULL;
        current_decl = current_decl->declaration.next_declaration;
    }
    if (necro_is_infer_error(infer)) return NULL;

    //----------------------------------------------------
    // Pass 2, Infer types for declarations, then unify assignments with their proxy types
    current_decl = ast;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        necro_infer_go(infer, current_decl->declaration.declaration_impl);
        if (necro_is_infer_error(infer)) return NULL;
        current_decl = current_decl->declaration.next_declaration;
    }

    //----------------------------------------------------
    // Pass 3, Generalize declared types
    current_decl = ast;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        switch (current_decl->declaration.declaration_impl->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            if (!necro_symtable_get(infer->symtable, current_decl->declaration.declaration_impl->simple_assignment.id)->type->pre_supplied)
            {
                NecroVar   var        = (NecroVar) { current_decl->declaration.declaration_impl->simple_assignment.id };
                NecroType* proxy_type = infer->symtable->data[var.id.id].type;
                infer->symtable->data[var.id.id].type = necro_gen(infer, proxy_type, infer->symtable->data[var.id.id].scope->parent);
            }
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            if (!necro_symtable_get(infer->symtable, current_decl->declaration.declaration_impl->apats_assignment.id)->type->pre_supplied)
            {
                NecroVar   var        = (NecroVar) { current_decl->declaration.declaration_impl->apats_assignment.id };
                NecroType* proxy_type = infer->symtable->data[var.id.id].type;
                infer->symtable->data[var.id.id].type = necro_gen(infer, proxy_type, infer->symtable->data[var.id.id].scope->parent);
            }
            break;
        case NECRO_AST_TYPE_SIGNATURE: break; // Do Nothing
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->declaration.declaration_impl->type);
        }
        if (necro_is_infer_error(infer)) return NULL;
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
    // Pass 0, add type signatures
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        switch (current_decl->top_declaration.declaration->type)
        {
        case NECRO_AST_TYPE_SIGNATURE: necro_infer_type_sig(infer, current_decl->top_declaration.declaration); break;
        default: break;
        };
        if (necro_is_infer_error(infer)) return NULL;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return NULL;

    //----------------------------------------------------
    // Pass 1, create new names to serve as proxy types (This is the easy solution to recursive let, but produces less general types)
    current_decl = ast;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        switch (current_decl->top_declaration.declaration->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            if (necro_symtable_get(infer->symtable, current_decl->top_declaration.declaration->simple_assignment.id)->type == NULL)
            {
                NecroType* new_name = necro_new_name(infer);
                new_name->var.var.scope = infer->symtable->data[current_decl->top_declaration.declaration->simple_assignment.id.id].scope;
                infer->symtable->data[current_decl->top_declaration.declaration->simple_assignment.id.id].type = new_name;
            }
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            if (necro_symtable_get(infer->symtable, current_decl->top_declaration.declaration->apats_assignment.id)->type == NULL)
            {
                NecroType* new_name = necro_new_name(infer);
                new_name->var.var.scope = infer->symtable->data[current_decl->declaration.declaration_impl->apats_assignment.id.id].scope;
                infer->symtable->data[current_decl->declaration.declaration_impl->apats_assignment.id.id].type = new_name;
            }
            break;
        case NECRO_AST_TYPE_SIGNATURE: break; // Do Nothing
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->top_declaration.declaration->type);
        }
        if (necro_is_infer_error(infer)) return NULL;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return NULL;

    //----------------------------------------------------
    // Pass 2, Infer types for rhs of declarations
    current_decl = ast;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        necro_infer_go(infer, current_decl->declaration.declaration_impl);
        if (necro_is_infer_error(infer)) return NULL;
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (necro_is_infer_error(infer)) return NULL;

    //----------------------------------------------------
    // Pass 3, Infer types for rhs of declarations
    current_decl = ast;
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        switch (current_decl->top_declaration.declaration->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            if (!necro_symtable_get(infer->symtable, current_decl->top_declaration.declaration->simple_assignment.id)->type->pre_supplied)
            {
                NecroVar   var        = (NecroVar) { current_decl->top_declaration.declaration->simple_assignment.id };
                NecroType* proxy_type = infer->symtable->data[var.id.id].type;
                infer->symtable->data[var.id.id].type = necro_gen(infer, proxy_type, infer->symtable->data[var.id.id].scope->parent);
            }
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            if (!necro_symtable_get(infer->symtable, current_decl->top_declaration.declaration->apats_assignment.id)->type->pre_supplied)
            {
                NecroVar   var        = (NecroVar) { current_decl->top_declaration.declaration->apats_assignment.id };
                NecroType* proxy_type = infer->symtable->data[var.id.id].type;
                infer->symtable->data[var.id.id].type = necro_gen(infer, proxy_type, infer->symtable->data[var.id.id].scope->parent);
            }
            break;
        case NECRO_AST_TYPE_SIGNATURE: break; // Do Nothing
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->top_declaration.declaration->type);
        }
        if (necro_is_infer_error(infer)) return NULL;
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
// Arithmetic Sequence
//=====================================================
NecroType* necro_infer_arithmetic_sequence(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* type = necro_symtable_get(infer->symtable, infer->prim_types.int_type.id)->type;
    if (ast->arithmetic_sequence.from != NULL)
        necro_unify(infer, type, necro_infer_go(infer, ast->arithmetic_sequence.from), ast->scope);
    if (ast->arithmetic_sequence.then != NULL)
        necro_unify(infer, type, necro_infer_go(infer, ast->arithmetic_sequence.then), ast->scope);
    if (ast->arithmetic_sequence.to != NULL)
        necro_unify(infer, type, necro_infer_go(infer, ast->arithmetic_sequence.to), ast->scope);
    return necro_make_con_1(infer, infer->prim_types.list_type.symbol, type);
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
    case NECRO_AST_APATS_ASSIGNMENT:    return necro_infer_apats_assignment(infer, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:     return necro_infer_right_hand_side(infer, ast);
    case NECRO_AST_LAMBDA:              return necro_infer_lambda(infer, ast);
    case NECRO_AST_LET_EXPRESSION:      return necro_infer_let_expression(infer, ast);
    case NECRO_AST_DECL:                return necro_infer_declaration(infer, ast);
    case NECRO_AST_TOP_DECL:            return necro_infer_top_declaration(infer, ast);
    case NECRO_AST_TUPLE:               return necro_infer_tuple(infer, ast);
    case NECRO_AST_EXPRESSION_LIST:     return necro_infer_expression_list(infer, ast);
    case NECRO_AST_CASE:                return necro_infer_case(infer, ast);
    case NECRO_AST_WILDCARD:            return necro_infer_wildcard(infer, ast);
    case NECRO_AST_ARITHMETIC_SEQUENCE: return necro_infer_arithmetic_sequence(infer, ast);
    // case NECRO_AST_TYPE_SIGNATURE:      return necro_infer_type_sig(infer, ast);
    case NECRO_AST_TYPE_SIGNATURE:      return NULL;
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
        return NULL;
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

    // NecroIntern    intern     = necro_create_intern();
    // NecroPrimTypes prim_types = necro_create_prim_types(&intern);
    // NecroInfer     infer      = necro_create_infer(&intern, 0, prim_types);

    // // Test Var 1
    // {
    //     NecroType* avar           = necro_new_name(&infer);
    //     NecroType* icon           = necro_create_type_con(&infer, (NecroCon) { necro_intern_string(&intern, "Int"), { 0 } }, NULL, 0);
    //     necro_env_set(&infer, avar->var.var, icon);

    //     NecroNode* var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     var_node->type            = NECRO_AST_VARIABLE;
    //     var_node->variable.symbol = necro_intern_string(&intern, "a");
    //     var_node->variable.id.id  = avar->var.var.id.id;

    //     NecroType* result         = necro_infer(&infer, var_node);
    //     necro_print_type_test_result("expectEnv1", icon, "resultEnv1", result, &intern);
    // }

    // // Test Fexpr1
    // {
    //     necro_reset_infer(&infer);
    //     NecroType* avar = necro_new_name(&infer);
    //     NecroType* bvar = necro_new_name(&infer);
    //     NecroType* icon = infer.prim_types.int_type;
    //     NecroType* bcon = infer.prim_types.bool_type;
    //     NecroType* afun = necro_create_type_fun(&infer, icon, bcon);
    //     necro_env_set(&infer, avar->var.var, afun);
    //     necro_env_set(&infer, bvar->var.var, icon);

    //     NecroNode* a_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     a_var_node->type            = NECRO_AST_VARIABLE;
    //     a_var_node->variable.symbol = necro_intern_string(&intern, "a");
    //     a_var_node->variable.id.id  = avar->var.var.id.id;

    //     NecroNode* b_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     b_var_node->type            = NECRO_AST_VARIABLE;
    //     b_var_node->variable.symbol = necro_intern_string(&intern, "b");
    //     b_var_node->variable.id.id  = bvar->var.var.id.id;

    //     NecroNode* f_node                    = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     f_node->type                         = NECRO_AST_FUNCTION_EXPRESSION;
    //     f_node->fexpression.aexp             = a_var_node;
    //     f_node->fexpression.next_fexpression = b_var_node;

    //     NecroType* result = necro_infer(&infer, f_node);
    //     NecroType* expect = bcon;

    //     necro_print_type_test_result("expectFexp1", expect, "resultFexp1", result, &intern);
    //     puts("");
    //     necro_print_env(&infer);
    // }

    // // Test BinOp1
    // {
    //     necro_reset_infer(&infer);
    //     NecroType* avar = necro_new_name(&infer);
    //     NecroType* bvar = necro_new_name(&infer);
    //     NecroType* icon = infer.prim_types.int_type;
    //     necro_env_set(&infer, avar->var.var, icon);
    //     necro_env_set(&infer, bvar->var.var, icon);

    //     NecroNode* a_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     a_var_node->type            = NECRO_AST_VARIABLE;
    //     a_var_node->variable.symbol = necro_intern_string(&intern, "a");
    //     a_var_node->variable.id.id  = avar->var.var.id.id;

    //     NecroNode* b_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     b_var_node->type            = NECRO_AST_VARIABLE;
    //     b_var_node->variable.symbol = necro_intern_string(&intern, "b");
    //     b_var_node->variable.id.id  = bvar->var.var.id.id;

    //     NecroNode* bin_op_node   = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     bin_op_node->type        = NECRO_AST_BIN_OP;
    //     bin_op_node->bin_op.lhs  = a_var_node;
    //     bin_op_node->bin_op.type = NECRO_BIN_OP_ADD;
    //     bin_op_node->bin_op.rhs  = b_var_node;

    //     NecroType* result = necro_infer(&infer, bin_op_node);
    //     NecroType* expect = icon;

    //     necro_print_type_test_result("expectBinOp1", expect, "resultBinOp1", result, &intern);
    //     puts("");
    //     necro_print_env(&infer);
    // }

    // // Test BinOp2
    // {
    //     necro_reset_infer(&infer);
    //     NecroType* icon = infer.prim_types.int_type;

    //     NecroNode* x_node     = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     x_node->type          = NECRO_AST_CONSTANT;
    //     x_node->constant.type = NECRO_AST_CONSTANT_INTEGER;

    //     NecroNode* y_node     = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     y_node->type          = NECRO_AST_CONSTANT;
    //     y_node->constant.type = NECRO_AST_CONSTANT_INTEGER;

    //     NecroNode* bin_op_node   = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     bin_op_node->type        = NECRO_AST_BIN_OP;
    //     bin_op_node->bin_op.lhs  = x_node;
    //     bin_op_node->bin_op.type = NECRO_BIN_OP_ADD;
    //     bin_op_node->bin_op.rhs  = y_node;

    //     NecroType* result = necro_infer(&infer, bin_op_node);
    //     NecroType* expect = icon;

    //     necro_print_type_test_result("expectBinOp2", expect, "resultBinOp2", result, &intern);
    //     // necro_print_env(&infer);
    // }

    // // Test SimpleAssignment
    // {
    //     necro_reset_infer(&infer);
    //     NecroType* avar = necro_new_name(&infer);
    //     NecroType* bvar = necro_new_name(&infer);
    //     NecroType* cvar = necro_new_name(&infer);
    //     NecroType* bcon = infer.prim_types.bool_type;
    //     NecroType* bfun = necro_create_type_fun(&infer, cvar, bcon);
    //     necro_env_set(&infer, bvar->var.var, bfun);

    //     NecroNode* b_var_node       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     b_var_node->type            = NECRO_AST_VARIABLE;
    //     b_var_node->variable.symbol = necro_intern_string(&intern, "b");
    //     b_var_node->variable.id.id  = bvar->var.var.id.id;

    //     NecroNode* assign_node                       = necro_paged_arena_alloc(&infer.arena, sizeof(NecroNode));
    //     assign_node->type                            = NECRO_AST_SIMPLE_ASSIGNMENT;
    //     assign_node->simple_assignment.id            = avar->var.var.id;
    //     assign_node->simple_assignment.variable_name = necro_intern_string(&intern, "a");
    //     assign_node->simple_assignment.rhs           = b_var_node;

    //     NecroType* result = necro_infer(&infer, assign_node);
    //     // result            = necro_env_get(&infer, avar->var.var);
    //     NecroType* expect = necro_gen(&infer, necro_create_type_fun(&infer, cvar, bcon));
    //     expect            = necro_rename_var_for_testing_only(&infer, (NecroVar) { 5 }, necro_create_type_var(&infer, (NecroVar) { 4 }), expect);

    //     necro_print_type_test_result("expectSimpleAssignment1", expect, "resultSimpleAssignment2", result, &intern);
    //     puts("");
    //     necro_print_env(&infer);
    // }

}
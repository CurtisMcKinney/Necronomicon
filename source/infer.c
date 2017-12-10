/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"

typedef NecroAST_Node_Reified NecroNode;

NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroNode* ast, const char* error_message, ...);
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast);

// static const NecroInferResult null_infer_result = { NULL, NULL };
// /*
// typedef enum
// {
//     NECRO_AST_UNDEFINED,
//     NECRO_AST_CONSTANT,
//     NECRO_AST_UN_OP,
//     NECRO_AST_BIN_OP,
//     NECRO_AST_IF_THEN_ELSE,
//     NECRO_AST_TOP_DECL,
//     NECRO_AST_DECL,
//     NECRO_AST_SIMPLE_ASSIGNMENT,
//     NECRO_AST_APATS_ASSIGNMENT,
//     NECRO_AST_RIGHT_HAND_SIDE,
//     NECRO_AST_LET_EXPRESSION,
//     NECRO_AST_FUNCTION_EXPRESSION,
//     NECRO_AST_VARIABLE,
//     NECRO_AST_APATS,
//     NECRO_AST_WILDCARD,
//     NECRO_AST_LAMBDA,
//     NECRO_AST_DO,
//     NECRO_AST_LIST_NODE,
//     NECRO_AST_EXPRESSION_LIST,
//     NECRO_AST_TUPLE,
//     NECRO_BIND_ASSIGNMENT,
//     NECRO_AST_ARITHMETIC_SEQUENCE,
//     NECRO_AST_CASE,
//     NECRO_AST_CASE_ALTERNATIVE,
//     NECRO_AST_CONID,
//     NECRO_AST_TYPE_APP,
//     NECRO_AST_BIN_OP_SYM,
//     NECRO_AST_CONSTRUCTOR,
//     NECRO_AST_SIMPLE_TYPE,
//     NECRO_AST_DATA_DECLARATION,
//     NECRO_AST_TYPE_CLASS_CONTEXT,
//     NECRO_AST_TYPE_CLASS_DECLARATION,
//     NECRO_AST_TYPE_CLASS_INSTANCE,
//     NECRO_AST_TYPE_SIGNATURE,
//     NECRO_AST_FUNCTION_TYPE,
//     // NECRO_AST_MODULE,
// } NecroAST_NodeType;
// */

//=====================================================
// Constant
//=====================================================
NecroType* necro_infer_constant(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONSTANT);
    if (infer->error.return_code != NECRO_SUCCESS)
        return NULL;
    switch (ast->constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:   return necro_make_float_type(infer);
    case NECRO_AST_CONSTANT_INTEGER: return necro_make_int_type(infer);
    case NECRO_AST_CONSTANT_BOOL:    return necro_make_bool_type(infer);
    case NECRO_AST_CONSTANT_CHAR:    return necro_make_char_type(infer);
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
    if (infer->error.return_code != NECRO_SUCCESS)
        return NULL;
    NecroType* type = necro_env_get(infer, (NecroVar) { ast->variable.id });
    if (type == NULL)
        return necro_infer_ast_error(infer, NULL, ast, "Type not found for variable: %s", necro_intern_get_string(infer->intern, ast->variable.symbol));
    return necro_inst(infer, type);
}

//=====================================================
// Function Expression
//=====================================================
NecroType* necro_infer_fexpr(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    if (infer->error.return_code != NECRO_SUCCESS)
        return NULL;
    NecroType* e0_type     = necro_infer_go(infer, ast->fexpression.aexp);
    NecroType* e1_type     = necro_infer_go(infer, ast->fexpression.next_fexpression);
    if (infer->error.return_code != NECRO_SUCCESS)
        return NULL;
    NecroType* result_type = necro_new_name(infer);
    NecroType* f_type      = necro_create_type_fun(infer, e1_type, result_type);
    necro_unify(infer, e0_type, f_type);
    return result_type;
}

//=====================================================
// BinOp
//=====================================================
NecroType* necro_infer_bin_op(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_BIN_OP);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    NecroType* x_type      = necro_infer_go(infer, ast->bin_op.lhs);
    NecroType* op_type     = necro_make_bin_op_type(infer, ast->bin_op.type);
    NecroType* y_type      = necro_infer_go(infer, ast->bin_op.rhs);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    NecroType* result_type = necro_new_name(infer);
    NecroType* bin_op_type = necro_create_type_fun(infer, x_type, necro_create_type_fun(infer, y_type, result_type));
    necro_unify(infer, op_type, bin_op_type);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    return result_type;
}

//=====================================================
// Simple Assignment
//=====================================================
NecroType* necro_infer_simple_assignment(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    NecroID    varid        = ast->simple_assignment.id;
    NecroType* rhs_type     = necro_infer_go(infer, ast->simple_assignment.rhs);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    NecroType* gen_rhs_type = necro_gen(infer, rhs_type);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    necro_env_set(infer, (NecroVar) { varid.id }, gen_rhs_type);
    // Assignments return type of rhs
    return gen_rhs_type;
}

//=====================================================
// RightHandSide
//=====================================================
NecroType* necro_infer_right_hand_side(NecroInfer* infer, NecroNode* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    necro_infer_go(infer, ast->right_hand_side.declarations);
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
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
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    NecroNode* current_decl = ast;
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
        // TODO: Assign unique IDs to apats as well during renaming
        // case NECRO_AST_APATS_ASSIGNMENT:
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->declaration.declaration_impl->type);
        }
        current_decl = current_decl->declaration.next_declaration;
    }
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    current_decl = ast;
    // Pass 2, Infer types for declarations, then unify assignments with their proxy types
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_DECL);
        switch (current_decl->declaration.declaration_impl->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            NecroType* proxy_type = necro_env_get(infer, (NecroVar){current_decl->declaration.declaration_impl->simple_assignment.id});
            NecroType* rhs_type   = necro_infer_go(infer, current_decl->declaration.declaration_impl->simple_assignment.rhs);
            if (infer->error.return_code != NECRO_SUCCESS) return NULL;
            necro_unify(infer, proxy_type, rhs_type);
            *proxy_type = *necro_gen(infer, proxy_type);
            break;
        }
        // TODO: Assign unique IDs to apats as well during renaming
        // case NECRO_AST_APATS_ASSIGNMENT:
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->declaration.declaration_impl->type);
        }
        current_decl = current_decl->declaration.next_declaration;
    }
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
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    NecroNode* current_decl = ast;
    // Pass 1, create new names to serve as proxy types (This is the easy solution to recursive let, but produces less general types)
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        switch (current_decl->top_declaration.declaration->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            NecroType* new_name = necro_new_name(infer);
            necro_env_set(infer, (NecroVar) { current_decl->top_declaration.declaration->simple_assignment.id }, new_name);
            break;
        }
        // TODO: Assign unique IDs to apats as well during renaming
        // case NECRO_AST_APATS_ASSIGNMENT:
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->top_declaration.declaration->type);
        }
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    current_decl = ast;
    // Pass 2, Infer types for declarations, then unify assignments with their proxy types
    while (current_decl != NULL)
    {
        assert(current_decl->type == NECRO_AST_TOP_DECL);
        switch (current_decl->top_declaration.declaration->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            NecroType* proxy_type = necro_env_get(infer, (NecroVar){current_decl->top_declaration.declaration->simple_assignment.id});
            NecroType* rhs_type   = necro_infer_go(infer, current_decl->declaration.declaration_impl->simple_assignment.rhs);
            if (infer->error.return_code != NECRO_SUCCESS) return NULL;
            necro_unify(infer, proxy_type, rhs_type);
            // *proxy_type = *necro_gen(infer, proxy_type);
            break;
        }
        // TODO: Assign unique IDs to apats as well during renaming
        // case NECRO_AST_APATS_ASSIGNMENT:
        default: return necro_infer_ast_error(infer, NULL, ast, "Compiler bug: Unknown or unimplemented declaration type: %d", current_decl->top_declaration.declaration->type);
        }
        current_decl = current_decl->top_declaration.next_top_decl;
    }
    // Declarations themselves have no types
    return NULL;
}

//=====================================================
// Let
//=====================================================
// // TODO: Recursive Let bindings
// inline NecroType* necro_infer_let(NecroInfer* infer, NecroNode* ast)
// {
//     assert(infer != NULL);
//     assert(ast != NULL);
//     assert(ast->type == NECRO_AST_BIN_OP);
//     if (infer->error.return_code != NECRO_SUCCESS) return NULL;
// }

//=====================================================
// Infer Go
//=====================================================
NecroType* necro_infer_go(NecroInfer* infer, NecroNode* ast)
{
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
    case NECRO_AST_SIMPLE_ASSIGNMENT:   return necro_infer_simple_assignment(infer, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:     return necro_infer_right_hand_side(infer, ast);
    case NECRO_AST_DECL:                return necro_infer_declaration(infer, ast);
    case NECRO_AST_TOP_DECL:            return necro_infer_top_declaration(infer, ast);
    // case NECRO_AST_LET_EXPRESSION: return necro_infer_let(infer, gamma, ast);

    default:                            return necro_infer_ast_error(infer, NULL, ast, "AST type %d has not been implemented for type inference", ast->type);
    }
    return NULL;
}

NecroType* necro_infer(NecroInfer* infer, NecroNode* ast)
{
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
    NecroInfer       infer  = necro_create_infer(&intern);

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
        NecroType* icon = necro_make_int_type(&infer);
        NecroType* bcon = necro_make_bool_type(&infer);
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
        NecroType* icon = necro_make_int_type(&infer);
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
        NecroType* icon = necro_make_int_type(&infer);

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
        NecroType* bcon = necro_make_bool_type(&infer);
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
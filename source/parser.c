/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "parser.h"

//#define PARSE_DEBUG_PRINT 1

// =====================================================
// Abstract Syntax Tree
// =====================================================

NecroAST_Node* ast_alloc_node(NecroAST* ast)
{
    return (NecroAST_Node*) arena_alloc(&ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

NecroAST_Node* ast_alloc_node_local_ptr(NecroAST* ast, NecroAST_LocalPtr* local_ptr)
{
    *local_ptr = ast->arena.size / sizeof(NecroAST_Node);
    return (NecroAST_Node*) arena_alloc(&ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

NecroAST_LocalPtr ast_last_node_ptr(NecroAST* ast)
{
    NecroAST_LocalPtr local_ptr = null_local_ptr;
    if (ast->arena.size > 0)
    {
        return (ast->arena.size / sizeof(NecroAST_Node)) - 1;
    }
    return local_ptr;
}

// used to back track AST modifications during parsing
void ast_pop_node(NecroAST* ast)
{
    assert(ast->arena.size >= sizeof(NecroAST_Node));
    ast->arena.size -= sizeof(NecroAST_Node);
}

void print_ast_impl(NecroAST* ast, NecroAST_Node* ast_node, uint32_t depth)
{
    for (uint32_t i = 0;  i < depth; ++i)
    {
        printf("\t");
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
        case NECRO_BIN_OP_AND:
            puts("(&&)");
            break;
        case NECRO_BIN_OP_OR:
            puts("(||)");
            break;
        default:
            puts("(Undefined Binary Operator)");
            break;
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.lhs), depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.rhs), depth + 1);
        break;

    case NECRO_AST_CONSTANT:
        switch(ast_node->constant.type)
        {
        case NECRO_AST_CONSTANT_FLOAT:
            printf("(%f)\n", ast_node->constant.double_literal);
            break;
        case NECRO_AST_CONSTANT_INTEGER:
            printf("(%lli)\n", ast_node->constant.int_literal);
            break;
        case NECRO_AST_CONSTANT_STRING:
            printf("(%s)\n", ast_node->constant.str.data);
            break;
        case NECRO_AST_CONSTANT_BOOL:
            printf("(%i)\n", ast_node->constant.boolean_literal);
            break;
        }
        break;
    default:
        puts("(Undefined)");
        break;
    }
}

void print_ast(NecroAST* ast)
{
    print_ast_impl(ast, ast_get_root_node(ast), 0);
}

// =====================================================
// Recursive Descent Parser
// =====================================================

static inline bool can_continue_parse(NecroLexToken** tokens, NecroAST_LocalPtr last_ast_ptr)
{
    return (last_ast_ptr != null_local_ptr) && ((*tokens)->token != NECRO_LEX_END_OF_STREAM);
}

NecroAST_LocalPtr parse_expression(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_constant(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_unary_operation(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_binary_operation(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_function_composition(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_end_of_stream(NecroLexToken** tokens, NecroAST* ast);

NecroParse_Result parse_ast(NecroLexToken** tokens, NecroAST* ast)
{
    NecroAST_LocalPtr local_ptr = parse_expression(tokens, ast);
#ifdef PARSE_DEBUG_PRINT
    printf(
        " parse_ast result { tokens**: %p, tokens*: %p, token: %s , ast: %p }\n",
        tokens,
        *tokens,
        necro_lex_token_type_string((*tokens)->token),
        ast);
#endif // PARSE_DEBUG_PRINT
    if ((local_ptr != null_local_ptr) &&
        ((*tokens)->token ==  NECRO_LEX_END_OF_STREAM) || (*tokens)->token ==  NECRO_LEX_SEMI_COLON)
    {
        return ParseSuccessful;
    }

    return ParseError;
}

NecroAST_LocalPtr parse_expression(NecroLexToken** tokens, NecroAST* ast)
{
#ifdef PARSE_DEBUG_PRINT
    printf(
        " parse_expression { tokens**: %p, tokens*: %p, token: %s , ast: %p }\n",
        tokens,
        *tokens,
        necro_lex_token_type_string((*tokens)->token),
        ast);
#endif // PARSE_DEBUG_PRINT

    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    NecroAST_LocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_constant(tokens, ast);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_function_composition(tokens, ast);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_binary_operation(tokens, ast);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_unary_operation(tokens, ast);
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

NecroAST_LocalPtr parse_constant(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    NecroAST_LocalPtr local_ptr = null_local_ptr;
    switch(tokens[0]->token)
    {
    case NECRO_LEX_FLOAT_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_FLOAT;
            constant.double_literal = tokens[0]->double_literal;
            ast_node->constant = constant;
            ++(*tokens);
#ifdef PARSE_DEBUG_PRINT
    printf(" parse_expression NECRO_LEX_FLOAT_LITERAL { ast_node: %p, local_ptr: %u }\n", ast_node, local_ptr);
#endif // PARSE_DEBUG_PRINT
            return local_ptr;
        }
    case NECRO_LEX_INTEGER_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_INTEGER;
            constant.int_literal = tokens[0]->int_literal;
            ast_node->constant = constant;
            ++(*tokens);
#ifdef PARSE_DEBUG_PRINT
    printf(" parse_expression NECRO_LEX_INTEGER_LITERAL { ast_node: %p, local_ptr: %u }\n", ast_node, local_ptr);
#endif // PARSE_DEBUG_PRINT
            return local_ptr;
        }
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

NecroAST_LocalPtr parse_unary_operation(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

NecroAST_LocalPtr parse_binary_operation(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    NecroAST_LocalPtr bin_op_ptr = null_local_ptr;
    NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &bin_op_ptr);
    ast_node->type = NECRO_AST_BIN_OP;
    ast_node->bin_op.lhs = parse_expression(tokens, ast);
    bool success = can_continue_parse(tokens, ast_node->bin_op.lhs);
    if (success)
    {
        NecroAST_BinOpType bin_op_type = NECRO_BIN_OP_ADD;
        switch((*tokens)->token)
        {
        case NECRO_LEX_ADD:
            bin_op_type = NECRO_BIN_OP_ADD;
            break;
        case NECRO_LEX_SUB:
            bin_op_type = NECRO_BIN_OP_SUB;
            break;
        case NECRO_LEX_MUL:
            bin_op_type = NECRO_BIN_OP_MUL;
            break;
        case NECRO_LEX_DIV:
            bin_op_type = NECRO_BIN_OP_DIV;
            break;
        case NECRO_LEX_MOD:
            bin_op_type = NECRO_BIN_OP_MOD;
            break;
        case NECRO_LEX_GT:
            bin_op_type = NECRO_BIN_OP_GT;
            break;
        case NECRO_LEX_LT:
            bin_op_type = NECRO_BIN_OP_LT;
            break;
        case NECRO_LEX_GTE:
            bin_op_type = NECRO_BIN_OP_GTE;
            break;
        case NECRO_LEX_LTE:
            bin_op_type = NECRO_BIN_OP_LTE;
            break;
        case NECRO_LEX_DOUBLE_COLON:
            bin_op_type = NECRO_BIN_OP_DOUBLE_COLON;
            break;
        case NECRO_LEX_LEFT_SHIFT:
            bin_op_type = NECRO_BIN_OP_LEFT_SHIFT;
            break;
        case NECRO_LEX_RIGHT_SHIFT:
            bin_op_type = NECRO_BIN_OP_RIGHT_SHIFT;
            break;
        case NECRO_LEX_PIPE:
            bin_op_type = NECRO_BIN_OP_PIPE;
            break;
        case NECRO_LEX_FORWARD_PIPE:
            bin_op_type = NECRO_BIN_OP_FORWARD_PIPE;
            break;
        case NECRO_LEX_BACK_PIPE:
            bin_op_type = NECRO_BIN_OP_BACK_PIPE;
            break;
        case NECRO_LEX_EQUALS:
            bin_op_type = NECRO_BIN_OP_EQUALS;
            break;
        case NECRO_LEX_AND:
            bin_op_type = NECRO_BIN_OP_AND;
            break;
        case NECRO_LEX_OR:
            bin_op_type = NECRO_BIN_OP_OR;
            break;
        default:
            success = false;
            break;
        }

        if (success)
        {
            ast_node->bin_op.type = bin_op_type;
            ++(*tokens);
        }
    }

    if (success)
    {
        ast_node->bin_op.rhs = parse_expression(tokens, ast);
        if (ast_node->bin_op.rhs != null_local_ptr)
        {
            return bin_op_ptr;
        }
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

NecroAST_LocalPtr parse_function_composition(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

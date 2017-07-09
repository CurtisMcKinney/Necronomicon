/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include "intern.h"
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

void print_ast_impl(NecroAST* ast, NecroAST_Node* ast_node, NecroIntern* intern, uint32_t depth)
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
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.lhs), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.rhs), intern, depth + 1);
        break;

    case NECRO_AST_CONSTANT:
        switch(ast_node->constant.type)
        {
        case NECRO_AST_CONSTANT_FLOAT:
            printf("(%f)\n", ast_node->constant.double_literal);
            break;
        case NECRO_AST_CONSTANT_INTEGER:
            printf("(%li)\n", ast_node->constant.int_literal);
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
        print_ast_impl(ast, ast_get_node(ast, ast_node->if_then_else.if_expr), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->if_then_else.then_expr), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->if_then_else.else_expr), intern, depth + 1);
        break;
    default:
        puts("(Undefined)");
        break;
    }
}

void print_ast(NecroAST* ast, NecroIntern* intern, NecroAST_LocalPtr root_node_ptr)
{
    print_ast_impl(ast, ast_get_node(ast, root_node_ptr), intern, 0);
}

// =====================================================
// Recursive Descent Parser
// =====================================================

typedef enum
{
    NECRO_NO_BINARY_LOOK_AHEAD,
    NECRO_BINARY_LOOK_AHEAD,
} NecroParser_LookAheadBinary;

NecroAST_LocalPtr parse_expression(NecroLexToken** tokens, NecroAST* ast, NecroParser_LookAheadBinary look_ahead_binary);
NecroAST_LocalPtr parse_parenthetical_expression(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_l_expression(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_if_then_else_expression(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_constant(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_unary_operation(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_binary_operation(NecroLexToken** tokens, NecroAST* ast, NecroAST_LocalPtr lhs_local_ptr);
NecroAST_LocalPtr parse_function_composition(NecroLexToken** tokens, NecroAST* ast);
NecroAST_LocalPtr parse_end_of_stream(NecroLexToken** tokens, NecroAST* ast);

NecroParse_Result parse_ast(NecroLexToken** tokens, NecroAST* ast, NecroAST_LocalPtr* out_root_node_ptr)
{
    NecroAST_LocalPtr local_ptr = parse_expression(tokens, ast, NECRO_BINARY_LOOK_AHEAD);
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
        *out_root_node_ptr = local_ptr;
        return ParseSuccessful;
    }

    *out_root_node_ptr = null_local_ptr;
    return ParseError;
}

NecroAST_LocalPtr parse_expression(NecroLexToken** tokens, NecroAST* ast, NecroParser_LookAheadBinary look_ahead_binary)
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
        local_ptr = parse_parenthetical_expression(tokens, ast);
    }

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
        local_ptr = parse_unary_operation(tokens, ast);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_l_expression(tokens, ast);
    }

    if ((look_ahead_binary == NECRO_BINARY_LOOK_AHEAD) && (local_ptr != null_local_ptr)) // Try parsing expression as a binary operation expression
    {
        NecroAST_LocalPtr bin_op_local_ptr = parse_binary_operation(tokens, ast, local_ptr);
        if (bin_op_local_ptr != null_local_ptr)
            local_ptr = bin_op_local_ptr;
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

    case NECRO_LEX_STRING_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_STRING;
            constant.symbol = tokens[0]->symbol;
            ast_node->constant = constant;
            ++(*tokens);
            return local_ptr;
        }
    case NECRO_LEX_CHAR_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_CHAR;
            constant.char_literal = tokens[0]->char_literal;
            ast_node->constant = constant;
            ++(*tokens);
            return local_ptr;
        }
    case NECRO_AST_CONSTANT_BOOL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_BOOL;
            constant.boolean_literal = tokens[0]->boolean_literal;
            ast_node->constant = constant;
            ++(*tokens);
            return local_ptr;
        }
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

NecroAST_LocalPtr parse_parenthetical_expression(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;

    if ((*tokens)->token == NECRO_LEX_LEFT_PAREN)
    {
        ++(*tokens);
        NecroAST_LocalPtr local_ptr = parse_expression(tokens, ast, NECRO_BINARY_LOOK_AHEAD);
        if (local_ptr != null_local_ptr && (*tokens)->token == NECRO_LEX_RIGHT_PAREN)
        {
            ++(*tokens);
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

NecroAST_BinOpType token_to_bin_op_type(NECRO_LEX_TOKEN_TYPE token_type)
{
    switch(token_type)
    {
    case NECRO_LEX_ADD:
        return NECRO_BIN_OP_ADD;
    case NECRO_LEX_SUB:
        return NECRO_BIN_OP_SUB;
    case NECRO_LEX_MUL:
        return NECRO_BIN_OP_MUL;
    case NECRO_LEX_DIV:
        return NECRO_BIN_OP_DIV;
    case NECRO_LEX_MOD:
        return NECRO_BIN_OP_MOD;
    case NECRO_LEX_GT:
        return NECRO_BIN_OP_GT;
    case NECRO_LEX_LT:
        return NECRO_BIN_OP_LT;
    case NECRO_LEX_GTE:
        return NECRO_BIN_OP_GTE;
    case NECRO_LEX_LTE:
        return NECRO_BIN_OP_LTE;
    case NECRO_LEX_DOUBLE_COLON:
        return NECRO_BIN_OP_DOUBLE_COLON;
    case NECRO_LEX_LEFT_SHIFT:
        return NECRO_BIN_OP_LEFT_SHIFT;
    case NECRO_LEX_RIGHT_SHIFT:
        return NECRO_BIN_OP_RIGHT_SHIFT;
    case NECRO_LEX_PIPE:
        return NECRO_BIN_OP_PIPE;
    case NECRO_LEX_FORWARD_PIPE:
        return NECRO_BIN_OP_FORWARD_PIPE;
    case NECRO_LEX_BACK_PIPE:
        return NECRO_BIN_OP_BACK_PIPE;
    case NECRO_LEX_EQUALS:
        return NECRO_BIN_OP_EQUALS;
    case NECRO_LEX_AND:
        return NECRO_BIN_OP_AND;
    case NECRO_LEX_OR:
        return NECRO_BIN_OP_OR;
    default:
        return NECRO_BIN_OP_UNDEFINED;
    }
}

NecroAST_LocalPtr parse_binary_expression(NecroLexToken** tokens, NecroAST* ast, NecroAST_LocalPtr lhs_local_ptr, int min_precedence)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;

    NecroLexToken* current_token = (*tokens);
    NecroAST_BinOpType bin_op_type = token_to_bin_op_type(current_token->token);
    NecroParse_BinOpBehavior bin_op_behavior = bin_op_behaviors[bin_op_type];

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED || bin_op_behavior.precedence < min_precedence)
    {
        return lhs_local_ptr;
    }

    if (lhs_local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr bin_op_local_ptr = null_local_ptr;
        NecroAST_Node* bin_op_ast_node = NULL;
        NecroAST_LocalPtr rhs_local_ptr = null_local_ptr;
        while (true)
        {
            current_token = (*tokens);
            bin_op_type = token_to_bin_op_type(current_token->token);

#ifdef PARSE_DEBUG_PRINT
            printf(
                "parse_binary_expression while (true) { *token: %p, token: %s, ast: %p, min_precedence: %i, bin_op_type: %i, precedence: %i, associativity: %i }\n",
                *tokens,
                necro_lex_token_type_string((*tokens)->token),
                ast,
                min_precedence,
                bin_op_type,
                bin_op_behaviors[bin_op_type].precedence,
                bin_op_behaviors[bin_op_type].associativity);
#endif // PARSE_DEBUG_PRINT

            if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
            {
                break;
            }

            NecroParse_BinOpBehavior bin_op_behavior = bin_op_behaviors[bin_op_type];
            if (bin_op_behavior.precedence < min_precedence)
            {
                break;
            }

            if (bin_op_local_ptr != null_local_ptr)
            {
                lhs_local_ptr = bin_op_local_ptr;
            }

            bin_op_ast_node = ast_alloc_node_local_ptr(ast, &bin_op_local_ptr);
            bin_op_ast_node->type = NECRO_AST_BIN_OP;
            bin_op_ast_node->bin_op.type = bin_op_type;
            ++(*tokens); // consume current_token

            int next_min_precedence;
            if (bin_op_behavior.associativity == NECRO_BIN_OP_ASSOC_LEFT)
                next_min_precedence = bin_op_behavior.precedence + 1;
            else
                next_min_precedence = bin_op_behavior.precedence;

            rhs_local_ptr = parse_expression(tokens, ast, NECRO_NO_BINARY_LOOK_AHEAD);
            NecroAST_BinOpType look_ahead_bin_op_type = token_to_bin_op_type((*tokens)->token);
            NecroParse_BinOpBehavior look_ahead_bin_op_behavior = bin_op_behaviors[look_ahead_bin_op_type];

#ifdef PARSE_DEBUG_PRINT
            printf(" parse_binary_expression rhs_local_ptr { *tokens: %p, token: %s, ast: %p, lhs_local_ptr: %u, rhs_local_ptr: %u }\n",
            *tokens,
            necro_lex_token_type_string((*tokens)->token),
            ast,
            lhs_local_ptr,
            rhs_local_ptr);
#endif // PARSE_DEBUG_PRINT

            while ((look_ahead_bin_op_type != NECRO_BIN_OP_UNDEFINED) && (look_ahead_bin_op_behavior.precedence >= next_min_precedence))
            {
#ifdef PARSE_DEBUG_PRINT
                printf(" parse_binary_expression while (rhs_local_ptr) { *tokens: %p, token: %s, ast: %p, lhs_local_ptr: %u, rhs_local_ptr: %u }\n",
                *tokens,
                necro_lex_token_type_string((*tokens)->token),
                ast,
                lhs_local_ptr,
                rhs_local_ptr);
#endif // PARSE_DEBUG_PRINT

                rhs_local_ptr = parse_binary_expression(tokens, ast, rhs_local_ptr, next_min_precedence);
                look_ahead_bin_op_type = token_to_bin_op_type((*tokens)->token);
                look_ahead_bin_op_behavior = bin_op_behaviors[look_ahead_bin_op_type];
            }

            if (rhs_local_ptr == null_local_ptr)
            {
                break;
            }

            bin_op_ast_node->bin_op.lhs = lhs_local_ptr;
            bin_op_ast_node->bin_op.rhs = rhs_local_ptr;
        }

        if ((lhs_local_ptr != null_local_ptr) && (rhs_local_ptr != null_local_ptr))
        {
#ifdef PARSE_DEBUG_PRINT
            printf(
                " parse_binary_expression SUCCESS { *tokens: %p, token: %s, ast: %p, min_precedence: %i }\n",
                *tokens,
                necro_lex_token_type_string((*tokens)->token),
                ast,
                min_precedence);
#endif // PARSE_DEBUG_PRINT
            // // swap lhs node and bin op node so that the bin op expression is before the lhs
            // if (lhs_swap)
            // {
            //     // Swap node objects
            //     NecroAST_Node lhs_ast_tmp = *lhs_ast_node;
            //     *lhs_ast_node = *bin_op_ast_node;
            //     *bin_op_ast_node = lhs_ast_tmp;
            //
            //     // Swap node pointers
            //     NecroAST_Node* lhs_ast_node_ptr_tmp = lhs_ast_node;
            //     lhs_ast_node = bin_op_ast_node;
            //     bin_op_ast_node = lhs_ast_node_ptr_tmp;
            //
            //     // Swap node local_ptrs
            //     NecroAST_LocalPtr lhs_local_ptr_tmp = lhs_local_ptr;
            //     lhs_local_ptr = bin_op_local_ptr;
            //     bin_op_local_ptr = lhs_local_ptr_tmp;
            // }

            bin_op_ast_node->bin_op.lhs = lhs_local_ptr;
            bin_op_ast_node->bin_op.rhs = rhs_local_ptr;
            return parse_binary_operation(tokens, ast, bin_op_local_ptr); // Try to keep evaluating lower precedence tokens
        }
#ifdef PARSE_DEBUG_PRINT
            printf(" parse_binary_expression FAILED { *tokens: %p, token: %s, ast: %p, lhs_local_ptr: %u, rhs_local_ptr: %u }\n",
            *tokens,
            necro_lex_token_type_string((*tokens)->token),
            ast,
            lhs_local_ptr,
            rhs_local_ptr);
#endif // PARSE_DEBUG_PRINT
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return lhs_local_ptr;
}

NecroAST_LocalPtr parse_binary_operation(NecroLexToken** tokens, NecroAST* ast, NecroAST_LocalPtr lhs_local_ptr)
{
    NecroLexToken* current_token = (*tokens);
    NecroAST_BinOpType bin_op_type = token_to_bin_op_type(current_token->token);

#ifdef PARSE_DEBUG_PRINT
            printf(
                "parse_binary_operation { *tokens: %p, token: %s, ast: %p, bin_op_type: %i, precedence: %i, associativity: %i }\n",
                *tokens,
                necro_lex_token_type_string((*tokens)->token),
                ast,
                bin_op_type,
                bin_op_behaviors[bin_op_type].precedence,
                bin_op_behaviors[bin_op_type].associativity);
#endif // PARSE_DEBUG_PRINT

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        return lhs_local_ptr;
    }

    return parse_binary_expression(tokens, ast, lhs_local_ptr, bin_op_behaviors[bin_op_type].precedence);
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

NecroAST_LocalPtr parse_l_expression(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;
    NecroAST_LocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_if_then_else_expression(tokens, ast);
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

NecroAST_LocalPtr parse_if_then_else_expression(NecroLexToken** tokens, NecroAST* ast)
{
    if ((*tokens)->token == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroLexToken* original_tokens = *tokens;
    size_t original_ast_size = ast->arena.size;

    if ((*tokens)->token == NECRO_LEX_IF)
    {
        NecroAST_LocalPtr if_then_else_local_ptr = null_local_ptr;
        NecroAST_Node* ast_node = ast_alloc_node_local_ptr(ast, &if_then_else_local_ptr);
        ast_node->type = NECRO_AST_IF_THEN_ELSE;

        ++(*tokens); // consume IF token
        NecroAST_LocalPtr if_local_ptr = parse_expression(tokens, ast, NECRO_BINARY_LOOK_AHEAD);
        if (if_local_ptr != null_local_ptr && (*tokens)->token == NECRO_LEX_THEN)
        {
            ++(*tokens); // consume THEN token
            NecroAST_LocalPtr then_local_ptr = parse_expression(tokens, ast, NECRO_BINARY_LOOK_AHEAD);
            if (then_local_ptr != null_local_ptr && (*tokens)->token == NECRO_LEX_ELSE)
            {
                ++(*tokens); // consume ELSE token
                NecroAST_LocalPtr else_local_ptr = parse_expression(tokens, ast, NECRO_BINARY_LOOK_AHEAD);
                if (else_local_ptr != null_local_ptr)
                {
                    ast_node->if_then_else = (NecroAST_IfThenElse) { if_local_ptr, then_local_ptr, else_local_ptr };
                    return if_then_else_local_ptr;
                }
            }
        }
    }

    *tokens = original_tokens;
    ast->arena.size = original_ast_size; // backtrack AST modifications
    return null_local_ptr;
}

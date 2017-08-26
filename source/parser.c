/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "parser.h"

// #define PARSE_DEBUG_PRINT 1

// =====================================================
// Parser helpers
// =====================================================

// Snap shot of parser state for back tracking
typedef struct
{
    size_t current_token;
    size_t ast_size;
} NecroParser_Snapshot;

static inline NecroParser_Snapshot snapshot_parser(NecroParser* parser)
{
    return (NecroParser_Snapshot) { parser->current_token, parser->ast->arena.size };
}

static inline void restore_parser(NecroParser* parser, NecroParser_Snapshot snapshot)
{
    parser->current_token = snapshot.current_token;
    parser->ast->arena.size = snapshot.ast_size;
}

static inline NecroLexToken* peek_token(NecroParser* parser)
{
    return parser->tokens + parser->current_token;
}

static inline NECRO_LEX_TOKEN_TYPE peek_token_type(NecroParser* parser)
{
    return peek_token(parser)->token;
}

static inline void consume_token(NecroParser* parser)
{
    ++parser->current_token;
}

// =====================================================
// Abstract Syntax Tree
// =====================================================

NecroAST_Node* ast_alloc_node(NecroParser* parser)
{
    return (NecroAST_Node*) arena_alloc(&parser->ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

NecroAST_Node* ast_alloc_node_local_ptr(NecroParser* parser, NecroAST_LocalPtr* local_ptr)
{
    *local_ptr = parser->ast->arena.size / sizeof(NecroAST_Node);
    return (NecroAST_Node*) arena_alloc(&parser->ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

NecroAST_LocalPtr ast_last_node_ptr(NecroParser* parser)
{
    NecroAST_LocalPtr local_ptr = null_local_ptr;
    if (parser->ast->arena.size > 0)
    {
        return (parser->ast->arena.size / sizeof(NecroAST_Node)) - 1;
    }
    return local_ptr;
}

#define AST_TAB "  "

void print_ast_impl(NecroAST* ast, NecroAST_Node* ast_node, NecroIntern* intern, uint32_t depth)
{
    assert(ast != NULL);
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
        print_ast_impl(ast, ast_get_node(ast, ast_node->if_then_else.if_expr), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->if_then_else.then_expr), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->if_then_else.else_expr), intern, depth + 1);
        break;

    case NECRO_AST_TOP_DECL:
        puts("(Top Declaration)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->top_declaration.declaration), intern, depth + 1);
        if (ast_node->top_declaration.next_top_decl != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->top_declaration.next_top_decl), intern, depth);
        }
        break;

    case NECRO_AST_DECL:
        puts("(Declaration)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->declaration.declaration_impl), intern, depth + 1);
        if (ast_node->declaration.next_declaration != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->declaration.next_declaration), intern, depth);
        }
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        printf("(Assignment: %s)\n", necro_intern_get_string(intern, ast_node->simple_assignment.variable_name));
        print_ast_impl(ast, ast_get_node(ast, ast_node->simple_assignment.rhs), intern, depth + 1);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        puts("(Right Hand Side)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->right_hand_side.expression), intern, depth + 1);
        if (ast_node->right_hand_side.declarations != null_local_ptr)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "where");
            print_ast_impl(ast, ast_get_node(ast, ast_node->right_hand_side.declarations), intern, depth + 3);
        }
        break;

    case NECRO_AST_FUNCTION_EXPRESSION:
        puts("(fexp)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->fexpression.aexp), intern, depth + 1);
        if (ast_node->fexpression.next_fexpression != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->fexpression.next_fexpression), intern, depth + 1);
        }
        break;

    case NECRO_AST_VARIABLE:
        switch(ast_node->variable.variable_type)
        {
        case NECRO_AST_VARIABLE_ID:
            {
                const char* variable_string = necro_intern_get_string(intern, ast_node->variable.variable_id);
                if (variable_string)
                {
                    printf("(varid: %s)\n", variable_string);
                }
                else
                {
                    puts("(varid: \"\")");
                }
            }
            break;

        case NECRO_AST_VARIABLE_SYMBOL:
            {
                const char* variable_string = necro_lex_token_type_string(ast_node->variable.variable_symbol);
                if (variable_string)
                {
                    printf("(varsym: %s)\n", variable_string);
                }
                else
                {
                    puts("(varsym: \"\")");
                }
            };
            break;
        }
        break;

    case NECRO_AST_APATS:
        puts("(Apat)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->apats.apat), intern, depth + 1);
        if (ast_node->apats.next_apat != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->apats.next_apat), intern, depth);
        }
        break;

    case NECRO_AST_WILDCARD:
        puts("(_)");
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
        printf("(Apats Assignment: %s)\n", necro_intern_get_string(intern, ast_node->apats_assignment.variable_name));
        print_ast_impl(ast, ast_get_node(ast, ast_node->apats_assignment.apats), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->apats_assignment.rhs), intern, depth + 1);
        break;

    default:
        puts("(Undefined)");
        break;
    }
}

void print_ast(NecroAST* ast, NecroIntern* intern, NecroAST_LocalPtr root_node_ptr)
{
    if (root_node_ptr == null_local_ptr)
    {
        puts("(Empty AST)");
    }
    else
    {
        print_ast_impl(ast, ast_get_node(ast, root_node_ptr), intern, 0);
    }
}

double compute_ast_math_impl(NecroAST* ast, NecroAST_Node* ast_node)
{
    switch(ast_node->type)
    {
    case NECRO_AST_BIN_OP:
        {
            const double a = compute_ast_math_impl(ast, ast_get_node(ast, ast_node->bin_op.lhs));
            const double b = compute_ast_math_impl(ast, ast_get_node(ast, ast_node->bin_op.rhs));
            switch(ast_node->bin_op.type)
            {
            case NECRO_BIN_OP_ADD:
                return a + b;
            case NECRO_BIN_OP_SUB:
                return a - b;
            case NECRO_BIN_OP_MUL:
                return a * b;
            case NECRO_BIN_OP_DIV:
                return a / b;
            default:
                puts("(Undefined Binary Operator)");
                break;
            }
        }
        break;

    case NECRO_AST_CONSTANT:
        switch(ast_node->constant.type)
        {
        case NECRO_AST_CONSTANT_FLOAT:
            return ast_node->constant.double_literal;
        case NECRO_AST_CONSTANT_INTEGER:
            return (double) ast_node->constant.int_literal;
        }
        break;
    default:
        puts("(compute_ast_math) Unrecognized AST node for mathematical expression.");
        break;
    }

    return 0.0;
}

void compute_ast_math(NecroAST* ast, NecroAST_LocalPtr root_node_ptr)
{
    double result = compute_ast_math_impl(ast, ast_get_node(ast, root_node_ptr));
    printf("compute_ast_math: %f\n", result);
}

// =====================================================
// Recursive Descent Parser
// =====================================================

typedef enum
{
    NECRO_NO_BINARY_LOOK_AHEAD,
    NECRO_BINARY_LOOK_AHEAD,
} NecroParser_LookAheadBinary;

NecroAST_LocalPtr parse_expression_impl(NecroParser* parser, NecroParser_LookAheadBinary look_ahead_binary);

static inline NecroAST_LocalPtr parse_expression(NecroParser* parser)
{
    return parse_expression_impl(parser, NECRO_BINARY_LOOK_AHEAD);
}

NecroAST_LocalPtr parse_parenthetical_expression(NecroParser* parser);
NecroAST_LocalPtr parse_l_expression(NecroParser* parser);
NecroAST_LocalPtr parse_if_then_else_expression(NecroParser* parser);
NecroAST_LocalPtr parse_constant(NecroParser* parser);
NecroAST_LocalPtr parse_unary_operation(NecroParser* parser);
NecroAST_LocalPtr parse_binary_operation(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr);
NecroAST_LocalPtr parse_function_composition(NecroParser* parser);
NecroAST_LocalPtr parse_end_of_stream(NecroParser* parser);
NecroAST_LocalPtr parse_top_declarations(NecroParser* parser);
NecroAST_LocalPtr parse_declarations(NecroParser* parser);
NecroAST_LocalPtr parse_simple_assignment(NecroParser* parser);
NecroAST_LocalPtr parse_apats_assignment(NecroParser* parser);
NecroAST_LocalPtr parse_apats(NecroParser* parser);
NecroAST_LocalPtr parse_right_hand_side(NecroParser* parser);

NecroParse_Result parse_ast(NecroParser* parser, NecroAST_LocalPtr* out_root_node_ptr)
{
    NecroAST_LocalPtr local_ptr = parse_top_declarations(parser);
#ifdef PARSE_DEBUG_PRINT
    printf(
        " parse_ast result { tokens*: %p, token: %s , ast: %p }\n",
        parser->tokens,
        necro_lex_token_type_string(peek_token_type(parser)),
        parser->ast);
#endif // PARSE_DEBUG_PRINT
    if ((local_ptr != null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        if ((peek_token_type(parser) ==  NECRO_LEX_END_OF_STREAM) || (peek_token_type(parser) ==  NECRO_LEX_SEMI_COLON))
        {
            *out_root_node_ptr = local_ptr;
            return ParseSuccessful;
        }
        else
        {
            NecroLexToken* look_ahead_token = peek_token(parser);
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Parsing ended without error, but not all tokens were consumed. This is likely a parser bug.\n"
                "Parsing ended at line %zu, character %zu. Parsing stopped at the token %s, which is token number %zu.",
                look_ahead_token->line_number,
                look_ahead_token->character_number,
                necro_lex_token_type_string(look_ahead_token->token),
                parser->current_token);
        }
    }

    *out_root_node_ptr = null_local_ptr;
    return ParseError;
}

NecroAST_LocalPtr parse_top_declarations(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || peek_token_type(parser) == NECRO_LEX_SEMI_COLON || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr declarations_local_ptr = parse_declarations(parser);
    if ((declarations_local_ptr != null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        NecroAST_LocalPtr next_top_decl = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser);
            next_top_decl = parse_top_declarations(parser);
        }

        NecroAST_LocalPtr top_decl_local_ptr = null_local_ptr;
        NecroAST_Node* top_decl_node = ast_alloc_node_local_ptr(parser, &top_decl_local_ptr);
        top_decl_node->top_declaration.declaration = declarations_local_ptr;
        top_decl_node->top_declaration.next_top_decl = next_top_decl;
        top_decl_node->type = NECRO_AST_TOP_DECL;
        return top_decl_local_ptr;
    }

    if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        snprintf(
            parser->error_message,
            MAX_ERROR_MESSAGE_SIZE,
            "Failed to parse any top level declarations.");

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_simple_assignment(parser);
    }

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_apats_assignment(parser);
    }

    if ((declaration_local_ptr != null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        NecroAST_LocalPtr next_decl = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser); // consume SemiColon token
            next_decl = parse_declarations_list(parser);
        }

        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroAST_LocalPtr decl_local_ptr = null_local_ptr;
            NecroAST_Node* decl_node = ast_alloc_node_local_ptr(parser, &decl_local_ptr);
            decl_node->declaration.declaration_impl = declaration_local_ptr;
            decl_node->declaration.next_declaration = next_decl;
            decl_node->type = NECRO_AST_DECL;
            return decl_local_ptr;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_declarations(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    bool is_list = peek_token_type(parser) == NECRO_LEX_LEFT_BRACE;
    if (is_list)
    {
        consume_token(parser); // consume left brace token
    }

    NecroAST_LocalPtr declarations_list_local_ptr = parse_declarations_list(parser);
    if (declarations_list_local_ptr != null_local_ptr)
    {
        if (is_list)
        {
            if ((parser->descent_state != NECRO_DESCENT_PARSE_ERROR) && (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE))
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                snprintf(
                    parser->error_message,
                    MAX_ERROR_MESSAGE_SIZE,
                    "Failed to parse declaration, at line %zu, character %zu. Expected a \'}\' token but instead found %s",
                    look_ahead_token->line_number,
                    look_ahead_token->character_number,
                    necro_lex_token_type_string(look_ahead_token->token));

                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
            consume_token(parser); // consume right brace token
        }

        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            return declarations_list_local_ptr;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_simple_assignment(NecroParser* parser)
{
    const NecroLexToken* variable_name_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = variable_name_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type != NECRO_LEX_IDENTIFIER ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume identifier token

    if (peek_token_type(parser) == NECRO_LEX_ASSIGN)
    {
        consume_token(parser); // consume '=' operator
        NecroLexToken* look_ahead_token = peek_token(parser);
        NecroAST_LocalPtr rhs_local_ptr = parse_right_hand_side(parser);
        if (rhs_local_ptr != null_local_ptr)
        {
            NecroAST_LocalPtr assignment_local_ptr = null_local_ptr;
            NecroAST_Node* assignment_node = ast_alloc_node_local_ptr(parser, &assignment_local_ptr);
            assignment_node->simple_assignment.variable_name = variable_name_token->symbol;
            assignment_node->simple_assignment.rhs = rhs_local_ptr;
            assignment_node->type = NECRO_AST_SIMPLE_ASSIGNMENT;
            return assignment_local_ptr;
        }
        else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Right hand side of assignment failed to parse at line %zu, character %zu.",
                look_ahead_token->line_number,
                look_ahead_token->character_number);

            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_apats_assignment(NecroParser* parser)
{
    const NecroLexToken* variable_name_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = variable_name_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type != NECRO_LEX_IDENTIFIER ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume identifier token

    NecroAST_LocalPtr apats_local_ptr = parse_apats(parser);

    if (apats_local_ptr != null_local_ptr && peek_token_type(parser) == NECRO_LEX_ASSIGN)
    {
        consume_token(parser); // consume '=' operator
        NecroLexToken* look_ahead_token = peek_token(parser);
        NecroAST_LocalPtr rhs_local_ptr = parse_right_hand_side(parser);
        if (rhs_local_ptr != null_local_ptr)
        {
            NecroAST_LocalPtr assignment_local_ptr = null_local_ptr;
            NecroAST_Node* assignment_node = ast_alloc_node_local_ptr(parser, &assignment_local_ptr);
            assignment_node->apats_assignment.variable_name = variable_name_token->symbol;
            assignment_node->apats_assignment.apats = apats_local_ptr;
            assignment_node->apats_assignment.rhs = rhs_local_ptr;
            assignment_node->type = NECRO_AST_APATS_ASSIGNMENT;
            return assignment_local_ptr;
        }
        else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Right hand side of assignment failed to parse at line %zu, character %zu.",
                look_ahead_token->line_number,
                look_ahead_token->character_number);

            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_right_hand_side(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroLexToken* look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr expression_local_ptr = parse_expression(parser);
    if (expression_local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr declarations_local_ptr = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_WHERE)
        {
            NecroLexToken* where_token = peek_token(parser);
            consume_token(parser); // consume where token
            declarations_local_ptr = parse_declarations(parser);
            if (declarations_local_ptr == null_local_ptr)
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                snprintf(
                    parser->error_message,
                    MAX_ERROR_MESSAGE_SIZE,
                    "\'where\' clause failed to parse. Expected declarations after \'where\' token on line %zu, character %zu. "
                    "Failed to parse declaration starting at line %zu, character %zu",
                    where_token->line_number,
                    where_token->character_number,
                    look_ahead_token->line_number,
                    look_ahead_token->character_number);

                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
        }

        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroAST_LocalPtr rhs_local_ptr = null_local_ptr;
            NecroAST_Node* rhs_node = ast_alloc_node_local_ptr(parser, &rhs_local_ptr);
            rhs_node->right_hand_side.expression = expression_local_ptr;
            rhs_node->right_hand_side.declarations = declarations_local_ptr;
            rhs_node->type = NECRO_AST_RIGHT_HAND_SIDE;
            return rhs_local_ptr;
        }
    }
    else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        snprintf(
            parser->error_message,
            MAX_ERROR_MESSAGE_SIZE,
            "Right hand side expression failed to parse at line %zu, character %zu.",
            look_ahead_token->line_number,
            look_ahead_token->character_number);

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_expression_impl(NecroParser* parser, NecroParser_LookAheadBinary look_ahead_binary)
{
#ifdef PARSE_DEBUG_PRINT
    printf(
        " parse_expression { tokens*: %p, token: %s , ast: %p }\n",
        parser->tokens,
        peek_token_type(parser),
        necro_lex_token_type_string(peek_token_type(parser)),
        ast);
#endif // PARSE_DEBUG_PRINT

    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr local_ptr = null_local_ptr;

    // if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    // {
    //     local_ptr = parse_function_composition(parser);
    // }
    //
    // if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    // {
    //     local_ptr = parse_unary_operation(parser);
    // }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_l_expression(parser);
    }

    if ((parser->descent_state != NECRO_DESCENT_PARSE_ERROR) &&
        (look_ahead_binary == NECRO_BINARY_LOOK_AHEAD) &&
        (local_ptr != null_local_ptr)) // Try parsing expression as a binary operation expression
    {
        NecroAST_LocalPtr bin_op_local_ptr = parse_binary_operation(parser, local_ptr);
        if (bin_op_local_ptr != null_local_ptr)
            local_ptr = bin_op_local_ptr;
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_variable(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    const NecroLexToken* variable_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = variable_token->token;
    consume_token(parser); // Consume variable token
    if (token_type == NECRO_LEX_IDENTIFIER) // Parse Variable Name
    {
        NecroAST_LocalPtr varid_local_ptr = null_local_ptr;
        NecroAST_Node* varid_node = ast_alloc_node_local_ptr(parser, &varid_local_ptr);
        varid_node->type = NECRO_AST_VARIABLE;
        varid_node->variable.variable_id = variable_token->symbol;
        varid_node->variable.variable_type = NECRO_AST_VARIABLE_ID;
        return varid_local_ptr;
    }
    else if (token_type == NECRO_LEX_LEFT_PAREN) // Parenthetical variable symbol
    {
        const NecroLexToken* sym_variable_token = peek_token(parser);
        const NECRO_LEX_TOKEN_TYPE sym_token_type = sym_variable_token->token;
        switch(sym_token_type)
        {
        case NECRO_LEX_ADD:
        case NECRO_LEX_SUB:
        case NECRO_LEX_MUL:
        case NECRO_LEX_DIV:
        case NECRO_LEX_MOD:
        case NECRO_LEX_GT:
        case NECRO_LEX_LT:
        case NECRO_LEX_GTE:
        case NECRO_LEX_LTE:
        case NECRO_LEX_DOUBLE_COLON:
        case NECRO_LEX_LEFT_SHIFT:
        case NECRO_LEX_RIGHT_SHIFT:
        case NECRO_LEX_PIPE:
        case NECRO_LEX_FORWARD_PIPE:
        case NECRO_LEX_BACK_PIPE:
        case NECRO_LEX_EQUALS:
        case NECRO_LEX_NOT_EQUALS:
        case NECRO_LEX_AND:
        case NECRO_LEX_OR:
        case NECRO_LEX_BIND_RIGHT:
        case NECRO_LEX_BIND_LEFT:
        case NECRO_LEX_DOUBLE_EXCLAMATION:
        case NECRO_LEX_APPEND:
        case NECRO_LEX_ACCENT:
        case NECRO_LEX_DOT:
        case NECRO_LEX_TILDE:
        case NECRO_LEX_BACK_SLASH:
        case NECRO_LEX_CARET:
        case NECRO_LEX_DOLLAR:
        case NECRO_LEX_AT:
        case NECRO_LEX_AMPERSAND:
        case NECRO_LEX_COLON:
        case NECRO_LEX_QUESTION_MARK:
        case NECRO_LEX_EXCLAMATION:
        case NECRO_LEX_HASH:
        case NECRO_LEX_UNIT:
        case NECRO_LEX_LEFT_ARROW:
        case NECRO_LEX_RIGHT_ARROW:
            consume_token(parser); // consume symbol token
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            {
                const NecroLexToken* look_ahead_token = peek_token(parser);
                snprintf(
                    parser->error_message,
                    MAX_ERROR_MESSAGE_SIZE,
                    "Failed to parse parenthetical variable symbol because there was no closing bracket at line %zu, character %zu. Failed beginning with token %s",
                    look_ahead_token->line_number,
                    look_ahead_token->character_number,
                    necro_lex_token_type_string(look_ahead_token->token));

                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
            else
            {
                consume_token(parser); // consume ")" token
                NecroAST_LocalPtr varsym_local_ptr = null_local_ptr;
                NecroAST_Node* varsym_node = ast_alloc_node_local_ptr(parser, &varsym_local_ptr);
                varsym_node->type = NECRO_AST_VARIABLE;
                varsym_node->variable.variable_symbol = sym_token_type;
                varsym_node->variable.variable_type = NECRO_AST_VARIABLE_SYMBOL;
                return varsym_local_ptr;
            }
            break;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_application_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_variable(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_constant(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_parenthetical_expression(parser);
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_function_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr aexpression = parse_application_expression(parser);
    if (aexpression != null_local_ptr)
    {
        NecroAST_LocalPtr next_fexpression = parse_function_expression(parser);
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroAST_LocalPtr fexpression_local_ptr = null_local_ptr;
            NecroAST_Node* fexpression_node = ast_alloc_node_local_ptr(parser, &fexpression_local_ptr);
            fexpression_node->type = NECRO_AST_FUNCTION_EXPRESSION;
            fexpression_node->fexpression.aexp = aexpression;
            fexpression_node->fexpression.next_fexpression = next_fexpression;
            return fexpression_local_ptr;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_wildcard(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    if (peek_token_type(parser) != NECRO_LEX_UNDER_SCORE)
        return null_local_ptr;

    consume_token(parser);
    NecroAST_LocalPtr wildcard_local_ptr = null_local_ptr;
    NecroAST_Node* wildcard_node = ast_alloc_node_local_ptr(parser, &wildcard_local_ptr);
    wildcard_node->type = NECRO_AST_WILDCARD;
    wildcard_node->undefined._pad = 0;
    return wildcard_local_ptr;
}

NecroAST_LocalPtr parse_apat(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_variable(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_wildcard(parser);
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_apats(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr apat_local_ptr = parse_apat(parser);
    if (apat_local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr next_apat_local_ptr = parse_apats(parser);
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroAST_LocalPtr apats_local_ptr = null_local_ptr;
            NecroAST_Node* apats_node = ast_alloc_node_local_ptr(parser, &apats_local_ptr);
            apats_node->type = NECRO_AST_APATS;
            apats_node->apats.apat = apat_local_ptr;
            apats_node->apats.next_apat = next_apat_local_ptr;
            return apats_local_ptr;
        }
    }

    if (apat_local_ptr != null_local_ptr)
    {
        return apat_local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_constant(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr local_ptr = null_local_ptr;
    switch(peek_token_type(parser))
    {
    case NECRO_LEX_FLOAT_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(parser, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_FLOAT;
            constant.double_literal = peek_token(parser)->double_literal;
            ast_node->constant = constant;
            consume_token(parser);
#ifdef PARSE_DEBUG_PRINT
    printf(" parse_expression NECRO_LEX_FLOAT_LITERAL { ast_node: %p, local_ptr: %u }\n", ast_node, local_ptr);
#endif // PARSE_DEBUG_PRINT
            return local_ptr;
        }
    case NECRO_LEX_INTEGER_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(parser, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_INTEGER;
            constant.int_literal = peek_token(parser)->int_literal;
            ast_node->constant = constant;
            consume_token(parser);
#ifdef PARSE_DEBUG_PRINT
    printf(" parse_expression NECRO_LEX_INTEGER_LITERAL { ast_node: %p, local_ptr: %u }\n", ast_node, local_ptr);
#endif // PARSE_DEBUG_PRINT
            return local_ptr;
        }

    case NECRO_LEX_STRING_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(parser, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_STRING;
            constant.symbol = peek_token(parser)->symbol;
            ast_node->constant = constant;
            consume_token(parser);
            return local_ptr;
        }
    case NECRO_LEX_CHAR_LITERAL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(parser, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_CHAR;
            constant.char_literal = peek_token(parser)->char_literal;
            ast_node->constant = constant;
            consume_token(parser);
            return local_ptr;
        }
    case NECRO_AST_CONSTANT_BOOL:
        {
            NecroAST_Node* ast_node = ast_alloc_node_local_ptr(parser, &local_ptr);
            ast_node->type = NECRO_AST_CONSTANT;
            NecroAST_Constant constant;
            constant.type = NECRO_AST_CONSTANT_BOOL;
            constant.boolean_literal = peek_token(parser)->boolean_literal;
            ast_node->constant = constant;
            consume_token(parser);
            return local_ptr;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_parenthetical_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LEFT_PAREN)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume left parentheses token
    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr local_ptr = parse_expression(parser);

    if (local_ptr == null_local_ptr)
    {
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Failed to parse expression after \'(\' token at line %zu, character %zu. Failed beginning with token %s",
                look_ahead_token->line_number,
                look_ahead_token->character_number,
                necro_lex_token_type_string(look_ahead_token->token));

            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        snprintf(
            parser->error_message,
            MAX_ERROR_MESSAGE_SIZE,
            "Failed to parse parenthetical expression because there was no closing bracket at line %zu, character %zu. Failed beginning with token %s",
            look_ahead_token->line_number,
            look_ahead_token->character_number,
            necro_lex_token_type_string(look_ahead_token->token));

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    consume_token(parser);
    return local_ptr;
}

NecroAST_LocalPtr parse_unary_operation(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    restore_parser(parser, snapshot);
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

NecroAST_LocalPtr parse_binary_expression(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr, int min_precedence)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroLexToken* current_token = peek_token(parser);
    NecroAST_BinOpType bin_op_type = token_to_bin_op_type(current_token->token);
    NecroParse_BinOpBehavior bin_op_behavior = bin_op_behaviors[bin_op_type];

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED || bin_op_behavior.precedence < min_precedence)
    {
        return lhs_local_ptr;
    }

    if (lhs_local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr bin_op_local_ptr = null_local_ptr;
        NecroAST_LocalPtr rhs_local_ptr = null_local_ptr;
        while (true)
        {
            current_token = peek_token(parser);
            bin_op_type = token_to_bin_op_type(current_token->token);

#ifdef PARSE_DEBUG_PRINT
            printf(
                "parse_binary_expression while (true) { current_token: %p, token: %s, ast: %p, min_precedence: %i, bin_op_type: %i, precedence: %i, associativity: %i }\n",
                current_token,
                necro_lex_token_type_string(peek_token_type(parser)),
                parser->ast,
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

            consume_token(parser); // consume current_token

            int next_min_precedence;
            if (bin_op_behavior.associativity == NECRO_BIN_OP_ASSOC_LEFT)
                next_min_precedence = bin_op_behavior.precedence + 1;
            else
                next_min_precedence = bin_op_behavior.precedence;

            rhs_local_ptr = parse_expression_impl(parser, NECRO_NO_BINARY_LOOK_AHEAD);
            NecroLexToken* look_ahead_token = peek_token(parser);
            NecroAST_BinOpType look_ahead_bin_op_type = token_to_bin_op_type(look_ahead_token->token);
            NecroParse_BinOpBehavior look_ahead_bin_op_behavior = bin_op_behaviors[look_ahead_bin_op_type];

#ifdef PARSE_DEBUG_PRINT
            printf(" parse_binary_expression rhs_local_ptr { tokens: %p, token: %s, ast: %p, lhs_local_ptr: %u, rhs_local_ptr: %u }\n",
            parser->tokens,
            necro_lex_token_type_string(peek_token_type(parser)),
            parser->ast,
            lhs_local_ptr,
            rhs_local_ptr);
#endif // PARSE_DEBUG_PRINT

            while ((parser->descent_state != NECRO_DESCENT_PARSE_ERROR) &&
                   (look_ahead_bin_op_type != NECRO_BIN_OP_UNDEFINED) &&
                   (look_ahead_bin_op_behavior.precedence >= next_min_precedence))
            {
#ifdef PARSE_DEBUG_PRINT
                printf(" parse_binary_expression while (rhs_local_ptr) { tokens: %p, token: %s, ast: %p, lhs_local_ptr: %u, rhs_local_ptr: %u }\n",
                parser->tokens,
                necro_lex_token_type_string(peek_token_type(parser)),
                parser->ast,
                lhs_local_ptr,
                rhs_local_ptr);
#endif // PARSE_DEBUG_PRINT

                rhs_local_ptr = parse_binary_expression(parser, rhs_local_ptr, next_min_precedence);
                if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
                {
                    restore_parser(parser, snapshot);
                    return null_local_ptr;
                }

                if (look_ahead_token != peek_token(parser))
                {
                    look_ahead_token = peek_token(parser);
                    look_ahead_bin_op_type = token_to_bin_op_type(look_ahead_token->token);
                    look_ahead_bin_op_behavior = bin_op_behaviors[look_ahead_bin_op_type];
                }
                else // stop parsing if we haven't consumed any tokens
                {
                    break;
                }
            }

            if (rhs_local_ptr == null_local_ptr)
            {
                break;
            }

            {
                NecroAST_Node* bin_op_ast_node = ast_alloc_node_local_ptr(parser, &bin_op_local_ptr);
                bin_op_ast_node->type = NECRO_AST_BIN_OP;
                bin_op_ast_node->bin_op.type = bin_op_type;
                bin_op_ast_node->bin_op.lhs = lhs_local_ptr;
                bin_op_ast_node->bin_op.rhs = rhs_local_ptr;
            }
        }

        if ((bin_op_local_ptr != null_local_ptr) && (lhs_local_ptr != null_local_ptr) && (rhs_local_ptr != null_local_ptr))
        {
#ifdef PARSE_DEBUG_PRINT
            printf(
                " parse_binary_expression SUCCESS { tokens: %p, token: %s, ast: %p, min_precedence: %i }\n",
                parser->tokens,
                necro_lex_token_type_string(peek_token_type(parser)),
                parser->ast,
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

            NecroAST_Node* bin_op_ast_node = ast_get_node(parser->ast, bin_op_local_ptr);
            bin_op_ast_node->bin_op.lhs = lhs_local_ptr;
            bin_op_ast_node->bin_op.rhs = rhs_local_ptr;
            return parse_binary_operation(parser, bin_op_local_ptr); // Try to keep evaluating lower precedence tokens
        }
#ifdef PARSE_DEBUG_PRINT
            printf(" parse_binary_expression FAILED { tokens: %p, token: %s, ast: %p, lhs_local_ptr: %u, rhs_local_ptr: %u }\n",
            parser->tokens,
            necro_lex_token_type_string(peek_token_type(parser)),
            parser->ast,
            lhs_local_ptr,
            rhs_local_ptr);
#endif // PARSE_DEBUG_PRINT
    }

    restore_parser(parser, snapshot);
    return lhs_local_ptr;
}

NecroAST_LocalPtr parse_binary_operation(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr)
{
    NecroLexToken* current_token = peek_token(parser);
    NecroAST_BinOpType bin_op_type = token_to_bin_op_type(current_token->token);

#ifdef PARSE_DEBUG_PRINT
            printf(
                "parse_binary_operation { tokens: %p, token: %s, ast: %p, bin_op_type: %i, precedence: %i, associativity: %i }\n",
                parser->tokens,
                necro_lex_token_type_string(peek_token_type(parser)),
                parser->ast,
                bin_op_type,
                bin_op_behaviors[bin_op_type].precedence,
                bin_op_behaviors[bin_op_type].associativity);
#endif // PARSE_DEBUG_PRINT

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        return lhs_local_ptr;
    }

    return parse_binary_expression(parser, lhs_local_ptr, bin_op_behaviors[bin_op_type].precedence);
}

NecroAST_LocalPtr parse_function_composition(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    /* parse function composition... */

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_l_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_if_then_else_expression(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_function_expression(parser);
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_if_then_else_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    if (peek_token_type(parser) != NECRO_LEX_IF)
        return null_local_ptr;

    // After this point any failure to pattern match will result in a complete parse failure

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr if_then_else_local_ptr = null_local_ptr;
    {
        NecroAST_Node* ast_node = ast_alloc_node_local_ptr(parser, &if_then_else_local_ptr);
        ast_node->type = NECRO_AST_IF_THEN_ELSE;
    }

    consume_token(parser); // consume IF token
    NecroLexToken* look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr if_local_ptr = parse_expression(parser);
    if (if_local_ptr == null_local_ptr)
    {
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Failed to parse expression after \'if\' token at line %zu, character %zu. Failed beginning with token %s",
                look_ahead_token->line_number,
                look_ahead_token->character_number,
                necro_lex_token_type_string(look_ahead_token->token));

            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_THEN)
    {
        snprintf(
            parser->error_message,
            MAX_ERROR_MESSAGE_SIZE,
            "Failed to parse \'if-then-else\' expression at line %zu, character %zu. Expected a \'then\' token, but found %s",
            look_ahead_token->line_number,
            look_ahead_token->character_number,
            necro_lex_token_type_string(look_ahead_token->token));

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    consume_token(parser); // consume THEN token
    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr then_local_ptr = parse_expression(parser);
    if (then_local_ptr == null_local_ptr)
    {
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Failed to parse expression after \'then\' token in \'if-then-else\' expression, at line %zu, character %zu. Failed beginning with token %s",
                look_ahead_token->line_number,
                look_ahead_token->character_number,
                necro_lex_token_type_string(look_ahead_token->token));

            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    look_ahead_token = peek_token(parser);
    if (peek_token_type(parser) != NECRO_LEX_ELSE)
    {
        snprintf(
            parser->error_message,
            MAX_ERROR_MESSAGE_SIZE,
            "Failed to parse \'if-then-else\' expression at line %zu, character %zu. Expected an \'else\' token, but found %s",
            look_ahead_token->line_number,
            look_ahead_token->character_number,
            necro_lex_token_type_string(look_ahead_token->token));

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    consume_token(parser); // consume ELSE token
    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr else_local_ptr = parse_expression(parser);
    if (else_local_ptr == null_local_ptr)
    {
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            snprintf(
                parser->error_message,
                MAX_ERROR_MESSAGE_SIZE,
                "Failed to parse expression after \'else\' token in \'if-then-else\' expression, at line %zu, character %zu. Failed beginning with token %s",
                look_ahead_token->line_number,
                look_ahead_token->character_number,
                necro_lex_token_type_string(look_ahead_token->token));

            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    NecroAST_Node* ast_node = ast_get_node(parser->ast, if_then_else_local_ptr);
    ast_node->if_then_else = (NecroAST_IfThenElse) { if_local_ptr, then_local_ptr, else_local_ptr };
    return if_then_else_local_ptr;
}

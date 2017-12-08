/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdarg.h>
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

const char* var_type_string(NECRO_VAR_TYPE var_type)
{
    switch (var_type)
    {
    case NECRO_VAR_VAR:                      return "Var";
    case NECRO_VAR_TYPE_FREE_VAR:            return "Type Free Var";
    case NECRO_VAR_DECLARATION:              return "Var Declaration";
    case NECRO_VAR_SIG:                      return "Sig";
    case NECRO_VAR_CLASS_SIG:                return "TypeClass Sig";
    default:                                 return "Undefined";
    }
}

const char* con_type_string(NECRO_CON_TYPE con_type)
{
    switch (con_type)
    {
    case NECRO_CON_VAR:              return "ConVar";
    case NECRO_CON_TYPE_VAR:         return "TypeVar";
    case NECRO_CON_DATA_DECLARATION: return "Data Declaration";
    case NECRO_CON_TYPE_DECLARATION: return "Type Declaration";
    default:                         return "Undefined";
    }
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
    NecroAST_Node* node = (NecroAST_Node*)arena_alloc(&parser->ast->arena, sizeof(NecroAST_Node), arena_allow_realloc);
    node->source_loc = parser->tokens[parser->current_token > 0 ? (parser->current_token - 1) : 0].source_loc;
    return node;
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

    case NECRO_AST_LET_EXPRESSION:
        puts("(Let)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->let_expression.declarations), intern, depth + 1);
        if (ast_node->right_hand_side.declarations != null_local_ptr)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "in");
            print_ast_impl(ast, ast_get_node(ast, ast_node->let_expression.expression), intern, depth + 3);
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
        {
            const char* variable_string = necro_intern_get_string(intern, ast_node->variable.symbol);
            if (variable_string)
            {
                printf("(varid: %s, vtype: %s)\n", variable_string, var_type_string(ast_node->variable.var_type));
            }
            else
            {
                printf("(varid: \"\", vtype: %s)\n", var_type_string(ast_node->variable.var_type));
            }
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

    case NECRO_AST_LAMBDA:
        puts("\\(lambda)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->lambda.apats), intern, depth + 1);
        for (uint32_t i = 0;  i < (depth + 1); ++i)
        {
            printf(AST_TAB);
        }
        puts("->");
        print_ast_impl(ast, ast_get_node(ast, ast_node->lambda.expression), intern, depth + 2);
        break;

    case NECRO_AST_DO:
        puts("(do)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->do_statement.statement_list), intern, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        puts("([])");
        if (ast_node->expression_list.expressions != null_local_ptr)
            print_ast_impl(ast, ast_get_node(ast, ast_node->expression_list.expressions), intern, depth + 1);
        break;

    case NECRO_AST_TUPLE:
        puts("(tuple)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->expression_list.expressions), intern, depth + 1);
        break;

    case NECRO_AST_LIST_NODE:
        printf("\r"); // clear current line
        print_ast_impl(ast, ast_get_node(ast, ast_node->list.item), intern, depth);
        if (ast_node->list.next_item != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->list.next_item), intern, depth);
        }
        break;

    case NECRO_BIND_ASSIGNMENT:
        printf("(Bind: %s)\n", necro_intern_get_string(intern, ast_node->bind_assignment.variable_name));
        print_ast_impl(ast, ast_get_node(ast, ast_node->bind_assignment.expression), intern, depth + 1);
        break;

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        {
            switch(ast_node->arithmetic_sequence.type)
            {
            case NECRO_ARITHMETIC_ENUM_FROM:
                puts("(EnumFrom)");
                print_ast_impl(ast, ast_get_node(ast, ast_node->arithmetic_sequence.from), intern, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_TO:
                puts("(EnumFromTo)");
                print_ast_impl(ast, ast_get_node(ast, ast_node->arithmetic_sequence.from), intern, depth + 1);
                print_ast_impl(ast, ast_get_node(ast, ast_node->arithmetic_sequence.to), intern, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_THEN_TO:
                puts("(EnumFromThenTo)");
                print_ast_impl(ast, ast_get_node(ast, ast_node->arithmetic_sequence.from), intern, depth + 1);
                print_ast_impl(ast, ast_get_node(ast, ast_node->arithmetic_sequence.then), intern, depth + 1);
                print_ast_impl(ast, ast_get_node(ast, ast_node->arithmetic_sequence.to), intern, depth + 1);
                break;
            default:
                assert(false);
                break;
            }
        }
        break;

    case NECRO_AST_CASE:
        puts("case");
        print_ast_impl(ast, ast_get_node(ast, ast_node->case_expression.expression), intern, depth + 1);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("of");
        print_ast_impl(ast, ast_get_node(ast, ast_node->case_expression.alternatives), intern, depth + 1);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        printf("\r"); // clear current line
        print_ast_impl(ast, ast_get_node(ast, ast_node->case_alternative.pat), intern, depth + 0);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("->");
        print_ast_impl(ast, ast_get_node(ast, ast_node->case_alternative.body), intern, depth + 1);
        break;

    case NECRO_AST_CONID:
    {
        const char* con_string = necro_intern_get_string(intern, ast_node->conid.symbol);
        if (con_string)
            printf("(conid: %s, ctype: %s)\n", con_string, con_type_string(ast_node->conid.con_type));
        else
            printf("(conid: \"\", ctype: %s)\n", con_type_string(ast_node->conid.con_type));
        break;
    }

    case NECRO_AST_DATA_DECLARATION:
        puts("(data)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->data_declaration.simpletype), intern, depth + 1);
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts(" = ");
        print_ast_impl(ast, ast_get_node(ast, ast_node->data_declaration.constructor_list), intern, depth + 1);
        break;

    case NECRO_AST_SIMPLE_TYPE:
        puts("(simple type)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->simple_type.type_con), intern, depth + 1);
        if (ast_node->simple_type.type_var_list != null_local_ptr)
            print_ast_impl(ast, ast_get_node(ast, ast_node->simple_type.type_var_list), intern, depth + 2);
        break;

    case NECRO_AST_CONSTRUCTOR:
        puts("(constructor)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->constructor.conid), intern, depth + 1);
        if (ast_node->constructor.arg_list != null_local_ptr)
            print_ast_impl(ast, ast_get_node(ast, ast_node->constructor.arg_list), intern, depth + 1);
        break;

    case NECRO_AST_TYPE_APP:
        puts("(type app)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_app.ty), intern, depth + 1);
        if (ast_node->type_app.next_ty != null_local_ptr)
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_app.next_ty), intern, depth + 1);
        break;

    case NECRO_AST_BIN_OP_SYM:
        printf("(%s)\n", necro_intern_get_string(intern, ast_get_node(ast, ast_node->bin_op_sym.op)->conid.symbol));
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op_sym.left), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op_sym.right), intern, depth + 1);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        printf("\r");
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_signature.var), intern, depth + 0);
        for (uint32_t i = 0;  i < depth + 1; ++i) printf(AST_TAB);
        puts("::");
        if (ast_node->type_signature.context != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_signature.context), intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_signature.type), intern, depth + 1);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        printf("\r");
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_context.conid), intern, depth + 0);
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_context.varid), intern, depth + 0);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        puts("(class)");
        if (ast_node->type_class_declaration.context != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.context), intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.tycls), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.tyvar), intern, depth + 1);
        if (ast_node->type_class_declaration.declarations != null_local_ptr)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.declarations), intern, depth + 2);
        }
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        puts("(instance)");
        if (ast_node->type_class_instance.context != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_instance.context), intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_instance.qtycls), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_instance.inst), intern, depth + 1);
        if (ast_node->type_class_declaration.declarations != null_local_ptr)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.declarations), intern, depth + 2);
        }
        break;

    case NECRO_AST_FUNCTION_TYPE:
        puts("(");
        print_ast_impl(ast, ast_get_node(ast, ast_node->function_type.type), intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts("->");
        print_ast_impl(ast, ast_get_node(ast, ast_node->function_type.next_on_arrow), intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts(")");
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

NecroAST_LocalPtr write_error_and_restore(NecroParser* parser, NecroParser_Snapshot snapshot, const char* error_message, ...);

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
NecroAST_LocalPtr parse_tuple(NecroParser* parser);
NecroAST_LocalPtr parse_l_expression(NecroParser* parser);
NecroAST_LocalPtr parse_if_then_else_expression(NecroParser* parser);
NecroAST_LocalPtr parse_constant(NecroParser* parser);
NecroAST_LocalPtr parse_unary_operation(NecroParser* parser);
NecroAST_LocalPtr parse_binary_operation(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr);
NecroAST_LocalPtr parse_end_of_stream(NecroParser* parser);
NecroAST_LocalPtr parse_top_declarations(NecroParser* parser);
NecroAST_LocalPtr parse_declarations(NecroParser* parser);
NecroAST_LocalPtr parse_simple_assignment(NecroParser* parser);
NecroAST_LocalPtr parse_apats_assignment(NecroParser* parser);
NecroAST_LocalPtr parse_apats(NecroParser* parser);
NecroAST_LocalPtr parse_lambda(NecroParser* parser);
NecroAST_LocalPtr parse_right_hand_side(NecroParser* parser);
NecroAST_LocalPtr parse_do(NecroParser* parser);
NecroAST_LocalPtr parse_expression_list(NecroParser* parser);
NecroAST_LocalPtr parse_arithmetic_sequence(NecroParser* parser);
NecroAST_LocalPtr parse_case(NecroParser* parser);
NecroAST_LocalPtr parse_pat(NecroParser* parser);

NecroAST_LocalPtr parse_con(NecroParser* parser,  NECRO_CON_TYPE var_type);
NecroAST_LocalPtr parse_gcon(NecroParser* parser, NECRO_CON_TYPE var_type);
NecroAST_LocalPtr parse_qcon(NecroParser* parser, NECRO_CON_TYPE var_type);
NecroAST_LocalPtr parse_list_pat(NecroParser* parser);
NecroAST_LocalPtr parse_tuple_pattern(NecroParser* parser);
NecroAST_LocalPtr parse_top_level_data_declaration(NecroParser* parser);
NecroAST_LocalPtr parse_declaration(NecroParser* parser);
NecroAST_LocalPtr parse_type_signature(NecroParser* parser, NECRO_SIG_TYPE sig_type);
NecroAST_LocalPtr parse_type_class_declaration(NecroParser* parser);
NecroAST_LocalPtr parse_type_class_instance(NecroParser* parser);

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
            // snprintf(
                // parser->error_message,
                // MAX_ERROR_MESSAGE_SIZE,
            necro_error(&parser->error, look_ahead_token->source_loc,
                "Parsing ended without error, but not all tokens were consumed. This is likely a parser bug.\n Parsing stopped at the token %s (number: %d), which is token number %zu.",
                // "Parsing ended at line %zu, character %zu. Parsing stopped at the token %s, which is token number %zu.",
                // look_ahead_token->line_number,
                // look_ahead_token->character_number,
                necro_lex_token_type_string(look_ahead_token->token),
                look_ahead_token->token,
                parser->current_token);
        }
    }

    *out_root_node_ptr = null_local_ptr;
    return NECRO_ERROR;
}

NecroAST_LocalPtr parse_top_declarations(NecroParser* parser)
{
    // if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || peek_token_type(parser) == NECRO_LEX_SEMI_COLON || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot               = snapshot_parser(parser);
    NecroAST_LocalPtr    declarations_local_ptr = null_local_ptr;

    // declaration
    if ((declarations_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        // declarations_local_ptr = parse_declarations(parser);
        declarations_local_ptr = parse_declaration(parser);
    }

    // data declaration
    if ((declarations_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declarations_local_ptr = parse_top_level_data_declaration(parser);
    }

    // type class declaration
    if ((declarations_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declarations_local_ptr = parse_type_class_declaration(parser);
    }

    // type class instance
    if ((declarations_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declarations_local_ptr = parse_type_class_instance(parser);
    }

    if ((declarations_local_ptr != null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        NecroAST_LocalPtr next_top_decl = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser);
            next_top_decl = parse_top_declarations(parser);
        }
        NecroAST_LocalPtr top_decl_local_ptr         = null_local_ptr;
        NecroAST_Node* top_decl_node                 = ast_alloc_node_local_ptr(parser, &top_decl_local_ptr);
        top_decl_node->top_declaration.declaration   = declarations_local_ptr;
        top_decl_node->top_declaration.next_top_decl = next_top_decl;
        top_decl_node->type                          = NECRO_AST_TOP_DECL;
        return top_decl_local_ptr;
    }

    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Failed to parse any top level declarations.");
        // necro_error(&parser->error, (NecroSourceLoc){ 0, 0, 0 }, "Failed to parse any top level declaration");
        // necro_error(&parser->error, peek_token(parser)->source_loc, "Failed parsing top level declaration");
        // parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        return null_local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_type_signature(parser, NECRO_SIG_DECLARATION);
    }

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_simple_assignment(parser);
    }

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_apats_assignment(parser);
    }

    return declaration_local_ptr;
}

NecroAST_LocalPtr parse_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        // token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr declaration_local_ptr = parse_declaration(parser);

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
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Failed to parse declaration, at line %zu, character %zu. Expected a \'}\' token but instead found %s",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number,
                //     necro_lex_token_type_string(look_ahead_token->token));
                necro_error(&parser->error, look_ahead_token->source_loc, "Expected \'}\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
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
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Right hand side of assignment failed to parse at line %zu, character %zu.",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number);
            necro_error(&parser->error, look_ahead_token->source_loc, "Right hand side of assignment failed to parse");
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
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Right hand side of assignment failed to parse at line %zu, character %zu.",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number);
            necro_error(&parser->error, look_ahead_token->source_loc, "Right hand side of assignment failed to parse");
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
            if (declarations_local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "\'where\' clause failed to parse. Expected declarations after \'where\' token on line %zu, character %zu. "
                //     "Failed to parse declaration starting at line %zu, character %zu",
                //     where_token->line_number,
                //     where_token->character_number,
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number);
                necro_error(&parser->error, look_ahead_token->source_loc, "\'where\' clause failed to parse. Expected declarations after \'where\'");
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
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Right hand side expression failed to parse at line %zu, character %zu.",
        //     look_ahead_token->line_number,
        //     look_ahead_token->character_number);
        necro_error(&parser->error, look_ahead_token->source_loc, "Right hand side expression failed to parse");
        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_let_expression(NecroParser* parser)
{
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LET ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume 'let' token

    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACE)
    {
        look_ahead_token = peek_token(parser);
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "\'let\' expression failed to parse. Expected \'{\' token on line %zu, character %zu, but instead found %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));

        necro_error(&parser->error, look_ahead_token->source_loc, "\'let\' expression failed to parse. Expected \'{\' found %s", necro_lex_token_type_string(look_ahead_token->token));
        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;

        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    consume_token(parser); // consume '{' token

    NecroAST_LocalPtr declarations_local_ptr = parse_declarations(parser);
    if (declarations_local_ptr != null_local_ptr)
    {
        bool consumed_right_brace = false;
        if (peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
        {
            consume_token(parser);
            consumed_right_brace = true;
        }

        if (peek_token_type(parser) == NECRO_LEX_IN)
        {
            NecroLexToken* in_token = peek_token(parser);
            consume_token(parser); // consume 'in' token
            NecroAST_LocalPtr expression_local_ptr = parse_expression(parser);
            if (expression_local_ptr == null_local_ptr &&
                parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "\'let in\' expression failed to parse. Expected expression after \'in\' token on line %zu, character %zu. "
                //     "Failed to parse expression starting at line %zu, character %zu",
                //     in_token->line_number,
                //     in_token->character_number,
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number);
                necro_error(&parser->error, look_ahead_token->source_loc, "\'let in\' expression failed to parse. Expected expression after \'in\'");
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }

            if (!consumed_right_brace)
            {
                if (peek_token_type(parser) != NECRO_LEX_SEMI_COLON)
                {
                    look_ahead_token = peek_token(parser);
                    // snprintf(
                    //     parser->error_message,
                    //     MAX_ERROR_MESSAGE_SIZE,
                    //     "\'let\' expression failed to parse. Expected \';\' token on line %zu, character %zu, but instead found %s",
                    //     look_ahead_token->line_number,
                    //     look_ahead_token->character_number,
                    //     necro_lex_token_type_string(look_ahead_token->token));
                    necro_error(&parser->error, look_ahead_token->source_loc, "\'let\' expression failed to parse. Expected \';\' but instead found %s", necro_lex_token_type_string(look_ahead_token->token));
                    parser->descent_state = NECRO_DESCENT_PARSE_ERROR;

                    restore_parser(parser, snapshot);
                    return null_local_ptr;
                }

                consume_token(parser); // consume ';' token

                if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                {
                    look_ahead_token = peek_token(parser);
                    // snprintf(
                    //     parser->error_message,
                    //     MAX_ERROR_MESSAGE_SIZE,
                    //     "\'let\' expression failed to parse. Expected \'}\' token on line %zu, character %zu, but instead found %s",
                    //     look_ahead_token->line_number,
                    //     look_ahead_token->character_number,
                    //     necro_lex_token_type_string(look_ahead_token->token));
                    necro_error(&parser->error, look_ahead_token->source_loc, "\'let\' expression failed to parse. Expected \'}\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
                    parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
                    restore_parser(parser, snapshot);
                    return null_local_ptr;
                }

                consume_token(parser); // consume '}' token
            }

            if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                NecroAST_LocalPtr let_local_ptr = null_local_ptr;
                NecroAST_Node* let_node = ast_alloc_node_local_ptr(parser, &let_local_ptr);
                let_node->let_expression.expression = expression_local_ptr;
                let_node->let_expression.declarations = declarations_local_ptr;
                let_node->type = NECRO_AST_LET_EXPRESSION;
                return let_local_ptr;
            }
        }
        else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            look_ahead_token = peek_token(parser);
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "\'let\' expression failed to parse. Expected \'in\' token on line %zu, character %zu, but instead found %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "\'let\' expression failed to parse. Expected \'in\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }
    }
    else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Let expression failed to parse at line %zu, character %zu.",
        //     look_ahead_token->line_number,
        //     look_ahead_token->character_number);
        necro_error(&parser->error, look_ahead_token->source_loc, "\'let\' expression failed to parse.");
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

NecroAST_LocalPtr parse_variable(NecroParser* parser, NECRO_VAR_TYPE var_type)
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
        varid_node->variable.symbol   = variable_token->symbol;
        varid_node->variable.var_type = var_type;
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
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Failed to parse parenthetical variable symbol because there was no closing bracket at line %zu, character %zu. Failed beginning with token %s",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number,
                //     necro_lex_token_type_string(look_ahead_token->token));
                necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse parenthetical variable symbol because there was no closing bracket. Failed beginning with token %s", necro_lex_token_type_string(look_ahead_token->token));
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
            else
            {
                consume_token(parser); // consume ")" token
                NecroAST_LocalPtr varsym_local_ptr = null_local_ptr;
                NecroAST_Node* varsym_node = ast_alloc_node_local_ptr(parser, &varsym_local_ptr);
                varsym_node->type = NECRO_AST_VARIABLE;
                varsym_node->variable.symbol   = sym_variable_token->symbol;
                varsym_node->variable.var_type = var_type;
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
        local_ptr = parse_variable(parser, NECRO_VAR_VAR);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_gcon(parser, NECRO_CON_VAR);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_constant(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_parenthetical_expression(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_tuple(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_expression_list(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_arithmetic_sequence(parser);
    }

    if (local_ptr != null_local_ptr)
    {
        return local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_function_expression_go(NecroParser* parser, NecroAST_LocalPtr left)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (left == null_local_ptr)
        return null_local_ptr;

    NecroParser_Snapshot snapshot    = snapshot_parser(parser);
    NecroAST_LocalPtr    aexpression = parse_application_expression(parser);
    if (aexpression == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return left;
    }

    NecroAST_LocalPtr ptr  = null_local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &ptr);
    node->type             = NECRO_AST_FUNCTION_EXPRESSION;
    node->type_app.ty      = left;
    node->type_app.next_ty = aexpression;
    return parse_function_expression_go(parser, ptr);
}

NecroAST_LocalPtr parse_function_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot    = snapshot_parser(parser);
    NecroAST_LocalPtr    aexpression = parse_function_expression_go(parser, parse_application_expression(parser));
    if (aexpression == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    return aexpression;

    // old version, wrong associativity
    // NecroAST_LocalPtr aexpression = parse_application_expression(parser);
    // if (aexpression != null_local_ptr)
    // {
    //     NecroAST_LocalPtr next_fexpression = parse_function_expression(parser);
    //     if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    //         return null_local_ptr;
    //     // if (next_fexpression != null_local_ptr)
    //     // {
    //         NecroAST_LocalPtr fexpression_local_ptr = null_local_ptr;
    //         NecroAST_Node* fexpression_node = ast_alloc_node_local_ptr(parser, &fexpression_local_ptr);
    //         fexpression_node->type = NECRO_AST_FUNCTION_EXPRESSION;
    //         fexpression_node->fexpression.aexp = aexpression;
    //         fexpression_node->fexpression.next_fexpression = next_fexpression;
    //         return fexpression_local_ptr;
    //     // }
    //     // else
    //     // {
    //     //     return aexpression;
    //     // }
    // }

    // restore_parser(parser, snapshot);
    // return null_local_ptr;
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

    // var
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_variable(parser, NECRO_VAR_DECLARATION);
    }

    // gcon
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_gcon(parser, NECRO_CON_VAR);
    }

    // _
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_wildcard(parser);
    }

    // Numeric literal
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        if (peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL || peek_token_type(parser) == NECRO_LEX_FLOAT_LITERAL)
        {
            local_ptr = parse_constant(parser);
        }
    }

    // (pat)
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        NecroParser_Snapshot snapshot = snapshot_parser(parser);
        if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
        {
            consume_token(parser);
            local_ptr = parse_pat(parser);
            if (local_ptr != null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                if (peek_token_type(parser) == NECRO_LEX_RIGHT_PAREN)
                {

                    consume_token(parser);
                }
                else
                {
                    //     return write_error_and_restore(parser, snapshot, "Expected ')' at end of pattern, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
                    local_ptr = null_local_ptr;
                    restore_parser(parser, snapshot);
                }
            }
            else
            {
                local_ptr = null_local_ptr;
                restore_parser(parser, snapshot);
            }
        }
    }

    // Tuple pattern (pat1, ... , patk)
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_tuple_pattern(parser);
    }

    // list pattern [pat1, ... , patk]
    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_list_pat(parser);
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
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Failed to parse expression after \'(\' token at line %zu, character %zu. Failed beginning with token %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse expression after \'(\'. Failed beginning with token %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        if (look_ahead_token->token == NECRO_LEX_COMMA)
        {
            // if a comma is next then this may be a tuple, keep trying to parse
            restore_parser(parser, snapshot);
            return null_local_ptr;
        }

        // otherwise we expect a closing parentheses. This is a fatal parse error.
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Failed to parse parenthetical expression because there was no closing bracket at line %zu, character %zu. Failed beginning with token %s",
        //     look_ahead_token->line_number,
        //     look_ahead_token->character_number,
        //     necro_lex_token_type_string(look_ahead_token->token));
        necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse parenthetical expression because there was no closing bracket. Failed beginning with token %s", necro_lex_token_type_string(look_ahead_token->token));
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
    case NECRO_LEX_COLON:
        return NECRO_BIN_OP_COLON;
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
    case NECRO_LEX_NOT_EQUALS:
        return NECRO_BIN_OP_NOT_EQUALS;
    case NECRO_LEX_AND:
        return NECRO_BIN_OP_AND;
    case NECRO_LEX_OR:
        return NECRO_BIN_OP_OR;
    case NECRO_LEX_DOT:
        return NECRO_BIN_OP_DOT;
    case NECRO_LEX_DOLLAR:
        return NECRO_BIN_OP_DOLLAR;
    case NECRO_LEX_BIND_RIGHT:
        return NECRO_BIN_OP_BIND_RIGHT;
    case NECRO_LEX_BIND_LEFT:
        return NECRO_BIN_OP_BIND_LEFT;
    case NECRO_LEX_DOUBLE_EXCLAMATION:
        return NECRO_BIN_OP_DOUBLE_EXCLAMATION;
    case NECRO_LEX_APPEND:
        return NECRO_BIN_OP_APPEND;
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

NecroAST_LocalPtr parse_l_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_do(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_let_expression(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_if_then_else_expression(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_function_expression(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_lambda(parser);
    }

    if (local_ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        local_ptr = parse_case(parser);
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
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Failed to parse expression after \'if\' token at line %zu, character %zu. Failed beginning with token %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse expression after \'if\'. Failed beginning with token %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_THEN)
    {
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Failed to parse \'if-then-else\' expression at line %zu, character %zu. Expected a \'then\' token, but found %s",
        //     look_ahead_token->line_number,
        //     look_ahead_token->character_number,
        //     necro_lex_token_type_string(look_ahead_token->token));
        necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse \'if-then-else\' expression. Expected a \'then\' token, but found %s", necro_lex_token_type_string(look_ahead_token->token));
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
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Failed to parse expression after \'then\' token in \'if-then-else\' expression, at line %zu, character %zu. Failed beginning with token %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse expression after \'then\' token in \'if-then-else\' expression. Failed beginning with token %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    look_ahead_token = peek_token(parser);
    if (peek_token_type(parser) != NECRO_LEX_ELSE)
    {
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Failed to parse \'if-then-else\' expression at line %zu, character %zu. Expected an \'else\' token, but found %s",
        //     look_ahead_token->line_number,
        //     look_ahead_token->character_number,
        //     necro_lex_token_type_string(look_ahead_token->token));
        necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse \'if-then-else\' expression. Expected an \'else\' token, but found %s", necro_lex_token_type_string(look_ahead_token->token));
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
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Failed to parse expression after \'else\' token in \'if-then-else\' expression, at line %zu, character %zu. Failed beginning with token %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "Failed to parse expression after \'else\' token in \'if-then-else\' expression. Failed beginning with token %s", necro_lex_token_type_string(look_ahead_token->token));
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

NecroAST_LocalPtr parse_lambda(NecroParser* parser)
{
    const NecroLexToken* backslash_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = backslash_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type != NECRO_LEX_BACK_SLASH ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume backslash token

    NecroAST_LocalPtr apats_local_ptr = parse_apats(parser);

    if (apats_local_ptr != null_local_ptr)
    {
        const NECRO_LEX_TOKEN_TYPE next_token = peek_token_type(parser);
        if (next_token == NECRO_LEX_RIGHT_ARROW)
        {
            consume_token(parser); // consume '->' token
            NecroLexToken* look_ahead_token = peek_token(parser);
            NecroAST_LocalPtr expression_local_ptr = parse_expression(parser);
            if (expression_local_ptr != null_local_ptr)
            {
                NecroAST_LocalPtr lambda_local_ptr = null_local_ptr;
                NecroAST_Node* lambda_node = ast_alloc_node_local_ptr(parser, &lambda_local_ptr);
                lambda_node->lambda.apats = apats_local_ptr;
                lambda_node->lambda.expression = expression_local_ptr;
                lambda_node->type = NECRO_AST_LAMBDA;
                return lambda_local_ptr;
            }
            else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Lambda expression failed to parse at line %zu, character %zu.",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number);
                necro_error(&parser->error, look_ahead_token->source_loc, "Lambda expression failed to parse.");
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
        }
        else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroLexToken* look_ahead_token = peek_token(parser);
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Lambda expression failed to parse at line %zu, character %zu. Expected \'->\' token, but found %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(next_token));

            necro_error(&parser->error, look_ahead_token->source_loc, "Lambda expression failed to parse. Expected \'->\' token, but found %s", necro_lex_token_type_string(next_token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }
    }
    else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        NecroLexToken* look_ahead_token = peek_token(parser);
        // snprintf(
        //     parser->error_message,
        //     MAX_ERROR_MESSAGE_SIZE,
        //     "Lambda patterns failed to parse at line %zu, character %zu.",
        //     look_ahead_token->line_number,
        //     look_ahead_token->character_number);
        necro_error(&parser->error, look_ahead_token->source_loc, "Lambda patterns failed to parse.");
        parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

typedef NecroAST_LocalPtr (*ParseFunc)(NecroParser* parser);

 NecroAST_LocalPtr parse_list(NecroParser* parser, NECRO_LEX_TOKEN_TYPE token_divider, ParseFunc parse_func)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr item_local_ptr = parse_func(parser);
    if (item_local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr next_item_local_ptr = null_local_ptr;
        if (peek_token_type(parser) == token_divider)
        {
            consume_token(parser); // consume divider token
            next_item_local_ptr = parse_list(parser, token_divider, parse_func);
        }

        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroAST_LocalPtr list_local_ptr = null_local_ptr;
            NecroAST_Node* list_node = ast_alloc_node_local_ptr(parser, &list_local_ptr);
            list_node->type = NECRO_AST_LIST_NODE;
            list_node->list.item = item_local_ptr;
            list_node->list.next_item = next_item_local_ptr;
            return list_local_ptr;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_do_item(NecroParser* parser)
{
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    if (peek_token_type(parser) == NECRO_LEX_IDENTIFIER)
    {
        const NecroLexToken* variable_token = peek_token(parser);
        consume_token(parser); // Consume variable token
        if (variable_token->token == NECRO_LEX_IDENTIFIER && peek_token_type(parser) == NECRO_LEX_LEFT_ARROW)
        {
            consume_token(parser); // consume left arrow
            NecroAST_LocalPtr expression_local_ptr = parse_expression(parser);
            if (expression_local_ptr != null_local_ptr)
            {
                NecroAST_LocalPtr bind_assignment_local_ptr = null_local_ptr;
                NecroAST_Node* bind_assignment_node = ast_alloc_node_local_ptr(parser, &bind_assignment_local_ptr);
                bind_assignment_node->type = NECRO_BIND_ASSIGNMENT;
                bind_assignment_node->bind_assignment.variable_name = variable_token->symbol;
                bind_assignment_node->bind_assignment.expression = expression_local_ptr;
                return bind_assignment_local_ptr;
            }
            else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                const NecroLexToken* look_ahead_token = peek_token(parser);
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Bind failed in to parse at line %zu, character %zu",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number);
                necro_error(&parser->error, look_ahead_token->source_loc, "Bind failed to parse.");
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
        }
    }
    else if (peek_token_type(parser) == NECRO_LEX_LET)
    {
        consume_token(parser); // Consume 'LET'
        if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACE)
            return write_error_and_restore(parser, snapshot, "Expected \'{\', but found %s", necro_lex_token_type_string(peek_token_type(parser)));
        consume_token(parser); // Consume '{'
        NecroAST_LocalPtr declarations = parse_declarations_list(parser);
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
            return write_error_and_restore(parser, snapshot, "Expected \'}\', but found %s", necro_lex_token_type_string(peek_token_type(parser)));
        consume_token(parser); // Consume '}'
        return declarations;
    }

    restore_parser(parser, snapshot);
    return parse_expression(parser);
}

NecroAST_LocalPtr parse_do(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE do_token_type = peek_token_type(parser);
    if (do_token_type != NECRO_LEX_DO ||
        do_token_type == NECRO_LEX_END_OF_STREAM ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume do token

    const NECRO_LEX_TOKEN_TYPE l_curly_token_type = peek_token_type(parser);
    if (l_curly_token_type == NECRO_LEX_LEFT_BRACE)
    {
        consume_token(parser); // consume left curly brace
        NecroAST_LocalPtr statement_list_local_ptr = parse_list(parser, NECRO_LEX_SEMI_COLON, parse_do_item);
        if (statement_list_local_ptr != null_local_ptr)
        {
            NecroLexToken* look_ahead_token = peek_token(parser);
            if (look_ahead_token->token == NECRO_LEX_SEMI_COLON)
            {
                consume_token(parser); // consume semicolon token
            }

            look_ahead_token = peek_token(parser);
            if (look_ahead_token->token == NECRO_LEX_RIGHT_BRACE)
            {
                consume_token(parser); // consume right curly brace
                NecroAST_LocalPtr do_local_ptr = null_local_ptr;
                NecroAST_Node* do_node = ast_alloc_node_local_ptr(parser, &do_local_ptr);
                do_node->type = NECRO_AST_DO;
                do_node->do_statement.statement_list = statement_list_local_ptr;
                return do_local_ptr;
            }
            else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Do statement failed to parse at line %zu, character %zu. Expected closing curly brace but found %s",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number,
                //     necro_lex_token_type_string(look_ahead_token->token));
                necro_error(&parser->error, look_ahead_token->source_loc, "Do statement failed to parse. Expected closing curly brace but found %s",necro_lex_token_type_string(look_ahead_token->token) );
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_expression_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE brace_token_type = peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACKET ||
        brace_token_type == NECRO_LEX_END_OF_STREAM ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume '[' token

    NecroAST_LocalPtr statement_list_local_ptr = parse_list(parser, NECRO_LEX_COMMA, parse_expression);
    if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        NecroLexToken* look_ahead_token = peek_token(parser);
        if (look_ahead_token->token == NECRO_LEX_RIGHT_BRACKET)
        {
            consume_token(parser); // consume ']' token
            NecroAST_LocalPtr list_local_ptr = null_local_ptr;
            NecroAST_Node* list_node = ast_alloc_node_local_ptr(parser, &list_local_ptr);
            list_node->type = NECRO_AST_EXPRESSION_LIST;
            list_node->expression_list.expressions = statement_list_local_ptr;
            return list_local_ptr;
        }
        else if (look_ahead_token->token != NECRO_LEX_DOUBLE_DOT &&
                parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "List expression failed to parse at line %zu, character %zu. Expected closing bracket but found %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "List expression failed to parse. Expected closing bracket but found %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_arithmetic_sequence(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE brace_token_type = peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACKET ||
        brace_token_type == NECRO_LEX_END_OF_STREAM ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume '[' token

    NecroAST_LocalPtr from = parse_expression(parser);
    NecroAST_LocalPtr then = null_local_ptr;
    NecroAST_LocalPtr to = null_local_ptr;

    if (from == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    if (peek_token_type(parser) == NECRO_LEX_COMMA)
    {
        consume_token(parser); // consume ','
        then = parse_expression(parser);

        if (then == null_local_ptr)
        {
            if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Arithmetic expression failed to parse at line %zu, character %zu. \'then\' expression failed to parse.",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number);
                necro_error(&parser->error, look_ahead_token->source_loc, "Arithmetic expression failed to parse. \'then\' expression failed to parse.", necro_lex_token_type_string(look_ahead_token->token));
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }

            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
    }

    if (peek_token_type(parser) != NECRO_LEX_DOUBLE_DOT)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    consume_token(parser); // consume '..'

    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        to = parse_expression(parser);
        if (to == null_local_ptr)
        {
            if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
            {
                NecroLexToken* look_ahead_token = peek_token(parser);
                // snprintf(
                //     parser->error_message,
                //     MAX_ERROR_MESSAGE_SIZE,
                //     "Arithmetic expression failed to parse at line %zu, character %zu. \'to\' expression failed to parse.",
                //     look_ahead_token->line_number,
                //     look_ahead_token->character_number);
                necro_error(&parser->error, look_ahead_token->source_loc, "Arithmetic expression failed to parse at line. \'to\' expression failed to parse.");
                parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
            }

            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
    }

    if (then != null_local_ptr && to == null_local_ptr)
    {
        // this means we're probably just a normal list expression
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            NecroLexToken* look_ahead_token = peek_token(parser);
            // snprintf(
            //     parser->error_message,
            //     MAX_ERROR_MESSAGE_SIZE,
            //     "Arithmetic expression failed to parse at line %zu, character %zu. Expected \']\' but found %s",
            //     look_ahead_token->line_number,
            //     look_ahead_token->character_number,
            //     necro_lex_token_type_string(look_ahead_token->token));
            necro_error(&parser->error, look_ahead_token->source_loc, "Arithmetic expression failed to parse. Expected \']\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }

        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    consume_token(parser); // consume ']' token

    if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        assert(from != null_local_ptr);

        NecroAST_LocalPtr arithmetic_local_ptr = null_local_ptr;
        NecroAST_Node* arithmetic_node = ast_alloc_node_local_ptr(parser, &arithmetic_local_ptr);
        arithmetic_node->type = NECRO_AST_ARITHMETIC_SEQUENCE;
        arithmetic_node->arithmetic_sequence.from = from;
        arithmetic_node->arithmetic_sequence.then = then;
        arithmetic_node->arithmetic_sequence.to = to;

        if (to == null_local_ptr)
        {
            if (then == null_local_ptr)
            {
                arithmetic_node->arithmetic_sequence.type = NECRO_ARITHMETIC_ENUM_FROM;
            }
            else
            {
                assert(false);
            }
        }
        else if (then == null_local_ptr)
        {
            arithmetic_node->arithmetic_sequence.type = NECRO_ARITHMETIC_ENUM_FROM_TO;
        }
        else
        {
            arithmetic_node->arithmetic_sequence.type = NECRO_ARITHMETIC_ENUM_FROM_THEN_TO;
        }

        return arithmetic_local_ptr;
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_tuple(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type != NECRO_LEX_LEFT_PAREN ||
        token_type == NECRO_LEX_END_OF_STREAM ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume '(' token

    NecroAST_LocalPtr statement_list_local_ptr = parse_list(parser, NECRO_LEX_COMMA, parse_expression);
    if (statement_list_local_ptr != null_local_ptr)
    {
        NecroLexToken* look_ahead_token = peek_token(parser);
        if (look_ahead_token->token == NECRO_LEX_RIGHT_PAREN)
        {
            consume_token(parser); // consume ')' token
            NecroAST_LocalPtr list_local_ptr = null_local_ptr;
            NecroAST_Node* list_node = ast_alloc_node_local_ptr(parser, &list_local_ptr);
            list_node->type = NECRO_AST_TUPLE;
            list_node->expression_list.expressions = statement_list_local_ptr;
            return list_local_ptr;
        }
        else if (parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
        {
            necro_error(&parser->error, look_ahead_token->source_loc, "List expression failed to parse. Expected closing bracket but found %s", necro_lex_token_type_string(look_ahead_token->token));
            parser->descent_state = NECRO_DESCENT_PARSE_ERROR;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

//=====================================================
// Case / Patterns
//=====================================================

NecroAST_LocalPtr write_error_and_restore(NecroParser* parser, NecroParser_Snapshot snapshot, const char* error_message, ...)
{
    restore_parser(parser, snapshot);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    va_list args;
	va_start(args, error_message);

    necro_verror(&parser->error, peek_token(parser)->source_loc, error_message, args);
    parser->descent_state = NECRO_DESCENT_PARSE_ERROR;

    va_end(args);

    return null_local_ptr;
}

NecroAST_LocalPtr parse_case_alternative(NecroParser* parser)
{
    // Return on } or where
    if (peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE || peek_token_type(parser) == NECRO_LEX_WHERE || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot  = snapshot_parser(parser);

    // pat
    NecroAST_LocalPtr pat_local_ptr = parse_pat(parser);
    if (pat_local_ptr == null_local_ptr)
        return write_error_and_restore(parser, snapshot, "Case alternative failed to parse. Expected pattern, found %s", necro_lex_token_type_string(peek_token_type(parser)));

    // ->
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_ARROW)
        return write_error_and_restore(parser, snapshot, "Case alternative failed to parse, expected \'->\', found %s", necro_lex_token_type_string(peek_token_type(parser)));
    consume_token(parser); // consume right arrow

    // body expr
    NecroAST_LocalPtr body_local_ptr = parse_expression(parser);
    if (body_local_ptr == null_local_ptr)
        return write_error_and_restore(parser, snapshot, "Case alternative failed to parse. Expected expression, found %s", necro_lex_token_type_string(peek_token_type(parser)));

    // SUCCESS!
    NecroAST_LocalPtr case_alternative_local_ptr = null_local_ptr;
    NecroAST_Node*    case_alternative_node      = ast_alloc_node_local_ptr(parser, &case_alternative_local_ptr);
    case_alternative_node->type                  = NECRO_AST_CASE_ALTERNATIVE;
    case_alternative_node->case_alternative.pat  = pat_local_ptr;
    case_alternative_node->case_alternative.body = body_local_ptr;
    return case_alternative_local_ptr;
}

NecroAST_LocalPtr parse_case(NecroParser* parser)
{
    // case
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type != NECRO_LEX_CASE || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser); // consume 'CASE' token

    // expr
    NecroAST_LocalPtr case_expr_local_ptr = parse_expression(parser);
    if (case_expr_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // 'of'
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_OF)
        return write_error_and_restore(parser, snapshot, "Case expression failed to parse. Expected \'of\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
    consume_token(parser); // consume 'OF' token

    // '{'
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LEFT_BRACE)
        return write_error_and_restore(parser, snapshot, "Case expression failed to parse. Expected \'{\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
    consume_token(parser); // consume '{' token

    // alternatives
    NecroAST_LocalPtr alternative_list_local_ptr = parse_list(parser, NECRO_LEX_SEMI_COLON, parse_case_alternative);
    if (case_expr_local_ptr == null_local_ptr)
        return write_error_and_restore(parser, snapshot, "Case expression with no alternatives, instead found %s", necro_lex_token_type_string(look_ahead_token->token));

    // '}'
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
        return write_error_and_restore(parser, snapshot, "Case expression failed to parse. Expected \'}\' but found %s", necro_lex_token_type_string(look_ahead_token->token));
    consume_token(parser); // consume '}' token

    // SUCCESS!
    NecroAST_LocalPtr case_local_ptr        = null_local_ptr;
    NecroAST_Node*    case_node             = ast_alloc_node_local_ptr(parser, &case_local_ptr);
    case_node->type                         = NECRO_AST_CASE;
    case_node->case_expression.expression   = case_expr_local_ptr;
    case_node->case_expression.alternatives = alternative_list_local_ptr;
    return case_local_ptr;
}

NecroAST_LocalPtr parse_qconop(NecroParser* parser, NECRO_CON_TYPE con_type);
NecroAST_LocalPtr parse_pat_precedence(NecroParser* parser, int32_t precedence);

NecroAST_LocalPtr parse_oppat(NecroParser* parser, int32_t precedence)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // Left pat
    NecroAST_LocalPtr left = parse_pat_precedence(parser, precedence + 1);
    if (left == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // Op
    NecroAST_LocalPtr op = parse_qconop(parser, NECRO_VAR_VAR);
    if (op == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // Right pat
    NecroAST_LocalPtr right = parse_pat_precedence(parser, precedence + 1);
    if (right == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // Alloc and init node
    NecroAST_LocalPtr local_ptr = null_local_ptr;
    NecroAST_Node*    node      = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                  = NECRO_AST_BIN_OP_SYM;
    node->bin_op_sym.left       = left;
    node->bin_op_sym.op         = op;
    node->bin_op_sym.right      = right;
    return local_ptr;
}

NecroAST_LocalPtr parse_pat(NecroParser* parser)
{
    return parse_pat_precedence(parser, 0);
}

// Pat
NecroAST_LocalPtr parse_pat_precedence(NecroParser* parser, int32_t precedence)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr ptr = null_local_ptr;

    // oppat
    if (precedence < 1)
    {
        ptr = parse_oppat(parser, precedence);
        if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
            return null_local_ptr;
        if (ptr != null_local_ptr)
            return ptr;
    }

    // Literal
    if (peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL || peek_token_type(parser) == NECRO_LEX_FLOAT_LITERAL)
        return parse_constant(parser);

    // gcon apat1 ... apatk
    ptr = parse_gcon(parser, NECRO_CON_VAR);
    if (ptr != null_local_ptr)
    {
        NecroAST_LocalPtr apats_local_ptr = parse_apats(parser);
        if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
            return null_local_ptr;
        // if (apats_local_ptr == null_local_ptr)
        // {
        //     restore_parser(parser, snapshot);
        //     return null_local_ptr;
        // }
        NecroAST_LocalPtr constr_ptr      = null_local_ptr;
        NecroAST_Node*    constr_node     = ast_alloc_node_local_ptr(parser, &constr_ptr);
        // NecroAST_Node* con_node   = ast_get_node(parser->ast, ptr);
        // // NecroSymbol    con_symbol = con_node->conid.symbol;
        constr_node->type                 = NECRO_AST_CONSTRUCTOR;
        constr_node->constructor.conid    = ptr;
        constr_node->constructor.arg_list = apats_local_ptr;
        return constr_ptr;
    }

    // apat
    ptr = parse_apat(parser);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (ptr != null_local_ptr)
        return ptr;

    return null_local_ptr;

}

NecroAST_LocalPtr parse_gcon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr con_local_ptr = null_local_ptr;

    // ()
    if (peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol symbol = peek_token(parser)->symbol;
        consume_token(parser);
        NecroAST_Node* node  = ast_alloc_node_local_ptr(parser, &con_local_ptr);
        node->type           = NECRO_AST_CONID;
        node->conid.symbol   = symbol;
        node->conid.con_type = con_type;
    }
    // []
    else if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    {
        NecroSymbol symbol = peek_token(parser)->symbol;
        consume_token(parser);
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
        {
            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
        consume_token(parser);
        NecroAST_Node* node  = ast_alloc_node_local_ptr(parser, &con_local_ptr);
        node->type           = NECRO_AST_CONID;
        node->conid.symbol   = symbol;
        node->conid.con_type = con_type;
    }
    // Constructor
    else
    {
        con_local_ptr = parse_qcon(parser, con_type);
    }
    if (con_local_ptr == null_local_ptr)
        restore_parser(parser, snapshot);
    return con_local_ptr;
}

NecroAST_LocalPtr parse_consym(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    NecroAST_BinOpType bin_op_type = token_to_bin_op_type(peek_token_type(parser));
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
        return null_local_ptr;
    NecroAST_LocalPtr con_local_ptr = null_local_ptr;
    NecroAST_Node*    con_node      = ast_alloc_node_local_ptr(parser, &con_local_ptr);
    con_node->type                  = NECRO_AST_CONID;
    con_node->conid.symbol          = peek_token(parser)->symbol;
    con_node->conid.con_type        = con_type;
    // con_node->conid.symbol          = necro_intern_string(parser->intern, necro_lex_token_type_string(peek_token_type(parser)));
    consume_token(parser);
    return con_local_ptr;
}

NecroAST_LocalPtr parse_qconsym(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    // Save until we actuall have modules...
 	// [ modid . ] consym
    return parse_consym(parser, con_type);
}

// conid | (consym)
NecroAST_LocalPtr parse_con(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot         = snapshot_parser(parser);
    NecroAST_LocalPtr    con_local_ptr    = null_local_ptr;
    NecroLexToken*       look_ahead_token = peek_token(parser);

    if (look_ahead_token->token == NECRO_LEX_TYPE_IDENTIFIER)
    {
        NecroAST_Node* con_node  = ast_alloc_node_local_ptr(parser, &con_local_ptr);
        con_node->type           = NECRO_AST_CONID;
        con_node->conid.symbol   = look_ahead_token->symbol;
        con_node->conid.con_type = con_type;
        consume_token(parser);
        return con_local_ptr;
    }
    else if (look_ahead_token->token == NECRO_LEX_LEFT_PAREN)
    {
        consume_token(parser);
        con_local_ptr = parse_consym(parser, con_type);
        if (con_local_ptr != null_local_ptr)
        {
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            {
                restore_parser(parser, snapshot);
                return null_local_ptr;
            }
            consume_token(parser);
            return con_local_ptr;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroAST_LocalPtr parse_qcon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    // Save until we actuall have modules...
 	// [ modid . ] conid
    return parse_con(parser, con_type);
}

// Is this needed!?
// // gconsym 	-> 	: | qconsym
// NecroAST_LocalPtr parse_gconsym(NecroParser* parser)
// {
//     if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
//         return null_local_ptr;
//     // NecroParser_Snapshot snapshot         = snapshot_parser(parser);
//     // NecroAST_LocalPtr    con_local_ptr    = null_local_ptr;
//     // NecroLexToken*       look_ahead_token = peek_token(parser);
//     if (peek_token_type(parser) == NECRO_LEX_COLON)
//     {
//     }
// }

// qconop 	-> 	gconsym | `qcon`
NecroAST_LocalPtr parse_qconop(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot      = snapshot_parser(parser);
    NecroAST_LocalPtr    con_local_ptr = null_local_ptr;

    con_local_ptr = parse_consym(parser, con_type);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (con_local_ptr != null_local_ptr)
        return con_local_ptr;

    if (peek_token_type(parser) != NECRO_LEX_ACCENT)
        return null_local_ptr;
    consume_token(parser);
    con_local_ptr = parse_qcon(parser, con_type);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR || con_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    if (peek_token_type(parser) != NECRO_LEX_ACCENT)
        write_error_and_restore(parser, snapshot, "Expected \'`\' in function operator expression, but found %s", necro_lex_token_type_string(peek_token_type(parser)));
    consume_token(parser);
    return con_local_ptr;
}

NecroAST_LocalPtr parse_list_pat(NecroParser* parser)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // [
    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
        return null_local_ptr;
    consume_token(parser);

    // Pats
    NecroAST_LocalPtr list_local_ptr = parse_list(parser, NECRO_LEX_COMMA, parse_pat);
    if (list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
        // return write_error_and_restore(parser, snapshot, "Case expression with no alternatives, instead found %s", necro_lex_token_type_string(look_ahead_token->token));
    }

    // ]
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // SUCCESS!
    NecroAST_LocalPtr local_ptr       = null_local_ptr;
    NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                        = NECRO_AST_EXPRESSION_LIST;
    node->expression_list.expressions = list_local_ptr;
    return local_ptr;
}

NecroAST_LocalPtr parse_tuple_pattern(NecroParser* parser)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // (
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
        return null_local_ptr;
    consume_token(parser);

    // Pats
    NecroAST_LocalPtr list_local_ptr = parse_list(parser, NECRO_LEX_COMMA, parse_pat);
    if (list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
        // return write_error_and_restore(parser, snapshot, "Case expression with no alternatives, instead found %s", necro_lex_token_type_string(look_ahead_token->token));
    }

    // )
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // Success!
    NecroAST_LocalPtr local_ptr       = null_local_ptr;
    NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                        = NECRO_AST_TUPLE;
    node->expression_list.expressions = list_local_ptr;
    return local_ptr;
}

//=====================================================
// Types
//=====================================================
NecroAST_LocalPtr parse_tycon(NecroParser* parser, NECRO_CON_TYPE con_type);
NecroAST_LocalPtr parse_tyvar(NecroParser* parser, NECRO_VAR_TYPE var_type);
NecroAST_LocalPtr parse_btype(NecroParser* parser);
NecroAST_LocalPtr parse_tuple_type(NecroParser* parser);
NecroAST_LocalPtr parse_list_type(NecroParser* parser);
NecroAST_LocalPtr parse_atype(NecroParser* parser, NECRO_VAR_TYPE tyvar_var_type);
NecroAST_LocalPtr parse_constr(NecroParser* parser);
NecroAST_LocalPtr parse_simpletype(NecroParser* parser);
NecroAST_LocalPtr parse_constructors(NecroParser* parser);

NecroAST_LocalPtr parse_top_level_data_declaration(NecroParser* parser)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // data
    if (peek_token_type(parser) != NECRO_LEX_DATA)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // simpletype
    NecroAST_LocalPtr simpletype = parse_simpletype(parser);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (simpletype == null_local_ptr)
        return write_error_and_restore(parser, snapshot, "Expected a type to follow 'data', but found: %s", necro_lex_token_type_string(peek_token_type(parser)));

    // =
    if (peek_token_type(parser) != NECRO_LEX_ASSIGN)
        return write_error_and_restore(parser, snapshot, "Expected '=' to follow type in data declaration, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
    consume_token(parser);

    // constructor_list
    NecroAST_LocalPtr constructor_list = parse_list(parser, NECRO_LEX_PIPE, parse_constr);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (constructor_list == null_local_ptr)
        return write_error_and_restore(parser, snapshot, "Expected Data Constructor to follow '=' in data declaration, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));

    // Success!
    NecroAST_LocalPtr ptr                   = null_local_ptr;
    NecroAST_Node*    node                  = ast_alloc_node_local_ptr(parser, &ptr);
    node->type                              = NECRO_AST_DATA_DECLARATION;
    node->data_declaration.simpletype       = simpletype;
    node->data_declaration.constructor_list = constructor_list;
    return ptr;
}

NecroAST_LocalPtr parse_atype_list(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // tyvar
    NecroAST_LocalPtr atype = parse_atype(parser, NECRO_VAR_VAR);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (atype == null_local_ptr)
        return null_local_ptr;

    // next
    NecroAST_LocalPtr next = parse_atype_list(parser);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // Success!
    NecroAST_LocalPtr ptr   = null_local_ptr;
    NecroAST_Node*    node  = ast_alloc_node_local_ptr(parser, &ptr);
    node->type              = NECRO_AST_LIST_NODE;
    node->list.item         = atype;
    node->list.next_item    = next;
    return ptr;
}

NecroAST_LocalPtr parse_constr(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // tycon
    NecroAST_LocalPtr tycon = parse_tycon(parser, NECRO_CON_DATA_DECLARATION);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (tycon == null_local_ptr)
        return null_local_ptr;

    // atype list
    NecroAST_LocalPtr atype_list = parse_atype_list(parser);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // Success!
    NecroAST_LocalPtr ptr      = null_local_ptr;
    NecroAST_Node*    node     = ast_alloc_node_local_ptr(parser, &ptr);
    node->type                 = NECRO_AST_CONSTRUCTOR;
    node->constructor.conid    = tycon;
    node->constructor.arg_list = atype_list;
    return ptr;
}

NecroAST_LocalPtr parse_tyvar_list(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // tyvar
    NecroAST_LocalPtr tyvar = parse_tyvar(parser, NECRO_VAR_DECLARATION);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (tyvar == null_local_ptr)
        return null_local_ptr;

    // next
    NecroAST_LocalPtr next = parse_tyvar_list(parser);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // Success!
    NecroAST_LocalPtr ptr   = null_local_ptr;
    NecroAST_Node*    node  = ast_alloc_node_local_ptr(parser, &ptr);
    node->type              = NECRO_AST_LIST_NODE;
    node->list.item         = tyvar;
    node->list.next_item    = next;
    return ptr;
}

NecroAST_LocalPtr parse_simpletype(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // tycon
    NecroAST_LocalPtr tycon = parse_tycon(parser, NECRO_CON_TYPE_DECLARATION);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (tycon == null_local_ptr)
        return null_local_ptr;

    // tyvar list
    NecroAST_LocalPtr tyvars = parse_tyvar_list(parser);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // Success!
    NecroAST_LocalPtr ptr           = null_local_ptr;
    NecroAST_Node*    node          = ast_alloc_node_local_ptr(parser, &ptr);
    node->type                      = NECRO_AST_SIMPLE_TYPE;
    node->simple_type.type_con      = tycon;
    node->simple_type.type_var_list = tyvars;
    return ptr;
}

NecroAST_LocalPtr parse_type(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr   ty_ptr    = parse_btype(parser);
    if (ty_ptr == null_local_ptr || peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    // Next Type after arrow
    NecroAST_LocalPtr next_after_arrow_ptr = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_RIGHT_ARROW)
    {
        consume_token(parser);
        NecroAST_LocalPtr next_after_arrow_ptr = parse_type(parser);
        if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
            return null_local_ptr;
        if (next_after_arrow_ptr == null_local_ptr)
            return write_error_and_restore(parser, snapshot, "Expected a Type after ->, but found %s", necro_lex_token_type_string(peek_token_type(parser)));

        NecroAST_LocalPtr ptr             = null_local_ptr;
        NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &ptr);
        node->type                        = NECRO_AST_FUNCTION_TYPE;
        node->function_type.type          = ty_ptr;
        node->function_type.next_on_arrow = next_after_arrow_ptr;
        return ptr;
    }
    else
    {
        return ty_ptr;
    }
}

NecroAST_LocalPtr parse_btype_go(NecroParser* parser, NecroAST_LocalPtr left)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    if (left == null_local_ptr)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr atype = parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (atype == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return left;
    }

    NecroAST_LocalPtr ptr  = null_local_ptr;
    NecroAST_Node*    node = ast_alloc_node_local_ptr(parser, &ptr);
    node->type             = NECRO_AST_TYPE_APP;
    node->type_app.ty      = left;
    node->type_app.next_ty = atype;
    return parse_btype_go(parser, ptr);
}

NecroAST_LocalPtr parse_btype(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    atype    = parse_btype_go(parser, parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    if (atype == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    return atype;
}

NecroAST_LocalPtr parse_atype(NecroParser* parser, NECRO_VAR_TYPE tyvar_var_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    ptr      = null_local_ptr;

    // tycon
    ptr = parse_tycon(parser, NECRO_CON_TYPE_VAR);

    // tyvar
    if (ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        ptr = parse_tyvar(parser, tyvar_var_type);
    }

    if (ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        ptr = parse_tuple_type(parser);
    }

    // [type]
    if (ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        ptr = parse_list_type(parser);
    }

    // (type)
    if (ptr == null_local_ptr && parser->descent_state != NECRO_DESCENT_PARSE_ERROR)
    {
        if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
        {
            consume_token(parser);
            ptr = parse_type(parser);
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            {
                restore_parser(parser, snapshot);
                ptr = null_local_ptr;
            }
            consume_token(parser);
        }
    }

    return ptr;
}

NecroAST_LocalPtr parse_gtycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr type_con_local_ptr = null_local_ptr;

    // ()
    if (peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol symbol = peek_token(parser)->symbol;
        consume_token(parser);
        NecroAST_Node* type_con_node  = ast_alloc_node_local_ptr(parser, &type_con_local_ptr);
        type_con_node->type           = NECRO_AST_CONID;
        type_con_node->conid.symbol   = symbol;
        type_con_node->conid.con_type = con_type;
    }
    // []
    else if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    {
        NecroSymbol symbol = peek_token(parser)->symbol;
        consume_token(parser);
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
        {
            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
        consume_token(parser);
        NecroAST_Node* type_con_node  = ast_alloc_node_local_ptr(parser, &type_con_local_ptr);
        type_con_node->type           = NECRO_AST_CONID;
        type_con_node->conid.symbol   = symbol;
        type_con_node->conid.con_type = con_type;
    }
    // Constructor
    else
    {
        type_con_local_ptr = parse_tycon(parser, con_type);
    }
    if (type_con_local_ptr == null_local_ptr)
        restore_parser(parser, snapshot);
    return type_con_local_ptr;
}

NecroAST_LocalPtr parse_tycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    // ()
    if (peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol symbol = peek_token(parser)->symbol;
        consume_token(parser);
        NecroAST_LocalPtr local;
        NecroAST_Node* type_con_node  = ast_alloc_node_local_ptr(parser, &local);
        type_con_node->type           = NECRO_AST_CONID;
        type_con_node->conid.symbol   = symbol;
        type_con_node->conid.con_type = con_type;
        return local;
    }

    // IDENTIFIER
    NecroParser_Snapshot snapshot           = snapshot_parser(parser);
    NecroAST_LocalPtr    type_con_local_ptr = null_local_ptr;
    NecroLexToken*       look_ahead_token   = peek_token(parser);

    if (look_ahead_token->token != NECRO_LEX_TYPE_IDENTIFIER)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    NecroAST_Node* node  = ast_alloc_node_local_ptr(parser, &type_con_local_ptr);
    node->type           = NECRO_AST_CONID;
    node->conid.symbol   = look_ahead_token->symbol;
    node->conid.con_type = con_type;
    consume_token(parser);
    return type_con_local_ptr;
}

NecroAST_LocalPtr parse_qtycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    // Save until we actually have modules...
 	// [ modid . ] conid
    return parse_tycon(parser, con_type);
}

NecroAST_LocalPtr parse_tyvar(NecroParser* parser, NECRO_VAR_TYPE var_type)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot           = snapshot_parser(parser);
    NecroAST_LocalPtr    type_var_local_ptr = null_local_ptr;
    NecroLexToken*       look_ahead_token   = peek_token(parser);

    if (look_ahead_token->token != NECRO_LEX_IDENTIFIER)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    NecroAST_Node* node     = ast_alloc_node_local_ptr(parser, &type_var_local_ptr);
    node->type              = NECRO_AST_VARIABLE;
    node->variable.symbol   = look_ahead_token->symbol;
    node->variable.var_type = var_type;
    consume_token(parser);
    return type_var_local_ptr;
}

NecroAST_LocalPtr parse_list_type(NecroParser* parser)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    ptr      = null_local_ptr;

    // [
    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // type
    ptr = parse_type(parser);
    if (ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // ]
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // SUCCESS!
    NecroAST_LocalPtr list_ptr  = null_local_ptr;
    NecroAST_Node*    list_node = ast_alloc_node_local_ptr(parser, &list_ptr);
    list_node->type             = NECRO_AST_CONID;
    list_node->conid.symbol     = necro_intern_string(parser->intern, "[]");
    list_node->conid.con_type   = NECRO_CON_TYPE_VAR;

    NecroAST_LocalPtr app_ptr  = null_local_ptr;
    NecroAST_Node*    app_node = ast_alloc_node_local_ptr(parser, &app_ptr);
    app_node->type             = NECRO_AST_TYPE_APP;
    app_node->type_app.ty      = list_ptr;
    app_node->type_app.next_ty = ptr;
    return app_ptr;
}


NecroAST_LocalPtr parse_tuple_type(NecroParser* parser)
{
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // (
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
        return null_local_ptr;
    consume_token(parser);

    // types
    NecroAST_LocalPtr list_local_ptr = parse_list(parser, NECRO_LEX_COMMA, parse_type);
    if (list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // require at least k = 2
    {
        NecroAST_Node* list_node = ast_get_node(parser->ast, list_local_ptr);
        if (list_node->list.next_item == null_local_ptr)
        {
            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
    }

    // )
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // SUCCESS!
    NecroAST_LocalPtr local_ptr       = null_local_ptr;
    NecroAST_Node*    node            = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                        = NECRO_AST_TUPLE;
    node->expression_list.expressions = list_local_ptr;
    return local_ptr;
}

//=====================================================
// Type Declarations
//=====================================================
NecroAST_LocalPtr parse_tycls(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    return parse_tycon(parser, con_type);
}

NecroAST_LocalPtr parse_qtycls(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    return parse_qtycon(parser, con_type);
}

NecroAST_LocalPtr parse_simple_class(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // qtycl
    NecroAST_LocalPtr conid = parse_qtycls(parser, NECRO_CON_TYPE_VAR);
    if (conid == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // varid
    NecroAST_LocalPtr varid = parse_tyvar(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (conid == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // SUCCESS!
    NecroAST_LocalPtr local_ptr    = null_local_ptr;
    NecroAST_Node*    node         = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                     = NECRO_AST_TYPE_CLASS_CONTEXT;
    node->type_class_context.conid = conid;
    node->type_class_context.varid = varid;
    return local_ptr;
}

NecroAST_LocalPtr parse_context(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    {
        consume_token(parser);
        NecroAST_LocalPtr class_list = parse_list(parser, NECRO_LEX_COMMA, parse_simple_class);
        if (class_list == null_local_ptr)
        {
            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            return write_error_and_restore(parser, snapshot, "Expected ')' at end of context, but found %s", necro_lex_token_type_string(peek_token_type(parser)));
        consume_token(parser);
        return class_list;
    }
    else
    {
        return parse_simple_class(parser);
    }
}

NecroAST_LocalPtr parse_type_signature(NecroParser* parser, NECRO_SIG_TYPE sig_type)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;
    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // var
    NecroAST_LocalPtr var;
    if (sig_type == NECRO_SIG_DECLARATION)
        var = parse_variable(parser, NECRO_VAR_SIG);
    else if (sig_type == NECRO_SIG_TYPE_CLASS)
        var = parse_variable(parser, NECRO_VAR_CLASS_SIG);
    if (var == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // ::
    if (peek_token_type(parser) != NECRO_LEX_DOUBLE_COLON)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // context
    NecroParser_Snapshot context_snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    context          = parse_context(parser);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // =>
    if (context != null_local_ptr)
    {
        if (peek_token_type(parser) != NECRO_LEX_FAT_RIGHT_ARROW)
        {
            restore_parser(parser, context_snapshot);
            context = null_local_ptr;
        }
        else
        {
            consume_token(parser);
        }
    }

    // type
    NecroAST_LocalPtr type = parse_type(parser);
    if (type == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // SUCCESS!
    NecroAST_LocalPtr local_ptr   = null_local_ptr;
    NecroAST_Node*    node        = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                    = NECRO_AST_TYPE_SIGNATURE;
    node->type_signature.var      = var;
    node->type_signature.context  = context;
    node->type_signature.type     = type;
    node->type_signature.sig_type = sig_type;
    return local_ptr;
}

// NecroAST_LocalPtr parse_expression_type_signature(NecroParser* parser)

NecroAST_LocalPtr parse_class_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_type_signature(parser, NECRO_SIG_TYPE_CLASS);
    }

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_simple_assignment(parser);
    }

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_apats_assignment(parser);
    }

    return declaration_local_ptr;
}

NecroAST_LocalPtr parse_class_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr declaration_local_ptr = parse_class_declaration(parser);

    if ((declaration_local_ptr != null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        NecroAST_LocalPtr next_decl = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser); // consume SemiColon token
            next_decl = parse_class_declarations_list(parser);
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

NecroAST_LocalPtr parse_type_class_declaration(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // class
    if (peek_token_type(parser) != NECRO_LEX_CLASS)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // context
    NecroParser_Snapshot context_snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    context          = parse_context(parser);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // =>
    if (context != null_local_ptr)
    {
        if (peek_token_type(parser) != NECRO_LEX_FAT_RIGHT_ARROW)
        {
            restore_parser(parser, context_snapshot);
            context = null_local_ptr;
        }
        else
        {
            consume_token(parser);
        }
    }

    // tycls
    NecroAST_LocalPtr tycls = parse_tycls(parser, NECRO_CON_TYPE_DECLARATION);
    if (tycls == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // tyvar
    NecroAST_LocalPtr tyvar = parse_tyvar(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (tyvar == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // declarations
    NecroAST_LocalPtr declarations = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        consume_token(parser);
        if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACE)
        {
            consume_token(parser);
            declarations = parse_class_declarations_list(parser);
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                write_error_and_restore(parser, snapshot, "Expected '}' at end of type class declaration, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
            consume_token(parser);
        }
    }

    // SUCCESS!
    NecroAST_LocalPtr local_ptr               = null_local_ptr;
    NecroAST_Node*    node                    = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                                = NECRO_AST_TYPE_CLASS_DECLARATION;
    node->type_class_declaration.context      = context;
    node->type_class_declaration.tycls        = tycls;
    node->type_class_declaration.tyvar        = tyvar;
    node->type_class_declaration.declarations = declarations;
    return local_ptr;
}

NecroAST_LocalPtr parse_instance_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_simple_assignment(parser);
    }

    if ((declaration_local_ptr == null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        declaration_local_ptr = parse_apats_assignment(parser);
    }

    return declaration_local_ptr;
}

NecroAST_LocalPtr parse_instance_declaration_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE ||
        parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    NecroAST_LocalPtr declaration_local_ptr = parse_instance_declaration(parser);

    if ((declaration_local_ptr != null_local_ptr) && (parser->descent_state != NECRO_DESCENT_PARSE_ERROR))
    {
        NecroAST_LocalPtr next_decl = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser); // consume SemiColon token
            next_decl = parse_instance_declaration_list(parser);
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

NecroAST_LocalPtr parse_inst(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // gtycon
    NecroAST_LocalPtr ptr = parse_gtycon(parser, NECRO_CON_TYPE_VAR);
    if (ptr != null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return ptr;

    // [tyvar]
    if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    {
        consume_token(parser);
        ptr = parse_tyvar(parser, NECRO_VAR_VAR);
        if (ptr != null_local_ptr)
        {
            if (peek_token_type(parser) == NECRO_LEX_RIGHT_BRACKET)
            {
                consume_token(parser);
                return ptr;
            }
        }
        restore_parser(parser, snapshot);
        ptr = null_local_ptr;
    }

    return ptr;
}

NecroAST_LocalPtr parse_type_class_instance(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
        return null_local_ptr;

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // instance
    if (peek_token_type(parser) != NECRO_LEX_INSTANCE)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // context
    NecroParser_Snapshot context_snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    context          = parse_context(parser);
    if (parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // =>
    if (context != null_local_ptr)
    {
        if (peek_token_type(parser) != NECRO_LEX_FAT_RIGHT_ARROW)
        {
            restore_parser(parser, context_snapshot);
            context = null_local_ptr;
        }
        else
        {
            consume_token(parser);
        }
    }

    // qtycls
    NecroAST_LocalPtr qtycls = parse_qtycls(parser, NECRO_CON_TYPE_VAR);
    if (qtycls == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // inst
    NecroAST_LocalPtr inst = parse_inst(parser);
    if (inst == null_local_ptr || parser->descent_state == NECRO_DESCENT_PARSE_ERROR)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // declarations
    NecroAST_LocalPtr declarations = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        consume_token(parser);
        if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACE)
        {
            consume_token(parser);
            declarations = parse_instance_declaration_list(parser);
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                write_error_and_restore(parser, snapshot, "Expected '}' at end of type class declaration, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
            consume_token(parser);
        }
    }

    // SUCCESS!
    NecroAST_LocalPtr local_ptr            = null_local_ptr;
    NecroAST_Node*    node                 = ast_alloc_node_local_ptr(parser, &local_ptr);
    node->type                             = NECRO_AST_TYPE_CLASS_INSTANCE;
    node->type_class_instance.context      = context;
    node->type_class_instance.qtycls       = qtycls;
    node->type_class_instance.inst         = inst;
    node->type_class_instance.declarations = declarations;
    return local_ptr;
}
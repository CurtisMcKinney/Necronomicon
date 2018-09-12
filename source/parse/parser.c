/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include "intern.h"
#include "parser.h"
#include "parse_ast.h"

// #define PARSE_DEBUG_PRINT 1

static const size_t MAX_LOCAL_PTR = ((size_t)((NecroAST_LocalPtr)-1));

///////////////////////////////////////////////////////
// NecroParser
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_DESCENT_PARSING,
    NECRO_DESCENT_PARSING_PATTERN,
    NECRO_DESCENT_PARSE_DONE
} NecroParse_DescentState;

typedef struct NecroParser
{
    NecroAST                ast;
    NecroLexToken*          tokens;
    size_t                  current_token;
    NecroParse_DescentState descent_state;
    NecroError              error;
    NecroIntern*            intern;
    bool                    parsing_pat_assignment; // TODO / HACK; Find a better way to delineate
} NecroParser;

NecroParser construct_parser(NecroLexToken* tokens, size_t num_tokens, NecroIntern* intern)
{
    return (NecroParser)
    {
        .current_token          = 0,
        .ast                    = (NecroAST) { construct_arena(num_tokens * sizeof(NecroAST_Node)) },
        .tokens                 = tokens,
        .descent_state          = NECRO_DESCENT_PARSING,
        .intern                 = intern,
        .parsing_pat_assignment = false,
    };
}

void destruct_parser(NecroParser* parser)
{
    // Ownership of ast is passed out before deconstruction
    parser->current_token          = 0;
    parser->tokens                 = NULL;
    parser->descent_state          = NECRO_DESCENT_PARSE_DONE;
    parser->intern                 = NULL;
    parser->parsing_pat_assignment = false;
}

NecroAST necro_empty_ast()
{
    return (NecroAST) { .arena = necro_empty_arena(), .root = 0 };
}

void necro_destroy_ast(NecroAST* ast)
{
    destruct_arena(&ast->arena);
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroAST_Node* ast_get_node(NecroAST* ast, NecroAST_LocalPtr local_ptr)
{
    assert(ast != NULL);
    assert(local_ptr != null_local_ptr);
    return ((NecroAST_Node*) ast->arena.region) + local_ptr;
}

NecroAST_Node* ast_get_root_node(NecroAST* ast)
{
    assert(ast != NULL);
    return (NecroAST_Node*) ast->arena.region;
}

// Snap shot of parser state for back tracking
typedef struct
{
    size_t current_token;
    size_t ast_size;
} NecroParser_Snapshot;

inline NecroParser_Snapshot snapshot_parser(NecroParser* parser)
{
    return (NecroParser_Snapshot) { parser->current_token, parser->ast.arena.size };
}

inline void restore_parser(NecroParser* parser, NecroParser_Snapshot snapshot)
{
    parser->current_token = snapshot.current_token;
    parser->ast.arena.size = snapshot.ast_size;
}

inline NecroLexToken* peek_token(NecroParser* parser)
{
    return parser->tokens + parser->current_token;
}

inline NECRO_LEX_TOKEN_TYPE peek_token_type(NecroParser* parser)
{
    return peek_token(parser)->token;
}

inline void consume_token(NecroParser* parser)
{
    ++parser->current_token;
}

const char* bin_op_name(NecroAST_BinOpType type)
{
    switch (type)
    {
    case NECRO_BIN_OP_ADD:
        return "(+)";
    case NECRO_BIN_OP_SUB:
        return "(-)";
    case NECRO_BIN_OP_MUL:
        return "(*)";
    case NECRO_BIN_OP_DIV:
        return "(/)";
    case NECRO_BIN_OP_MOD:
        return "(%)";
    case NECRO_BIN_OP_GT:
        return "(>)";
    case NECRO_BIN_OP_LT:
        return "(<)";
    case NECRO_BIN_OP_GTE:
        return "(>=)";
    case NECRO_BIN_OP_LTE:
        return "(<=)";
    case NECRO_BIN_OP_COLON:
        return "(:)";
    case NECRO_BIN_OP_DOUBLE_COLON:
        return "(::)";
    case NECRO_BIN_OP_LEFT_SHIFT:
        return "(<<)";
    case NECRO_BIN_OP_RIGHT_SHIFT:
        return "(>>)";
    case NECRO_BIN_OP_PIPE:
        return "(|)";
    case NECRO_BIN_OP_FORWARD_PIPE:
        return "(|>)";
    case NECRO_BIN_OP_BACK_PIPE:
        return "(<|)";
    case NECRO_BIN_OP_EQUALS:
        return "(=)";
    case NECRO_BIN_OP_NOT_EQUALS:
        return "(/=)";
    case NECRO_BIN_OP_AND:
        return "(&&)";
    case NECRO_BIN_OP_OR:
        return "(||)";
    case NECRO_BIN_OP_DOT:
        return "(.)";
    case NECRO_BIN_OP_DOLLAR:
        return "($)";
    case NECRO_BIN_OP_BIND_RIGHT:
        return "(>>=)";
    case NECRO_BIN_OP_BIND_LEFT:
        return "(=<<)";
    case NECRO_BIN_OP_DOUBLE_EXCLAMATION:
        return "(!!)";
    case NECRO_BIN_OP_APPEND:
        return "(++)";
    case NECRO_BIN_OP_FBY:
        return "(-->)";
    default:
        assert(false);
        return "(Undefined Binary Operator)";
    }
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
    case NECRO_VAR_TYPE_VAR_DECLARATION:     return "TypeVar Declaration";
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
    return (NecroAST_Node*) arena_alloc(&parser->ast.arena, sizeof(NecroAST_Node), arena_allow_realloc);
}

NecroAST_Node* ast_alloc_node_local_ptr(struct NecroParser* parser, NecroAST_LocalPtr* local_ptr)
{
    const size_t offset = parser->ast.arena.size / sizeof(NecroAST_Node);
    assert(offset < MAX_LOCAL_PTR);
    *local_ptr = (uint32_t) offset;
    NecroAST_Node* node = (NecroAST_Node*)arena_alloc(&parser->ast.arena, sizeof(NecroAST_Node), arena_allow_realloc);
    node->source_loc = parser->tokens[parser->current_token > 0 ? (parser->current_token - 1) : 0].source_loc;
    return node;
}

NecroAST_Node* necro_parse_ast_alloc(NecroArena* arena, NecroAST_LocalPtr* local_ptr)
{
    const size_t offset = arena->size / sizeof(NecroAST_Node);
    assert(offset < MAX_LOCAL_PTR);
    *local_ptr = (uint32_t) offset;
    NecroAST_Node* node = (NecroAST_Node*)arena_alloc(arena, sizeof(NecroAST_Node), arena_allow_realloc);
    return node;
}

NecroAST_LocalPtr ast_last_node_ptr(NecroParser* parser)
{
    NecroAST_LocalPtr local_ptr = null_local_ptr;
    if (parser->ast.arena.size > 0)
    {
        const size_t offset = (parser->ast.arena.size / sizeof(NecroAST_Node)) - 1;
        assert(offset < MAX_LOCAL_PTR);
        return (NecroAST_LocalPtr) offset;
    }
    return local_ptr;
}

void print_ast_impl(NecroAST* ast, NecroAST_Node* ast_node, NecroIntern* intern, uint32_t depth)
{
    assert(ast != NULL);
    assert(ast_node != NULL);
    assert(intern != NULL);
    for (uint32_t i = 0;  i < depth; ++i)
    {
        printf(STRING_TAB);
    }

    switch(ast_node->type)
    {
    case NECRO_AST_BIN_OP:
        // puts(bin_op_name(ast_node->bin_op.type));
        printf("(%s)\n", necro_intern_get_string(intern, ast_node->bin_op.symbol));
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.lhs), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->bin_op.rhs), intern, depth + 1);
        break;

    case NECRO_AST_CONSTANT:
        switch (ast_node->constant.type)
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
        // case NECRO_AST_CONSTANT_BOOL:
        //     printf("(%s)\n", ast_node->constant.boolean_literal ? " True" : "False");
        //     break;
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
        if (ast_node->simple_assignment.initializer != null_local_ptr)
        {
            print_white_space(depth * 2);
            printf("<\n");
            print_ast_impl(ast, ast_get_node(ast, ast_node->simple_assignment.initializer), intern, depth);
            print_white_space(depth * 2);
            printf(">\n");
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->simple_assignment.rhs), intern, depth + 1);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        puts("(Right Hand Side)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->right_hand_side.expression), intern, depth + 1);
        if (ast_node->right_hand_side.declarations != null_local_ptr)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(STRING_TAB);
            }
            puts(STRING_TAB STRING_TAB "where");
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
                printf(STRING_TAB);
            }
            puts(STRING_TAB STRING_TAB "in");
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

    case NECRO_AST_PAT_ASSIGNMENT:
        puts("(Pat Assignment)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->pat_assignment.pat), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->pat_assignment.rhs), intern, depth + 1);
        break;

    case NECRO_AST_LAMBDA:
        puts("\\(lambda)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->lambda.apats), intern, depth + 1);
        for (uint32_t i = 0;  i < (depth + 1); ++i)
        {
            printf(STRING_TAB);
        }
        puts("->");
        print_ast_impl(ast, ast_get_node(ast, ast_node->lambda.expression), intern, depth + 2);
        break;

    case NECRO_AST_DO:
        puts("(do)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->do_statement.statement_list), intern, depth + 1);
        break;

    case NECRO_AST_PAT_EXPRESSION:
        puts("(pat)");
        print_ast_impl(ast, ast_get_node(ast, ast_node->pattern_expression.expressions), intern, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        puts("([])");
        if (ast_node->expression_list.expressions != null_local_ptr)
            print_ast_impl(ast, ast_get_node(ast, ast_node->expression_list.expressions), intern, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_ARRAY:
        puts("({})");
        if (ast_node->expression_array.expressions != null_local_ptr)
            print_ast_impl(ast, ast_get_node(ast, ast_node->expression_array.expressions), intern, depth + 1);
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

    case NECRO_PAT_BIND_ASSIGNMENT:
        printf("(Pat Bind)\n");
        print_ast_impl(ast, ast_get_node(ast, ast_node->pat_bind_assignment.pat), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->pat_bind_assignment.expression), intern, depth + 1);
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
        for (uint32_t i = 0;  i < depth; ++i) printf(STRING_TAB);
        puts("of");
        print_ast_impl(ast, ast_get_node(ast, ast_node->case_expression.alternatives), intern, depth + 1);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        printf("\r"); // clear current line
        print_ast_impl(ast, ast_get_node(ast, ast_node->case_alternative.pat), intern, depth + 0);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(STRING_TAB);
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
        for (uint32_t i = 0;  i < depth; ++i) printf(STRING_TAB);
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

    case NECRO_AST_OP_LEFT_SECTION:
        printf("(LeftSection %s)\n", bin_op_name(ast_node->op_left_section.type));
        print_ast_impl(ast, ast_get_node(ast, ast_node->op_left_section.left), intern, depth + 1);
        break;

    case NECRO_AST_OP_RIGHT_SECTION:
        printf("(%s RightSection)\n", bin_op_name(ast_node->op_right_section.type));
        print_ast_impl(ast, ast_get_node(ast, ast_node->op_right_section.right), intern, depth + 1);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        printf("\r");
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_signature.var), intern, depth + 0);
        for (uint32_t i = 0;  i < depth + 1; ++i) printf(STRING_TAB);
        puts("::");
        if (ast_node->type_signature.context != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_signature.context), intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(STRING_TAB);
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
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(STRING_TAB);
            puts("=>");
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.tycls), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.tyvar), intern, depth + 1);
        if (ast_node->type_class_declaration.declarations != null_local_ptr)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(STRING_TAB);
            puts("where");
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.declarations), intern, depth + 2);
        }
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        puts("(instance)");
        if (ast_node->type_class_instance.context != null_local_ptr)
        {
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_instance.context), intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(STRING_TAB);
            puts("=>");
        }
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_instance.qtycls), intern, depth + 1);
        print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_instance.inst), intern, depth + 1);
        if (ast_node->type_class_declaration.declarations != null_local_ptr)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(STRING_TAB);
            puts("where");
            print_ast_impl(ast, ast_get_node(ast, ast_node->type_class_declaration.declarations), intern, depth + 2);
        }
        break;

    case NECRO_AST_FUNCTION_TYPE:
        puts("(");
        print_ast_impl(ast, ast_get_node(ast, ast_node->function_type.type), intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(STRING_TAB);
        puts("->");
        print_ast_impl(ast, ast_get_node(ast, ast_node->function_type.next_on_arrow), intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(STRING_TAB);
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

///////////////////////////////////////////////////////
// BIN_OP
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_BIN_OP_ASSOC_LEFT,
    NECRO_BIN_OP_ASSOC_NONE,
    NECRO_BIN_OP_ASSOC_RIGHT
} NecroParse_BinOpAssociativity;

typedef struct
{
    int precedence;
    NecroParse_BinOpAssociativity associativity;
} NecroParse_BinOpBehavior;

static const NecroParse_BinOpBehavior bin_op_behaviors[NECRO_BIN_OP_COUNT + 1] = {
    { 6, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_ADD
    { 6, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_SUB
    { 7, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_MUL
    { 7, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_DIV
    { 7, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_MOD
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_GT
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_LT
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_GTE
    { 4, NECRO_BIN_OP_ASSOC_NONE },  // NECRO_BIN_OP_LTE
    { 5, NECRO_BIN_OP_ASSOC_RIGHT },  // NECRO_BIN_OP_COLON
	{ 9, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_DOUBLE_COLON
	{ 1, NECRO_BIN_OP_ASSOC_RIGHT },  // NECRO_BIN_OP_LEFT_SHIFT
	{ 1, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_RIGHT_SHIFT
	{ 2, NECRO_BIN_OP_ASSOC_LEFT },  // NECRO_BIN_OP_PIPE
	{ 0, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_FORWARD_PIPE
	{ 0, NECRO_BIN_OP_ASSOC_LEFT }, // NECRO_BIN_OP_BACK_PIPE
    { 4, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_EQUALS
    { 4, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_NOT_EQUALS
	{ 3, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_AND
	{ 2, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_OR
    { 9, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_DOT
    { 0, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_DOLLAR
    { 1, NECRO_BIN_OP_ASSOC_LEFT }, // NECRO_BIN_OP_BIND_RIGHT
    { 1, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_BIND_LEFT
    { 9, NECRO_BIN_OP_ASSOC_LEFT }, // NECRO_BIN_OP_DOUBLE_EXCLAMATION
    { 5, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_APPEND
	// { 1, NECRO_BIN_OP_ASSOC_RIGHT }, // NECRO_BIN_OP_FBY
    { 0, NECRO_BIN_OP_ASSOC_NONE }   // NECRO_BIN_OP_UNDEFINED
};

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

NecroResult(NecroAST_LocalPtr) parse_expression_impl(NecroParser* parser, NecroParser_LookAheadBinary look_ahead_binary);

static inline NecroResult(NecroAST_LocalPtr) parse_expression(NecroParser* parser)
{
    return parse_expression_impl(parser, NECRO_BINARY_LOOK_AHEAD);
}

//=====================================================
// Forward declarations
//=====================================================
typedef NecroResult(NecroAST_LocalPtr) (*ParseFunc)(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_top_declarations(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_declaration(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_declarations(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_list(NecroParser* parser, NECRO_LEX_TOKEN_TYPE token_divider, ParseFunc parse_func);
NecroResult(NecroAST_LocalPtr) parse_parenthetical_expression(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_tuple(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_l_expression(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_if_then_else_expression(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_binary_operation(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr);
NecroResult(NecroAST_LocalPtr) parse_op_left_section(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_op_right_section(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_simple_assignment(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_apats_assignment(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_pat_assignment(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_right_hand_side(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_apats(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_lambda(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_do(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_expression_list(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_pattern_expression(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_arithmetic_sequence(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_expression_array(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_case(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_pat(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_gcon(NecroParser* parser, NECRO_CON_TYPE var_type);
NecroResult(NecroAST_LocalPtr) parse_list_pat(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_tuple_pattern(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_top_level_data_declaration(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_type_signature(NecroParser* parser, NECRO_SIG_TYPE sig_type);
NecroResult(NecroAST_LocalPtr) parse_type_class_declaration(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_type_class_instance(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_initializer(NecroParser* parser);
// NecroResult(NecroAST_LocalPtr) parse_unary_operation(NecroParser* parser);

NecroAST_LocalPtr parse_constant(NecroParser* parser);
NecroAST_LocalPtr parse_con(NecroParser* parser,  NECRO_CON_TYPE var_type);
NecroAST_LocalPtr parse_qcon(NecroParser* parser, NECRO_CON_TYPE var_type);

#define NECRO_INITIALIZER_TOKEN NECRO_LEX_TILDE

NecroParse_DescentState enter_parse_pattern_state(NecroParser* parser);
void                    restore_parse_state(NecroParser* parser, NecroParse_DescentState prev_state);

NecroResult(void) necro_parse(NecroLexTokenVector* tokens, NecroIntern* intern, NecroAST* out_ast, NecroCompileInfo info)
{
    NecroParser       parser    = construct_parser(tokens->data, tokens->length, intern);
    NecroAST_LocalPtr local_ptr = necro_try_map(NecroAST_LocalPtr, void, parse_top_declarations(&parser));
    parser.ast.root             = local_ptr;
    if ((peek_token_type(&parser) != NECRO_LEX_END_OF_STREAM && peek_token_type(&parser) != NECRO_LEX_SEMI_COLON) || (local_ptr == null_local_ptr && tokens->length > 0))
    {
        NecroLexToken* look_ahead_token = peek_token(&parser);
        return necro_parse_error(look_ahead_token->source_loc, look_ahead_token->end_loc);
    }
    *out_ast = parser.ast;
    if (info.compilation_phase == NECRO_PHASE_PARSE && info.verbosity > 0)
    {
        print_ast(out_ast, intern, local_ptr);
    }
    destruct_parser(&parser);
    return ok_void();
}

inline bool necro_is_parse_result_non_error_null(NecroResult(NecroAST_LocalPtr) result)
{
    return result.type == NECRO_RESULT_OK && result.value == null_local_ptr;
}

NecroResult(NecroAST_LocalPtr) parse_top_declarations(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot           snapshot               = snapshot_parser(parser);
    NecroResult(NecroAST_LocalPtr) declarations_local_ptr = ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc                 source_loc             = peek_token(parser)->source_loc;

    // declaration
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = parse_declaration(parser);
    }

    // data declaration
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = parse_top_level_data_declaration(parser);
    }

    // type class declaration
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = parse_type_class_declaration(parser);
    }

    // type class instance
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = parse_type_class_instance(parser);
    }

    // ;
    if (necro_is_parse_result_non_error_null(declarations_local_ptr) && peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
    {
        consume_token(parser);
        // declarations_local_ptr = parse_type_class_instance(parser);
        return parse_top_declarations(parser);
    }

    // NULL result
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        NecroSourceLoc parse_error_source_loc = peek_token(parser)->source_loc;
        NecroSourceLoc parse_error_end_loc    = peek_token(parser)->end_loc;
        restore_parser(parser, snapshot);
        while (peek_token_type(parser) != NECRO_LEX_SEMI_COLON && peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
            consume_token(parser);
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            while (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
                consume_token(parser);
            NecroResult(NecroAST_LocalPtr) next_top_decl_result = parse_top_declarations(parser);
            if (next_top_decl_result.type == NECRO_RESULT_OK)
            {
                return ok_NecroAST_LocalPtr(null_local_ptr);
            }
            else
            {
                return necro_parse_error_cons(necro_parse_error(parse_error_source_loc, parse_error_end_loc).error, next_top_decl_result.error);
            }
        }
        else
        {
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
    }

    // Error result
    else if (declarations_local_ptr.type == NECRO_RESULT_ERROR)
    {
        while (peek_token_type(parser) != NECRO_LEX_SEMI_COLON && peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
            consume_token(parser);
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            while (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
                consume_token(parser);
            NecroResult(NecroAST_LocalPtr) next_top_decl_result = parse_top_declarations(parser);
            if (next_top_decl_result.type == NECRO_RESULT_OK)
            {
                return declarations_local_ptr;
            }
            else
            {
                return necro_parse_error_cons(declarations_local_ptr.error, next_top_decl_result.error);
            }
        }
        else
        {
            return declarations_local_ptr;
        }
    }

    // OK result
    else
    {
        // Finish
        NecroAST_LocalPtr declarations_local_ptr_value = declarations_local_ptr.value;
        NecroAST_LocalPtr next_top_decl                = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser);
            next_top_decl = necro_try(NecroAST_LocalPtr, parse_top_declarations(parser));
        }
        NecroAST_LocalPtr top_decl_local_ptr = necro_parse_ast_create_top_decl(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, declarations_local_ptr_value, next_top_decl);
        return ok_NecroAST_LocalPtr(top_decl_local_ptr);
    }
}

NecroResult(NecroAST_LocalPtr) parse_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_type_signature(parser, NECRO_SIG_DECLARATION));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_simple_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat_assignment(parser));
    }

    return ok_NecroAST_LocalPtr(declaration_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // List Item
    NecroSourceLoc                 source_loc            = peek_token(parser)->source_loc;
    NecroParser_Snapshot           snapshot              = snapshot_parser(parser);
    NecroResult(NecroAST_LocalPtr) declaration_local_ptr = parse_declaration(parser);

    // NULL result
    if (necro_is_parse_result_non_error_null(declaration_local_ptr))
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    // Error result
    else if (declaration_local_ptr.type == NECRO_RESULT_ERROR)
    {
        while (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE && peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
            consume_token(parser);
        while (peek_token_type(parser) == NECRO_LEX_SEMI_COLON || peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
            consume_token(parser);
        return declaration_local_ptr;
    }

    // OK result
    else
    {
        NecroAST_LocalPtr declaration_local_value = declaration_local_ptr.value;
        // List Next
        NecroAST_LocalPtr next_decl = null_local_ptr;
        if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            consume_token(parser);
            next_decl = necro_try(NecroAST_LocalPtr, parse_declarations_list(parser));
        }
        // Finish
        NecroAST_LocalPtr decl_local_ptr = necro_parse_ast_create_decl(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, declaration_local_value, next_decl);
        return ok_NecroAST_LocalPtr(decl_local_ptr);
    }
}

NecroResult(NecroAST_LocalPtr) parse_declarations(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // '{'
    bool is_list = peek_token_type(parser) == NECRO_LEX_LEFT_BRACE;
    if (is_list)
    {
        consume_token(parser);
    }

    // Declaration list
    NecroAST_LocalPtr declarations_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_declarations_list(parser));
    if (declarations_list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // '}'
    if (is_list)
    {
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
        {
            NecroSourceLoc source_loc = peek_token(parser)->source_loc;
            NecroSourceLoc end_loc    = peek_token(parser)->end_loc;
            while (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE && peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
                consume_token(parser);
            while (peek_token_type(parser) == NECRO_LEX_SEMI_COLON || peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
                consume_token(parser);
            return necro_declarations_missing_right_brace_error(source_loc, end_loc);
        }
        consume_token(parser);
    }

    return ok_NecroAST_LocalPtr(declarations_list_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_simple_assignment(NecroParser* parser)
{
    // Identifier
    NecroParser_Snapshot       snapshot            = snapshot_parser(parser);
    const NecroLexToken*       variable_name_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type          = variable_name_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM || token_type != NECRO_LEX_IDENTIFIER)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc             source_loc          = peek_token(parser)->source_loc;
    consume_token(parser);

    // Initializer
    NecroAST_LocalPtr initializer = null_local_ptr;
    if (peek_token_type(parser) == NECRO_INITIALIZER_TOKEN)
    {
        NecroParser_Snapshot initializer_snapshot = snapshot_parser(parser);
        initializer = necro_try(NecroAST_LocalPtr, parse_initializer(parser));
        if (initializer == null_local_ptr)
            restore_parser(parser, initializer_snapshot);
    }

    // '='
    if (peek_token_type(parser) != NECRO_LEX_ASSIGN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Rhs
    NecroLexToken*    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr rhs_local_ptr    = necro_try(NecroAST_LocalPtr, parse_right_hand_side(parser));
    if (rhs_local_ptr == null_local_ptr)
    {
        return necro_simple_assignment_rhs_failed_to_parse_error(variable_name_token->source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroAST_LocalPtr assignment_local_ptr = necro_parse_ast_create_simple_assignment(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, variable_name_token->symbol, rhs_local_ptr, initializer);
    return ok_NecroAST_LocalPtr(assignment_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_apats_assignment(NecroParser* parser)
{
    // Identifier
    NecroParser_Snapshot       snapshot            = snapshot_parser(parser);
    const NecroLexToken*       variable_name_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type          = variable_name_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM || token_type != NECRO_LEX_IDENTIFIER)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc             source_loc          = peek_token(parser)->source_loc;
    consume_token(parser);

    // Apats
    NecroAST_LocalPtr apats_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats(parser));

    // '='
    if (apats_local_ptr == null_local_ptr || peek_token_type(parser) != NECRO_LEX_ASSIGN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Rhs
    NecroLexToken*    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr rhs_local_ptr    = necro_try(NecroAST_LocalPtr, parse_right_hand_side(parser));
    if (rhs_local_ptr == null_local_ptr)
    {
        return necro_apat_assignment_rhs_failed_to_parse_error(variable_name_token->source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroAST_LocalPtr assignment_local_ptr = necro_parse_ast_create_apats_assignment(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, variable_name_token->symbol, apats_local_ptr, rhs_local_ptr);
    return ok_NecroAST_LocalPtr(assignment_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_pat_assignment(NecroParser* parser)
{
    // Pattern
    NecroParser_Snapshot       snapshot   = snapshot_parser(parser);
    const NecroLexToken*       top_token  = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = top_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc       = peek_token(parser)->source_loc;
    parser->parsing_pat_assignment  = true; // TODO: Hack, Fix this with better way, likely with state field in parser struct
    NecroAST_LocalPtr pat_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat(parser));
    parser->parsing_pat_assignment  = false;

    // '='
    if (pat_local_ptr == null_local_ptr || peek_token_type(parser) != NECRO_LEX_ASSIGN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Rhs
    NecroLexToken*    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr rhs_local_ptr    = necro_try(NecroAST_LocalPtr, parse_right_hand_side(parser));
    if (rhs_local_ptr == null_local_ptr)
    {
        return necro_pat_assignment_rhs_failed_to_parse_error(top_token->source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroAST_LocalPtr assignment_local_ptr = necro_parse_ast_create_pat_assignment(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, pat_local_ptr, rhs_local_ptr);
    return ok_NecroAST_LocalPtr(assignment_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_right_hand_side(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // Expression
    NecroSourceLoc       source_loc           = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot             = snapshot_parser(parser);
    NecroAST_LocalPtr    expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Where
    NecroLexToken*    look_ahead_token       = peek_token(parser);
    NecroAST_LocalPtr declarations_local_ptr = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        consume_token(parser);
        declarations_local_ptr = necro_try(NecroAST_LocalPtr, parse_declarations(parser));
        if (declarations_local_ptr == null_local_ptr)
            return necro_rhs_empty_where_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
    }

    // Finish
    NecroAST_LocalPtr rhs_local_ptr = necro_parse_ast_create_rhs(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, expression_local_ptr, declarations_local_ptr);
    return ok_NecroAST_LocalPtr(rhs_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_let_expression(NecroParser* parser)
{
    // 'let'
    // NecroParser_Snapshot snapshot         = snapshot_parser(parser);
    NecroLexToken*       look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LET)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // '{'
    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACE)
    {
        return necro_let_expected_left_brace_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
    }
    consume_token(parser);

    // Declarations
    NecroAST_LocalPtr declarations_local_ptr = necro_try(NecroAST_LocalPtr, parse_declarations(parser));
    if (declarations_local_ptr == null_local_ptr)
    {
        return necro_let_failed_to_parse_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
    }

    // '}'
    bool consumed_right_brace = false;
    if (peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
    {
        consume_token(parser);
        consumed_right_brace = true;
    }

    // 'in'
    if (peek_token_type(parser) != NECRO_LEX_IN)
    {
        return necro_let_missing_in_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
    }
    consume_token(parser);

    // 'in' Expression
    NecroAST_LocalPtr expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        return necro_let_empty_in_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
    }

    // ';' && '}'
    if (!consumed_right_brace)
    {
        // ';'
        if (peek_token_type(parser) != NECRO_LEX_SEMI_COLON)
        {
            return necro_let_expected_semicolon_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
        }
        consume_token(parser);

        // '}'
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
        {
            return necro_let_expected_right_brace_error(look_ahead_token->source_loc, peek_token(parser)->end_loc);
        }
        consume_token(parser);
    }

    // Finish
    NecroAST_LocalPtr let_local_ptr = necro_parse_ast_create_let(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, expression_local_ptr, declarations_local_ptr);
    return ok_NecroAST_LocalPtr(let_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_expression_impl(NecroParser* parser, NecroParser_LookAheadBinary look_ahead_binary)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr   local_ptr = null_local_ptr;

    // Unary Op
    // if (local_ptr == null_local_ptr)
    // {
    //     local_ptr = parse_unary_operation(parser);
    // }

    // L-Expression
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_l_expression(parser));
    }

    // BinOp
    if (look_ahead_binary == NECRO_BINARY_LOOK_AHEAD && local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr bin_op_local_ptr = necro_try(NecroAST_LocalPtr, parse_binary_operation(parser, local_ptr));
        if (bin_op_local_ptr != null_local_ptr)
            local_ptr = bin_op_local_ptr;
    }

    if (local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroAST_LocalPtr parse_variable(NecroParser* parser, NECRO_VAR_TYPE var_type)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    // Token
    NecroSourceLoc             source_loc     = peek_token(parser)->source_loc;
    NecroParser_Snapshot       snapshot       = snapshot_parser(parser);
    const NecroLexToken*       variable_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type     = variable_token->token;
    consume_token(parser);

    // Variable Name
    if (token_type == NECRO_LEX_IDENTIFIER)
    {
        NecroAST_LocalPtr varid_local_ptr = necro_parse_ast_create_var(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, variable_token->symbol, var_type, null_local_ptr);
        return varid_local_ptr;
    }

    // Parenthetical Variable Name, '('
    else if (token_type == NECRO_LEX_LEFT_PAREN)
    {
        // Symbol
        const NecroLexToken*       sym_variable_token = peek_token(parser);
        const NECRO_LEX_TOKEN_TYPE sym_token_type     = sym_variable_token->token;
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
            consume_token(parser);
            // ')'
            if (peek_token_type(parser) == NECRO_LEX_RIGHT_PAREN)
            {
                consume_token(parser); // consume ")" token
                NecroAST_LocalPtr varsym_local_ptr = necro_parse_ast_create_var(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, sym_variable_token->symbol, var_type, null_local_ptr);
                return varsym_local_ptr;
            }
            break;
        }
    }

    restore_parser(parser, snapshot);
    return null_local_ptr;
}

///////////////////////////////////////////////////////
// Expressions
///////////////////////////////////////////////////////
NecroResult(NecroAST_LocalPtr) parse_application_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot  = snapshot_parser(parser);
    NecroAST_LocalPtr    local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_variable(parser, NECRO_VAR_VAR);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_gcon(parser, NECRO_CON_VAR));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_constant(parser);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_op_left_section(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_op_right_section(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_parenthetical_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_tuple(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_expression_list(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_expression_array(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_arithmetic_sequence(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroAST_LocalPtr(local_ptr);
    }

    restore_parser(parser, snapshot);
    return ok_NecroAST_LocalPtr(null_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_function_expression_go(NecroParser* parser, NecroAST_LocalPtr left)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || left == null_local_ptr)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // aexpression
    NecroSourceLoc       source_loc  = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot    = snapshot_parser(parser);
    NecroAST_LocalPtr    aexpression = necro_try(NecroAST_LocalPtr, parse_application_expression(parser));
    if (aexpression == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(left);
    }

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_fexpr(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, left, aexpression);
    return parse_function_expression_go(parser, ptr);
}

NecroResult(NecroAST_LocalPtr) parse_function_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // aexpression
    NecroParser_Snapshot snapshot    = snapshot_parser(parser);
    NecroAST_LocalPtr    expr        = necro_try(NecroAST_LocalPtr, parse_application_expression(parser));
    NecroAST_LocalPtr    aexpression = necro_try(NecroAST_LocalPtr, parse_function_expression_go(parser, expr));
    if (aexpression == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    return ok_NecroAST_LocalPtr(aexpression);
}

///////////////////////////////////////////////////////
// Constant Constructors
///////////////////////////////////////////////////////
NecroResult(NecroAST_LocalPtr) parse_const_tuple(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_const_con(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_parenthetical_const_con(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_const_list(NecroParser* parser);

NecroResult(NecroAST_LocalPtr) parse_const_acon(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot  = snapshot_parser(parser);
    NecroAST_LocalPtr    local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_gcon(parser, NECRO_CON_VAR));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_constant(parser);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_parenthetical_const_con(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_const_tuple(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_const_list(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroAST_LocalPtr(local_ptr);
    }

    restore_parser(parser, snapshot);
    return ok_NecroAST_LocalPtr(null_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_parenthetical_const_con(NecroParser* parser)
{
    // '('
    NecroLexToken* look_ahead_token = peek_token(parser);
    NecroSourceLoc source_loc       = look_ahead_token->source_loc;
    if (look_ahead_token->token != NECRO_LEX_LEFT_PAREN)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    consume_token(parser);

    // Con
    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr local_ptr = necro_try(NecroAST_LocalPtr, parse_const_con(parser));
    if (local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ')'
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        if (look_ahead_token->token == NECRO_LEX_COMMA)
        {
            // if a comma is next then this may be a tuple, keep trying to parse
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
        return necro_const_con_missing_right_brace(source_loc, look_ahead_token->end_loc);
    }
    consume_token(parser);

    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_const_con_go(NecroParser* parser, NecroAST_LocalPtr left)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || left == null_local_ptr)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // aexpression
    NecroSourceLoc       source_loc  = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot    = snapshot_parser(parser);
    NecroAST_LocalPtr    aexpression = necro_try(NecroAST_LocalPtr, parse_const_acon(parser));
    if (aexpression == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(left);
    }

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_fexpr(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, left, aexpression);
    return parse_const_con_go(parser, ptr);
}

NecroResult(NecroAST_LocalPtr) parse_const_con(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroParser_Snapshot snapshot    = snapshot_parser(parser);
    NecroAST_LocalPtr    con         = necro_try(NecroAST_LocalPtr, parse_const_acon(parser));
    NecroAST_LocalPtr    aexpression = necro_try(NecroAST_LocalPtr, parse_const_con_go(parser, con));
    if (aexpression == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    return ok_NecroAST_LocalPtr(aexpression);
}

NecroResult(NecroAST_LocalPtr) parse_const_tuple(NecroParser* parser)
{
    // '('
    NecroSourceLoc             source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot       snapshot   = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type != NECRO_LEX_LEFT_PAREN || token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    consume_token(parser);

    // Expressions
    NecroAST_LocalPtr statement_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_const_con));
    if (statement_list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ')'
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        return necro_tuple_missing_paren_error(source_loc, look_ahead_token->end_loc);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr list_local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, statement_list_local_ptr);
    return ok_NecroAST_LocalPtr(list_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_const_list(NecroParser* parser)
{
    // [
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Expressions
    NecroAST_LocalPtr list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_const_con));

    // ]
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_expression_list(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, list_local_ptr);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_initializer(NecroParser* parser)
{
    // Initializer
    NecroParser_Snapshot snapshot         = snapshot_parser(parser);
    NecroLexToken*       look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_INITIALIZER_TOKEN)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    consume_token(parser);

    // Constant Con
    NecroAST_LocalPtr local_ptr = necro_try(NecroAST_LocalPtr, parse_const_con(parser));
    if (local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    return ok_NecroAST_LocalPtr(local_ptr);
}

///////////////////////////////////////////////////////
// Patterns
///////////////////////////////////////////////////////
NecroResult(NecroAST_LocalPtr) parse_wildcard(NecroParser* parser)
{
    // Underscore
    if (peek_token_type(parser) != NECRO_LEX_UNDER_SCORE)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr wildcard_local_ptr = necro_parse_ast_create_wildcard(&parser->ast.arena, source_loc, peek_token(parser)->source_loc);
    return ok_NecroAST_LocalPtr(wildcard_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_apat(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParse_DescentState prev_state = enter_parse_pattern_state(parser);
    NecroParser_Snapshot    snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr       local_ptr  = null_local_ptr;

    // var
    if (local_ptr == null_local_ptr)
    {
        local_ptr = parse_variable(parser, NECRO_VAR_DECLARATION);
    }

    // Initialized var
    if (local_ptr == null_local_ptr && peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    {
        consume_token(parser); // '('
        local_ptr = parse_variable(parser, NECRO_VAR_DECLARATION);
        // Initializer
        NecroAST_LocalPtr initializer = null_local_ptr;
        if (local_ptr != null_local_ptr && parser->parsing_pat_assignment && peek_token_type(parser) == NECRO_INITIALIZER_TOKEN)
        {
            // NecroParser_Snapshot initializer_snapshot = snapshot_parser(parser);
            initializer = necro_try(NecroAST_LocalPtr, parse_initializer(parser));
            if (initializer == null_local_ptr)
                restore_parser(parser, snapshot);
            else
            {
                if (peek_token_type(parser) == NECRO_LEX_RIGHT_PAREN)
                {
                    consume_token(parser);
                    // Set initializer
                    NecroAST_Node* variable_node = ast_get_node(&parser->ast, local_ptr);
                    assert(variable_node != NULL);
                    assert(variable_node->type == NECRO_AST_VARIABLE);
                    variable_node->variable.initializer = initializer;
                }
                else
                {
                    //     return write_error_and_restore(parser, snapshot, "Expected ')' at end of pattern, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
                    local_ptr = null_local_ptr;
                    restore_parser(parser, snapshot);
                }
            }
        }
        else
        {
            local_ptr = null_local_ptr;
            restore_parser(parser, snapshot);
        }
    }

    // gcon
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_gcon(parser, NECRO_CON_VAR));
    }

    // _
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_wildcard(parser));
    }

    // Numeric literal
    if (local_ptr == null_local_ptr && peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL || peek_token_type(parser) == NECRO_LEX_FLOAT_LITERAL)
    {
        local_ptr = parse_constant(parser);
    }

    // (pat)
    if (local_ptr == null_local_ptr)
    {
        if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
        {
            consume_token(parser);
            local_ptr = necro_try(NecroAST_LocalPtr, parse_pat(parser));
            if (local_ptr != null_local_ptr)
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
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_tuple_pattern(parser));
    }

    // list pattern [pat1, ... , patk]
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_list_pat(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroAST_LocalPtr(local_ptr);
    }

    restore_parse_state(parser, prev_state);
    restore_parser(parser, snapshot);
    return ok_NecroAST_LocalPtr(null_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_apats(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc          source_loc = peek_token(parser)->source_loc;
    NecroParse_DescentState prev_state = enter_parse_pattern_state(parser);
    NecroParser_Snapshot    snapshot   = snapshot_parser(parser);

    NecroAST_LocalPtr apat_local_ptr = necro_try(NecroAST_LocalPtr, parse_apat(parser));
    if (apat_local_ptr != null_local_ptr)
    {
        NecroAST_LocalPtr next_apat_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats(parser));
        NecroAST_LocalPtr apats_local_ptr     = necro_parse_ast_create_apats(&parser->ast.arena, source_loc, peek_token(parser)->source_loc, apat_local_ptr, next_apat_local_ptr);
        restore_parse_state(parser, prev_state);
        return ok_NecroAST_LocalPtr(apats_local_ptr);
    }

    if (apat_local_ptr != null_local_ptr)
    {
        restore_parse_state(parser, prev_state);
        return ok_NecroAST_LocalPtr(apat_local_ptr);
    }

    restore_parse_state(parser, prev_state);
    restore_parser(parser, snapshot);
    return ok_NecroAST_LocalPtr(null_local_ptr);
}

NecroAST_LocalPtr parse_constant(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr    local_ptr  = null_local_ptr;
    switch(peek_token_type(parser))
    {
    case NECRO_LEX_FLOAT_LITERAL:
        {
            NecroAST_Constant constant;
            constant.type           = (parser->descent_state == NECRO_DESCENT_PARSING_PATTERN) ? NECRO_AST_CONSTANT_FLOAT_PATTERN : NECRO_AST_CONSTANT_FLOAT;
            constant.double_literal = peek_token(parser)->double_literal;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, constant);
            consume_token(parser);
            return local_ptr;
        }
    case NECRO_LEX_INTEGER_LITERAL:
        {
            NecroAST_Constant constant;
            constant.type           = (parser->descent_state == NECRO_DESCENT_PARSING_PATTERN) ? NECRO_AST_CONSTANT_INTEGER_PATTERN : NECRO_AST_CONSTANT_INTEGER;
            constant.int_literal    = peek_token(parser)->int_literal;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, constant);
            consume_token(parser);
            return local_ptr;
        }

    case NECRO_LEX_STRING_LITERAL:
        {
            NecroAST_Constant constant;
            constant.type           = NECRO_AST_CONSTANT_STRING;
            constant.symbol         = peek_token(parser)->symbol;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, constant);
            consume_token(parser);
            return local_ptr;
        }
    case NECRO_LEX_CHAR_LITERAL:
        {
            NecroAST_Constant constant;
            constant.type           = (parser->descent_state == NECRO_DESCENT_PARSING_PATTERN) ? NECRO_AST_CONSTANT_CHAR_PATTERN : NECRO_AST_CONSTANT_CHAR;
            constant.char_literal   = peek_token(parser)->char_literal;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, constant);
            consume_token(parser);
            return local_ptr;
        }
    }
    restore_parser(parser, snapshot);
    return null_local_ptr;
}

NecroResult(NecroAST_LocalPtr) parse_parenthetical_expression(NecroParser* parser)
{
    // '('
    NecroSourceLoc       source_loc       = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot         = snapshot_parser(parser);
    NecroLexToken*       look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LEFT_PAREN)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    consume_token(parser);

    // Expression
    look_ahead_token            = peek_token(parser);
    NecroAST_LocalPtr local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (local_ptr == null_local_ptr)
    {
        return necro_paren_expression_failed_to_parse_error(source_loc, peek_token(parser)->end_loc);
    }

    // ')
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        if (look_ahead_token->token == NECRO_LEX_COMMA)
        {
            // if a comma is next then this may be a tuple, keep trying to parse
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
        return necro_paren_expression_missing_paren_error(source_loc, peek_token(parser)->end_loc);
    }

    // Finish
    consume_token(parser);
    return ok_NecroAST_LocalPtr(local_ptr);
}

// NecroAST_LocalPtr parse_unary_operation(NecroParser* parser)
// {
//     if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
//         return null_local_ptr;
//     NecroParser_Snapshot snapshot = snapshot_parser(parser);
//     restore_parser(parser, snapshot);
//     return null_local_ptr;
// }

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

NecroResult(NecroAST_LocalPtr) parse_binary_expression(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr, int min_precedence)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || lhs_local_ptr == null_local_ptr)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc       source_loc    = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot      = snapshot_parser(parser);
    NecroLexToken*       current_token = peek_token(parser);
    NecroSymbol          bin_op_symbol = current_token->symbol;
    NecroAST_BinOpType   bin_op_type   = token_to_bin_op_type(current_token->token);

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED || bin_op_behaviors[bin_op_type].precedence < min_precedence)
    {
        return ok_NecroAST_LocalPtr(lhs_local_ptr);
    }

    NecroAST_LocalPtr bin_op_local_ptr = null_local_ptr;
    NecroAST_LocalPtr rhs_local_ptr    = null_local_ptr;
    while (true)
    {
        current_token = peek_token(parser);
        bin_op_symbol = current_token->symbol;
        bin_op_type   = token_to_bin_op_type(current_token->token);

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

        rhs_local_ptr                                       = necro_try(NecroAST_LocalPtr, parse_expression_impl(parser, NECRO_NO_BINARY_LOOK_AHEAD));
        NecroLexToken*           look_ahead_token           = peek_token(parser);
        NecroAST_BinOpType       look_ahead_bin_op_type     = token_to_bin_op_type(look_ahead_token->token);
        NecroParse_BinOpBehavior look_ahead_bin_op_behavior = bin_op_behaviors[look_ahead_bin_op_type];

        while (look_ahead_bin_op_type != NECRO_BIN_OP_UNDEFINED &&
               look_ahead_bin_op_behavior.precedence >= next_min_precedence)
        {

            rhs_local_ptr = necro_try(NecroAST_LocalPtr, parse_binary_expression(parser, rhs_local_ptr, next_min_precedence));

            if (look_ahead_token != peek_token(parser))
            {
                look_ahead_token           = peek_token(parser);
                look_ahead_bin_op_type     = token_to_bin_op_type(look_ahead_token->token);
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

        bin_op_local_ptr = necro_parse_ast_create_bin_op(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, lhs_local_ptr, rhs_local_ptr, bin_op_type, bin_op_symbol);
    }

    if (bin_op_local_ptr == null_local_ptr ||
        lhs_local_ptr    == null_local_ptr ||
        rhs_local_ptr    == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Finish
    NecroAST_Node* bin_op_ast_node = ast_get_node(&parser->ast, bin_op_local_ptr);
    bin_op_ast_node->bin_op.lhs    = lhs_local_ptr;
    bin_op_ast_node->bin_op.rhs    = rhs_local_ptr;
    return parse_binary_operation(parser, bin_op_local_ptr); // Try to keep evaluating lower precedence tokens
}

NecroResult(NecroAST_LocalPtr) parse_binary_operation(NecroParser* parser, NecroAST_LocalPtr lhs_local_ptr)
{
    NecroLexToken*     current_token = peek_token(parser);
    NecroAST_BinOpType bin_op_type   = token_to_bin_op_type(current_token->token);
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        return ok_NecroAST_LocalPtr(lhs_local_ptr);
    }
    return parse_binary_expression(parser, lhs_local_ptr, bin_op_behaviors[bin_op_type].precedence);
}

NecroResult(NecroAST_LocalPtr) parse_op_left_section(NecroParser* parser)
{
    // '('
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
    {
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Left expression
    NecroAST_LocalPtr left = necro_try(NecroAST_LocalPtr, parse_expression_impl(parser, NECRO_NO_BINARY_LOOK_AHEAD));
    if (left == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Op
    const NecroLexToken*     bin_op_token  = peek_token(parser);
    const NecroSymbol        bin_op_symbol = bin_op_token->symbol;
    const NecroAST_BinOpType bin_op_type   = token_to_bin_op_type(bin_op_token->token);

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // ')'
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_op_left_section(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, left, bin_op_type, bin_op_symbol);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_op_right_section(NecroParser* parser)
{
    // '('
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
    {
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Op
    const NecroLexToken*     bin_op_token  = peek_token(parser);
    const NecroSymbol        bin_op_symbol = bin_op_token->symbol;
    const NecroAST_BinOpType bin_op_type   = token_to_bin_op_type(bin_op_token->token);
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Right expression
    const NecroAST_LocalPtr right = necro_try(NecroAST_LocalPtr, parse_expression_impl(parser, NECRO_NO_BINARY_LOOK_AHEAD));
    if (right == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ')'
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_op_right_section(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, right, bin_op_type, bin_op_symbol);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_l_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot  = snapshot_parser(parser);
    NecroAST_LocalPtr    local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_pattern_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_do(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_let_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_if_then_else_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_function_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_lambda(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try(NecroAST_LocalPtr, parse_case(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroAST_LocalPtr(local_ptr);
    }

    restore_parser(parser, snapshot);
    return ok_NecroAST_LocalPtr(null_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_if_then_else_expression(NecroParser* parser)
{
    // if
    if (peek_token_type(parser) != NECRO_LEX_IF)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc       if_source_loc          = peek_token(parser)->source_loc;
    // NecroParser_Snapshot snapshot               = snapshot_parser(parser);
    consume_token(parser);

    // if expression
    NecroLexToken*    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr if_local_ptr     = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (if_local_ptr == null_local_ptr)
    {
        return necro_if_failed_to_parse_error(if_source_loc, look_ahead_token->end_loc);
    }

    // then
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_THEN)
    {
        return necro_if_missing_then_error(if_source_loc, look_ahead_token->end_loc);
    }
    consume_token(parser);

    // then expression
    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr then_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (then_local_ptr == null_local_ptr)
    {
        return necro_if_missing_expr_after_then_error(if_source_loc, look_ahead_token->end_loc);
    }

    // else
    look_ahead_token = peek_token(parser);
    if (peek_token_type(parser) != NECRO_LEX_ELSE)
    {
        return necro_if_missing_else_error(if_source_loc, look_ahead_token->end_loc);
    }
    consume_token(parser);

    // else expression
    look_ahead_token = peek_token(parser);
    NecroAST_LocalPtr else_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (else_local_ptr == null_local_ptr)
    {
        return necro_if_missing_expr_after_else_error(if_source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroAST_LocalPtr if_then_else_local_ptr = necro_parse_ast_create_if_then_else(&parser->ast.arena, if_source_loc, peek_token(parser)->end_loc, if_local_ptr, then_local_ptr, else_local_ptr);
    return ok_NecroAST_LocalPtr(if_then_else_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_lambda(NecroParser* parser)
{
    // '\'
    NecroSourceLoc             back_loc        = peek_token(parser)->source_loc;
    // NecroParser_Snapshot       snapshot        = snapshot_parser(parser);
    const NecroLexToken*       backslash_token = peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type      = backslash_token->token;
    if (token_type != NECRO_LEX_BACK_SLASH)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    consume_token(parser);

    // Apats
    NecroAST_LocalPtr apats_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats(parser));
    if (apats_local_ptr == null_local_ptr)
    {
        return necro_lambda_failed_to_parse_pattern_error(back_loc, peek_token(parser)->end_loc);
    }

    // Arrow
    const NECRO_LEX_TOKEN_TYPE next_token = peek_token_type(parser);
    if (next_token != NECRO_LEX_RIGHT_ARROW)
    {
        return necro_lambda_missing_arrow_error(back_loc, peek_token(parser)->end_loc);
    }
    consume_token(parser);

    // Expression
    NecroAST_LocalPtr expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        return necro_lambda_failed_to_parse_error(back_loc, peek_token(parser)->end_loc);
    }

    // Finish
    NecroAST_LocalPtr lambda_local_ptr = necro_parse_ast_create_lambda(&parser->ast.arena, back_loc, peek_token(parser)->end_loc, apats_local_ptr, expression_local_ptr);
    return ok_NecroAST_LocalPtr(lambda_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_list(NecroParser* parser, NECRO_LEX_TOKEN_TYPE token_divider, ParseFunc parse_func)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // Item
    NecroSourceLoc       source_loc     = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot       = snapshot_parser(parser);
    NecroAST_LocalPtr    item_local_ptr = necro_try(NecroAST_LocalPtr, parse_func(parser));
    if (item_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Divider and Next
    NecroAST_LocalPtr next_item_local_ptr = null_local_ptr;
    if (peek_token_type(parser) == token_divider)
    {
        consume_token(parser); // consume divider token
        next_item_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, token_divider, parse_func));
    }

    // Finish
    NecroAST_LocalPtr list_local_ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, item_local_ptr, next_item_local_ptr);
    return ok_NecroAST_LocalPtr(list_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_do_item(NecroParser* parser)
{
    NecroParser_Snapshot snapshot       = snapshot_parser(parser);
    NECRO_LEX_TOKEN_TYPE top_token_type = peek_token_type(parser);
    switch (top_token_type)
    {
    case NECRO_LEX_IDENTIFIER:
    {
        NecroSourceLoc       source_loc     = peek_token(parser)->source_loc;
        const NecroLexToken* variable_token = peek_token(parser);
        consume_token(parser); // Consume variable token
        if (variable_token->token == NECRO_LEX_IDENTIFIER && peek_token_type(parser) == NECRO_LEX_LEFT_ARROW)
        {
            consume_token(parser); // consume left arrow
            NecroAST_LocalPtr expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
            if (expression_local_ptr != null_local_ptr)
            {
                NecroAST_LocalPtr bind_assignment_local_ptr = necro_parse_ast_create_bind_assignment(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, variable_token->symbol, expression_local_ptr);
                return ok_NecroAST_LocalPtr(bind_assignment_local_ptr);
            }
            else
            {
                return necro_do_bind_failed_to_parse_error(variable_token->source_loc, peek_token(parser)->end_loc);
            }
        }
        restore_parser(parser, snapshot);
    } break;

    case NECRO_LEX_LET:
    {
        consume_token(parser); // Consume 'LET'
        if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACE)
            return necro_do_let_expected_left_brace_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
        consume_token(parser); // Consume '{'
        NecroAST_LocalPtr declarations = necro_try(NecroAST_LocalPtr, parse_declarations_list(parser));
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
            return necro_do_let_expected_right_brace_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
        consume_token(parser); // Consume '}'
        return ok_NecroAST_LocalPtr(declarations);
    } break;

    } // switch (top_token_type)

    // Pattern bind
    {
        NecroSourceLoc    pat_loc       = peek_token(parser)->source_loc;
        NecroAST_LocalPtr pat_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat(parser));
        if (pat_local_ptr != null_local_ptr)
        {
            if (peek_token_type(parser) == NECRO_LEX_LEFT_ARROW)
            {
                consume_token(parser); // consume '<-'
                NecroAST_LocalPtr expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
                if (expression_local_ptr != null_local_ptr)
                {
                    NecroAST_LocalPtr bind_assignment_local_ptr = necro_parse_ast_create_pat_bind_assignment(&parser->ast.arena, pat_loc, peek_token(parser)->end_loc, pat_local_ptr, expression_local_ptr);
                    return ok_NecroAST_LocalPtr(bind_assignment_local_ptr);
                }
                else
                {
                    return necro_do_bind_failed_to_parse_error(pat_loc, peek_token(parser)->end_loc);
                }
            }
        }
        restore_parser(parser, snapshot);
    }

    // Expression
    {
        NecroAST_LocalPtr expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
        if (expression_local_ptr != null_local_ptr)
        {
            return ok_NecroAST_LocalPtr(expression_local_ptr);
        }
    }

    restore_parser(parser, snapshot);
    return parse_expression(parser);
}

NecroResult(NecroAST_LocalPtr) parse_do(NecroParser* parser)
{
    // do
    NecroParser_Snapshot       snapshot      = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE do_token_type = peek_token_type(parser);
    if (do_token_type != NECRO_LEX_DO)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // {
    const NECRO_LEX_TOKEN_TYPE l_curly_token_type = peek_token_type(parser);
    if (l_curly_token_type != NECRO_LEX_LEFT_BRACE)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Statements
    NecroAST_LocalPtr statement_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_SEMI_COLON, parse_do_item));
    if (statement_list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    //;
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token == NECRO_LEX_SEMI_COLON)
    {
        consume_token(parser);
    }

    // }
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
    {
        return necro_do_missing_right_brace_error(look_ahead_token->source_loc, look_ahead_token->end_loc);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr do_local_ptr = necro_parse_ast_create_do(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroAST_LocalPtr(do_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_expression_list(NecroParser* parser)
{
    // [
    NecroSourceLoc             source_loc       = peek_token(parser)->source_loc;
    NecroParser_Snapshot       snapshot         = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE brace_token_type = peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    consume_token(parser);

    // Expressions
    NecroAST_LocalPtr statement_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_expression));

    // ]
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACKET)
    {
        if (look_ahead_token->token == NECRO_LEX_DOUBLE_DOT)
        {
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
        else
        {
            return necro_list_missing_right_bracket_error(source_loc, look_ahead_token->end_loc);
        }
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr list_local_ptr = necro_parse_ast_create_expression_list(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroAST_LocalPtr(list_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_expression_array(NecroParser* parser)
{
    // {
    NecroParser_Snapshot       snapshot         = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE brace_token_type = peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Expressions
    NecroAST_LocalPtr statement_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_expression));

    // }
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
    {
        if (look_ahead_token->token == NECRO_LEX_DOUBLE_DOT)
        {
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
        else
        {
            return necro_array_missing_right_brace_error(source_loc, look_ahead_token->end_loc);
        }
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr list_local_ptr = necro_parse_ast_create_expression_array(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroAST_LocalPtr(list_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_pat_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // aexpr
    NecroAST_LocalPtr local_ptr = necro_try(NecroAST_LocalPtr, parse_application_expression(parser));
    if (local_ptr != null_local_ptr)
        return ok_NecroAST_LocalPtr(local_ptr);
    restore_parser(parser, snapshot);

    // Wildcard
    local_ptr = necro_try(NecroAST_LocalPtr, parse_wildcard(parser));
    if (local_ptr != null_local_ptr)
        return ok_NecroAST_LocalPtr(local_ptr);
    restore_parser(parser, snapshot);

    return ok_NecroAST_LocalPtr(null_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_expressions_for_pattern_expression(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // Pat expression
    NecroSourceLoc       source_loc           = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot             = snapshot_parser(parser);
    NecroAST_LocalPtr    expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Next Pat expression
    NecroAST_LocalPtr next_expression_local_ptr = necro_try(NecroAST_LocalPtr, parse_expressions_for_pattern_expression(parser));

    // Finish
    NecroAST_LocalPtr expressions_local_ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, expression_local_ptr, next_expression_local_ptr);
    return ok_NecroAST_LocalPtr(expressions_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_pattern_expression(NecroParser* parser)
{
    // pat
    // NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    if (peek_token_type(parser) != NECRO_LEX_PAT)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Expressions
    NecroAST_LocalPtr expression_list = necro_try(NecroAST_LocalPtr, parse_expressions_for_pattern_expression(parser));
    if (expression_list == null_local_ptr)
    {
        return necro_pattern_empty_expression_list_error(source_loc, peek_token(parser)->end_loc);
    }

    // Finish
    NecroAST_LocalPtr pat_expression_local_ptr = necro_parse_ast_create_pat_expr(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, expression_list);
    return ok_NecroAST_LocalPtr(pat_expression_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_arithmetic_sequence(NecroParser* parser)
{
    // [
    NecroParser_Snapshot       snapshot         = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE brace_token_type = peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    NecroAST_LocalPtr from = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    NecroAST_LocalPtr then = null_local_ptr;
    NecroAST_LocalPtr to   = null_local_ptr;

    // from
    if (from == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ,
    if (peek_token_type(parser) == NECRO_LEX_COMMA)
    {
        consume_token(parser); // consume ','
        then = necro_try(NecroAST_LocalPtr, parse_expression(parser));
        if (then == null_local_ptr)
            return necro_arithmetic_sequence_failed_to_parse_then_error(source_loc, peek_token(parser)->end_loc);
    }

    // ..
    if (peek_token_type(parser) != NECRO_LEX_DOUBLE_DOT)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // to
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        to = necro_try(NecroAST_LocalPtr, parse_expression(parser));
        if (to == null_local_ptr)
            return necro_arithmetic_sequence_failed_to_parse_to_error(source_loc, peek_token(parser)->end_loc);
    }

    if (then != null_local_ptr && to == null_local_ptr)
    {
        // this means we're probably just a normal list expression
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ]
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        return necro_arithmetic_sequence_missing_right_bracket_error(source_loc, peek_token(parser)->end_loc);
    }
    consume_token(parser);

    // Finish
    NecroAST_ArithmeticSeqType seq_type = NECRO_ARITHMETIC_ENUM_FROM_THEN_TO;
    if (to == null_local_ptr)
    {
        if (then == null_local_ptr)
            seq_type = NECRO_ARITHMETIC_ENUM_FROM;
        else
            assert(false);
    }
    else if (then == null_local_ptr)
    {
        seq_type = NECRO_ARITHMETIC_ENUM_FROM_TO;
    }
    else
    {
        seq_type = NECRO_ARITHMETIC_ENUM_FROM_THEN_TO;
    }
    NecroAST_LocalPtr arithmetic_local_ptr = necro_parse_ast_create_arithmetic_sequence(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, from, then, to, seq_type);
    return ok_NecroAST_LocalPtr(arithmetic_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_tuple(NecroParser* parser)
{
    // (
    NecroParser_Snapshot       snapshot   = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type != NECRO_LEX_LEFT_PAREN)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Expressions
    NecroAST_LocalPtr statement_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_expression));
    if (statement_list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // )
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        return necro_tuple_missing_paren_error(source_loc, look_ahead_token->end_loc);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr list_local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroAST_LocalPtr(list_local_ptr);
}

//=====================================================
// Case / Patterns
//=====================================================
NecroResult(NecroAST_LocalPtr) parse_case_alternative(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE || peek_token_type(parser) == NECRO_LEX_WHERE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc source_loc = peek_token(parser)->source_loc;

    // pat
    NecroAST_LocalPtr pat_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat(parser));
    if (pat_local_ptr == null_local_ptr)
        return necro_case_alternative_expected_pattern_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);

    // ->
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_ARROW)
        return necro_case_alternative_expected_arrow_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
    consume_token(parser);

    // body expr
    NecroAST_LocalPtr body_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (body_local_ptr == null_local_ptr)
        return necro_case_alternative_expected_expression_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);

    // finish
    NecroAST_LocalPtr case_alternative_local_ptr = necro_parse_ast_create_case_alternative(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, pat_local_ptr, body_local_ptr);
    return ok_NecroAST_LocalPtr(case_alternative_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_case(NecroParser* parser)
{
    // case
    NecroParser_Snapshot       snapshot   = snapshot_parser(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type != NECRO_LEX_CASE)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // expr
    NecroAST_LocalPtr case_expr_local_ptr = necro_try(NecroAST_LocalPtr, parse_expression(parser));
    if (case_expr_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // 'of'
    NecroLexToken* look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_OF)
        return necro_case_alternative_expected_of_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
    consume_token(parser);

    // '{'
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LEFT_BRACE)
        return necro_case_alternative_expected_left_brace_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
    consume_token(parser);

    // alternatives
    NecroAST_LocalPtr alternative_list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_SEMI_COLON, parse_case_alternative));
    if (alternative_list_local_ptr == null_local_ptr)
        return necro_case_alternative_empty_error(source_loc, peek_token(parser)->end_loc);

    // '}'
    look_ahead_token = peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
        return necro_case_alternative_expected_right_brace_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr case_local_ptr = necro_parse_ast_create_case(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, case_expr_local_ptr, alternative_list_local_ptr);
    return ok_NecroAST_LocalPtr(case_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_qconop(NecroParser* parser, NECRO_CON_TYPE con_type);
NecroResult(NecroAST_LocalPtr) parse_lpat(NecroParser* parser);

// TODO: This needs to work like BinOps and is currently just wrong.
NecroResult(NecroAST_LocalPtr) parse_oppat(NecroParser* parser)
{
    NecroParse_DescentState prev_state = enter_parse_pattern_state(parser);
    NecroParser_Snapshot    snapshot   = snapshot_parser(parser);
    NecroSourceLoc          source_loc = peek_token(parser)->source_loc;

    // Left pat
    NecroAST_LocalPtr left = necro_try(NecroAST_LocalPtr, parse_lpat(parser));
    if (left == null_local_ptr)
    {
        restore_parse_state(parser, prev_state);
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Op
    NecroAST_LocalPtr op = necro_try(NecroAST_LocalPtr, parse_qconop(parser, NECRO_VAR_VAR));
    if (op == null_local_ptr)
    {
        restore_parse_state(parser, prev_state);
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Right pat
    NecroAST_LocalPtr right = necro_try(NecroAST_LocalPtr, parse_pat(parser));
    if (right == null_local_ptr)
    {
        restore_parse_state(parser, prev_state);
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_bin_op_sym(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, left, op, right);
    restore_parse_state(parser, prev_state);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_pat(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParse_DescentState prev_state = enter_parse_pattern_state(parser);
    NecroParser_Snapshot    snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr       ptr        = necro_try(NecroAST_LocalPtr, parse_oppat(parser));
    if (ptr == null_local_ptr)
    {
        ptr = necro_try(NecroAST_LocalPtr, parse_lpat(parser));
    }

    if (ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
    }

    restore_parse_state(parser, prev_state);

    return ok_NecroAST_LocalPtr(ptr);
}

NecroParse_DescentState enter_parse_pattern_state(NecroParser* parser)
{
    NecroParse_DescentState prev_state = parser->descent_state;
    parser->descent_state              = NECRO_DESCENT_PARSING_PATTERN;
    return prev_state;
}

void restore_parse_state(NecroParser* parser, NecroParse_DescentState prev_state)
{
    parser->descent_state = prev_state;
}

// Pat
NecroResult(NecroAST_LocalPtr) parse_lpat(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc          source_loc = peek_token(parser)->source_loc;
    NecroParse_DescentState prev_state = enter_parse_pattern_state(parser);
    NecroParser_Snapshot    snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr       ptr        = null_local_ptr;

    // gcon apat1 ... apatk
    {
        ptr = necro_try(NecroAST_LocalPtr, parse_gcon(parser, NECRO_CON_VAR));
        if (ptr != null_local_ptr)
        {
            NecroAST_LocalPtr apats_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats(parser));
            if (apats_local_ptr != null_local_ptr)
            {
                // NecroAST_LocalPtr constr_ptr      = null_local_ptr;
                // NecroAST_Node*    constr_node     = ast_alloc_node_local_ptr(parser, &constr_ptr);
                // constr_node->type                 = NECRO_AST_CONSTRUCTOR;
                // constr_node->constructor.conid    = ptr;
                // constr_node->constructor.arg_list = apats_local_ptr;
                // ptr                               = constr_ptr;
                ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, ptr, apats_local_ptr);
            }
        }
    }

    // apat
    if (ptr == null_local_ptr)
    {
        ptr = necro_try(NecroAST_LocalPtr, parse_apat(parser));
    }

    // Literal
    if (ptr == null_local_ptr)
    {
        // Literal
        if (peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL || peek_token_type(parser) == NECRO_LEX_FLOAT_LITERAL)
        {
            ptr = parse_constant(parser);
        }
    }

    if (ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
    }

    restore_parse_state(parser, prev_state);
    return ok_NecroAST_LocalPtr(ptr);
}

NecroResult(NecroAST_LocalPtr) parse_gcon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroParser_Snapshot snapshot      = snapshot_parser(parser);
    NecroAST_LocalPtr    con_local_ptr = null_local_ptr;

    // ()
    if (peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol symbol = peek_token(parser)->symbol;
        con_local_ptr      = necro_parse_ast_create_conid(&parser->ast.arena, peek_token(parser)->source_loc, peek_token(parser)->end_loc, symbol, con_type);
        consume_token(parser);
    }

    // Don't need this?!?!?
    // // []
    // else if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    // {
    //     // NecroSymbol symbol = peek_token(parser)->symbol;
    //     consume_token(parser);
    //     if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    //     {
    //         restore_parser(parser, snapshot);
    //         return null_local_ptr;
    //     }
    //     consume_token(parser);
    //     NecroAST_Node* node  = ast_alloc_node_local_ptr(parser, &con_local_ptr);
    //     node->type           = NECRO_AST_CONID;
    //     node->conid.symbol   = necro_intern_string(parser->intern, "[]");
    //     node->conid.con_type = con_type;
    // }

    // Constructor
    else
    {
        con_local_ptr = parse_qcon(parser, con_type);
    }
    if (con_local_ptr == null_local_ptr)
        restore_parser(parser, snapshot);
    return ok_NecroAST_LocalPtr(con_local_ptr);
}

NecroAST_LocalPtr parse_consym(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroAST_BinOpType bin_op_type = token_to_bin_op_type(peek_token_type(parser));
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
        return null_local_ptr;
    NecroSourceLoc    source_loc    = peek_token(parser)->source_loc;
    NecroAST_LocalPtr con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, peek_token(parser)->symbol, con_type);
    consume_token(parser);
    return con_local_ptr;
}

NecroAST_LocalPtr parse_qconsym(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    // Save until we actuall have modules...
 	// [ modid . ] consym
    return parse_consym(parser, con_type);
}

// conid | (consym)
NecroAST_LocalPtr parse_con(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroParser_Snapshot snapshot         = snapshot_parser(parser);
    NecroAST_LocalPtr    con_local_ptr    = null_local_ptr;
    NecroLexToken*       look_ahead_token = peek_token(parser);

    if (look_ahead_token->token == NECRO_LEX_TYPE_IDENTIFIER)
    {
        con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, peek_token(parser)->source_loc, peek_token(parser)->end_loc, look_ahead_token->symbol, con_type);
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
    // Save until we actuall have modules...
 	// [ modid . ] conid
    return parse_con(parser, con_type);
}

// TODO: Get colon patterns working!
// Is this needed!?
// // gconsym 	-> 	: | qconsym
// NecroAST_LocalPtr parse_gconsym(NecroParser* parser)
// {
//     // NecroParser_Snapshot snapshot         = snapshot_parser(parser);
//     // NecroAST_LocalPtr    con_local_ptr    = null_local_ptr;
//     // NecroLexToken*       look_ahead_token = peek_token(parser);
//     if (peek_token_type(parser) == NECRO_LEX_COLON)
//     {
//     }
// }

// qconop 	-> 	gconsym | `qcon`
NecroResult(NecroAST_LocalPtr) parse_qconop(NecroParser* parser, NECRO_CON_TYPE con_type)
{

    NecroParser_Snapshot snapshot      = snapshot_parser(parser);
    NecroAST_LocalPtr    con_local_ptr = null_local_ptr;
    NecroSourceLoc       source_loc    = peek_token(parser)->source_loc;

    con_local_ptr = parse_consym(parser, con_type);
    if (con_local_ptr != null_local_ptr)
        return ok_NecroAST_LocalPtr(con_local_ptr);

    if (peek_token_type(parser) != NECRO_LEX_ACCENT)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    consume_token(parser);
    con_local_ptr = parse_qcon(parser, con_type);

    if (con_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    if (peek_token_type(parser) != NECRO_LEX_ACCENT)
        return necro_fn_op_expected_accent_error(source_loc, peek_token(parser)->end_loc);

    consume_token(parser);
    return ok_NecroAST_LocalPtr(con_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_list_pat(NecroParser* parser)
{
    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // [
    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Pats
    NecroAST_LocalPtr list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_pat));
    if (list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ]
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_expression_list(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, list_local_ptr);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_tuple_pattern(NecroParser* parser)
{
    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // (
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // Pats
    NecroAST_LocalPtr list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_pat));
    if (list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // )
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, list_local_ptr);
    return ok_NecroAST_LocalPtr(local_ptr);
}

//=====================================================
// Types
//=====================================================
NecroAST_LocalPtr              parse_tycon(NecroParser* parser, NECRO_CON_TYPE con_type);
NecroAST_LocalPtr              parse_tyvar(NecroParser* parser, NECRO_VAR_TYPE var_type);
NecroAST_LocalPtr              parse_simpletype(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_btype(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_tuple_type(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_list_type(NecroParser* parser);
NecroResult(NecroAST_LocalPtr) parse_atype(NecroParser* parser, NECRO_VAR_TYPE tyvar_var_type);
NecroResult(NecroAST_LocalPtr) parse_constr(NecroParser* parser);

NecroResult(NecroAST_LocalPtr) parse_top_level_data_declaration(NecroParser* parser)
{
    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // data
    if (peek_token_type(parser) != NECRO_LEX_DATA)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // simpletype
    NecroAST_LocalPtr simpletype = parse_simpletype(parser);
    if (simpletype == null_local_ptr)
        return necro_data_expected_type_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);

    // =
    if (peek_token_type(parser) != NECRO_LEX_ASSIGN)
        return necro_data_expected_assign_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
    consume_token(parser);

    // constructor_list
    NecroAST_LocalPtr constructor_list = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_PIPE, parse_constr));
    if (constructor_list == null_local_ptr)
        return necro_data_expected_data_con_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_data_declaration(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, simpletype, constructor_list);
    return ok_NecroAST_LocalPtr(ptr);
}

NecroResult(NecroAST_LocalPtr) parse_atype_list(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // tyvar
    NecroSourceLoc    source_loc = peek_token(parser)->source_loc;
    NecroAST_LocalPtr atype      = necro_try(NecroAST_LocalPtr, parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    if (atype == null_local_ptr)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // next
    NecroAST_LocalPtr next = necro_try(NecroAST_LocalPtr, parse_atype_list(parser));

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, atype, next);
    return ok_NecroAST_LocalPtr(ptr);
}

NecroResult(NecroAST_LocalPtr) parse_constr(NecroParser* parser)
{
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;

    // tycon
    NecroAST_LocalPtr tycon = parse_tycon(parser, NECRO_CON_DATA_DECLARATION);
    if (tycon == null_local_ptr)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    // atype list
    NecroAST_LocalPtr atype_list = necro_try(NecroAST_LocalPtr, parse_atype_list(parser));

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, tycon, atype_list);
    return ok_NecroAST_LocalPtr(ptr);
}

NecroAST_LocalPtr parse_tyvar_list(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;

    // tyvar
    NecroAST_LocalPtr tyvar = parse_tyvar(parser, NECRO_VAR_TYPE_VAR_DECLARATION);
    if (tyvar == null_local_ptr)
        return null_local_ptr;

    // next
    NecroAST_LocalPtr next = parse_tyvar_list(parser);

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, tyvar, next);
    return ptr;
}

NecroAST_LocalPtr parse_simpletype(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;

    // tycon
    NecroAST_LocalPtr tycon = parse_tycon(parser, NECRO_CON_TYPE_DECLARATION);
    if (tycon == null_local_ptr)
        return null_local_ptr;

    // tyvar list
    NecroAST_LocalPtr tyvars = parse_tyvar_list(parser);

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_simple_type(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, tycon, tyvars);
    return ptr;
}

NecroResult(NecroAST_LocalPtr) parse_type(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr   ty_ptr      = necro_try(NecroAST_LocalPtr, parse_btype(parser));
    if (ty_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    // Next Type after arrow
    if (peek_token_type(parser) == NECRO_LEX_RIGHT_ARROW)
    {
        consume_token(parser);
        NecroAST_LocalPtr next_after_arrow_ptr = necro_try(NecroAST_LocalPtr, parse_type(parser));
        if (next_after_arrow_ptr == null_local_ptr)
            return necro_type_expected_type_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
        NecroAST_LocalPtr ptr = necro_parse_ast_create_function_type(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, ty_ptr, next_after_arrow_ptr);
        return ok_NecroAST_LocalPtr(ptr);
    }
    else
    {
        return ok_NecroAST_LocalPtr(ty_ptr);
    }
}

NecroResult(NecroAST_LocalPtr) parse_btype_go(NecroParser* parser, NecroAST_LocalPtr left)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || left == null_local_ptr)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr    atype      = necro_try(NecroAST_LocalPtr, parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    if (atype == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(left);
    }

    NecroAST_LocalPtr ptr = necro_parse_ast_create_type_app(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, left, atype);
    return parse_btype_go(parser, ptr);
}

NecroResult(NecroAST_LocalPtr) parse_btype(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    atype    = necro_try(NecroAST_LocalPtr, parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    NecroAST_LocalPtr    btype    = necro_try(NecroAST_LocalPtr, parse_btype_go(parser, atype));
    if (btype == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    return ok_NecroAST_LocalPtr(btype);
}

NecroResult(NecroAST_LocalPtr) parse_atype(NecroParser* parser, NECRO_VAR_TYPE tyvar_var_type)
{
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    ptr      = null_local_ptr;

    // tycon
    ptr = parse_tycon(parser, NECRO_CON_TYPE_VAR);

    // tyvar
    if (ptr == null_local_ptr)
    {
        ptr = parse_tyvar(parser, tyvar_var_type);
    }

    if (ptr == null_local_ptr)
    {
        ptr = necro_try(NecroAST_LocalPtr, parse_tuple_type(parser));
    }

    // [type]
    if (ptr == null_local_ptr)
    {
        ptr = necro_try(NecroAST_LocalPtr, parse_list_type(parser));
    }

    // (type)
    if (ptr == null_local_ptr)
    {
        if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
        {
            consume_token(parser);
            ptr = necro_try(NecroAST_LocalPtr, parse_type(parser));
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            {
                restore_parser(parser, snapshot);
                ptr = null_local_ptr;
            }
            consume_token(parser);
        }
    }

    return ok_NecroAST_LocalPtr(ptr);
}

NecroAST_LocalPtr parse_gtycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroSourceLoc       source_loc         = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot           = snapshot_parser(parser);
    NecroAST_LocalPtr    type_con_local_ptr = null_local_ptr;

    // ()
    if (peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol    symbol  = peek_token(parser)->symbol;
        type_con_local_ptr     = necro_parse_ast_create_conid(&parser->ast.arena, peek_token(parser)->source_loc, peek_token(parser)->end_loc, symbol, con_type);
        consume_token(parser);
    }
    // []
    else if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    {
        consume_token(parser);
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
        {
            restore_parser(parser, snapshot);
            return null_local_ptr;
        }
        type_con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, necro_intern_string(parser->intern, "[]"), con_type);
        consume_token(parser);
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
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    // ()
    if (peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol       symbol = peek_token(parser)->symbol;
        NecroAST_LocalPtr local  = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, symbol, con_type);
        consume_token(parser);
        return local;
    }

    // IDENTIFIER
    NecroParser_Snapshot snapshot           = snapshot_parser(parser);
    NecroLexToken*       look_ahead_token   = peek_token(parser);

    if (look_ahead_token->token != NECRO_LEX_TYPE_IDENTIFIER)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    NecroAST_LocalPtr type_con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, look_ahead_token->symbol, con_type);
    consume_token(parser);
    return type_con_local_ptr;
}

NecroAST_LocalPtr parse_qtycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    // Save until we actually have modules...
 	// [ modid . ] conid
    return parse_tycon(parser, con_type);
}

NecroAST_LocalPtr parse_tyvar(NecroParser* parser, NECRO_VAR_TYPE var_type)
{
    NecroParser_Snapshot snapshot           = snapshot_parser(parser);
    NecroLexToken*       look_ahead_token   = peek_token(parser);

    if (look_ahead_token->token != NECRO_LEX_IDENTIFIER)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // Finish
    NecroAST_LocalPtr type_var_local_ptr = necro_parse_ast_create_var(&parser->ast.arena, peek_token(parser)->source_loc, peek_token(parser)->end_loc, look_ahead_token->symbol, var_type, null_local_ptr);
    consume_token(parser);
    return type_var_local_ptr;
}

NecroResult(NecroAST_LocalPtr) parse_list_type(NecroParser* parser)
{
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroAST_LocalPtr    ptr        = null_local_ptr;

    // [
    if (peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // type
    ptr = necro_try(NecroAST_LocalPtr, parse_type(parser));
    if (ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ]
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        return necro_type_list_expected_right_bracket(source_loc, peek_token(parser)->end_loc);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr list_ptr = necro_parse_ast_create_conid(&parser->ast.arena, peek_token(parser)->source_loc, peek_token(parser)->end_loc, necro_intern_string(parser->intern, "[]"), NECRO_CON_TYPE_VAR);
    NecroAST_LocalPtr app_ptr  = necro_parse_ast_create_type_app(&parser->ast.arena, peek_token(parser)->source_loc, peek_token(parser)->end_loc, list_ptr, ptr);
    return ok_NecroAST_LocalPtr(app_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_tuple_type(NecroParser* parser)
{
    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // (
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // types
    NecroAST_LocalPtr list_local_ptr = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_type));
    if (list_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // require at least k = 2
    {
        NecroAST_Node* list_node = ast_get_node(&parser->ast, list_local_ptr);
        if (list_node->list.next_item == null_local_ptr)
        {
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
    }

    // )
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, list_local_ptr);
    return ok_NecroAST_LocalPtr(local_ptr);
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

NecroResult(NecroAST_LocalPtr) parse_simple_class(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;

    // qtycl
    NecroAST_LocalPtr conid = parse_qtycls(parser, NECRO_CON_TYPE_VAR);
    if (conid == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // varid
    NecroAST_LocalPtr varid = parse_tyvar(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (varid == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_class_context(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, conid, varid);
    return ok_NecroAST_LocalPtr(local_ptr);
}

bool necro_is_empty_list_node(NecroParser* parser, NecroAST_LocalPtr list_ptr)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;
    if (list_ptr == null_local_ptr) return true;
    NecroAST_Node* list_node = ast_get_node(&parser->ast, list_ptr);
    return list_node == NULL || list_node->list.item == null_local_ptr;
}

NecroResult(NecroAST_LocalPtr) parse_context(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroParser_Snapshot snapshot = snapshot_parser(parser);
    if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    {
        consume_token(parser);
        NecroAST_LocalPtr class_list = necro_try(NecroAST_LocalPtr, parse_list(parser, NECRO_LEX_COMMA, parse_simple_class));
        if (class_list == null_local_ptr || necro_is_empty_list_node(parser, class_list))
        {
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
        if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
        {
            restore_parser(parser, snapshot);
            return ok_NecroAST_LocalPtr(null_local_ptr);
        }
        // return write_error_and_restore(parser, snapshot, "Expected ')' at end of context, but found %s", necro_lex_token_type_string(peek_token_type(parser)));
        consume_token(parser);
        return ok_NecroAST_LocalPtr(class_list);
    }
    else
    {
        return parse_simple_class(parser);
    }
}

NecroResult(NecroAST_LocalPtr) parse_type_signature(NecroParser* parser, NECRO_SIG_TYPE sig_type)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);
    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;

    // var
    NecroAST_LocalPtr var;
    if (sig_type == NECRO_SIG_DECLARATION)
        var = parse_variable(parser, NECRO_VAR_SIG);
    else if (sig_type == NECRO_SIG_TYPE_CLASS)
        var = parse_variable(parser, NECRO_VAR_CLASS_SIG);
    else
        var = null_local_ptr;

    if (var == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ::
    if (peek_token_type(parser) != NECRO_LEX_DOUBLE_COLON)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    consume_token(parser);

    // context
    NecroParser_Snapshot context_snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    context          = necro_try(NecroAST_LocalPtr, parse_context(parser));

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
    NecroAST_LocalPtr type = necro_try(NecroAST_LocalPtr, parse_type(parser));
    if (type == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_type_signature(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, var, context, type, sig_type);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_class_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_type_signature(parser, NECRO_SIG_TYPE_CLASS));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_simple_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat_assignment(parser));
    }

    return ok_NecroAST_LocalPtr(declaration_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_class_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);

    NecroAST_LocalPtr declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_class_declaration(parser));

    if (declaration_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    NecroAST_LocalPtr next_decl = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
    {
        consume_token(parser); // consume SemiColon token
        next_decl = necro_try(NecroAST_LocalPtr, parse_class_declarations_list(parser));
    }

    NecroAST_LocalPtr decl_local_ptr = necro_parse_ast_create_decl(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, declaration_local_ptr, next_decl);
    return ok_NecroAST_LocalPtr(decl_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_type_class_declaration(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // class
    if (peek_token_type(parser) != NECRO_LEX_CLASS)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // context
    NecroParser_Snapshot context_snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    context          = necro_try(NecroAST_LocalPtr, parse_context(parser));

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
    if (tycls == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // tyvar
    NecroAST_LocalPtr tyvar = parse_tyvar(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (tyvar == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // declarations
    NecroAST_LocalPtr declarations = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        consume_token(parser);
        if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACE)
        {
            consume_token(parser);
            declarations = necro_try(NecroAST_LocalPtr, parse_class_declarations_list(parser));
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                return necro_class_expected_right_brace_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
            consume_token(parser);
        }
    }

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_class_declaration(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, context, tycls, tyvar, declarations);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_instance_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON    ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroAST_LocalPtr declaration_local_ptr = null_local_ptr;

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_simple_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_apats_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_pat_assignment(parser));
    }

    return ok_NecroAST_LocalPtr(declaration_local_ptr);
}

NecroResult(NecroAST_LocalPtr) parse_instance_declaration_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON    ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroSourceLoc source_loc = peek_token(parser)->source_loc;

    // Declaration
    NecroParser_Snapshot snapshot              = snapshot_parser(parser);
    NecroAST_LocalPtr    declaration_local_ptr = necro_try(NecroAST_LocalPtr, parse_instance_declaration(parser));
    if (declaration_local_ptr == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // ;
    NecroAST_LocalPtr next_decl = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
    {
        consume_token(parser);
        next_decl = necro_try(NecroAST_LocalPtr, parse_instance_declaration_list(parser));
    }

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_decl(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, declaration_local_ptr, next_decl);
    return ok_NecroAST_LocalPtr(local_ptr);
}

NecroAST_LocalPtr parse_inst_constr(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);

    // (
    if (peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // tycon
    NecroAST_LocalPtr tycon = parse_gtycon(parser, NECRO_CON_TYPE_VAR);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || tycon == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // tyvar list
    NecroAST_LocalPtr ty_var_list = parse_tyvar_list(parser);
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }

    // )
    if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        restore_parser(parser, snapshot);
        return null_local_ptr;
    }
    consume_token(parser);

    // Finish
    NecroAST_LocalPtr ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, tycon, ty_var_list);
    return ptr;
}

NecroAST_LocalPtr parse_inst(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroSourceLoc       source_loc = peek_token(parser)->source_loc;
    NecroParser_Snapshot snapshot   = snapshot_parser(parser);

    // gtycon
    NecroAST_LocalPtr ptr = parse_gtycon(parser, NECRO_CON_TYPE_VAR);
    if (ptr != null_local_ptr)
        return ptr;

    ptr = parse_inst_constr(parser);
    if (ptr != null_local_ptr)
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
                NecroAST_LocalPtr con_ptr  = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, necro_intern_string(parser->intern, "[]"), NECRO_CON_TYPE_VAR);
                NecroAST_LocalPtr list_ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, con_ptr, ptr);
                consume_token(parser);
                return list_ptr;
            }
        }
        restore_parser(parser, snapshot);
        ptr = null_local_ptr;
    }
    if (ptr != null_local_ptr)
        return ptr;

    // TODO: Finish
    // // (a -> b)
    // if (peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    // {
    //     consume_token(parser);
    //     NecroAST_LocalPtr ty_var_1_ptr = parse_tyvar(parser, NECRO_VAR_TYPE_VAR_DECLARATION);
    //     if (ty_var_1_ptr == null_local_ptr)
    //     {
    //         restore_parser(parser, snapshot);
    //         return null_local_ptr;
    //     }
    //     if (peek_token_type(parser) != NECRO_LEX_RIGHT_ARROW)
    //     {
    //         restore_parser(parser, snapshot);
    //         return null_local_ptr;
    //     }
    //     consume_token(parser);
    //     NecroAST_LocalPtr ty_var_2_ptr = parse_tyvar(parser, NECRO_VAR_TYPE_VAR_DECLARATION);
    //     if (ty_var_2_ptr == null_local_ptr)
    //     {
    //         restore_parser(parser, snapshot);
    //         return null_local_ptr;
    //     }
    //     if (peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    //     {
    //         restore_parser(parser, snapshot);
    //         return null_local_ptr;
    //     }
    //     consume_token(parser);

    //     NecroAST_LocalPtr con_ptr  = null_local_ptr;
    //     NecroAST_Node*    con_node = ast_alloc_node_local_ptr(parser, &con_ptr);
    //     con_node->type             = NECRO_AST_CONID;
    //     con_node->conid.symbol     = necro_intern_string(parser->intern, "(->)");
    //     con_node->conid.con_type   = NECRO_CON_TYPE_DECLARATION;

    //     NecroAST_LocalPtr list2_ptr  = null_local_ptr;
    //     NecroAST_Node*    list2_node = ast_alloc_node_local_ptr(parser, &list2_ptr);
    //     list2_node->type             = NECRO_AST_LIST_NODE;
    //     list2_node->list.item        = ty_var_2_ptr;
    //     list2_node->list.next_item   = null_local_ptr;

    //     NecroAST_LocalPtr list1_ptr  = null_local_ptr;
    //     NecroAST_Node*    list1_node = ast_alloc_node_local_ptr(parser, &list1_ptr);
    //     list1_node->type             = NECRO_AST_LIST_NODE;
    //     list1_node->list.item        = ty_var_1_ptr;
    //     list2_node->list.next_item   = list2_ptr;

    //     NecroAST_LocalPtr arr_ptr  = null_local_ptr;
    //     NecroAST_Node*    node     = ast_alloc_node_local_ptr(parser, &arr_ptr);
    //     node->type                 = NECRO_AST_CONSTRUCTOR;
    //     node->constructor.conid    = con_ptr;
    //     node->constructor.arg_list = list1_ptr;
    //     return arr_ptr;
    // }

    return ptr;
}

NecroResult(NecroAST_LocalPtr) parse_type_class_instance(NecroParser* parser)
{
    if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroAST_LocalPtr(null_local_ptr);

    NecroParser_Snapshot snapshot = snapshot_parser(parser);

    // instance
    if (peek_token_type(parser) != NECRO_LEX_INSTANCE)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = peek_token(parser)->source_loc;
    consume_token(parser);

    // context
    NecroParser_Snapshot context_snapshot = snapshot_parser(parser);
    NecroAST_LocalPtr    context          = necro_try(NecroAST_LocalPtr, parse_context(parser));

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
    if (qtycls == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // inst
    NecroAST_LocalPtr inst = parse_inst(parser);
    if (inst == null_local_ptr)
    {
        restore_parser(parser, snapshot);
        return ok_NecroAST_LocalPtr(null_local_ptr);
    }

    // declarations
    NecroAST_LocalPtr declarations = null_local_ptr;
    if (peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        consume_token(parser);
        if (peek_token_type(parser) == NECRO_LEX_LEFT_BRACE)
        {
            consume_token(parser);
            declarations = necro_try(NecroAST_LocalPtr, parse_instance_declaration_list(parser));
            if (peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                return necro_instance_expected_right_brace_error(peek_token(parser)->source_loc, peek_token(parser)->end_loc);
            consume_token(parser);
        }
    }

    // Finish
    NecroAST_LocalPtr local_ptr = necro_parse_ast_create_instance(&parser->ast.arena, source_loc, peek_token(parser)->end_loc, context, qtycls, inst, declarations);
    return ok_NecroAST_LocalPtr(local_ptr);
}
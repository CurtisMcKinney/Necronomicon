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

static const size_t MAX_LOCAL_PTR = ((size_t)((NecroParseAstLocalPtr)-1));

///////////////////////////////////////////////////////
// NecroParser
///////////////////////////////////////////////////////
typedef enum
{
    NECRO_PARSING,
    NECRO_PARSING_PATTERN,
    NECRO_PARSING_DONE
} NECRO_PARSE_STATE;

typedef struct NecroParser
{
    NecroParseAstArena ast;
    NecroLexToken*     tokens;
    size_t             current_token;
    NECRO_PARSE_STATE  descent_state;
    NecroIntern*       intern;
    bool               parsing_pat_assignment; // TODO / HACK; Find a better way to delineate
} NecroParser;

NecroParser necro_parser_create(NecroLexToken* tokens, size_t num_tokens, NecroIntern* intern, NecroSymbol module_name)
{
    return (NecroParser)
    {
        .current_token          = 0,
        .ast                    = (NecroParseAstArena) { .arena = necro_arena_create(num_tokens * sizeof(NecroParseAst)), .root = 0, .module_name = module_name },
        .tokens                 = tokens,
        .descent_state          = NECRO_PARSING,
        .intern                 = intern,
        .parsing_pat_assignment = false,
    };
}

void necro_parser_destroy(NecroParser* parser)
{
    // Ownership of ast is passed out before deconstruction
    parser->current_token          = 0;
    parser->tokens                 = NULL;
    parser->descent_state          = NECRO_PARSING_DONE;
    parser->intern                 = NULL;
    parser->parsing_pat_assignment = false;
}

NecroParseAstArena necro_parse_ast_arena_empty()
{
    return (NecroParseAstArena) { .arena = necro_arena_empty(), .root = 0, .module_name = NULL };
}

NecroParseAstArena necro_parse_ast_arena_create(size_t capacity)
{
    return (NecroParseAstArena) { .arena = necro_arena_create(capacity), .root = 0, .module_name = NULL };
}

void necro_parse_ast_arena_destroy(NecroParseAstArena* ast)
{
    necro_arena_destroy(&ast->arena);
}

///////////////////////////////////////////////////////
// Utility
///////////////////////////////////////////////////////
NecroParseAst* necro_parse_ast_get_node(NecroParseAstArena* ast, NecroParseAstLocalPtr local_ptr)
{
    assert(ast != NULL);
    assert(local_ptr != null_local_ptr);
    return ((NecroParseAst*) ast->arena.region) + local_ptr;
}

NecroParseAst* necro_parse_ast_get_root_node(NecroParseAstArena* ast)
{
    assert(ast != NULL);
    return (NecroParseAst*) ast->arena.region;
}

// Snap shot of parser state for back tracking
typedef struct
{
    size_t current_token;
    size_t ast_size;
} NecroParserSnapshot;

inline NecroParserSnapshot necro_parse_snapshot(NecroParser* parser)
{
    return (NecroParserSnapshot) { parser->current_token, parser->ast.arena.size };
}

inline void necro_parse_restore(NecroParser* parser, NecroParserSnapshot snapshot)
{
    parser->current_token = snapshot.current_token;
    parser->ast.arena.size = snapshot.ast_size;
}

inline NecroLexToken* necro_parse_peek_token(NecroParser* parser)
{
    return parser->tokens + parser->current_token;
}

inline NECRO_LEX_TOKEN_TYPE necro_parse_peek_token_type(NecroParser* parser)
{
    return necro_parse_peek_token(parser)->token;
}

inline void necro_parse_consume_token(NecroParser* parser)
{
    ++parser->current_token;
}

const char* necro_bin_op_name(NECRO_BIN_OP_TYPE type)
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

const char* necro_var_type_string(NECRO_VAR_TYPE var_type)
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

const char* necro_con_type_string(NECRO_CON_TYPE con_type)
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
NecroParseAst* necro_parse_ast_alloc(NecroArena* arena, NecroParseAstLocalPtr* local_ptr)
{
    NecroParseAst* node = (NecroParseAst*)necro_arena_alloc(arena, sizeof(NecroParseAst), NECRO_ARENA_REALLOC);
    const size_t offset = node - ((NecroParseAst*)arena->region);
    assert(offset < MAX_LOCAL_PTR);
    assert((((NecroParseAst*)arena->region) + offset) == node);
    *local_ptr = offset;
    return node;
}

void necro_parse_ast_print_go(NecroParseAstArena* ast, NecroParseAst* ast_node, uint32_t depth)
{
    assert(ast != NULL);
    assert(ast_node != NULL);
    for (uint32_t i = 0;  i < depth; ++i)
    {
        printf(STRING_TAB);
    }

    switch(ast_node->type)
    {
    case NECRO_AST_BIN_OP:
        // puts(bin_op_name(ast_node->bin_op.type));
        printf("(%s)\n", ast_node->bin_op.symbol->str);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->bin_op.lhs), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->bin_op.rhs), depth + 1);
        break;

    case NECRO_AST_CONSTANT:
        switch (ast_node->constant.type)
        {
        case NECRO_AST_CONSTANT_FLOAT:
        case NECRO_AST_CONSTANT_FLOAT_PATTERN:
            printf("(%f)\n", ast_node->constant.double_literal);
            break;
        case NECRO_AST_CONSTANT_INTEGER:
        case NECRO_AST_CONSTANT_TYPE_INT:
        case NECRO_AST_CONSTANT_INTEGER_PATTERN:
#if WIN32
            printf("(%lli)\n", ast_node->constant.int_literal);
#else
            printf("(%li)\n", ast_node->constant.int_literal);
#endif
            break;
        case NECRO_AST_CONSTANT_STRING:
        {
            const char* string = ast_node->constant.symbol->str;
            if (string)
                printf("(\"%s\")\n", string);
        }
        break;
        case NECRO_AST_CONSTANT_CHAR:
        case NECRO_AST_CONSTANT_CHAR_PATTERN:
            printf("(\'%c\')\n", ast_node->constant.char_literal);
            break;
        // case NECRO_AST_CONSTANT_BOOL:
        //     printf("(%s)\n", ast_node->constant.boolean_literal ? " True" : "False");
        //     break;
        default:
            assert(false);
            break;
        }
        break;

    case NECRO_AST_IF_THEN_ELSE:
        puts("(If then else)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->if_then_else.if_expr), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->if_then_else.then_expr), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->if_then_else.else_expr), depth + 1);
        break;

    case NECRO_AST_TOP_DECL:
        puts("(Top Declaration)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->top_declaration.declaration), depth + 1);
        if (ast_node->top_declaration.next_top_decl != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->top_declaration.next_top_decl), depth);
        }
        break;

    case NECRO_AST_DECL:
        puts("(Declaration)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->declaration.declaration_impl), depth + 1);
        if (ast_node->declaration.next_declaration != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->declaration.next_declaration), depth);
        }
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        printf("(Assignment: %s)\n", ast_node->simple_assignment.variable_name->str);
        if (ast_node->simple_assignment.initializer != null_local_ptr)
        {
            print_white_space(depth * 2);
            printf("<\n");
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->simple_assignment.initializer), depth);
            print_white_space(depth * 2);
            printf(">\n");
        }
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->simple_assignment.rhs), depth + 1);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        puts("(Right Hand Side)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->right_hand_side.expression), depth + 1);
        if (ast_node->right_hand_side.declarations != null_local_ptr)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(STRING_TAB);
            }
            puts(STRING_TAB STRING_TAB "where");
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->right_hand_side.declarations), depth + 3);
        }
        break;

    case NECRO_AST_LET_EXPRESSION:
        puts("(Let)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->let_expression.declarations), depth + 1);
        if (ast_node->right_hand_side.declarations != null_local_ptr)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(STRING_TAB);
            }
            puts(STRING_TAB STRING_TAB "in");
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->let_expression.expression), depth + 3);
        }
        break;


    case NECRO_AST_FUNCTION_EXPRESSION:
        puts("(fexp)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->fexpression.aexp), depth + 1);
        if (ast_node->fexpression.next_fexpression != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->fexpression.next_fexpression), depth + 1);
        }
        break;

    case NECRO_AST_VARIABLE:
        {
            const char* variable_string = ast_node->variable.symbol->str;
            if (variable_string)
            {
                printf("(varid: %s, vtype: %s)\n", variable_string, necro_var_type_string(ast_node->variable.var_type));
            }
            else
            {
                printf("(varid: \"\", vtype: %s)\n", necro_var_type_string(ast_node->variable.var_type));
            }
        }
        break;

    case NECRO_AST_APATS:
        puts("(Apat)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->apats.apat), depth + 1);
        if (ast_node->apats.next_apat != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->apats.next_apat), depth);
        }
        break;

    case NECRO_AST_WILDCARD:
        puts("(_)");
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
        printf("(Apats Assignment: %s)\n", ast_node->apats_assignment.variable_name->str);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->apats_assignment.apats), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->apats_assignment.rhs), depth + 1);
        break;

    case NECRO_AST_PAT_ASSIGNMENT:
        puts("(Pat Assignment)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->pat_assignment.pat), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->pat_assignment.rhs), depth + 1);
        break;

    case NECRO_AST_LAMBDA:
        puts("\\(lambda)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->lambda.apats), depth + 1);
        for (uint32_t i = 0;  i < (depth + 1); ++i)
        {
            printf(STRING_TAB);
        }
        puts("->");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->lambda.expression), depth + 2);
        break;

    case NECRO_AST_DO:
        puts("(do)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->do_statement.statement_list), depth + 1);
        break;

    case NECRO_AST_FOR_LOOP:
        puts("(for)");
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->for_loop.range_init), depth + 1);
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->for_loop.value_init), depth + 1);
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->for_loop.index_apat), depth + 1);
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->for_loop.value_apat), depth + 1);
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->for_loop.expression), depth + 1);
        break;

    case NECRO_AST_PAT_EXPRESSION:
        puts("(pat)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->pattern_expression.expressions), depth + 1);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        puts("([])");
        if (ast_node->expression_list.expressions != null_local_ptr)
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->expression_list.expressions), depth + 1);
        break;

    case NECRO_AST_EXPRESSION_ARRAY:
        puts("({})");
        if (ast_node->expression_array.expressions != null_local_ptr)
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->expression_array.expressions), depth + 1);
        break;

    case NECRO_AST_TUPLE:
        puts("(tuple)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->expression_list.expressions), depth + 1);
        break;

    case NECRO_AST_LIST_NODE:
        printf("\r"); // clear current line
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->list.item), depth);
        if (ast_node->list.next_item != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->list.next_item), depth);
        }
        break;

    case NECRO_BIND_ASSIGNMENT:
        printf("(Bind: %s)\n", ast_node->bind_assignment.variable_name->str);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->bind_assignment.expression), depth + 1);
        break;

    case NECRO_PAT_BIND_ASSIGNMENT:
        printf("(Pat Bind)\n");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->pat_bind_assignment.pat), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->pat_bind_assignment.expression), depth + 1);
        break;

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        {
            switch(ast_node->arithmetic_sequence.type)
            {
            case NECRO_ARITHMETIC_ENUM_FROM:
                puts("(EnumFrom)");
                necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->arithmetic_sequence.from), depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_TO:
                puts("(EnumFromTo)");
                necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->arithmetic_sequence.from), depth + 1);
                necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->arithmetic_sequence.to), depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_THEN_TO:
                puts("(EnumFromThenTo)");
                necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->arithmetic_sequence.from), depth + 1);
                necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->arithmetic_sequence.then), depth + 1);
                necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->arithmetic_sequence.to), depth + 1);
                break;
            default:
                assert(false);
                break;
            }
        }
        break;

    case NECRO_AST_CASE:
        puts("case");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->case_expression.expression), depth + 1);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(STRING_TAB);
        puts("of");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->case_expression.alternatives), depth + 1);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        printf("\r"); // clear current line
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->case_alternative.pat), depth + 0);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(STRING_TAB);
        puts("->");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->case_alternative.body), depth + 1);
        break;

    case NECRO_AST_CONID:
    {
        const char* con_string = ast_node->conid.symbol->str;
        if (con_string)
            printf("(conid: %s, ctype: %s)\n", con_string, necro_con_type_string(ast_node->conid.con_type));
        else
            printf("(conid: \"\", ctype: %s)\n", necro_con_type_string(ast_node->conid.con_type));
        break;
    }

    case NECRO_AST_DATA_DECLARATION:
        puts("(data)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->data_declaration.simpletype), depth + 1);
        for (uint32_t i = 0;  i < depth; ++i) printf(STRING_TAB);
        puts(" = ");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->data_declaration.constructor_list), depth + 1);
        break;

    case NECRO_AST_SIMPLE_TYPE:
        puts("(simple type)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->simple_type.type_con), depth + 1);
        if (ast_node->simple_type.type_var_list != null_local_ptr)
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->simple_type.type_var_list), depth + 2);
        break;

    case NECRO_AST_CONSTRUCTOR:
        puts("(constructor)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->constructor.conid), depth + 1);
        if (ast_node->constructor.arg_list != null_local_ptr)
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->constructor.arg_list), depth + 1);
        break;

    case NECRO_AST_TYPE_APP:
        puts("(type app)");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_app.ty), depth + 1);
        if (ast_node->type_app.next_ty != null_local_ptr)
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_app.next_ty), depth + 1);
        break;

    case NECRO_AST_BIN_OP_SYM:
        printf("(%s)\n", necro_parse_ast_get_node(ast, ast_node->bin_op_sym.op)->conid.symbol->str);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->bin_op_sym.left), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->bin_op_sym.right), depth + 1);
        break;

    case NECRO_AST_OP_LEFT_SECTION:
        printf("(LeftSection %s)\n", necro_bin_op_name(ast_node->op_left_section.type));
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->op_left_section.left), depth + 1);
        break;

    case NECRO_AST_OP_RIGHT_SECTION:
        printf("(%s RightSection)\n", necro_bin_op_name(ast_node->op_right_section.type));
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->op_right_section.right), depth + 1);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        printf("\r");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_signature.var), depth + 0);
        for (uint32_t i = 0;  i < depth + 1; ++i) printf(STRING_TAB);
        puts("::");
        if (ast_node->type_signature.context != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_signature.context), depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(STRING_TAB);
            puts("=>");
        }
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_signature.type), depth + 1);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        printf("\r");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_context.conid), depth + 0);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_context.varid), depth + 0);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        puts("(class)");
        if (ast_node->type_class_declaration.context != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_declaration.context), depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(STRING_TAB);
            puts("=>");
        }
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_declaration.tycls), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_declaration.tyvar), depth + 1);
        if (ast_node->type_class_declaration.declarations != null_local_ptr)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(STRING_TAB);
            puts("where");
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_declaration.declarations), depth + 2);
        }
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        puts("(instance)");
        if (ast_node->type_class_instance.context != null_local_ptr)
        {
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_instance.context), depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(STRING_TAB);
            puts("=>");
        }
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_instance.qtycls), depth + 1);
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_instance.inst), depth + 1);
        if (ast_node->type_class_declaration.declarations != null_local_ptr)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(STRING_TAB);
            puts("where");
            necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->type_class_declaration.declarations), depth + 2);
        }
        break;

    case NECRO_AST_FUNCTION_TYPE:
        puts("(");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->function_type.type), depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(STRING_TAB);
        puts("->");
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast_node->function_type.next_on_arrow), depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(STRING_TAB);
        puts(")");
        break;

    default:
        puts("(Undefined)");
        break;
    }

}

void necro_parse_ast_print(NecroParseAstArena* ast)
{
    if (ast->root == null_local_ptr)
    {
        puts("(Empty AST)");
    }
    else
    {
        necro_parse_ast_print_go(ast, necro_parse_ast_get_node(ast, ast->root), 0);
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
} NecroParseBinOpAssociativity;

typedef struct
{
    int precedence;
    NecroParseBinOpAssociativity associativity;
} NecroParseBinOpBehavior;

static const NecroParseBinOpBehavior bin_op_behaviors[NECRO_BIN_OP_COUNT + 1] = {
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

// =====================================================
// Recursive Descent Parser
// =====================================================

typedef enum
{
    NECRO_NO_BINARY_LOOK_AHEAD,
    NECRO_BINARY_LOOK_AHEAD,
} NECRO_BINARY_LOOK_AHEAD_TYPE;

NecroResult(NecroParseAstLocalPtr) necro_parse_expression_go(NecroParser* parser, NECRO_BINARY_LOOK_AHEAD_TYPE look_ahead_binary);

static inline NecroResult(NecroParseAstLocalPtr) necro_parse_expression(NecroParser* parser)
{
    return necro_parse_expression_go(parser, NECRO_BINARY_LOOK_AHEAD);
}

//=====================================================
// Forward declarations
//=====================================================
typedef NecroResult(NecroParseAstLocalPtr) (*NecroParseFunc)(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_top_declarations(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_declaration(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_declarations(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_list(NecroParser* parser, NECRO_LEX_TOKEN_TYPE token_divider, NecroParseFunc parse_func);
NecroResult(NecroParseAstLocalPtr) necro_parse_parenthetical_expression(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_tuple(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_l_expression(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_if_then_else_expression(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_binary_operation(NecroParser* parser, NecroParseAstLocalPtr lhs_local_ptr);
NecroResult(NecroParseAstLocalPtr) necro_parse_op_left_section(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_op_right_section(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_simple_assignment(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_apats_assignment(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_pat_assignment(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_right_hand_side(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_apats(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_lambda(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_do(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_expression_list(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_pattern_expression(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_arithmetic_sequence(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_expression_array(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_case(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_for_loop(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_pat(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_gcon(NecroParser* parser, NECRO_CON_TYPE var_type);
NecroResult(NecroParseAstLocalPtr) necro_parse_list_pat(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_tuple_pattern(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_top_level_data_declaration(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_type_signature(NecroParser* parser, NECRO_SIG_TYPE sig_type);
NecroResult(NecroParseAstLocalPtr) necro_parse_type_class_declaration(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_type_class_instance(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_initializer(NecroParser* parser);
// NecroResult(NecroAST_LocalPtr) parse_unary_operation(NecroParser* parser);

NecroParseAstLocalPtr necro_parse_constant(NecroParser* parser);
NecroParseAstLocalPtr necro_parse_con(NecroParser* parser,  NECRO_CON_TYPE var_type);
NecroParseAstLocalPtr necro_parse_qcon(NecroParser* parser, NECRO_CON_TYPE var_type);

#define NECRO_INITIALIZER_TOKEN NECRO_LEX_TILDE

NECRO_PARSE_STATE necro_parse_enter_state(NecroParser* parser);
void              necro_parse_restore_state(NecroParser* parser, NECRO_PARSE_STATE prev_state);

NecroResult(void) necro_parse(NecroCompileInfo info, NecroIntern* intern, NecroLexTokenVector* tokens, NecroSymbol module_name, NecroParseAstArena* out_ast)
{
    NecroParser           parser    = necro_parser_create(tokens->data, tokens->length, intern, module_name);
    *out_ast = parser.ast;
    NecroParseAstLocalPtr local_ptr = necro_try_map_result(NecroParseAstLocalPtr, void, necro_parse_top_declarations(&parser));
    parser.ast.root                 = local_ptr;
    // if ((necro_parse_peek_token_type(&parser) != NECRO_LEX_END_OF_STREAM && necro_parse_peek_token_type(&parser) != NECRO_LEX_SEMI_COLON) || (local_ptr == null_local_ptr && tokens->length > 0))
    if (necro_parse_peek_token_type(&parser) != NECRO_LEX_END_OF_STREAM || (local_ptr == null_local_ptr && tokens->length > 0))
    {
        NecroLexToken* look_ahead_token = necro_parse_peek_token(&parser);
        return necro_parse_error(look_ahead_token->source_loc, look_ahead_token->end_loc);
    }
    *out_ast = parser.ast;
    if (info.verbosity > 1 || (info.compilation_phase == NECRO_PHASE_PARSE && info.verbosity > 0))
    {
        necro_parse_ast_print(out_ast);
    }
    necro_parser_destroy(&parser);
    return ok_void();
}

static inline bool necro_is_parse_result_non_error_null(NecroResult(NecroParseAstLocalPtr) result)
{
    return result.type == NECRO_RESULT_OK && result.value == null_local_ptr;
}

NecroResult(NecroParseAstLocalPtr) necro_parse_top_declarations(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot                snapshot               = necro_parse_snapshot(parser);
    NecroResult(NecroParseAstLocalPtr) declarations_local_ptr = ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc                     source_loc             = necro_parse_peek_token(parser)->source_loc;

    // ;
    if (necro_is_parse_result_non_error_null(declarations_local_ptr) && necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
    {
        necro_parse_consume_token(parser);
        // declarations_local_ptr = parse_type_class_instance(parser);
        return necro_parse_top_declarations(parser);
    }

    // declaration
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = necro_parse_declaration(parser);
    }

    // data declaration
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = necro_parse_top_level_data_declaration(parser);
    }

    // type class declaration
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = necro_parse_type_class_declaration(parser);
    }

    // type class instance
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        declarations_local_ptr = necro_parse_type_class_instance(parser);
    }

    // NULL result
    if (necro_is_parse_result_non_error_null(declarations_local_ptr))
    {
        NecroSourceLoc parse_error_source_loc = necro_parse_peek_token(parser)->source_loc;
        NecroSourceLoc parse_error_end_loc    = necro_parse_peek_token(parser)->end_loc;
        necro_parse_restore(parser, snapshot);
        while (necro_parse_peek_token_type(parser) != NECRO_LEX_SEMI_COLON && necro_parse_peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
            necro_parse_consume_token(parser);
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            while (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
                necro_parse_consume_token(parser);
            NecroResult(NecroParseAstLocalPtr) next_top_decl_result = necro_parse_top_declarations(parser);
            if (next_top_decl_result.type == NECRO_RESULT_OK)
            {
                // return ok_NecroParseAstLocalPtr(null_local_ptr);
                return necro_error_map(void, NecroParseAstLocalPtr, necro_parse_error(parse_error_source_loc, parse_error_end_loc));
            }
            else
            {
                return necro_parse_error_cons(necro_parse_error(parse_error_source_loc, parse_error_end_loc).error, next_top_decl_result.error);
            }
        }
        else
        {
            // return ok_NecroParseAstLocalPtr(null_local_ptr);
            return necro_error_map(void, NecroParseAstLocalPtr, necro_parse_error(parse_error_source_loc, parse_error_end_loc));
        }
    }

    // Error result
    else if (declarations_local_ptr.type == NECRO_RESULT_ERROR)
    {
        while (necro_parse_peek_token_type(parser) != NECRO_LEX_SEMI_COLON && necro_parse_peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
            necro_parse_consume_token(parser);
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            while (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
                necro_parse_consume_token(parser);
            NecroResult(NecroParseAstLocalPtr) next_top_decl_result = necro_parse_top_declarations(parser);
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
        NecroParseAstLocalPtr declarations_local_ptr_value = declarations_local_ptr.value;
        NecroParseAstLocalPtr next_top_decl                = null_local_ptr;
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            necro_parse_consume_token(parser);
            next_top_decl = necro_try_result(NecroParseAstLocalPtr, necro_parse_top_declarations(parser));
        }
        NecroParseAstLocalPtr top_decl_local_ptr = necro_parse_ast_create_top_decl(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, declarations_local_ptr_value, next_top_decl);
        return ok_NecroParseAstLocalPtr(top_decl_local_ptr);
    }
}

NecroResult(NecroParseAstLocalPtr) necro_parse_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParseAstLocalPtr declaration_local_ptr = null_local_ptr;

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_type_signature(parser, NECRO_SIG_DECLARATION));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_simple_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat_assignment(parser));
    }

    return ok_NecroParseAstLocalPtr(declaration_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // List Item
    NecroSourceLoc                     source_loc            = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot                snapshot              = necro_parse_snapshot(parser);
    NecroResult(NecroParseAstLocalPtr) declaration_local_ptr = necro_parse_declaration(parser);

    // NULL result
    if (necro_is_parse_result_non_error_null(declaration_local_ptr))
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    // Error result
    else if (declaration_local_ptr.type == NECRO_RESULT_ERROR)
    {
        while (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE && necro_parse_peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
            necro_parse_consume_token(parser);
        while (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON || necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
            necro_parse_consume_token(parser);
        return declaration_local_ptr;
    }

    // OK result
    else
    {
        NecroParseAstLocalPtr declaration_local_value = declaration_local_ptr.value;
        // List Next
        NecroParseAstLocalPtr next_decl = null_local_ptr;
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
        {
            necro_parse_consume_token(parser);
            next_decl = necro_try_result(NecroParseAstLocalPtr, necro_parse_declarations_list(parser));
        }
        // Finish
        NecroParseAstLocalPtr decl_local_ptr = necro_parse_ast_create_decl(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, declaration_local_value, next_decl);
        return ok_NecroParseAstLocalPtr(decl_local_ptr);
    }
}

NecroResult(NecroParseAstLocalPtr) necro_parse_declarations(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // '{'
    bool is_list = necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_BRACE;
    if (is_list)
    {
        necro_parse_consume_token(parser);
    }

    // Declaration list
    NecroParseAstLocalPtr declarations_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_declarations_list(parser));
    if (declarations_list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // '}'
    if (is_list)
    {
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
        {
            NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
            NecroSourceLoc end_loc    = necro_parse_peek_token(parser)->end_loc;
            while (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE && necro_parse_peek_token_type(parser) != NECRO_LEX_END_OF_STREAM)
                necro_parse_consume_token(parser);
            while (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON || necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
                necro_parse_consume_token(parser);
            return necro_declarations_missing_right_brace_error(source_loc, end_loc);
        }
        necro_parse_consume_token(parser);
    }

    return ok_NecroParseAstLocalPtr(declarations_list_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_simple_assignment(NecroParser* parser)
{
    // Identifier
    NecroParserSnapshot        snapshot            = necro_parse_snapshot(parser);
    const NecroLexToken*       variable_name_token = necro_parse_peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type          = variable_name_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM || token_type != NECRO_LEX_IDENTIFIER)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc             source_loc          = necro_parse_peek_token(parser)->source_loc;
    NecroSourceLoc             end_loc             = necro_parse_peek_token(parser)->end_loc;
    necro_parse_consume_token(parser);

    // Initializer
    NecroParseAstLocalPtr initializer = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == NECRO_INITIALIZER_TOKEN)
    {
        NecroParserSnapshot initializer_snapshot = necro_parse_snapshot(parser);
        initializer = necro_try_result(NecroParseAstLocalPtr, necro_parse_initializer(parser));
        if (initializer == null_local_ptr)
            necro_parse_restore(parser, initializer_snapshot);
    }

    // '='
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_ASSIGN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Rhs
    NecroLexToken*        look_ahead_token = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr rhs_local_ptr    = necro_try_result(NecroParseAstLocalPtr, necro_parse_right_hand_side(parser));
    if (rhs_local_ptr == null_local_ptr)
    {
        return necro_simple_assignment_rhs_failed_to_parse_error(variable_name_token->source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroParseAstLocalPtr assignment_local_ptr = necro_parse_ast_create_simple_assignment(&parser->ast.arena, source_loc, end_loc, variable_name_token->symbol, rhs_local_ptr, initializer);
    return ok_NecroParseAstLocalPtr(assignment_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_apats_assignment(NecroParser* parser)
{
    // Identifier
    NecroParserSnapshot        snapshot            = necro_parse_snapshot(parser);
    const NecroLexToken*       variable_name_token = necro_parse_peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type          = variable_name_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM || token_type != NECRO_LEX_IDENTIFIER)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc             source_loc          = necro_parse_peek_token(parser)->source_loc;
    NecroSourceLoc             end_loc             = necro_parse_peek_token(parser)->end_loc;
    necro_parse_consume_token(parser);

    // Apats
    NecroParseAstLocalPtr apats_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats(parser));

    // '='
    if (apats_local_ptr == null_local_ptr || necro_parse_peek_token_type(parser) != NECRO_LEX_ASSIGN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Rhs
    NecroLexToken*        look_ahead_token = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr rhs_local_ptr    = necro_try_result(NecroParseAstLocalPtr, necro_parse_right_hand_side(parser));
    if (rhs_local_ptr == null_local_ptr)
    {
        return necro_apat_assignment_rhs_failed_to_parse_error(variable_name_token->source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroParseAstLocalPtr assignment_local_ptr = necro_parse_ast_create_apats_assignment(&parser->ast.arena, source_loc, end_loc, variable_name_token->symbol, apats_local_ptr, rhs_local_ptr);
    return ok_NecroParseAstLocalPtr(assignment_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_pat_assignment(NecroParser* parser)
{
    // Pattern
    NecroParserSnapshot        snapshot   = necro_parse_snapshot(parser);
    const NecroLexToken*       top_token  = necro_parse_peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = top_token->token;
    if (token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc       = necro_parse_peek_token(parser)->source_loc;
    parser->parsing_pat_assignment  = true; // TODO: Hack, Fix this with better way, likely with state field in parser struct
    NecroParseAstLocalPtr pat_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat(parser));
    parser->parsing_pat_assignment  = false;

    // '='
    if (pat_local_ptr == null_local_ptr || necro_parse_peek_token_type(parser) != NECRO_LEX_ASSIGN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Rhs
    NecroLexToken*        look_ahead_token = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr rhs_local_ptr    = necro_try_result(NecroParseAstLocalPtr, necro_parse_right_hand_side(parser));
    if (rhs_local_ptr == null_local_ptr)
    {
        return necro_pat_assignment_rhs_failed_to_parse_error(top_token->source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroParseAstLocalPtr assignment_local_ptr = necro_parse_ast_create_pat_assignment(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, pat_local_ptr, rhs_local_ptr);
    return ok_NecroParseAstLocalPtr(assignment_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_right_hand_side(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // Expression
    NecroSourceLoc        source_loc           = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot             = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Where
    NecroLexToken*        look_ahead_token       = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr declarations_local_ptr = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        necro_parse_consume_token(parser);
        declarations_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_declarations(parser));
        if (declarations_local_ptr == null_local_ptr)
            return necro_rhs_empty_where_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // Finish
    NecroParseAstLocalPtr rhs_local_ptr = necro_parse_ast_create_rhs(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, expression_local_ptr, declarations_local_ptr);
    return ok_NecroParseAstLocalPtr(rhs_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) parse_let_expression(NecroParser* parser)
{
    // 'let'
    // NecroParser_Snapshot snapshot         = snapshot_parser(parser);
    NecroLexToken*       look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LET)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // '{'
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_BRACE)
    {
        return necro_let_expected_left_brace_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
    }
    necro_parse_consume_token(parser);

    // Declarations
    NecroParseAstLocalPtr declarations_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_declarations(parser));
    if (declarations_local_ptr == null_local_ptr)
    {
        return necro_let_failed_to_parse_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // '}'
    bool consumed_right_brace = false;
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE)
    {
        necro_parse_consume_token(parser);
        consumed_right_brace = true;
    }

    // 'in'
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_IN)
    {
        return necro_let_missing_in_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
    }
    necro_parse_consume_token(parser);

    // 'in' Expression
    NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        return necro_let_empty_in_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // ';' && '}'
    if (!consumed_right_brace)
    {
        // ';'
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_SEMI_COLON)
        {
            return necro_let_expected_semicolon_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
        }
        necro_parse_consume_token(parser);

        // '}'
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
        {
            return necro_let_expected_right_brace_error(look_ahead_token->source_loc, necro_parse_peek_token(parser)->end_loc);
        }
        necro_parse_consume_token(parser);
    }

    // Finish
    NecroParseAstLocalPtr let_local_ptr = necro_parse_ast_create_let(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, expression_local_ptr, declarations_local_ptr);
    return ok_NecroParseAstLocalPtr(let_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_expression_go(NecroParser* parser, NECRO_BINARY_LOOK_AHEAD_TYPE look_ahead_binary)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot   snapshot  = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr local_ptr = null_local_ptr;

    // Unary Op
    // if (local_ptr == null_local_ptr)
    // {
    //     local_ptr = parse_unary_operation(parser);
    // }

    // L-Expression
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_l_expression(parser));
    }

    // BinOp
    if (look_ahead_binary == NECRO_BINARY_LOOK_AHEAD && local_ptr != null_local_ptr)
    {
        NecroParseAstLocalPtr bin_op_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_binary_operation(parser, local_ptr));
        if (bin_op_local_ptr != null_local_ptr)
            local_ptr = bin_op_local_ptr;
    }

    if (local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroParseAstLocalPtr necro_parse_variable(NecroParser* parser, NECRO_VAR_TYPE var_type)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    // Token
    NecroSourceLoc             source_loc     = necro_parse_peek_token(parser)->source_loc;
    NecroSourceLoc             end_loc        = necro_parse_peek_token(parser)->end_loc;
    NecroParserSnapshot        snapshot       = necro_parse_snapshot(parser);
    const NecroLexToken*       variable_token = necro_parse_peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type     = variable_token->token;
    necro_parse_consume_token(parser);

    // Variable Name
    if (token_type == NECRO_LEX_IDENTIFIER)
    {
        NecroParseAstLocalPtr varid_local_ptr = necro_parse_ast_create_var(&parser->ast.arena, source_loc, end_loc, variable_token->symbol, var_type, null_local_ptr, NECRO_TYPE_ZERO_ORDER);
        return varid_local_ptr;
    }

    // Parenthetical Variable Name, '('
    else if (token_type == NECRO_LEX_LEFT_PAREN)
    {
        // Symbol
        const NecroLexToken*       sym_variable_token = necro_parse_peek_token(parser);
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
            necro_parse_consume_token(parser);
            // ')'
            if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_PAREN)
            {
                necro_parse_consume_token(parser); // consume ")" token
                NecroParseAstLocalPtr varsym_local_ptr = necro_parse_ast_create_var(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, sym_variable_token->symbol, var_type, null_local_ptr, NECRO_TYPE_ZERO_ORDER);
                return varsym_local_ptr;
            }
            break;

        default:
            break;
        }
    }

    necro_parse_restore(parser, snapshot);
    return null_local_ptr;
}

///////////////////////////////////////////////////////
// Expressions
///////////////////////////////////////////////////////
NecroResult(NecroParseAstLocalPtr) necro_parse_application_expression(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot   snapshot  = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_parse_variable(parser, NECRO_VAR_VAR);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_gcon(parser, NECRO_CON_VAR));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_parse_constant(parser);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_op_left_section(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_op_right_section(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_parenthetical_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_tuple(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression_list(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression_array(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_arithmetic_sequence(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroParseAstLocalPtr(local_ptr);
    }

    necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_function_expression_go(NecroParser* parser, NecroParseAstLocalPtr left)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || left == null_local_ptr)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // aexpression
    NecroSourceLoc        source_loc  = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot    = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr aexpression = necro_try_result(NecroParseAstLocalPtr, necro_parse_application_expression(parser));
    if (aexpression == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(left);
    }

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_fexpr(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, left, aexpression);
    return necro_parse_function_expression_go(parser, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_function_expression(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // aexpression
    NecroParserSnapshot   snapshot    = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr expr        = necro_try_result(NecroParseAstLocalPtr, necro_parse_application_expression(parser));
    NecroParseAstLocalPtr aexpression = necro_try_result(NecroParseAstLocalPtr, necro_parse_function_expression_go(parser, expr));
    if (aexpression == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    return ok_NecroParseAstLocalPtr(aexpression);
}

///////////////////////////////////////////////////////
// Constant Constructors
///////////////////////////////////////////////////////
NecroResult(NecroParseAstLocalPtr) necro_parse_const_tuple(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_const_con(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_parenthetical_const_con(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_const_list(NecroParser* parser);

NecroResult(NecroParseAstLocalPtr) necro_parse_const_acon(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot   snapshot  = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_gcon(parser, NECRO_CON_VAR));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_parse_constant(parser);
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_parenthetical_const_con(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_tuple(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_list(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroParseAstLocalPtr(local_ptr);
    }

    necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_parenthetical_const_con(NecroParser* parser)
{
    // '('
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    NecroSourceLoc source_loc       = look_ahead_token->source_loc;
    if (look_ahead_token->token != NECRO_LEX_LEFT_PAREN)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroParserSnapshot  snapshot = necro_parse_snapshot(parser);
    necro_parse_consume_token(parser);

    // Con
    look_ahead_token                = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_con(parser));
    if (local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ')'
    look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        if (look_ahead_token->token == NECRO_LEX_COMMA)
        {
            // if a comma is next then this may be a tuple, keep trying to parse
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
        return necro_const_con_missing_right_brace(source_loc, look_ahead_token->end_loc);
    }
    necro_parse_consume_token(parser);

    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_const_con_go(NecroParser* parser, NecroParseAstLocalPtr left)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || left == null_local_ptr)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // aexpression
    NecroSourceLoc        source_loc  = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot    = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr aexpression = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_acon(parser));
    if (aexpression == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(left);
    }

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_fexpr(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, left, aexpression);
    return necro_parse_const_con_go(parser, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_const_con(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroParserSnapshot   snapshot    = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr con         = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_acon(parser));
    NecroParseAstLocalPtr aexpression = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_con_go(parser, con));
    if (aexpression == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    return ok_NecroParseAstLocalPtr(aexpression);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_const_tuple(NecroParser* parser)
{
    // '('
    NecroSourceLoc             source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot        snapshot   = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type != NECRO_LEX_LEFT_PAREN || token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    necro_parse_consume_token(parser);

    // Expressions
    NecroParseAstLocalPtr statement_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_const_con));
    if (statement_list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ')'
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        return necro_tuple_missing_paren_error(source_loc, look_ahead_token->end_loc);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr list_local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, statement_list_local_ptr);
    return ok_NecroParseAstLocalPtr(list_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_const_list(NecroParser* parser)
{
    if (true)
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    // [
    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Expressions
    NecroParseAstLocalPtr list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_const_con));

    // ]
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_expression_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, list_local_ptr);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_initializer(NecroParser* parser)
{
    // Initializer
    NecroParserSnapshot  snapshot         = necro_parse_snapshot(parser);
    NecroLexToken*       look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_INITIALIZER_TOKEN)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    necro_parse_consume_token(parser);

    // Constant Con
    NecroParseAstLocalPtr local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_const_con(parser));
    if (local_ptr != null_local_ptr)
        return ok_NecroParseAstLocalPtr(local_ptr);

    // default
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_IDENTIFIER && necro_parse_peek_token(parser)->symbol == necro_intern_string(parser->intern, "default"))
    {
        local_ptr = necro_parse_variable(parser, NECRO_VAR_VAR);
        if (local_ptr != null_local_ptr)
            return ok_NecroParseAstLocalPtr(local_ptr);
    }

    // Not an initializer
    necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

///////////////////////////////////////////////////////
// Patterns
///////////////////////////////////////////////////////
NecroResult(NecroParseAstLocalPtr) necro_parse_wildcard(NecroParser* parser)
{
    // Underscore
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_UNDER_SCORE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr wildcard_local_ptr = necro_parse_ast_create_wildcard(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc);
    return ok_NecroParseAstLocalPtr(wildcard_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_apat(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NECRO_PARSE_STATE    prev_state = necro_parse_enter_state(parser);
    NecroParserSnapshot  snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr local_ptr  = null_local_ptr;

    // var
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_parse_variable(parser, NECRO_VAR_DECLARATION);
    }

    // Initialized var
    if (local_ptr == null_local_ptr && necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    {
        necro_parse_consume_token(parser); // '('
        local_ptr = necro_parse_variable(parser, NECRO_VAR_DECLARATION);
        // Initializer
        NecroParseAstLocalPtr initializer = null_local_ptr;
        if (local_ptr != null_local_ptr && parser->parsing_pat_assignment && necro_parse_peek_token_type(parser) == NECRO_INITIALIZER_TOKEN)
        {
            // NecroParser_Snapshot initializer_snapshot = snapshot_parser(parser);
            initializer = necro_try_result(NecroParseAstLocalPtr, necro_parse_initializer(parser));
            if (initializer == null_local_ptr)
                necro_parse_restore(parser, snapshot);
            else
            {
                if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_PAREN)
                {
                    necro_parse_consume_token(parser);
                    // Set initializer
                    NecroParseAst* variable_node = necro_parse_ast_get_node(&parser->ast, local_ptr);
                    assert(variable_node != NULL);
                    assert(variable_node->type == NECRO_AST_VARIABLE);
                    variable_node->variable.initializer = initializer;
                }
                else
                {
                    //     return write_error_and_restore(parser, snapshot, "Expected ')' at end of pattern, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
                    local_ptr = null_local_ptr;
                    necro_parse_restore(parser, snapshot);
                }
            }
        }
        else
        {
            local_ptr = null_local_ptr;
            necro_parse_restore(parser, snapshot);
        }
    }

    // gcon
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_gcon(parser, NECRO_CON_VAR));
    }

    // _
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_wildcard(parser));
    }

    // Numeric literal
    if (local_ptr == null_local_ptr && ((necro_parse_peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL) || necro_parse_peek_token_type(parser) == NECRO_LEX_FLOAT_LITERAL))
    {
        local_ptr = necro_parse_constant(parser);
    }

    // (pat)
    if (local_ptr == null_local_ptr)
    {
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
        {
            necro_parse_consume_token(parser);
            local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat(parser));
            if (local_ptr != null_local_ptr)
            {
                if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_PAREN)
                {
                    necro_parse_consume_token(parser);
                }
                else
                {
                    //     return write_error_and_restore(parser, snapshot, "Expected ')' at end of pattern, but found: %s", necro_lex_token_type_string(peek_token_type(parser)));
                    local_ptr = null_local_ptr;
                    necro_parse_restore(parser, snapshot);
                }
            }
            else
            {
                local_ptr = null_local_ptr;
                necro_parse_restore(parser, snapshot);
            }
        }
    }

    // Tuple pattern (pat1, ... , patk)
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_tuple_pattern(parser));
    }

    // list pattern [pat1, ... , patk]
    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list_pat(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroParseAstLocalPtr(local_ptr);
    }

    necro_parse_restore_state(parser, prev_state);
    necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_apats(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;
    NECRO_PARSE_STATE   prev_state = necro_parse_enter_state(parser);
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);

    NecroParseAstLocalPtr apat_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apat(parser));
    if (apat_local_ptr != null_local_ptr)
    {
        NecroParseAstLocalPtr next_apat_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats(parser));
        NecroParseAstLocalPtr apats_local_ptr     = necro_parse_ast_create_apats(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->source_loc, apat_local_ptr, next_apat_local_ptr);
        necro_parse_restore_state(parser, prev_state);
        return ok_NecroParseAstLocalPtr(apats_local_ptr);
    }

    if (apat_local_ptr != null_local_ptr)
    {
        necro_parse_restore_state(parser, prev_state);
        return ok_NecroParseAstLocalPtr(apat_local_ptr);
    }

    necro_parse_restore_state(parser, prev_state);
    necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

NecroParseAstLocalPtr necro_parse_constant(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;

    NecroSourceLoc        source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr local_ptr  = null_local_ptr;
    switch(necro_parse_peek_token_type(parser))
    {
    case NECRO_LEX_FLOAT_LITERAL:
        {
            NecroParseAstConstant constant;
            constant.type           = (parser->descent_state == NECRO_PARSING_PATTERN) ? NECRO_AST_CONSTANT_FLOAT_PATTERN : NECRO_AST_CONSTANT_FLOAT;
            constant.double_literal = necro_parse_peek_token(parser)->double_literal;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, constant);
            necro_parse_consume_token(parser);
            return local_ptr;
        }
    case NECRO_LEX_INTEGER_LITERAL:
        {
            NecroParseAstConstant constant;
            constant.type           = (parser->descent_state == NECRO_PARSING_PATTERN) ? NECRO_AST_CONSTANT_INTEGER_PATTERN : NECRO_AST_CONSTANT_INTEGER;
            constant.int_literal    = necro_parse_peek_token(parser)->int_literal;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, constant);
            necro_parse_consume_token(parser);
            return local_ptr;
        }

    case NECRO_LEX_STRING_LITERAL:
        {
            NecroParseAstConstant constant;
            constant.type           = NECRO_AST_CONSTANT_STRING;
            constant.symbol         = necro_parse_peek_token(parser)->symbol;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, constant);
            necro_parse_consume_token(parser);
            return local_ptr;
        }
    case NECRO_LEX_CHAR_LITERAL:
        {
            NecroParseAstConstant constant;
            constant.type           = (parser->descent_state == NECRO_PARSING_PATTERN) ? NECRO_AST_CONSTANT_CHAR_PATTERN : NECRO_AST_CONSTANT_CHAR;
            constant.char_literal   = necro_parse_peek_token(parser)->char_literal;
            local_ptr               = necro_parse_ast_create_constant(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, constant);
            necro_parse_consume_token(parser);
            return local_ptr;
        }
    default:
        break;
    }
    necro_parse_restore(parser, snapshot);
    return null_local_ptr;
}

NecroResult(NecroParseAstLocalPtr) necro_parse_parenthetical_expression(NecroParser* parser)
{
    // '('
    NecroSourceLoc      source_loc       = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot snapshot         = necro_parse_snapshot(parser);
    NecroLexToken*      look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LEFT_PAREN)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    necro_parse_consume_token(parser);

    // Expression
    look_ahead_token                = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (local_ptr == null_local_ptr)
    {
        return necro_paren_expression_failed_to_parse_error(source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // ')
    look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        if (look_ahead_token->token == NECRO_LEX_COMMA)
        {
            // if a comma is next then this may be a tuple, keep trying to parse
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
        return necro_paren_expression_missing_paren_error(source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // Finish
    necro_parse_consume_token(parser);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

// NecroAST_LocalPtr parse_unary_operation(NecroParser* parser)
// {
//     if (peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
//         return null_local_ptr;
//     NecroParser_Snapshot snapshot = snapshot_parser(parser);
//     restore_parser(parser, snapshot);
//     return null_local_ptr;
// }

NECRO_BIN_OP_TYPE necro_token_to_bin_op_type(NECRO_LEX_TOKEN_TYPE token_type)
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

NecroResult(NecroParseAstLocalPtr) necro_parse_binary_expression(NecroParser* parser, NecroParseAstLocalPtr lhs_local_ptr, int min_precedence)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || lhs_local_ptr == null_local_ptr)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc      source_loc    = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot snapshot      = necro_parse_snapshot(parser);
    NecroLexToken*      current_token = necro_parse_peek_token(parser);
    NecroSymbol         bin_op_symbol = current_token->symbol;
    NECRO_BIN_OP_TYPE   bin_op_type   = necro_token_to_bin_op_type(current_token->token);

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED || bin_op_behaviors[bin_op_type].precedence < min_precedence)
    {
        return ok_NecroParseAstLocalPtr(lhs_local_ptr);
    }

    NecroParseAstLocalPtr bin_op_local_ptr = null_local_ptr;
    NecroParseAstLocalPtr rhs_local_ptr    = null_local_ptr;
    while (true)
    {
        current_token = necro_parse_peek_token(parser);
        bin_op_symbol = current_token->symbol;
        bin_op_type   = necro_token_to_bin_op_type(current_token->token);

        if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
        {
            break;
        }

        NecroParseBinOpBehavior bin_op_behavior = bin_op_behaviors[bin_op_type];
        if (bin_op_behavior.precedence < min_precedence)
        {
            break;
        }

        if (bin_op_local_ptr != null_local_ptr)
        {
            lhs_local_ptr = bin_op_local_ptr;
        }
        necro_parse_consume_token(parser); // consume current_token

        int next_min_precedence;
        if (bin_op_behavior.associativity == NECRO_BIN_OP_ASSOC_LEFT)
            next_min_precedence = bin_op_behavior.precedence + 1;
        else
            next_min_precedence = bin_op_behavior.precedence;

        rhs_local_ptr                                       = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression_go(parser, NECRO_NO_BINARY_LOOK_AHEAD));
        NecroLexToken*           look_ahead_token           = necro_parse_peek_token(parser);
        NECRO_BIN_OP_TYPE       look_ahead_bin_op_type     = necro_token_to_bin_op_type(look_ahead_token->token);
        NecroParseBinOpBehavior look_ahead_bin_op_behavior = bin_op_behaviors[look_ahead_bin_op_type];

        while (look_ahead_bin_op_type != NECRO_BIN_OP_UNDEFINED &&
               look_ahead_bin_op_behavior.precedence >= next_min_precedence)
        {

            rhs_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_binary_expression(parser, rhs_local_ptr, next_min_precedence));

            if (look_ahead_token != necro_parse_peek_token(parser))
            {
                look_ahead_token           = necro_parse_peek_token(parser);
                look_ahead_bin_op_type     = necro_token_to_bin_op_type(look_ahead_token->token);
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

        bin_op_local_ptr = necro_parse_ast_create_bin_op(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, lhs_local_ptr, rhs_local_ptr, bin_op_type, bin_op_symbol);
    }

    if (bin_op_local_ptr == null_local_ptr ||
        lhs_local_ptr    == null_local_ptr ||
        rhs_local_ptr    == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Finish
    NecroParseAst* bin_op_ast_node = necro_parse_ast_get_node(&parser->ast, bin_op_local_ptr);
    bin_op_ast_node->bin_op.lhs    = lhs_local_ptr;
    bin_op_ast_node->bin_op.rhs    = rhs_local_ptr;
    return necro_parse_binary_operation(parser, bin_op_local_ptr); // Try to keep evaluating lower precedence tokens
}

NecroResult(NecroParseAstLocalPtr) necro_parse_binary_operation(NecroParser* parser, NecroParseAstLocalPtr lhs_local_ptr)
{
    NecroLexToken*     current_token = necro_parse_peek_token(parser);
    NECRO_BIN_OP_TYPE bin_op_type   = necro_token_to_bin_op_type(current_token->token);
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        return ok_NecroParseAstLocalPtr(lhs_local_ptr);
    }
    return necro_parse_binary_expression(parser, lhs_local_ptr, bin_op_behaviors[bin_op_type].precedence);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_op_left_section(NecroParser* parser)
{
    // '('
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
    {
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    NecroParserSnapshot  snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc       source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Left expression
    NecroParseAstLocalPtr left = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression_go(parser, NECRO_NO_BINARY_LOOK_AHEAD));
    if (left == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Op
    const NecroLexToken*     bin_op_token  = necro_parse_peek_token(parser);
    const NecroSymbol        bin_op_symbol = bin_op_token->symbol;
    const NECRO_BIN_OP_TYPE bin_op_type   = necro_token_to_bin_op_type(bin_op_token->token);

    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // ')'
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_op_left_section(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, left, bin_op_type, bin_op_symbol);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_op_right_section(NecroParser* parser)
{
    // '('
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
    {
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    NecroParserSnapshot  snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc       source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Op
    const NecroLexToken*     bin_op_token  = necro_parse_peek_token(parser);
    const NecroSymbol        bin_op_symbol = bin_op_token->symbol;
    const NECRO_BIN_OP_TYPE bin_op_type   = necro_token_to_bin_op_type(bin_op_token->token);
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Right expression
    const NecroParseAstLocalPtr right = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression_go(parser, NECRO_NO_BINARY_LOOK_AHEAD));
    if (right == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ')'
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_op_right_section(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, right, bin_op_type, bin_op_symbol);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_l_expression(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot   snapshot  = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr local_ptr = null_local_ptr;

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pattern_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_do(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, parse_let_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_if_then_else_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_function_expression(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_lambda(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_case(parser));
    }

    if (local_ptr == null_local_ptr)
    {
        local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_for_loop(parser));
    }

    if (local_ptr != null_local_ptr)
    {
        return ok_NecroParseAstLocalPtr(local_ptr);
    }

    necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_if_then_else_expression(NecroParser* parser)
{
    // if
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_IF)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc       if_source_loc          = necro_parse_peek_token(parser)->source_loc;
    // NecroParser_Snapshot snapshot               = snapshot_parser(parser);
    necro_parse_consume_token(parser);

    // if expression
    NecroLexToken*        look_ahead_token = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr if_local_ptr     = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (if_local_ptr == null_local_ptr)
    {
        return necro_if_failed_to_parse_error(if_source_loc, look_ahead_token->end_loc);
    }

    // then
    look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_THEN)
    {
        return necro_if_missing_then_error(if_source_loc, look_ahead_token->end_loc);
    }
    necro_parse_consume_token(parser);

    // then expression
    look_ahead_token                     = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr then_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (then_local_ptr == null_local_ptr)
    {
        return necro_if_missing_expr_after_then_error(if_source_loc, look_ahead_token->end_loc);
    }

    // else
    look_ahead_token = necro_parse_peek_token(parser);
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_ELSE)
    {
        return necro_if_missing_else_error(if_source_loc, look_ahead_token->end_loc);
    }
    necro_parse_consume_token(parser);

    // else expression
    look_ahead_token                     = necro_parse_peek_token(parser);
    NecroParseAstLocalPtr else_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (else_local_ptr == null_local_ptr)
    {
        return necro_if_missing_expr_after_else_error(if_source_loc, look_ahead_token->end_loc);
    }

    // Finish
    NecroParseAstLocalPtr if_then_else_local_ptr = necro_parse_ast_create_if_then_else(&parser->ast.arena, if_source_loc, necro_parse_peek_token(parser)->end_loc, if_local_ptr, then_local_ptr, else_local_ptr);
    return ok_NecroParseAstLocalPtr(if_then_else_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_lambda(NecroParser* parser)
{
    // '\'
    NecroSourceLoc             back_loc        = necro_parse_peek_token(parser)->source_loc;
    // NecroParser_Snapshot       snapshot        = snapshot_parser(parser);
    const NecroLexToken*       backslash_token = necro_parse_peek_token(parser);
    const NECRO_LEX_TOKEN_TYPE token_type      = backslash_token->token;
    if (token_type != NECRO_LEX_BACK_SLASH)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    necro_parse_consume_token(parser);

    // Apats
    NecroParseAstLocalPtr apats_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats(parser));
    if (apats_local_ptr == null_local_ptr)
    {
        return necro_lambda_failed_to_parse_pattern_error(back_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // Arrow
    const NECRO_LEX_TOKEN_TYPE next_token = necro_parse_peek_token_type(parser);
    if (next_token != NECRO_LEX_RIGHT_ARROW)
    {
        return necro_lambda_missing_arrow_error(back_loc, necro_parse_peek_token(parser)->end_loc);
    }
    necro_parse_consume_token(parser);

    // Expression
    NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        return necro_lambda_failed_to_parse_error(back_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // Finish
    NecroParseAstLocalPtr lambda_local_ptr = necro_parse_ast_create_lambda(&parser->ast.arena, back_loc, necro_parse_peek_token(parser)->end_loc, apats_local_ptr, expression_local_ptr);
    return ok_NecroParseAstLocalPtr(lambda_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_list(NecroParser* parser, NECRO_LEX_TOKEN_TYPE token_divider, NecroParseFunc parse_func)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // Item
    NecroSourceLoc        source_loc     = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot       = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr item_local_ptr = necro_try_result(NecroParseAstLocalPtr, parse_func(parser));
    if (item_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Divider and Next
    NecroParseAstLocalPtr next_item_local_ptr = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == token_divider)
    {
        necro_parse_consume_token(parser); // consume divider token
        next_item_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, token_divider, parse_func));
    }

    // Finish
    NecroParseAstLocalPtr list_local_ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, item_local_ptr, next_item_local_ptr);
    return ok_NecroParseAstLocalPtr(list_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) parse_do_item(NecroParser* parser)
{
    NecroParserSnapshot  snapshot       = necro_parse_snapshot(parser);
    NECRO_LEX_TOKEN_TYPE top_token_type = necro_parse_peek_token_type(parser);
    switch (top_token_type)
    {
    case NECRO_LEX_IDENTIFIER:
    {
        NecroSourceLoc       source_loc     = necro_parse_peek_token(parser)->source_loc;
        const NecroLexToken* variable_token = necro_parse_peek_token(parser);
        necro_parse_consume_token(parser); // Consume variable token
        if (variable_token->token == NECRO_LEX_IDENTIFIER && necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_ARROW)
        {
            necro_parse_consume_token(parser); // consume left arrow
            NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
            if (expression_local_ptr != null_local_ptr)
            {
                NecroParseAstLocalPtr bind_assignment_local_ptr = necro_parse_ast_create_bind_assignment(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, variable_token->symbol, expression_local_ptr);
                return ok_NecroParseAstLocalPtr(bind_assignment_local_ptr);
            }
            else
            {
                return necro_do_bind_failed_to_parse_error(variable_token->source_loc, necro_parse_peek_token(parser)->end_loc);
            }
        }
        necro_parse_restore(parser, snapshot);
    } break;

    case NECRO_LEX_LET:
    {
        necro_parse_consume_token(parser); // Consume 'LET'
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_BRACE)
            return necro_do_let_expected_left_brace_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
        necro_parse_consume_token(parser); // Consume '{'
        NecroParseAstLocalPtr declarations = necro_try_result(NecroParseAstLocalPtr, necro_parse_declarations_list(parser));
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
            return necro_do_let_expected_right_brace_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
        necro_parse_consume_token(parser); // Consume '}'
        return ok_NecroParseAstLocalPtr(declarations);
    } break;

    default:
        break;

    } // switch (top_token_type)

    // Pattern bind
    {
        NecroSourceLoc        pat_loc       = necro_parse_peek_token(parser)->source_loc;
        NecroParseAstLocalPtr pat_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat(parser));
        if (pat_local_ptr != null_local_ptr)
        {
            if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_ARROW)
            {
                necro_parse_consume_token(parser); // consume '<-'
                NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
                if (expression_local_ptr != null_local_ptr)
                {
                    NecroParseAstLocalPtr bind_assignment_local_ptr = necro_parse_ast_create_pat_bind_assignment(&parser->ast.arena, pat_loc, necro_parse_peek_token(parser)->end_loc, pat_local_ptr, expression_local_ptr);
                    return ok_NecroParseAstLocalPtr(bind_assignment_local_ptr);
                }
                else
                {
                    return necro_do_bind_failed_to_parse_error(pat_loc, necro_parse_peek_token(parser)->end_loc);
                }
            }
        }
        necro_parse_restore(parser, snapshot);
    }

    // Expression
    {
        NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
        if (expression_local_ptr != null_local_ptr)
        {
            return ok_NecroParseAstLocalPtr(expression_local_ptr);
        }
    }

    necro_parse_restore(parser, snapshot);
    return necro_parse_expression(parser);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_do(NecroParser* parser)
{
    // do
    NecroParserSnapshot        snapshot      = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE do_token_type = necro_parse_peek_token_type(parser);
    if (do_token_type != NECRO_LEX_DO)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // {
    const NECRO_LEX_TOKEN_TYPE l_curly_token_type = necro_parse_peek_token_type(parser);
    if (l_curly_token_type != NECRO_LEX_LEFT_BRACE)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Statements
    NecroParseAstLocalPtr statement_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_SEMI_COLON, parse_do_item));
    if (statement_list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    //;
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token == NECRO_LEX_SEMI_COLON)
    {
        necro_parse_consume_token(parser);
    }

    // }
    look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
    {
        return necro_do_missing_right_brace_error(look_ahead_token->source_loc, look_ahead_token->end_loc);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr do_local_ptr = necro_parse_ast_create_do(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroParseAstLocalPtr(do_local_ptr);
}

// TODO: Retrofit to array patterns
NecroResult(NecroParseAstLocalPtr) necro_parse_expression_list(NecroParser* parser)
{
    // NOTE: Removing Expression lists [expr] for now in favor of pattern syntax.
    if (true)
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    // [
    NecroSourceLoc             source_loc       = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot        snapshot         = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE brace_token_type = necro_parse_peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    necro_parse_consume_token(parser);

    // Expressions
    NecroParseAstLocalPtr statement_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_expression));

    // ]
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACKET)
    {
        if (look_ahead_token->token == NECRO_LEX_DOUBLE_DOT)
        {
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
        else
        {
            return necro_list_missing_right_bracket_error(source_loc, look_ahead_token->end_loc);
        }
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr list_local_ptr = necro_parse_ast_create_expression_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroParseAstLocalPtr(list_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_expression_array(NecroParser* parser)
{
    // {
    NecroParserSnapshot        snapshot         = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE brace_token_type = necro_parse_peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Expressions
    NecroParseAstLocalPtr statement_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_expression));

    // }
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
    {
        if (look_ahead_token->token == NECRO_LEX_DOUBLE_DOT)
        {
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
        else
        {
            return necro_array_missing_right_brace_error(source_loc, look_ahead_token->end_loc);
        }
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr list_local_ptr = necro_parse_ast_create_expression_array(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroParseAstLocalPtr(list_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_pat_expression(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // aexpr
    NecroParseAstLocalPtr local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_application_expression(parser));
    if (local_ptr != null_local_ptr)
        return ok_NecroParseAstLocalPtr(local_ptr);
    necro_parse_restore(parser, snapshot);

    // Wildcard
    local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_wildcard(parser));
    if (local_ptr != null_local_ptr)
        return ok_NecroParseAstLocalPtr(local_ptr);
    necro_parse_restore(parser, snapshot);

    return ok_NecroParseAstLocalPtr(null_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_expressions_for_pattern_expression(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // Pat expression
    NecroSourceLoc        source_loc           = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot             = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat_expression(parser));
    if (expression_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Next Pat expression
    NecroParseAstLocalPtr next_expression_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expressions_for_pattern_expression(parser));

    // Finish
    NecroParseAstLocalPtr expressions_local_ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, expression_local_ptr, next_expression_local_ptr);
    return ok_NecroParseAstLocalPtr(expressions_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_pattern_expression(NecroParser* parser)
{
    // [
    if (necro_parse_peek_token(parser)->token != NECRO_LEX_LEFT_BRACKET)
        return ok(NecroParseAstLocalPtr, null_local_ptr);

    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Expressions
    NecroParseAstLocalPtr expression_list = necro_try_result(NecroParseAstLocalPtr, necro_parse_expressions_for_pattern_expression(parser));
    if (expression_list == null_local_ptr)
    {
        return necro_pattern_empty_expression_list_error(source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // ]
    if (necro_parse_peek_token(parser)->token != NECRO_LEX_RIGHT_BRACKET)
    {
        necro_parse_restore(parser, snapshot);
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr pat_expression_local_ptr = necro_parse_ast_create_pat_expr(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, expression_list);
    return ok_NecroParseAstLocalPtr(pat_expression_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_arithmetic_sequence(NecroParser* parser)
{
    // [
    NecroParserSnapshot        snapshot         = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE brace_token_type = necro_parse_peek_token_type(parser);
    if (brace_token_type != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    NecroParseAstLocalPtr from = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    NecroParseAstLocalPtr then = null_local_ptr;
    NecroParseAstLocalPtr to   = null_local_ptr;

    // from
    if (from == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ,
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_COMMA)
    {
        necro_parse_consume_token(parser); // consume ','
        then = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
        if (then == null_local_ptr)
            return necro_arithmetic_sequence_failed_to_parse_then_error(source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    // ..
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_DOUBLE_DOT)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // to
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        to = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
        if (to == null_local_ptr)
            return necro_arithmetic_sequence_failed_to_parse_to_error(source_loc, necro_parse_peek_token(parser)->end_loc);
    }

    if (then != null_local_ptr && to == null_local_ptr)
    {
        // this means we're probably just a normal list expression
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ]
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        return necro_arithmetic_sequence_missing_right_bracket_error(source_loc, necro_parse_peek_token(parser)->end_loc);
    }
    necro_parse_consume_token(parser);

    // Finish
    NECRO_ARITHMETIC_SEQUENCE_TYPE seq_type = NECRO_ARITHMETIC_ENUM_FROM_THEN_TO;
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
    NecroParseAstLocalPtr arithmetic_local_ptr = necro_parse_ast_create_arithmetic_sequence(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, from, then, to, seq_type);
    return ok_NecroParseAstLocalPtr(arithmetic_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_tuple(NecroParser* parser)
{
    // (
    NecroParserSnapshot        snapshot   = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type != NECRO_LEX_LEFT_PAREN)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Expressions
    NecroParseAstLocalPtr statement_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_expression));
    if (statement_list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // )
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_PAREN)
    {
        return necro_tuple_missing_paren_error(source_loc, look_ahead_token->end_loc);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr list_local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, statement_list_local_ptr);
    return ok_NecroParseAstLocalPtr(list_local_ptr);
}

//=====================================================
// Case / Patterns
//=====================================================
NecroResult(NecroParseAstLocalPtr) necro_parse_case_alternative(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_BRACE || necro_parse_peek_token_type(parser) == NECRO_LEX_WHERE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;

    // pat
    NecroParseAstLocalPtr pat_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat(parser));
    if (pat_local_ptr == null_local_ptr)
        return necro_case_alternative_expected_pattern_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // ->
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_ARROW)
        return necro_case_alternative_expected_arrow_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    // body expr
    NecroParseAstLocalPtr body_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (body_local_ptr == null_local_ptr)
        return necro_case_alternative_expected_expression_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // finish
    NecroParseAstLocalPtr case_alternative_local_ptr = necro_parse_ast_create_case_alternative(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, pat_local_ptr, body_local_ptr);
    return ok_NecroParseAstLocalPtr(case_alternative_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_case(NecroParser* parser)
{
    // case
    NecroParserSnapshot        snapshot   = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type != NECRO_LEX_CASE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // expr
    NecroParseAstLocalPtr case_expr_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (case_expr_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // 'of'
    NecroLexToken* look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_OF)
        return necro_case_alternative_expected_of_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    // '{'
    look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_LEFT_BRACE)
        return necro_case_alternative_expected_left_brace_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    // alternatives
    NecroParseAstLocalPtr alternative_list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_SEMI_COLON, necro_parse_case_alternative));
    if (alternative_list_local_ptr == null_local_ptr)
        return necro_case_alternative_empty_error(source_loc, necro_parse_peek_token(parser)->end_loc);

    // '}'
    look_ahead_token = necro_parse_peek_token(parser);
    if (look_ahead_token->token != NECRO_LEX_RIGHT_BRACE)
        return necro_case_alternative_expected_right_brace_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr case_local_ptr = necro_parse_ast_create_case(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, case_expr_local_ptr, alternative_list_local_ptr);
    return ok_NecroParseAstLocalPtr(case_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_for_loop(NecroParser* parser)
{
    // for
    // NecroParserSnapshot        snapshot   = necro_parse_snapshot(parser);
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type != NECRO_LEX_FOR)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // TODO: This doesn't seem like a normal expression parse...
    // range_init
    NecroParseAstLocalPtr range_init = necro_try_result(NecroParseAstLocalPtr, necro_parse_application_expression(parser));
    if (range_init == null_local_ptr)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // value_init
    NecroParseAstLocalPtr value_init = necro_try_result(NecroParseAstLocalPtr, necro_parse_application_expression(parser));
    if (value_init == null_local_ptr)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // loop
    if (necro_parse_peek_token(parser)->token != NECRO_LEX_LOOP)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    NECRO_PARSE_STATE  prev_state = necro_parse_enter_state(parser);

    // index_apat
    NecroParseAstLocalPtr index_apat = necro_try_result(NecroParseAstLocalPtr, necro_parse_apat(parser));
    if (index_apat == null_local_ptr)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // value_apat
    NecroParseAstLocalPtr value_apat = necro_try_result(NecroParseAstLocalPtr, necro_parse_apat(parser));
    if (value_apat == null_local_ptr)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    necro_parse_restore_state(parser, prev_state);

    // ->
    if (necro_parse_peek_token(parser)->token != NECRO_LEX_RIGHT_ARROW)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    // expression
    NecroParseAstLocalPtr expression = necro_try_result(NecroParseAstLocalPtr, necro_parse_expression(parser));
    if (expression == null_local_ptr)
        return necro_malformed_for_loop_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // Finish
    NecroParseAstLocalPtr for_local_ptr = necro_parse_ast_create_for_loop(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, range_init, value_init, index_apat, value_apat, expression);
    return ok(NecroParseAstLocalPtr, for_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_qconop(NecroParser* parser, NECRO_CON_TYPE con_type);
NecroResult(NecroParseAstLocalPtr) necro_parse_lpat(NecroParser* parser);

// TODO: This needs to work like BinOps and is currently just wrong.
NecroResult(NecroParseAstLocalPtr) necro_parse_oppat(NecroParser* parser)
{
    NECRO_PARSE_STATE   prev_state = necro_parse_enter_state(parser);
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;

    // Left pat
    NecroParseAstLocalPtr left = necro_try_result(NecroParseAstLocalPtr, necro_parse_lpat(parser));
    if (left == null_local_ptr)
    {
        necro_parse_restore_state(parser, prev_state);
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Op
    NecroParseAstLocalPtr op = necro_try_result(NecroParseAstLocalPtr, necro_parse_qconop(parser, NECRO_CON_VAR));
    if (op == null_local_ptr)
    {
        necro_parse_restore_state(parser, prev_state);
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Right pat
    NecroParseAstLocalPtr right = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat(parser));
    if (right == null_local_ptr)
    {
        necro_parse_restore_state(parser, prev_state);
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_bin_op_sym(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, left, op, right);
    necro_parse_restore_state(parser, prev_state);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_pat(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NECRO_PARSE_STATE     prev_state = necro_parse_enter_state(parser);
    NecroParserSnapshot   snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr ptr        = necro_try_result(NecroParseAstLocalPtr, necro_parse_oppat(parser));
    if (ptr == null_local_ptr)
    {
        ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_lpat(parser));
    }

    if (ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
    }

    necro_parse_restore_state(parser, prev_state);

    return ok_NecroParseAstLocalPtr(ptr);
}

NECRO_PARSE_STATE necro_parse_enter_state(NecroParser* parser)
{
    NECRO_PARSE_STATE prev_state = parser->descent_state;
    parser->descent_state              = NECRO_PARSING_PATTERN;
    return prev_state;
}

void necro_parse_restore_state(NecroParser* parser, NECRO_PARSE_STATE prev_state)
{
    parser->descent_state = prev_state;
}

// Pat
NecroResult(NecroParseAstLocalPtr) necro_parse_lpat(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc        source_loc = necro_parse_peek_token(parser)->source_loc;
    NECRO_PARSE_STATE     prev_state = necro_parse_enter_state(parser);
    NecroParserSnapshot   snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr ptr        = null_local_ptr;

    // gcon apat1 ... apatk
    {
        ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_gcon(parser, NECRO_CON_VAR));
        if (ptr != null_local_ptr)
        {
            NecroParseAstLocalPtr apats_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats(parser));
            if (apats_local_ptr != null_local_ptr)
            {
                // NecroAST_LocalPtr constr_ptr      = null_local_ptr;
                // NecroAST_Node*    constr_node     = ast_alloc_node_local_ptr(parser, &constr_ptr);
                // constr_node->type                 = NECRO_AST_CONSTRUCTOR;
                // constr_node->constructor.conid    = ptr;
                // constr_node->constructor.arg_list = apats_local_ptr;
                // ptr                               = constr_ptr;
                ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, ptr, apats_local_ptr);
            }
        }
    }

    // apat
    if (ptr == null_local_ptr)
    {
        ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apat(parser));
    }

    // Literal
    if (ptr == null_local_ptr)
    {
        // Literal
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL || necro_parse_peek_token_type(parser) == NECRO_LEX_FLOAT_LITERAL)
        {
            ptr = necro_parse_constant(parser);
        }
    }

    if (ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
    }

    necro_parse_restore_state(parser, prev_state);
    return ok_NecroParseAstLocalPtr(ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_gcon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroParserSnapshot   snapshot      = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr con_local_ptr = null_local_ptr;

    // ()
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol symbol = necro_parse_peek_token(parser)->symbol;
        con_local_ptr      = necro_parse_ast_create_conid(&parser->ast.arena, necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc, symbol, con_type);
        necro_parse_consume_token(parser);
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
        con_local_ptr = necro_parse_qcon(parser, con_type);
    }
    if (con_local_ptr == null_local_ptr)
        necro_parse_restore(parser, snapshot);
    return ok_NecroParseAstLocalPtr(con_local_ptr);
}

NecroParseAstLocalPtr necro_parse_consym(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NECRO_BIN_OP_TYPE bin_op_type = necro_token_to_bin_op_type(necro_parse_peek_token_type(parser));
    if (bin_op_type == NECRO_BIN_OP_UNDEFINED)
        return null_local_ptr;
    NecroSourceLoc        source_loc    = necro_parse_peek_token(parser)->source_loc;
    NecroParseAstLocalPtr con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, necro_parse_peek_token(parser)->symbol, con_type);
    necro_parse_consume_token(parser);
    return con_local_ptr;
}

NecroParseAstLocalPtr necro_parse_qconsym(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    // Save until we actuall have modules...
 	// [ modid . ] consym
    return necro_parse_consym(parser, con_type);
}

// conid | (consym)
NecroParseAstLocalPtr necro_parse_con(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroParserSnapshot   snapshot         = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr con_local_ptr    = null_local_ptr;
    NecroLexToken*        look_ahead_token = necro_parse_peek_token(parser);

    if (look_ahead_token->token == NECRO_LEX_TYPE_IDENTIFIER)
    {
        con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc, look_ahead_token->symbol, con_type);
        necro_parse_consume_token(parser);
        return con_local_ptr;
    }
    else if (look_ahead_token->token == NECRO_LEX_LEFT_PAREN)
    {
        necro_parse_consume_token(parser);
        con_local_ptr = necro_parse_consym(parser, con_type);
        if (con_local_ptr != null_local_ptr)
        {
            if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            {
                necro_parse_restore(parser, snapshot);
                return null_local_ptr;
            }
            necro_parse_consume_token(parser);
            return con_local_ptr;
        }
    }

    necro_parse_restore(parser, snapshot);
    return null_local_ptr;
}

NecroParseAstLocalPtr necro_parse_qcon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    // Save until we actuall have modules...
 	// [ modid . ] conid
    return necro_parse_con(parser, con_type);
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
NecroResult(NecroParseAstLocalPtr) necro_parse_qconop(NecroParser* parser, NECRO_CON_TYPE con_type)
{

    NecroParserSnapshot   snapshot      = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr con_local_ptr = null_local_ptr;
    NecroSourceLoc        source_loc    = necro_parse_peek_token(parser)->source_loc;

    con_local_ptr = necro_parse_consym(parser, con_type);
    if (con_local_ptr != null_local_ptr)
        return ok_NecroParseAstLocalPtr(con_local_ptr);

    if (necro_parse_peek_token_type(parser) != NECRO_LEX_ACCENT)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    necro_parse_consume_token(parser);
    con_local_ptr = necro_parse_qcon(parser, con_type);

    if (con_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_ACCENT)
        return necro_fn_op_expected_accent_error(source_loc, necro_parse_peek_token(parser)->end_loc);

    necro_parse_consume_token(parser);
    return ok_NecroParseAstLocalPtr(con_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_list_pat(NecroParser* parser)
{
    if (true)
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // [
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Pats
    NecroParseAstLocalPtr list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_pat));
    if (list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ]
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_expression_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, list_local_ptr);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_tuple_pattern(NecroParser* parser)
{
    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // (
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // Pats
    NecroParseAstLocalPtr list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_pat));
    if (list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // )
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, list_local_ptr);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

//=====================================================
// Types
//=====================================================
NecroParseAstLocalPtr              necro_parse_tycon(NecroParser* parser, NECRO_CON_TYPE con_type);
NecroParseAstLocalPtr              necro_parse_tyvar(NecroParser* parser, NECRO_VAR_TYPE var_type);
NecroResult(NecroParseAstLocalPtr) necro_parse_simpletype(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_btype(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_tuple_type(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_list_type(NecroParser* parser);
NecroResult(NecroParseAstLocalPtr) necro_parse_atype(NecroParser* parser, NECRO_VAR_TYPE tyvar_var_type);
NecroResult(NecroParseAstLocalPtr) necro_parse_constr(NecroParser* parser);

NecroResult(NecroParseAstLocalPtr) necro_parse_top_level_data_declaration(NecroParser* parser)
{
    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // data
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_DATA)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // simpletype
    NecroParseAstLocalPtr simpletype = necro_try_result(NecroParseAstLocalPtr, necro_parse_simpletype(parser));
    if (simpletype == null_local_ptr)
        return necro_data_expected_type_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // =
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_ASSIGN)
        return necro_data_expected_assign_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
    necro_parse_consume_token(parser);

    // constructor_list
    NecroParseAstLocalPtr constructor_list = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_PIPE, necro_parse_constr));
    if (constructor_list == null_local_ptr)
        return necro_data_expected_data_con_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_data_declaration(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, simpletype, constructor_list);
    return ok_NecroParseAstLocalPtr(ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_atype_list(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // tyvar
    NecroSourceLoc        source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParseAstLocalPtr atype      = necro_try_result(NecroParseAstLocalPtr, necro_parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    if (atype == null_local_ptr)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // next
    NecroParseAstLocalPtr next = necro_try_result(NecroParseAstLocalPtr, necro_parse_atype_list(parser));

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, atype, next);
    return ok_NecroParseAstLocalPtr(ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_constr(NecroParser* parser)
{
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;

    // tycon
    NecroParseAstLocalPtr tycon = necro_parse_tycon(parser, NECRO_CON_DATA_DECLARATION);
    if (tycon == null_local_ptr)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    // atype list
    NecroParseAstLocalPtr atype_list = necro_try_result(NecroParseAstLocalPtr, necro_parse_atype_list(parser));

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, tycon, atype_list);
    return ok_NecroParseAstLocalPtr(ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_tyvar_list(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;

    NecroParseAstLocalPtr tyvar = null_local_ptr;

    // (
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    {
        necro_parse_consume_token(parser);
        // var :: Sig
        tyvar = necro_try_result(NecroParseAstLocalPtr, necro_parse_type_signature(parser, NECRO_SIG_TYPE_VAR));
        if (tyvar == null_local_ptr)
        {
            necro_parse_restore(parser, snapshot);
            return ok(NecroParseAstLocalPtr, null_local_ptr);
        }
        // )
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
        {
            return necro_type_sig_expected_right_paren_error(source_loc, necro_parse_peek_token(parser)->end_loc);
        }
        necro_parse_consume_token(parser);
    }
    else
    {
        // tyvar
        tyvar = necro_parse_tyvar(parser, NECRO_VAR_TYPE_VAR_DECLARATION);
        if (tyvar == null_local_ptr)
            return ok(NecroParseAstLocalPtr, null_local_ptr);
    }

    // next
    NecroParseAstLocalPtr next = necro_try_result(NecroParseAstLocalPtr, necro_parse_tyvar_list(parser));

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_list(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, tyvar, next);
    return ok(NecroParseAstLocalPtr, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_simpletype(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;

    // tycon
    NecroParseAstLocalPtr tycon = necro_parse_tycon(parser, NECRO_CON_TYPE_DECLARATION);
    if (tycon == null_local_ptr)
        return ok(NecroParseAstLocalPtr, null_local_ptr);

    // tyvar list
    NecroParseAstLocalPtr tyvars = necro_try_result(NecroParseAstLocalPtr, necro_parse_tyvar_list(parser));

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_simple_type(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, tycon, tyvars);
    return ok(NecroParseAstLocalPtr, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_type(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc        source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr ty_ptr     = necro_try_result(NecroParseAstLocalPtr, necro_parse_btype(parser));
    if (ty_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    // Next Type after arrow
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_ARROW)
    {
        necro_parse_consume_token(parser);
        NecroParseAstLocalPtr next_after_arrow_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_type(parser));
        if (next_after_arrow_ptr == null_local_ptr)
            return necro_type_expected_type_error(source_loc, necro_parse_peek_token(parser)->end_loc);
                NecroParseAstLocalPtr ptr = necro_parse_ast_create_function_type(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, ty_ptr, next_after_arrow_ptr);
        return ok_NecroParseAstLocalPtr(ptr);
    }
    else
    {
        return ok_NecroParseAstLocalPtr(ty_ptr);
    }
}

NecroResult(NecroParseAstLocalPtr) necro_parse_btype_go(NecroParser* parser, NecroParseAstLocalPtr left)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || left == null_local_ptr)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc        source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr atype      = necro_try_result(NecroParseAstLocalPtr, necro_parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    if (atype == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(left);
    }

    NecroParseAstLocalPtr ptr = necro_parse_ast_create_type_app(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, left, atype);
    return necro_parse_btype_go(parser, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_btype(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroParserSnapshot  snapshot = necro_parse_snapshot(parser);

    // *type
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_MUL)
    {
        NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
        necro_parse_consume_token(parser);
        NecroParseAstLocalPtr ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_btype(parser)); // TODO/NOTE: Or perhaps necro_parse_type
        if (ptr != null_local_ptr)
        {
            ptr = necro_parse_ast_create_type_attribute(&parser->ast.arena, source_loc, source_loc, ptr, NECRO_TYPE_ATTRIBUTE_STAR);
            return ok(NecroParseAstLocalPtr, ptr);
        }
        necro_parse_restore(parser, snapshot);
    }

    // .type
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_DOT)
    {
        NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
        necro_parse_consume_token(parser);
        NecroParseAstLocalPtr ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_btype(parser)); // TODO/NOTE: Or perhaps necro_parse_type
        if (ptr != null_local_ptr)
        {
            ptr = necro_parse_ast_create_type_attribute(&parser->ast.arena, source_loc, source_loc, ptr, NECRO_TYPE_ATTRIBUTE_DOT);
            return ok(NecroParseAstLocalPtr, ptr);
        }
        necro_parse_restore(parser, snapshot);
    }

    NecroParseAstLocalPtr atype    = necro_try_result(NecroParseAstLocalPtr, necro_parse_atype(parser, NECRO_VAR_TYPE_FREE_VAR));
    NecroParseAstLocalPtr btype    = necro_try_result(NecroParseAstLocalPtr, necro_parse_btype_go(parser, atype));
    if (btype == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    return ok_NecroParseAstLocalPtr(btype);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_atype(NecroParser* parser, NECRO_VAR_TYPE tyvar_var_type)
{
    NecroParserSnapshot   snapshot = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr ptr      = null_local_ptr;

    // tycon
    ptr = necro_parse_tycon(parser, NECRO_CON_TYPE_VAR);

    // tyvar
    if (ptr == null_local_ptr)
    {
        // ^tyvar
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_CARET)
        {
            necro_parse_consume_token(parser); // consume ^
            ptr = necro_parse_tyvar(parser, tyvar_var_type);
            if (ptr != null_local_ptr)
            {
                NecroParseAst* ast_node  = necro_parse_ast_get_node(&parser->ast, ptr);
                ast_node->variable.order = NECRO_TYPE_HIGHER_ORDER;
            }
            else
            {
                necro_parse_restore(parser, snapshot);
            }
        }
        else
        {
            ptr = necro_parse_tyvar(parser, tyvar_var_type);
        }
    }

    // Int literal
    if (ptr == null_local_ptr && necro_parse_peek_token_type(parser) == NECRO_LEX_INTEGER_LITERAL)
    {
        ptr = necro_parse_constant(parser);
        if (ptr != null_local_ptr)
        {
            NecroParseAst* ptr_ast = necro_parse_ast_get_node(&parser->ast, ptr);
            ptr_ast->constant.type = NECRO_AST_CONSTANT_TYPE_INT;
        }
    }

    // tuple type
    if (ptr == null_local_ptr)
    {
        ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_tuple_type(parser));
    }

    // [type]
    if (ptr == null_local_ptr)
    {
        ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list_type(parser));
    }

    // (type)
    if (ptr == null_local_ptr)
    {
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
        {
            necro_parse_consume_token(parser);
            ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_type(parser));
            if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
            {
                necro_parse_restore(parser, snapshot);
                ptr = null_local_ptr;
            }
            necro_parse_consume_token(parser);
        }
    }

    return ok_NecroParseAstLocalPtr(ptr);
}

NecroParseAstLocalPtr necro_parse_gtycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroSourceLoc        source_loc         = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot   snapshot           = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr type_con_local_ptr = null_local_ptr;

    // ()
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol    symbol  = necro_parse_peek_token(parser)->symbol;
        type_con_local_ptr     = necro_parse_ast_create_conid(&parser->ast.arena, necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc, symbol, con_type);
        necro_parse_consume_token(parser);
    }
    // []
    else if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    {
        necro_parse_consume_token(parser);
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
        {
            necro_parse_restore(parser, snapshot);
            return null_local_ptr;
        }
        type_con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, necro_intern_string(parser->intern, "[]"), con_type);
        necro_parse_consume_token(parser);
    }
    // Constructor
    else
    {
        type_con_local_ptr = necro_parse_tycon(parser, con_type);
    }
    if (type_con_local_ptr == null_local_ptr)
        necro_parse_restore(parser, snapshot);
    return type_con_local_ptr;
}

NecroParseAstLocalPtr necro_parse_tycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    // ()
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_UNIT)
    {
        NecroSymbol           symbol = necro_parse_peek_token(parser)->symbol;
        NecroParseAstLocalPtr local  = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, symbol, con_type);
        necro_parse_consume_token(parser);
        return local;
    }

    // IDENTIFIER
    NecroParserSnapshot snapshot           = necro_parse_snapshot(parser);
    NecroLexToken*      look_ahead_token   = necro_parse_peek_token(parser);

    if (look_ahead_token->token != NECRO_LEX_TYPE_IDENTIFIER)
    {
        necro_parse_restore(parser, snapshot);
        return null_local_ptr;
    }

    NecroParseAstLocalPtr type_con_local_ptr = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, look_ahead_token->symbol, con_type);
    necro_parse_consume_token(parser);
    return type_con_local_ptr;
}

NecroParseAstLocalPtr necro_parse_qtycon(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    // Save until we actually have modules...
 	// [ modid . ] conid
    return necro_parse_tycon(parser, con_type);
}

NecroParseAstLocalPtr necro_parse_tyvar(NecroParser* parser, NECRO_VAR_TYPE var_type)
{
    NecroParserSnapshot snapshot           = necro_parse_snapshot(parser);
    NecroLexToken*      look_ahead_token   = necro_parse_peek_token(parser);

    if (look_ahead_token->token != NECRO_LEX_IDENTIFIER)
    {
        necro_parse_restore(parser, snapshot);
        return null_local_ptr;
    }

    // Finish
    NecroParseAstLocalPtr type_var_local_ptr = necro_parse_ast_create_var(&parser->ast.arena, necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc, look_ahead_token->symbol, var_type, null_local_ptr, NECRO_TYPE_ZERO_ORDER);
    necro_parse_consume_token(parser);
    return type_var_local_ptr;
}

NecroResult(NecroParseAstLocalPtr) necro_parse_list_type(NecroParser* parser)
{
    // NOTE: Removing support for Lists
    if (true)
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    NecroParserSnapshot   snapshot   = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr ptr        = null_local_ptr;

    // [
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_BRACKET)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // type
    ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_type(parser));
    if (ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ]
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACKET)
    {
        return necro_type_list_expected_right_bracket(source_loc, necro_parse_peek_token(parser)->end_loc);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr list_ptr = necro_parse_ast_create_conid(&parser->ast.arena, necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc, necro_intern_string(parser->intern, "[]"), NECRO_CON_TYPE_VAR);
    NecroParseAstLocalPtr app_ptr  = necro_parse_ast_create_type_app(&parser->ast.arena, necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc, list_ptr, ptr);
    return ok_NecroParseAstLocalPtr(app_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_tuple_type(NecroParser* parser)
{
    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // (
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // types
    NecroParseAstLocalPtr list_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_type));
    if (list_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // require at least k = 2
    {
        NecroParseAst* list_node = necro_parse_ast_get_node(&parser->ast, list_local_ptr);
        if (list_node->list.next_item == null_local_ptr)
        {
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
    }

    // )
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_tuple(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, list_local_ptr);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

//=====================================================
// Type Declarations
//=====================================================
NecroParseAstLocalPtr necro_parse_tycls(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    return necro_parse_tycon(parser, con_type);
}

NecroParseAstLocalPtr necro_parse_qtycls(NecroParser* parser, NECRO_CON_TYPE con_type)
{
    return necro_parse_qtycon(parser, con_type);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_simple_class(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;

    // qtycl
    NecroParseAstLocalPtr conid = necro_parse_qtycls(parser, NECRO_CON_TYPE_VAR);
    if (conid == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // varid
    NecroParseAstLocalPtr varid = necro_parse_tyvar(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (varid == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_class_context(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, conid, varid);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

bool necro_is_empty_list_node(NecroParser* parser, NecroParseAstLocalPtr list_ptr)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return null_local_ptr;
    if (list_ptr == null_local_ptr) return true;
    NecroParseAst* list_node = necro_parse_ast_get_node(&parser->ast, list_ptr);
    return list_node == NULL || list_node->list.item == null_local_ptr;
}

NecroResult(NecroParseAstLocalPtr) necro_parse_context(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_PAREN)
    {
        necro_parse_consume_token(parser);
        NecroParseAstLocalPtr class_list = necro_try_result(NecroParseAstLocalPtr, necro_parse_list(parser, NECRO_LEX_COMMA, necro_parse_simple_class));
        if (class_list == null_local_ptr || necro_is_empty_list_node(parser, class_list))
        {
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
        {
            necro_parse_restore(parser, snapshot);
            return ok_NecroParseAstLocalPtr(null_local_ptr);
        }
        // return write_error_and_restore(parser, snapshot, "Expected ')' at end of context, but found %s", necro_lex_token_type_string(peek_token_type(parser)));
        necro_parse_consume_token(parser);
        return ok_NecroParseAstLocalPtr(class_list);
    }
    else
    {
        return necro_parse_simple_class(parser);
    }
}

NecroResult(NecroParseAstLocalPtr) necro_parse_type_signature(NecroParser* parser, NECRO_SIG_TYPE sig_type)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);
    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroSourceLoc      end_loc    = necro_parse_peek_token(parser)->end_loc;

    // var
    NecroParseAstLocalPtr var;
    if (sig_type == NECRO_SIG_DECLARATION)
        var = necro_parse_variable(parser, NECRO_VAR_SIG);
    else if (sig_type == NECRO_SIG_TYPE_CLASS)
        var = necro_parse_variable(parser, NECRO_VAR_CLASS_SIG);
    else if (sig_type == NECRO_SIG_TYPE_VAR)
        var = necro_parse_variable(parser, NECRO_VAR_TYPE_VAR_DECLARATION);
    else
        var = null_local_ptr;

    if (var == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ::
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_DOUBLE_COLON)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // context
    NecroParserSnapshot   context_snapshot = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr context          = necro_try_result(NecroParseAstLocalPtr, necro_parse_context(parser));

    // =>
    if (context != null_local_ptr)
    {
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_FAT_RIGHT_ARROW)
        {
            necro_parse_restore(parser, context_snapshot);
            context = null_local_ptr;
        }
        else
        {
            necro_parse_consume_token(parser);
        }
    }

    // type
    NecroParseAstLocalPtr type = necro_try_result(NecroParseAstLocalPtr, necro_parse_type(parser));
    if (type == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_type_signature(&parser->ast.arena, source_loc, end_loc, var, context, type, sig_type);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_class_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParseAstLocalPtr declaration_local_ptr = null_local_ptr;

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_type_signature(parser, NECRO_SIG_TYPE_CLASS));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_simple_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat_assignment(parser));
    }

    return ok_NecroParseAstLocalPtr(declaration_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_class_declarations_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);

    NecroParseAstLocalPtr declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_class_declaration(parser));

    if (declaration_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    NecroParseAstLocalPtr next_decl = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
    {
        necro_parse_consume_token(parser); // consume SemiColon token
        next_decl = necro_try_result(NecroParseAstLocalPtr, necro_parse_class_declarations_list(parser));
    }

    NecroParseAstLocalPtr decl_local_ptr = necro_parse_ast_create_decl(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, declaration_local_ptr, next_decl);
    return ok_NecroParseAstLocalPtr(decl_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_type_class_declaration(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // class
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_CLASS)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // context
    NecroParserSnapshot   context_snapshot = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr context          = necro_try_result(NecroParseAstLocalPtr, necro_parse_context(parser));

    // =>
    if (context != null_local_ptr)
    {
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_FAT_RIGHT_ARROW)
        {
            necro_parse_restore(parser, context_snapshot);
            context = null_local_ptr;
        }
        else
        {
            necro_parse_consume_token(parser);
        }
    }

    // tycls
    NecroParseAstLocalPtr tycls = necro_parse_tycls(parser, NECRO_CON_TYPE_DECLARATION);
    if (tycls == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // tyvar
    NecroParseAstLocalPtr tyvar = necro_parse_tyvar(parser, NECRO_VAR_TYPE_FREE_VAR);
    if (tyvar == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // declarations
    NecroParseAstLocalPtr declarations = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        necro_parse_consume_token(parser);
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_BRACE)
        {
            necro_parse_consume_token(parser);
            declarations = necro_try_result(NecroParseAstLocalPtr, necro_parse_class_declarations_list(parser));
            if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                return necro_class_expected_right_brace_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
            necro_parse_consume_token(parser);
        }
    }

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_class_declaration(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, context, tycls, tyvar, declarations);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_instance_declaration(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON    ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParseAstLocalPtr declaration_local_ptr = null_local_ptr;

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_simple_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_apats_assignment(parser));
    }

    if (declaration_local_ptr == null_local_ptr)
    {
        declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_pat_assignment(parser));
    }

    return ok_NecroParseAstLocalPtr(declaration_local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_instance_declaration_list(NecroParser* parser)
{
    const NECRO_LEX_TOKEN_TYPE token_type = necro_parse_peek_token_type(parser);
    if (token_type == NECRO_LEX_END_OF_STREAM ||
        token_type == NECRO_LEX_SEMI_COLON    ||
        token_type == NECRO_LEX_RIGHT_BRACE)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;

    // Declaration
    NecroParserSnapshot   snapshot              = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr declaration_local_ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_instance_declaration(parser));
    if (declaration_local_ptr == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // ;
    NecroParseAstLocalPtr next_decl = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_SEMI_COLON)
    {
        necro_parse_consume_token(parser);
        next_decl = necro_try_result(NecroParseAstLocalPtr, necro_parse_instance_declaration_list(parser));
    }

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_decl(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, declaration_local_ptr, next_decl);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_inst_constr(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok(NecroParseAstLocalPtr, null_local_ptr);

    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);

    // (
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_LEFT_PAREN)
    {
        necro_parse_restore(parser, snapshot);
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // tycon
    NecroParseAstLocalPtr tycon = necro_parse_gtycon(parser, NECRO_CON_TYPE_VAR);
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM || tycon == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    }

    // tyvar list
    NecroParseAstLocalPtr ty_var_list = necro_try_result(NecroParseAstLocalPtr, necro_parse_tyvar_list(parser));
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
    {
        necro_parse_restore(parser, snapshot);
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    }

    // )
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_PAREN)
    {
        necro_parse_restore(parser, snapshot);
        return ok(NecroParseAstLocalPtr, null_local_ptr);
    }
    necro_parse_consume_token(parser);

    // Finish
    NecroParseAstLocalPtr ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, tycon, ty_var_list);
    return ok(NecroParseAstLocalPtr, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_inst(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok(NecroParseAstLocalPtr, null_local_ptr);

    NecroSourceLoc      source_loc = necro_parse_peek_token(parser)->source_loc;
    NecroParserSnapshot snapshot   = necro_parse_snapshot(parser);

    // gtycon
    NecroParseAstLocalPtr ptr = necro_parse_gtycon(parser, NECRO_CON_TYPE_VAR);
    if (ptr != null_local_ptr)
        return ok(NecroParseAstLocalPtr, ptr);

    ptr = necro_try_result(NecroParseAstLocalPtr, necro_parse_inst_constr(parser));
    if (ptr != null_local_ptr)
        return ok(NecroParseAstLocalPtr, ptr);

    // [tyvar]
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_BRACKET)
    {
        necro_parse_consume_token(parser);
        ptr = necro_parse_tyvar(parser, NECRO_VAR_VAR);
        if (ptr != null_local_ptr)
        {
            if (necro_parse_peek_token_type(parser) == NECRO_LEX_RIGHT_BRACKET)
            {
                NecroParseAstLocalPtr con_ptr  = necro_parse_ast_create_conid(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, necro_intern_string(parser->intern, "[]"), NECRO_CON_TYPE_VAR);
                NecroParseAstLocalPtr list_ptr = necro_parse_ast_create_constructor(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, con_ptr, ptr);
                necro_parse_consume_token(parser);
                return ok(NecroParseAstLocalPtr, list_ptr);
            }
        }
        necro_parse_restore(parser, snapshot);
        ptr = null_local_ptr;
    }
    if (ptr != null_local_ptr)
        return ok(NecroParseAstLocalPtr, ptr);

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

    // Nothing recognized, rewind
    necro_parse_restore(parser, snapshot);
    return ok(NecroParseAstLocalPtr, ptr);
}

NecroResult(NecroParseAstLocalPtr) necro_parse_type_class_instance(NecroParser* parser)
{
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_END_OF_STREAM)
        return ok_NecroParseAstLocalPtr(null_local_ptr);

    NecroParserSnapshot snapshot = necro_parse_snapshot(parser);

    // instance
    if (necro_parse_peek_token_type(parser) != NECRO_LEX_INSTANCE)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }
    NecroSourceLoc source_loc = necro_parse_peek_token(parser)->source_loc;
    necro_parse_consume_token(parser);

    // context
    NecroParserSnapshot   context_snapshot = necro_parse_snapshot(parser);
    NecroParseAstLocalPtr context          = necro_try_result(NecroParseAstLocalPtr, necro_parse_context(parser));

    // =>
    if (context != null_local_ptr)
    {
        if (necro_parse_peek_token_type(parser) != NECRO_LEX_FAT_RIGHT_ARROW)
        {
            necro_parse_restore(parser, context_snapshot);
            context = null_local_ptr;
        }
        else
        {
            necro_parse_consume_token(parser);
        }
    }

    // qtycls
    NecroParseAstLocalPtr qtycls = necro_parse_qtycls(parser, NECRO_CON_TYPE_VAR);
    if (qtycls == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // inst
    NecroParseAstLocalPtr inst = necro_try_result(NecroParseAstLocalPtr, necro_parse_inst(parser));
    if (inst == null_local_ptr)
    {
        necro_parse_restore(parser, snapshot);
        return ok_NecroParseAstLocalPtr(null_local_ptr);
    }

    // declarations
    NecroParseAstLocalPtr declarations = null_local_ptr;
    if (necro_parse_peek_token_type(parser) == NECRO_LEX_WHERE)
    {
        necro_parse_consume_token(parser);
        if (necro_parse_peek_token_type(parser) == NECRO_LEX_LEFT_BRACE)
        {
            necro_parse_consume_token(parser);
            declarations = necro_try_result(NecroParseAstLocalPtr, necro_parse_instance_declaration_list(parser));
            if (necro_parse_peek_token_type(parser) != NECRO_LEX_RIGHT_BRACE)
                return necro_instance_expected_right_brace_error(necro_parse_peek_token(parser)->source_loc, necro_parse_peek_token(parser)->end_loc);
            necro_parse_consume_token(parser);
        }
    }

    // Finish
    NecroParseAstLocalPtr local_ptr = necro_parse_ast_create_instance(&parser->ast.arena, source_loc, necro_parse_peek_token(parser)->end_loc, context, qtycls, inst, declarations);
    return ok_NecroParseAstLocalPtr(local_ptr);
}

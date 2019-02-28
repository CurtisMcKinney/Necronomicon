/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "symtable.h"
#include "type_class.h"
#include "ast.h"
#include "d_analyzer.h"

#define AST_TAB "  "

void necro_ast_print_go(NecroAst* ast, uint32_t depth)
{
    for (uint32_t i = 0;  i < depth; ++i)
    {
        printf(AST_TAB);
    }

    if (ast == NULL)
    {
        printf("NULL\n");
        return;
    }

    switch(ast->type)
    {
    case NECRO_AST_BIN_OP:
        printf("(%s)\n", necro_ast_symbol_most_qualified_name(ast->bin_op.ast_symbol));
        necro_ast_print_go(ast->bin_op.lhs, depth + 1);
        necro_ast_print_go(ast->bin_op.rhs, depth + 1);
        break;

    case NECRO_AST_CONSTANT:
        switch(ast->constant.type)
        {
        case NECRO_AST_CONSTANT_FLOAT_PATTERN:   // FALL THROUGH
            printf("pat_float: ");
        case NECRO_AST_CONSTANT_FLOAT:
            printf("(%f)\n", ast->constant.double_literal);
            break;
        case NECRO_AST_CONSTANT_INTEGER_PATTERN: // FALL THROUGH
            printf("pat_int: ");
        case NECRO_AST_CONSTANT_TYPE_INT:
        case NECRO_AST_CONSTANT_INTEGER:
#if WIN32
            printf("(%lli)\n", ast->constant.int_literal);
#else
            printf("(%li)\n", ast->constant.int_literal);
#endif
            break;
        case NECRO_AST_CONSTANT_STRING:
            {
                const char* string = ast->constant.symbol->str;
                if (string)
                    printf("(\"%s\")\n", string);
            }
            break;
        case NECRO_AST_CONSTANT_CHAR:
            printf("(\'%c\')\n", ast->constant.char_literal);
            break;
        }
        break;

    case NECRO_AST_IF_THEN_ELSE:
        puts("(If then else)");
        necro_ast_print_go(ast->if_then_else.if_expr, depth + 1);
        necro_ast_print_go(ast->if_then_else.then_expr, depth + 1);
        necro_ast_print_go(ast->if_then_else.else_expr, depth + 1);
        break;

    case NECRO_AST_TOP_DECL:
        puts("(Top Declaration)");
        necro_ast_print_go(ast->top_declaration.declaration, depth + 1);
        if (ast->top_declaration.next_top_decl != NULL)
        {
            necro_ast_print_go(ast->top_declaration.next_top_decl, depth);
        }
        break;

    case NECRO_AST_DECL:
        puts("(Declaration)");
        necro_ast_print_go(ast->declaration.declaration_impl, depth + 1);
        if (ast->declaration.next_declaration != NULL)
        {
            necro_ast_print_go(ast->declaration.next_declaration, depth);
        }
        break;

    case NECRO_AST_DECLARATION_GROUP_LIST:
        puts("(DeclarationGroupList)");
        necro_ast_print_go(ast->declaration_group_list.declaration_group, depth + 1);
        if (ast->declaration_group_list.next != NULL)
            necro_ast_print_go(ast->declaration_group_list.next, depth);
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        printf("(Assignment: %s)\n", necro_ast_symbol_most_qualified_name(ast->simple_assignment.ast_symbol));
        if (ast->simple_assignment.initializer != NULL)
        {
            print_white_space(depth * 2);
            printf(" ~\n");
            necro_ast_print_go(ast->simple_assignment.initializer, depth * 2);
        }
        necro_ast_print_go(ast->simple_assignment.rhs, depth + 1);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        puts("(Right Hand Side)");
        necro_ast_print_go(ast->right_hand_side.expression, depth + 1);
        if (ast->right_hand_side.declarations != NULL)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "where");
            necro_ast_print_go(ast->right_hand_side.declarations, depth + 3);
        }
        break;

    case NECRO_AST_LET_EXPRESSION:
        puts("(Let)");
        necro_ast_print_go(ast->let_expression.declarations, depth + 1);
        if (ast->right_hand_side.declarations != NULL)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "in");
            necro_ast_print_go(ast->let_expression.expression, depth + 3);
        }
        break;


    case NECRO_AST_FUNCTION_EXPRESSION:
        puts("(fexp)");
        necro_ast_print_go(ast->fexpression.aexp, depth + 1);
        if (ast->fexpression.next_fexpression != NULL)
        {
            necro_ast_print_go(ast->fexpression.next_fexpression, depth + 1);
        }
        break;

    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_SIG:
        case NECRO_VAR_CLASS_SIG:
        case NECRO_VAR_VAR:
        case NECRO_VAR_DECLARATION:
            printf("(var: %s, type: %s)\n", necro_ast_symbol_most_qualified_name(ast->variable.ast_symbol), necro_var_type_string(ast->variable.var_type));
            break;
        default:
            printf("(var: %s, type: %s)\n", ast->variable.ast_symbol->source_name->str, necro_var_type_string(ast->variable.var_type));
            break;
        }
        if (ast->variable.initializer != NULL)
        {
            print_white_space(depth + 10);
            printf("~\n");
            necro_ast_print_go(ast->variable.initializer, depth + 1);
        }
        break;

    case NECRO_AST_APATS:
        puts("(Apat)");
        necro_ast_print_go(ast->apats.apat, depth + 1);
        if (ast->apats.next_apat != NULL)
        {
            necro_ast_print_go(ast->apats.next_apat, depth);
        }
        break;

    case NECRO_AST_WILDCARD:
        puts("(_)");
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
        printf("(Apats Assignment: %s)\n", necro_ast_symbol_most_qualified_name(ast->apats_assignment.ast_symbol));
        necro_ast_print_go(ast->apats_assignment.apats, depth + 1);
        necro_ast_print_go(ast->apats_assignment.rhs, depth + 1);
        break;

    case NECRO_AST_PAT_ASSIGNMENT:
        printf("(Pat Assignment)\n");
        necro_ast_print_go(ast->pat_assignment.pat, depth + 1);
        necro_ast_print_go(ast->pat_assignment.rhs, depth + 1);
        break;

    case NECRO_AST_LAMBDA:
        puts("\\(lambda)");
        necro_ast_print_go(ast->lambda.apats, depth + 1);
        for (uint32_t i = 0;  i < (depth + 1); ++i)
        {
            printf(AST_TAB);
        }
        puts("->");
        necro_ast_print_go(ast->lambda.expression, depth + 2);
        break;

    case NECRO_AST_DO:
        puts("(do)");
        necro_ast_print_go(ast->do_statement.statement_list, depth + 1);
        break;

    case NECRO_AST_PAT_EXPRESSION:
        puts("(pat)");
        necro_ast_print_go(ast->pattern_expression.expressions, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        puts("([])");
        if (ast->expression_list.expressions != NULL)
            necro_ast_print_go(ast->expression_list.expressions, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_ARRAY:
        puts("({})");
        if (ast->expression_array.expressions != NULL)
            necro_ast_print_go(ast->expression_array.expressions, depth + 1);
        break;

    case NECRO_AST_TUPLE:
        puts("(tuple)");
        necro_ast_print_go(ast->expression_list.expressions, depth + 1);
        break;

    case NECRO_AST_LIST_NODE:
        printf("\r"); // clear current line
        necro_ast_print_go(ast->list.item, depth);
        if (ast->list.next_item != NULL)
        {
            necro_ast_print_go(ast->list.next_item, depth);
        }
        break;

    case NECRO_BIND_ASSIGNMENT:
        printf("(Bind: %s)\n", necro_ast_symbol_most_qualified_name(ast->bind_assignment.ast_symbol));
        necro_ast_print_go(ast->bind_assignment.expression, depth + 1);
        break;

    case NECRO_PAT_BIND_ASSIGNMENT:
        printf("(Pat Bind)\n");
        necro_ast_print_go(ast->pat_bind_assignment.pat, depth + 1);
        necro_ast_print_go(ast->pat_bind_assignment.expression, depth + 1);
        break;

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        {
            switch(ast->arithmetic_sequence.type)
            {
            case NECRO_ARITHMETIC_ENUM_FROM:
                puts("(EnumFrom)");
                necro_ast_print_go(ast->arithmetic_sequence.from, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_TO:
                puts("(EnumFromTo)");
                necro_ast_print_go(ast->arithmetic_sequence.from, depth + 1);
                necro_ast_print_go(ast->arithmetic_sequence.to, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_THEN_TO:
                puts("(EnumFromThenTo)");
                necro_ast_print_go(ast->arithmetic_sequence.from, depth + 1);
                necro_ast_print_go(ast->arithmetic_sequence.then, depth + 1);
                necro_ast_print_go(ast->arithmetic_sequence.to, depth + 1);
                break;
            default:
                assert(false);
                break;
            }
        }
        break;

    case NECRO_AST_CASE:
        puts("case");
        necro_ast_print_go(ast->case_expression.expression, depth + 1);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("of");
        necro_ast_print_go(ast->case_expression.alternatives, depth + 1);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        printf("\r"); // clear current line
        necro_ast_print_go(ast->case_alternative.pat, depth + 0);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("->");
        necro_ast_print_go(ast->case_alternative.body, depth + 1);
        break;

    case NECRO_AST_CONID:
        printf("(conid: %s, ctype: %s)\n", necro_ast_symbol_most_qualified_name(ast->conid.ast_symbol), necro_con_type_string(ast->conid.con_type));
        break;

    case NECRO_AST_DATA_DECLARATION:
        puts("(data)");
        necro_ast_print_go(ast->data_declaration.simpletype, depth + 1);
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts(" = ");
        necro_ast_print_go(ast->data_declaration.constructor_list, depth + 1);
        break;

    case NECRO_AST_SIMPLE_TYPE:
        puts("(simple type)");
        necro_ast_print_go(ast->simple_type.type_con, depth + 1);
        if (ast->simple_type.type_var_list != NULL)
            necro_ast_print_go(ast->simple_type.type_var_list, depth + 2);
        break;

    case NECRO_AST_CONSTRUCTOR:
        puts("(constructor)");
        necro_ast_print_go(ast->constructor.conid, depth + 1);
        if (ast->constructor.arg_list != NULL)
            necro_ast_print_go(ast->constructor.arg_list, depth + 1);
        break;

    case NECRO_AST_TYPE_APP:
        puts("(type app)");
        necro_ast_print_go(ast->type_app.ty, depth + 1);
        if (ast->type_app.next_ty != NULL)
            necro_ast_print_go(ast->type_app.next_ty, depth + 1);
        break;

    case NECRO_AST_BIN_OP_SYM:
        printf("(%s)\n", necro_ast_symbol_most_qualified_name(ast->bin_op_sym.op->conid.ast_symbol));
        necro_ast_print_go(ast->bin_op_sym.left, depth + 1);
        necro_ast_print_go(ast->bin_op_sym.right, depth + 1);
        break;

    case NECRO_AST_OP_LEFT_SECTION:
        printf("(LeftSection %s)\n", necro_ast_symbol_most_qualified_name(ast->op_left_section.ast_symbol));
        necro_ast_print_go(ast->op_left_section.left, depth + 1);
        break;

    case NECRO_AST_OP_RIGHT_SECTION:
        printf("(%s RightSection)\n", necro_ast_symbol_most_qualified_name(ast->op_right_section.ast_symbol));
        necro_ast_print_go(ast->op_right_section.right, depth + 1);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        printf("\r");
        necro_ast_print_go(ast->type_signature.var, depth + 0);
        for (uint32_t i = 0;  i < depth + 1; ++i) printf(AST_TAB);
        puts("::");
        if (ast->type_signature.context != NULL)
        {
            necro_ast_print_go(ast->type_signature.context, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        necro_ast_print_go(ast->type_signature.type, depth + 1);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        printf("\r");
        necro_ast_print_go(ast->type_class_context.conid, depth + 0);
        necro_ast_print_go(ast->type_class_context.varid, depth + 0);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        puts("(class)");
        if (ast->type_class_declaration.context != NULL)
        {
            necro_ast_print_go(ast->type_class_declaration.context, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        necro_ast_print_go(ast->type_class_declaration.tycls, depth + 1);
        necro_ast_print_go(ast->type_class_declaration.tyvar, depth + 1);
        if (ast->type_class_declaration.declarations != NULL)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            necro_ast_print_go(ast->type_class_declaration.declarations, depth + 2);
        }
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        puts("(instance)");
        if (ast->type_class_instance.context != NULL)
        {
            necro_ast_print_go(ast->type_class_instance.context, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        necro_ast_print_go(ast->type_class_instance.qtycls, depth + 1);
        necro_ast_print_go(ast->type_class_instance.inst, depth + 1);
        if (ast->type_class_declaration.declarations != NULL)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            necro_ast_print_go(ast->type_class_declaration.declarations, depth + 2);
        }
        break;

    case NECRO_AST_FUNCTION_TYPE:
        puts("(");
        necro_ast_print_go(ast->function_type.type, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts("->");
        necro_ast_print_go(ast->function_type.next_on_arrow, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts(")");
        break;

    default:
        puts("(Undefined)");
        break;
    }
}

void necro_ast_print(NecroAst* ast)
{
    if (ast == NULL)
    {
        puts("(Empty AST)");
    }
    else
    {
        necro_ast_print_go(ast, 0);
    }
}

void necro_ast_arena_print(NecroAstArena* ast)
{
    if (ast->root == NULL)
    {
        puts("(Empty AST)");
    }
    else
    {
        necro_ast_print_go(ast->root, 0);
    }
}

NecroAst* necro_reify_go(NecroParseAstArena* parse_ast_arena, NecroParseAstLocalPtr parse_ast_ptr, NecroPagedArena* arena, NecroIntern* intern);
NecroAstArena necro_reify(NecroCompileInfo info, NecroIntern* intern, NecroParseAstArena* ast)
{
    NecroAstArena ast_arena = necro_ast_arena_create(ast->module_name);
    ast_arena.root          = necro_reify_go(ast, ast->root, &ast_arena.arena, intern);
    if (info.compilation_phase == NECRO_PHASE_REIFY && info.verbosity > 0)
        necro_ast_print(ast_arena.root);
    return ast_arena;
}

// TODO (Curtis, 2-14-19): Refactor to use ast creation functions
NecroAst* necro_reify_go(NecroParseAstArena* parse_ast_arena, NecroParseAstLocalPtr parse_ast_ptr, NecroPagedArena* arena, NecroIntern* intern)
{
    if (parse_ast_ptr == null_local_ptr)
        return NULL;
    NecroParseAst* ast = necro_parse_ast_get_node(parse_ast_arena, parse_ast_ptr);
    if (ast == NULL)
        return NULL;
    NecroAst* reified_ast   = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    reified_ast->type       = ast->type;
    reified_ast->source_loc = ast->source_loc;
    reified_ast->end_loc    = ast->end_loc;
    reified_ast->scope      = NULL;
    reified_ast->necro_type = NULL;
    switch (ast->type)
    {
    case NECRO_AST_UNDEFINED:
        reified_ast->undefined._pad = ast->undefined._pad;
        break;

    case NECRO_AST_CONSTANT:
        switch (ast->constant.type)
        {

        case NECRO_AST_CONSTANT_FLOAT:
        {
            NecroAst* from_ast               = necro_ast_create_var(arena, intern, "fromRational", NECRO_VAR_VAR);
            from_ast->source_loc             = reified_ast->source_loc;
            from_ast->end_loc                = reified_ast->end_loc;
            NecroAst* new_ast                = necro_paged_arena_alloc(arena, sizeof(NecroAst));
            *new_ast                         = *reified_ast;
            new_ast->constant.double_literal = ast->constant.double_literal;
            new_ast->constant.type           = ast->constant.type;
            new_ast->constant.pat_from_ast   = NULL;
            new_ast->constant.pat_eq_ast     = NULL;
            new_ast->source_loc              = reified_ast->source_loc;
            new_ast->end_loc                 = reified_ast->end_loc;
            reified_ast                      = necro_ast_create_fexpr(arena, from_ast, new_ast);
            reified_ast->source_loc          = reified_ast->source_loc;
            reified_ast->end_loc             = reified_ast->end_loc;
            break;
        }

        case NECRO_AST_CONSTANT_INTEGER:
        {
            NecroAst* from_ast             = necro_ast_create_var(arena, intern, "fromInt", NECRO_VAR_VAR);
            from_ast->source_loc           = reified_ast->source_loc;
            from_ast->end_loc              = reified_ast->end_loc;
            NecroAst* new_ast              = necro_paged_arena_alloc(arena, sizeof(NecroAst));
            *new_ast                       = *reified_ast;
            new_ast->constant.int_literal  = ast->constant.int_literal;
            new_ast->constant.type         = ast->constant.type;
            new_ast->constant.pat_from_ast = NULL;
            new_ast->constant.pat_eq_ast   = NULL;
            new_ast->source_loc            = reified_ast->source_loc;
            new_ast->end_loc               = reified_ast->end_loc;
            reified_ast                    = necro_ast_create_fexpr(arena, from_ast, new_ast);
            reified_ast->source_loc        = reified_ast->source_loc;
            reified_ast->end_loc           = reified_ast->end_loc;
            break;
        }

        case NECRO_AST_CONSTANT_FLOAT_PATTERN:
            reified_ast->constant.double_literal = ast->constant.double_literal;
            reified_ast->constant.type           = ast->constant.type;
            reified_ast->constant.pat_from_ast   = NULL;
            reified_ast->constant.pat_eq_ast     = NULL;
            break;

        case NECRO_AST_CONSTANT_TYPE_INT:
        case NECRO_AST_CONSTANT_INTEGER_PATTERN:
            reified_ast->constant.int_literal  = ast->constant.int_literal;
            reified_ast->constant.type         = ast->constant.type;
            reified_ast->constant.pat_from_ast = NULL;
            reified_ast->constant.pat_eq_ast   = NULL;
            break;
        case NECRO_AST_CONSTANT_CHAR_PATTERN:
        case NECRO_AST_CONSTANT_CHAR:
            reified_ast->constant.char_literal = ast->constant.char_literal;
            reified_ast->constant.type         = ast->constant.type;
            reified_ast->constant.pat_from_ast = NULL;
            reified_ast->constant.pat_eq_ast   = NULL;
            break;
        case NECRO_AST_CONSTANT_STRING:
            reified_ast->constant.symbol       = ast->constant.symbol;
            reified_ast->constant.type         = ast->constant.type;
            reified_ast->constant.pat_from_ast = NULL;
            reified_ast->constant.pat_eq_ast   = NULL;
            break;
        }
        break;

    case NECRO_AST_UN_OP:
        break;
    case NECRO_AST_BIN_OP:
        reified_ast->bin_op.lhs          = necro_reify_go(parse_ast_arena, ast->bin_op.lhs, arena, intern);
        reified_ast->bin_op.rhs          = necro_reify_go(parse_ast_arena, ast->bin_op.rhs, arena, intern);
        reified_ast->bin_op.ast_symbol   = necro_ast_symbol_create(arena, ast->bin_op.symbol, ast->bin_op.symbol, NULL, NULL);
        reified_ast->bin_op.type         = ast->bin_op.type;
        reified_ast->bin_op.inst_context = NULL;
        reified_ast->bin_op.inst_subs    = NULL;
        break;
    case NECRO_AST_IF_THEN_ELSE:
        reified_ast->if_then_else.if_expr   = necro_reify_go(parse_ast_arena, ast->if_then_else.if_expr, arena, intern);
        reified_ast->if_then_else.then_expr = necro_reify_go(parse_ast_arena, ast->if_then_else.then_expr, arena, intern);
        reified_ast->if_then_else.else_expr = necro_reify_go(parse_ast_arena, ast->if_then_else.else_expr, arena, intern);
        break;
    case NECRO_AST_TOP_DECL:
        reified_ast->top_declaration.declaration   = necro_reify_go(parse_ast_arena, ast->top_declaration.declaration, arena, intern);
        reified_ast->top_declaration.next_top_decl = necro_reify_go(parse_ast_arena, ast->top_declaration.next_top_decl, arena, intern);
        // reified_ast->top_declaration.group_list    = NULL;
        break;
    case NECRO_AST_DECL:
        reified_ast->declaration.declaration_impl       = necro_reify_go(parse_ast_arena, ast->declaration.declaration_impl, arena, intern);
        reified_ast->declaration.next_declaration       = necro_reify_go(parse_ast_arena, ast->declaration.next_declaration, arena, intern);
        reified_ast->declaration.declaration_group_list = NULL;
        reified_ast->declaration.info                   = NULL;
        reified_ast->declaration.index                  = -1;
        reified_ast->declaration.low_link               = 0;
        reified_ast->declaration.on_stack               = false;
        reified_ast->declaration.type_checked           = false;
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        reified_ast->simple_assignment.initializer       = necro_reify_go(parse_ast_arena, ast->simple_assignment.initializer, arena, intern);
        reified_ast->simple_assignment.rhs               = necro_reify_go(parse_ast_arena, ast->simple_assignment.rhs, arena, intern);
        reified_ast->simple_assignment.declaration_group = NULL;
        reified_ast->simple_assignment.is_recursive      = false;
        reified_ast->simple_assignment.ast_symbol        = necro_ast_symbol_create(arena, ast->simple_assignment.variable_name, ast->simple_assignment.variable_name, parse_ast_arena->module_name, reified_ast);
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        reified_ast->apats_assignment.apats             = necro_reify_go(parse_ast_arena, ast->apats_assignment.apats, arena, intern);
        reified_ast->apats_assignment.rhs               = necro_reify_go(parse_ast_arena, ast->apats_assignment.rhs, arena, intern);
        reified_ast->apats_assignment.declaration_group = NULL;
        reified_ast->apats_assignment.ast_symbol        = necro_ast_symbol_create(arena, ast->apats_assignment.variable_name, ast->apats_assignment.variable_name, parse_ast_arena->module_name, reified_ast);
        reified_ast->apats_assignment.is_recursive      = false;
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        reified_ast->pat_assignment.pat                      = necro_reify_go(parse_ast_arena, ast->pat_assignment.pat, arena, intern);
        reified_ast->pat_assignment.rhs                      = necro_reify_go(parse_ast_arena, ast->pat_assignment.rhs, arena, intern);
        reified_ast->pat_assignment.declaration_group        = NULL;
        reified_ast->pat_assignment.optional_type_signatures = NULL;
        break;
    case NECRO_AST_RIGHT_HAND_SIDE:
        reified_ast->right_hand_side.expression   = necro_reify_go(parse_ast_arena, ast->right_hand_side.expression, arena, intern);
        reified_ast->right_hand_side.declarations = necro_reify_go(parse_ast_arena, ast->right_hand_side.declarations, arena, intern);
        break;
    case NECRO_AST_LET_EXPRESSION:
        reified_ast->let_expression.expression   = necro_reify_go(parse_ast_arena, ast->let_expression.expression, arena, intern);
        reified_ast->let_expression.declarations = necro_reify_go(parse_ast_arena, ast->let_expression.declarations, arena, intern);
        break;
    case NECRO_AST_FUNCTION_EXPRESSION:
        reified_ast->fexpression.aexp             = necro_reify_go(parse_ast_arena, ast->fexpression.aexp, arena, intern);
        reified_ast->fexpression.next_fexpression = necro_reify_go(parse_ast_arena, ast->fexpression.next_fexpression, arena, intern);
        break;
    case NECRO_AST_VARIABLE:
        switch (ast->variable.var_type)
        {
        case NECRO_VAR_TYPE_FREE_VAR:
        case NECRO_VAR_TYPE_VAR_DECLARATION:
        case NECRO_VAR_DECLARATION:
        case NECRO_VAR_CLASS_SIG:
            reified_ast->variable.ast_symbol = necro_ast_symbol_create(arena, ast->variable.symbol, ast->variable.symbol, parse_ast_arena->module_name, reified_ast);
            break;
        case NECRO_VAR_SIG:
        case NECRO_VAR_VAR:
            reified_ast->variable.ast_symbol = necro_ast_symbol_create(arena, ast->variable.symbol, ast->variable.symbol, NULL, NULL);
            break;
        }
        reified_ast->variable.var_type     = ast->variable.var_type;
        reified_ast->variable.inst_context = NULL;
        reified_ast->variable.inst_subs    = NULL;
        reified_ast->variable.initializer  = necro_reify_go(parse_ast_arena, ast->variable.initializer, arena, intern);
        reified_ast->variable.is_recursive = false;
        reified_ast->variable.order        = ast->variable.order;
        break;
    case NECRO_AST_APATS:
        reified_ast->apats.apat      = necro_reify_go(parse_ast_arena, ast->apats.apat, arena, intern);
        reified_ast->apats.next_apat = necro_reify_go(parse_ast_arena, ast->apats.next_apat, arena, intern);
        break;
    case NECRO_AST_WILDCARD:
        reified_ast->undefined._pad = ast->undefined._pad;
        break;
    case NECRO_AST_LAMBDA:
        reified_ast->lambda.apats      = necro_reify_go(parse_ast_arena, ast->lambda.apats, arena, intern);
        reified_ast->lambda.expression = necro_reify_go(parse_ast_arena, ast->lambda.expression, arena, intern);
        break;
    case NECRO_AST_DO:
        reified_ast->do_statement.statement_list = necro_reify_go(parse_ast_arena, ast->do_statement.statement_list, arena, intern);
        reified_ast->do_statement.monad_var      = NULL;
        break;
    case NECRO_AST_PAT_EXPRESSION:
        reified_ast->pattern_expression.expressions = necro_reify_go(parse_ast_arena, ast->pattern_expression.expressions, arena, intern);
        break;
    case NECRO_AST_LIST_NODE:
        reified_ast->list.item      = necro_reify_go(parse_ast_arena, ast->list.item, arena, intern);
        reified_ast->list.next_item = necro_reify_go(parse_ast_arena, ast->list.next_item, arena, intern);
        break;
    case NECRO_AST_EXPRESSION_LIST:
        reified_ast->expression_list.expressions = necro_reify_go(parse_ast_arena, ast->expression_list.expressions, arena, intern);
        break;
    case NECRO_AST_EXPRESSION_ARRAY:
        reified_ast->expression_array.expressions = necro_reify_go(parse_ast_arena, ast->expression_array.expressions, arena, intern);
        break;
    case NECRO_AST_TUPLE:
        reified_ast->tuple.expressions = necro_reify_go(parse_ast_arena, ast->tuple.expressions, arena, intern);
        break;
    case NECRO_BIND_ASSIGNMENT:
        reified_ast->bind_assignment.expression    = necro_reify_go(parse_ast_arena, ast->bind_assignment.expression, arena, intern);
        reified_ast->bind_assignment.ast_symbol    = necro_ast_symbol_create(arena, ast->bind_assignment.variable_name, ast->bind_assignment.variable_name, parse_ast_arena->module_name, reified_ast);
        break;
    case NECRO_PAT_BIND_ASSIGNMENT:
        reified_ast->pat_bind_assignment.pat        = necro_reify_go(parse_ast_arena, ast->pat_bind_assignment.pat, arena, intern);
        reified_ast->pat_bind_assignment.expression = necro_reify_go(parse_ast_arena, ast->pat_bind_assignment.expression, arena, intern);
        break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        reified_ast->arithmetic_sequence.from = necro_reify_go(parse_ast_arena, ast->arithmetic_sequence.from, arena, intern);
        reified_ast->arithmetic_sequence.then = necro_reify_go(parse_ast_arena, ast->arithmetic_sequence.then, arena, intern);
        reified_ast->arithmetic_sequence.to   = necro_reify_go(parse_ast_arena, ast->arithmetic_sequence.to, arena, intern);
        reified_ast->arithmetic_sequence.type = ast->arithmetic_sequence.type;
        break;
    case NECRO_AST_CASE:
        reified_ast->case_expression.expression   = necro_reify_go(parse_ast_arena, ast->case_expression.expression, arena, intern);
        reified_ast->case_expression.alternatives = necro_reify_go(parse_ast_arena, ast->case_expression.alternatives, arena, intern);
        break;
    case NECRO_AST_CASE_ALTERNATIVE:
        reified_ast->case_alternative.body = necro_reify_go(parse_ast_arena, ast->case_alternative.body, arena, intern);
        reified_ast->case_alternative.pat  = necro_reify_go(parse_ast_arena, ast->case_alternative.pat, arena, intern);
        break;
    case NECRO_AST_CONID:
        reified_ast->conid.con_type   = ast->conid.con_type;
        switch (ast->conid.con_type)
        {
        case NECRO_CON_VAR:
        case NECRO_CON_TYPE_VAR:
            reified_ast->conid.ast_symbol = necro_ast_symbol_create(arena, ast->conid.symbol, ast->conid.symbol, NULL, NULL);
            break;
        case NECRO_CON_DATA_DECLARATION:
        case NECRO_CON_TYPE_DECLARATION:
            reified_ast->conid.ast_symbol = necro_ast_symbol_create(arena, ast->conid.symbol, ast->conid.symbol, parse_ast_arena->module_name, NULL);
            break;
        }
        break;
    case NECRO_AST_TYPE_APP:
        reified_ast->type_app.ty      = necro_reify_go(parse_ast_arena, ast->type_app.ty, arena, intern);
        reified_ast->type_app.next_ty = necro_reify_go(parse_ast_arena, ast->type_app.next_ty, arena, intern);
        break;
    case NECRO_AST_BIN_OP_SYM:
        reified_ast->bin_op_sym.left  = necro_reify_go(parse_ast_arena, ast->bin_op_sym.left, arena, intern);
        reified_ast->bin_op_sym.op    = necro_reify_go(parse_ast_arena, ast->bin_op_sym.op, arena, intern);
        reified_ast->bin_op_sym.right = necro_reify_go(parse_ast_arena, ast->bin_op_sym.right, arena, intern);
        break;
    case NECRO_AST_OP_LEFT_SECTION:
        reified_ast->op_left_section.left         = necro_reify_go(parse_ast_arena, ast->op_left_section.left, arena, intern);
        reified_ast->op_left_section.ast_symbol   = necro_ast_symbol_create(arena, ast->op_left_section.symbol, ast->op_left_section.symbol, NULL, reified_ast);
        reified_ast->op_left_section.type         = ast->op_left_section.type;
        reified_ast->op_left_section.inst_context = NULL;
        reified_ast->op_left_section.inst_subs    = NULL;
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        reified_ast->op_right_section.ast_symbol   = necro_ast_symbol_create(arena, ast->op_right_section.symbol, ast->op_right_section.symbol, NULL, reified_ast);
        reified_ast->op_right_section.type         = ast->op_right_section.type;
        reified_ast->op_right_section.right        = necro_reify_go(parse_ast_arena, ast->op_right_section.right, arena, intern);
        reified_ast->op_right_section.inst_context = NULL;
        reified_ast->op_right_section.inst_subs    = NULL;
        break;
    case NECRO_AST_CONSTRUCTOR:
        reified_ast->constructor.conid    = necro_reify_go(parse_ast_arena, ast->constructor.conid, arena, intern);
        reified_ast->constructor.arg_list = necro_reify_go(parse_ast_arena, ast->constructor.arg_list, arena, intern);
        break;
    case NECRO_AST_SIMPLE_TYPE:
        reified_ast->simple_type.type_con      = necro_reify_go(parse_ast_arena, ast->simple_type.type_con, arena, intern);
        reified_ast->simple_type.type_var_list = necro_reify_go(parse_ast_arena, ast->simple_type.type_var_list, arena, intern);
        break;
    case NECRO_AST_DATA_DECLARATION:
        reified_ast->data_declaration.simpletype       = necro_reify_go(parse_ast_arena, ast->data_declaration.simpletype, arena, intern);
        reified_ast->data_declaration.constructor_list = necro_reify_go(parse_ast_arena, ast->data_declaration.constructor_list, arena, intern);
        reified_ast->data_declaration.is_recursive     = false;
        reified_ast->data_declaration.ast_symbol       = NULL;
        break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:
        reified_ast->type_class_context.conid = necro_reify_go(parse_ast_arena, ast->type_class_context.conid, arena, intern);
        reified_ast->type_class_context.varid = necro_reify_go(parse_ast_arena, ast->type_class_context.varid, arena, intern);
        break;
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        reified_ast->type_class_declaration.context                     = necro_reify_go(parse_ast_arena, ast->type_class_declaration.context, arena, intern);
        reified_ast->type_class_declaration.tycls                       = necro_reify_go(parse_ast_arena, ast->type_class_declaration.tycls, arena, intern);
        reified_ast->type_class_declaration.tyvar                       = necro_reify_go(parse_ast_arena, ast->type_class_declaration.tyvar, arena, intern);
        reified_ast->type_class_declaration.declarations                = necro_reify_go(parse_ast_arena, ast->type_class_declaration.declarations, arena, intern);
        reified_ast->type_class_declaration.declaration_group           = NULL;
        reified_ast->type_class_declaration.ast_symbol                  = NULL;
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        reified_ast->type_class_instance.context             = necro_reify_go(parse_ast_arena, ast->type_class_instance.context, arena, intern);
        reified_ast->type_class_instance.qtycls              = necro_reify_go(parse_ast_arena, ast->type_class_instance.qtycls, arena, intern);
        reified_ast->type_class_instance.inst                = necro_reify_go(parse_ast_arena, ast->type_class_instance.inst, arena, intern);
        reified_ast->type_class_instance.declarations        = necro_reify_go(parse_ast_arena, ast->type_class_instance.declarations, arena, intern);
        reified_ast->type_class_instance.declaration_group   = NULL;
        reified_ast->type_class_instance.ast_symbol          = NULL;
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        reified_ast->type_signature.var               = necro_reify_go(parse_ast_arena, ast->type_signature.var, arena, intern);
        reified_ast->type_signature.context           = necro_reify_go(parse_ast_arena, ast->type_signature.context, arena, intern);
        reified_ast->type_signature.type              = necro_reify_go(parse_ast_arena, ast->type_signature.type, arena, intern);
        reified_ast->type_signature.sig_type          = ast->type_signature.sig_type;
        reified_ast->type_signature.declaration_group = NULL;
        break;
    case NECRO_AST_FUNCTION_TYPE:
        reified_ast->function_type.type          = necro_reify_go(parse_ast_arena, ast->function_type.type, arena, intern);
        reified_ast->function_type.next_on_arrow = necro_reify_go(parse_ast_arena, ast->function_type.next_on_arrow, arena, intern);
        break;
    default:
        assert(false);
        break;
    }
    return reified_ast;
}

NecroAstArena necro_ast_arena_empty()
{
    return (NecroAstArena)
    {
        .arena        = necro_paged_arena_empty(),
        .root         = NULL,
        .clash_suffix = 0,
        .module_name  = NULL,
        .module_names = NULL,
    };
}

NecroAstArena necro_ast_arena_create(NecroSymbol module_name)
{
    NecroPagedArena arena             = necro_paged_arena_create();
    NecroScope*     module_names      = necro_scope_create(&arena, NULL);
    NecroScope*     module_type_names = necro_scope_create(&arena, NULL);
    return (NecroAstArena)
    {
        .arena             = arena,
        .root              = NULL,
        .clash_suffix      = 13000,
        .module_name       = module_name,
        .module_names      = module_names,
        .module_type_names = module_type_names,
    };
}

void necro_ast_arena_destroy(NecroAstArena* ast)
{
    necro_paged_arena_destroy(&ast->arena);
    ast->root         = NULL;
    ast->module_names = 0;
}

//=====================================================
// Manual AST construction
//=====================================================
inline NecroAst* necro_ast_alloc(NecroPagedArena* arena, NECRO_AST_TYPE type)
{
    NecroAst* ast   = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type       = type;
    ast->source_loc = NULL_LOC;
    ast->end_loc    = NULL_LOC;
    ast->necro_type = NULL;
    ast->scope      = NULL;
    return ast;
}

NecroAst* necro_ast_create_conid(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NECRO_CON_TYPE con_type)
{
    NecroAst*   ast        = necro_ast_alloc(arena, NECRO_AST_CONID);
    NecroSymbol con_symbol = necro_intern_string(intern, con_name);
    ast->conid.con_type    = con_type;
    ast->conid.ast_symbol  = necro_ast_symbol_create(arena, con_symbol, con_symbol, NULL, NULL);
    return ast;
}

NecroAst* necro_ast_create_conid_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NECRO_CON_TYPE con_type)
{
    NecroAst* ast         = necro_ast_alloc(arena, NECRO_AST_CONID);
    ast->conid.con_type   = con_type;
    ast->conid.ast_symbol = ast_symbol;
    // ast_symbol->ast       = ast;
    return ast;
}

NecroAst* necro_ast_create_var(NecroPagedArena* arena, NecroIntern* intern, const char* variable_name, NECRO_VAR_TYPE var_type)
{
    NecroAst* ast               = necro_ast_alloc(arena, NECRO_AST_VARIABLE);
    ast->variable.var_type      = var_type;
    ast->variable.inst_context  = NULL;
    ast->variable.inst_subs     = NULL;
    ast->variable.initializer   = NULL;
    ast->variable.is_recursive  = false;
    ast->variable.order         = NECRO_TYPE_ZERO_ORDER;
    NecroSymbol variable_symbol = necro_intern_string(intern, variable_name);
    switch (ast->variable.var_type)
    {
    case NECRO_VAR_TYPE_FREE_VAR:
    case NECRO_VAR_TYPE_VAR_DECLARATION:
    case NECRO_VAR_DECLARATION:
    case NECRO_VAR_CLASS_SIG:
        ast->variable.ast_symbol = necro_ast_symbol_create(arena, variable_symbol, variable_symbol, NULL, ast);
        break;
    case NECRO_VAR_SIG:
    case NECRO_VAR_VAR:
        ast->variable.ast_symbol = necro_ast_symbol_create(arena, variable_symbol, variable_symbol, NULL, NULL);
        break;
    }
    return ast;
}

NecroAst* necro_ast_create_var_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NECRO_VAR_TYPE var_type)
{
    NecroAst* ast              = necro_ast_alloc(arena, NECRO_AST_VARIABLE);
    ast->variable.var_type     = var_type;
    ast->variable.inst_context = NULL;
    ast->variable.inst_subs    = NULL;
    ast->variable.initializer  = NULL;
    ast->variable.is_recursive = false;
    ast->variable.order        = NECRO_TYPE_ZERO_ORDER;
    ast->variable.ast_symbol   = ast_symbol;
    return ast;
}

NecroAst* necro_ast_create_var_full(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NECRO_VAR_TYPE var_type, NecroInstSub* inst_subs, NecroAst* initializer, NECRO_TYPE_ORDER order)
{
    NecroAst* ast                 = necro_ast_alloc(arena, NECRO_AST_VARIABLE);
    ast->variable.var_type        = var_type;
    ast->variable.inst_context    = NULL;
    ast->variable.inst_subs       = inst_subs;
    ast->variable.initializer     = initializer;
    ast->variable.is_recursive    = false;
    ast->variable.order           = order;
    ast->variable.ast_symbol      = ast_symbol;
    return ast;
}

NecroAst* necro_ast_create_list(NecroPagedArena* arena, NecroAst* item, NecroAst* next)
{
    NecroAst* ast       = necro_ast_alloc(arena, NECRO_AST_LIST_NODE);
    ast->list.item      = item;
    ast->list.next_item = next;
    return ast;
}

NecroAst* necro_ast_create_var_list(NecroPagedArena* arena, NecroIntern* intern, size_t num_vars, NECRO_VAR_TYPE var_type)
{
    NecroAst* var_head = NULL;
    NecroAst* curr_var = NULL;
    static char* var_names[27] = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z" };
    size_t name_index = 0;
    assert(num_vars < 27);
    while (num_vars > 0)
    {
        if (var_head == NULL)
        {
            var_head = necro_ast_create_list(arena, necro_ast_create_var(arena, intern, var_names[name_index], var_type), NULL);
            curr_var = var_head;
        }
        else
        {
            curr_var->list.next_item = necro_ast_create_list(arena, necro_ast_create_var(arena, intern, var_names[name_index], var_type), NULL);
            curr_var                 = curr_var->list.next_item;
        }
        name_index++;
        num_vars--;
    }
    return var_head;
}

NecroAst* necro_ast_create_data_con(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NecroAst* arg_list)
{
    NecroAst* ast             = necro_ast_alloc(arena, NECRO_AST_CONSTRUCTOR);
    ast->constructor.conid    = necro_ast_create_conid(arena, intern, con_name, NECRO_CON_DATA_DECLARATION);
    ast->constructor.arg_list = arg_list;
    return ast;
}

NecroAst* necro_ast_create_data_con_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* con_name, NecroAst* arg_list)
{
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_CONSTRUCTOR);
    ast->constructor.conid = necro_ast_create_conid_with_ast_symbol(arena, con_name, NECRO_CON_DATA_DECLARATION);
    ast->constructor.arg_list = arg_list;
    return ast;
}

NecroAst* necro_ast_create_constructor_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* con_name_symbol, NecroAst* arg_list)
{
    NecroAst* ast             = necro_ast_alloc(arena, NECRO_AST_CONSTRUCTOR);
	ast->constructor.conid    = necro_ast_create_conid_with_ast_symbol(arena, con_name_symbol, NECRO_CON_VAR);
	ast->constructor.arg_list = arg_list;
	return ast;
}

NecroAst* necro_ast_create_constructor(NecroPagedArena* arena, NecroAst* conid, NecroAst* arg_list)
{
    NecroAst* ast             = necro_ast_alloc(arena, NECRO_AST_CONSTRUCTOR);
	ast->constructor.conid    = conid;
	ast->constructor.arg_list = arg_list;
	return ast;
}

NecroAst* necro_ast_create_simple_type(NecroPagedArena* arena, NecroIntern* intern, const char* simple_type_name, NecroAst* ty_var_list)
{
    NecroAst* ast                  = necro_ast_alloc(arena, NECRO_AST_SIMPLE_TYPE);
    ast->simple_type.type_con      = necro_ast_create_conid(arena, intern, simple_type_name, NECRO_CON_TYPE_DECLARATION);
    ast->simple_type.type_var_list = ty_var_list;
    return ast;
}

NecroAst* necro_ast_create_simple_type_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* simple_type_name, NecroAst* ty_var_list)
{
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_SIMPLE_TYPE);
    ast->simple_type.type_con = necro_ast_create_conid_with_ast_symbol(arena, simple_type_name, NECRO_CON_TYPE_DECLARATION);
    ast->simple_type.type_var_list = ty_var_list;
    return ast;
}

NecroAst* necro_ast_create_simple_type_with_type_con(NecroPagedArena* arena, NecroAst* type_con, NecroAst* ty_var_list)
{
    NecroAst* ast                  = necro_ast_alloc(arena, NECRO_AST_SIMPLE_TYPE);
    ast->simple_type.type_con      = type_con;
    ast->simple_type.type_var_list = ty_var_list;
    return ast;
}

NecroAst* necro_ast_create_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroAst* simple_type, NecroAst* constructor_list)
{
    UNUSED(intern);
    NecroAst* ast                           = necro_ast_alloc(arena, NECRO_AST_DATA_DECLARATION);
    ast->data_declaration.simpletype        = simple_type;
    ast->data_declaration.constructor_list  = constructor_list;
    ast->data_declaration.ast_symbol        = NULL;
    ast->data_declaration.declaration_group = NULL;
    ast->data_declaration.is_recursive      = false;
    return ast;
}

NecroAst* necro_ast_create_data_declaration_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* simple_type, NecroAst* constructor_list)
{
    NecroAst* ast                           = necro_ast_alloc(arena, NECRO_AST_DATA_DECLARATION);
    ast->data_declaration.ast_symbol        = ast_symbol;
    ast->data_declaration.simpletype        = simple_type;
    ast->data_declaration.constructor_list  = constructor_list;
    ast->data_declaration.declaration_group = NULL;
    ast->data_declaration.is_recursive      = false;
    return ast;
}

NecroAst* necro_ast_create_type_app(NecroPagedArena* arena, NecroAst* type1, NecroAst* type2)
{
    NecroAst* ast         = necro_ast_alloc(arena, NECRO_AST_TYPE_APP);
    ast->type_app.ty      = type1;
    ast->type_app.next_ty = type2;
    return ast;
}

NecroAst* necro_ast_create_type_fn(NecroPagedArena* arena, NecroAst* type1, NecroAst* type2)
{
    NecroAst* ast                    = necro_ast_alloc(arena, NECRO_AST_FUNCTION_TYPE);
    ast->function_type.type          = type1;
    ast->function_type.next_on_arrow = type2;
    return ast;
}

NecroAst* necro_ast_create_fexpr(NecroPagedArena* arena, NecroAst* f_ast, NecroAst* x_ast)
{
    NecroAst* ast                     = necro_ast_alloc(arena, NECRO_AST_FUNCTION_EXPRESSION);
    ast->fexpression.aexp             = f_ast;
    ast->fexpression.next_fexpression = x_ast;
    return ast;
}

NecroAst* necro_ast_create_fn_type_sig(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* context_ast, NecroAst* type_ast, NECRO_VAR_TYPE var_type, NECRO_SIG_TYPE sig_type)
{
    NecroAst* ast                         = necro_ast_alloc(arena, NECRO_AST_TYPE_SIGNATURE);
    ast->type_signature.var               = necro_ast_create_var(arena, intern, var_name, var_type);
    ast->type_signature.context           = context_ast;
    ast->type_signature.type              = type_ast;
    ast->type_signature.sig_type          = sig_type;
    ast->type_signature.declaration_group = NULL;
    return ast;
}

NecroAst* necro_ast_create_type_class(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* class_var, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                                           = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_DECLARATION);
    ast->type_class_declaration.tycls                       = necro_ast_create_conid(arena, intern, class_name, NECRO_CON_TYPE_DECLARATION);
    ast->type_class_declaration.tyvar                       = necro_ast_create_var(arena, intern, class_var, NECRO_VAR_TYPE_FREE_VAR);
    ast->type_class_declaration.context                     = context_ast;
    ast->type_class_declaration.declarations                = declarations_ast;
    ast->type_class_declaration.declaration_group           = NULL;
    ast->type_class_declaration.ast_symbol                  = NULL;
    return ast;
}

NecroAst* necro_ast_create_type_class_with_ast_symbols(NecroPagedArena* arena, NecroAstSymbol* class_name, NecroAstSymbol* class_var, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_DECLARATION);
    ast->type_class_declaration.tycls = necro_ast_create_conid_with_ast_symbol(arena, class_name, NECRO_CON_TYPE_DECLARATION);
    ast->type_class_declaration.tyvar = necro_ast_create_var_with_ast_symbol(arena, class_var, NECRO_VAR_TYPE_FREE_VAR);
    ast->type_class_declaration.context = context_ast;
    ast->type_class_declaration.declarations = declarations_ast;
    ast->type_class_declaration.declaration_group = NULL;
    ast->type_class_declaration.ast_symbol = NULL;
    return ast;
}

NecroAst* necro_ast_create_type_class_full(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* tycls, NecroAst* tyvar, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                                           = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_DECLARATION);
    ast->type_class_declaration.tycls                       = tycls;
    ast->type_class_declaration.tyvar                       = tyvar;
    ast->type_class_declaration.context                     = context_ast;
    ast->type_class_declaration.declarations                = declarations_ast;
    ast->type_class_declaration.declaration_group           = NULL;
    ast->type_class_declaration.ast_symbol                  = ast_symbol;
    return ast;
}

NecroAst* necro_ast_create_instance(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, NecroAst* inst_ast, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                                 = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_INSTANCE);
    ast->type_class_instance.qtycls               = necro_ast_create_conid(arena, intern, class_name, NECRO_CON_TYPE_VAR);
    ast->type_class_instance.inst                 = inst_ast;
    ast->type_class_instance.context              = context_ast;
    ast->type_class_instance.declarations         = declarations_ast;
    ast->type_class_declaration.declaration_group = NULL;
    ast->type_class_instance.ast_symbol           = NULL;
    return ast;
}

NecroAst* necro_ast_create_instance_with_symbol(NecroPagedArena* arena, NecroAstSymbol* class_name, NecroAst* inst_ast, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_INSTANCE);
    ast->type_class_instance.qtycls = necro_ast_create_conid_with_ast_symbol(arena, class_name, NECRO_CON_TYPE_VAR);
    ast->type_class_instance.inst = inst_ast;
    ast->type_class_instance.context = context_ast;
    ast->type_class_instance.declarations = declarations_ast;
    ast->type_class_declaration.declaration_group = NULL;
    ast->type_class_instance.ast_symbol = NULL;
    return ast;
}

NecroAst* necro_ast_create_instance_full(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* qtycls, NecroAst* inst_ast, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                                 = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_INSTANCE);
    ast->type_class_instance.ast_symbol           = ast_symbol;
    ast->type_class_instance.qtycls               = qtycls;
    ast->type_class_instance.inst                 = inst_ast;
    ast->type_class_instance.context              = context_ast;
    ast->type_class_instance.declarations         = declarations_ast;
    ast->type_class_declaration.declaration_group = NULL;
    return ast;
}

NecroAst* necro_ast_create_top_decl(NecroPagedArena* arena, NecroAst* top_level_declaration, NecroAst* next)
{
    NecroAst* ast                      = necro_ast_alloc(arena, NECRO_AST_TOP_DECL);
    ast->top_declaration.declaration   = top_level_declaration;
    ast->top_declaration.next_top_decl = next;
    return ast;
}

NecroAst* necro_ast_create_decl(NecroPagedArena* arena, NecroAst* declaration, NecroAst* next)
{
    // assert(declaration != NULL);
    NecroAst* ast                           = necro_ast_alloc(arena, NECRO_AST_DECL);
    ast->declaration.declaration_impl       = declaration;
    ast->declaration.next_declaration       = next;
    ast->declaration.declaration_group_list = NULL;
    ast->declaration.info                   = NULL;
    ast->declaration.index                  = -1;
    ast->declaration.low_link               = 0;
    ast->declaration.on_stack               = false;
    ast->declaration.type_checked           = false;
    return ast;
}

NecroAst* necro_ast_create_simple_assignment(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* rhs_ast)
{
    assert(rhs_ast != NULL);
    NecroSymbol variable_symbol                    = necro_intern_string(intern, var_name);
    NecroAst*   ast                                = necro_ast_alloc(arena, NECRO_AST_SIMPLE_ASSIGNMENT);
    ast->simple_assignment.rhs                     = rhs_ast;
    ast->simple_assignment.initializer             = NULL;
    ast->simple_assignment.declaration_group       = NULL;
    ast->simple_assignment.is_recursive            = false;
    ast->simple_assignment.ast_symbol              = necro_ast_symbol_create(arena, variable_symbol, variable_symbol, NULL, ast);
    ast->simple_assignment.optional_type_signature = NULL;
    return ast;
}

NecroAst* necro_ast_create_simple_assignment_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* initializer, NecroAst* rhs_ast)
{
    assert(rhs_ast != NULL);
    NecroAst* ast                                  = necro_ast_alloc(arena, NECRO_AST_SIMPLE_ASSIGNMENT);
    ast->simple_assignment.rhs                     = rhs_ast;
    ast->simple_assignment.initializer             = initializer;
    ast->simple_assignment.declaration_group       = NULL;
    ast->simple_assignment.is_recursive            = false;
    ast->simple_assignment.ast_symbol              = ast_symbol;
    ast->simple_assignment.optional_type_signature = NULL;
    // ast_symbol->ast                                = ast;
    return ast;
}

NecroAst* necro_ast_create_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* var_name, NecroAst* next)
{
    assert(class_name != NULL);
    assert(var_name != NULL);
    NecroAst* ast                 = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_CONTEXT);
    ast->type_class_context.conid = necro_ast_create_conid(arena, intern, class_name, NECRO_CON_TYPE_VAR);
    ast->type_class_context.varid = necro_ast_create_var(arena, intern, var_name, NECRO_VAR_TYPE_FREE_VAR);

    NecroAst* list_ast       = necro_ast_alloc(arena, NECRO_AST_LIST_NODE);
    list_ast->list.item      = ast;
    list_ast->list.next_item = next;
    return list_ast;
}

NecroAst* necro_ast_create_context_with_ast_symbols(NecroPagedArena* arena, NecroAstSymbol* class_name, NecroAstSymbol* var_name)
{
    assert(class_name != NULL);
    assert(var_name != NULL);
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_CONTEXT);
    ast->type_class_context.conid = necro_ast_create_conid_with_ast_symbol(arena, class_name, NECRO_CON_TYPE_VAR);
    ast->type_class_context.varid = necro_ast_create_var_with_ast_symbol(arena, var_name, NECRO_VAR_TYPE_FREE_VAR);
    return ast;
}

NecroAst* necro_ast_create_context_full(NecroPagedArena* arena, NecroAst* conid, NecroAst* varid)
{
    NecroAst* ast                 = necro_ast_alloc(arena, NECRO_AST_TYPE_CLASS_CONTEXT);
    ast->type_class_context.conid = conid;
    ast->type_class_context.varid = varid;
    return ast;
}

NecroAst* necro_ast_create_apats(NecroPagedArena* arena, NecroAst* apat_item, NecroAst* next_apat)
{
    assert(apat_item != NULL);
    NecroAst* ast        = necro_ast_alloc(arena, NECRO_AST_APATS);
    ast->apats.apat      = apat_item;
    ast->apats.next_apat = next_apat;
    return ast;
}

NecroAst* necro_ast_create_apats_assignment(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* apats, NecroAst* rhs_ast)
{
    assert(apats != NULL);
    assert(rhs_ast != NULL);
    assert(apats->type == NECRO_AST_APATS);
    assert(rhs_ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    NecroSymbol variable_symbol                   = necro_intern_string(intern, var_name);
    NecroAst* ast                                 = necro_ast_alloc(arena, NECRO_AST_APATS_ASSIGNMENT);
    ast->apats_assignment.apats                   = apats;
    ast->apats_assignment.rhs                     = rhs_ast;
    ast->apats_assignment.declaration_group       = NULL;
    ast->apats_assignment.is_recursive            = false;
    ast->apats_assignment.ast_symbol              = necro_ast_symbol_create(arena, variable_symbol, variable_symbol, NULL, ast);
    ast->apats_assignment.optional_type_signature = NULL;
    return ast;
}

NecroAst* necro_ast_create_apats_assignment_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* apats, NecroAst* rhs_ast)
{
	assert(apats != NULL);
	assert(rhs_ast != NULL);
	assert(apats->type == NECRO_AST_APATS);
	assert(rhs_ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    NecroAst* ast                                 = necro_ast_alloc(arena, NECRO_AST_APATS_ASSIGNMENT);
	ast->apats_assignment.apats                   = apats;
	ast->apats_assignment.rhs                     = rhs_ast;
	ast->apats_assignment.declaration_group       = NULL;
    ast->apats_assignment.is_recursive            = false;
	ast->apats_assignment.ast_symbol              = ast_symbol;
	ast->apats_assignment.optional_type_signature = NULL;
	return ast;
}

NecroAst* necro_ast_create_lambda(NecroPagedArena* arena, NecroAst* apats, NecroAst* expr_ast)
{
    assert(apats != NULL);
    assert(expr_ast != NULL);
    assert(apats->type == NECRO_AST_APATS);
    NecroAst* ast          = necro_ast_alloc(arena, NECRO_AST_LAMBDA);
    ast->lambda.apats      = apats;
    ast->lambda.expression = expr_ast;
    return ast;
}

NecroAst* necro_ast_create_rhs(NecroPagedArena* arena, NecroAst* expression, NecroAst* declarations)
{
    assert(expression != NULL);
    NecroAst* ast                     = necro_ast_alloc(arena, NECRO_AST_RIGHT_HAND_SIDE);
    ast->right_hand_side.expression   = expression;
    ast->right_hand_side.declarations = declarations;
    return ast;
}

NecroAst* necro_ast_create_wildcard(NecroPagedArena* arena)
{
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_WILDCARD);
    return ast;
}

NecroAst* necro_ast_create_bin_op(NecroPagedArena* arena, NecroIntern* intern, const char* op_name, NecroAst* lhs, NecroAst* rhs)
{
    assert(op_name != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    NecroSymbol op_symbol    = necro_intern_string(intern, op_name);
    NecroAst* ast            = necro_ast_alloc(arena, NECRO_AST_BIN_OP);
    ast->bin_op.ast_symbol   = necro_ast_symbol_create(arena, op_symbol, op_symbol, NULL, ast);
    ast->bin_op.lhs          = lhs;
    ast->bin_op.rhs          = rhs;
    ast->bin_op.inst_context = NULL;
    ast->bin_op.inst_subs    = NULL;
    return ast;
}

NecroAst* necro_ast_create_bin_op_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* lhs, NecroAst* rhs)
{
    assert(ast_symbol != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    NecroAst* ast            = necro_ast_alloc(arena, NECRO_AST_BIN_OP);
    ast->bin_op.ast_symbol   = ast_symbol;
    ast->bin_op.lhs          = lhs;
    ast->bin_op.rhs          = rhs;
    ast->bin_op.inst_context = NULL;
    ast->bin_op.inst_subs    = NULL;
    return ast;
}

NecroAst* necro_ast_create_bin_op_full(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* lhs, NecroAst* rhs, NecroInstSub* inst_subs)
{
    NecroAst* ast = necro_ast_create_bin_op_with_ast_symbol(arena, ast_symbol, lhs, rhs);
    ast->bin_op.inst_subs = inst_subs;
    return ast;
}

NecroAst* necro_ast_create_bin_op_sym_with_ast_symbol(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* lhs, NecroAst* rhs)
{
    assert(ast_symbol != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    NecroAst* ast = necro_ast_alloc(arena, NECRO_AST_BIN_OP_SYM);
    ast->bin_op_sym.op = necro_ast_create_conid_with_ast_symbol(arena, ast_symbol, NECRO_CON_VAR);
    ast->bin_op_sym.left = lhs;
    ast->bin_op_sym.right = rhs;
    return ast;
}

NecroAst* necro_ast_create_left_section(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* lhs)
{
    assert(ast_symbol != NULL);
    assert(lhs != NULL);
    NecroAst* ast                      = necro_ast_alloc(arena, NECRO_AST_OP_LEFT_SECTION);
    ast->op_left_section.ast_symbol    = ast_symbol;
    ast->op_left_section.left          = lhs;
    ast->op_left_section.inst_context  = NULL;
    ast->op_left_section.inst_subs     = NULL;
    ast->op_left_section.op_necro_type = NULL;
    return ast;
}

NecroAst* necro_ast_create_left_section_full(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* lhs, NecroInstSub* inst_subs)
{
    NecroAst* ast                  = necro_ast_create_left_section(arena, ast_symbol, lhs);
    ast->op_left_section.inst_subs = inst_subs;
    return ast;
}

NecroAst* necro_ast_create_right_section(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* rhs)
{
    assert(ast_symbol != NULL);
    assert(rhs != NULL);
    NecroAst* ast                       = necro_ast_alloc(arena, NECRO_AST_OP_RIGHT_SECTION);
    ast->op_right_section.ast_symbol    = ast_symbol;
    ast->op_right_section.right         = rhs;
    ast->op_right_section.inst_context  = NULL;
    ast->op_right_section.inst_subs     = NULL;
    ast->op_right_section.op_necro_type = NULL;
    return ast;
}

NecroAst* necro_ast_create_right_section_full(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* rhs, NecroInstSub* inst_subs)
{
    NecroAst* ast                   = necro_ast_create_right_section(arena, ast_symbol, rhs);
    ast->op_right_section.inst_subs = inst_subs;
    return ast;
}

NecroAst* necro_ast_create_type_signature(NecroPagedArena* arena, NECRO_SIG_TYPE sig_type, NecroAst* var, NecroAst* context, NecroAst* type)
{
    NecroAst* ast                         = necro_ast_alloc(arena, NECRO_AST_TYPE_SIGNATURE);
    ast->type_signature.sig_type          = sig_type;
    ast->type_signature.var               = var;
    ast->type_signature.context           = context;
    ast->type_signature.type              = type;
    ast->type_signature.declaration_group = NULL;
    return ast;
}

NecroAst* necro_ast_create_constant(NecroPagedArena* arena, NecroParseAstConstant constant)
{
    NecroAst* ast              = necro_ast_alloc(arena, NECRO_AST_CONSTANT);
    ast->constant.pat_eq_ast   = NULL;
    ast->constant.pat_from_ast = NULL;
    ast->constant.type         = constant.type;
    switch (constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT:
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
        ast->constant.double_literal = constant.double_literal;
        break;

    case NECRO_AST_CONSTANT_INTEGER:
    case NECRO_AST_CONSTANT_TYPE_INT:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
        ast->constant.int_literal = constant.int_literal;
        break;

    case NECRO_AST_CONSTANT_CHAR:
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
        ast->constant.char_literal = constant.char_literal;
        break;

    case NECRO_AST_CONSTANT_STRING:
        ast->constant.symbol = constant.symbol;
        break;
    }
    return ast;
}

NecroAst* necro_ast_create_let(NecroPagedArena* arena, NecroAst* expression_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                    = necro_ast_alloc(arena, NECRO_AST_LET_EXPRESSION);
    ast->let_expression.expression   = expression_ast;
    ast->let_expression.declarations = declarations_ast;
    return ast;
}

NecroAst* necro_ast_create_do(NecroPagedArena* arena, NecroAst* statement_list_ast)
{
    NecroAst* ast                    = necro_ast_alloc(arena, NECRO_AST_DO);
    ast->do_statement.monad_var      = NULL;
    ast->do_statement.statement_list = statement_list_ast;
    return ast;
}

NecroAst* necro_ast_create_bind_assignment(NecroPagedArena* arena, NecroAstSymbol* ast_symbol, NecroAst* expr)
{
    NecroAst* ast                   = necro_ast_alloc(arena, NECRO_BIND_ASSIGNMENT);
    ast->bind_assignment.ast_symbol = ast_symbol;
    ast->bind_assignment.expression = expr;
    return ast;
}

NecroAst* necro_ast_create_if_then_else(NecroPagedArena* arena, NecroAst* if_ast, NecroAst* then_ast, NecroAst* else_ast)
{
    NecroAst* ast               = necro_ast_alloc(arena, NECRO_AST_IF_THEN_ELSE);
    ast->if_then_else.if_expr   = if_ast;
    ast->if_then_else.then_expr = then_ast;
    ast->if_then_else.else_expr = else_ast;
    return ast;
}

NecroAst* necro_ast_create_pat_assignment(NecroPagedArena* arena, NecroAst* pat, NecroAst* rhs)
{
    NecroAst* ast                                = necro_ast_alloc(arena, NECRO_AST_PAT_ASSIGNMENT);
    ast->pat_assignment.pat                      = pat;
    ast->pat_assignment.rhs                      = rhs;
    ast->pat_assignment.declaration_group        = NULL;
    ast->pat_assignment.optional_type_signatures = NULL;
    return ast;
}

NecroAst* necro_ast_create_pat_bind_assignment(NecroPagedArena* arena, NecroAst* pat, NecroAst* expression)
{
    NecroAst* ast                       = necro_ast_alloc(arena, NECRO_PAT_BIND_ASSIGNMENT);
    ast->pat_bind_assignment.pat        = pat;
    ast->pat_bind_assignment.expression = expression;
    return ast;
}

NecroAst* necro_ast_create_expression_list(NecroPagedArena* arena, NecroAst* expressions)
{
    NecroAst* ast                    = necro_ast_alloc(arena, NECRO_AST_EXPRESSION_LIST);
    ast->expression_list.expressions = expressions;
    return ast;
}

NecroAst* necro_ast_create_expression_array(NecroPagedArena* arena, NecroAst* expressions)
{
    NecroAst* ast                     = necro_ast_alloc(arena, NECRO_AST_EXPRESSION_ARRAY);
    ast->expression_array.expressions = expressions;
    return ast;
}

NecroAst* necro_ast_create_pat_expression(NecroPagedArena* arena, NecroAst* expressions)
{
    NecroAst* ast                       = necro_ast_alloc(arena, NECRO_AST_PAT_EXPRESSION);
    ast->pattern_expression.expressions = expressions;
    return ast;
}

NecroAst* necro_ast_create_tuple(NecroPagedArena* arena, NecroAst* expressions)
{
    NecroAst* ast          = necro_ast_alloc(arena, NECRO_AST_TUPLE);
    ast->tuple.expressions = expressions;
    return ast;
}

NecroAst* necro_ast_create_arithmetic_sequence(NecroPagedArena* arena, NECRO_ARITHMETIC_SEQUENCE_TYPE sequence_type, NecroAst* from, NecroAst* then, NecroAst* to)
{
    NecroAst* ast                 = necro_ast_alloc(arena, NECRO_AST_ARITHMETIC_SEQUENCE);
    ast->arithmetic_sequence.type = sequence_type;
    ast->arithmetic_sequence.from = from;
    ast->arithmetic_sequence.then = then;
    ast->arithmetic_sequence.to   = to;
    return ast;
}

NecroAst* necro_ast_create_case(NecroPagedArena* arena, NecroAst* alternatives, NecroAst* expression)
{
    NecroAst* ast                     = necro_ast_alloc(arena, NECRO_AST_CASE);
    ast->case_expression.alternatives = alternatives;
    ast->case_expression.expression   = expression;
    return ast;
}

NecroAst* necro_ast_create_case_alternative(NecroPagedArena* arena, NecroAst* pat, NecroAst* body)
{
    NecroAst* ast              = necro_ast_alloc(arena, NECRO_AST_CASE_ALTERNATIVE);
    ast->case_alternative.pat  = pat;
    ast->case_alternative.body = body;
    return ast;
}

NecroAst* necro_ast_create_declaration_group_list(NecroPagedArena* arena, NecroAst* declaration_group, NecroAst* prev)
{
    if (declaration_group != NULL)
        assert(declaration_group->type == NECRO_AST_DECL);
    NecroAst* ast                                 = necro_ast_alloc(arena, NECRO_AST_DECLARATION_GROUP_LIST);
    ast->declaration_group_list.declaration_group = declaration_group;
    ast->declaration_group_list.next              = NULL;
    if (prev == NULL)
    {
        return ast;
    }
    else
    {
        prev->declaration_group_list.next = ast;
        return prev;
    }
}

NecroAst* necro_ast_create_declaration_group_list_with_next(NecroPagedArena* arena, NecroAst* declaration_group, NecroAst* next)
{
    assert(declaration_group != NULL);
    assert(declaration_group->type == NECRO_AST_DECL);
    NecroAst* ast                                 = necro_ast_alloc(arena, NECRO_AST_DECLARATION_GROUP_LIST);
    ast->declaration_group_list.declaration_group = declaration_group;
    ast->declaration_group_list.next              = next;
    return ast;
}

NecroAst* necro_ast_declaration_group_append(NecroPagedArena* arena, NecroAst* declaration_ast, NecroAst* declaration_group_head)
{
    NecroAst* ast = necro_ast_create_decl(arena, declaration_ast, NULL);
    if (declaration_group_head == NULL)
        return ast;
    assert(declaration_group_head->type == NECRO_AST_DECL);
    NecroAst* curr = declaration_group_head;
    while (curr->declaration.next_declaration != NULL)
        curr = curr->declaration.next_declaration;
    curr->declaration.next_declaration = ast;
    return declaration_group_head;
}

void necro_ast_declaration_group_prepend_to_group_in_group_list(NecroAst* group_list, NecroAst* group_to_prepend)
{
    assert(group_list->type == NECRO_AST_DECLARATION_GROUP_LIST);
    assert(group_to_prepend->type == NECRO_AST_DECL);
    assert(group_to_prepend != NULL);
    NecroAst* curr = group_to_prepend;
    while (curr->declaration.next_declaration != NULL)
        curr = curr->declaration.next_declaration;
    curr->declaration.next_declaration = group_list->declaration_group_list.declaration_group;
    group_list->declaration_group_list.declaration_group = curr;
}

NecroAst* necro_ast_declaration_group_list_append(NecroPagedArena* arena, NecroAst* declaration_group, NecroAst* declaration_group_list_head)
{
    if (declaration_group != NULL)
        assert(declaration_group->type == NECRO_AST_DECL);
    NecroAst* declaration_group_list = necro_ast_create_declaration_group_list(arena, declaration_group, NULL);
    if (declaration_group_list_head == NULL)
        return declaration_group_list;
    assert(declaration_group_list_head->type == NECRO_AST_DECLARATION_GROUP_LIST);
    NecroAst* curr = declaration_group_list_head;
    while (curr->declaration_group_list.next != NULL)
        curr = curr->declaration_group_list.next;
    curr->declaration_group_list.next = declaration_group_list;
    return declaration_group_list_head;
}

///////////////////////////////////////////////////////
// Assert Eq
///////////////////////////////////////////////////////
void necro_ast_assert_eq_go(NecroAst* ast1, NecroAst* ast2);

void necro_ast_assert_ast_symbol_name_eq(NecroAstSymbol* ast_symbol1, NecroAstSymbol* ast_symbol2)
{
    assert(ast_symbol1 != NULL);
    assert(ast_symbol2 != NULL);
		assert(strcmp(ast_symbol1->source_name->str, ast_symbol2->source_name->str) == 0);
		assert(strcmp(ast_symbol1->name->str, ast_symbol2->name->str) == 0);
		assert(strcmp(ast_symbol1->module_name->str, ast_symbol2->module_name->str) == 0);
}

void necro_ast_assert_eq_constant(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_CONSTANT);
    assert(ast2->type == NECRO_AST_CONSTANT);
    assert(ast1->constant.type == ast2->constant.type);
    switch (ast1->constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
    case NECRO_AST_CONSTANT_FLOAT:
        assert(ast1->constant.double_literal == ast2->constant.double_literal);
        break;
    case NECRO_AST_CONSTANT_TYPE_INT:
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
    case NECRO_AST_CONSTANT_INTEGER:
        assert(ast1->constant.int_literal == ast2->constant.int_literal);
        break;
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
    case NECRO_AST_CONSTANT_CHAR:
        assert(ast1->constant.char_literal == ast2->constant.char_literal);
        break;
    case NECRO_AST_CONSTANT_STRING:
    {
        const char* str1 = ast1->constant.symbol->str;
        const char* str2 = ast2->constant.symbol->str;
        assert(strcmp(str1, str2) == 0);
        break;
    }
    }
    UNUSED(ast1);
    UNUSED(ast2);
}

void necro_ast_assert_eq_bin_op(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_BIN_OP);
    assert(ast2->type == NECRO_AST_BIN_OP);
    assert(ast1->bin_op.type == ast2->bin_op.type);
    necro_ast_assert_ast_symbol_name_eq(ast1->bin_op.ast_symbol, ast2->bin_op.ast_symbol);
    necro_ast_assert_eq_go(ast1->bin_op.lhs, ast2->bin_op.lhs);
    necro_ast_assert_eq_go(ast1->bin_op.rhs, ast2->bin_op.rhs);
}

void necro_ast_assert_eq_if_then_else(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_IF_THEN_ELSE);
    assert(ast2->type == NECRO_AST_IF_THEN_ELSE);
    necro_ast_assert_eq_go(ast1->if_then_else.if_expr, ast2->if_then_else.if_expr);
    necro_ast_assert_eq_go(ast1->if_then_else.then_expr, ast2->if_then_else.then_expr);
    necro_ast_assert_eq_go(ast1->if_then_else.else_expr, ast2->if_then_else.else_expr);
}

void necro_ast_assert_eq_top_decl(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TOP_DECL);
    assert(ast2->type == NECRO_AST_TOP_DECL);
    necro_ast_assert_eq_go(ast1->top_declaration.declaration, ast2->top_declaration.declaration);
    necro_ast_assert_eq_go(ast1->top_declaration.next_top_decl, ast2->top_declaration.next_top_decl);
}

void necro_ast_assert_eq_decl(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_DECL);
    assert(ast2->type == NECRO_AST_DECL);
    necro_ast_assert_eq_go(ast1->declaration.declaration_impl, ast2->declaration.declaration_impl);
    necro_ast_assert_eq_go(ast1->declaration.next_declaration, ast2->declaration.next_declaration);
}

void necro_ast_assert_eq_declaration_group_list(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_DECLARATION_GROUP_LIST);
    assert(ast2->type == NECRO_AST_DECLARATION_GROUP_LIST);
    necro_ast_assert_eq_go(ast1->declaration_group_list.declaration_group, ast2->declaration_group_list.declaration_group);
    necro_ast_assert_eq_go(ast1->declaration_group_list.next, ast2->declaration_group_list.next);
}

void necro_ast_assert_eq_simple_assignment(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    assert(ast2->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    necro_ast_assert_ast_symbol_name_eq(ast1->simple_assignment.ast_symbol, ast2->simple_assignment.ast_symbol);
    necro_ast_assert_eq_go(ast1->simple_assignment.initializer, ast2->simple_assignment.initializer);
    necro_ast_assert_eq_go(ast1->simple_assignment.rhs, ast2->simple_assignment.rhs);
}

void necro_ast_assert_eq_apats_assignment(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_APATS_ASSIGNMENT);
    assert(ast2->type == NECRO_AST_APATS_ASSIGNMENT);
    necro_ast_assert_ast_symbol_name_eq(ast1->apats_assignment.ast_symbol, ast2->apats_assignment.ast_symbol);
    necro_ast_assert_eq_go(ast1->apats_assignment.apats, ast2->apats_assignment.apats);
    necro_ast_assert_eq_go(ast1->apats_assignment.rhs, ast2->apats_assignment.rhs);
}

void necro_ast_assert_eq_pat_assignment(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_PAT_ASSIGNMENT);
    assert(ast2->type == NECRO_AST_PAT_ASSIGNMENT);
    necro_ast_assert_eq_go(ast1->pat_assignment.pat, ast2->pat_assignment.pat);
    necro_ast_assert_eq_go(ast1->pat_assignment.rhs, ast2->pat_assignment.rhs);
}

void necro_ast_assert_eq_rhs(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_RIGHT_HAND_SIDE);
    assert(ast2->type == NECRO_AST_RIGHT_HAND_SIDE);
    necro_ast_assert_eq_go(ast1->right_hand_side.expression, ast2->right_hand_side.expression);
    necro_ast_assert_eq_go(ast1->right_hand_side.declarations, ast2->right_hand_side.declarations);
}

void necro_ast_assert_eq_let(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_LET_EXPRESSION);
    assert(ast2->type == NECRO_AST_LET_EXPRESSION);
    necro_ast_assert_eq_go(ast1->let_expression.expression, ast2->let_expression.expression);
    necro_ast_assert_eq_go(ast1->let_expression.declarations, ast2->let_expression.declarations);
}

void necro_ast_assert_eq_fexpr(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_FUNCTION_EXPRESSION);
    assert(ast2->type == NECRO_AST_FUNCTION_EXPRESSION);
    necro_ast_assert_eq_go(ast1->fexpression.aexp, ast2->fexpression.aexp);
    necro_ast_assert_eq_go(ast1->fexpression.next_fexpression, ast2->fexpression.next_fexpression);
}

void necro_ast_assert_eq_var(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_VARIABLE);
    assert(ast2->type == NECRO_AST_VARIABLE);
    necro_ast_assert_ast_symbol_name_eq(ast1->variable.ast_symbol, ast2->variable.ast_symbol);
    necro_ast_assert_eq_go(ast1->variable.initializer, ast2->variable.initializer);
    assert(ast2->variable.var_type == ast2->variable.var_type);
}

void necro_ast_assert_eq_apats(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_APATS);
    assert(ast2->type == NECRO_AST_APATS);
    necro_ast_assert_eq_go(ast1->apats.apat, ast2->apats.apat);
    necro_ast_assert_eq_go(ast1->apats.next_apat, ast2->apats.next_apat);
}

void necro_ast_assert_eq_lambda(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_LAMBDA);
    assert(ast2->type == NECRO_AST_LAMBDA);
    necro_ast_assert_eq_go(ast1->lambda.apats, ast2->lambda.apats);
    necro_ast_assert_eq_go(ast1->lambda.expression, ast2->lambda.expression);
}

void necro_ast_assert_eq_do(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_DO);
    assert(ast2->type == NECRO_AST_DO);
    necro_ast_assert_eq_go(ast1->do_statement.statement_list, ast2->do_statement.statement_list);
}

void necro_ast_assert_eq_pat_expression(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_PAT_EXPRESSION);
    assert(ast2->type == NECRO_AST_PAT_EXPRESSION);
    necro_ast_assert_eq_go(ast1->pattern_expression.expressions, ast2->pattern_expression.expressions);
}

void necro_ast_assert_eq_list(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_LIST_NODE);
    assert(ast2->type == NECRO_AST_LIST_NODE);
    necro_ast_assert_eq_go(ast1->list.item, ast2->list.item);
    necro_ast_assert_eq_go(ast1->list.next_item, ast2->list.next_item);
}

void necro_ast_assert_eq_expression_list(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_EXPRESSION_LIST);
    assert(ast2->type == NECRO_AST_EXPRESSION_LIST);
    necro_ast_assert_eq_go(ast1->expression_list.expressions, ast2->expression_list.expressions);
}

void necro_ast_assert_eq_expression_array(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_EXPRESSION_ARRAY);
    assert(ast2->type == NECRO_AST_EXPRESSION_ARRAY);
    necro_ast_assert_eq_go(ast1->expression_array.expressions, ast2->expression_array.expressions);
}

void necro_ast_assert_eq_tuple(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TUPLE);
    assert(ast2->type == NECRO_AST_TUPLE);
    necro_ast_assert_eq_go(ast1->tuple.expressions, ast2->tuple.expressions);
}

void necro_ast_assert_eq_bind_assignment(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_BIND_ASSIGNMENT);
    assert(ast2->type == NECRO_BIND_ASSIGNMENT);
    necro_ast_assert_ast_symbol_name_eq(ast1->bind_assignment.ast_symbol, ast2->bind_assignment.ast_symbol);
    necro_ast_assert_eq_go(ast1->bind_assignment.expression, ast2->bind_assignment.expression);
}

void necro_ast_assert_eq_pat_bind_assignment(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_PAT_BIND_ASSIGNMENT);
    assert(ast2->type == NECRO_PAT_BIND_ASSIGNMENT);
    necro_ast_assert_eq_go(ast1->pat_bind_assignment.pat, ast2->pat_bind_assignment.pat);
    necro_ast_assert_eq_go(ast1->pat_bind_assignment.expression, ast2->pat_bind_assignment.expression);
}

void necro_ast_assert_eq_arithmetic_sequence(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    assert(ast2->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    necro_ast_assert_eq_go(ast1->arithmetic_sequence.from, ast2->arithmetic_sequence.from);
    necro_ast_assert_eq_go(ast1->arithmetic_sequence.then, ast2->arithmetic_sequence.then);
    necro_ast_assert_eq_go(ast1->arithmetic_sequence.to, ast2->arithmetic_sequence.to);
}

void necro_ast_assert_eq_case(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_CASE);
    assert(ast2->type == NECRO_AST_CASE);
    necro_ast_assert_eq_go(ast1->case_expression.expression, ast2->case_expression.expression);
    necro_ast_assert_eq_go(ast1->case_expression.alternatives, ast2->case_expression.alternatives);
}

void necro_ast_assert_eq_case_alternative(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_CASE_ALTERNATIVE);
    assert(ast2->type == NECRO_AST_CASE_ALTERNATIVE);
    necro_ast_assert_eq_go(ast1->case_alternative.pat, ast2->case_alternative.pat);
    necro_ast_assert_eq_go(ast1->case_alternative.body, ast2->case_alternative.body);
}

void necro_ast_assert_eq_conid(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_CONID);
    assert(ast2->type == NECRO_AST_CONID);
    necro_ast_assert_ast_symbol_name_eq(ast1->conid.ast_symbol, ast2->conid.ast_symbol);
    assert(ast1->conid.con_type == ast2->conid.con_type);
}

void necro_ast_assert_eq_type_app(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TYPE_APP);
    assert(ast2->type == NECRO_AST_TYPE_APP);
    necro_ast_assert_eq_go(ast1->type_app.ty, ast2->type_app.ty);
    necro_ast_assert_eq_go(ast1->type_app.next_ty, ast2->type_app.next_ty);
}

void necro_ast_assert_eq_bin_op_sym(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_BIN_OP_SYM);
    assert(ast2->type == NECRO_AST_BIN_OP_SYM);
    necro_ast_assert_eq_go(ast1->bin_op_sym.left, ast2->bin_op_sym.left);
    necro_ast_assert_eq_go(ast1->bin_op_sym.op, ast2->bin_op_sym.op);
    necro_ast_assert_eq_go(ast1->bin_op_sym.right, ast2->bin_op_sym.right);
}

void necro_ast_assert_eq_left_section(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_OP_LEFT_SECTION);
    assert(ast2->type == NECRO_AST_OP_LEFT_SECTION);
    necro_ast_assert_ast_symbol_name_eq(ast1->op_left_section.ast_symbol, ast2->op_left_section.ast_symbol);
    necro_ast_assert_eq_go(ast1->op_left_section.left, ast2->op_left_section.left);
}

void necro_ast_assert_eq_right_section(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_OP_RIGHT_SECTION);
    assert(ast2->type == NECRO_AST_OP_RIGHT_SECTION);
    necro_ast_assert_ast_symbol_name_eq(ast1->op_right_section.ast_symbol, ast2->op_right_section.ast_symbol);
    necro_ast_assert_eq_go(ast1->op_right_section.right, ast2->op_right_section.right);
}

void necro_ast_assert_eq_constructor(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_CONSTRUCTOR);
    assert(ast2->type == NECRO_AST_CONSTRUCTOR);
    necro_ast_assert_eq_go(ast1->constructor.conid, ast2->constructor.conid);
    necro_ast_assert_eq_go(ast1->constructor.arg_list, ast2->constructor.arg_list);
}

void necro_ast_assert_eq_simple_type(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_SIMPLE_TYPE);
    assert(ast2->type == NECRO_AST_SIMPLE_TYPE);
    necro_ast_assert_eq_go(ast1->simple_type.type_con, ast2->simple_type.type_con);
    necro_ast_assert_eq_go(ast1->simple_type.type_var_list, ast2->simple_type.type_var_list);
}

void necro_ast_assert_eq_data_declaration(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_DATA_DECLARATION);
    assert(ast2->type == NECRO_AST_DATA_DECLARATION);
    necro_ast_assert_eq_go(ast1->data_declaration.simpletype, ast2->data_declaration.simpletype);
    necro_ast_assert_eq_go(ast1->data_declaration.constructor_list, ast2->data_declaration.constructor_list);
}

void necro_ast_assert_eq_type_class_context(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TYPE_CLASS_CONTEXT);
    assert(ast2->type == NECRO_AST_TYPE_CLASS_CONTEXT);
    necro_ast_assert_eq_go(ast1->type_class_context.conid, ast2->type_class_context.conid);
    necro_ast_assert_eq_go(ast1->type_class_context.varid, ast2->type_class_context.varid);
}

void necro_ast_assert_eq_type_class_declaration(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    assert(ast2->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    necro_ast_assert_eq_go(ast1->type_class_declaration.context, ast2->type_class_declaration.context);
    necro_ast_assert_eq_go(ast1->type_class_declaration.tycls, ast2->type_class_declaration.tycls);
    necro_ast_assert_eq_go(ast1->type_class_declaration.tyvar, ast2->type_class_declaration.tyvar);
    necro_ast_assert_eq_go(ast1->type_class_declaration.declarations, ast2->type_class_declaration.declarations);
}

void necro_ast_assert_eq_type_class_instance(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    assert(ast2->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    necro_ast_assert_eq_go(ast1->type_class_instance.context, ast2->type_class_instance.context);
    necro_ast_assert_eq_go(ast1->type_class_instance.qtycls, ast2->type_class_instance.qtycls);
    necro_ast_assert_eq_go(ast1->type_class_instance.inst, ast2->type_class_instance.inst);
    necro_ast_assert_eq_go(ast1->type_class_instance.declarations, ast2->type_class_instance.declarations);
}

void necro_ast_assert_eq_type_signature(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_TYPE_SIGNATURE);
    assert(ast2->type == NECRO_AST_TYPE_SIGNATURE);
    necro_ast_assert_eq_go(ast1->type_signature.var, ast2->type_signature.var);
    necro_ast_assert_eq_go(ast1->type_signature.context, ast2->type_signature.context);
    necro_ast_assert_eq_go(ast1->type_signature.type, ast2->type_signature.type);
    assert(ast1->type_signature.sig_type == ast2->type_signature.sig_type);
}

void necro_ast_assert_eq_function_type(NecroAst* ast1, NecroAst* ast2)
{
    assert(ast1->type == NECRO_AST_FUNCTION_TYPE);
    assert(ast2->type == NECRO_AST_FUNCTION_TYPE);
    necro_ast_assert_eq_go(ast1->function_type.type, ast2->function_type.type);
    necro_ast_assert_eq_go(ast1->function_type.next_on_arrow, ast2->function_type.next_on_arrow);
}

void necro_ast_assert_eq_go(NecroAst* ast1, NecroAst* ast2)
{
    if (ast1 == NULL && ast2 == NULL)
        return;
    assert(ast1 != NULL);
    assert(ast2 != NULL);
    assert(ast1->type == ast2->type);
    switch (ast1->type)
    {
    // case NECRO_AST_UN_OP:                  necro_ast_assert_eq_var(intern1, ast1, ast2); break;
    case NECRO_AST_CONSTANT:               necro_ast_assert_eq_constant(ast1, ast2); break;
    case NECRO_AST_BIN_OP:                 necro_ast_assert_eq_bin_op(ast1, ast2); break;
    case NECRO_AST_IF_THEN_ELSE:           necro_ast_assert_eq_if_then_else(ast1, ast2); break;
    case NECRO_AST_TOP_DECL:               necro_ast_assert_eq_top_decl(ast1, ast2); break;
    case NECRO_AST_DECL:                   necro_ast_assert_eq_decl(ast1, ast2); break;
    case NECRO_AST_DECLARATION_GROUP_LIST: necro_ast_assert_eq_declaration_group_list(ast1, ast2); break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:      necro_ast_assert_eq_simple_assignment(ast1, ast2); break;
    case NECRO_AST_APATS_ASSIGNMENT:       necro_ast_assert_eq_apats_assignment(ast1, ast2); break;
    case NECRO_AST_PAT_ASSIGNMENT:         necro_ast_assert_eq_pat_assignment(ast1, ast2); break;
    case NECRO_AST_RIGHT_HAND_SIDE:        necro_ast_assert_eq_rhs(ast1, ast2); break;
    case NECRO_AST_LET_EXPRESSION:         necro_ast_assert_eq_let(ast1, ast2); break;
    case NECRO_AST_FUNCTION_EXPRESSION:    necro_ast_assert_eq_fexpr(ast1, ast2); break;
    case NECRO_AST_VARIABLE:               necro_ast_assert_eq_var(ast1, ast2); break;
    case NECRO_AST_APATS:                  necro_ast_assert_eq_apats(ast1, ast2); break;
    case NECRO_AST_WILDCARD:               break;
    case NECRO_AST_LAMBDA:                 necro_ast_assert_eq_lambda(ast1, ast2); break;
    case NECRO_AST_DO:                     necro_ast_assert_eq_do(ast1, ast2); break;
    case NECRO_AST_PAT_EXPRESSION:         necro_ast_assert_eq_pat_expression(ast1, ast2); break;
    case NECRO_AST_LIST_NODE:              necro_ast_assert_eq_list(ast1, ast2); break;
    case NECRO_AST_EXPRESSION_LIST:        necro_ast_assert_eq_expression_list(ast1, ast2); break;
    case NECRO_AST_EXPRESSION_ARRAY:       necro_ast_assert_eq_expression_array(ast1, ast2); break;
    case NECRO_AST_TUPLE:                  necro_ast_assert_eq_tuple(ast1, ast2); break;
    case NECRO_BIND_ASSIGNMENT:            necro_ast_assert_eq_bind_assignment(ast1, ast2); break;
    case NECRO_PAT_BIND_ASSIGNMENT:        necro_ast_assert_eq_pat_bind_assignment(ast1, ast2); break;
    case NECRO_AST_ARITHMETIC_SEQUENCE:    necro_ast_assert_eq_arithmetic_sequence(ast1, ast2); break;
    case NECRO_AST_CASE:                   necro_ast_assert_eq_case(ast1, ast2); break;
    case NECRO_AST_CASE_ALTERNATIVE:       necro_ast_assert_eq_case_alternative(ast1, ast2); break;
    case NECRO_AST_CONID:                  necro_ast_assert_eq_conid(ast1, ast2); break;
    case NECRO_AST_TYPE_APP:               necro_ast_assert_eq_type_app(ast1, ast2); break;
    case NECRO_AST_BIN_OP_SYM:             necro_ast_assert_eq_bin_op_sym(ast1, ast2); break;
    case NECRO_AST_OP_LEFT_SECTION:        necro_ast_assert_eq_left_section(ast1, ast2); break;
    case NECRO_AST_OP_RIGHT_SECTION:       necro_ast_assert_eq_right_section(ast1, ast2); break;
    case NECRO_AST_CONSTRUCTOR:            necro_ast_assert_eq_constructor(ast1, ast2); break;
    case NECRO_AST_SIMPLE_TYPE:            necro_ast_assert_eq_simple_type(ast1, ast2); break;
    case NECRO_AST_DATA_DECLARATION:       necro_ast_assert_eq_data_declaration(ast1, ast2); break;
    case NECRO_AST_TYPE_CLASS_CONTEXT:     necro_ast_assert_eq_type_class_context(ast1, ast2); break;
    case NECRO_AST_TYPE_CLASS_DECLARATION: necro_ast_assert_eq_type_class_declaration(ast1, ast2); break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:    necro_ast_assert_eq_type_class_instance(ast1, ast2); break;
    case NECRO_AST_TYPE_SIGNATURE:         necro_ast_assert_eq_type_signature(ast1, ast2); break;
    case NECRO_AST_FUNCTION_TYPE:          necro_ast_assert_eq_function_type(ast1, ast2); break;
    default:
        assert(false);
        return;
    }
}

void necro_ast_assert_eq(NecroAst* ast1, NecroAst* ast2)
{
    necro_ast_assert_eq_go(ast1, ast2);
}


///////////////////////////////////////////////////////
// Deep Copy
///////////////////////////////////////////////////////
NecroAst* necro_ast_copy_basic_info(NecroPagedArena* arena, NecroAst* declaration_group, NecroAst* ast_to_copy, NecroAst* ast2)
{
    ast2->necro_type = necro_type_deep_copy(arena, ast_to_copy->necro_type);
    ast2->source_loc = ast_to_copy->source_loc;
    ast2->end_loc    = ast_to_copy->end_loc;
    ast2->scope      = NULL; // ???
    if (declaration_group != NULL)
        assert(declaration_group->type == NECRO_AST_DECL || declaration_group->type == NECRO_AST_DECLARATION_GROUP_LIST);
    switch (ast2->type)
    {
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        ast2->type_class_declaration.declaration_group = declaration_group;
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        ast2->type_class_instance.declaration_group = declaration_group;
        break;
    case NECRO_AST_TYPE_SIGNATURE:
        ast2->type_signature.declaration_group = declaration_group;
        break;
    case NECRO_AST_DATA_DECLARATION:
        ast2->data_declaration.declaration_group = declaration_group;
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        ast2->simple_assignment.declaration_group = declaration_group;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        ast2->apats_assignment.declaration_group = declaration_group;
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        ast2->pat_assignment.declaration_group = declaration_group;
        break;
    case NECRO_AST_DECL:
        ast2->declaration.declaration_group_list = declaration_group;
        break;
    }
    return ast2;
}

NecroAst* necro_ast_deep_copy_go(NecroPagedArena* arena, NecroAst* declaration_group, NecroAst* ast)
{
    if (ast == NULL)
        return NULL;
    switch (ast->type)
    {
    case NECRO_AST_TOP_DECL:
    {
        NecroAst* copy_head = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_top_decl(arena, necro_ast_deep_copy_go(arena, declaration_group, ast->top_declaration.declaration), NULL));
        NecroAst* copy_curr = copy_head;
        ast                 = ast->top_declaration.next_top_decl;
        while (ast != NULL)
        {
            copy_curr->top_declaration.next_top_decl = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_top_decl(arena, necro_ast_deep_copy_go(arena, declaration_group, ast->top_declaration.declaration), NULL));
            copy_curr                                = copy_curr->top_declaration.next_top_decl;
            ast                                      = ast->top_declaration.next_top_decl;
        }
        return copy_head;
    }
    case NECRO_AST_DECL:
    {
        NecroAst* copy_head                     = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_decl(arena, NULL, NULL));
        NecroAst* copy_curr                     = copy_head;
        copy_curr->declaration.declaration_impl = necro_ast_deep_copy_go(arena, copy_curr, ast->declaration.declaration_impl);
        ast                                     = ast->declaration.next_declaration;
        while (ast != NULL)
        {
            copy_curr->declaration.next_declaration = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_decl(arena, NULL, NULL));
            copy_curr                               = copy_curr->declaration.next_declaration;
            copy_curr->declaration.declaration_impl = necro_ast_deep_copy_go(arena, copy_curr, ast->declaration.declaration_impl);
            ast                                     = ast->declaration.next_declaration;
        }
        return copy_head;
    }
    case NECRO_AST_DECLARATION_GROUP_LIST:
    {
        NecroAst* copy_head                                 = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_declaration_group_list(arena, NULL, NULL));
        NecroAst* copy_curr                                 = copy_head;
        copy_curr->declaration_group_list.declaration_group = necro_ast_deep_copy_go(arena, copy_curr, ast->declaration_group_list.declaration_group);
        ast                                                 = ast->declaration_group_list.next;
        while (ast != NULL)
        {
            copy_curr->declaration_group_list.next              = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_declaration_group_list(arena, NULL, NULL));
            copy_curr                                           = copy_curr->declaration_group_list.next;
            copy_curr->declaration_group_list.declaration_group = necro_ast_deep_copy_go(arena, copy_curr, ast->declaration_group_list.declaration_group);
            ast                                                 = ast->declaration_group_list.next;
        }
        return copy_head;
    }
    case NECRO_AST_CONSTANT:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_constant(arena, (NecroParseAstConstant) { .type = ast->constant.type, .int_literal = ast->constant.int_literal }));
    case NECRO_AST_BIN_OP:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_bin_op_full(arena, ast->bin_op.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->bin_op.lhs),
            necro_ast_deep_copy_go(arena, declaration_group, ast->bin_op.rhs),
            necro_type_deep_copy_subs(arena, ast->bin_op.inst_subs)));
    case NECRO_AST_IF_THEN_ELSE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_if_then_else(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->if_then_else.if_expr),
            necro_ast_deep_copy_go(arena, declaration_group, ast->if_then_else.then_expr),
            necro_ast_deep_copy_go(arena, declaration_group, ast->if_then_else.else_expr)));
    case NECRO_AST_SIMPLE_ASSIGNMENT:
    {
        NecroAst* copy = necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_simple_assignment_with_ast_symbol(arena, ast->simple_assignment.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->simple_assignment.initializer),
            necro_ast_deep_copy_go(arena, declaration_group, ast->simple_assignment.rhs)));
        copy->simple_assignment.is_recursive = ast->simple_assignment.is_recursive;
        return copy;
    }
    case NECRO_AST_APATS_ASSIGNMENT:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_apats_assignment_with_ast_symbol(arena, ast->apats_assignment.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->apats_assignment.apats),
            necro_ast_deep_copy_go(arena, declaration_group, ast->apats_assignment.rhs)));
    case NECRO_AST_PAT_ASSIGNMENT:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_pat_assignment(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->pat_assignment.pat),
            necro_ast_deep_copy_go(arena, declaration_group, ast->pat_assignment.rhs)));
    case NECRO_AST_RIGHT_HAND_SIDE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_rhs(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->right_hand_side.expression),
            necro_ast_deep_copy_go(arena, declaration_group, ast->right_hand_side.declarations)));
    case NECRO_AST_LET_EXPRESSION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_let(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->let_expression.expression),
            necro_ast_deep_copy_go(arena, declaration_group, ast->let_expression.declarations)));
    case NECRO_AST_FUNCTION_EXPRESSION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_fexpr(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->fexpression.aexp),
            necro_ast_deep_copy_go(arena, declaration_group, ast->fexpression.next_fexpression)));
    case NECRO_AST_VARIABLE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_var_full(arena,
            necro_ast_symbol_deep_copy(arena, ast->variable.ast_symbol),
            ast->variable.var_type, necro_type_deep_copy_subs(arena, ast->variable.inst_subs),
            necro_ast_deep_copy_go(arena, declaration_group, ast->variable.initializer), ast->variable.order));
    case NECRO_AST_APATS:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_apats(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->apats.apat),
            necro_ast_deep_copy_go(arena, declaration_group, ast->apats.next_apat)));
    case NECRO_AST_WILDCARD:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_wildcard(arena));
    case NECRO_AST_LAMBDA:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_lambda(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->lambda.apats),
            necro_ast_deep_copy_go(arena, declaration_group, ast->lambda.expression)));
    case NECRO_AST_DO:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_do(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->do_statement.statement_list)));
    case NECRO_AST_LIST_NODE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_list(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->list.item),
            necro_ast_deep_copy_go(arena, declaration_group, ast->list.next_item)));
    case NECRO_AST_EXPRESSION_LIST:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_expression_list(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->expression_list.expressions)));
    case NECRO_AST_EXPRESSION_ARRAY:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_expression_array(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->expression_array.expressions)));
    case NECRO_AST_PAT_EXPRESSION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_pat_expression(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->pattern_expression.expressions)));
    case NECRO_AST_TUPLE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_tuple(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->tuple.expressions)));
    case NECRO_BIND_ASSIGNMENT:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_bind_assignment(arena, ast->bind_assignment.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->bind_assignment.expression)));
    case NECRO_PAT_BIND_ASSIGNMENT:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_pat_bind_assignment(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->pat_bind_assignment.pat),
            necro_ast_deep_copy_go(arena, declaration_group, ast->pat_bind_assignment.expression)));
    case NECRO_AST_ARITHMETIC_SEQUENCE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_arithmetic_sequence(arena, ast->arithmetic_sequence.type,
            necro_ast_deep_copy_go(arena, declaration_group, ast->arithmetic_sequence.from),
            necro_ast_deep_copy_go(arena, declaration_group, ast->arithmetic_sequence.then),
            necro_ast_deep_copy_go(arena, declaration_group, ast->arithmetic_sequence.to)));
    case NECRO_AST_CASE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_case(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->case_expression.alternatives),
            necro_ast_deep_copy_go(arena, declaration_group, ast->case_expression.expression)));
    case NECRO_AST_CASE_ALTERNATIVE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_case_alternative(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->case_alternative.pat),
            necro_ast_deep_copy_go(arena, declaration_group, ast->case_alternative.body)));
    case NECRO_AST_CONID:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_conid_with_ast_symbol(arena, ast->conid.ast_symbol, ast->conid.con_type));
    case NECRO_AST_TYPE_APP:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_type_app(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_app.ty),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_app.next_ty)));
    case NECRO_AST_OP_LEFT_SECTION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_left_section_full(arena, ast->op_left_section.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->op_left_section.left),
            necro_type_deep_copy_subs(arena, ast->op_left_section.inst_subs)));
    case NECRO_AST_OP_RIGHT_SECTION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_right_section_full(arena, ast->op_right_section.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->op_right_section.right),
            necro_type_deep_copy_subs(arena, ast->op_right_section.inst_subs)));
    case NECRO_AST_CONSTRUCTOR:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_constructor(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->constructor.conid),
            necro_ast_deep_copy_go(arena, declaration_group, ast->constructor.arg_list)));
    case NECRO_AST_SIMPLE_TYPE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_simple_type_with_type_con(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->simple_type.type_con),
            necro_ast_deep_copy_go(arena, declaration_group, ast->simple_type.type_var_list)));
    case NECRO_AST_DATA_DECLARATION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_data_declaration_with_ast_symbol(arena, ast->data_declaration.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->data_declaration.simpletype),
            necro_ast_deep_copy_go(arena, declaration_group, ast->data_declaration.constructor_list)));
    case NECRO_AST_TYPE_CLASS_DECLARATION:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_type_class_full(arena, ast->type_class_declaration.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_declaration.tycls),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_declaration.tycls),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_declaration.context),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_declaration.declarations)));
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_instance_full(arena, ast->type_class_instance.ast_symbol,
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_instance.qtycls),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_instance.inst),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_instance.context),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_instance.declarations)));
    case NECRO_AST_TYPE_SIGNATURE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_type_signature(arena, ast->type_signature.sig_type,
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_signature.var),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_signature.context),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_signature.type)));
    case NECRO_AST_TYPE_CLASS_CONTEXT:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_context_full(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_context.conid),
            necro_ast_deep_copy_go(arena, declaration_group, ast->type_class_context.varid)));
    case NECRO_AST_FUNCTION_TYPE:
        return necro_ast_copy_basic_info(arena, declaration_group, ast, necro_ast_create_type_fn(arena,
            necro_ast_deep_copy_go(arena, declaration_group, ast->function_type.type),
            necro_ast_deep_copy_go(arena, declaration_group, ast->function_type.next_on_arrow)));
    default:
        assert(false);
        return NULL;
    }
}

NecroAst* necro_ast_deep_copy(NecroPagedArena* arena, NecroAst* ast)
{
    return necro_ast_deep_copy_go(arena, NULL, ast);
}


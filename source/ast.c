/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "symtable.h"
#include "type_class.h"
#include "ast.h"

#define AST_TAB "  "

void necro_ast_print_go(NecroAst* ast, NecroIntern* intern, uint32_t depth)
{
    assert(intern != NULL);
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
        printf("(%s, id: %d)\n", necro_intern_get_string(intern, ast->bin_op.symbol), ast->bin_op.id.id);
        necro_ast_print_go(ast->bin_op.lhs, intern, depth + 1);
        necro_ast_print_go(ast->bin_op.rhs, intern, depth + 1);
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
        case NECRO_AST_CONSTANT_INTEGER:
#if WIN32
            printf("(%lli)\n", ast->constant.int_literal);
#else
            printf("(%li)\n", ast->constant.int_literal);
#endif
            break;
        case NECRO_AST_CONSTANT_STRING:
            {
                const char* string = necro_intern_get_string(intern, ast->constant.symbol);
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
        necro_ast_print_go(ast->if_then_else.if_expr, intern, depth + 1);
        necro_ast_print_go(ast->if_then_else.then_expr, intern, depth + 1);
        necro_ast_print_go(ast->if_then_else.else_expr, intern, depth + 1);
        break;

    case NECRO_AST_TOP_DECL:
        if (ast->top_declaration.group_list != NULL)
        {
            puts("");
            NecroDeclarationGroupList* groups = ast->top_declaration.group_list;
            while (groups != NULL)
            {
                for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
                puts("(Top Declaration Group)");
                NecroDeclarationGroup* declarations = groups->declaration_group;
                while (declarations != NULL)
                {
                    necro_ast_print_go(declarations->declaration_ast, intern, depth + 1);
                    declarations = declarations->next;
                }
                groups = groups->next;
            }
        }
        else
        {
            puts("(Top Declaration)");
            necro_ast_print_go(ast->top_declaration.declaration, intern, depth + 1);
            if (ast->top_declaration.next_top_decl != NULL)
            {
                necro_ast_print_go(ast->top_declaration.next_top_decl, intern, depth);
            }
        }
        break;

    case NECRO_AST_DECL:
        if (ast->declaration.group_list != NULL)
        {
            puts("");
            NecroDeclarationGroupList* groups = ast->declaration.group_list;
            while (groups != NULL)
            {
                for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
                puts("(Declaration Group)");
                NecroDeclarationGroup* declarations = groups->declaration_group;
                while (declarations != NULL)
                {
                    necro_ast_print_go(declarations->declaration_ast, intern, depth + 1);
                    declarations = declarations->next;
                }
                groups = groups->next;
            }
        }
        else
        {
            puts("(Declaration)");
            necro_ast_print_go(ast->declaration.declaration_impl, intern, depth + 1);
            if (ast->declaration.next_declaration != NULL)
            {
                necro_ast_print_go(ast->declaration.next_declaration, intern, depth);
            }
        }
        break;

    case NECRO_AST_SIMPLE_ASSIGNMENT:
        printf("(Assignment: %s, id: %d)\n", necro_intern_get_string(intern, ast->simple_assignment.variable_name), ast->simple_assignment.id.id);
        if (ast->simple_assignment.initializer != NULL)
        {
            print_white_space(depth * 2);
            printf(" ~\n");
            necro_ast_print_go(ast->simple_assignment.initializer, intern, depth * 2);
            // print_white_space(depth * 2);
        }
        necro_ast_print_go(ast->simple_assignment.rhs, intern, depth + 1);
        break;

    case NECRO_AST_RIGHT_HAND_SIDE:
        puts("(Right Hand Side)");
        necro_ast_print_go(ast->right_hand_side.expression, intern, depth + 1);
        if (ast->right_hand_side.declarations != NULL)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "where");
            necro_ast_print_go(ast->right_hand_side.declarations, intern, depth + 3);
        }
        break;

    case NECRO_AST_LET_EXPRESSION:
        puts("(Let)");
        necro_ast_print_go(ast->let_expression.declarations, intern, depth + 1);
        if (ast->right_hand_side.declarations != NULL)
        {
            for (uint32_t i = 0;  i < depth; ++i)
            {
                printf(AST_TAB);
            }
            puts(AST_TAB AST_TAB "in");
            necro_ast_print_go(ast->let_expression.expression, intern, depth + 3);
        }
        break;


    case NECRO_AST_FUNCTION_EXPRESSION:
        puts("(fexp)");
        necro_ast_print_go(ast->fexpression.aexp, intern, depth + 1);
        if (ast->fexpression.next_fexpression != NULL)
        {
            necro_ast_print_go(ast->fexpression.next_fexpression, intern, depth + 1);
        }
        break;

    case NECRO_AST_VARIABLE:
    {
        const char* variable_string = necro_intern_get_string(intern, ast->variable.symbol);
        if (variable_string)
        {
            printf("(varid: %s, id: %d, vtype: %s)\n", variable_string, ast->variable.id.id, necro_var_type_string(ast->variable.var_type));
            if (ast->variable.initializer != NULL)
            {
                print_white_space(depth + 10);
                printf("~\n");
                necro_ast_print_go(ast->variable.initializer, intern, depth + 1);
            }
        }
        else
        {
            puts("(varid: ???");
        }
        break;
    }

    case NECRO_AST_APATS:
        puts("(Apat)");
        necro_ast_print_go(ast->apats.apat, intern, depth + 1);
        if (ast->apats.next_apat != NULL)
        {
            necro_ast_print_go(ast->apats.next_apat, intern, depth);
        }
        break;

    case NECRO_AST_WILDCARD:
        puts("(_)");
        break;

    case NECRO_AST_APATS_ASSIGNMENT:
        printf("(Apats Assignment: %s, id: %d)\n", necro_intern_get_string(intern, ast->apats_assignment.variable_name), ast->apats_assignment.id.id);
        necro_ast_print_go(ast->apats_assignment.apats, intern, depth + 1);
        necro_ast_print_go(ast->apats_assignment.rhs, intern, depth + 1);
        break;

    case NECRO_AST_PAT_ASSIGNMENT:
        // printf("(Pat Assignment: %s, id: %d)\n", necro_intern_get_string(intern, ast->pat_assignment.variable_name), ast->pat_assignment.id.id);
        printf("(Pat Assignment)\n");
        necro_ast_print_go(ast->pat_assignment.pat, intern, depth + 1);
        necro_ast_print_go(ast->pat_assignment.rhs, intern, depth + 1);
        break;

    case NECRO_AST_LAMBDA:
        puts("\\(lambda)");
        necro_ast_print_go(ast->lambda.apats, intern, depth + 1);
        for (uint32_t i = 0;  i < (depth + 1); ++i)
        {
            printf(AST_TAB);
        }
        puts("->");
        necro_ast_print_go(ast->lambda.expression, intern, depth + 2);
        break;

    case NECRO_AST_DO:
        puts("(do)");
        necro_ast_print_go(ast->do_statement.statement_list, intern, depth + 1);
        break;

    case NECRO_AST_PAT_EXPRESSION:
        puts("(pat)");
        necro_ast_print_go(ast->pattern_expression.expressions, intern, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_LIST:
        puts("([])");
        if (ast->expression_list.expressions != NULL)
            necro_ast_print_go(ast->expression_list.expressions, intern, depth + 1);
        break;

    case NECRO_AST_EXPRESSION_ARRAY:
        puts("({})");
        if (ast->expression_array.expressions != NULL)
            necro_ast_print_go(ast->expression_array.expressions, intern, depth + 1);
        break;

    case NECRO_AST_TUPLE:
        puts("(tuple)");
        necro_ast_print_go(ast->expression_list.expressions, intern, depth + 1);
        break;

    case NECRO_AST_LIST_NODE:
        printf("\r"); // clear current line
        necro_ast_print_go(ast->list.item, intern, depth);
        if (ast->list.next_item != NULL)
        {
            necro_ast_print_go(ast->list.next_item, intern, depth);
        }
        break;

    case NECRO_BIND_ASSIGNMENT:
        printf("(Bind: %s, id: %d)\n", necro_intern_get_string(intern, ast->bind_assignment.variable_name), ast->bind_assignment.id.id);
        necro_ast_print_go(ast->bind_assignment.expression, intern, depth + 1);
        break;

    case NECRO_PAT_BIND_ASSIGNMENT:
        printf("(Pat Bind)\n");
        necro_ast_print_go(ast->pat_bind_assignment.pat, intern, depth + 1);
        necro_ast_print_go(ast->pat_bind_assignment.expression, intern, depth + 1);
        break;

    case NECRO_AST_ARITHMETIC_SEQUENCE:
        {
            switch(ast->arithmetic_sequence.type)
            {
            case NECRO_ARITHMETIC_ENUM_FROM:
                puts("(EnumFrom)");
                necro_ast_print_go(ast->arithmetic_sequence.from, intern, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_TO:
                puts("(EnumFromTo)");
                necro_ast_print_go(ast->arithmetic_sequence.from, intern, depth + 1);
                necro_ast_print_go(ast->arithmetic_sequence.to, intern, depth + 1);
                break;
            case NECRO_ARITHMETIC_ENUM_FROM_THEN_TO:
                puts("(EnumFromThenTo)");
                necro_ast_print_go(ast->arithmetic_sequence.from, intern, depth + 1);
                necro_ast_print_go(ast->arithmetic_sequence.then, intern, depth + 1);
                necro_ast_print_go(ast->arithmetic_sequence.to, intern, depth + 1);
                break;
            default:
                assert(false);
                break;
            }
        }
        break;

    case NECRO_AST_CASE:
        puts("case");
        necro_ast_print_go(ast->case_expression.expression, intern, depth + 1);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("of");
        necro_ast_print_go(ast->case_expression.alternatives, intern, depth + 1);
        break;

    case NECRO_AST_CASE_ALTERNATIVE:
        printf("\r"); // clear current line
        necro_ast_print_go(ast->case_alternative.pat, intern, depth + 0);
        printf("\r"); // clear current line
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts("->");
        necro_ast_print_go(ast->case_alternative.body, intern, depth + 1);
        break;

    case NECRO_AST_CONID:
    {
        const char* con_string = necro_intern_get_string(intern, ast->conid.symbol);
        if (con_string)
            printf("(conid: %s, id: %d, ctype: %s)\n", con_string, ast->conid.id.id, necro_con_type_string(ast->conid.con_type));
        else
            puts("(conid: ???)");
        break;
    }

    case NECRO_AST_DATA_DECLARATION:
        puts("(data)");
        necro_ast_print_go(ast->data_declaration.simpletype, intern, depth + 1);
        for (uint32_t i = 0;  i < depth; ++i) printf(AST_TAB);
        puts(" = ");
        necro_ast_print_go(ast->data_declaration.constructor_list, intern, depth + 1);
        break;

    case NECRO_AST_SIMPLE_TYPE:
        puts("(simple type)");
        necro_ast_print_go(ast->simple_type.type_con, intern, depth + 1);
        if (ast->simple_type.type_var_list != NULL)
            necro_ast_print_go(ast->simple_type.type_var_list, intern, depth + 2);
        break;

    case NECRO_AST_CONSTRUCTOR:
        puts("(constructor)");
        necro_ast_print_go(ast->constructor.conid, intern, depth + 1);
        if (ast->constructor.arg_list != NULL)
            necro_ast_print_go(ast->constructor.arg_list, intern, depth + 1);
        break;

    case NECRO_AST_TYPE_APP:
        puts("(type app)");
        necro_ast_print_go(ast->type_app.ty, intern, depth + 1);
        if (ast->type_app.next_ty != NULL)
            necro_ast_print_go(ast->type_app.next_ty, intern, depth + 1);
        break;

    case NECRO_AST_BIN_OP_SYM:
        printf("(%s)\n", necro_intern_get_string(intern, ast->bin_op_sym.op->conid.symbol));
        necro_ast_print_go(ast->bin_op_sym.left, intern, depth + 1);
        necro_ast_print_go(ast->bin_op_sym.right, intern, depth + 1);
        break;

    case NECRO_AST_OP_LEFT_SECTION:
        printf("(LeftSection %s)\n", necro_bin_op_name(ast->op_left_section.type));
        necro_ast_print_go(ast->op_left_section.left, intern, depth + 1);
        break;

    case NECRO_AST_OP_RIGHT_SECTION:
        printf("(%s RightSection)\n", necro_bin_op_name(ast->op_right_section.type));
        necro_ast_print_go(ast->op_right_section.right, intern, depth + 1);
        break;

    case NECRO_AST_TYPE_SIGNATURE:
        printf("\r");
        necro_ast_print_go(ast->type_signature.var, intern, depth + 0);
        for (uint32_t i = 0;  i < depth + 1; ++i) printf(AST_TAB);
        puts("::");
        if (ast->type_signature.context != NULL)
        {
            necro_ast_print_go(ast->type_signature.context, intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        necro_ast_print_go(ast->type_signature.type, intern, depth + 1);
        break;

    case NECRO_AST_TYPE_CLASS_CONTEXT:
        printf("\r");
        necro_ast_print_go(ast->type_class_context.conid, intern, depth + 0);
        necro_ast_print_go(ast->type_class_context.varid, intern, depth + 0);
        break;

    case NECRO_AST_TYPE_CLASS_DECLARATION:
        puts("(class)");
        if (ast->type_class_declaration.context != NULL)
        {
            necro_ast_print_go(ast->type_class_declaration.context, intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        necro_ast_print_go(ast->type_class_declaration.tycls, intern, depth + 1);
        necro_ast_print_go(ast->type_class_declaration.tyvar, intern, depth + 1);
        if (ast->type_class_declaration.declarations != NULL)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            necro_ast_print_go(ast->type_class_declaration.declarations, intern, depth + 2);
        }
        if (ast->type_class_declaration.dictionary_data_declaration != NULL)
            necro_ast_print_go(ast->type_class_declaration.dictionary_data_declaration, intern, depth);
        break;

    case NECRO_AST_TYPE_CLASS_INSTANCE:
        puts("(instance)");
        if (ast->type_class_instance.context != NULL)
        {
            necro_ast_print_go(ast->type_class_instance.context, intern, depth + 1);
            for (uint32_t i = 0;  i < depth + 2; ++i) printf(AST_TAB);
            puts("=>");
        }
        necro_ast_print_go(ast->type_class_instance.qtycls, intern, depth + 1);
        necro_ast_print_go(ast->type_class_instance.inst, intern, depth + 1);
        if (ast->type_class_declaration.declarations != NULL)
        {
            for (uint32_t i = 0; i < depth; ++i) printf(AST_TAB);
            puts("where");
            necro_ast_print_go(ast->type_class_declaration.declarations, intern, depth + 2);
        }
        if (ast->type_class_instance.dictionary_instance != NULL)
            necro_ast_print_go(ast->type_class_instance.dictionary_instance, intern, depth);
        break;

    case NECRO_AST_FUNCTION_TYPE:
        puts("(");
        necro_ast_print_go(ast->function_type.type, intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts("->");
        necro_ast_print_go(ast->function_type.next_on_arrow, intern, depth + 1);
        for (uint32_t i = 0;  i < depth + 0; ++i) printf(AST_TAB);
        puts(")");
        break;

    default:
        puts("(Undefined)");
        break;
    }
}

void necro_ast_print(NecroAst* ast, NecroIntern* intern)
{
    if (ast == NULL)
    {
        puts("(Empty AST)");
    }
    else
    {
        necro_ast_print_go(ast, intern, 0);
    }
}

void necro_ast_arena_print(NecroAstArena* ast, NecroIntern* intern)
{
    if (ast->root == NULL)
    {
        puts("(Empty AST)");
    }
    else
    {
        necro_ast_print_go(ast->root, intern, 0);
    }
}

inline void necro_op_symbol_to_method_symbol(NecroIntern* intern, NECRO_BIN_OP_TYPE op_type, NecroSymbol* symbol)
{
    // change op name to method names
    switch (op_type)
    {
    case NECRO_BIN_OP_ADD:        *symbol = necro_intern_string(intern, "add");    break;
    case NECRO_BIN_OP_SUB:        *symbol = necro_intern_string(intern, "sub");    break;
    case NECRO_BIN_OP_MUL:        *symbol = necro_intern_string(intern, "mul");    break;
    case NECRO_BIN_OP_DIV:        *symbol = necro_intern_string(intern, "div");    break;
    case NECRO_BIN_OP_EQUALS:     *symbol = necro_intern_string(intern, "eq");     break;
    case NECRO_BIN_OP_NOT_EQUALS: *symbol = necro_intern_string(intern, "neq");    break;
    case NECRO_BIN_OP_GT:         *symbol = necro_intern_string(intern, "gt");     break;
    case NECRO_BIN_OP_LT:         *symbol = necro_intern_string(intern, "lt");     break;
    case NECRO_BIN_OP_GTE:        *symbol = necro_intern_string(intern, "gte");    break;
    case NECRO_BIN_OP_LTE:        *symbol = necro_intern_string(intern, "lte");    break;
    case NECRO_BIN_OP_BIND_RIGHT: *symbol = necro_intern_string(intern, "bind");   break;
    case NECRO_BIN_OP_APPEND:     *symbol = necro_intern_string(intern, "append"); break;
    default: break;
    }
}

NecroAst* necro_reify_go(NecroParseAstArena* parse_ast_arena, NecroParseAstLocalPtr parse_ast_ptr, NecroPagedArena* arena, NecroIntern* intern);
NecroAstArena necro_reify(NecroCompileInfo info, NecroIntern* intern, NecroParseAstArena* ast)
{
    NecroPagedArena arena = necro_paged_arena_create();
    NecroAst*       root  = necro_reify_go(ast, ast->root, &arena, intern);
    if (info.compilation_phase == NECRO_PHASE_REIFY && info.verbosity > 0)
        necro_ast_print(root, intern);
    return (NecroAstArena)
    {
        .arena = arena,
        .root = root,
    };
}

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
            NecroAst* from_ast             = necro_ast_create_var(arena, intern, "fromRational", NECRO_VAR_VAR);
            NecroAst* new_ast              = necro_paged_arena_alloc(arena, sizeof(NecroAst));
            *new_ast                       = *reified_ast;
            new_ast->constant.symbol       = ast->constant.symbol;
            new_ast->constant.type         = ast->constant.type;
            new_ast->constant.pat_from_ast = NULL;
            new_ast->constant.pat_eq_ast   = NULL;
            reified_ast                    = necro_ast_create_fexpr(arena, from_ast, new_ast);
            break;
        }

        case NECRO_AST_CONSTANT_INTEGER:
        {
            NecroAst* from_ast             = necro_ast_create_var(arena, intern, "fromInt", NECRO_VAR_VAR);
            NecroAst* new_ast              = necro_paged_arena_alloc(arena, sizeof(NecroAst));
            *new_ast                       = *reified_ast;
            new_ast->constant.symbol       = ast->constant.symbol;
            new_ast->constant.type         = ast->constant.type;
            new_ast->constant.pat_from_ast = NULL;
            new_ast->constant.pat_eq_ast   = NULL;
            reified_ast                    = necro_ast_create_fexpr(arena, from_ast, new_ast);
            break;
        }

        default:
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
        necro_op_symbol_to_method_symbol(intern, ast->bin_op.type, &ast->bin_op.symbol);
        reified_ast->bin_op.lhs          = necro_reify_go(parse_ast_arena, ast->bin_op.lhs, arena, intern);
        reified_ast->bin_op.rhs          = necro_reify_go(parse_ast_arena, ast->bin_op.rhs, arena, intern);
        reified_ast->bin_op.symbol       = ast->bin_op.symbol;
        reified_ast->bin_op.type         = ast->bin_op.type;
        reified_ast->bin_op.inst_context = NULL;
        break;
    case NECRO_AST_IF_THEN_ELSE:
        reified_ast->if_then_else.if_expr   = necro_reify_go(parse_ast_arena, ast->if_then_else.if_expr, arena, intern);
        reified_ast->if_then_else.then_expr = necro_reify_go(parse_ast_arena, ast->if_then_else.then_expr, arena, intern);
        reified_ast->if_then_else.else_expr = necro_reify_go(parse_ast_arena, ast->if_then_else.else_expr, arena, intern);
        break;
    case NECRO_AST_TOP_DECL:
        reified_ast->top_declaration.declaration   = necro_reify_go(parse_ast_arena, ast->top_declaration.declaration, arena, intern);
        reified_ast->top_declaration.next_top_decl = necro_reify_go(parse_ast_arena, ast->top_declaration.next_top_decl, arena, intern);
        reified_ast->top_declaration.group_list    = NULL;
        break;
    case NECRO_AST_DECL:
        reified_ast->declaration.declaration_impl = necro_reify_go(parse_ast_arena, ast->declaration.declaration_impl, arena, intern);
        reified_ast->declaration.next_declaration = necro_reify_go(parse_ast_arena, ast->declaration.next_declaration, arena, intern);
        reified_ast->declaration.group_list       = NULL;
        break;
    case NECRO_AST_SIMPLE_ASSIGNMENT:
        reified_ast->simple_assignment.initializer       = necro_reify_go(parse_ast_arena, ast->simple_assignment.initializer, arena, intern);
        reified_ast->simple_assignment.rhs               = necro_reify_go(parse_ast_arena, ast->simple_assignment.rhs, arena, intern);
        reified_ast->simple_assignment.variable_name     = ast->simple_assignment.variable_name;
        reified_ast->simple_assignment.declaration_group = NULL;
        reified_ast->simple_assignment.is_recursive      = false;
        break;
    case NECRO_AST_APATS_ASSIGNMENT:
        reified_ast->apats_assignment.variable_name     = ast->apats_assignment.variable_name;
        reified_ast->apats_assignment.apats             = necro_reify_go(parse_ast_arena, ast->apats_assignment.apats, arena, intern);
        reified_ast->apats_assignment.rhs               = necro_reify_go(parse_ast_arena, ast->apats_assignment.rhs, arena, intern);
        reified_ast->apats_assignment.declaration_group = NULL;
        break;
    case NECRO_AST_PAT_ASSIGNMENT:
        reified_ast->pat_assignment.pat               = necro_reify_go(parse_ast_arena, ast->pat_assignment.pat, arena, intern);
        reified_ast->pat_assignment.rhs               = necro_reify_go(parse_ast_arena, ast->pat_assignment.rhs, arena, intern);
        reified_ast->pat_assignment.declaration_group = NULL;
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
        reified_ast->variable.symbol       = ast->variable.symbol;
        reified_ast->variable.var_type     = ast->variable.var_type;
        reified_ast->variable.inst_context = NULL;
        reified_ast->variable.initializer  = necro_reify_go(parse_ast_arena, ast->variable.initializer, arena, intern);
        reified_ast->variable.is_recursive = false;
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
        reified_ast->bind_assignment.variable_name = ast->bind_assignment.variable_name;
        reified_ast->bind_assignment.expression    = necro_reify_go(parse_ast_arena, ast->bind_assignment.expression, arena, intern);
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
        reified_ast->conid.symbol   = ast->conid.symbol;
        reified_ast->conid.con_type = ast->conid.con_type;
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
        necro_op_symbol_to_method_symbol(intern, ast->op_left_section.type, &ast->op_left_section.symbol);
        reified_ast->op_left_section.left         = necro_reify_go(parse_ast_arena, ast->op_left_section.left, arena, intern);
        reified_ast->op_left_section.symbol       = ast->op_left_section.symbol;
        reified_ast->op_left_section.type         = ast->op_left_section.type;
        reified_ast->op_left_section.inst_context = NULL;
        break;
    case NECRO_AST_OP_RIGHT_SECTION:
        necro_op_symbol_to_method_symbol(intern, ast->op_right_section.type, &ast->op_right_section.symbol);
        reified_ast->op_right_section.symbol       = ast->op_right_section.symbol;
        reified_ast->op_right_section.type         = ast->op_right_section.type;
        reified_ast->op_right_section.right        = necro_reify_go(parse_ast_arena, ast->op_right_section.right, arena, intern);
        reified_ast->op_right_section.inst_context = NULL;
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
        reified_ast->type_class_declaration.dictionary_data_declaration = NULL;
        reified_ast->type_class_declaration.declaration_group           = NULL;
        break;
    case NECRO_AST_TYPE_CLASS_INSTANCE:
        reified_ast->type_class_instance.context             = necro_reify_go(parse_ast_arena, ast->type_class_instance.context, arena, intern);
        reified_ast->type_class_instance.qtycls              = necro_reify_go(parse_ast_arena, ast->type_class_instance.qtycls, arena, intern);
        reified_ast->type_class_instance.inst                = necro_reify_go(parse_ast_arena, ast->type_class_instance.inst, arena, intern);
        reified_ast->type_class_instance.declarations        = necro_reify_go(parse_ast_arena, ast->type_class_instance.declarations, arena, intern);
        reified_ast->type_class_instance.dictionary_instance = NULL;
        reified_ast->type_class_instance.declaration_group   = NULL;
        reified_ast->type_class_instance.instance_name       = necro_create_type_class_instance_name(intern, reified_ast);
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
        .arena = necro_paged_arena_empty(),
        .root  = NULL,
    };
}

NecroAstArena necro_ast_arena_create()
{
    NecroPagedArena        arena = necro_paged_arena_create();
    NecroAst* root  = NULL;
    return (NecroAstArena)
    {
        .arena = arena,
        .root = root,
    };
}

void necro_ast_arena_destroy(NecroAstArena* ast)
{
    necro_paged_arena_destroy(&ast->arena);
}

//=====================================================
// Manual AST construction
//=====================================================
typedef NecroAst NecroAst;
NecroAst* necro_ast_create_conid(NecroPagedArena* arena, NecroIntern* intern, const char* con_name, NECRO_CON_TYPE con_type)
{
    NecroAst* ast       = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type           = NECRO_AST_CONID;
    ast->conid.symbol   = necro_intern_string(intern, con_name);
    ast->conid.id       = (NecroID) { 0 };
    ast->conid.con_type = con_type;
    return ast;
}

NecroAst* necro_ast_create_var(NecroPagedArena* arena, NecroIntern* intern, const char* variable_name, NECRO_VAR_TYPE var_type)
{
    NecroAst* ast              = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                  = NECRO_AST_VARIABLE;
    ast->variable.symbol       = necro_intern_string(intern, variable_name);
    ast->variable.var_type     = var_type;
    ast->variable.id           = (NecroID) { 0 };
    ast->variable.inst_context = NULL;
    ast->variable.initializer  = NULL;
    ast->variable.is_recursive = false;
    return ast;
}

NecroAst* necro_ast_create_list(NecroPagedArena* arena, NecroAst* item, NecroAst* next)
{
    NecroAst* ast   = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type           = NECRO_AST_LIST_NODE;
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
    NecroAst* ast             = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                 = NECRO_AST_CONSTRUCTOR;
    ast->constructor.conid    = necro_ast_create_conid(arena, intern, con_name, NECRO_CON_DATA_DECLARATION);
    ast->constructor.arg_list = arg_list;
    return ast;
}

NecroAst* necro_ast_create_simple_type(NecroPagedArena* arena, NecroIntern* intern, const char* simple_type_name, NecroAst* ty_var_list)
{
    NecroAst* ast                  = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                      = NECRO_AST_SIMPLE_TYPE;
    ast->simple_type.type_con      = necro_ast_create_conid(arena, intern, simple_type_name, NECRO_CON_TYPE_DECLARATION);
    ast->simple_type.type_var_list = ty_var_list;
    return ast;
}

NecroAst* necro_ast_create_data_declaration(NecroPagedArena* arena, NecroIntern* intern, NecroAst* simple_type, NecroAst* constructor_list)
{
    UNUSED(intern);
    NecroAst* ast                          = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                              = NECRO_AST_DATA_DECLARATION;
    ast->data_declaration.simpletype       = simple_type;
    ast->data_declaration.constructor_list = constructor_list;
    return ast;
}

NecroAst* necro_ast_create_type_app(NecroPagedArena* arena, NecroAst* type1, NecroAst* type2)
{
    NecroAst* ast         = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type             = NECRO_AST_TYPE_APP;
    ast->type_app.ty      = type1;
    ast->type_app.next_ty = type2;
    return ast;
}

NecroAst* necro_ast_create_type_fn(NecroPagedArena* arena, NecroAst* type1, NecroAst* type2)
{
    NecroAst* ast                    = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                        = NECRO_AST_FUNCTION_TYPE;
    ast->function_type.type          = type1;
    ast->function_type.next_on_arrow = type2;
    return ast;
}

NecroAst* necro_ast_create_fexpr(NecroPagedArena* arena, NecroAst* f_ast, NecroAst* x_ast)
{
    NecroAst* ast                     = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                         = NECRO_AST_FUNCTION_EXPRESSION;
    ast->fexpression.aexp             = f_ast;
    ast->fexpression.next_fexpression = x_ast;
    return ast;
}

NecroAst* necro_ast_create_fn_type_sig(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* context_ast, NecroAst* type_ast, NECRO_VAR_TYPE var_type, NECRO_SIG_TYPE sig_type)
{
    NecroAst* ast                         = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                             = NECRO_AST_TYPE_SIGNATURE;
    ast->type_signature.var               = necro_ast_create_var(arena, intern, var_name, var_type);
    ast->type_signature.context           = context_ast;
    ast->type_signature.type              = type_ast;
    ast->type_signature.sig_type          = sig_type;
    ast->type_signature.declaration_group = NULL;
    return ast;
}

NecroAst* necro_ast_create_type_class(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* class_var, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                                 = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                                     = NECRO_AST_TYPE_CLASS_DECLARATION;
    ast->type_class_declaration.tycls             = necro_ast_create_conid(arena, intern, class_name, NECRO_CON_TYPE_DECLARATION);
    ast->type_class_declaration.tyvar             = necro_ast_create_var(arena, intern, class_var, NECRO_VAR_TYPE_FREE_VAR);
    ast->type_class_declaration.context           = context_ast;
    ast->type_class_declaration.declarations      = declarations_ast;
    ast->type_class_declaration.declaration_group = NULL;
    return ast;
}

NecroAst* necro_ast_create_instance(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, NecroAst* inst_ast, NecroAst* context_ast, NecroAst* declarations_ast)
{
    NecroAst* ast                                 = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                                     = NECRO_AST_TYPE_CLASS_INSTANCE;
    ast->type_class_instance.qtycls               = necro_ast_create_conid(arena, intern, class_name, NECRO_CON_TYPE_VAR);
    ast->type_class_instance.inst                 = inst_ast;
    ast->type_class_instance.context              = context_ast;
    ast->type_class_instance.declarations         = declarations_ast;
    ast->type_class_instance.dictionary_instance  = NULL;
    ast->type_class_declaration.declaration_group = NULL;
    ast->type_class_instance.instance_name        = necro_create_type_class_instance_name(intern, ast);
    return ast;
}

NecroAst* necro_ast_create_top_decl(NecroPagedArena* arena, NecroAst* top_level_declaration, NecroAst* next)
{
    NecroAst* ast                      = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                          = NECRO_AST_TOP_DECL;
    ast->top_declaration.declaration   = top_level_declaration;
    ast->top_declaration.next_top_decl = next;
    ast->top_declaration.group_list    = NULL;
    return ast;
}

NecroAst* necro_ast_create_decl(NecroPagedArena* arena, NecroAst* declaration, NecroAst* next)
{
    assert(declaration != NULL);
    NecroAst* ast                     = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                         = NECRO_AST_DECL;
    ast->declaration.declaration_impl = declaration;
    ast->declaration.next_declaration = next;
    ast->declaration.group_list       = NULL;
    return ast;
}

NecroAst* necro_ast_create_simple_assignment(NecroPagedArena* arena, NecroIntern* intern, const char* var_name, NecroAst* rhs_ast)
{
    assert(rhs_ast != NULL);
    NecroAst* ast                            = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                                = NECRO_AST_SIMPLE_ASSIGNMENT;
    ast->simple_assignment.id                = (NecroID) { 0 };
    ast->simple_assignment.variable_name     = necro_intern_string(intern, var_name);
    ast->simple_assignment.rhs               = rhs_ast;
    ast->simple_assignment.initializer       = NULL;
    ast->simple_assignment.declaration_group = NULL;
    ast->simple_assignment.is_recursive      = false;
    return ast;
}

NecroAst* necro_ast_create_context(NecroPagedArena* arena, NecroIntern* intern, const char* class_name, const char* var_name, NecroAst* next)
{
    assert(class_name != NULL);
    assert(var_name != NULL);
    NecroAst* ast                 = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                     = NECRO_AST_TYPE_CLASS_CONTEXT;
    ast->type_class_context.conid = necro_ast_create_conid(arena, intern, class_name, NECRO_CON_TYPE_VAR);
    ast->type_class_context.varid = necro_ast_create_var(arena, intern, var_name, NECRO_VAR_TYPE_FREE_VAR);

    NecroAst* list_ast       = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    list_ast->type           = NECRO_AST_LIST_NODE;
    list_ast->list.item      = ast;
    list_ast->list.next_item = next;
    return list_ast;
}

NecroAst* necro_ast_create_apats(NecroPagedArena* arena, NecroAst* apat_item, NecroAst* next_apat)
{
    assert(apat_item != NULL);
    NecroAst* ast        = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type            = NECRO_AST_APATS;
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
    NecroAst* ast                           = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                               = NECRO_AST_APATS_ASSIGNMENT;
    ast->apats_assignment.id                = (NecroID) { 0 };
    ast->apats_assignment.variable_name     = necro_intern_string(intern, var_name);
    ast->apats_assignment.apats             = apats;
    ast->apats_assignment.rhs               = rhs_ast;
    ast->apats_assignment.declaration_group = NULL;
    return ast;
}

NecroAst* necro_ast_create_lambda(NecroPagedArena* arena, NecroAst* apats, NecroAst* expr_ast)
{
    assert(apats != NULL);
    assert(expr_ast != NULL);
    assert(apats->type == NECRO_AST_APATS);
    NecroAst* ast          = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type              = NECRO_AST_LAMBDA;
    ast->lambda.apats      = apats;
    ast->lambda.expression = expr_ast;
    return ast;
}

NecroAst* necro_ast_create_rhs(NecroPagedArena* arena, NecroAst* expression, NecroAst* declarations)
{
    assert(expression != NULL);
    NecroAst* ast                     = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                         = NECRO_AST_RIGHT_HAND_SIDE;
    ast->right_hand_side.expression   = expression;
    ast->right_hand_side.declarations = declarations;
    return ast;
}

NecroAst* necro_ast_create_wildcard(NecroPagedArena* arena)
{
    NecroAst* ast = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type     = NECRO_AST_WILDCARD;
    return ast;
}

NecroAst* necro_ast_create_bin_op(NecroPagedArena* arena, NecroIntern* intern, const char* op_name, NecroAst* lhs, NecroAst* rhs)
{
    assert(op_name != NULL);
    assert(lhs != NULL);
    assert(rhs != NULL);
    NecroAst* ast            = necro_paged_arena_alloc(arena, sizeof(NecroAst));
    ast->type                = NECRO_AST_BIN_OP;
    ast->bin_op.id           = (NecroID) { 0 };
    ast->bin_op.symbol       = necro_intern_string(intern, op_name);
    ast->bin_op.lhs          = lhs;
    ast->bin_op.rhs          = rhs;
    ast->bin_op.inst_context = NULL;
    return ast;
}

//=====================================================
// Dependency Analysis
//=====================================================
NecroDeclarationGroup* necro_create_declaration_group(NecroPagedArena* arena, NecroAst* declaration_ast, NecroDeclarationGroup* prev)
{
    NecroDeclarationGroup* declaration_group = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationGroup));
    declaration_group->declaration_ast       = declaration_ast;
    declaration_group->next                  = NULL;
    declaration_group->dependency_list       = NULL;
    declaration_group->info                  = NULL;
    declaration_group->type_checked          = false;
    declaration_group->index                 = -1;
    declaration_group->low_link              = 0;
    declaration_group->on_stack              = false;
    if (prev == NULL)
    {
        return declaration_group;
    }
    else
    {
        prev->next = declaration_group;
        return prev;
    }
}

NecroDeclarationGroup* necro_append_declaration_group(NecroPagedArena* arena, NecroAst* declaration_ast, NecroDeclarationGroup* head)
{
    NecroDeclarationGroup* declaration_group = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationGroup));
    declaration_group->declaration_ast       = declaration_ast;
    declaration_group->next                  = NULL;
    declaration_group->dependency_list       = NULL;
    declaration_group->info                  = NULL;
    declaration_group->type_checked          = false;
    declaration_group->index                 = -1;
    declaration_group->low_link              = 0;
    declaration_group->on_stack              = false;
    if (head == NULL)
        return declaration_group;
    NecroDeclarationGroup* curr = head;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = declaration_group;
    return head;
}

void necro_append_declaration_group_to_group_in_group_list(NecroPagedArena* arena, NecroDeclarationGroupList* group_list, NecroDeclarationGroup* group_to_append)
{
    UNUSED(arena);
    NecroDeclarationGroup* existing_group = group_list->declaration_group;
    if (existing_group == NULL)
    {
        group_list->declaration_group = group_to_append;
        return;
    }
    NecroDeclarationGroup* curr = existing_group;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = group_to_append;
}

void necro_prepend_declaration_group_to_group_in_group_list(NecroPagedArena* arena, NecroDeclarationGroupList* group_list, NecroDeclarationGroup* group_to_prepend)
{
    UNUSED(arena);
    assert(group_to_prepend != NULL);
    NecroDeclarationGroup* curr = group_to_prepend;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = group_list->declaration_group;
    group_list->declaration_group = curr;
}

NecroDeclarationGroupList* necro_create_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* prev)
{
    NecroDeclarationGroupList* declaration_group_list = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationGroupList));
    declaration_group_list->declaration_group         = declaration_group;
    declaration_group_list->next                      = NULL;
    if (prev == NULL)
    {
        return declaration_group_list;
    }
    else
    {
        prev->next = declaration_group_list;
        return prev;
    }
}

NecroDeclarationGroupList* necro_prepend_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* next)
{
    NecroDeclarationGroupList* declaration_group_list = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationGroupList));
    declaration_group_list->declaration_group         = declaration_group;
    declaration_group_list->next                      = next;
    return declaration_group_list;
}

NecroDeclarationGroupList* necro_append_declaration_group_list(NecroPagedArena* arena, NecroDeclarationGroup* declaration_group, NecroDeclarationGroupList* head)
{
    NecroDeclarationGroupList* declaration_group_list = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationGroupList));
    declaration_group_list->declaration_group         = declaration_group;
    declaration_group_list->next                      = NULL;
    if (head == NULL)
        return declaration_group_list;
    NecroDeclarationGroupList* curr = head;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = declaration_group_list;
    return head;
}

NecroDeclarationGroupList* necro_get_curr_group_list(NecroDeclarationGroupList* group_list)
{
    assert(group_list != NULL);
    while (group_list->next != NULL)
        group_list = group_list->next;
    return group_list;
}

NecroDeclarationsInfo* necro_create_declarations_info(NecroPagedArena* arena)
{
    NecroDeclarationsInfo* info = necro_paged_arena_alloc(arena, sizeof(NecroDeclarationsInfo));
    info->group_lists           = necro_create_declaration_group_list(arena, NULL, NULL);
    info->current_group         = NULL;
    info->stack                 = necro_create_declaration_group_vector();
    info->index                 = 0;
    return info;
}

NecroDependencyList*necro_create_dependency_list(NecroPagedArena* arena, NecroDeclarationGroup* dependency_group, NecroDependencyList* head)
{
    NecroDependencyList* dependency_list = necro_paged_arena_alloc(arena, sizeof(NecroDependencyList));
    dependency_list->dependency_group    = dependency_group;
    dependency_list->next                = NULL;
    if (head == NULL)
        return dependency_list;
    NecroDependencyList* curr = head;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = dependency_list;
    return curr;
}

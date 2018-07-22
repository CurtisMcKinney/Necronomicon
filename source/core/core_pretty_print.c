/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "core_pretty_print.h"

void necro_core_pretty_print_go(NecroCoreAST_Expression* ast, NecroSymTable* symtable, NecroIntern* intern, size_t depth);

void necro_core_pretty_print(NecroCoreAST* ast, NecroSymTable* symtable)
{
    necro_core_pretty_print_go(ast->root, symtable, symtable->intern, 0);
}

void necro_core_pretty_print_alts(NecroCoreAST_CaseAlt* alts, NecroSymTable* symtable, NecroIntern* intern, size_t depth)
{
    while (alts != NULL)
    {
        print_white_space(depth);
        if (alts->altCon == NULL)
            printf("_");
        else
            necro_core_pretty_print_go(alts->altCon, symtable, intern, depth + 4);
        printf(" -> ");
        necro_core_pretty_print_go(alts->expr, symtable, intern, depth + 4);
        if (alts->next != NULL)
            printf("\n");
        alts = alts->next;
    }
}

void necro_core_pretty_print_var(NecroVar var, NecroIntern* intern)
{
    // printf("%s.%d", necro_intern_get_string(intern, var.symbol), var.id.id);
    printf("%s", necro_intern_get_string(intern, var.symbol));
}

void necro_core_pretty_print_go(NecroCoreAST_Expression* ast, NecroSymTable* symtable, NecroIntern* intern, size_t depth)
{
    if (ast == NULL)
        return;
    switch (ast->expr_type)
    {
    case NECRO_CORE_EXPR_APP:
    {
        necro_core_pretty_print_go(ast->app.exprA, symtable, intern, depth);
        printf(" ");
        if (ast->app.exprB->expr_type == NECRO_CORE_EXPR_APP)
            printf("(");
        necro_core_pretty_print_go(ast->app.exprB, symtable, intern, depth);
        if (ast->app.exprB->expr_type == NECRO_CORE_EXPR_APP)
            printf(")");
        return;
    }
    case NECRO_CORE_EXPR_BIND:
    {
        // printf("%s ", necro_intern_get_string(intern, ast->bind.var.symbol));
        necro_core_pretty_print_var(ast->bind.var, intern);
        printf(" ");
        NecroCoreAST_Expression* lambdas = ast->bind.expr;
        while (lambdas->expr_type == NECRO_CORE_EXPR_LAM)
        {
            // printf("%s ", necro_intern_get_string(intern, lambdas->lambda.arg->var.symbol));
            necro_core_pretty_print_var(lambdas->lambda.arg->var, intern);
            printf(" ");
            lambdas = lambdas->lambda.expr;
        }
        printf("= ");
        necro_core_pretty_print_go(lambdas, symtable, intern, depth + 4);
        return;
    }
    case NECRO_CORE_EXPR_LAM:
    {
        // printf("\\%s -> ", necro_intern_get_string(intern, ast->lambda.arg->var.symbol));
        printf("\\");
        necro_core_pretty_print_var(ast->lambda.arg->var, intern);
        printf(" -> ");
        necro_core_pretty_print_go(ast->lambda.expr, symtable, intern, depth);
        return;
    }
    case NECRO_CORE_EXPR_CASE:
    {
        printf("case ");
        necro_core_pretty_print_go(ast->case_expr.expr, symtable, intern, depth);
        printf(" of\n");
        necro_core_pretty_print_alts(ast->case_expr.alts, symtable, intern, depth);
        return;
    }
    case NECRO_CORE_EXPR_LET:
    {
        printf("\n");
        print_white_space(depth);
        printf("let ");
        necro_core_pretty_print_go(ast->let.bind, symtable, intern, depth);
        printf("\n");
        print_white_space(depth);
        printf("in ");
        necro_core_pretty_print_go(ast->let.expr, symtable, intern, depth);
        // printf("\n");
        return;
    }
    case NECRO_CORE_EXPR_VAR:
    {
        necro_core_pretty_print_var(ast->var, intern);
        return;
    }
    case NECRO_CORE_EXPR_LIT:
    {
        switch (ast->lit.type)
        {
        case NECRO_AST_CONSTANT_INTEGER:         printf("%lld", ast->lit.int_literal);  return;
        case NECRO_AST_CONSTANT_INTEGER_PATTERN: printf("%lld", ast->lit.int_literal);  return;
        case NECRO_AST_CONSTANT_FLOAT:           printf("%f", ast->lit.double_literal); return;
        case NECRO_AST_CONSTANT_FLOAT_PATTERN:   printf("%f", ast->lit.double_literal); return;
        case NECRO_AST_CONSTANT_CHAR:            printf("%c", ast->lit.char_literal);   return;
        default:                                                                        return;
        }
        return;
    }
    case NECRO_CORE_EXPR_DATA_DECL:
    {
        necro_print_type_sig_go(necro_symtable_get(symtable, ast->data_decl.data_id.id)->type, intern);
        printf(" = ");
        NecroCoreAST_Expression con;
        con.expr_type = NECRO_CORE_EXPR_DATA_CON;
        con.data_con  = *ast->data_decl.con_list;
        necro_core_pretty_print_go(&con, symtable, intern, depth);
        return;
    }
    case NECRO_CORE_EXPR_DATA_CON:
    {
        printf("%s ", necro_intern_get_string(intern, ast->data_con.condid.symbol));
        NecroCoreAST_Expression* args = ast->data_con.arg_list;
        while (args != NULL)
        {
            bool need_parans = (args->list.expr->expr_type == NECRO_CORE_EXPR_DATA_CON && args->list.expr->data_con.arg_list != NULL) || args->list.expr->expr_type == NECRO_CORE_EXPR_APP;
            if (need_parans)
                printf("(");
            necro_core_pretty_print_go(args->list.expr, symtable, intern, depth);
            if (need_parans)
                printf(")");
            if (args->list.next != NULL)
                printf(" ");
            args = args->list.next;
        }
        if (ast->data_con.next != NULL)
        {
            printf(" | ");
            NecroCoreAST_Expression con;
            con.expr_type = NECRO_CORE_EXPR_DATA_CON;
            con.data_con  = *ast->data_con.next;
            necro_core_pretty_print_go(&con, symtable, intern, depth);
        }
        return;
    }
    case NECRO_CORE_EXPR_LIST:
    {
        while (ast != NULL)
        {
            if (ast->list.expr == NULL)
            {
                ast = ast->list.next;
                continue;
            }
            print_white_space(depth);
            if (ast->list.expr->expr_type == NECRO_CORE_EXPR_BIND)
            {
                // printf("%s.%d :: ", necro_intern_get_string(intern, ast->list.expr->bind.var.symbol), ast->list.expr->bind.var.id.id);
                printf("%s :: ", necro_intern_get_string(intern, ast->list.expr->bind.var.symbol));
                NecroSymbolInfo* info = necro_symtable_get(symtable, ast->list.expr->bind.var.id);
                if (info->closure_type != NULL)
                {
                    necro_print_type_sig_go(info->closure_type, intern);
                }
                else
                {
                    necro_print_type_sig_go(info->type, intern);
                }
                // printf("\n    (arity = %d, isRecursive = %s)\n", info->arity, ast->list.expr->bind.is_recursive ? "true" : "false");
                printf("\n");
                print_white_space(depth);
            }
            necro_core_pretty_print_go(ast->list.expr, symtable, intern, depth);
            if (ast->list.expr->expr_type == NECRO_CORE_EXPR_BIND)
            {
                NecroSymbolInfo* info = necro_symtable_get(symtable, ast->list.expr->bind.var.id);
                printf("\n(arity = %d, isRecursive = %s)", info->arity, ast->list.expr->bind.is_recursive ? "true" : "false");
            }
            if (ast->list.next != NULL)
                printf("\n\n");
            else
                printf("\n");
            ast = ast->list.next;
        }
        return;
    }
    case NECRO_CORE_EXPR_TYPE:
        printf("(");
        necro_print_type_sig_go(ast->type.type, intern);
        printf(")");
        return;
    default:                        assert(false && "Unimplemented AST type in necro_core_pretty_print_go"); return;
    }
}

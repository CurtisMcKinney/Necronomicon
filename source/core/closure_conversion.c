/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include "closure_conversion.h"
#include <stdio.h>
#include "type.h"
#include "prim.h"
#include "core_create.h"

///////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////
typedef struct NecroClosureConversion
{
    NecroPagedArena       arena;
    NecroSnapshotArena    snapshot_arena;
    NecroIntern*          intern;
    NecroSymTable*        symtable;
    NecroScopedSymTable*  scoped_symtable;
    NecroPrimTypes*       prim_types;
    NecroInfer*           infer;
    NecroCon              closure_cons[NECRO_MAX_CLOSURE_TYPES];
    bool                  converting_type;
} NecroClosureConversion;

///////////////////////////////////////////////////////
// Forward Declarations
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_closure_conversion_go(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast);

///////////////////////////////////////////////////////
// Closure Conversion
///////////////////////////////////////////////////////
NecroCoreAST necro_closure_conversion(NecroCoreAST* in_ast, NecroIntern* intern, NecroSymTable* symtable, NecroScopedSymTable* scoped_symtable, NecroPrimTypes* prim_types, NecroInfer* infer)
{
    NecroClosureConversion cc = (NecroClosureConversion)
    {
        .arena           = necro_create_paged_arena(),
        .snapshot_arena  = necro_create_snapshot_arena(0),
        .intern          = intern,
        .symtable        = symtable,
        .scoped_symtable = scoped_symtable,
        .prim_types      = prim_types,
        .infer           = infer,
        .converting_type = false,
    };
    for (size_t i = 0; i < NECRO_MAX_CLOSURE_TYPES; ++i)
    {
        char num_buffer[10];
        itoa(i, num_buffer, 10);
        char closure_name_buffer[16] = "_Closure";
        strcat(closure_name_buffer, num_buffer);
        NecroSymbol con_symbol = necro_intern_string(intern, closure_name_buffer);
        cc.closure_cons[i] = necro_get_data_con_from_symbol(prim_types, con_symbol);
    }
    NecroCoreAST_Expression* out_ast = necro_closure_conversion_go(&cc, in_ast->root);
    necro_destroy_snapshot_arena(&cc.snapshot_arena);
    return (NecroCoreAST) { .arena = cc.arena, .root = out_ast };
}

///////////////////////////////////////////////////////
// Closure conversion
///////////////////////////////////////////////////////
// TODO: Ask Chad why core is spitting out NULL elements in top level core lists
NecroCoreAST_Expression* necro_closure_conversion_list(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIST);
    NecroCoreAST_Expression* head = NULL;
    if (in_ast->list.expr == NULL)
        head = necro_create_core_list(&cc->arena, NULL, NULL);
    else
        head = necro_create_core_list(&cc->arena, necro_closure_conversion_go(cc, in_ast->list.expr), NULL);
    NecroCoreAST_Expression* curr = head;
    in_ast = in_ast->list.next;
    while (in_ast != NULL)
    {
        if (in_ast->list.expr == NULL)
            curr->list.next = necro_create_core_list(&cc->arena, NULL, NULL);
        else
            curr->list.next = necro_create_core_list(&cc->arena, necro_closure_conversion_go(cc, in_ast->list.expr), NULL);
        curr            = curr->list.next;
        in_ast = in_ast->list.next;
    }
    return head;
}

NecroCoreAST_Expression* necro_closure_conversion_lit(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LIT);
    return necro_create_core_lit(&cc->arena, in_ast->lit);
}

NecroCoreAST_CaseAlt* necro_closure_conversion_alts(NecroClosureConversion* cc, NecroCoreAST_CaseAlt* alts)
{
    assert(cc != NULL);
    if (alts == NULL)
        return NULL;
    cc->converting_type = true;
    NecroCoreAST_Expression* alt_con = necro_closure_conversion_go(cc, alts->altCon);
    cc->converting_type = false;
    NecroCoreAST_Expression* expr    = necro_closure_conversion_go(cc, alts->expr);
    NecroCoreAST_CaseAlt*    next    = necro_closure_conversion_alts(cc, alts->next);
    return necro_create_core_case_alt(&cc->arena, expr, alt_con, next);
}

NecroCoreAST_Expression* necro_closure_conversion_case(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_CASE);
    NecroCoreAST_Expression* expr      = necro_closure_conversion_go(cc, in_ast->case_expr.expr);
    NecroCoreAST_CaseAlt*    alts      = necro_closure_conversion_alts(cc, in_ast->case_expr.alts);
    NecroCoreAST_Expression* case_expr = necro_create_core_case(&cc->arena, expr, alts);
    case_expr->case_expr.type          = in_ast->case_expr.type;
    return case_expr;
}

NecroCoreAST_Expression* necro_closure_conversion_lam(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LAM);
    necro_symtable_get(cc->symtable, in_ast->lambda.arg->var.id)->arity = 0;
    NecroCoreAST_Expression* arg_ast  = necro_closure_conversion_go(cc, in_ast->lambda.arg);
    NecroCoreAST_Expression* expr_ast = necro_closure_conversion_go(cc, in_ast->lambda.expr);
    return necro_create_core_lam(&cc->arena, arg_ast, expr_ast);
}

NecroCoreAST_Expression* necro_closure_conversion_let(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_LET);
    NecroCoreAST_Expression* bind_ast = necro_closure_conversion_go(cc, in_ast->let.bind);
    NecroCoreAST_Expression* expr_ast = necro_closure_conversion_go(cc, in_ast->let.expr);
    return necro_create_core_let(&cc->arena, bind_ast, expr_ast);
}

NecroCoreAST_Expression* necro_closure_conversion_var(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_VAR);
    if (cc->converting_type)
        return necro_create_core_var(&cc->arena, in_ast->var);
    NecroSymbolInfo*         info    = necro_symtable_get(cc->symtable, in_ast->var.id);
    NecroCoreAST_Expression* var_ast = necro_create_core_var(&cc->arena, in_ast->var);
    var_ast->necro_type              = info->type;
    if (info->arity > 0)
    {
        NecroCoreAST_Expression* con_ast = necro_create_core_app(&cc->arena, necro_create_core_var(&cc->arena, necro_con_to_var(cc->closure_cons[0])), var_ast);
        return con_ast;
    }
    else
    {
        return var_ast;
    }
}

typedef struct NecroBuildBindClosureType
{
    NecroCoreAST_Expression* expr;
    NecroType*               closure_type;
    NecroType*               closure_type_head;
    size_t                   arity;
} NecroBuildBindClosureType;
bool necro_build_bind_closure_type(NecroClosureConversion* cc, NecroBuildBindClosureType* builder, NecroType* fun_left, NecroType* rest_of_type)
{
    if (builder->expr->expr_type != NECRO_CORE_EXPR_LAM)
    {
        if (builder->closure_type == NULL)
        {
            builder->closure_type_head = necro_create_type_con(cc->infer, cc->prim_types->closure_type, necro_create_type_list(cc->infer, rest_of_type, NULL), 1);
            builder->closure_type      = builder->closure_type_head;
        }
        else
        {
            builder->closure_type->fun.type2 = necro_create_type_con(cc->infer, cc->prim_types->closure_type, necro_create_type_list(cc->infer, rest_of_type, NULL), 1);
        }
        return true;
    }
    else
    {
        builder->arity++;
        NecroType* left_type = fun_left;
        if (left_type->type == NECRO_TYPE_FUN)
            left_type = necro_create_type_con(cc->infer, cc->prim_types->closure_type, necro_create_type_list(cc->infer, fun_left, NULL), 1);
        if (builder->closure_type == NULL)
        {
            builder->closure_type_head = necro_create_type_fun(cc->infer, left_type, NULL);
            builder->closure_type      = builder->closure_type_head;
        }
        else
        {
            builder->closure_type->fun.type2 = necro_create_type_fun(cc->infer, left_type, NULL);
            builder->closure_type            = builder->closure_type->fun.type2;
        }
        builder->expr = builder->expr->lambda.expr;
        return false;
    }
}

NecroCoreAST_Expression* necro_closure_conversion_bind(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_BIND);
    // Build closure type
    NecroType*                normal_type  = necro_symtable_get(cc->symtable, in_ast->bind.var.id)->type;
    NecroBuildBindClosureType builder      = (NecroBuildBindClosureType) { .expr = in_ast->bind.expr, .closure_type = NULL, .arity = 0 };
    while (normal_type->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* context = normal_type->for_all.context;
        while (context != NULL)
        {
            if (necro_build_bind_closure_type(cc, &builder, necro_symtable_get(cc->symtable, cc->prim_types->any_type.id)->type, NULL))
                assert(false); // Should never be possible
            context = context->next;
        }
        normal_type = normal_type->for_all.type;
    }
    while (normal_type->type == NECRO_TYPE_FUN)
    {
        if (necro_build_bind_closure_type(cc, &builder, normal_type->fun.type1, normal_type))
            break;
        normal_type = normal_type->fun.type2;
    }
    if (builder.closure_type_head == NULL)
        builder.closure_type_head = normal_type;
    else if (builder.closure_type->type == NECRO_TYPE_FUN && builder.closure_type->fun.type2 == NULL)
        builder.closure_type->fun.type2 = normal_type;
    necro_symtable_get(cc->symtable, in_ast->bind.var.id)->closure_type = builder.closure_type_head;
    necro_symtable_get(cc->symtable, in_ast->bind.var.id)->arity        = builder.arity;
    NecroCoreAST_Expression* expr_ast = necro_closure_conversion_go(cc, in_ast->bind.expr);
    NecroCoreAST_Expression* bind_ast = necro_create_core_bind(&cc->arena, expr_ast, in_ast->var);
    bind_ast->bind.is_recursive       = in_ast->bind.is_recursive;
    return bind_ast;
}


NecroCoreAST_Expression* necro_closure_conversion_data_decl(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_DECL);
    assert(in_ast->data_decl.con_list != NULL);
    cc->converting_type  = true;
    NecroCoreAST_Expression con;
    con.expr_type = NECRO_CORE_EXPR_DATA_CON;
    con.data_con  = *in_ast->data_decl.con_list;
    NecroCoreAST_Expression* decl = necro_create_core_data_decl(&cc->arena, in_ast->var, &necro_closure_conversion_go(cc, &con)->data_con);
    cc->converting_type           = false;
    return decl;
}

typedef struct NecroBuildDataConClosureType
{
    NecroCoreAST_Expression* in_con_args;
    NecroCoreAST_Expression* con_args_head;
    NecroCoreAST_Expression* con_args;
    NecroType*               closure_type;
    NecroType*               closure_type_head;
    size_t                   arity;
} NecroBuildDataConClosureType;
void necro_build_data_closure_type(NecroClosureConversion* cc, NecroBuildDataConClosureType* builder, NecroType* fun_left)
{
    assert(builder->in_con_args != NULL);
    builder->arity++;
    NecroType*               left_type = fun_left;
    NecroCoreAST_Expression* left_arg  = necro_closure_conversion_go(cc, builder->in_con_args->list.expr);
    if (left_type->type == NECRO_TYPE_FUN)
    {
        left_type = necro_create_type_con(cc->infer, cc->prim_types->closure_type, necro_create_type_list(cc->infer, fun_left, NULL), 1);
        left_arg  = necro_create_core_app(&cc->arena, necro_create_core_var(&cc->arena, necro_con_to_var(cc->prim_types->closure_type)), left_arg);
    }
    if (left_arg != NULL)
        left_arg->necro_type = left_type;
    if (builder->closure_type == NULL)
    {
        builder->closure_type_head = necro_create_type_fun(cc->infer, left_type, NULL);
        builder->closure_type      = builder->closure_type_head;
        builder->con_args_head     = necro_create_core_list(&cc->arena, left_arg, NULL);
        builder->con_args          = builder->con_args_head;
    }
    else
    {
        builder->closure_type->fun.type2 = necro_create_type_fun(cc->infer, left_type, NULL);
        builder->closure_type            = builder->closure_type->fun.type2;
        builder->con_args->list.next     = necro_create_core_list(&cc->arena, left_arg, NULL);
        builder->con_args                = builder->con_args->list.next;
    }
    builder->in_con_args = builder->in_con_args->list.next;
}
NecroCoreAST_Expression* necro_closure_conversion_data_con(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_DATA_CON);
    NecroType*                   normal_type  = necro_symtable_get(cc->symtable, in_ast->data_con.condid.id)->type;
    NecroBuildDataConClosureType builder      = (NecroBuildDataConClosureType) { .in_con_args = in_ast->data_con.arg_list, .closure_type = NULL, .arity = 0 };
    while (normal_type->type == NECRO_TYPE_FOR)
    {
        NecroTypeClassContext* context = normal_type->for_all.context;
        // We don't allow type class contexts in data constructors
        assert(context == NULL);
        // while (context != NULL)
        // {
        //     necro_build_data_closure_type(cc, &builder, necro_symtable_get(cc->symtable, cc->prim_types->any_type.id)->type);
        //     context = context->next;
        // }
        normal_type = normal_type->for_all.type;
    }
    while (normal_type->type == NECRO_TYPE_FUN)
    {
        necro_build_data_closure_type(cc, &builder, normal_type->fun.type1);
        normal_type = normal_type->fun.type2;
    }
    if (builder.closure_type_head == NULL)
    {
        builder.closure_type_head = normal_type;
        builder.con_args_head     = builder.in_con_args;
    }
    else if (builder.closure_type->type == NECRO_TYPE_FUN && builder.closure_type->fun.type2 == NULL)
    {
        builder.closure_type->fun.type2 = normal_type;
        builder.con_args->list.next     = necro_closure_conversion_go(cc, builder.in_con_args);
    }
    necro_symtable_get(cc->symtable, in_ast->data_con.condid.id)->closure_type = builder.closure_type_head;
    necro_symtable_get(cc->symtable, in_ast->data_con.condid.id)->arity        = builder.arity;
    // Go deeper
    if (in_ast->data_con.next != NULL)
    {
        NecroCoreAST_Expression con;
        con.expr_type = NECRO_CORE_EXPR_DATA_CON;
        con.data_con  = *in_ast->data_con.next;
        return necro_create_core_data_con(&cc->arena, in_ast->data_con.condid, builder.con_args_head, &necro_closure_conversion_go(cc, &con)->data_con);
    }
    else
    {
        return necro_create_core_data_con(&cc->arena, in_ast->data_con.condid, builder.con_args_head, NULL);
    }
}


NECRO_DECLARE_ARENA_LIST(NecroCoreAST_Expression*, CoreAST, core_ast);

size_t necro_get_arity(NecroClosureConversion* cc, NecroSymbolInfo* info)
{
    if (info->arity != -1)
    {
        return info->arity;
    }
    else
    {
        // This is some primitive value which doesn't have a proper body
        // Calculate naive arity based on type
        // TODO: Some way to assert that this is infact a primitive type!
        assert(info->core_ast == NULL);
        // assert(info->ast == NULL);
        assert(info->type != NULL);
        size_t     arity = 0;
        NecroType* type  = info->type;
        while (type->type == NECRO_TYPE_FOR)
        {
            NecroTypeClassContext* context = type->for_all.context;
            while (context != NULL)
            {
                arity++;
                context = context->next;
            }
            type = type->for_all.type;
        }
        while (type->type == NECRO_TYPE_FUN)
        {
            arity++;
            type = type->fun.type2;
        }
        info->arity = arity;
        return arity;
    }
}

NecroCoreAST_Expression* necro_closure_conversion_app(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    assert(in_ast != NULL);
    assert(in_ast->expr_type == NECRO_CORE_EXPR_APP);
    if (cc->converting_type)
        return necro_create_core_app(&cc->arena, necro_closure_conversion_go(cc, in_ast->app.exprA), necro_closure_conversion_go(cc, in_ast->app.exprB));
    int32_t                  num_args = 0;
    NecroCoreASTList*        args     = NULL;
    NecroCoreAST_Expression* app      = in_ast;
    while (app->expr_type == NECRO_CORE_EXPR_APP)
    {
        num_args++;
        args = necro_cons_core_ast_list(&cc->arena, necro_closure_conversion_go(cc, app->app.exprB), args);
        app = app->app.exprA;
    }
    assert(app->expr_type == NECRO_CORE_EXPR_VAR);
    NecroSymbolInfo* info       = necro_symtable_get(cc->symtable, app->var.id);
    int32_t          difference = necro_get_arity(cc, info) - num_args;
    if (difference == 0)
    {
        NecroCoreAST_Expression* result = necro_create_core_var(&cc->arena, app->var);
        while (args != NULL)
        {
            result = necro_create_core_app(&cc->arena, result, args->data);
            args = args->next;
        }
        return result;
    }
    else if (difference > 0)
    {
        // partial application (create closures)
        NecroCoreAST_Expression* result = necro_create_core_var(&cc->arena, necro_con_to_var(cc->closure_cons[num_args]));
        result = necro_create_core_app(&cc->arena, result, necro_create_core_var(&cc->arena, app->var));
        while (args != NULL)
        {
            difference--;
            result = necro_create_core_app(&cc->arena, result, args->data);
            args = args->next;
        }
        assert(difference == 0);
        return result;
    }
    else
    {
        // Partially apply apply!?!?!?
        // over application (apply closures)
        NecroCoreAST_Expression* result = necro_create_core_var(&cc->arena, app->var);
        for (int32_t i = 0; i < info->arity; ++i)
        {
            result = necro_create_core_app(&cc->arena, result, args->data);
            args = args->next;
        }
        result = necro_create_core_app(&cc->arena, necro_create_core_var(&cc->arena, necro_con_to_var(cc->prim_types->apply_type)), result);
        while (args != NULL)
        {
            difference++;
            result = necro_create_core_app(&cc->arena, result, args->data);
            args = args->next;
        }
        assert(difference == 0);
        return result;
    }
    // TODO: Functions stored in data types are ALSO closures!!!!
}

///////////////////////////////////////////////////////
// Closure Conversion Go
///////////////////////////////////////////////////////
NecroCoreAST_Expression* necro_closure_conversion_go(NecroClosureConversion* cc, NecroCoreAST_Expression* in_ast)
{
    assert(cc != NULL);
    // assert(in_ast != NULL);
    if (in_ast == NULL)
        return NULL;
    switch (in_ast->expr_type)
    {
    case NECRO_CORE_EXPR_APP:       return necro_closure_conversion_app(cc, in_ast);
    case NECRO_CORE_EXPR_BIND:      return necro_closure_conversion_bind(cc, in_ast);
    case NECRO_CORE_EXPR_LAM:       return necro_closure_conversion_lam(cc, in_ast);
    case NECRO_CORE_EXPR_CASE:      return necro_closure_conversion_case(cc, in_ast);
    case NECRO_CORE_EXPR_LET:       return necro_closure_conversion_let(cc, in_ast);
    case NECRO_CORE_EXPR_VAR:       return necro_closure_conversion_var(cc, in_ast);
    case NECRO_CORE_EXPR_LIT:       return necro_closure_conversion_lit(cc, in_ast);
    case NECRO_CORE_EXPR_DATA_DECL: return necro_closure_conversion_data_decl(cc, in_ast);
    case NECRO_CORE_EXPR_DATA_CON:  return necro_closure_conversion_data_con(cc, in_ast);
    case NECRO_CORE_EXPR_LIST:      return necro_closure_conversion_list(cc, in_ast);
    // case NECRO_CORE_EXPR_TYPE:      assert(false && "NECRO_CORE_EXPR_TYPE not implemented; why are you using it, hmmm?"); return NULL;
    // The NECRO_CORE_EXPR_TYPE constructor honestly makes no sense.
    case NECRO_CORE_EXPR_TYPE:      return necro_create_core_necro_type(&cc->arena, in_ast->type.type);
    default:                        assert(false && "Unimplemented AST type in necro_closure_conversion_go"); return NULL;
    }
}

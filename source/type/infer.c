/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include "symtable.h"
#include "prim.h"
#include "type_class.h"
#include "kind.h"
#include "infer.h"
#include "base.h"
#include "utility.h"

/*
    Notes (Curtis, 2-8-19):
        * Static type class scheme to replace dictionary transformation?
        * in lieu of the above, remove all monomorphism restrictions and make everything polymorphic?
        * Healthy defaulting scheme, including a --> ()
*/

// Forward declarations
NecroResult(NecroType) necro_infer_apat(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_pattern(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_var(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_tuple_type(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_simple_assignment(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_pat_assignment(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_infer_apats_assignment(NecroInfer* infer, NecroAst* ast);
NecroResult(NecroType) necro_gen_pat_go(NecroInfer* infer, NecroAst* ast);
NecroType*             necro_infer_constant(NecroInfer* infer, NecroAst* ast);
void                   necro_pat_new_name_go(NecroInfer* infer, NecroAst* ast);

//=====================================================
// TypeSig
//=====================================================
NecroResult(NecroType) necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:      return ok(NecroType, necro_type_var_create(infer->arena, ast->variable.ast_symbol));
    case NECRO_AST_TUPLE:         return necro_infer_tuple_type(infer, ast);
    case NECRO_AST_FUNCTION_TYPE:
    {
        NecroType* left  = necro_try(NecroType, necro_ast_to_type_sig_go(infer, ast->function_type.type));
        NecroType* right = necro_try(NecroType, necro_ast_to_type_sig_go(infer, ast->function_type.next_on_arrow));
        ast->necro_type  = necro_type_fn_create(infer->arena, left, right);
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_CONID:
        ast->necro_type = necro_type_con_create(infer->arena, ast->conid.ast_symbol->type->con.con_symbol, NULL, ast->conid.ast_symbol->type->con.arity);
        return ok(NecroType, ast->necro_type);

    case NECRO_AST_CONSTRUCTOR:
    {
        NecroType* con_args = NULL;
        NecroAst*  arg_list = ast->constructor.arg_list;
        size_t arity = 0;
        while (arg_list != NULL)
        {
            NecroType* arg_type = necro_try(NecroType, necro_ast_to_type_sig_go(infer, arg_list->list.item));
            if (con_args == NULL)
                con_args = necro_type_list_create(infer->arena, arg_type, NULL);
            else
                con_args->list.next = necro_type_list_create(infer->arena, arg_type, NULL);
            arg_list = arg_list->list.next_item;
            arity++;
        }
        NecroType* env_con_type = ast->constructor.conid->conid.ast_symbol->type;
        ast->necro_type         = necro_type_con_create(infer->arena, env_con_type->con.con_symbol, con_args, env_con_type->con.arity);
        return ok(NecroType, ast->necro_type);
    }

    // Perhaps this is fucked!?
    case NECRO_AST_TYPE_APP:
    {
        NecroType* left   = necro_try(NecroType, necro_ast_to_type_sig_go(infer, ast->type_app.ty));
        NecroType* right  = necro_try(NecroType, necro_ast_to_type_sig_go(infer, ast->type_app.next_ty));
        if (left->type == NECRO_TYPE_CON)
        {
            if (left->con.args == NULL)
            {
                left->con.args = necro_type_list_create(infer->arena, right, NULL);
            }
            else if (left->con.args != NULL)
            {
                NecroType* current_arg = left->con.args;
                while (current_arg->list.next != NULL)
                    current_arg = current_arg->list.next;
                current_arg->list.next = necro_type_list_create(infer->arena, right, NULL);
            }
            ast->necro_type = left;
            return ok(NecroType, ast->necro_type);
        }
        else
        {
            ast->necro_type = necro_type_app_create(infer->arena, left, right);
            return ok(NecroType, ast->necro_type);
        }
    }
    default: necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_infer_type_sig(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_SIGNATURE);

    NecroType*             type_sig = necro_try(NecroType, necro_ast_to_type_sig_go(infer, ast->type_signature.type));
    NecroTypeClassContext* context  = necro_try_map(NecroTypeClassContext, NecroType, necro_ast_to_context(infer, ast->type_signature.context));
    context                         = necro_union_contexts(infer->arena, context, NULL);
    necro_try(NecroType, necro_ambiguous_type_class_check(ast->type_signature.var->variable.ast_symbol, context, type_sig));
    necro_apply_constraints(infer->arena, type_sig, context);
    type_sig                        = necro_try(NecroType, necro_type_generalize(infer->arena, infer->base, type_sig, ast->type_signature.type->scope));
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type_sig));
    type_sig->kind                  = necro_kind_gen(infer->arena, infer->base, type_sig->kind);
    necro_try(NecroType, necro_kind_unify(type_sig->kind, infer->base->star_kind->type, ast->scope));

    // TODO: Finish!
    // kind check for type context!
    NecroTypeClassContext* curr_context = context;
    while (curr_context != NULL)
    {
        NecroTypeClass* type_class     = curr_context->type_class;
        NecroType*      type_class_var = type_class->type_var->type;
        NecroType*      var_type       = curr_context->var_symbol->type;
        necro_try(NecroType, necro_kind_unify(var_type->kind, type_class_var->kind, ast->scope));
        curr_context = curr_context->next;
    }

    type_sig->pre_supplied                             = true;
    ast->type_signature.var->variable.ast_symbol->type = type_sig;
    ast->necro_type                                    = type_sig;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Data Declaration
//=====================================================
size_t necro_count_ty_vars(NecroAst* ty_vars)
{
    size_t count = 0;
    while (ty_vars != NULL)
    {
        count++;
        ty_vars = ty_vars->list.next_item;
    }
    return count;
}

NecroType* necro_ty_vars_to_args(NecroInfer* infer, NecroAst* ty_vars)
{
    NecroType* type = NULL;
    NecroType* head = NULL;
    while (ty_vars != NULL)
    {
        assert(ty_vars->type == NECRO_AST_LIST_NODE);
        if (ty_vars->list.item->variable.ast_symbol->type == NULL)
            ty_vars->list.item->variable.ast_symbol->type = necro_type_var_create(infer->arena, ty_vars->list.item->variable.ast_symbol);
        if (type == NULL)
        {

            head = necro_type_list_create(infer->arena, ty_vars->list.item->variable.ast_symbol->type, NULL);
            type = head;
        }
        else
        {
            type->list.next = necro_type_list_create(infer->arena, ty_vars->list.item->variable.ast_symbol->type, NULL);
            type = type->list.next;
        }
        ty_vars = ty_vars->list.next_item;
    }
    return head;
}

NecroResult(NecroType) necro_create_data_constructor(NecroInfer* infer, NecroAst* ast, NecroType* data_type, size_t con_num)
{
    NecroType* con_head = NULL;
    NecroType* con_args = NULL;
    NecroAst*  args_ast = ast->constructor.arg_list;
    size_t     count    = 0;
    while (args_ast != NULL)
    {
        NecroType* arg = necro_try(NecroType, necro_ast_to_type_sig_go(infer, args_ast->list.item));
        if (con_args == NULL)
        {
            con_args = necro_type_fn_create(infer->arena, arg, NULL);
            con_head = con_args;
        }
        else
        {
            con_args->fun.type2 = necro_type_fn_create(infer->arena, arg, NULL);
            con_args            = con_args->fun.type2;
        }
        args_ast             = args_ast->list.next_item;
        if (args_ast != NULL)
            args_ast->necro_type = arg; // arg type
        count++;
    }
    NecroType* con_type = NULL;
    if (con_args == NULL)
    {
        con_type = data_type;
    }
    else
    {
        con_args->fun.type2 = data_type;
        con_type            = con_head;
    }
    con_type                                                 = necro_try(NecroType, necro_type_generalize(infer->arena, infer->base, con_type, NULL));
    // con_type->source_loc                                     = ast->source_loc;
    con_type->pre_supplied                                   = true;
    ast->constructor.conid->conid.ast_symbol->type           = con_type;
    ast->constructor.conid->conid.ast_symbol->is_constructor = true;
    ast->constructor.conid->conid.ast_symbol->con_num        = con_num;
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, con_type));
    necro_try(NecroType, necro_kind_unify(con_type->kind, infer->base->star_kind->type, NULL));
    ast->necro_type = con_type;
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_simple_type(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_TYPE);
    assert(ast->simple_type.type_con->type == NECRO_AST_CONID);
    NecroType* type                                   = necro_type_con_create(infer->arena, ast->simple_type.type_con->conid.ast_symbol, necro_ty_vars_to_args(infer, ast->simple_type.type_var_list), necro_count_ty_vars(ast->simple_type.type_var_list));
    type->pre_supplied                                = true;
    ast->simple_type.type_con->conid.ast_symbol->type = type;
    necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, type));
    ast->necro_type = type;
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_apats_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_APATS_ASSIGNMENT);

    NecroType* proxy_type = ast->apats_assignment.ast_symbol->type;

    // Unify args (long winded version for better error messaging
    NecroAst*  apats      = ast->apats_assignment.apats;
    NecroType* f_type     = NULL;
    NecroType* f_head     = NULL;
    while (apats != NULL)
    {
        NecroType* apat_type = necro_try(NecroType, necro_infer_apat(infer, apats->apats.apat));
        if (f_type == NULL)
        {
            f_type = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_type            = f_type->fun.type2;
        }

        apats = apats->apats.next_apat;
    }

    // Unify rhs
    NecroType* rhs        = necro_try(NecroType, necro_infer_go(infer, ast->apats_assignment.rhs));
    NecroType* rhs_proxy  = necro_type_fresh_var(infer->arena);
    f_type->fun.type2     = rhs_proxy;

    // Unify against each arg?

    // Unify args
    NecroResult(NecroType) lhs_result = necro_type_unify_with_info(infer->arena, infer->base, proxy_type, f_head, ast->scope, ast->source_loc, ast->apats_assignment.apats->end_loc);
    if (lhs_result.type == NECRO_RESULT_ERROR)
    {
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, rhs_proxy, rhs, ast->scope, ast->apats_assignment.rhs->source_loc, ast->apats_assignment.rhs->end_loc));
        return lhs_result;
    }

    // Unify rhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, rhs_proxy, rhs, ast->scope, ast->apats_assignment.rhs->source_loc, ast->apats_assignment.rhs->end_loc));

    ast->necro_type = f_head;
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_simple_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    NecroType* proxy_type = ast->simple_assignment.ast_symbol->type;
    NecroType* init_type  = necro_try(NecroType, necro_infer_go(infer, ast->simple_assignment.initializer));
    NecroType* rhs_type   = necro_try(NecroType, necro_infer_go(infer, ast->simple_assignment.rhs));
    if (ast->simple_assignment.declaration_group != NULL && ast->simple_assignment.declaration_group->next != NULL)
        ast->simple_assignment.is_recursive = true;
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, proxy_type, rhs_type, ast->scope, ast->simple_assignment.rhs->source_loc, ast->simple_assignment.rhs->end_loc));
    if (init_type != NULL)
    {
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, proxy_type, init_type, ast->scope, ast->simple_assignment.initializer->source_loc, ast->simple_assignment.initializer->end_loc));
    }
    ast->necro_type = rhs_type;
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_ensure_pat_binding_is_monomorphic(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
        if (necro_type_find(ast->variable.ast_symbol->type)->type == NECRO_TYPE_FOR)
            return necro_type_polymorphic_pat_bind_error(ast->variable.ast_symbol, ast->variable.ast_symbol->type, ast->source_loc, ast->end_loc);
        else
            return ok(NecroType, NULL);
    case NECRO_AST_CONID:    return ok(NecroType, NULL);
    case NECRO_AST_CONSTANT: return ok(NecroType, NULL);
    case NECRO_AST_WILDCARD: return ok(NecroType, NULL);
    case NECRO_AST_TUPLE:
    {
        NecroAst* tuple = ast->tuple.expressions;
        while (tuple != NULL)
        {
            necro_try(NecroType, necro_ensure_pat_binding_is_monomorphic(infer, tuple->list.item));
            tuple = tuple->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* list = ast->expression_list.expressions;
        while (list != NULL)
        {
            necro_try(NecroType, necro_ensure_pat_binding_is_monomorphic(infer, list->list.item));
            list = list->list.next_item;
        }
        return ok(NecroType, NULL);
    }

    case NECRO_AST_BIN_OP_SYM:
        necro_try(NecroType, necro_ensure_pat_binding_is_monomorphic(infer, ast->bin_op_sym.left));
        return necro_ensure_pat_binding_is_monomorphic(infer, ast->bin_op_sym.right);

    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_try(NecroType, necro_ensure_pat_binding_is_monomorphic(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }

    default: necro_unreachable(NecroType);
    }
}

NecroResult(NecroType) necro_infer_pat_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_PAT_ASSIGNMENT);
    assert(ast->pat_assignment.pat != NULL);
    necro_try(NecroType, necro_ensure_pat_binding_is_monomorphic(infer, ast->pat_assignment.pat));
    NecroType* pat_type = necro_try(NecroType, necro_infer_apat(infer, ast->pat_assignment.pat));
    NecroType* rhs_type = necro_try(NecroType, necro_infer_go(infer, ast->pat_assignment.rhs));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, pat_type, rhs_type, ast->scope, ast->pat_assignment.rhs->source_loc, ast->pat_assignment.rhs->end_loc));
    necro_try(NecroType, necro_ensure_pat_binding_is_monomorphic(infer, ast->pat_assignment.pat));
    ast->necro_type = pat_type;
    return ok(NecroType, ast->necro_type);
}

void necro_pat_new_name_go(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        NecroAstSymbol* data = ast->variable.ast_symbol;
        NecroType*      type = data->type;
        if (type == NULL)
        {
            NecroType* new_name  = necro_type_fresh_var(infer->arena);
            // new_name->source_loc = ast->source_loc;
            new_name->var.scope  = data->ast->scope;
            data->type           = new_name;
            data->type_status    = NECRO_TYPE_CHECKING;
        }
        else
        {
            type->pre_supplied = true;
        }
        if (data->ast == NULL)
            data->ast = ast;
        return;
    }
    case NECRO_AST_CONSTANT: return;
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_pat_new_name_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_pat_new_name_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    case NECRO_AST_WILDCARD: return;
    case NECRO_AST_BIN_OP_SYM:
    {
        necro_pat_new_name_go(infer, ast->bin_op_sym.left);
        necro_pat_new_name_go(infer, ast->bin_op_sym.right);
        return;
    }
    case NECRO_AST_CONID: return;
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_pat_new_name_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    default: assert(false); return;
    }
}

NecroResult(NecroType) necro_gen_pat_go(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        // TODO: Take a closer look at this!!!!
        NecroAstSymbol* data = ast->variable.ast_symbol;
        if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE)
        {
            data->type_status = NECRO_TYPE_DONE;
            return ok(NecroType, NULL);
        }
        // Monomorphism restriction
        // infer->symtable->data[id.id].type                      = necro_gen(infer, proxy_type, infer->symtable->data[id.id].scope->parent);
        data->type_status = NECRO_TYPE_DONE;
        necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, data->type));
        data->type->kind = necro_kind_gen(infer->arena, infer->base, data->type->kind);
        return necro_kind_unify(data->type->kind, infer->base->star_kind->type, NULL);
    }
    case NECRO_AST_CONSTANT:
        return ok(NecroType, NULL);
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_try(NecroType, necro_gen_pat_go(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_try(NecroType, necro_gen_pat_go(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    case NECRO_AST_WILDCARD:
        return ok(NecroType, NULL);
    case NECRO_AST_BIN_OP_SYM:
        necro_try(NecroType, necro_gen_pat_go(infer, ast->bin_op_sym.left));
        return necro_gen_pat_go(infer, ast->bin_op_sym.right);
    case NECRO_AST_CONID:
        return ok(NecroType, NULL);
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_try(NecroType, necro_gen_pat_go(infer, args->list.item));
            args = args->list.next_item;
        }
        return ok(NecroType, NULL);
    }
    default:
        necro_unreachable(NecroType);
    }
}

//=====================================================
// Variable
//=====================================================
NecroResult(NecroType) necro_infer_var_initializer(NecroInfer* infer, NecroAst* ast)
{
    if (ast->variable.initializer == NULL)
        return ok(NecroType, NULL);
    NecroType* init_type = necro_try(NecroType, necro_infer_go(infer, ast->variable.initializer));
    return necro_type_unify_with_info(infer->arena, infer->base, ast->necro_type, init_type, ast->scope, ast->variable.initializer->source_loc, ast->variable.initializer->end_loc);
}

NecroResult(NecroType) necro_infer_var(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_VARIABLE);
    NecroAstSymbol* data = ast->variable.ast_symbol;
    assert(data != NULL);

    // assert?
    if (data->declaration_group != NULL)
        assert(data->type != NULL);

    if (ast->variable.var_type == NECRO_VAR_VAR && data->declaration_group != NULL && data->type_status != NECRO_TYPE_DONE && data->ast != NULL)
    {
        if (data->ast->type == NECRO_AST_SIMPLE_ASSIGNMENT && data->ast->simple_assignment.initializer == NULL && data->ast->simple_assignment.ast_symbol != infer->base->prim_undefined)
        {
            return necro_type_uninitialized_recursive_value_error(data, data->type, data->ast->source_loc, data->ast->end_loc);
        }
        else if (data->ast->type == NECRO_AST_VARIABLE && data->ast->variable.var_type == NECRO_VAR_DECLARATION)
        {
            if (data->ast->variable.initializer == NULL)
            {
                return necro_type_uninitialized_recursive_value_error(data, data->type, data->ast->source_loc, data->ast->end_loc);
            }
            else
            {
                data->ast->variable.is_recursive = true;
                data->is_recursive               = true;
            }
        }
    }

    if (data->type == NULL)
    {
        data->type            = necro_type_fresh_var(infer->arena);
        data->type->var.scope = data->ast->scope;
        ast->necro_type       = data->type;
        necro_try(NecroType, necro_infer_var_initializer(infer, ast));
        return ok(NecroType, ast->necro_type);
    }
    else if (necro_type_is_bound_in_scope(data->type, ast->scope))
    {
        ast->necro_type = data->type;
        necro_try(NecroType, necro_infer_var_initializer(infer, ast));
        return ok(NecroType, ast->necro_type);
    }
    else
    {
        ast->variable.inst_context = NULL;
        NecroType* inst_type       = necro_try(NecroType, necro_type_instantiate_with_context(infer->arena, infer->base, data->type, ast->scope, &ast->variable.inst_context));
        ast->necro_type            = inst_type;
        necro_try(NecroType, necro_infer_var_initializer(infer, ast));
        return ok(NecroType, ast->necro_type);
    }
}

//=====================================================
// Constant
//=====================================================
NecroType* necro_infer_constant(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONSTANT);
    switch (ast->constant.type)
    {
    case NECRO_AST_CONSTANT_FLOAT_PATTERN:
    {
        ast->necro_type = infer->base->rational_type->type;
        return ast->necro_type;
        // Removing overloaded numeric literal patterns for now...
        // NecroType* new_name   = necro_new_name(infer->arena, ast->source_loc);
        // new_name->var.context = necro_create_type_class_context(infer->arena,
        //     necro_symtable_get(infer->symtable, infer->prim_types->fractional_type_class.id)->type_class,
        //     infer->prim_types->fractional_type_class,
        //     (NecroCon) { .id = new_name->var.var.id, .symbol = new_name->var.var.symbol }, NULL);
        // new_name->var.context = necro_create_type_class_context(infer->arena,
        //     necro_symtable_get(infer->symtable, infer->prim_types->eq_type_class.id)->type_class,
        //     infer->prim_types->eq_type_class,
        //     (NecroCon) { .id = new_name->var.var.id, .symbol = new_name->var.var.symbol }, new_name->var.context);
        // new_name->type_kind = infer->star_type_kind;
        // ast->necro_type     = new_name;
        // return new_name;
    }
    case NECRO_AST_CONSTANT_INTEGER_PATTERN:
    {
        ast->necro_type = infer->base->int_type->type;
        return ast->necro_type;
        // Removing overloaded numeric literal patterns for now...
        // NecroType* new_name   = necro_new_name(infer->arena, ast->source_loc);
        // new_name->var.context = necro_create_type_class_context(infer->arena,
        //     necro_symtable_get(infer->symtable, infer->prim_types->num_type_class.id)->type_class,
        //     infer->prim_types->num_type_class,
        //     (NecroCon) { .id = new_name->var.var.id, .symbol = new_name->var.var.symbol }, NULL);
        // new_name->var.context = necro_create_type_class_context(infer->arena,
        //     necro_symtable_get(infer->symtable, infer->prim_types->eq_type_class.id)->type_class,
        //     infer->prim_types->eq_type_class,
        //     (NecroCon) { .id = new_name->var.var.id, .symbol = new_name->var.var.symbol }, new_name->var.context);
        // new_name->type_kind   = infer->star_type_kind;
        // ast->necro_type = new_name;
        // return new_name;
    }
    case NECRO_AST_CONSTANT_FLOAT:
        ast->necro_type = infer->base->float_type->type;
        return ast->necro_type;
    case NECRO_AST_CONSTANT_INTEGER:
        ast->necro_type = infer->base->int_type->type;
        return ast->necro_type;
    case NECRO_AST_CONSTANT_CHAR:
        ast->necro_type = infer->base->char_type->type;
        return ast->necro_type;
    case NECRO_AST_CONSTANT_CHAR_PATTERN:
        ast->necro_type = infer->base->char_type->type;
        return ast->necro_type;
    case NECRO_AST_CONSTANT_STRING:
    {
        NecroType* array_type = necro_type_con1_create(infer->arena, infer->base->array_type, infer->base->char_type->type);
        ast->necro_type       = array_type;
        return array_type;
    }
    default:
        assert(false);
        return NULL;
    }
}

//=====================================================
// ConID
//=====================================================
NecroResult(NecroType) necro_infer_conid(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONID);
    ast->necro_type = necro_try(NecroType, necro_type_instantiate(infer->arena, infer->base, ast->conid.ast_symbol->type, ast->scope));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Wildcard
//=====================================================
NecroType* necro_infer_wildcard(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_WILDCARD);
    ast->necro_type = necro_type_fresh_var(infer->arena);
    return ast->necro_type;
}

//=====================================================
// Tuple
//=====================================================
NecroResult(NecroType) necro_infer_tuple(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        NecroType* item_type = necro_try(NecroType, necro_infer_go(infer, current_expression->list.item));
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, item_type, NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, item_type, NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    ast->necro_type = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_tuple_pattern(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        NecroType* item_type = necro_try(NecroType, necro_infer_pattern(infer, current_expression->list.item));
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, item_type, NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, item_type, NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    ast->necro_type = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_tuple_type(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        NecroType* item_type = necro_try(NecroType, necro_ast_to_type_sig_go(infer, current_expression->list.item));
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, item_type, NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, item_type, NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    ast->necro_type = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    return ok(NecroType, ast->necro_type);
}

///////////////////////////////////////////////////////
// Expression Array
///////////////////////////////////////////////////////
NecroResult(NecroType) necro_infer_expression_array(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_ARRAY);
    NecroAst*  current_cell  = ast->expression_array.expressions;
    NecroType* element_type  = necro_type_fresh_var(infer->arena);
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try(NecroType, necro_infer_go(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, item_type, element_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
        current_cell = current_cell->list.next_item;
    }
    ast->necro_type = necro_type_con1_create(infer->arena, infer->base->array_type, element_type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Expression List
//=====================================================
NecroResult(NecroType) necro_infer_expression_list(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    NecroAst*  current_cell = ast->expression_list.expressions;
    NecroType* list_type    = necro_type_fresh_var(infer->arena);
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try(NecroType, necro_infer_go(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, list_type, item_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
        current_cell = current_cell->list.next_item;
    }
    ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, list_type);
    return ok(NecroType, ast->necro_type);
}

NecroResult(NecroType) necro_infer_expression_list_pattern(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    NecroAst*  current_cell = ast->expression_list.expressions;
    NecroType* list_type    = necro_type_fresh_var(infer->arena);
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try(NecroType, necro_infer_pattern(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, list_type, item_type, ast->scope, current_cell->list.item->source_loc, current_cell->list.item->end_loc));
        current_cell = current_cell->list.next_item;
    }
    ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, list_type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Pat Expression
//=====================================================
NecroResult(NecroType) necro_infer_pat_expression(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_PAT_EXPRESSION);
    NecroAst* current_cell = ast->pattern_expression.expressions;
    NecroType* pat_type     = necro_type_fresh_var(infer->arena);
    pat_type                = necro_type_con1_create(infer->arena, infer->base->pattern_type, pat_type);
    while (current_cell != NULL)
    {
        NecroType* item_type = necro_try(NecroType, necro_infer_go(infer, current_cell->list.item));
        necro_try(NecroType, necro_type_unify(infer->arena, infer->base, pat_type, item_type, ast->scope));
        current_cell = current_cell->list.next_item;
    }
    ast->necro_type = pat_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Function Expression
//=====================================================
NecroResult(NecroType) necro_infer_fexpr(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    NecroType* e1_type     = necro_try(NecroType, necro_infer_go(infer, ast->fexpression.next_fexpression));
    NecroType* e0_type     = necro_try(NecroType, necro_infer_go(infer, ast->fexpression.aexp));

    NecroType* arg_type    = necro_type_fresh_var(infer->arena);
    NecroType* result_type = necro_type_fresh_var(infer->arena);
    NecroType* f_type      = necro_type_fn_create(infer->arena, arg_type, result_type);

    // Unify f (in f x)
    // necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, f_type, e0_type, ast->scope, ast->fexpression.aexp->source_loc, ast->fexpression.aexp->end_loc));
    NecroResult(NecroType) f_result = necro_type_unify_with_info(infer->arena, infer->base, f_type, e0_type, ast->scope, ast->fexpression.aexp->source_loc, ast->fexpression.aexp->end_loc);
    if (f_result.type == NECRO_RESULT_ERROR)
    {
        // Does this break things?
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, arg_type, e1_type, ast->scope, ast->fexpression.next_fexpression->source_loc, ast->fexpression.next_fexpression->end_loc));
        return f_result;
    }

    // Unify x (in f x)
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, arg_type, e1_type, ast->scope, ast->fexpression.next_fexpression->source_loc, ast->fexpression.next_fexpression->end_loc));

    ast->necro_type        = result_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// If then else
//=====================================================
NecroResult(NecroType) necro_infer_if_then_else(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_IF_THEN_ELSE);
    NecroType* if_type   = necro_try(NecroType, necro_infer_go(infer, ast->if_then_else.if_expr));
    NecroType* then_type = necro_try(NecroType, necro_infer_go(infer, ast->if_then_else.then_expr));
    NecroType* else_type = necro_try(NecroType, necro_infer_go(infer, ast->if_then_else.else_expr));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, infer->base->bool_type->type, if_type, ast->scope, ast->if_then_else.if_expr->source_loc, ast->if_then_else.if_expr->end_loc));
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, then_type, else_type, ast->scope, ast->if_then_else.else_expr->source_loc, ast->if_then_else.else_expr->end_loc));
    ast->necro_type = then_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// BinOp
//=====================================================
NecroResult(NecroType) necro_infer_bin_op(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_BIN_OP);
    NecroType* x_type        = necro_try(NecroType, necro_infer_go(infer, ast->bin_op.lhs));
    ast->bin_op.inst_context = NULL;
    NecroType* op_type       = necro_try(NecroType, necro_type_instantiate_with_context(infer->arena, infer->base, ast->bin_op.ast_symbol->type, ast->scope, &ast->bin_op.inst_context));
    NecroType* y_type        = necro_try(NecroType, necro_infer_go(infer, ast->bin_op.rhs));

    NecroType* left_type     = necro_type_fresh_var(infer->arena);
    NecroType* right_type    = necro_type_fresh_var(infer->arena);
    NecroType* result_type   = necro_type_fresh_var(infer->arena);
    NecroType* bin_op_type   = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, result_type));

    // Unify op
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, bin_op_type, op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify lhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, left_type, x_type, ast->scope, ast->bin_op.lhs->source_loc, ast->bin_op.lhs->end_loc));

    // Unify rhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, right_type, y_type, ast->scope, ast->bin_op.rhs->source_loc, ast->bin_op.rhs->end_loc));

    ast->necro_type = result_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Operator Left Section
//=====================================================
NecroResult(NecroType) necro_infer_op_left_section(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_OP_LEFT_SECTION);
    NecroType* x_type                  = necro_try(NecroType, necro_infer_go(infer, ast->op_left_section.left));
    ast->op_left_section.inst_context  = NULL;
    NecroType* op_type                 = necro_try(NecroType, necro_type_instantiate_with_context(infer->arena, infer->base, ast->op_left_section.ast_symbol->type, ast->scope, &ast->op_left_section.inst_context));
    ast->op_left_section.op_necro_type = op_type;

    NecroType* left_type               = necro_type_fresh_var(infer->arena);
    NecroType* result_type             = necro_type_fresh_var(infer->arena);
    NecroType* section_type            = necro_type_fn_create(infer->arena, left_type, result_type);

    // Unify op
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, section_type, op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify lhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, left_type, x_type, ast->scope, ast->op_left_section.left->source_loc, ast->op_left_section.left->end_loc));

    ast->necro_type = result_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Operator Right Section
//=====================================================
NecroResult(NecroType) necro_infer_op_right_section(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_OP_RIGHT_SECTION);
    ast->op_right_section.inst_context  = NULL;
    NecroType* op_type                  = necro_try(NecroType, necro_type_instantiate_with_context(infer->arena, infer->base, ast->op_right_section.ast_symbol->type, ast->scope, &ast->op_right_section.inst_context));
    ast->op_right_section.op_necro_type = op_type;
    NecroType* y_type                   = necro_try(NecroType, necro_infer_go(infer, ast->op_right_section.right));

    NecroType* left_type                = necro_type_fresh_var(infer->arena);
    NecroType* right_type               = necro_type_fresh_var(infer->arena);
    NecroType* result_type              = necro_type_fresh_var(infer->arena);
    NecroType* bin_op_type              = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, result_type));

    // Unify op
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, bin_op_type, op_type, ast->scope, ast->source_loc, ast->end_loc));

    // Unify rhs
    necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, right_type, y_type, ast->scope, ast->op_right_section.right->source_loc, ast->op_right_section.right->end_loc));

    ast->necro_type                     = necro_type_fn_create(infer->arena, left_type, result_type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Apat
//=====================================================
NecroResult(NecroType) necro_infer_apat(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:        return necro_infer_var(infer, ast);
    case NECRO_AST_TUPLE:           return necro_infer_tuple_pattern(infer, ast);
    case NECRO_AST_EXPRESSION_LIST: return necro_infer_expression_list_pattern(infer, ast);
    case NECRO_AST_WILDCARD:        return ok(NecroType, necro_type_fresh_var(infer->arena));
    case NECRO_AST_CONSTANT:        return ok(NecroType, necro_infer_constant(infer, ast));

    case NECRO_AST_BIN_OP_SYM:
    {
        assert(ast->bin_op_sym.op->type == NECRO_AST_CONID);
        NecroType* constructor_type = ast->bin_op_sym.op->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type            = necro_try(NecroType, necro_type_instantiate(infer->arena, infer->base, constructor_type, NULL));
        NecroType* left_type        = necro_try(NecroType, necro_infer_apat(infer, ast->bin_op_sym.left));
        NecroType* right_type       = necro_try(NecroType, necro_infer_apat(infer, ast->bin_op_sym.right));
        NecroType* data_type        = necro_type_fresh_var(infer->arena);
        NecroType* f_type           = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, data_type));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, constructor_type, f_type, ast->scope, ast->source_loc, ast->end_loc));
        ast->necro_type             = f_type;
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_CONID:
    {
        NecroType* constructor_type  = ast->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type = necro_try(NecroType, necro_type_instantiate(infer->arena, infer->base, constructor_type, NULL));
        constructor_type = necro_type_get_fully_applied_fun_type(constructor_type);
        NecroType* type  = necro_try(NecroType, necro_infer_conid(infer, ast));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, constructor_type, type, ast->scope, ast->source_loc, ast->end_loc));
        ast->necro_type = constructor_type;
        return ok(NecroType, ast->necro_type);
    }

    case NECRO_AST_CONSTRUCTOR:
    {
        NecroType* constructor_type  = ast->constructor.conid->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type             = necro_try(NecroType, necro_type_instantiate(infer->arena, infer->base, constructor_type, NULL));
        NecroType* pattern_type_head = NULL;
        NecroType* pattern_type      = NULL;
        NecroAst*  ast_args          = ast->constructor.arg_list;
        size_t     arg_count         = 0;
        while (ast_args != NULL)
        {
            NecroType* item_type = necro_try(NecroType, necro_infer_apat(infer, ast_args->list.item));
            if (pattern_type_head == NULL)
            {
                pattern_type_head = necro_type_fn_create(infer->arena, item_type, NULL);
                pattern_type      = pattern_type_head;
            }
            else
            {
                pattern_type->fun.type2 = necro_type_fn_create(infer->arena, item_type, NULL);
                pattern_type            = pattern_type->fun.type2;
            }
            arg_count++;
            ast_args = ast_args->list.next_item;
        }

        NecroType* result_type = necro_type_fresh_var(infer->arena);
        if (pattern_type_head == NULL)
        {
            pattern_type_head = result_type;
        }
        else
        {
            pattern_type->fun.type2 = result_type;
        }

        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, constructor_type, pattern_type_head, ast->scope, ast->source_loc, ast->end_loc));

        // Ensure it's fully_applied
        NecroType* fully_applied_constructor = necro_try(NecroType, necro_type_instantiate(infer->arena, infer->base, ast->constructor.conid->conid.ast_symbol->type, NULL));
        fully_applied_constructor            = necro_type_get_fully_applied_fun_type(fully_applied_constructor);
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, fully_applied_constructor, result_type, ast->scope, ast->source_loc, ast->end_loc));


        ast->necro_type = necro_type_find(result_type);
        return ok(NecroType, ast->necro_type);
    }

    default:
        necro_unreachable(NecroType);
    }
}

//=====================================================
// Pattern
//=====================================================
NecroResult(NecroType) necro_infer_pattern(NecroInfer* infer, NecroAst* ast)
{
    return necro_infer_apat(infer, ast);
}

//=====================================================
// Case
//=====================================================
NecroResult(NecroType) necro_infer_case(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CASE);
    NecroType* expression_type = necro_try(NecroType, necro_infer_go(infer, ast->case_expression.expression));
    NecroType* result_type     = necro_type_fresh_var(infer->arena);
    NecroAst*  alternatives    = ast->case_expression.alternatives;
    while (alternatives != NULL)
    {
        NecroAst*  alternative = alternatives->list.item;
        NecroType* alt_type    = necro_try(NecroType, necro_infer_pattern(infer, alternative->case_alternative.pat))
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, expression_type, alt_type, alternatives->scope, alternative->case_alternative.pat->source_loc, alternative->case_alternative.pat->end_loc));
        NecroType* body_type   = necro_try(NecroType, necro_infer_go(infer, alternative->case_alternative.body));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, result_type, body_type, alternatives->scope, alternative->case_alternative.body->source_loc, alternative->case_alternative.body->end_loc));
        alternatives = alternatives->list.next_item;
    }
    ast->necro_type = result_type;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Lambda
//=====================================================
NecroResult(NecroType) necro_infer_lambda(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LAMBDA);
    NecroAst*  apats  = ast->lambda.apats;
    NecroType* f_type = NULL;
    NecroType* f_head = NULL;
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        NecroType* apat_type = necro_try(NecroType, necro_infer_apat(infer, apats->apats.apat));
        if (f_type == NULL)
        {
            f_type = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_type_fn_create(infer->arena, apat_type, NULL);
            f_type = f_type->fun.type2;
        }
        apats = apats->apats.next_apat;
    }
    f_type->fun.type2 = necro_try(NecroType, necro_infer_go(infer, ast->lambda.expression));
    ast->necro_type   = f_head;
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// RightHandSide
//=====================================================
NecroResult(NecroType) necro_infer_right_hand_side(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    necro_try(NecroType, necro_infer_go(infer, ast->right_hand_side.declarations));
    ast->necro_type = necro_try(NecroType, necro_infer_go(infer, ast->right_hand_side.expression));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Let
//=====================================================
NecroResult(NecroType) necro_infer_let_expression(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LET_EXPRESSION);
    necro_try(NecroType, necro_infer_go(infer, ast->let_expression.declarations));
    ast->necro_type = necro_try(NecroType, necro_infer_go(infer, ast->let_expression.expression));
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Arithmetic Sequence
//=====================================================
NecroResult(NecroType) necro_infer_arithmetic_sequence(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    NecroType* type = infer->base->int_type->type;
    if (ast->arithmetic_sequence.from != NULL)
    {
        NecroType* from_type = necro_try(NecroType, necro_infer_go(infer, ast->arithmetic_sequence.from));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, type, from_type, ast->scope, ast->arithmetic_sequence.from->source_loc, ast->arithmetic_sequence.from->end_loc));
    }
    if (ast->arithmetic_sequence.then != NULL)
    {
        NecroType* then_type = necro_try(NecroType, necro_infer_go(infer, ast->arithmetic_sequence.then));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, type, then_type, ast->scope, ast->arithmetic_sequence.then->source_loc, ast->arithmetic_sequence.then->end_loc));
    }
    if (ast->arithmetic_sequence.to != NULL)
    {
        NecroType* to_type = necro_try(NecroType, necro_infer_go(infer, ast->arithmetic_sequence.to));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, type, to_type, ast->scope, ast->arithmetic_sequence.to->source_loc, ast->arithmetic_sequence.to->end_loc));
    }
    ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, type);
    return ok(NecroType, ast->necro_type);
}

//=====================================================
// Do
//=====================================================
NecroResult(NecroType) necro_infer_do_statement(NecroInfer* infer, NecroAst* ast, NecroType* monad_var)
{
    assert(ast != NULL);
    switch(ast->type)
    {

    case NECRO_AST_LET_EXPRESSION:
        ast->necro_type = necro_try(NecroType, necro_infer_let_expression(infer, ast));
        return ok(NecroType, NULL);

    case NECRO_BIND_ASSIGNMENT:
    {
        NecroType* var_name                   = necro_type_fresh_var(infer->arena);
        ast->bind_assignment.ast_symbol->type = var_name;
        NecroType* rhs_type                   = necro_try(NecroType, necro_infer_go(infer, ast->bind_assignment.expression));
        ast->necro_type                       = necro_type_app_create(infer->arena, monad_var, var_name);
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, rhs_type, ast->necro_type, ast->scope, ast->bind_assignment.expression->source_loc, ast->bind_assignment.expression->end_loc));
        return ok(NecroType, NULL);
    }

    case NECRO_PAT_BIND_ASSIGNMENT:
    {
        NecroType* pat_type = necro_try(NecroType, necro_infer_pattern(infer, ast->pat_bind_assignment.pat));
        NecroType* rhs_type = necro_try(NecroType, necro_infer_go(infer, ast->pat_bind_assignment.expression));
        ast->necro_type     = necro_type_app_create(infer->arena, monad_var, pat_type);
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, rhs_type, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        return ok(NecroType, NULL);
    }

    default:
    {
        NecroType* statement_type = necro_try(NecroType, necro_infer_go(infer, ast));
        ast->necro_type           = necro_type_app_create(infer->arena, monad_var, necro_type_fresh_var(infer->arena));
        necro_try(NecroType, necro_type_unify_with_info(infer->arena, infer->base, statement_type, ast->necro_type, ast->scope, ast->source_loc, ast->end_loc));
        return ok(NecroType, ast->necro_type);
    }

    }
}

NecroResult(NecroType) necro_infer_do(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DO);
    NecroType* monad_var      = necro_type_fresh_var(infer->arena);
    NecroAst*  statements     = ast->do_statement.statement_list;
    NecroType* statement_type = NULL;
    necro_apply_constraints(infer->arena, monad_var, necro_create_type_class_context(infer->arena, infer->base->monad_type_class->type_class, infer->base->monad_type_class, monad_var->var.var_symbol, NULL));
    while (statements != NULL)
    {
        statement_type = necro_try(NecroType, necro_infer_do_statement(infer, statements->list.item, monad_var));
        statements     = statements->list.next_item;
    }
    ast->necro_type             = statement_type;
    ast->do_statement.monad_var = monad_var;
    if (statement_type == NULL)
        return necro_type_final_do_statement_error(NULL, monad_var, ast->source_loc, ast->end_loc);
    else
        return ok(NecroType, ast->necro_type);
}

//=====================================================
// Declaration groups
//=====================================================
// NOTE: NEW! Local bindings are mono-typed! Read up from Simon Peyton Jones why this is generally a good idea.
NecroResult(NecroType) necro_infer_declaration_group(NecroInfer* infer, NecroDeclarationGroup* declaration_group)
{
    assert(declaration_group != NULL);
    NecroAst*       ast  = NULL;
    NecroAstSymbol* data = NULL;

    //-----------------------------
    // Pass 1, new names
    NecroDeclarationGroup* curr = declaration_group;
    while (curr != NULL)
    {
        if (curr->type_checked) { curr = curr->next; continue; }
        ast = curr->declaration_ast;
        switch(ast->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            data = ast->simple_assignment.ast_symbol;
            if (data->type == NULL)
            {
                NecroType* new_name = necro_type_fresh_var(infer->arena);
                new_name->var.scope = ast->scope;
                data->type          = new_name;
            }
            else
            {
                // Hack: For built-ins
                data->type->pre_supplied = true;
            }
            break;
        }
        case NECRO_AST_APATS_ASSIGNMENT:
        {
            data = ast->apats_assignment.ast_symbol;
            if (data->type == NULL)
            {
                NecroType* new_name = necro_type_fresh_var(infer->arena);
                new_name->var.scope = ast->scope;
                data->type          = new_name;
            }
            else
            {
                // Hack: For built-ins
                // symbol_info->type->pre_supplied = true;
            }
            break;
        }
        case NECRO_AST_PAT_ASSIGNMENT:
            necro_pat_new_name_go(infer, ast->pat_assignment.pat);
            break;
        case NECRO_AST_DATA_DECLARATION:
            necro_try(NecroType, necro_infer_simple_type(infer, ast->data_declaration.simpletype));
            break;
        case NECRO_AST_TYPE_SIGNATURE:
            necro_try(NecroType, necro_infer_type_sig(infer, ast));
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            necro_try(NecroType, necro_create_type_class(infer, ast));
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            necro_try(NecroType, necro_create_type_class_instance(infer, ast));
            break;
        default:
            necro_unreachable(NecroType);
        }
        curr = curr->next;
    }

    //-----------------------------
    // Pass 2, infer types
    curr = declaration_group;
    while (curr != NULL)
    {
        if (curr->type_checked) { curr = curr->next; continue; }
        ast = curr->declaration_ast;
        switch(ast->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
            necro_try(NecroType, necro_infer_simple_assignment(infer, ast));
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            necro_try(NecroType, necro_infer_apats_assignment(infer, ast));
            break;
        case NECRO_AST_PAT_ASSIGNMENT:
            necro_try(NecroType, necro_infer_pat_assignment(infer, ast));
            break;
        case NECRO_AST_DATA_DECLARATION:
        {
            NecroAst* constructor_list = ast->data_declaration.constructor_list;
            size_t    con_num          = 0;
            while (constructor_list != NULL)
            {
                necro_try(NecroType, necro_create_data_constructor(infer, constructor_list->list.item, ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol->type, con_num));
                constructor_list = constructor_list->list.next_item;
                con_num++;
            }
            ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol->con_num = con_num;
            break;
        }
        case NECRO_AST_TYPE_SIGNATURE:
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            break;
        default:
            necro_unreachable(NecroType);
        }
        curr = curr->next;
    }

    // declaration_group->type_checked = true;

    //-----------------------------
    // Pass 3, generalize
    curr = declaration_group;
    while (curr != NULL)
    {
        ast = curr->declaration_ast;
        if (curr->type_checked) { curr = curr->next;  continue; }
        switch(ast->type)
        {
        case NECRO_AST_SIMPLE_ASSIGNMENT:
        {
            data = ast->simple_assignment.ast_symbol;
            if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE) { data->type_status = NECRO_TYPE_DONE; curr->type_checked = true; curr = curr->next;  continue; }

            // Monomorphism restriction
            // symbol_info->type = necro_gen(infer, symbol_info->type, symbol_info->scope->parent);

            // necro_infer_kind(infer, symbol_info->type, infer->base->star_kind->type, symbol_info->type, "While declaraing a variable: ");
            necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, data->type));
            data->type->kind = necro_kind_gen(infer->arena, infer->base, data->type->kind);
            necro_try(NecroType, necro_kind_unify(data->type->kind, infer->base->star_kind->type, NULL));
            data->type_status = NECRO_TYPE_DONE;
            break;
        }
        case NECRO_AST_APATS_ASSIGNMENT:
        {
            data = ast->apats_assignment.ast_symbol;
            if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE) { data->type_status = NECRO_TYPE_DONE; curr->type_checked = true; curr = curr->next;  continue; }
            // monomorphic local bindings!!!!
            if (necro_symtable_get_top_level_ast_symbol(infer->scoped_symtable, data->name) == NULL)
                break;
            // if (symbol_info->scope->parent == NULL)
            // Is local binding....how to check!?
            // TODO (Curtis, 2-8-19): Only generalize top level functions!!!!!!!
            data->type       = necro_try(NecroType, necro_type_generalize(infer->arena, infer->base, data->type, ast->scope->parent));
            necro_try(NecroType, necro_kind_infer(infer->arena, infer->base, data->type));
            data->type->kind = necro_kind_gen(infer->arena, infer->base, data->type->kind);
            necro_try(NecroType, necro_kind_unify(data->type->kind, infer->base->star_kind->type, NULL));
            data->type_status = NECRO_TYPE_DONE;
            break;
        }
        case NECRO_AST_PAT_ASSIGNMENT:
        {
            necro_try(NecroType, necro_gen_pat_go(infer, ast->pat_assignment.pat));
            break;
        }
        case NECRO_AST_DATA_DECLARATION:
        {
            data             = ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol;
            data->type->kind = necro_kind_gen(infer->arena, infer->base, data->type->kind);
            break;
        }
        case NECRO_AST_TYPE_SIGNATURE:
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            break;
        default:
            necro_unreachable(NecroType);
        }
        curr->type_checked = true;
        curr               = curr->next;
    }

    return ok(NecroType, NULL);
}

//=====================================================
// Declaration
//=====================================================
NecroResult(NecroType) necro_infer_declaration(NecroInfer* infer, NecroAst* ast)
{
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DECL);

    //----------------------------------------------------
    // Infer types for declaration groups
    NecroDeclarationGroupList* groups = ast->declaration.group_list;
    while (groups != NULL)
    {
        if (groups->declaration_group != NULL)
        {
            necro_try(NecroType, necro_infer_declaration_group(infer, groups->declaration_group));
        }
        groups = groups->next;
    }

    // Declarations themselves have no types
    return ok(NecroType, NULL);
}

//=====================================================
// Top Declaration
//=====================================================
NecroResult(NecroType) necro_infer_top_declaration(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TOP_DECL);

    //----------------------------------------------------
    // Iterate declaration groups
    NecroDeclarationGroupList* groups = ast->top_declaration.group_list;
    while (groups != NULL)
    {
        if (groups->declaration_group != NULL)
        {
            necro_try(NecroType, necro_infer_declaration_group(infer, groups->declaration_group));
        }
        groups = groups->next;
    }

    // Declarations themselves have no types
    return ok(NecroType, NULL);
}

///////////////////////////////////////////////////////
// Recursion
///////////////////////////////////////////////////////
bool necro_is_fully_concrete(NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return false;
    case NECRO_TYPE_APP:  return necro_is_fully_concrete(type->app.type1) && necro_is_fully_concrete(type->app.type2);
    case NECRO_TYPE_FUN:  return necro_is_fully_concrete(type->fun.type1) && necro_is_fully_concrete(type->fun.type2);
    case NECRO_TYPE_CON:  return type->con.args == NULL || necro_is_fully_concrete(type->con.args);
    case NECRO_TYPE_FOR:  return false;
    case NECRO_TYPE_LIST:
        while (type != NULL)
        {
            if (!necro_is_fully_concrete(type->list.item))
                return false;
            type = type->list.next;
        }
        return true;
    default: assert(false); return false;
    }
}

bool necro_is_sized(NecroType* type)
{
    assert(type != NULL);
    type = necro_type_find(type);
    switch (type->type)
    {
    case NECRO_TYPE_VAR:  return false;
    case NECRO_TYPE_APP:  return necro_is_sized(type->app.type1) && necro_is_sized(type->app.type2);
    // case NECRO_TYPE_FUN:  return necro_is_sized(symtable, type->fun.type1) && necro_is_sized(symtable, type->fun.type2);
    case NECRO_TYPE_FUN:  return false;
    case NECRO_TYPE_CON:  return (!type->con.con_symbol->is_recursive) && (type->con.args == NULL || necro_is_sized(type->con.args));
    case NECRO_TYPE_FOR:  return false;
    case NECRO_TYPE_LIST:
        while (type != NULL)
        {
            if (!necro_is_sized(type->list.item))
                return false;
            type = type->list.next;
        }
        return true;
    default: assert(false); return false;
    }
}

//=====================================================
// Infer Go
//=====================================================
NecroResult(NecroType) necro_infer_go(NecroInfer* infer, NecroAst* ast)
{
    if (ast == NULL)
        return ok(NecroType, NULL);
    switch (ast->type)
    {

    case NECRO_AST_CONSTANT:               return ok(NecroType, necro_infer_constant(infer, ast));
    case NECRO_AST_WILDCARD:               return ok(NecroType, necro_infer_wildcard(infer, ast));
    case NECRO_AST_VARIABLE:               return necro_infer_var(infer, ast);
    case NECRO_AST_CONID:                  return necro_infer_conid(infer, ast);
    case NECRO_AST_FUNCTION_EXPRESSION:    return necro_infer_fexpr(infer, ast);
    case NECRO_AST_BIN_OP:                 return necro_infer_bin_op(infer, ast);
    case NECRO_AST_OP_LEFT_SECTION:        return necro_infer_op_left_section(infer, ast);
    case NECRO_AST_OP_RIGHT_SECTION:       return necro_infer_op_right_section(infer, ast);
    case NECRO_AST_IF_THEN_ELSE:           return necro_infer_if_then_else(infer, ast);
    case NECRO_AST_RIGHT_HAND_SIDE:        return necro_infer_right_hand_side(infer, ast);
    case NECRO_AST_LAMBDA:                 return necro_infer_lambda(infer, ast);
    case NECRO_AST_LET_EXPRESSION:         return necro_infer_let_expression(infer, ast);
    case NECRO_AST_DECL:                   return necro_infer_declaration(infer, ast);
    case NECRO_AST_TOP_DECL:               return necro_infer_top_declaration(infer, ast);
    case NECRO_AST_TUPLE:                  return necro_infer_tuple(infer, ast);
    case NECRO_AST_EXPRESSION_LIST:        return necro_infer_expression_list(infer, ast);
    case NECRO_AST_EXPRESSION_ARRAY:       return necro_infer_expression_array(infer, ast);
    case NECRO_AST_PAT_EXPRESSION:         return necro_infer_pat_expression(infer, ast);
    case NECRO_AST_CASE:                   return necro_infer_case(infer, ast);
    case NECRO_AST_ARITHMETIC_SEQUENCE:    return necro_infer_arithmetic_sequence(infer, ast);
    case NECRO_AST_DO:                     return necro_infer_do(infer, ast);
    case NECRO_AST_TYPE_SIGNATURE:         return necro_infer_type_sig(infer, ast);

    case NECRO_AST_SIMPLE_ASSIGNMENT:      /* FALLTHROUGH */
    case NECRO_AST_APATS_ASSIGNMENT:       /* FALLTHROUGH */
    case NECRO_AST_PAT_ASSIGNMENT:         /* FALLTHROUGH */
    case NECRO_AST_TYPE_CLASS_DECLARATION: /* FALLTHROUGH */
    case NECRO_AST_TYPE_CLASS_INSTANCE:    /* FALLTHROUGH */
    case NECRO_AST_DATA_DECLARATION:       /* FALLTHROUGH */
    default:                               necro_unreachable(NecroType);
    }
}

NecroResult(void) necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroInfer infer = necro_infer_create(&ast_arena->arena, intern, scoped_symtable, base, ast_arena);
    necro_try_map(NecroType, void, necro_infer_go(&infer, ast_arena->root));
    if (info.compilation_phase == NECRO_PHASE_INFER && info.verbosity > 0)
    {
        necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    // TODO: Removing type class translation for now...
    // TODO: Separate Infer and Type Class translation phases with Different State object for type class translation!!!!
    // necro_try(void, necro_type_class_translate(&infer, ast_arena->root));
    if (info.compilation_phase == NECRO_PHASE_TYPE_CLASS_TRANSLATE && info.verbosity > 0)
        necro_ast_arena_print(ast_arena);
    return ok_void();
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////

void necro_infer_test_result(const char* test_name, const char* str, NECRO_RESULT_TYPE expected_result, const NECRO_RESULT_ERROR_TYPE* error_type)
{
    // Set up
    NecroIntern         intern          = necro_intern_create();
    NecroSymTable       symtable        = necro_symtable_create(&intern);
    NecroScopedSymTable scoped_symtable = necro_scoped_symtable_create(&symtable);
    NecroBase           base            = necro_base_compile(&intern, &scoped_symtable);
    UNUSED(base);

    NecroLexTokenVector tokens          = necro_empty_lex_token_vector();
    NecroParseAstArena  parse_ast       = necro_parse_ast_arena_empty();
    NecroAstArena       ast             = necro_ast_arena_empty();
    NecroCompileInfo    info            = necro_test_compile_info();

    // Compile
    unwrap(void, necro_lex(info, &intern, str, strlen(str), &tokens));
    unwrap(void, necro_parse(info, &intern, &tokens, necro_intern_string(&intern, "Test"), &parse_ast));
    ast = necro_reify(info, &intern, &parse_ast);
    necro_build_scopes(info, &scoped_symtable, &ast);
    unwrap(void, necro_rename(info, &scoped_symtable, &intern, &ast));
    necro_dependency_analyze(info, &intern, &ast);
    NecroResult(void) result = necro_infer(info, &intern, &scoped_symtable, &base, &ast);

    // Assert
    // TODO (Curtis, 2-7-19): ASSERT_BREAK macro is broken.
    // ASSERT_BREAK(result.type == expected_result);
    assert(result.type == expected_result);
    bool passed = result.type == expected_result;
    if (expected_result == NECRO_RESULT_ERROR)
    {
        // ASSERT_BREAK(error_type != NULL);
        assert(error_type != NULL);
        if (result.error != NULL && error_type != NULL)
        {
            // ASSERT_BREAK(result.error->type == *error_type);
            assert(result.error->type == *error_type);
            passed &= result.error->type == *error_type;
        }
        else
        {
            passed = false;
        }
    }

    const char* result_string = passed ? "Passed" : "Failed";
    printf("Infer %s test: %s\n", test_name, result_string);
    fflush(stdout);
    // UNUSED(test_name);
    // UNUSED(result_string);

    // Clean up
    if (result.type == NECRO_RESULT_ERROR)
        necro_result_error_print(result.error, str, "Test");
    else
        free(result.error);
    necro_ast_arena_destroy(&ast);
    necro_parse_ast_arena_destroy(&parse_ast);
    necro_destroy_lex_token_vector(&tokens);
    necro_scoped_symtable_destroy(&scoped_symtable);
    necro_symtable_destroy(&symtable);
    necro_intern_destroy(&intern);
}

void necro_test_infer()
{
    necro_announce_phase("NecroInfer");
    fflush(stdout);

    ///////////////////////////////////////////////////////
    // Error tests
    ///////////////////////////////////////////////////////
    {
        const char* test_name = "NothingType";
        const char* test_source = ""
            "notcronomicon = Nothing\n";

        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_OK;
        const NECRO_RESULT_ERROR_TYPE expected_error = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "DataDeclarations";
        const char* test_source = ""
            "data Book = Pages\n"
            "data NotBook = EmptyPages\n";

        const NECRO_RESULT_TYPE expect_ok_result = NECRO_RESULT_OK;
        const NECRO_RESULT_ERROR_TYPE* no_expected_error = NULL;
        necro_infer_test_result(test_name, test_source, expect_ok_result, no_expected_error);
    }

    {
        const char*                   test_name           = "Uninitialized Recursive Value";
        const char*                   test_source         = "impossible = impossible\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // Note (Curtis, 2-9-19): Generalization is not broken, this is related to making all local bindings default to being monomorphic a la Simon Peyton Jones recommendation.
    // // This seems broken. Should trigger monomorphism restriction, and it's not
    // {
    //     const char* test_name = "PolymorphicPatternBinding";
    //     const char* test_source = ""
    //         "polyBad1 :: Bool\n"
    //         "polyBad1 = y where\n"
    //         "  x = Nothing\n"
    //         "  y = True\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    // Note (Curtis, 2-9-19): Generalization is not broken, this is related to making all local bindings default to being monomorphic a la Simon Peyton Jones recommendation.
    // {
    //     const char* test_name = "PolymorphicPatternBinding";
    //     const char* test_source = ""
    //         "polyBad2 :: Bool\n"
    //         "polyBad2 = y where\n"
    //         "  (x, y) = (Nothing, True)\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    {
        const char* test_name = "MistmatchedType: SimpleAssignment";
        const char* test_source = ""
            "data Book = Pages\n"
            "data NotBook = EmptyPages\n"
            "notcronomicon :: Maybe Book\n"
            "notcronomicon = Just EmptyPages\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name   = "MistmatchedType: Initializer";
        const char* test_source = "looper ~ () = looper && True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: ApatsAssignment";
        const char* test_source = ""
            "zeroIsNotNothing :: a -> Int\n"
            "zeroIsNotNothing z = Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: PatAssignment";
        const char* test_source = ""
            "earth :: Bool\n"
            "earth = toTheMoon where\n"
            "  toTheMoon :: Bool\n"
            "  (toTheSun, toTheMoon) = ((), Just True)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Array";
        const char* test_source = ""
            "notLikeTheOthers :: Array Bool\n"
            "notLikeTheOthers = { True, False, () }\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: List";
        const char* test_source = ""
            "notLikeTheOthers :: [()]\n"
            "notLikeTheOthers = [ (), False, () ]\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: fexpr";
        const char* test_source = ""
            "unitsForDays :: () -> ()\n"
            "unitsForDays x = x\n"
            "notAUnit :: Bool\n"
            "notAUnit = unitsForDays True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: fexpr2";
        const char* test_source = ""
            "unitsForDays :: () -> Maybe Bool -> ()\n"
            "unitsForDays x _ = x\n"
            "notAUnit :: Bool\n"
            "notAUnit = unitsForDays () True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: fexpr3";
        const char* test_source = ""
            "notAFunction :: ()\n"
            "notAFunction = ()\n"
            "unity :: ()\n"
            "unity = notAFunction () 0\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: if1";
        const char* test_source = ""
            "logicDied :: Maybe ()\n"
            "logicDied = if () then Just () else Nothing\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: if2";
        const char* test_source = ""
            "logicDiedAgain :: Maybe ()\n"
            "logicDiedAgain = if False then Just True else Just ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: BinOp";
        const char* test_source = ""
            "andIsPretend :: Bool\n"
            "andIsPretend = True && ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: LeftSection";
        const char* test_source = ""
            "chopItOff :: Bool -> Bool\n"
            "chopItOff = (() ||)\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: RightSection";
        const char* test_source = ""
            "noTheOtherOne :: Bool -> Bool\n"
            "noTheOtherOne = (|| ())\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats1";
        const char* test_source = ""
            "atTheApatsOfMadness :: Bool -> Bool\n"
            "atTheApatsOfMadness () = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats2";
        const char* test_source = ""
            "theCallOfWrongthulhu :: Int -> Int -> Bool\n"
            "theCallOfWrongthulhu x () = False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats3";
        const char* test_source = ""
            "data OnlyLogic = OnlyLogic Bool\n"
            "cantDoAnythingRight :: OnlyLogic -> Bool\n"
            "cantDoAnythingRight OnlyLogic = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats4";
        const char* test_source = ""
            "data DeathMayDie = DeathMayDie Bool ()\n"
            "eternalLies :: DeathMayDie -> Bool\n"
            "eternalLies (DeathMayDie x) = x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats5";
        const char* test_source = ""
            "wrongzathoth :: Int -> Int -> Bool\n"
            "wrongzathoth x = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Apats6";
        const char* test_source = ""
            "wrongzathoth2 :: Int -> Bool\n"
            "wrongzathoth2 x y z = True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Case1";
        const char* test_source = ""
            "notAGoodCase :: Bool\n"
            "notAGoodCase = case True of\n"
            "  True  -> ()\n"
            "  False -> True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Case2";
        const char* test_source = ""
            "fubar :: Bool\n"
            "fubar = case True of\n"
            "  ()    -> False\n"
            "  False -> True\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: Case3";
        const char* test_source = ""
            "rofl :: Bool\n"
            "rofl = case True of\n"
            "  True  -> False\n"
            "  False -> ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: ArithmeticSequence1";
        const char* test_source = ""
            "sequencer1 :: [Int]\n"
            "sequencer1 = [True..10]\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // TODO (Curtis, 2-8-19): Something wonky with this. However arithmetic sequences are a "nice to have"...
    // {
    //     const char* test_name = "MistmatchedType: ArithmeticSequence3";
    //     const char* test_source = ""
    //         "sequencer3 :: [Int]\n"
    //         "sequencer3 = [1, True..10]\n";
    //     const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
    //     const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
    //     necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    // }

    {
        const char* test_name = "MistmatchedType: ArithmeticSequence2";
        const char* test_source = ""
            "sequencer2 :: [Int]\n"
            "sequencer2 = [1..()]\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    // TODO (Curtis, 2-8-19): Do statements rely heavily on type class machinery behaving. Revisit once it's sorted.
    {
        const char* test_name = "MistmatchedType: do1";
        const char* test_source = ""
            "wrongnad :: Monad m => m Bool\n"
            "wrongnad = do\n"
            "  x <- pure ()\n"
            "  pure x\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "MistmatchedType: do2";
        const char* test_source = ""
            "wrongnad2 :: Monad m => m Bool\n"
            "wrongnad2 = do\n"
            "  pure ()\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_MISMATCHED_TYPE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

    {
        const char* test_name = "Occurs1";
        const char* test_source = ""
            "caughtInATimeLoop ~ Nothing = Just caughtInATimeLoop\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error      = NECRO_TYPE_OCCURS;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
    }

#if 0 // This crashes right now during inference :(
    {
        const char* test_name = "RigidTypeVariable";
        const char* test_source = ""
            "data Book = Pages\n"
            "class Fail a where result :: a -> a\n"
            "x :: Fail a => a\n"
            "x = Pages\n";
        const NECRO_RESULT_TYPE expect_error_result = NECRO_RESULT_ERROR;
        const NECRO_RESULT_ERROR_TYPE expected_error = NECRO_TYPE_RIGID_TYPE_VARIABLE;
        necro_infer_test_result(test_name, test_source, expect_error_result, &expected_error);
}
#endif

    ///////////////////////////////////////////////////////
    // OK tests
    ///////////////////////////////////////////////////////
    // Ok test, needs to match on AST
    {
        const char* test_name = "GeneralizationIsOk1";
        const char* test_source = ""
            "atTheMountainsOfOK = False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // Ok test, needs to match on AST
    {
        const char* test_name = "GeneralizationIsOk2";
        const char* test_source = ""
            "atTheMountainsOfOK2 x = False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    // Ok test, needs to match on AST
    {
        const char* test_name = "GeneralizationIsOk3";
        const char* test_source = ""
            "atTheMountainsOfOK3 x () = False\n";
            "okggoth :: "
            "atTheMountainsOfOK3 x () = False\n";
        const NECRO_RESULT_TYPE       expect_error_result = NECRO_RESULT_OK;
        necro_infer_test_result(test_name, test_source, expect_error_result, NULL);
    }

    /*
    FINISHED:
    NECRO_TYPE_UNINITIALIZED_RECURSIVE_VALUE,
    NECRO_TYPE_MISMATCHED_TYPE, (In all its various forms...Needs clean up work)

    TODO:
    NECRO_TYPE_RIGID_TYPE_VARIABLE,
    NECRO_TYPE_NOT_AN_INSTANCE_OF,
    NECRO_TYPE_OCCURS,
    NECRO_TYPE_MISMATCHED_ARITY,
    NECRO_TYPE_POLYMORPHIC_PAT_BIND,
    NECRO_TYPE_FINAL_DO_STATEMENT,
    NECRO_TYPE_AMBIGUOUS_CLASS,
    NECRO_TYPE_CONSTRAINS_ONLY_CLASS_VAR,
    NECRO_TYPE_AMBIGUOUS_TYPE_VAR,
    NECRO_TYPE_NON_CONCRETE_INITIALIZED_VALUE,
    NECRO_TYPE_NON_RECURSIVE_INITIALIZED_VALUE,
    NECRO_TYPE_NOT_A_CLASS,
    NECRO_TYPE_NOT_A_VISIBLE_METHOD,
    NECRO_TYPE_NO_EXPLICIT_IMPLEMENTATION,
    NECRO_TYPE_DOES_NOT_IMPLEMENT_SUPER_CLASS,
    NECRO_TYPE_MULTIPLE_CLASS_DECLARATIONS,
    NECRO_TYPE_MULTIPLE_INSTANCE_DECLARATIONS,
    */
}
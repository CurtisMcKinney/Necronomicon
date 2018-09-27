/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "prim.h"
#include "type_class.h"
#include "kind.h"
#include "infer.h"
#include "base.h"

#define NECRO_INFER_DEBUG           1
#if NECRO_INFER_DEBUG
#define TRACE_INFER(...) printf(__VA_ARGS__)
#else
#define TRACE_INFER(...)
#endif

// Forward declarations
NecroType* necro_infer_go(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_apat(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_pattern(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_constant(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_var(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_tuple_type(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_simple_assignment(NecroInfer* infer, NecroAst* ast);
void       necro_pat_new_name_go(NecroInfer* infer, NecroAst* ast);
void       necro_gen_pat_go(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_pat_assignment(NecroInfer* infer, NecroAst* ast);
NecroType* necro_infer_apats_assignment(NecroInfer* infer, NecroAst* ast);

//=====================================================
// TypeSig
//=====================================================
NecroType* necro_ast_to_type_sig_go(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    if (necro_is_infer_error(infer)) return NULL;
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:      return necro_type_var_create(infer->arena, ast->variable.ast_symbol);
    case NECRO_AST_TUPLE:         return necro_infer_tuple_type(infer, ast);
    case NECRO_AST_FUNCTION_TYPE:
    {
        NecroType* fun_type = necro_type_fn_create(infer->arena, necro_ast_to_type_sig_go(infer, ast->function_type.type), necro_ast_to_type_sig_go(infer, ast->function_type.next_on_arrow));
        ast->necro_type = fun_type;
        return fun_type;
    }
    case NECRO_AST_CONID:
    {
        NecroType* env_con_type = ast->conid.ast_symbol->type;
        NecroType* con_type     = necro_type_con_create(infer->arena, env_con_type->con.con_symbol, NULL, env_con_type->con.arity);
        assert(con_type != NULL);
        return con_type;
    }
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroType* con_args = NULL;
        NecroAst*  arg_list = ast->constructor.arg_list;
        size_t arity = 0;
        while (arg_list != NULL)
        {
            if (con_args == NULL)
                con_args = necro_type_list_create(infer->arena, necro_ast_to_type_sig_go(infer, arg_list->list.item), NULL);
            else
                con_args->list.next = necro_type_list_create(infer->arena, necro_ast_to_type_sig_go(infer, arg_list->list.item), NULL);
            arg_list = arg_list->list.next_item;
            arity++;
        }
        NecroType* env_con_type = ast->constructor.conid->conid.ast_symbol->type;
        NecroType* con_type     = necro_type_con_create(infer->arena, env_con_type->con.con_symbol, con_args, env_con_type->con.arity);
        assert(con_type != NULL);
        return con_type;
    }
    // Perhaps this is fucked!?
    case NECRO_AST_TYPE_APP:
    {
        NecroType* left   = necro_ast_to_type_sig_go(infer, ast->type_app.ty);
        NecroType* right  = necro_ast_to_type_sig_go(infer, ast->type_app.next_ty);
        if (necro_is_infer_error(infer)) return NULL;
        left->source_loc  = ast->source_loc;
        right->source_loc = ast->source_loc;
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
            return left;
        }
        else
        {
            return necro_type_app_create(infer->arena, left, right);
        }
    }
    default:
        assert(false);
        return NULL;
    }
}

NecroType* necro_infer_type_sig(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TYPE_SIGNATURE);
    if (necro_is_infer_error(infer)) return NULL;

    NecroType* type_sig = necro_ast_to_type_sig_go(infer, ast->type_signature.type);
    if (necro_is_infer_error(infer)) return NULL;

    NecroTypeClassContext* context = necro_ast_to_context(infer, ast->type_signature.context);
    if (necro_is_infer_error(infer)) return NULL;
    context = necro_union_contexts(infer->arena, context, NULL);
    if (necro_is_infer_error(infer)) return NULL;
    if (necro_ambiguous_type_class_check(ast->type_signature.var->variable.ast_symbol, context, type_sig)) return NULL;
    necro_apply_constraints(infer->arena, type_sig, context);

    type_sig = necro_type_generalize(infer, type_sig, ast->type_signature.type->scope);
    if (necro_is_infer_error(infer)) return NULL;

    // Kind inference
    necro_kind_infer(infer, type_sig, type_sig, "While inferring the type of a type signature");
    if (necro_is_infer_error(infer)) return NULL;
    type_sig->kind = necro_kind_gen(infer, type_sig->kind);
    if (necro_is_infer_error(infer)) return NULL;
    // necro_print_type_sig(type_sig, infer->intern);
    // necro_print_type_sig(type_sig->type_kind, infer->intern);
    necro_kind_unify(infer, type_sig->kind, infer->base->star_kind->type, ast->scope, type_sig, "While inferring the type of a type signature");
    if (necro_is_infer_error(infer)) return NULL;

    // TODO: Finish!
    // kind check for type context!
    NecroTypeClassContext* curr_context = context;
    while (curr_context != NULL)
    {
        NecroTypeClass* type_class     = curr_context->type_class;
        NecroType*      type_class_var = type_class->type_var->type;
        NecroType*      var_type       = curr_context->var_symbol->type;
        necro_kind_unify(infer, var_type->kind, type_class_var->kind, ast->scope, type_sig, "While inferring the type of a type signature");
        if (necro_is_infer_error(infer)) return NULL;
        curr_context = curr_context->next;
    }

    type_sig->pre_supplied = true;
    type_sig->source_loc   = ast->source_loc;

    // necro_symtable_get(infer->symtable, ast->type_signature.var->variable.id)->type = type_sig;
    ast->type_signature.var->variable.ast_symbol->type = type_sig;
    ast->necro_type                                             = type_sig;
    return NULL;
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

NecroType* necro_create_data_constructor(NecroInfer* infer, NecroAst* ast, NecroType* data_type, size_t con_num)
{
    NecroType* con_head = NULL;
    NecroType* con_args = NULL;
    NecroAst*  args_ast = ast->constructor.arg_list;
    size_t     count    = 0;
    while (args_ast != NULL)
    {
        NecroType* arg = necro_ast_to_type_sig_go(infer, args_ast->list.item);
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
        if (necro_is_infer_error(infer)) return NULL;
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
    // necro_kind_infer(infer, con_type, con_type, "While declaring a data constructor");
    // necro_kind_unify(infer, con_type->type_kind, infer->star_type_kind, NULL, con_type, "During a data declaration: ");
    con_type = necro_type_generalize(infer, con_type, NULL);
    if (necro_is_infer_error(infer)) return NULL;
    con_type->source_loc   = ast->source_loc;
    con_type->pre_supplied = true;
    ast->constructor.conid->conid.ast_symbol->type           = con_type;
    ast->constructor.conid->conid.ast_symbol->is_constructor = true;
    ast->constructor.conid->conid.ast_symbol->con_num        = con_num;
    necro_kind_infer(infer, con_type, con_type, "While declaring a data constructor");
    necro_kind_unify(infer, con_type->kind, infer->base->star_kind->type, NULL, con_type, "During a data declaration: ");
    ast->necro_type = con_type;
    return con_type;
}

NecroType* necro_infer_simple_type(NecroInfer* infer, NecroAst* ast, NecroAst* data_declaration_ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_TYPE);
    if (necro_is_infer_error(infer)) return NULL;
    // Higher-kinded type goes in symtable
    // necro_declare_type(infer, (NecroCon) { .symbol = ast->simple_type.type_con->conid.symbol, .id = ast->simple_type.type_con->conid.id.id }, necro_count_ty_vars(ast->simple_type.type_var_list));
    // if (necro_is_infer_error(infer)) return NULL;
    // Fully applied type used for Data Constructor declaration
    NecroType* type    = necro_type_con_create(infer->arena, ast->simple_type.type_con->conid.ast_symbol, necro_ty_vars_to_args(infer, ast->simple_type.type_var_list), necro_count_ty_vars(ast->simple_type.type_var_list));
    type->source_loc   = ast->source_loc;
    type->pre_supplied = true;
    ast->simple_type.type_con->conid.ast_symbol->type = type;
    ast->simple_type.type_con->conid.ast_symbol->ast  = data_declaration_ast;
    // if (necro_is_infer_error(infer)) return NULL;
    // necro_infer_kind(infer, type, infer->base->star_kind->type, type, "During data declaration");
    if (necro_is_infer_error(infer)) return NULL;
    // necro_symtable_get(infer->symtable, ast->simple_type.type_con->conid.id)->type->type_kind = necro_new_name(infer->arena, ast->source_loc);
    // necro_symtable_get(infer->symtable, ast->simple_type.type_con->conid.id)->type->type_kind = necro_new_name(infer->arena, ast->source_loc);
    necro_kind_infer(infer, type, type, "During data declaration");
    assert(type->kind != NULL);
    ast->necro_type = type;
    // necro_kind_unify(infer, type->type_kind, infer->star_type_kind, NULL, type, "During a data declaration: ");
    // necro_print_type_sig(necro_symtable_get(infer->symtable, ast->simple_type.type_con->conid.id)->type->type_kind, infer->intern);
    // TODO: data declarations in declaration groups, infer kinds everywhere, clean up what will be broken, add missing shit, etc...
    return type;
}

NecroType* necro_infer_apats_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_APATS_ASSIGNMENT);
    if (necro_is_infer_error(infer)) return NULL;
    NecroAst*  apats  = ast->apats_assignment.apats;
    NecroType* f_type = NULL;
    NecroType* f_head = NULL;
    if (necro_is_infer_error(infer)) return NULL;
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        if (necro_is_infer_error(infer)) return NULL;
        if (f_type == NULL)
        {
            f_type = necro_type_fn_create(infer->arena, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_type_fn_create(infer->arena, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_type            = f_type->fun.type2;
        }
        apats = apats->apats.next_apat;
    }
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* proxy_type = ast->apats_assignment.ast_symbol->type;
    NecroType* rhs        = necro_infer_go(infer, ast->apats_assignment.rhs);
    f_type->fun.type2     = rhs;
    // necro_infer_kind(infer, f_head, infer->base->star_kind->type, f_head, "While inferring the type of a function declaration: ");
    // necro_kind_infer(infer, f_head, f_head, "While inferring the type of a function declaration: ");
    // necro_kind_unify(infer, f_head->type_kind, infer->star_type_kind, NULL, f_head, "While inferring the type of a function declaration: ");
    necro_type_unify(infer, proxy_type, f_head, ast->scope, proxy_type, "While inferring the type of a function declaration: ");
    ast->necro_type = f_head;
    return NULL;
}

NecroType* necro_infer_simple_assignment(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_SIMPLE_ASSIGNMENT);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* proxy_type = ast->simple_assignment.ast_symbol->type;
    NecroType* init_type  = (ast->simple_assignment.initializer != NULL) ? necro_infer_go(infer, ast->simple_assignment.initializer) : NULL;
    NecroType* rhs_type   = necro_infer_go(infer, ast->simple_assignment.rhs);
    if (ast->simple_assignment.declaration_group != NULL && ast->simple_assignment.declaration_group->next != NULL)
        ast->simple_assignment.is_recursive = true;
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    necro_type_unify(infer, proxy_type, rhs_type, ast->scope, proxy_type, "While inferring the type of an assignment: ");
    if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    if (init_type != NULL)
    {
        necro_type_unify(infer, proxy_type, init_type, ast->scope, proxy_type, "While inferring the type of an assignment initializer: ");
        if (infer->error.return_code != NECRO_SUCCESS) return NULL;
    }
    ast->necro_type = rhs_type;
    return NULL;
}

void necro_ensure_pat_binding_is_monomorphic(NecroInfer* infer, NecroAst* ast)
{
    if (necro_is_infer_error(infer)) return;
    assert(infer != NULL);
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
        // TODO: NecroResultError
        // if (necro_symtable_get(infer->symtable, ast->variable.id)->type->type == NECRO_TYPE_FOR)
        //     necro_infer_ast_error(infer, necro_symtable_get(infer->symtable, ast->variable.id)->type, ast, "Type error: Polymorphic pattern bindings are illegal.");
        return;
    case NECRO_AST_CONID:    return;
    case NECRO_AST_CONSTANT: return;
    case NECRO_AST_WILDCARD: return;
    case NECRO_AST_TUPLE:
    {
        NecroAst* tuple = ast->tuple.expressions;
        while (tuple != NULL)
        {
            necro_ensure_pat_binding_is_monomorphic(infer, tuple->list.item);
            if (necro_is_infer_error(infer)) return;
            tuple = tuple->list.next_item;
        }
        return;
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* list = ast->expression_list.expressions;
        while (list != NULL)
        {
            necro_ensure_pat_binding_is_monomorphic(infer, list->list.item);
            if (necro_is_infer_error(infer)) return;
            list = list->list.next_item;
        }
        return;
    }

    case NECRO_AST_BIN_OP_SYM:
        necro_ensure_pat_binding_is_monomorphic(infer, ast->bin_op_sym.left);
        if (necro_is_infer_error(infer)) return;
        necro_ensure_pat_binding_is_monomorphic(infer, ast->bin_op_sym.right);
        return;

    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_ensure_pat_binding_is_monomorphic(infer, args->list.item);
            if (necro_is_infer_error(infer)) return;
            args = args->list.next_item;
        }
        return;
    }

    default: assert(false && "Compiler bug: Unimplemented pattern in necro_ensure_pat_binding_is_monomorphic: %d"); return;
    }
}

NecroType* necro_infer_pat_assignment(NecroInfer* infer, NecroAst* ast)
{
    if (necro_is_infer_error(infer)) return NULL;
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_PAT_ASSIGNMENT);
    assert(ast->pat_assignment.pat != NULL);
    necro_ensure_pat_binding_is_monomorphic(infer, ast->pat_assignment.pat);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* pat_type = necro_infer_apat(infer, ast->pat_assignment.pat);
    if (necro_is_infer_error(infer)) return NULL;
    NecroType* rhs_type = necro_infer_go(infer, ast->pat_assignment.rhs);
    if (necro_is_infer_error(infer)) return NULL;
    necro_type_unify(infer, pat_type, rhs_type, ast->scope, rhs_type, "While inferring the type of a pattern declaration: ");
    if (necro_is_infer_error(infer)) return NULL;
    necro_ensure_pat_binding_is_monomorphic(infer, ast->pat_assignment.pat);
    if (necro_is_infer_error(infer)) return NULL;
    ast->necro_type = pat_type;
    return NULL;
}

void necro_pat_new_name_go(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    if (necro_is_infer_error(infer)) return;
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        NecroAstSymbol* data = ast->variable.ast_symbol;
        NecroType*      type = data->type;
        if (type == NULL)
        {
            NecroType* new_name  = necro_type_fresh_var(infer->arena);
            new_name->source_loc = ast->source_loc;
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
    default: assert(false && "Unimplemented pattern in type inference during necro_pat_new_name_go: %d"); return;
    }
}

void necro_gen_pat_go(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    if (necro_is_infer_error(infer)) return;
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:
    {
        // TODO: Take a closer look at this!!!!
        // NecroSymbolInfo* symbol_info = necro_symtable_get(infer->symtable, ast->variable.id);
        NecroAstSymbol* data = ast->variable.ast_symbol;
        if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE)
        {
            data->type_status = NECRO_TYPE_DONE;
            return;
        }
        // Monomorphism restriction
        // infer->symtable->data[id.id].type                      = necro_gen(infer, proxy_type, infer->symtable->data[id.id].scope->parent);
        data->type_status = NECRO_TYPE_DONE;
        necro_kind_infer(infer, data->type, data->type, "While declaring a pattern variable: ");
        data->type->kind = necro_kind_gen(infer, data->type->kind);
        necro_kind_unify(infer, data->type->kind, infer->base->star_kind->type, NULL, data->type, "While declaring a pattern variable: ");
        return;
    }
    case NECRO_AST_CONSTANT: return;
    case NECRO_AST_TUPLE:
    {
        NecroAst* args = ast->tuple.expressions;
        while (args != NULL)
        {
            necro_gen_pat_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    case NECRO_AST_EXPRESSION_LIST:
    {
        NecroAst* args = ast->expression_list.expressions;
        while (args != NULL)
        {
            necro_gen_pat_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    case NECRO_AST_WILDCARD: return;
    case NECRO_AST_BIN_OP_SYM:
    {
        necro_gen_pat_go(infer, ast->bin_op_sym.left);
        necro_gen_pat_go(infer, ast->bin_op_sym.right);
        return;
    }
    case NECRO_AST_CONID: return;
    case NECRO_AST_CONSTRUCTOR:
    {
        NecroAst* args = ast->constructor.arg_list;
        while (args != NULL)
        {
            necro_gen_pat_go(infer, args->list.item);
            args = args->list.next_item;
        }
        return;
    }
    default: assert(false && "Unimplemented pattern in type inference during pattern generalization: %d"); return;
    }
}

//=====================================================
// Variable
//=====================================================
NecroType* necro_infer_var(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_VARIABLE);
    NecroAstSymbol* data = ast->variable.ast_symbol;
    assert(data != NULL);

    // assert?
    if (data->declaration_group != NULL)
        assert(data->type != NULL);

    if (ast->variable.var_type == NECRO_VAR_VAR && data->declaration_group != NULL && data->type_status != NECRO_TYPE_DONE && data->ast != NULL)
    {
        if (data->ast->type == NECRO_AST_SIMPLE_ASSIGNMENT && data->ast->simple_assignment.initializer == NULL)
        {
            // TODO: NecroResultError
            return NULL;
            // return necro_infer_ast_error(infer, NULL, ast, "%s cannot depend on itself without an initial value.\n Consider adding an initial value, such as:\n\n     %s ~ InitialValue\n",
            //     ast->variable.symbol->str,
            //     ast->variable.symbol->str);
        }
        else if (data->ast->type == NECRO_AST_VARIABLE && data->ast->variable.var_type == NECRO_VAR_DECLARATION)
        {
            if (data->ast->variable.initializer == NULL)
            {
                // TODO: NecroResultError
                return NULL;
                // return necro_infer_ast_error(infer, NULL, ast, "%s cannot depend on itself without an initial value.\n Consider adding an initial value, such as:\n\n     %s ~ InitialValue\n",
                //     ast->variable.symbol->str,
                //     ast->variable.symbol->str);
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
        if (ast->variable.initializer != NULL)
        {
            NecroType* init_type = necro_infer_go(infer, ast->variable.initializer);
            necro_type_unify(infer, ast->necro_type, init_type, ast->scope, ast->necro_type, "While type checking variable initializer");
        }
        return data->type;
    }
    else if (necro_type_is_bound_in_scope(data->type, ast->scope))
    {
        ast->necro_type = data->type;
        if (ast->variable.initializer != NULL)
        {
            NecroType* init_type = necro_infer_go(infer, ast->variable.initializer);
            necro_type_unify(infer, ast->necro_type, init_type, ast->scope, ast->necro_type, "While type checking variable initializer");
        }
        return data->type;
    }
    else
    {
        ast->variable.inst_context = NULL;
        NecroType* inst_type       = necro_type_instantiate_with_context(infer, data->type, ast->scope, &ast->variable.inst_context);
        ast->necro_type            = inst_type;
        if (ast->variable.initializer != NULL)
        {
            NecroType* init_type = necro_infer_go(infer, ast->variable.initializer);
            necro_type_unify(infer, ast->necro_type, init_type, ast->scope, ast->necro_type, "While type checking variable initializer");
        }
        return inst_type;
    }
}

//=====================================================
// Constant
//=====================================================
NecroType* necro_infer_constant(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONSTANT);
    if (necro_is_infer_error(infer)) return NULL;
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
    default: assert(false && "Compiler bug: Unrecognized constant type: %d"); return NULL;
    }
}

//=====================================================
// ConID
//=====================================================
NecroType* necro_infer_conid(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CONID);
    NecroType* con_type = ast->variable.ast_symbol->type;
    assert(con_type != NULL);
    NecroType* inst_type = necro_type_instantiate(infer, con_type, ast->scope);
    ast->necro_type = inst_type;
    assert(inst_type != NULL);
    return inst_type;
}

//=====================================================
// Wildcard
//=====================================================
NecroType* necro_infer_wildcard(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_WILDCARD);
    NecroType* type = necro_type_fresh_var(infer->arena);
    ast->necro_type = type;
    return type;
}

//=====================================================
// Tuple
//=====================================================
NecroType* necro_infer_tuple(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, necro_infer_go(infer, current_expression->list.item), NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, necro_infer_go(infer, current_expression->list.item), NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    NecroType* tuple = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    ast->necro_type  = tuple;
    return tuple;
}

NecroType* necro_infer_tuple_pattern(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    NecroAst* current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, necro_infer_pattern(infer, current_expression->list.item), NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, necro_infer_pattern(infer, current_expression->list.item), NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    NecroType* tuple  = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    ast->necro_type   = tuple;
    tuple->source_loc = ast->source_loc;
    return tuple;
}

NecroType* necro_infer_tuple_type(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_TUPLE);
    if (necro_is_infer_error(infer)) return NULL;
    NecroAst*  current_expression = ast->tuple.expressions;
    NecroType* types_head         = NULL;
    NecroType* tuple_types        = NULL;
    while (current_expression != NULL)
    {
        assert(current_expression->type == NECRO_AST_LIST_NODE);
        if (tuple_types == NULL)
        {
            tuple_types = necro_type_list_create(infer->arena, necro_ast_to_type_sig_go(infer, current_expression->list.item), NULL);
            types_head  = tuple_types;
        }
        else
        {
            tuple_types->list.next = necro_type_list_create(infer->arena, necro_ast_to_type_sig_go(infer, current_expression->list.item), NULL);
            tuple_types = tuple_types->list.next;
        }
        current_expression = current_expression->list.next_item;
    }
    NecroType* tuple = necro_type_tuple_con_create(infer->arena, infer->base, types_head);
    ast->necro_type  = tuple;
    return tuple;
}

///////////////////////////////////////////////////////
// Expression Array
///////////////////////////////////////////////////////
NecroType* necro_infer_expression_array(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_ARRAY);
    NecroAst*  current_cell  = ast->expression_array.expressions;
    NecroType* element_type  = necro_type_fresh_var(infer->arena);
    element_type->source_loc = ast->source_loc;
    while (current_cell != NULL)
    {
        necro_type_unify(infer, element_type, necro_infer_go(infer, current_cell->list.item), ast->scope, element_type, "While inferring the type for an array: ");
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    NecroType* array_type = necro_type_con1_create(infer->arena, infer->base->array_type, element_type);
    ast->necro_type       = array_type;
    return array_type;
}

//=====================================================
// Expression List
//=====================================================
NecroType* necro_infer_expression_list(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    if (necro_is_infer_error(infer)) return NULL;
    NecroAst* current_cell = ast->expression_list.expressions;
    NecroType* list_type   = necro_type_fresh_var(infer->arena);
    list_type->source_loc  = ast->source_loc;
    while (current_cell != NULL)
    {
        necro_type_unify(infer, list_type, necro_infer_go(infer, current_cell->list.item), ast->scope, list_type, "While inferring the type for a list []: ");
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    NecroType* list = necro_type_con1_create(infer->arena, infer->base->list_type, list_type);
    ast->necro_type = list;
    return list;
}

NecroType* necro_infer_expression_list_pattern(NecroInfer* infer, NecroAst* ast)
{
    if (necro_is_infer_error(infer)) return NULL;
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_EXPRESSION_LIST);
    NecroAst*  current_cell = ast->expression_list.expressions;
    NecroType* list_type    = necro_type_fresh_var(infer->arena);
    while (current_cell != NULL)
    {
        necro_type_unify(infer, list_type, necro_infer_pattern(infer, current_cell->list.item), ast->scope, list_type, "While inferring the type for a list [] pattern: ");
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    NecroType* list = necro_type_con1_create(infer->arena, infer->base->list_type, list_type);
    ast->necro_type = list;
    return list;
}

//=====================================================
// Pat Expression
//=====================================================
NecroType* necro_infer_pat_expression(NecroInfer* infer, NecroAst* ast)
{
    if (necro_is_infer_error(infer)) return NULL;
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_PAT_EXPRESSION);

    NecroAst* current_cell = ast->pattern_expression.expressions;
    NecroType* pat_type     = necro_type_fresh_var(infer->arena);
    pat_type                = necro_type_con1_create(infer->arena, infer->base->pattern_type, pat_type);
    pat_type->source_loc    = ast->source_loc;
    while (current_cell != NULL)
    {
        necro_type_unify(infer, pat_type, necro_infer_go(infer, current_cell->list.item), ast->scope, pat_type, "While inferring the type for a \'pat\' expression: ");
        if (necro_is_infer_error(infer)) return NULL;
        current_cell = current_cell->list.next_item;
    }
    ast->necro_type = pat_type;
    return pat_type;
}

//=====================================================
// Function Expression
//=====================================================
NecroType* necro_infer_fexpr(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_FUNCTION_EXPRESSION);
    NecroType* e0_type     = necro_infer_go(infer, ast->fexpression.aexp);
    NecroType* e1_type     = necro_infer_go(infer, ast->fexpression.next_fexpression);
    NecroType* result_type = necro_type_fresh_var(infer->arena);
    NecroType* f_type      = necro_type_fn_create(infer->arena, e1_type, result_type);
    necro_type_unify(infer, e0_type, f_type, ast->scope, f_type, "While inferring the type for a function application: ");
    ast->necro_type        = result_type;
    return result_type;
}

//=====================================================
// If then else
//=====================================================
NecroType* necro_infer_if_then_else(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_IF_THEN_ELSE);
    NecroType* if_type   = necro_infer_go(infer, ast->if_then_else.if_expr);
    NecroType* then_type = necro_infer_go(infer, ast->if_then_else.then_expr);
    NecroType* else_type = necro_infer_go(infer, ast->if_then_else.else_expr);
    necro_type_unify(infer, if_type, infer->base->bool_type->type, ast->scope, if_type, "While inferring the type of an if/then/else expression: ");
    necro_type_unify(infer, then_type, else_type, ast->scope, then_type, "While inferring the type of an if/then/else expression: ");
    ast->necro_type = then_type;
    return then_type;
}

//=====================================================
// BinOp
//=====================================================
NecroType* necro_infer_bin_op(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_BIN_OP);
    NecroType* x_type        = necro_infer_go(infer, ast->bin_op.lhs);
    ast->bin_op.inst_context = NULL;
    NecroType* op_type       = necro_type_instantiate_with_context(infer, ast->bin_op.ast_symbol->type, ast->scope, &ast->bin_op.inst_context);
    assert(op_type != NULL);
    NecroType* y_type        = necro_infer_go(infer, ast->bin_op.rhs);
    NecroType* result_type   = necro_type_fresh_var(infer->arena);
    NecroType* bin_op_type   = necro_type_fn_create(infer->arena, x_type, necro_type_fn_create(infer->arena, y_type, result_type));
    necro_type_unify(infer, op_type, bin_op_type, ast->scope, op_type, "While inferring the type of a bin-op: ");
    ast->necro_type          = bin_op_type;
    return result_type;
}

//=====================================================
// Operator Left Section
//=====================================================
NecroType* necro_infer_op_left_section(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_OP_LEFT_SECTION);
    NecroType* x_type                  = necro_infer_go(infer, ast->op_left_section.left);
    ast->op_left_section.inst_context  = NULL;
    NecroType* op_type                 = necro_type_instantiate_with_context(infer, ast->op_left_section.ast_symbol->type, ast->scope, &ast->op_left_section.inst_context);
    assert(op_type != NULL);
    ast->op_left_section.op_necro_type = op_type;
    NecroType* result_type             = necro_type_fresh_var(infer->arena);
    NecroType* section_type            = necro_type_fn_create(infer->arena, x_type, result_type);
    necro_type_unify(infer, op_type, section_type, ast->scope, op_type, "While inferring the type of a bin-op left section: ");
    ast->necro_type                    = section_type;
    return result_type;
}

//=====================================================
// Operator Right Section
//=====================================================
NecroType* necro_infer_op_right_section(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_OP_RIGHT_SECTION);
    ast->op_right_section.inst_context  = NULL;
    NecroType* op_type                  = necro_type_instantiate_with_context(infer, ast->op_right_section.ast_symbol->type, ast->scope, &ast->op_right_section.inst_context);
    assert(op_type != NULL);
    ast->op_right_section.op_necro_type = op_type;
    NecroType* y_type                   = necro_infer_go(infer, ast->op_right_section.right);
    NecroType* x_type                   = necro_type_fresh_var(infer->arena);
    NecroType* result_type              = necro_type_fresh_var(infer->arena);
    NecroType* bin_op_type              = necro_type_fn_create(infer->arena, x_type, necro_type_fn_create(infer->arena, y_type, result_type));
    necro_type_unify(infer, op_type, bin_op_type, ast->scope, op_type, "While inferring the type of a bin-op: ");
    ast->necro_type                     = necro_type_fn_create(infer->arena, x_type, result_type);
    return ast->necro_type;
}

//=====================================================
// Apat
//=====================================================
NecroType* necro_infer_apat(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    switch (ast->type)
    {
    case NECRO_AST_VARIABLE:        return necro_infer_var(infer, ast);
    case NECRO_AST_TUPLE:           return necro_infer_tuple_pattern(infer, ast);
    case NECRO_AST_EXPRESSION_LIST: return necro_infer_expression_list_pattern(infer, ast);
    case NECRO_AST_WILDCARD:        return necro_type_fresh_var(infer->arena);
    case NECRO_AST_CONSTANT:        return necro_infer_constant(infer, ast);

    case NECRO_AST_BIN_OP_SYM:
    {
        assert(ast->bin_op_sym.op->type == NECRO_AST_CONID);
        NecroType* constructor_type = ast->bin_op_sym.op->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type      = necro_type_instantiate(infer, constructor_type, NULL);
        NecroType* left_type  = necro_infer_apat(infer, ast->bin_op_sym.left);
        NecroType* right_type = necro_infer_apat(infer, ast->bin_op_sym.right);
        NecroType* data_type  = necro_type_fresh_var(infer->arena);
        NecroType* f_type     = necro_type_fn_create(infer->arena, left_type, necro_type_fn_create(infer->arena, right_type, data_type));
        necro_type_unify(infer, constructor_type, f_type, ast->scope, constructor_type, "While inferring the type of a bin-op pattern: ");
        ast->necro_type       = f_type;
        return data_type;
    }

    case NECRO_AST_CONID:
    {
        NecroType* constructor_type  = ast->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type             = necro_type_instantiate(infer, constructor_type, NULL);
        size_t     constructor_args  = 0;
        NecroType* con_iter          = constructor_type;
        while (con_iter->type == NECRO_TYPE_FUN)
        {
            constructor_args++;
            con_iter = con_iter->fun.type2;
        }
        NecroType* type = necro_infer_conid(infer, ast);
        if (constructor_args != 0)
            return necro_infer_ast_error(infer, type, ast, "Wrong number of arguments for constructor %s. Expected arity: %d, found arity %d", ast->conid.symbol->str, constructor_args, 0);
        necro_type_unify(infer, type, constructor_type, ast->scope, type, "While inferring the type of a constructor pattern: ");
        ast->necro_type = constructor_type;
        return type;
    }

    case NECRO_AST_CONSTRUCTOR:
    {
        NecroType* constructor_type  = ast->constructor.conid->conid.ast_symbol->type;
        assert(constructor_type != NULL);
        constructor_type             = necro_type_instantiate(infer, constructor_type, NULL);
        NecroType* pattern_type_head = NULL;
        NecroType* pattern_type      = NULL;
        NecroAst*  ast_args          = ast->constructor.arg_list;
        size_t     arg_count         = 0;
        while (ast_args != NULL)
        {
            if (pattern_type_head == NULL)
            {
                pattern_type_head = necro_type_fn_create(infer->arena, necro_infer_apat(infer, ast_args->list.item), NULL);
                pattern_type      = pattern_type_head;
            }
            else
            {
                pattern_type->fun.type2 = necro_type_fn_create(infer->arena, necro_infer_apat(infer, ast_args->list.item), NULL);
                pattern_type            = pattern_type->fun.type2;
            }
            arg_count++;
            ast_args = ast_args->list.next_item;
        }

        NecroType* data_type = necro_type_fresh_var(infer->arena);
        data_type->source_loc = ast->source_loc;
        if (pattern_type_head == NULL)
        {
            pattern_type_head = data_type;
        }
        else
        {
            pattern_type->fun.type2 = data_type;
        }

        // generalize type, inst type, unify against instantiated type
        size_t     constructor_args = 0;
        NecroType* con_iter         = constructor_type;
        while (con_iter->type == NECRO_TYPE_FUN)
        {
            constructor_args++;
            con_iter = con_iter->fun.type2;
        }
        if (arg_count != constructor_args)
            return necro_infer_ast_error(infer, pattern_type_head, ast, "Wrong number of arguments for constructor %s. Expected arity: %d, found arity %d", ast->constructor.conid->conid.symbol->str, constructor_args, arg_count);
        necro_type_unify(infer, constructor_type, pattern_type_head, ast->scope, pattern_type_head, "While inferring the type of a constructor pattern: ");
        ast->necro_type = constructor_type;
        return data_type;
    }

    default:
        assert(false && "Unimplemented pattern in apat type inference");
        return NULL;
    }
}

//=====================================================
// Pattern
//=====================================================
inline NecroType* necro_infer_pattern(NecroInfer* infer, NecroAst* ast)
{
    return necro_infer_apat(infer, ast);
}

//=====================================================
// Cast
//=====================================================
NecroType* necro_infer_case(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_CASE);
    NecroType* expression_type = necro_infer_go(infer, ast->case_expression.expression);
    NecroType* result_type     = necro_type_fresh_var(infer->arena);
    NecroAst*  alternatives    = ast->case_expression.alternatives;
    while (alternatives != NULL)
    {
        NecroAst* alternative = alternatives->list.item;
        necro_type_unify(infer, expression_type, necro_infer_pattern(infer, alternative->case_alternative.pat), alternatives->scope, expression_type, "While inferring the type of a case expression: ");
        necro_type_unify(infer, result_type, necro_infer_go(infer, alternative->case_alternative.body), alternatives->scope, result_type, "While inferring the type of a case expression: ");
        alternatives = alternatives->list.next_item;
    }
    // ast->case_expression.expression->necro_type = expression_type;
    // ast->case_expression.expression->necro_type = expression_type;
    ast->necro_type = result_type;
    return result_type;
}

//=====================================================
// Lambda
//=====================================================
NecroType* necro_infer_lambda(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LAMBDA);
    NecroAst*  apats  = ast->lambda.apats;
    NecroType* f_type = NULL;
    NecroType* f_head = NULL;
    while (apats != NULL)
    {
        assert(apats->type == NECRO_AST_APATS);
        if (f_type == NULL)
        {
            f_type = necro_type_fn_create(infer->arena, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_head = f_type;
        }
        else
        {
            f_type->fun.type2 = necro_type_fn_create(infer->arena, necro_infer_apat(infer, apats->apats.apat), NULL);
            f_type = f_type->fun.type2;
        }
        apats = apats->apats.next_apat;
    }
    f_type->fun.type2 = necro_infer_go(infer, ast->lambda.expression);
    ast->necro_type   = f_head;
    return f_head;
}

//=====================================================
// RightHandSide
//=====================================================
NecroType* necro_infer_right_hand_side(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_RIGHT_HAND_SIDE);
    necro_infer_go(infer, ast->right_hand_side.declarations);
    ast->necro_type = necro_infer_go(infer, ast->right_hand_side.expression);
    return ast->necro_type;
}

//=====================================================
// Let
//=====================================================
NecroType* necro_infer_let_expression(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_LET_EXPRESSION);
    necro_infer_go(infer, ast->let_expression.declarations);
    ast->necro_type = necro_infer_go(infer, ast->let_expression.expression);
    return ast->necro_type;
}

//=====================================================
// Arithmetic Sequence
//=====================================================
NecroType* necro_infer_arithmetic_sequence(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_ARITHMETIC_SEQUENCE);
    NecroType* type = infer->base->int_type->type;
    if (ast->arithmetic_sequence.from != NULL)
        necro_type_unify(infer, type, necro_infer_go(infer, ast->arithmetic_sequence.from), ast->scope, type, "While inferring the type of an arithmetic sequence: ");
    if (ast->arithmetic_sequence.then != NULL)
        necro_type_unify(infer, type, necro_infer_go(infer, ast->arithmetic_sequence.then), ast->scope, type, "While inferring the type of an arithmetic sequence: ");
    if (ast->arithmetic_sequence.to != NULL)
        necro_type_unify(infer, type, necro_infer_go(infer, ast->arithmetic_sequence.to), ast->scope, type, "While inferring the type of an arithemetic sequence: ");
    ast->necro_type = necro_type_con1_create(infer->arena, infer->base->list_type, type);
    return ast->necro_type;
}

//=====================================================
// Do
//=====================================================
NecroType* necro_infer_do_statement(NecroInfer* infer, NecroAst* ast, NecroType* monad_var)
{
    assert(infer != NULL);
    assert(ast != NULL);
    switch(ast->type)
    {

    case NECRO_AST_LET_EXPRESSION: ast->necro_type = necro_infer_let_expression(infer, ast); return NULL;

    case NECRO_BIND_ASSIGNMENT:
    {
        NecroType* var_name                            = necro_type_fresh_var(infer->arena);
        ast->bind_assignment.ast_symbol->type = var_name;
        NecroType* rhs_type                            = necro_infer_go(infer, ast->bind_assignment.expression);
        ast->necro_type                                = necro_type_app_create(infer->arena, monad_var, var_name);
        necro_type_unify(infer, rhs_type, ast->necro_type, ast->scope, rhs_type, "While inferring the type of a bind assignment: ");
        return NULL;
    }

    case NECRO_PAT_BIND_ASSIGNMENT:
    {
        NecroType* pat_type = necro_infer_pattern(infer, ast->pat_bind_assignment.pat);
        NecroType* rhs_type = necro_infer_go(infer, ast->pat_bind_assignment.expression);
        ast->necro_type     = necro_type_app_create(infer->arena, monad_var, pat_type);
        necro_type_unify(infer, rhs_type, ast->necro_type, ast->scope, rhs_type, "While inferring the type of a pattern bind assignment: ");
        return NULL;
    }

    // default: return necro_infer_ast_error(infer, NULL, ast, "Unimplemented ast type in infer_do_statement : %d", ast->type);
    // case NECRO_AST_VARIABLE:            statement_type = necro_infer_var(infer, ast);             break;
    // case NECRO_AST_CONID:               statement_type = necro_infer_conid(infer, ast);           break;
    // case NECRO_AST_EXPRESSION_LIST:     statement_type = necro_infer_expression_list(infer, ast); break;
    // case NECRO_AST_FUNCTION_EXPRESSION: statement_type = necro_infer_fexpr(infer, ast);           break;
    // This should be ok actually?
    default:
    {
        NecroType* statement_type = necro_infer_go(infer, ast);
        ast->necro_type           = necro_type_app_create(infer->arena, monad_var, necro_type_fresh_var(infer->arena));
        necro_type_unify(infer, statement_type, ast->necro_type, ast->scope, statement_type, "While inferring the type of a do statement: ");
        return ast->necro_type;
    }

    }
}

// TODO / NOTE: Perhaps drop Monads!?!?! Is that crazy? feels like a heavy handed solution to some specific use cases...
NecroType* necro_infer_do(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DO);
    NecroType* monad_var      = necro_type_fresh_var(infer->arena);
    NecroAst*  statements     = ast->do_statement.statement_list;
    NecroType* statement_type = NULL;
    necro_apply_constraints(infer->arena, monad_var, necro_create_type_class_context(infer->arena, infer->base->monad_type_class->type_class, infer->base->monad_type_class, monad_var->var.var_symbol, NULL));
    while (statements != NULL)
    {
        statement_type = necro_infer_do_statement(infer, statements->list.item, monad_var);
        statements = statements->list.next_item;
    }
    ast->necro_type = statement_type;
    ast->do_statement.monad_var = monad_var;
    if (statement_type == NULL)
        return necro_infer_ast_error(infer, NULL, ast, "The final statement in a do block must be an expression");
    else
        return statement_type;
}

//=====================================================
// Declaration groups
//=====================================================
// NOTE: NEW! Local bindings are mono-typed! Read up from Simon Peyton Jones why this is generally a good idea.
NecroType* necro_infer_declaration_group(NecroInfer* infer, NecroDeclarationGroup* declaration_group)
{
    assert(infer != NULL);
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
            necro_infer_simple_type(infer, ast->data_declaration.simpletype, ast);
            break;
        case NECRO_AST_TYPE_SIGNATURE:
            necro_infer_type_sig(infer, ast);
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            necro_create_type_class(infer, ast);
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            necro_create_type_class_instance(infer, ast);
            break;
        default:
            assert(false);
            break;
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
            necro_infer_simple_assignment(infer, ast);
            break;
        case NECRO_AST_APATS_ASSIGNMENT:
            necro_infer_apats_assignment(infer, ast);
            break;
        case NECRO_AST_PAT_ASSIGNMENT:
            necro_infer_pat_assignment(infer, ast);
            break;
        case NECRO_AST_DATA_DECLARATION:
        {
            NecroAst* constructor_list = ast->data_declaration.constructor_list;
            size_t    con_num          = 0;
            while (constructor_list != NULL)
            {
                necro_create_data_constructor(infer, constructor_list->list.item, ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol->type, con_num);
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
            assert(false);
            break;
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
            necro_kind_infer(infer, data->type, data->type, "While declaring a variable: ");
            if (necro_is_infer_error(infer)) return NULL;
            data->type->kind = necro_kind_gen(infer, data->type->kind);
            if (necro_is_infer_error(infer)) return NULL;
            necro_kind_unify(infer, data->type->kind, infer->base->star_kind->type, NULL, data->type, "While declaring a variable: ");
            if (necro_is_infer_error(infer)) return NULL;
            data->type_status = NECRO_TYPE_DONE;
            break;
        }
        case NECRO_AST_APATS_ASSIGNMENT:
        {
            data = ast->apats_assignment.ast_symbol;
            if (data->type->pre_supplied || data->type_status == NECRO_TYPE_DONE) { data->type_status = NECRO_TYPE_DONE; curr->type_checked = true; curr = curr->next;  continue; }
            // if (symbol_info->scope->parent == NULL)
            // Is local binding....how to check!?
            data->type       = necro_type_generalize(infer, data->type, ast->scope->parent);
            necro_kind_infer(infer, data->type, data->type, "While declaring a variable: ");
            data->type->kind = necro_kind_gen(infer, data->type->kind);
            necro_kind_unify(infer, data->type->kind, infer->base->star_kind->type, NULL, data->type, "While declaring a variable: ");
            data->type_status = NECRO_TYPE_DONE;
            break;
        }
        case NECRO_AST_PAT_ASSIGNMENT:
        {
            necro_gen_pat_go(infer, ast->pat_assignment.pat);
            break;
        }
        case NECRO_AST_DATA_DECLARATION:
        {
            data             = ast->data_declaration.simpletype->simple_type.type_con->conid.ast_symbol;
            data->type->kind = necro_kind_gen(infer, data->type->kind);
            break;
        }
        case NECRO_AST_TYPE_SIGNATURE:
            break;
        case NECRO_AST_TYPE_CLASS_DECLARATION:
            break;
        case NECRO_AST_TYPE_CLASS_INSTANCE:
            break;
        default:
            assert(false);
            break;
        }
        curr->type_checked = true;
        curr               = curr->next;
    }

    return NULL;
}

//=====================================================
// Declaration
//=====================================================
NecroType* necro_infer_declaration(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    assert(ast != NULL);
    assert(ast->type == NECRO_AST_DECL);

    //----------------------------------------------------
    // Infer types for declaration groups
    NecroDeclarationGroupList* groups = ast->declaration.group_list;
    while (groups != NULL)
    {
        if (groups->declaration_group != NULL)
        {
            necro_infer_declaration_group(infer, groups->declaration_group);
        }
        groups = groups->next;
    }

    // Declarations themselves have no types
    return NULL;
}

//=====================================================
// Top Declaration
//=====================================================
NecroType* necro_infer_top_declaration(NecroInfer* infer, NecroAst* ast)
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
            necro_infer_declaration_group(infer, groups->declaration_group);
        }
        groups = groups->next;
    }

    // Declarations themselves have no types
    return NULL;
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
NecroType* necro_infer_go(NecroInfer* infer, NecroAst* ast)
{
    assert(infer != NULL);
    if (ast == NULL)
        return NULL;
    switch (ast->type)
    {

    case NECRO_AST_CONSTANT:               return necro_infer_constant(infer, ast);
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
    case NECRO_AST_WILDCARD:               return necro_infer_wildcard(infer, ast);
    case NECRO_AST_ARITHMETIC_SEQUENCE:    return necro_infer_arithmetic_sequence(infer, ast);
    case NECRO_AST_DO:                     return necro_infer_do(infer, ast);
    case NECRO_AST_TYPE_SIGNATURE:         return necro_infer_type_sig(infer, ast);

    // case NECRO_AST_SIMPLE_ASSIGNMENT:      return NULL;
    // case NECRO_AST_APATS_ASSIGNMENT:       return NULL;
    // case NECRO_AST_PAT_ASSIGNMENT:         return NULL;
    // case NECRO_AST_TYPE_CLASS_DECLARATION: return NULL;
    // case NECRO_AST_TYPE_CLASS_INSTANCE:    return NULL;
    // case NECRO_AST_DATA_DECLARATION:       return NULL;

    default:
        assert(false);
        return NULL;
    }
}

NecroType* necro_infer_ast_error(NecroInfer* infer, NecroType* type, NecroAst* ast, const char* error_message, ...)
{
    UNUSED(infer);
    UNUSED(type);
    UNUSED(ast);
    UNUSED(error_message);
    return NULL;
}

NecroResult(void) necro_infer(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroInfer infer = necro_infer_create(&ast_arena->arena, intern, scoped_symtable, base);
    // TODO: Error handling
    // TODO: Printing symbol data (perhaps crawl top level?)
    necro_infer_go(&infer, ast_arena->root);
    if (info.compilation_phase == NECRO_PHASE_INFER && info.verbosity > 0)
    {
        necro_ast_arena_print(ast_arena);
        return ok_void();
    }
    // TODO: New / Different State object for type class translation!!!!
    necro_type_class_translate(&infer, ast_arena->root);
    if (info.compilation_phase == NECRO_PHASE_TYPE_CLASS_TRANSLATE && info.verbosity > 0)
        necro_ast_arena_print(ast_arena);
    return ok_void();
}

///////////////////////////////////////////////////////
// Testing
///////////////////////////////////////////////////////
void necro_test_infer()
{
    necro_announce_phase("NecroInfer");
}
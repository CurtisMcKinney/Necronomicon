/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "symtable.h"
#include "lifetime.h"

NecroLifetime* necro_create_constant_lifetime(NecroInfer* infer)
{
    NecroSymbol     constant_symbol = necro_intern_string(infer->intern, "\'constant");
    NecroSymbolInfo constant_info   = necro_create_initial_symbol_info(constant_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         constant_id     = necro_symtable_insert(infer->symtable, constant_info);
    NecroCon        constant_con    = (NecroCon) { .id = constant_id, .symbol = constant_symbol };
    NecroType*      constant_type   = necro_alloc_type(infer);
    constant_type->type             = NECRO_TYPE_CON;
    constant_type->con              = (NecroTypeCon)
    {
        .con      = constant_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    constant_type->type_kind    = NULL;
    constant_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, constant_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, constant_con.id)->type = constant_type;
    return constant_type;
}

NecroLifetime* necro_create_static_lifetime(NecroInfer* infer)
{
    NecroSymbol     static_symbol = necro_intern_string(infer->intern, "\'static");
    NecroSymbolInfo static_info   = necro_create_initial_symbol_info(static_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         static_id     = necro_symtable_insert(infer->symtable, static_info);
    NecroCon        static_con    = (NecroCon) { .id = static_id, .symbol = static_symbol };
    NecroType*      static_type   = necro_alloc_type(infer);
    static_type->type             = NECRO_TYPE_CON;
    static_type->con              = (NecroTypeCon)
    {
        .con      = static_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    static_type->type_kind    = NULL;
    static_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, static_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, static_con.id)->type = static_type;
    return static_type;
}

NecroLifetime* necro_create_never_lifetime(NecroInfer* infer)
{
    NecroSymbol     never_symbol = necro_intern_string(infer->intern, "\'never");
    NecroSymbolInfo never_info   = necro_create_initial_symbol_info(never_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         never_id     = necro_symtable_insert(infer->symtable, never_info);
    NecroCon        never_con    = (NecroCon) { .id = never_id, .symbol = never_symbol };
    NecroType*      never_type   = necro_alloc_type(infer);
    never_type->type             = NECRO_TYPE_CON;
    never_type->con              = (NecroTypeCon)
    {
        .con      = never_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    never_type->type_kind    = NULL;
    never_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, never_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, never_con.id)->type = never_type;
    return never_type;
}

NecroLifetime* necro_create_delay_lifetime(NecroInfer* infer)
{
    NecroSymbol     delay_symbol = necro_intern_string(infer->intern, "\'delay");
    NecroSymbolInfo delay_info   = necro_create_initial_symbol_info(delay_symbol, (NecroSourceLoc) { 0 }, NULL, infer->intern);
    NecroID         delay_id     = necro_symtable_insert(infer->symtable, delay_info);
    NecroCon        delay_con    = (NecroCon) { .id = delay_id, .symbol = delay_symbol };
    NecroType*      delay_type   = necro_alloc_type(infer);
    delay_type->type             = NECRO_TYPE_CON;
    delay_type->con              = (NecroTypeCon)
    {
        .con      = delay_con,
        .args     = NULL,
        .arity    = 0,
        .is_class = false,
    };
    delay_type->type_kind    = NULL;
    delay_type->pre_supplied = true;
    assert(necro_symtable_get(infer->symtable, delay_con.id)->type == NULL);
    necro_symtable_get(infer->symtable, delay_con.id)->type = delay_type;
    return delay_type;
}

void necro_infer_lifetime(NecroInfer* infer, NecroASTNode* ast)
{
}

void necro_rigid_lifetime_variable_error(NecroInfer* infer, NecroVar type_var, NecroLifetime* type, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    const char* type_name = NULL;
    if (type->type == NECRO_TYPE_CON)
        type_name = necro_intern_get_string(infer->intern, type->con.con.symbol);
    else if (type->type == NECRO_TYPE_APP)
        type_name = "TypeApp";
    else if (type->type == NECRO_TYPE_FUN)
        type_name = "(->)";
    else if (type->type == NECRO_TYPE_VAR)
        type_name = necro_id_as_character_string(infer, type->var.var);
    else
        assert(false);
    const char* var_name = necro_id_as_character_string(infer, type_var);
    necro_infer_error(infer, error_preamble, macro_type, "Couldn't match lifetime \'%s\' with lifetime \'%s\'.\n\'%s\' is a lifetime kind variable bound by a type signature.", var_name, type_name, var_name);
}

inline void necro_instantiate_lifetime_var(NecroInfer* infer, NecroTypeVar* lifetime_var, NecroLifetime* lifetime, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    necr_bind_type_var(infer, lifetime_var->var, lifetime);
}

void necro_unify_var_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(lifetime1 != NULL);
    assert(lifetime1->type == NECRO_TYPE_VAR);
    if (lifetime2 == NULL)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Mismatched arities");
        return;
    }
    if (lifetime1 == lifetime2)
        return;
    switch (lifetime2->type)
    {
    case NECRO_TYPE_VAR:
        // necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        // necro_kind_unify(infer, type1->type_kind, type2->type_kind, scope, macro_type, error_preamble);
        if (lifetime1->var.var.id.id == lifetime2->var.var.id.id)
            return;
        else if (lifetime1->var.is_rigid && lifetime2->var.is_rigid)
            necro_rigid_lifetime_variable_error(infer, lifetime1->var.var, lifetime2, macro_type, error_preamble);
        else if (necro_occurs(infer, lifetime1, lifetime2, macro_type, error_preamble))
            return;
        else if (lifetime1->var.is_rigid)
            necro_instantiate_lifetime_var(infer, &lifetime2->var, lifetime1, macro_type, error_preamble);
        else if (lifetime2->var.is_rigid)
            necro_instantiate_lifetime_var(infer, &lifetime1->var, lifetime2, macro_type, error_preamble);
        else if (necro_is_bound_in_scope(infer, lifetime1, scope))
            necro_instantiate_lifetime_var(infer, &lifetime2->var, lifetime1, macro_type, error_preamble);
        else
            necro_instantiate_lifetime_var(infer, &lifetime1->var, lifetime2, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    case NECRO_TYPE_APP:
    case NECRO_TYPE_FUN:
        if (lifetime1->var.is_rigid)
        {
            necro_rigid_lifetime_variable_error(infer, lifetime1->var.var, lifetime2, macro_type, error_preamble);
            return;
        }
        if (necro_occurs(infer, lifetime1, lifetime2, macro_type, error_preamble))
        {
            return;
        }
        necro_instantiate_lifetime_var(infer, &lifetime1->var, lifetime2, macro_type, error_preamble);
        // necro_unify_kinds(infer, type2, &type2->kind, &type1->kind, macro_type, error_preamble);
        // necro_kind_unify(infer, type1->type_kind, type2->type_kind, scope, macro_type, error_preamble);
        return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to lifetime unify polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to lifetime unify LifetimeVar with type args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent lifetime type (lifetime1: %d, lifetime2: %s) type found in necro_unify_var_lifetimes.", lifetime1->type, lifetime2->type); return;
    }
}

void necro_unify_con_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(lifetime1 != NULL);
    assert(lifetime1->type == NECRO_TYPE_CON);
    assert(lifetime2 != NULL);
    switch (lifetime2->type)
    {
    case NECRO_TYPE_VAR:
        if (lifetime2->var.is_rigid)
            necro_rigid_lifetime_variable_error(infer, lifetime2->var.var, lifetime1, macro_type, error_preamble);
        else if (necro_occurs(infer, lifetime2, lifetime1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_lifetime_var(infer, &lifetime2->var, lifetime1, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
        if (lifetime1->con.con.symbol.id != lifetime2->con.con.symbol.id)
        {
            necro_infer_error(infer, error_preamble, lifetime1, "Attempting to unify two different lifetime types, lifetime1: %s lifetime2: %s", necro_intern_get_string(infer->intern, lifetime1->con.con.symbol), necro_intern_get_string(infer->intern, lifetime2->con.con.symbol));
        }
        else
        {
            NecroLifetime* original_lifetime1 = lifetime1;
            NecroLifetime* original_lifetime2 = lifetime2;
            lifetime1 = lifetime1->con.args;
            lifetime2 = lifetime2->con.args;
            while (lifetime1 != NULL && lifetime2 != NULL)
            {
                if (lifetime1 == NULL || lifetime2 == NULL)
                {
                    necro_infer_error(infer, error_preamble, lifetime1, "Mismatched arities, lifetime1: %s, lifetime2: %s", necro_intern_get_string(infer->intern, original_lifetime1->con.con.symbol), necro_intern_get_string(infer->intern, original_lifetime2->con.con.symbol));
                    return;
                }
                assert(lifetime1->type == NECRO_TYPE_LIST);
                assert(lifetime2->type == NECRO_TYPE_LIST);
                necro_unify_lifetimes(infer, lifetime1->list.item, lifetime2->list.item, scope, macro_type, error_preamble);
                if (necro_is_infer_error(infer)) return;
                lifetime1 = lifetime1->list.next;
                lifetime2 = lifetime2->list.next;
            }
        }
        return;
    case NECRO_TYPE_APP:
    {
        NecroType* uncurried_con = necro_curry_con(infer, lifetime1);
        if (uncurried_con == NULL)
        {
            necro_infer_error(infer, error_preamble, lifetime1, "Arity mismatch during unification for type: %s", necro_intern_get_string(infer->intern, lifetime1->con.con.symbol));
        }
        else
        {
            necro_unify_lifetimes(infer, uncurried_con, lifetime2, scope, macro_type, error_preamble);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify lifetime TypeCon (%s) with (->).", necro_intern_get_string(infer->intern, lifetime1->con.con.symbol)); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify lifetime TypeCon (%s) with type args list.", necro_intern_get_string(infer->intern, lifetime1->con.con.symbol)); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify lifetime polytype."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent type (type1: %d, type2: %s) type found in necro_unify.", lifetime1->type, lifetime2->type); return;
    }
}

void necro_unify_app_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer)) return;
    assert(lifetime1 != NULL);
    assert(lifetime1->type == NECRO_TYPE_APP);
    if (necro_is_infer_error(infer)) return;
    if (lifetime1 == NULL)
    {
        necro_infer_error(infer, error_preamble, macro_type, "Arity mismatch during lifetime unification for type: %s", necro_intern_get_string(infer->intern, lifetime1->con.con.symbol));
        return;
    }
    switch (lifetime2->type)
    {
    case NECRO_TYPE_VAR:
        if (lifetime2->var.is_rigid)
            necro_rigid_lifetime_variable_error(infer, lifetime2->var.var, lifetime1, macro_type, error_preamble);
        else if (necro_occurs(infer, lifetime2, lifetime1, macro_type, error_preamble))
            return;
        else
            necro_instantiate_lifetime_var(infer, &lifetime2->var, lifetime1, macro_type, error_preamble);
        return;
    case NECRO_TYPE_APP:
        necro_unify_lifetimes(infer, lifetime1->app.type1, lifetime2->app.type1, scope, macro_type, error_preamble);
        necro_unify_lifetimes(infer, lifetime1->app.type2, lifetime2->app.type2, scope, macro_type, error_preamble);
        return;
    case NECRO_TYPE_CON:
    {
        NecroType* uncurried_con = necro_curry_con(infer, lifetime2);
        if (uncurried_con == NULL)
        {
            necro_infer_error(infer, error_preamble, macro_type, "Arity mismatch during lifetime unification for type: %s", necro_intern_get_string(infer->intern, lifetime2->con.con.symbol));
        }
        else
        {
            necro_unify_lifetimes(infer, lifetime1, uncurried_con, scope, macro_type, error_preamble);
        }
        return;
    }
    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, macro_type, "Attempting to unify lifetime app  with lifetime (->)."); return;
    case NECRO_TYPE_FOR:  necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify lifetime polytype."); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Attempted to unify lifetime app with lifetime args list."); return;
    default:              necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent lifetime type (lifetime type 1: %d, lifetime type 2: %s) type found in necro_unify_app_lifetimes.", lifetime1->type, lifetime2->type); return;
    }
}

void necro_unify_lifetimes(NecroInfer* infer, NecroLifetime* lifetime1, NecroLifetime* lifetime2, NecroScope* scope, NecroType* macro_type, const char* error_preamble)
{
    if (necro_is_infer_error(infer))
        return;
    assert(infer != NULL);
    assert(lifetime1 != NULL);
    assert(lifetime2 != NULL);
    lifetime1 = necro_find(infer, lifetime1);
    lifetime2 = necro_find(infer, lifetime2);
    if (lifetime1 == lifetime2)
        return;
    switch (lifetime1->type)
    {
    case NECRO_TYPE_VAR:  necro_unify_var_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_CON:  necro_unify_con_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;
    case NECRO_TYPE_APP:  necro_unify_app_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble); return;

    case NECRO_TYPE_FOR:
    {
        while (lifetime1->type == NECRO_TYPE_FOR)
            lifetime1 = lifetime1->for_all.type;
        necro_unify_lifetimes(infer, lifetime1, lifetime2, scope, macro_type, error_preamble);
        return;
    }

    case NECRO_TYPE_FUN:  necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime function in necro_unify_lifetimes"); return;
    case NECRO_TYPE_LIST: necro_infer_error(infer, error_preamble, lifetime1, "Compiler bug: Found lifetime args list in necro_unify_lifetimes"); return;
    default: necro_infer_error(infer, error_preamble, macro_type, "Compiler bug: Non-existent lifetime node type (lifetime1: %d, lifetime2: %s) found in necro_unify_lifetimes.", lifetime1->type, lifetime2->type); return;
    }
}

// typedef struct NecroTimeScope
// {
//     struct NecroTypeScope* parent;
// } NecroTimeScope;

// typedef struct
// {
//     NecroError       error;
//     NecroPagedArena* arena;
//     NecroSymTable*   symtable;
//     NecroTimeScope*  top_time_scope;
// } NecroCircularAnalyzer;

// // TODO: Finish
// // Store time scope in AST?
// void c_analyze_go(NecroCircularAnalyzer* c_analyzer, NecroASTNode* ast)
// {
//     if (ast == NULL || c_analyzer->error.return_code != NECRO_SUCCESS)
//         return;
//     switch (ast->type)
//     {

//     //=====================================================
//     // Declaration type things
//     //=====================================================
//     case NECRO_AST_TOP_DECL:
//     {
//         NecroASTNode* curr = ast;
//         while (curr != NULL)
//         {
//             if (curr->top_declaration.declaration->type == NECRO_AST_SIMPLE_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_APATS_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_PAT_ASSIGNMENT)
//                 c_analyze_go(c_analyzer, curr->top_declaration.declaration);
//             curr = curr->top_declaration.next_top_decl;
//         }
//         break;
//     }

//     case NECRO_AST_DECL:
//     {
//         NecroASTNode* curr = ast;
//         while (curr != NULL)
//         {
//             if (curr->top_declaration.declaration->type == NECRO_AST_SIMPLE_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_APATS_ASSIGNMENT || curr->top_declaration.declaration->type == NECRO_AST_PAT_ASSIGNMENT)
//                 c_analyze_go(c_analyzer, curr->top_declaration.declaration);
//             curr = curr->declaration.next_declaration;
//         }
//         break;
//     }

//     case NECRO_AST_TYPE_CLASS_INSTANCE:
//     {
//         c_analyze_go(c_analyzer, ast->type_class_instance.context);
//         c_analyze_go(c_analyzer, ast->type_class_instance.qtycls);
//         c_analyze_go(c_analyzer, ast->type_class_instance.inst);
//         c_analyze_go(c_analyzer, ast->type_class_instance.declarations);
//         break;
//     }

//     //=====================================================
//     // Assignment type things
//     //=====================================================
//     case NECRO_AST_SIMPLE_ASSIGNMENT:
//     {
//         c_analyze_go(c_analyzer, ast->simple_assignment.rhs);
//         break;
//     }

//     case NECRO_AST_APATS_ASSIGNMENT:
//     {
//         c_analyze_go(c_analyzer, ast->apats_assignment.apats);
//         c_analyze_go(c_analyzer, ast->apats_assignment.rhs);
//         break;
//     }

//     case NECRO_AST_PAT_ASSIGNMENT:
//         c_analyze_go(c_analyzer, ast->pat_assignment.rhs);
//         break;

//     case NECRO_AST_DATA_DECLARATION:
//         c_analyze_go(c_analyzer, ast->data_declaration.simpletype);
//         c_analyze_go(c_analyzer, ast->data_declaration.constructor_list);
//         break;

//     //=====================================================
//     // Variable type things
//     //=====================================================
//     case NECRO_AST_VARIABLE:
//         switch (ast->variable.var_type)
//         {
//         case NECRO_VAR_VAR:
//         {
//             // NecroSymbolInfo* symbol_info = necro_symtable_get(c_analyzer->symtable, ast->variable.id);
//             // if (symbol_info->declaration_group == NULL) return;
//             // NecroDeclarationGroup* w = necro_symtable_get(c_analyzer->symtable, ast->variable.id)->declaration_group;
//             // assert(w->info != NULL);
//             // if (w->info->current_group == NULL)
//             //     w->info->current_group = w;
//             // NecroDeclarationGroup* v = w->info->current_group;
//             // assert(v != NULL);
//             // if (w->index == -1)
//             // {
//             //     symbol_info->declaration_group->info->current_group = w;
//             //     c_analyze_go(c_analyzer, w->declaration_ast);
//             //     v->low_link = min(w->low_link, v->low_link);
//             // }
//             // else if (w->on_stack)
//             // {
//             //     v->low_link = min(w->low_link, v->low_link);
//             // }
//             // symbol_info->declaration_group->info->current_group = v;
//             break;
//         }
//         case NECRO_VAR_DECLARATION:          break;
//         case NECRO_VAR_SIG:                  break;
//         case NECRO_VAR_TYPE_VAR_DECLARATION: break;
//         case NECRO_VAR_TYPE_FREE_VAR:        break;
//         case NECRO_VAR_CLASS_SIG:            break;
//         default: assert(false);
//         }
//         break;

//     case NECRO_AST_CONID:
//         if (ast->conid.con_type == NECRO_CON_TYPE_VAR)
//         {
//             // NecroSymbolInfo* symbol_info = necro_symtable_get(c_analyzer->symtable, ast->conid.id);
//             // if (symbol_info->declaration_group == NULL) return;
//             // NecroDeclarationGroup* w = necro_symtable_get(c_analyzer->symtable, ast->conid.id)->declaration_group;
//             // assert(w->info != NULL);
//             // if (w->info->current_group == NULL)
//             //     w->info->current_group = w;
//             // NecroDeclarationGroup* v = w->info->current_group;
//             // assert(v != NULL);
//             // if (w->index == -1)
//             // {
//             //     symbol_info->declaration_group->info->current_group = w;
//             //     c_analyze_go(c_analyzer, w->declaration_ast);
//             //     v->low_link = min(w->low_link, v->low_link);
//             // }
//             // else if (w->on_stack)
//             // {
//             //     v->low_link = min(w->low_link, v->low_link);
//             // }
//             // symbol_info->declaration_group->info->current_group = v;
//             break;
//         }
//         break;

//     //=====================================================
//     // Other Stuff
//     //=====================================================
//     case NECRO_AST_UNDEFINED:
//         break;
//     case NECRO_AST_CONSTANT:
//         break;
//     case NECRO_AST_UN_OP:
//         break;
//     case NECRO_AST_BIN_OP:
//         c_analyze_go(c_analyzer, ast->bin_op.lhs);
//         c_analyze_go(c_analyzer, ast->bin_op.rhs);
//         break;
//     case NECRO_AST_IF_THEN_ELSE:
//         c_analyze_go(c_analyzer, ast->if_then_else.if_expr);
//         c_analyze_go(c_analyzer, ast->if_then_else.then_expr);
//         c_analyze_go(c_analyzer, ast->if_then_else.else_expr);
//         break;
//     case NECRO_AST_OP_LEFT_SECTION:
//         c_analyze_go(c_analyzer, ast->op_left_section.left);
//         break;
//     case NECRO_AST_OP_RIGHT_SECTION:
//         c_analyze_go(c_analyzer, ast->op_right_section.right);
//         break;
//     case NECRO_AST_RIGHT_HAND_SIDE:
//         c_analyze_go(c_analyzer, ast->right_hand_side.declarations);
//         c_analyze_go(c_analyzer, ast->right_hand_side.expression);
//         break;
//     case NECRO_AST_LET_EXPRESSION:
//         c_analyze_go(c_analyzer, ast->let_expression.declarations);
//         c_analyze_go(c_analyzer, ast->let_expression.expression);
//         break;
//     case NECRO_AST_FUNCTION_EXPRESSION:
//         c_analyze_go(c_analyzer, ast->fexpression.aexp);
//         c_analyze_go(c_analyzer, ast->fexpression.next_fexpression);
//         break;
//     case NECRO_AST_APATS:
//         c_analyze_go(c_analyzer, ast->apats.apat);
//         c_analyze_go(c_analyzer, ast->apats.next_apat);
//         break;
//     case NECRO_AST_WILDCARD:
//         break;
//     case NECRO_AST_LAMBDA:
//         c_analyze_go(c_analyzer, ast->lambda.apats);
//         c_analyze_go(c_analyzer, ast->lambda.expression);
//         break;
//     case NECRO_AST_DO:
//         c_analyze_go(c_analyzer, ast->do_statement.statement_list);
//         break;
//     case NECRO_AST_LIST_NODE:
//         c_analyze_go(c_analyzer, ast->list.item);
//         c_analyze_go(c_analyzer, ast->list.next_item);
//         break;
//     case NECRO_AST_EXPRESSION_LIST:
//         c_analyze_go(c_analyzer, ast->expression_list.expressions);
//         break;
//     case NECRO_AST_TUPLE:
//         c_analyze_go(c_analyzer, ast->tuple.expressions);
//         break;
//     case NECRO_BIND_ASSIGNMENT:
//         c_analyze_go(c_analyzer, ast->bind_assignment.expression);
//         break;
//     case NECRO_PAT_BIND_ASSIGNMENT:
//         c_analyze_go(c_analyzer, ast->pat_bind_assignment.pat);
//         c_analyze_go(c_analyzer, ast->pat_bind_assignment.expression);
//         break;
//     case NECRO_AST_ARITHMETIC_SEQUENCE:
//         c_analyze_go(c_analyzer, ast->arithmetic_sequence.from);
//         c_analyze_go(c_analyzer, ast->arithmetic_sequence.then);
//         c_analyze_go(c_analyzer, ast->arithmetic_sequence.to);
//         break;
//     case NECRO_AST_CASE:
//         c_analyze_go(c_analyzer, ast->case_expression.expression);
//         c_analyze_go(c_analyzer, ast->case_expression.alternatives);
//         break;
//     case NECRO_AST_CASE_ALTERNATIVE:
//         c_analyze_go(c_analyzer, ast->case_alternative.pat);
//         c_analyze_go(c_analyzer, ast->case_alternative.body);
//         break;

//     // Other stuff
//     case NECRO_AST_TYPE_APP:
//         // c_analyze_go(c_analyzer, ast->type_app.ty);
//         // c_analyze_go(c_analyzer, ast->type_app.next_ty);
//         break;
//     case NECRO_AST_BIN_OP_SYM:
//         c_analyze_go(c_analyzer, ast->bin_op_sym.left);
//         c_analyze_go(c_analyzer, ast->bin_op_sym.op);
//         c_analyze_go(c_analyzer, ast->bin_op_sym.right);
//         break;
//     case NECRO_AST_CONSTRUCTOR:
//         c_analyze_go(c_analyzer, ast->constructor.conid);
//         c_analyze_go(c_analyzer, ast->constructor.arg_list);
//         break;
//     case NECRO_AST_SIMPLE_TYPE:
//         // c_analyze_go(c_analyzer, ast->simple_type.type_con);
//         // c_analyze_go(c_analyzer, ast->simple_type.type_var_list);
//         break;
//     case NECRO_AST_TYPE_CLASS_DECLARATION:
//         // c_analyze_go(c_analyzer, ast->type_class_declaration.context);
//         // c_analyze_go(c_analyzer, ast->type_class_declaration.tycls);
//         // c_analyze_go(c_analyzer, ast->type_class_declaration.tyvar);
//         // c_analyze_go(c_analyzer, ast->type_class_declaration.declarations);
//         break;
//     case NECRO_AST_TYPE_SIGNATURE:
//         // c_analyze_go(c_analyzer, ast->type_signature.var);
//         // c_analyze_go(c_analyzer, ast->type_signature.context);
//         // c_analyze_go(c_analyzer, ast->type_signature.type);
//         break;
//     case NECRO_AST_TYPE_CLASS_CONTEXT:
//         // c_analyze_go(c_analyzer, ast->type_class_context.conid);
//         // c_analyze_go(c_analyzer, ast->type_class_context.varid);
//         break;
//     case NECRO_AST_FUNCTION_TYPE:
//         // c_analyze_go(c_analyzer, ast->function_type.type);
//         // c_analyze_go(c_analyzer, ast->function_type.next_on_arrow);
//         break;

//     default:
//         necro_error(&c_analyzer->error, ast->source_loc, "Unrecognized AST Node type found in dependency analysis: %d", ast->type);
//         break;
//     }
// }
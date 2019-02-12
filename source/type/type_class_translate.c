/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include "type_class_translate.h"
#include "base.h"

///////////////////////////////////////////////////////
// NecroTypeClassTranslate
///////////////////////////////////////////////////////

typedef struct
{
    NecroIntern*         intern;
    NecroScopedSymTable* scoped_symtable;
    NecroBase*           base;
    NecroAstArena*       ast_arena;
    NecroPagedArena*     arena;
    NecroSnapshotArena   snapshot_arena;
} NecroTypeClassTranslate;

NecroTypeClassTranslate necro_type_class_translate_empty()
{
    NecroTypeClassTranslate type_class_translate = (NecroTypeClassTranslate)
    {
        .intern          = NULL,
        .arena           = NULL,
        .snapshot_arena  = necro_snapshot_arena_empty(),
        .scoped_symtable = NULL,
        .base            = NULL,
        .ast_arena       = NULL,
    };
    return type_class_translate;
}

NecroTypeClassTranslate necro_type_class_translate_create(NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroTypeClassTranslate type_class_translate = (NecroTypeClassTranslate)
    {
        .intern          = intern,
        .arena           = &ast_arena->arena,
        .snapshot_arena  = necro_snapshot_arena_create(),
        .scoped_symtable = scoped_symtable,
        .base            = base,
        .ast_arena       = ast_arena,
    };
    return type_class_translate;
}

void necro_type_class_translate_destroy(NecroTypeClassTranslate* type_class_translate)
{
    assert(type_class_translate != NULL);
    necro_snapshot_arena_destroy(&type_class_translate->snapshot_arena);
    *type_class_translate = necro_type_class_translate_empty();
}

NecroResult(void) necro_type_class_translate(NecroCompileInfo info, NecroIntern* intern, NecroScopedSymTable* scoped_symtable, NecroBase* base, NecroAstArena* ast_arena)
{
    NecroTypeClassTranslate type_class_translate = necro_type_class_translate_create(intern, scoped_symtable, base, ast_arena);
    necro_type_class_translate_destroy(&type_class_translate);

    NecroResult(void) result = ok_void(); // TODO: Finish

    if (result.type == NECRO_RESULT_OK)
    {
        if (info.compilation_phase == NECRO_PHASE_TYPE_CLASS_TRANSLATE && info.verbosity > 0)
            necro_ast_arena_print(ast_arena);
        return ok_void();
    }

    return result;
}

///////////////////////////////////////////////////////
// Go
///////////////////////////////////////////////////////
// TODO: Replace original in type_class.h
NecroResult(void) necro_type_class_translate_go2(NecroTypeClassTranslate* type_class_translate, NecroAst* ast)
{
    UNUSED(type_class_translate);
    if (ast == NULL)
        return ok_void();
    switch (ast->type)
    {
//
//         //=====================================================
//         // Declaration type things
//         //=====================================================
//     case NECRO_AST_TOP_DECL:
//     {
//         NecroDeclarationGroupList* group_list = ast->top_declaration.group_list;
//         while (group_list != NULL)
//         {
//             NecroDeclarationGroup* declaration_group = group_list->declaration_group;
//             while (declaration_group != NULL)
//             {
//                 necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, declaration_group->declaration_ast));
//                 declaration_group = declaration_group->next;
//             }
//             group_list = group_list->next;
//         }
//         return ok(NecroType, NULL);
//     }
//
//     case NECRO_AST_DECL:
//     {
//         NecroDeclarationGroupList* group_list = ast->declaration.group_list;
//         while (group_list != NULL)
//         {
//             NecroDeclarationGroup* declaration_group = group_list->declaration_group;
//             while (declaration_group != NULL)
//             {
//                 necro_type_class_translate_go(dictionary_context, infer, declaration_group->declaration_ast);
//                 declaration_group = declaration_group->next;
//             }
//             group_list = group_list->next;
//         }
//         return ok(NecroType, NULL);
//     }
//
//     case NECRO_AST_TYPE_CLASS_INSTANCE:
//     {
//         // NecroCon type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
//         // NecroCon data_type_name;
//         // if (ast->type_class_instance.inst->type == NECRO_AST_CONID)
//         //     data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->conid.symbol, .id = ast->type_class_instance.inst->conid.id };
//         // else if (ast->type_class_instance.inst->type == NECRO_AST_CONSTRUCTOR)
//         //     data_type_name = (NecroCon) { .symbol = ast->type_class_instance.inst->constructor.conid->conid.symbol, .id = ast->type_class_instance.inst->constructor.conid->conid.id };
//         // else
//         //     assert(false);
//         // NecroTypeClassInstance*          instance               = necro_get_type_class_instance(infer, data_type_name.symbol, type_class_name.symbol);
//         // NecroSymbol                      dictionary_arg_name    = necro_create_dictionary_arg_name(infer->intern, type_class_name.symbol, data_type_name.symbol);
//         // NecroASTNode*                    dictionary_arg_var     = necro_create_variable_ast(infer->arena, infer->intern, necro_intern_get_string(infer->intern, dictionary_arg_name), NECRO_VAR_DECLARATION);
//         // dictionary_arg_var->scope                               = ast->scope;
//         // necro_rename_declare_pass(infer->renamer, infer->arena, dictionary_arg_var);
//         // if (necro_is_infer_error(infer)) return;
//         // NecroTypeClassDictionaryContext* new_dictionary_context = necro_create_type_class_dictionary_context(infer->arena, type_class_name, data_type_name, dictionary_arg_var, dictionary_context);
//         // necro_type_class_translate_go(new_dictionary_context, infer, env, ast->type_class_instance.context);
//         // necro_type_class_translate_go(new_dictionary_context, infer, env, ast->type_class_instance.qtycls);
//         // necro_type_class_translate_go(new_dictionary_context, infer, env, ast->type_class_instance.inst);
//         return necro_type_class_translate_go(dictionary_context, infer, ast->type_class_instance.declarations);
//     }
//
//     //=====================================================
//     // Assignment type things
//     //=====================================================
//     case NECRO_AST_SIMPLE_ASSIGNMENT:
//     {
//         NecroType*                       type                   = necro_type_find(ast->necro_type);
//         NecroTypeClassDictionaryContext* new_dictionary_context = dictionary_context;
//         NecroAst*                        dictionary_args        = NULL;
//         NecroAst*                        dictionary_args_head   = NULL;
//         NecroAst*                        rhs                    = ast->simple_assignment.rhs;
//         if (ast->simple_assignment.initializer != NULL && !necro_is_fully_concrete(type))
//         {
//             return necro_type_non_concrete_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
//         }
//         if (ast->simple_assignment.initializer != NULL && !ast->simple_assignment.is_recursive)
//         {
//             return necro_type_non_recursive_initialized_value_error(ast->simple_assignment.ast_symbol, ast->necro_type, ast->source_loc, ast->end_loc);
//         }
//         while (type->type == NECRO_TYPE_FOR)
//         {
//             NecroTypeClassContext* for_all_context = type->for_all.context;
//             while (for_all_context != NULL)
//             {
//                 NecroSymbol dictionary_arg_name = necro_create_dictionary_arg_name(infer->intern, for_all_context->class_symbol->source_name, for_all_context->var_symbol->source_name);
//                 NecroAst*   dictionary_arg_var  = necro_ast_create_var(infer->arena, infer->intern, dictionary_arg_name->str, NECRO_VAR_DECLARATION);
//                 dictionary_arg_var->scope       = ast->simple_assignment.rhs->scope;
//                 // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//                 // necro_rename_declare_pass(infer->renamer, infer->arena, dictionary_arg_var);
//                 new_dictionary_context = necro_create_type_class_dictionary_context(infer->arena, for_all_context->class_symbol, for_all_context->var_symbol, dictionary_arg_var, new_dictionary_context);
//                 if (dictionary_args_head == NULL)
//                 {
//                     dictionary_args = necro_ast_create_apats(infer->arena, dictionary_arg_var, NULL);
//                     dictionary_args_head = dictionary_args;
//                 }
//                 else
//                 {
//                     dictionary_args->apats.next_apat = necro_ast_create_apats(infer->arena, dictionary_arg_var, NULL);
//                     dictionary_args = dictionary_args->apats.next_apat;
//                 }
//                 for_all_context = for_all_context->next;
//             }
//             type = type->for_all.type;
//         }
//         if (dictionary_args_head != NULL)
//         {
//             NecroAst* new_rhs = necro_ast_create_wildcard(infer->arena);
//             *new_rhs          = *rhs;
//             *rhs              = *necro_ast_create_lambda(infer->arena, dictionary_args_head, new_rhs);
//         }
//         necro_try(NecroType, necro_type_class_translate_go(new_dictionary_context, infer, ast->simple_assignment.initializer));
//         return necro_type_class_translate_go(new_dictionary_context, infer, rhs);
//     }
//
//     case NECRO_AST_APATS_ASSIGNMENT:
//     {
//         NecroType*                       type                   = ast->necro_type;
//         NecroTypeClassDictionaryContext* new_dictionary_context = dictionary_context;
//         NecroAst*                        dictionary_args        = NULL;
//         NecroAst*                        dictionary_args_head   = NULL;
//         while (type->type == NECRO_TYPE_FOR)
//         {
//             NecroTypeClassContext* for_all_context = type->for_all.context;
//             while (for_all_context != NULL)
//             {
//                 NecroSymbol dictionary_arg_name = necro_create_dictionary_arg_name(infer->intern, for_all_context->class_symbol->source_name, for_all_context->var_symbol->source_name);
//                 NecroAst*   dictionary_arg_var  = necro_ast_create_var(infer->arena, infer->intern, dictionary_arg_name->str, NECRO_VAR_DECLARATION);
//                 dictionary_arg_var->scope = ast->apats_assignment.rhs->scope;
//                 // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//                 // necro_rename_declare_pass(infer->renamer, infer->arena, dictionary_arg_var);
//                 new_dictionary_context = necro_create_type_class_dictionary_context(infer->arena, for_all_context->class_symbol, for_all_context->var_symbol, dictionary_arg_var, new_dictionary_context);
//                 if (dictionary_args_head == NULL)
//                 {
//                     dictionary_args = necro_ast_create_apats(infer->arena, dictionary_arg_var, NULL);
//                     dictionary_args_head = dictionary_args;
//                 }
//                 else
//                 {
//                     dictionary_args->apats.next_apat = necro_ast_create_apats(infer->arena, dictionary_arg_var, NULL);
//                     dictionary_args = dictionary_args->apats.next_apat;
//                 }
//                 for_all_context = for_all_context->next;
//             }
//             type = type->for_all.type;
//         }
//         if (dictionary_args_head != NULL)
//         {
//             dictionary_args->apats.next_apat = ast->apats_assignment.apats;
//             ast->apats_assignment.apats      = dictionary_args_head;
//         }
//         necro_try(NecroType, necro_type_class_translate_go(new_dictionary_context, infer, ast->apats_assignment.apats));
//         return necro_type_class_translate_go(new_dictionary_context, infer, ast->apats_assignment.rhs);
//     }
//
//     // NOTE: We are making pattern bindings fully monomorphic now (even if a type signature is given. Haskell was at least like this at one point)
//     // If we REALLY want this (for some reason!?!?) we can look into putting more effort into getting polymorphic pattern bindings in.
//     // For now they have a poor weight to power ration.
//     case NECRO_AST_PAT_ASSIGNMENT:
//         necro_try(NecroType, necro_rec_check_pat_assignment(infer, ast->pat_assignment.pat));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->pat_assignment.rhs);
//
//     case NECRO_AST_DATA_DECLARATION:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->data_declaration.simpletype));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->data_declaration.constructor_list);
//
//     case NECRO_AST_VARIABLE:
//         switch (ast->variable.var_type)
//         {
//         case NECRO_VAR_VAR:
//         {
//             NecroAstSymbol* data = ast->variable.ast_symbol;
//             if (data->method_type_class != NULL)
//             {
//                 // TODO: necro_ast_create_var_from_symbol!?
//                 NecroAst* var_ast = necro_ast_create_var(infer->arena, infer->intern, ast->variable.ast_symbol->source_name->str, NECRO_VAR_VAR);
//                 *var_ast = *ast;
//                 NecroTypeClassContext* inst_context = var_ast->variable.inst_context;
//                 assert(inst_context != NULL);
//                 NecroAst* m_var = necro_try_map(NecroAst, NecroType, necro_resolve_method(infer, data->method_type_class, inst_context, var_ast, dictionary_context));
//                 assert(m_var != NULL);
//                 m_var->scope   = ast->scope;
//                 // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//                 // necro_rename_var_pass(infer->renamer, infer->arena, m_var);
//                 *ast = *m_var;
//             }
//             else if (ast->variable.inst_context != NULL)
//             {
//                 NecroAst* var_ast = necro_ast_create_var(infer->arena, infer->intern, ast->variable.ast_symbol->source_name->str, NECRO_VAR_VAR);
//                 *var_ast = *ast;
//                 var_ast->scope = ast->scope;
//                 NecroTypeClassContext* inst_context = ast->variable.inst_context;
//                 while (inst_context != NULL)
//                 {
//                     NecroAst* d_var = necro_try_map(NecroAst, NecroType, necro_resolve_context_to_dictionary(infer, inst_context, dictionary_context));
//                     assert(d_var != NULL);
//                     var_ast = necro_ast_create_fexpr(infer->arena, var_ast, d_var);
//                     d_var->scope   = ast->scope;
//                     // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//                     // necro_rename_var_pass(infer->renamer, infer->arena, d_var);
//                     var_ast->scope = ast->scope;
//                     inst_context = inst_context->next;
//                 }
//                 *ast = *var_ast;
//             }
//             return ok(NecroType, NULL);
//         }
//
//         case NECRO_VAR_DECLARATION:          return ok(NecroType, NULL);
//         case NECRO_VAR_SIG:                  return ok(NecroType, NULL);
//         case NECRO_VAR_TYPE_VAR_DECLARATION: return ok(NecroType, NULL);
//         case NECRO_VAR_TYPE_FREE_VAR:        return ok(NecroType, NULL);
//         case NECRO_VAR_CLASS_SIG:            return ok(NecroType, NULL);
//         default:
//             necro_unreachable(NecroType);
//         }
//
//     case NECRO_AST_CONID:
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_UNDEFINED:
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_CONSTANT:
//         necro_type_class_translate_constant(infer, ast, dictionary_context);
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_UN_OP:
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_BIN_OP:
//     {
//         assert(ast->necro_type != NULL);
//         // TODO: necro_ast_create_bin_op
//         NecroAst* op_ast              = necro_ast_create_var(infer->arena, infer->intern, ast->bin_op.ast_symbol->source_name->str, NECRO_VAR_VAR);
//         op_ast->variable.inst_context = ast->bin_op.inst_context;
//         op_ast->variable.ast_symbol   = ast->bin_op.ast_symbol;
//         op_ast->scope                 = ast->scope;
//         op_ast->necro_type            = ast->necro_type;
//         op_ast                        = necro_ast_create_fexpr(infer->arena, op_ast, ast->bin_op.lhs);
//         op_ast->scope                 = ast->scope;
//         op_ast->necro_type            = ast->necro_type;
//         op_ast                        = necro_ast_create_fexpr(infer->arena, op_ast, ast->bin_op.rhs);
//         op_ast->scope                 = ast->scope;
//         op_ast->necro_type            = ast->necro_type;
//         *ast = *op_ast;
//         if (ast->bin_op.inst_context != NULL)
//             necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast));
//         return ok(NecroType, NULL);
//     }
//
//     case NECRO_AST_OP_LEFT_SECTION:
//     {
//         assert(ast->necro_type != NULL);
//         // TODO: necro_ast_create_left_section
//         NecroAst* op_ast              = necro_ast_create_var(infer->arena, infer->intern, ast->op_left_section.ast_symbol->source_name->str, NECRO_VAR_VAR);
//         op_ast->variable.inst_context = ast->op_left_section.inst_context;
//         op_ast->variable.ast_symbol   = ast->op_left_section.ast_symbol;
//         op_ast->scope                 = ast->scope;
//         op_ast->necro_type            = ast->op_left_section.op_necro_type;
//         op_ast                        = necro_ast_create_fexpr(infer->arena, op_ast, ast->op_left_section.left);
//         op_ast->scope                 = ast->scope;
//         op_ast->necro_type            = ast->necro_type;
//         *ast = *op_ast;
//         if (ast->op_left_section.inst_context != NULL)
//             necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast));
//         return ok(NecroType, NULL);
//     }
//
//     case NECRO_AST_OP_RIGHT_SECTION:
//     {
//         // TODO: necro_ast_create_right_section
//         // TODO: necro_ast_create_var_from_ast_symbol
//         assert(ast->necro_type != NULL);
//         NecroAst* x_var_arg                   = necro_ast_create_var(infer->arena, infer->intern, "left@rightSection", NECRO_VAR_DECLARATION);
//         NecroAst* x_var_var                   = necro_ast_create_var(infer->arena, infer->intern, "left@rightSection", NECRO_VAR_VAR);
//         NecroAst* op_ast                      = necro_ast_create_var(infer->arena, infer->intern, ast->op_right_section.ast_symbol->source_name->str, NECRO_VAR_VAR);
//         op_ast->variable.inst_context         = ast->op_right_section.inst_context;
//         op_ast->variable.ast_symbol           = ast->op_right_section.ast_symbol;
//         op_ast->scope                         = NULL;
//         op_ast->necro_type                    = ast->op_right_section.op_necro_type;
//         op_ast                                = necro_ast_create_fexpr(infer->arena, op_ast, x_var_var);
//         op_ast                                = necro_ast_create_fexpr(infer->arena, op_ast, ast->op_right_section.right);
//         NecroAst* lambda_ast                  = necro_ast_create_lambda(infer->arena, necro_ast_create_apats(infer->arena, x_var_arg, NULL), op_ast);
//         infer->scoped_symtable->current_scope = ast->scope;
//         lambda_ast->necro_type                = ast->necro_type;
//         necro_build_scopes_go(infer->scoped_symtable, lambda_ast);
//         // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//         // necro_rename_declare_pass(infer->renamer, infer->arena, lambda_ast);
//         // necro_rename_var_pass(infer->renamer, infer->arena, lambda_ast);
//         *ast = *lambda_ast;
//         if (ast->op_right_section.inst_context != NULL)
//             necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast));
//         return ok(NecroType, NULL);
//     }
//
//     case NECRO_AST_IF_THEN_ELSE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->if_then_else.if_expr));
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->if_then_else.then_expr));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->if_then_else.else_expr);
//
//     case NECRO_AST_RIGHT_HAND_SIDE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->right_hand_side.declarations));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->right_hand_side.expression);
//
//     case NECRO_AST_LET_EXPRESSION:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->let_expression.declarations));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->let_expression.expression);
//
//     case NECRO_AST_FUNCTION_EXPRESSION:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->fexpression.aexp));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->fexpression.next_fexpression);
//
//     case NECRO_AST_APATS:
//         // necro_type_class_translate_go(infer, ast->apats.apat, dictionary_context);
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->apats.apat));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->apats.next_apat);
//
//     case NECRO_AST_WILDCARD:
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_LAMBDA:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->lambda.apats));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->lambda.expression);
//
//     case NECRO_AST_DO:
//     {
//         // DO statements NOT FULLY IMPLEMENTED currently
//         necro_unreachable(NecroType);
//         // TODO: REDO!
//         // NecroAst* bind_node = necro_ast_create_var(infer->arena, infer->intern, "bind", NECRO_VAR_VAR);
//         // NecroScope* scope = ast->scope;
//         // while (scope->parent != NULL)
//         //     scope = scope->parent;
//         // bind_node->scope = bind_node->scope = scope;
//         // // TODO: NOTE THIS NEEDS TO BE REPLACED WITH NEW SYSTEM THAT DOESN'T USE THE RENAMER!
//         // // necro_rename_var_pass(infer->renamer, infer->arena, bind_node);
//         // bind_node->necro_type = necro_symtable_get(infer->symtable, bind_node->variable.id)->type;
//         // NecroTypeClassContext* bind_context     = necro_symtable_get(infer->symtable, bind_node->variable.id)->type->for_all.context;
//         // NecroTypeClassContext* monad_context    = ast->do_statement.monad_var->var.context;
//         // while (monad_context->type_class_name.id.id != bind_context->type_class_name.id.id)
//         //     monad_context = monad_context->next;
//         // // assert(monad_context->type_class_name.id.id != bind_context->type_class_name.id.id);
//         // NecroAst*          bind_method_inst = necro_resolve_method(infer, monad_context->type_class, monad_context, bind_node, dictionary_context);
//         // necro_ast_print(bind_method_inst);
//         // necro_type_class_translate_go(dictionary_context, infer, ast->do_statement.statement_list);
//         // break;
//     }
//
//     case NECRO_AST_LIST_NODE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->list.item));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->list.next_item);
//
//     case NECRO_AST_EXPRESSION_LIST:
//         return necro_type_class_translate_go(dictionary_context, infer, ast->expression_list.expressions);
//
//     case NECRO_AST_EXPRESSION_ARRAY:
//         return necro_type_class_translate_go(dictionary_context, infer, ast->expression_array.expressions);
//
//     case NECRO_AST_PAT_EXPRESSION:
//         return necro_type_class_translate_go(dictionary_context, infer, ast->pattern_expression.expressions);
//
//     case NECRO_AST_TUPLE:
//         return necro_type_class_translate_go(dictionary_context, infer, ast->tuple.expressions);
//
//     case NECRO_BIND_ASSIGNMENT:
//         return necro_type_class_translate_go(dictionary_context, infer, ast->bind_assignment.expression);
//
//     case NECRO_PAT_BIND_ASSIGNMENT:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->pat_bind_assignment.pat));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->pat_bind_assignment.expression);
//
//     case NECRO_AST_ARITHMETIC_SEQUENCE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->arithmetic_sequence.from));
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->arithmetic_sequence.then));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->arithmetic_sequence.to);
//
//     case NECRO_AST_CASE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->case_expression.expression));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->case_expression.alternatives);
//
//     case NECRO_AST_CASE_ALTERNATIVE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->case_alternative.pat));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->case_alternative.body);
//
//     case NECRO_AST_TYPE_APP:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->type_app.ty));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->type_app.next_ty);
//
//     case NECRO_AST_BIN_OP_SYM:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->bin_op_sym.left));
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->bin_op_sym.op));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->bin_op_sym.right);
//
//     case NECRO_AST_CONSTRUCTOR:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->constructor.conid));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->constructor.arg_list);
//
//     case NECRO_AST_SIMPLE_TYPE:
//         necro_try(NecroType, necro_type_class_translate_go(dictionary_context, infer, ast->simple_type.type_con));
//         return necro_type_class_translate_go(dictionary_context, infer, ast->simple_type.type_var_list);
//
//     case NECRO_AST_TYPE_CLASS_DECLARATION:
// #if 0
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.context);
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.tycls);
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.tyvar);
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_class_declaration.declarations);
// #endif
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_TYPE_SIGNATURE:
// #if 0
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_signature.var);
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_signature.context);
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_signature.type);
// #endif
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_TYPE_CLASS_CONTEXT:
// #if 0
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_class_context.conid);
//         necro_type_class_translate_go(dictionary_context, infer, ast->type_class_context.varid);
// #endif
//         return ok(NecroType, NULL);
//
//     case NECRO_AST_FUNCTION_TYPE:
// #if 0
//         necro_type_class_translate_go(dictionary_context, infer, ast->function_type.type);
//         necro_type_class_translate_go(dictionary_context, infer, ast->function_type.next_on_arrow);
// #endif
//         return ok(NecroType, NULL);
//
    default:
        necro_unreachable(void);
    }
}

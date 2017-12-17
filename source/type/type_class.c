/* Copyright (C) Chad McKinney and Curtis McKinney - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 */

#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include "infer.h"
#include "symtable.h"
#include "type_class.h"

#define NECRO_TYPE_CLASS_DEBUG 1
#if NECRO_TYPE_CLASS_DEBUG
#define TRACE_TYPE_CLASS(...) printf(__VA_ARGS__)
#else
#define TRACE_TYPE_CLASS(...)
#endif

NecroTypeClassEnv necro_create_type_class_env()
{
    return (NecroTypeClassEnv)
    {
        .class_table    = necro_create_type_class_table(),
        .instance_table = necro_create_type_class_instance_table(),
        .arena          = necro_create_paged_arena(),
    };
}

void necro_destroy_type_class_env(NecroTypeClassEnv* env)
{
    necro_destroy_type_class_table(&env->class_table);
    necro_destroy_type_class_instance_table(&env->instance_table);
    necro_destroy_paged_arena(&env->arena);
}

//=====================================================
// Type Class Declaration member
//=====================================================
void necro_create_type_class_declaration(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    assert(ast->type == NECRO_AST_TYPE_CLASS_DECLARATION);
    TRACE_TYPE_CLASS("necro_create_type_class\n");
    NecroTypeClass* type_class       = necro_type_class_table_insert(&env->class_table, ast->type_class_declaration.tycls->conid.id.id, NULL);
    type_class->type_class_name      = (NecroCon) { .symbol = ast->type_class_declaration.tycls->conid.symbol, .id = ast->type_class_declaration.tycls->conid.id };
    type_class->member_type_sig_list = NULL;
    type_class->type_var             = (NecroCon) { .symbol = ast->type_class_declaration.tyvar->variable.symbol, .id = ast->type_class_declaration.tyvar->variable.id };
    NecroTypeClassContext* context   = NULL;

    // Construct context
    // 1. Add TypeClass as context to member type signatures
    // 2. Add ambiguity check to make sure (what exactly)???
    NecroNode* contexts = ast->type_class_declaration.context;
    while (contexts != NULL)
    {
        TRACE_TYPE_CLASS("context type: %d\n", contexts->type);
        if (contexts->type == NECRO_AST_LIST_NODE)
        {
            assert(contexts->list.item->type == NECRO_AST_TYPE_CLASS_CONTEXT);
            NecroTypeClassContext* new_context = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassContext));
            new_context->type_class_name       = (NecroCon) { .symbol = contexts->list.item->type_class_context.conid->conid.symbol,    .id = contexts->list.item->type_class_context.conid->conid.id };
            new_context->type_var              = (NecroCon) { .symbol = contexts->list.item->type_class_context.varid->variable.symbol, .id = contexts->list.item->type_class_context.varid->variable.id };
            new_context->next_context          = context;
            context                            = new_context;
            contexts                           = contexts->list.next_item;
        }
        else if (contexts->type == NECRO_AST_TYPE_CLASS_CONTEXT)
        {
            context                  = necro_paged_arena_alloc(&env->arena, sizeof(NecroTypeClassContext));
            context->type_class_name = (NecroCon) { .symbol = contexts->type_class_context.conid->conid.symbol,    .id = contexts->type_class_context.conid->conid.id };
            context->type_var        = (NecroCon) { .symbol = contexts->type_class_context.varid->variable.symbol, .id = contexts->type_class_context.varid->variable.id };
            context->next_context    = NULL;
            break;
        }
        else
        {
            assert(false);
        }
    }

    // Pass 1 - Member Type Signatures
    NecroNode* declarations = ast->type_class_declaration.declarations;
    while (declarations != NULL)
    {
        // assert(declarations->declaration.declaration_impl->type == NECRO_AST_TYPE_SIGNATURE);
        if (declarations->declaration.declaration_impl->type != NECRO_AST_TYPE_SIGNATURE)
            continue;
        necro_infer_type_sig(infer, declarations->declaration.declaration_impl);
        NecroType* declaration_type = necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type;
        necro_check_type_sanity(infer, declaration_type);
        if (necro_is_infer_error(infer)) return;
        // unify, then generalize?
        // declaration_type = necro_gen(infer, declaration_type, NULL);
        necro_print_type_sig(declaration_type, infer->intern);
        // necro_symtable_get(infer->symtable, declarations->declaration.declaration_impl->type_signature.var->variable.id)->type = declaration_type;
        type_class->member_type_sig_list = necro_create_type_list(infer, declaration_type, type_class->member_type_sig_list);
        declarations = declarations->declaration.next_declaration;
    }

    // // Pass 2 - Default Member Implementations
    // while (declarations != NULL)
    // {
    //     if (declarations->declaration.declaration_impl->type != NECRO_AST_SIMPLE_ASSIGNMENT || declarations->declaration.declaration_impl->type != NECRO_AST_APATS_ASSIGNMENT)
    //         continue;
    //     NecroType* declaration_type = necro_infer_go(env->infer, declarations->declaration.declaration_impl);
    //     necro_check_type_sanity(env->infer, declaration_type);
    //     if (necro_is_infer_error(env->infer)) return;
    //     type_class->member_type_sig_list = necro_create_type_list(env->infer, declaration_type, type_class->member_type_sig_list);
    //     declarations = declarations->declaration.next_declaration;
    // }

}

//=====================================================
// Type Class Instance
//=====================================================
void necro_create_type_class_instance(NecroInfer* infer, NecroTypeClassEnv* env, NecroNode* ast)
{
    // NecroAST_LocalPtr context;// optional, null_local_ptr if not present
    // NecroAST_LocalPtr qtycls;
    // NecroAST_LocalPtr inst;   // Can either be NECRO_AST_CON_ID or NECRO_AST_CONSTRUCTOR
    // NecroAST_LocalPtr declarations; // Points to the next in the list, null_local_ptr if the end

    // TODO: Enforce that each type variable can only be used once in the instance head!

    assert(ast->type == NECRO_AST_TYPE_CLASS_INSTANCE);
    TRACE_TYPE_CLASS("necro_create_type_class_instance\n");
    // NecroCon       type_class_name = (NecroCon) { .symbol = ast->type_class_instance.qtycls->conid.symbol, .id = ast->type_class_instance.qtycls->conid.id };
    // uint64_t        key       =
    // NecroTypeClass* type_class       = necro_type_class_table_insert(&env->class_table, ast->type_class_declaration.tycls->conid.id.id, NULL);
    // type_class->type_class_name      = (NecroCon) { .symbol = ast->type_class_declaration.tycls->conid.symbol, .id = ast->type_class_declaration.tycls->conid.id };
    // type_class->member_type_sig_list = NULL;
    // type_class->type_var             = (NecroCon) { .symbol = ast->type_class_declaration.tyvar->variable.symbol, .id = ast->type_class_declaration.tyvar->variable.id };
    // NecroTypeClassContext* context   = NULL;

}
